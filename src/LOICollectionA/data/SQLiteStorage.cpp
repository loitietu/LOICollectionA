#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

#include "LOICollectionA/base/ScopeGuard.h"

#include "LOICollectionA/data/SQLiteStorage.h"

SQLiteStorage::ConnectionContext::ConnectionContext(const std::string& path, bool readOnly) {
    this->database = std::make_unique<SQLite::Database>(path, 
        readOnly ? SQLite::OPEN_READONLY : SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE
    );

    if (readOnly)
        return;

    database->exec("PRAGMA journal_mode = WAL;");
    database->exec("PRAGMA synchronous = NORMAL;");
    database->exec("PRAGMA temp_store = MEMORY;");
    database->exec("PRAGMA cache_size = 8096;");
    database->exec("PRAGMA busy_timeout = 5000;");
}

SQLiteConnectionPool::SQLiteConnectionPool(const std::string& path, size_t size, bool readOnly) {
    std::unique_lock lock(this->mMutex);

    for (size_t i = 0; i < size; ++i) {
        auto context = std::make_shared<SQLiteStorage::ConnectionContext>(path, readOnly);

        this->mConnections.push_back(context);
        this->mAvailableConnections.push(context);
    }
}

SQLiteConnectionPool::~SQLiteConnectionPool() {
    std::unique_lock lock(this->mMutex);

    while (!this->mAvailableConnections.empty()) 
        this->mAvailableConnections.pop();
    
    this->mConnections.clear();
}

std::shared_ptr<SQLiteStorage::ConnectionContext> SQLiteConnectionPool::getConnection() {
    std::unique_lock lock(this->mMutex);
    
    this->mConnectionAvailable.wait(lock, [this]() -> bool {
        return !this->mAvailableConnections.empty();
    });
    
    auto conn = this->mAvailableConnections.front();
    this->mAvailableConnections.pop();
    this->mActiveConnections++;
    
    return conn;
}

void SQLiteConnectionPool::returnConnection(std::shared_ptr<SQLiteStorage::ConnectionContext> conn) {
    std::unique_lock lock(this->mMutex);

    this->mAvailableConnections.push(conn);
    this->mActiveConnections--;

    lock.unlock();

    this->mConnectionAvailable.notify_one();
}

SQLiteStorage::SQLiteStorage(const std::string& path, size_t size) {
    this->writeConnectionPool = std::make_unique<SQLiteConnectionPool>(path, size, false);
    this->readConnectionPool = std::make_unique<SQLiteConnectionPool>(path, size, true);
}
SQLiteStorage::~SQLiteStorage() = default;

SQLite::Statement& SQLiteStorage::getCachedStatement(ConnectionContext& context, const std::string& sql) {
    std::unique_lock lock(context.cacheMutex);
    
    if (auto it = context.stmtCache.find(sql); it != context.stmtCache.end()) 
        return *it->second;
    
    auto stmt = std::make_unique<SQLite::Statement>(*context.database, sql);
    return *context.stmtCache.emplace(sql, std::move(stmt)).first->second;
}

void SQLiteStorage::exec(std::string_view sql) {
    std::unique_lock lock(this->mMutex);

    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    conn->database->exec(std::string(sql));
}

void SQLiteStorage::create(std::string_view table) {
    std::unique_lock lock(this->mMutex);

    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    conn->database->exec("CREATE TABLE IF NOT EXISTS " + std::string(table) + " (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;");
}

void SQLiteStorage::remove(std::string_view table) {
    std::unique_lock lock(this->mMutex);

    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    conn->database->exec("DROP TABLE IF EXISTS " + std::string(table) + ";");

    std::unique_lock cachelock(conn->cacheMutex);

    conn->stmtCache.clear();
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    std::unique_lock lock(this->mMutex);

    auto conn = this->writeConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "INSERT INTO " + std::string(table) + " (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->writeConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(key));
    stmt.bind(2, std::string(value));
    stmt.exec();
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    std::unique_lock lock(this->mMutex);

    auto conn = this->writeConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "DELETE FROM " + std::string(table) + " WHERE key = ?;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->writeConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(key));
    stmt.exec();
}

void SQLiteStorage::delByPrefix(std::string_view table, std::string_view prefix) {
    std::unique_lock lock(this->mMutex);
    
    auto conn = this->writeConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "DELETE FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->writeConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(prefix) + "%");
    stmt.exec();
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT 1 FROM " + std::string(table) + " WHERE key = ? LIMIT 1;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(key));
    return stmt.executeStep();
}

bool SQLiteStorage::has(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(table));
    return stmt.executeStep();
}

bool SQLiteStorage::hasByPrefix(std::string_view table, std::string_view prefix, int size) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT COUNT(*) FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(prefix) + "%");
    if (!stmt.executeStep())
        return false;

    return stmt.getColumn(0).getInt() == size;
}

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT key, value FROM " + std::string(table) + ";"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    std::unordered_map<std::string, std::string> result;
    while (stmt.executeStep()) {
        result.emplace(
            stmt.getColumn(0).getText(),
            stmt.getColumn(1).getText()
        );
    }

    return result;
}

std::unordered_map<std::string, std::string> SQLiteStorage::getByPrefix(std::string_view table, std::string_view prefix) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT key, value FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(prefix) + "%");

    std::unordered_map<std::string, std::string> result;
    while (stmt.executeStep()) {
        result.emplace(
            stmt.getColumn(0).getText(),
            stmt.getColumn(1).getText()
        );
    }

    return result;
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view defaultValue) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT value FROM " + std::string(table) + " WHERE key = ?;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(key));
    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();

    return std::string(defaultValue);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT key FROM " + std::string(table) + ";"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    std::vector<std::string> keys;
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

std::vector<std::string> SQLiteStorage::list() {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    std::vector<std::string> tables;
    while (stmt.executeStep())
        tables.emplace_back(stmt.getColumn(0).getText());

    return tables;
}

std::vector<std::string> SQLiteStorage::listByPrefix(std::string_view table, std::string_view prefix) {
    auto conn = this->readConnectionPool->getConnection();
    auto& stmt = getCachedStatement(*conn,
        "SELECT key FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([this, conn, &stmt]() -> void { 
        stmt.reset(); 

        this->readConnectionPool->returnConnection(conn); 
    });

    stmt.bind(1, std::string(prefix) + "%");

    std::vector<std::string> keys;
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

SQLiteStorageTransaction::SQLiteStorageTransaction(SQLiteStorage& storage) : mStorage(storage) {
    this->mConnection = mStorage.writeConnectionPool->getConnection();
    this->mTransaction = std::make_unique<SQLite::Transaction>(*mConnection->database);
}

SQLiteStorageTransaction::~SQLiteStorageTransaction() {
    if (!this->mTransaction) 
        return;

    this->commit();
}

void SQLiteStorageTransaction::commit() {
    if (!this->mTransaction) 
        return;

    try {
        this->mTransaction->commit();
        this->mTransaction.reset();
    } catch (...) {}

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
}

void SQLiteStorageTransaction::rollback() {
    if (!this->mTransaction) 
        return;

    try {
        this->mTransaction->rollback();
        this->mTransaction.reset();
    } catch(...) {}

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
}
