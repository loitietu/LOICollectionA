#include <algorithm>
#include <format>
#include <memory>
#include <ranges>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/ScopeGuard.h"

#include "LOICollectionA/data/SQLiteStorage.h"

struct SQLiteStorage::ConnectionContext  {
    std::unique_ptr<SQLite::Database> database;

    LRUCache<std::string, SQLite::Statement> stmtCache;
    
    ConnectionContext(const std::string& path, size_t cacheSize = 100, bool readOnly = false);
};

SQLiteStorage::ConnectionContext::ConnectionContext(const std::string& path, size_t cacheSize, bool readOnly) : stmtCache(cacheSize) {
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

SQLiteConnectionPool::SQLiteConnectionPool(const std::string& path, size_t size, size_t cacheSize, bool readOnly) {
    std::unique_lock lock(this->mMutex);

    for (size_t i = 0; i < size; ++i) {
        auto context = std::make_shared<SQLiteStorage::ConnectionContext>(path, cacheSize, readOnly);

        this->mConnections.push_back(context);
        this->mAvailableConnections.push(context);
    }
}

SQLiteConnectionPool::~SQLiteConnectionPool() {
    std::unique_lock lock(this->mMutex);
    
    for (auto& conn : this->mConnections)
        conn->stmtCache.clear();
    
    std::queue<std::shared_ptr<SQLiteStorage::ConnectionContext>> empty;
    std::swap(this->mAvailableConnections, empty);

    this->mConnections.clear();
}

std::shared_ptr<SQLiteStorage::ConnectionContext> SQLiteConnectionPool::getConnection(int timeout) {
    std::unique_lock lock(this->mMutex);
    
    if (!this->mConnectionAvailable.wait_for(lock, std::chrono::milliseconds(timeout), [this]() -> bool {
        return !this->mAvailableConnections.empty();
    })) {
        return nullptr;
    }
    
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

SQLiteStorage::SQLiteStorage(const std::string& path, size_t readSize, size_t writeSize, size_t cacheSize) {
    this->writeConnectionPool = std::make_unique<SQLiteConnectionPool>(path, writeSize, cacheSize, false);
    this->readConnectionPool = std::make_unique<SQLiteConnectionPool>(path, readSize, cacheSize, true);
}
SQLiteStorage::~SQLiteStorage() = default;

SQLite::Statement& SQLiteStorage::getCachedStatement(ConnectionContext& context, const std::string& sql) {
    auto cache = context.stmtCache.get(sql);
    if (cache.has_value())
        return *cache.value();

    auto stmt = std::make_shared<SQLite::Statement>(*context.database, sql);
    context.stmtCache.put(sql, stmt);

    return *stmt;
}

void SQLiteStorage::exec(std::shared_ptr<ConnectionContext> context, std::string_view sql) {
    context->database->exec(std::string(sql));
}

void SQLiteStorage::create(std::shared_ptr<ConnectionContext> context, std::string_view table, CreateCallback callback) {
    std::string columns;
    
    if (callback) {
        ColumnCallback columnCallback = [&columns](std::string column) -> void {
            columns += (columns.empty() ? "" : ", ") + column + " TEXT";
        };

        callback(columnCallback);
    }

    context->database->exec(std::format(
        "CREATE TABLE IF NOT EXISTS {} ("
        "key TEXT PRIMARY KEY, "
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP "
        "{}"
        ");",
        table, (columns.empty() ? "" : ", " + columns)
    ));
}

void SQLiteStorage::remove(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    context->database->exec(std::format("DROP TABLE IF EXISTS {};", table));

    context->stmtCache.clear();
}

void SQLiteStorage::set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view value) {
    auto& stmt = getCachedStatement(*context,
        std::format("INSERT INTO {0} (key, {1}) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET {1} = excluded.{1}, updated_at = CURRENT_TIMESTAMP;", table, column)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    stmt.bind(2, std::string(value));
    stmt.exec();
}

void SQLiteStorage::set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values) {
    if (values.empty())
        return;
    
    std::vector<std::string> data = values
        | std::views::keys
        | std::ranges::to<std::vector<std::string>>();

    std::string columns = std::ranges::fold_left((data | std::views::drop(1)), data.front(), [](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + ", " + s;
    });

    std::vector<std::string> placeholders(data.size());
    std::ranges::fill(placeholders, "?");

    std::string placeholder = std::ranges::fold_left(std::views::drop(placeholders, 1), placeholders.front(), [](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + ", " + s;
    });
    
    auto& stmt = getCachedStatement(*context, std::format(
        "INSERT OR REPLACE INTO {} (key, {}, updated_at) VALUES (?, {}, CURRENT_TIMESTAMP);",
        table, columns, placeholder
    ));

    auto guard = make_success_guard([&stmt]() { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));

    for (int i = 0; i < static_cast<int>(data.size()); ++i)
        stmt.bind(i + 2, values.at(data[i]));

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

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    std::vector<std::string> data = this->columns(context, table)
        | std::views::drop(3)
        | std::ranges::to<std::vector<std::string>>();
    
    if (data.empty())
        return {};

    std::string sql = std::ranges::fold_left(std::views::drop(data, 1), data.front(), [](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + ", " + s;
    });

    auto& stmt = getCachedStatement(*context,
        std::format("SELECT {} FROM {} WHERE key = ?;", sql, table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::unordered_map<std::string, std::string> result;
    stmt.bind(1, std::string(key));

    if (stmt.executeStep()) {
        for (int i = 0; i < static_cast<int>(data.size()); ++i) {
            std::string mResult = stmt.getColumn(i).getText();
            if (mResult.empty())
                continue;

            result.emplace(data[i], mResult);
        }
    }

    return result;
}

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::string> keys) {
    if (keys.empty())
        return {};
    
    std::vector<std::string> data = this->columns(context, table)
        | std::views::drop(3)
        | std::ranges::to<std::vector<std::string>>();
    
    if (data.empty())
        return {};

    std::string sql = std::ranges::fold_left((data | std::views::drop(1)), data.front(), [](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + ", " + s;
    });

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;
    
    auto chunks = std::views::chunk(keys, 500);
    for (auto chunk : chunks) {
        std::vector<std::string> placeholders(chunk.size());
        std::ranges::fill(placeholders, "?");

        std::string placeholder = std::ranges::fold_left(std::views::drop(placeholders, 1), placeholders.front(), [](std::string acc, const std::string& s) -> std::string {
            return std::move(acc) + ", " + s;
        });
        
        auto& stmt = getCachedStatement(*context, std::format(
            "SELECT key, {} FROM {} WHERE key IN ({})",
            sql, table, placeholder
        ));

        auto guard = make_success_guard([&stmt]() -> void { 
            stmt.reset(); 
        });
        
        for (int i = 0; i < static_cast<int>(chunk.size()); ++i)
            stmt.bind(i + 1, chunk[i]);

        while (stmt.executeStep()) {
            std::string key = stmt.getColumn(0).getText();

            std::unordered_map<std::string, std::string> mRowData;
            
            auto indices = std::views::iota(0, static_cast<int>(data.size()));
            for (int i : indices) {
                std::string mResult = stmt.getColumn(i + 1).getText();
                if (mResult.empty())
                    continue;

                mRowData.emplace(data[i], mResult);
            }
            
            result.emplace(std::move(key), std::move(mRowData));
        }
    }

    return result;
}

std::string SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT {0} FROM {1} WHERE key = ?;", column, table)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();

    return std::string(defaultValue);
}

std::string SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue, FindCondition match) {
    if (conditions.empty())
        return std::string(defaultValue);

    auto transformed = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        });
    std::string where = std::ranges::fold_left(std::views::drop(transformed, 1), transformed.front(), [match](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + (match == FindCondition::AND ? " AND " : " OR ") + s;
    });

    auto& stmt = getCachedStatement(*context,
        std::format("SELECT key FROM {} WHERE {} LIMIT 1;", table, where)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();

    return std::string(defaultValue);
}

