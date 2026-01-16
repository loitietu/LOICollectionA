#include <format>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <shared_mutex>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

#include "LOICollectionA/base/ScopeGuard.h"

#include "LOICollectionA/data/SQLiteStorage.h"

struct SQLiteStorage::ConnectionContext  {
    std::unique_ptr<SQLite::Database> database;

    std::shared_mutex cacheMutex;
    std::unordered_map<std::string, std::shared_ptr<SQLite::Statement>> stmtCache;
    
    ConnectionContext(const std::string& path, bool readOnly = false);
};

SQLiteStorage::ConnectionContext::ConnectionContext(const std::string& path, bool readOnly) {
    this->database = std::make_unique<SQLite::Database>(path, 
        readOnly ? SQLite::OPEN_READONLY : SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE
    );

    if (readOnly)
        return;

    this->database->exec("PRAGMA journal_mode = WAL;");
    this->database->exec("PRAGMA synchronous = NORMAL;");
    this->database->exec("PRAGMA temp_store = MEMORY;");
    this->database->exec("PRAGMA cache_size = 8096;");
    this->database->exec("PRAGMA busy_timeout = 5000;");
    this->database->exec("PRAGMA optimize;");
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
    
    for (auto& conn : this->mConnections) {
        std::unique_lock cacheLock(conn->cacheMutex);

        conn->stmtCache.clear();
    }
    
    std::queue<std::shared_ptr<SQLiteStorage::ConnectionContext>> empty;
    std::swap(this->mAvailableConnections, empty);

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
    {
        std::shared_lock lock(context.cacheMutex);
        if (auto it = context.stmtCache.find(sql); it != context.stmtCache.end())
            return *it->second;
    }
    
    std::unique_lock lock(context.cacheMutex);
    
    if (auto it = context.stmtCache.find(sql); it != context.stmtCache.end())
        return *it->second;
    
    auto stmt = std::make_shared<SQLite::Statement>(*context.database, sql);
    context.stmtCache.emplace(sql, stmt);

    return *stmt;
}

void SQLiteStorage::exec(std::shared_ptr<ConnectionContext> context, std::string_view sql) {
    context->database->exec(std::string(sql));
}

void SQLiteStorage::create(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    context->database->exec(std::format("CREATE TABLE IF NOT EXISTS {} (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;", table));
}

void SQLiteStorage::remove(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    context->database->exec(std::format("DROP TABLE IF EXISTS {};", table));

    std::unique_lock lock(context->cacheMutex);

    context->stmtCache.clear();
}

void SQLiteStorage::set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view value) {
    auto& stmt = getCachedStatement(*context,
        std::format("INSERT INTO {} (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    stmt.bind(2, std::string(value));
    stmt.exec();
}

void SQLiteStorage::del(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    auto& stmt = getCachedStatement(*context,
        std::format("DELETE FROM {} WHERE key = ?;", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    stmt.exec();
}

void SQLiteStorage::delByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix) {
    auto& stmt = getCachedStatement(*context,
        std::format("DELETE FROM {} WHERE key LIKE ? ESCAPE '\\';", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(prefix) + "%");
    stmt.exec();
}

bool SQLiteStorage::has(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT 1 FROM {} WHERE key = ? LIMIT 1;", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    return stmt.executeStep();
}

bool SQLiteStorage::has(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = getCachedStatement(*context,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1;"
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(table));
    return stmt.executeStep();
}

bool SQLiteStorage::hasByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix, int size) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT COUNT(*) FROM {} WHERE key LIKE ? ESCAPE '\\';", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(prefix) + "%");
    if (!stmt.executeStep())
        return false;

    return stmt.getColumn(0).getInt() == size;
}

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT key, value FROM {};", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
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

std::unordered_map<std::string, std::string> SQLiteStorage::getByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT key, value FROM {} WHERE key LIKE ? ESCAPE '\\';", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
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

std::string SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view defaultValue) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT value FROM {} WHERE key = ?;", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();

    return std::string(defaultValue);
}

std::vector<std::string> SQLiteStorage::list(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT key FROM {};", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> keys;
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

std::vector<std::string> SQLiteStorage::list(std::shared_ptr<ConnectionContext> context) {
    auto& stmt = getCachedStatement(*context,
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;"
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> tables;
    while (stmt.executeStep())
        tables.emplace_back(stmt.getColumn(0).getText());

    return tables;
}

std::vector<std::string> SQLiteStorage::listByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT key FROM {} WHERE key LIKE ? ESCAPE '\\';", table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(prefix) + "%");

    std::vector<std::string> keys;
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

void SQLiteStorage::exec(std::string_view sql) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->exec(conn, sql);
}

void SQLiteStorage::create(std::string_view table) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->create(conn, table);
}

void SQLiteStorage::remove(std::string_view table) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->remove(conn, table);
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->set(conn, table, key, value);
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->del(conn, table, key);
}

void SQLiteStorage::delByPrefix(std::string_view table, std::string_view prefix) {
    auto conn = this->writeConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->delByPrefix(conn, table, prefix);
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->has(conn, table, key);
}

bool SQLiteStorage::has(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->has(conn, table);
}

bool SQLiteStorage::hasByPrefix(std::string_view table, std::string_view prefix, int size) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->hasByPrefix(conn, table, prefix, size);
}

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->get(conn, table);
}

std::unordered_map<std::string, std::string> SQLiteStorage::getByPrefix(std::string_view table, std::string_view prefix) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->getByPrefix(conn, table, prefix);
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view defaultValue) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->get(conn, table, key, defaultValue);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->list(conn, table);
}

std::vector<std::string> SQLiteStorage::list() {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->list(conn);
}

std::vector<std::string> SQLiteStorage::listByPrefix(std::string_view table, std::string_view prefix) {
    auto conn = this->readConnectionPool->getConnection();
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->listByPrefix(conn, table, prefix);
}

SQLiteStorageTransaction::SQLiteStorageTransaction(SQLiteStorage& storage, bool readOnly) : mStorage(storage) {
    this->mConnection = readOnly ? mStorage.readConnectionPool->getConnection() : mStorage.writeConnectionPool->getConnection();
    this->mTransaction = std::make_unique<SQLite::Transaction>(*mConnection->database);
}

SQLiteStorageTransaction::~SQLiteStorageTransaction() {
    if (!this->mTransaction) 
        return;

    this->rollback();
}

bool SQLiteStorageTransaction::commit() {
    if (!this->mTransaction) 
        return false;

    try {
        this->mTransaction->commit();
        this->mTransaction.reset();
    } catch (...) {
        this->mStorage.writeConnectionPool->returnConnection(mConnection);
        return false;
    }

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
    return true;
}

bool SQLiteStorageTransaction::rollback() {
    if (!this->mTransaction) 
        return false;

    try {
        this->mTransaction->rollback();
        this->mTransaction.reset();
    } catch(...) {
        this->mStorage.writeConnectionPool->returnConnection(mConnection);
        return false;
    }

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
    return true;
}

std::shared_ptr<SQLiteStorage::ConnectionContext> SQLiteStorageTransaction::connection() const {
    return this->mConnection;
}