std::vector<std::string> SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    if (conditions.empty())
        return {};
    
    auto transformed = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        });
    std::string where = std::ranges::fold_left(std::views::drop(transformed, 1), transformed.front(), [match](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + (match == FindCondition::AND ? " AND " : " OR ") + s;
    });

    auto& stmt = getCachedStatement(*context,
        std::format("SELECT DISTINCT key FROM {} WHERE {};", table, where)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    std::vector<std::string> result;
    while (stmt.executeStep())
        result.emplace_back(stmt.getColumn(0).getText());

    return result;
}

std::vector<std::string> SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    if (conditions.empty())
        return {};
    
    auto transformed = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        });
    std::string where = std::ranges::fold_left(std::views::drop(transformed, 1), transformed.front(), [match](std::string acc, const std::string& s) -> std::string {
        return std::move(acc) + (match == FindCondition::AND ? " AND " : " OR ") + s;
    });

    auto& stmt = getCachedStatement(*context,
        std::format("SELECT DISTINCT {} FROM {} WHERE {};", column, table, where)
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    std::vector<std::string> result;
    while (stmt.executeStep())
        result.emplace_back(stmt.getColumn(0).getText());

    return result;
}

std::vector<std::string> SQLiteStorage::list(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = getCachedStatement(*context,
        std::format("SELECT DISTINCT key FROM {};", table)
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
        "SELECT DISTINCT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;"
    );

    auto guard = make_success_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> tables;
    while (stmt.executeStep())
        tables.emplace_back(stmt.getColumn(0).getText());

    return tables;
}

std::vector<std::string> SQLiteStorage::columns(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& info = getCachedStatement(*context,
        std::format("PRAGMA table_info({})", table)
    );

    std::vector<std::string> columns;
    while (info.executeStep())
        columns.emplace_back(info.getColumn(1).getText());

    return columns;
}

void SQLiteStorage::exec(std::string_view sql) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->exec(conn, sql);
}

void SQLiteStorage::create(std::string_view table, CreateCallback callback) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->create(conn, table, callback);
}

void SQLiteStorage::remove(std::string_view table) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });
    
    this->remove(conn, table);
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view column, std::string_view value) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->set(conn, table, key, column, value);
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->set(conn, table, key, values);
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    auto conn = this->writeConnectionPool->getConnection();

    if (!conn)
        return;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->writeConnectionPool->returnConnection(conn); 
    });

    this->del(conn, table, key);
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return false;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->has(conn, table, key);
}

bool SQLiteStorage::has(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return false;

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->has(conn, table);
}

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::string_view table, std::string_view key) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->get(conn, table, key);
}

std::unordered_map<std::string, std::unordered_map<std::string, std::string>> SQLiteStorage::get(std::string_view table, std::vector<std::string> keys) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->get(conn, table, keys);
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return std::string(defaultValue);

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->get(conn, table, key, column, defaultValue);
}

std::string SQLiteStorage::find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue, FindCondition match) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return std::string(defaultValue);

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->find(conn, table, conditions, defaultValue, match);
}

std::vector<std::string> SQLiteStorage::find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->find(conn, table, conditions, match);
}

std::vector<std::string> SQLiteStorage::find(std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->find(conn, table, column, conditions, match);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->list(conn, table);
}

std::vector<std::string> SQLiteStorage::list() {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};
    
    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->list(conn);
}

std::vector<std::string> SQLiteStorage::columns(std::string_view table) {
    auto conn = this->readConnectionPool->getConnection();

    if (!conn)
        return {};

    auto guard = make_scope_guard([this, conn]() -> void { 
        this->readConnectionPool->returnConnection(conn); 
    });

    return this->columns(conn, table);
}

SQLiteStorageTransaction::SQLiteStorageTransaction(SQLiteStorage& storage, bool readOnly) : mStorage(storage) {
    this->mConnection = readOnly ? mStorage.readConnectionPool->getConnection() : mStorage.writeConnectionPool->getConnection();

    if (!this->mConnection)
        return;

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
