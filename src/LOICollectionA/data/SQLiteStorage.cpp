#include <format>
#include <memory>
#include <ranges>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/ScopeGuard.h"

#include "LOICollectionA/data/SQLiteError.h"

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

ll::Expected<std::shared_ptr<SQLiteStorage::ConnectionContext>> SQLiteConnectionPool::getConnection(int timeout) {
    std::unique_lock lock(this->mMutex);
    
    if (!this->mConnectionAvailable.wait_for(lock, std::chrono::milliseconds(timeout), [this]() -> bool {
        return !this->mAvailableConnections.empty();
    })) {
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(SQLiteError::SQLiteStorageErrorCode::Timeout));
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

ll::Expected<void> SQLiteStorage::exec(std::shared_ptr<ConnectionContext> context, std::string_view sql) {
    int result = context->database->tryExec(std::string(sql));
    if (result != SQLITE_OK)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return {};
}

ll::Expected<void> SQLiteStorage::create(std::shared_ptr<ConnectionContext> context, std::string_view table, CreateCallback callback) {
    std::string columns;
    
    if (callback) {
        ColumnCallback columnCallback = [&columns](std::string column) -> void {
            columns += (columns.empty() ? "" : ", ") + column + " TEXT";
        };

        callback(columnCallback);
    }

    int result = context->database->tryExec(std::format(
        "CREATE TABLE IF NOT EXISTS {} ("
        "key TEXT PRIMARY KEY, "
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP "
        "{}"
        ");",
        table, (columns.empty() ? "" : ", " + columns)
    ));

    if (result != SQLITE_OK)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return {};
}

ll::Expected<void> SQLiteStorage::remove(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    int result = context->database->tryExec(std::format("DROP TABLE IF EXISTS {};", table));
    if (result != SQLITE_OK)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    context->stmtCache.clear();

    return {};
}

ll::Expected<void> SQLiteStorage::set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view value) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("INSERT INTO {0} (key, {1}) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET {1} = excluded.{1}, updated_at = CURRENT_TIMESTAMP;", table, column)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    stmt.bind(2, std::string(value));
    
    int result = stmt.tryExecuteStep();
    if (result != SQLITE_DONE)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return {};
}

ll::Expected<void> SQLiteStorage::set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values) {
    if (values.empty())
        return {};
    
    std::vector<std::string> data = values
        | std::views::keys
        | std::ranges::to<std::vector<std::string>>();

    std::string columns = data
        | std::views::join_with(std::string_view(", "))
        | std::ranges::to<std::string>();

    std::string placeholder = std::views::repeat(std::string_view("?"), data.size())
        | std::views::join_with(std::string_view(", "))
        | std::ranges::to<std::string>();
    
    auto& stmt = this->getCachedStatement(*context, std::format(
        "INSERT OR REPLACE INTO {} (key, {}, updated_at) VALUES (?, {}, CURRENT_TIMESTAMP);",
        table, columns, placeholder
    ));

    auto guard = make_scope_guard([&stmt]() { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));

    for (int i = 0; i < static_cast<int>(data.size()); ++i)
        stmt.bind(i + 2, values.at(data[i]));

    int result = stmt.tryExecuteStep();
    if (result != SQLITE_DONE)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return {};
}

ll::Expected<void> SQLiteStorage::del(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("DELETE FROM {} WHERE key = ?;", table)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    
    int result = stmt.tryExecuteStep();
    if (result != SQLITE_DONE)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return {};
}

ll::Expected<void> SQLiteStorage::del(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::string> keys) {
    if (keys.empty())
        return {};

    auto chunks = std::views::chunk(keys, 500);
    for (auto chunk : chunks) {
        std::string placeholder = std::views::repeat(std::string_view("?"), chunk.size())
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();
        
        auto& stmt = this->getCachedStatement(*context, std::format(
            "DELETE FROM {} WHERE key IN ({})",
            table, placeholder
        ));

        auto guard = make_scope_guard([&stmt]() -> void { 
            stmt.reset(); 
        });
        
        for (int i = 0; i < static_cast<int>(chunk.size()); ++i)
            stmt.bind(i + 1, chunk[i]);

        int result = stmt.tryExecuteStep();
        if (result != SQLITE_DONE)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));
    }

    return {};
}

ll::Expected<bool> SQLiteStorage::has(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT 1 FROM {} WHERE key = ? LIMIT 1;", table)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));
    
    int result = stmt.tryExecuteStep();
    if (result != SQLITE_ROW && result != SQLITE_DONE)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return result == SQLITE_ROW;
}

ll::Expected<bool> SQLiteStorage::has(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = this->getCachedStatement(*context,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1;"
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(table));
    
    int result = stmt.tryExecuteStep();
    if (result != SQLITE_ROW && result != SQLITE_DONE)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return result == SQLITE_ROW;
}

ll::Expected<std::unordered_map<std::string, std::string>> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key) {
    return this->columns(context, table)
        .transform([](std::vector<std::string> v) -> std::vector<std::string> {
            return v | std::views::drop(3) | std::ranges::to<std::vector<std::string>>();
        })
        .and_then([this, context, table, key](std::vector<std::string> data) -> ll::Expected<std::unordered_map<std::string, std::string>> {
            std::string sql = data
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();

            auto& stmt = this->getCachedStatement(*context,
                std::format("SELECT {} FROM {} WHERE key = ?;", sql, table));

            auto guard = make_scope_guard([&stmt]() {
                stmt.reset();
            });

            stmt.bind(1, std::string(key));

            int result = stmt.tryExecuteStep();
            if (result == SQLITE_DONE)
                return {};

            if (result != SQLITE_ROW)
                return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

            std::unordered_map<std::string, std::string> dataResult;
            for (int i = 0; i < static_cast<int>(data.size()); ++i)
                dataResult.emplace(data[i], stmt.getColumn(i).getText());

            return dataResult;
        });
}

ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::string> keys) {
    if (keys.empty())
        return {};

    return this->columns(context, table)
        .transform([](std::vector<std::string> v) -> std::vector<std::string> {
            return v | std::views::drop(3) | std::ranges::to<std::vector<std::string>>();
        })
        .and_then([this, context, table, keys](std::vector<std::string> data) -> ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> {
            std::string sql = data
                | std::views::join_with(std::string_view(", "))
                | std::ranges::to<std::string>();

            std::unordered_map<std::string, std::unordered_map<std::string, std::string>> dataResult;
            
            auto chunks = std::views::chunk(keys, 500);
            for (auto chunk : chunks) {
                std::string placeholder = std::views::repeat(std::string_view("?"), chunk.size())
                    | std::views::join_with(std::string_view(", "))
                    | std::ranges::to<std::string>();
                
                auto& stmt = this->getCachedStatement(*context, std::format(
                    "SELECT key, {} FROM {} WHERE key IN ({})",
                    sql, table, placeholder
                ));

                auto guard = make_scope_guard([&stmt]() -> void { 
                    stmt.reset(); 
                });
                
                for (int i = 0; i < static_cast<int>(chunk.size()); ++i)
                    stmt.bind(i + 1, chunk[i]);

                while (true) {
                    int result = stmt.tryExecuteStep();
                    if (result == SQLITE_DONE)
                        break;

                    if (result != SQLITE_ROW)
                        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

                    std::string key = stmt.getColumn(0).getText();

                    std::unordered_map<std::string, std::string> mRowData;

                    auto indices = std::views::iota(0, static_cast<int>(data.size()));
                    for (int i : indices)
                        mRowData.emplace(data[i], stmt.getColumn(i + 1).getText());

                    dataResult.emplace(key, std::move(mRowData));
                }
            }

            return dataResult;
        });
}

ll::Expected<std::string> SQLiteStorage::get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT {0} FROM {1} WHERE key = ?;", column, table)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    stmt.bind(1, std::string(key));

    int result = stmt.tryExecuteStep();
    if (result == SQLITE_DONE)
        return std::string(defaultValue);

    if (result != SQLITE_ROW)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return stmt.getColumn(0).getText();
}

ll::Expected<std::string> SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue, FindCondition match) {
    if (conditions.empty())
        return std::string(defaultValue);

    std::string where = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        })
        | std::views::join_with(std::string_view(match == FindCondition::AND ? " AND " : " OR "))
        | std::ranges::to<std::string>();

    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT key FROM {} WHERE {} LIMIT 1;", table, where)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    int result = stmt.tryExecuteStep();
    if (result == SQLITE_DONE)
        return std::string(defaultValue);

    if (result != SQLITE_ROW)
        return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

    return stmt.getColumn(0).getText();
}

ll::Expected<std::vector<std::string>> SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    if (conditions.empty())
        return {};
    
    std::string where = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        })
        | std::views::join_with(std::string_view(match == FindCondition::AND ? " AND " : " OR "))
        | std::ranges::to<std::string>();

    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT DISTINCT key FROM {} WHERE {};", table, where)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    std::vector<std::string> dataResult;
    while (true) {
        int result = stmt.tryExecuteStep();
        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

        dataResult.emplace_back(stmt.getColumn(0).getText());
    }

    return dataResult;
}

ll::Expected<std::vector<std::string>> SQLiteStorage::find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    if (conditions.empty())
        return {};
    
    std::string where = conditions
        | std::views::transform([](const std::pair<std::string, std::string>& p) -> std::string {
            return p.first + " = ?";
        })
        | std::views::join_with(std::string_view(match == FindCondition::AND ? " AND " : " OR "))
        | std::ranges::to<std::string>();

    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT DISTINCT {} FROM {} WHERE {};", column, table, where)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    for (int i = 0; i < static_cast<int>(conditions.size()); ++i)
        stmt.bind(i + 1, conditions[i].second);

    std::vector<std::string> dataResult;
    while (true) {
        int result = stmt.tryExecuteStep();
        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

        dataResult.emplace_back(stmt.getColumn(0).getText());
    }

    return dataResult;
}

ll::Expected<std::vector<std::string>> SQLiteStorage::list(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("SELECT DISTINCT key FROM {};", table)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> keys;
    while (true) {
        int result = stmt.tryExecuteStep();
        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

        keys.emplace_back(stmt.getColumn(0).getText());
    }

    return keys;
}

ll::Expected<std::vector<std::string>> SQLiteStorage::list(std::shared_ptr<ConnectionContext> context) {
    auto& stmt = this->getCachedStatement(*context,
        "SELECT DISTINCT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name;"
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> tables;
    while (true) {
        int result = stmt.tryExecuteStep();
        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

        tables.emplace_back(stmt.getColumn(0).getText());
    }

    return tables;
}

ll::Expected<std::vector<std::string>> SQLiteStorage::columns(std::shared_ptr<ConnectionContext> context, std::string_view table) {
    auto& stmt = this->getCachedStatement(*context,
        std::format("PRAGMA table_info({})", table)
    );

    auto guard = make_scope_guard([&stmt]() -> void { 
        stmt.reset(); 
    });

    std::vector<std::string> columns;
    while (true) {
        int result = stmt.tryExecuteStep();
        if (result == SQLITE_DONE)
            break;

        if (result != SQLITE_ROW)
            return ll::makeErrorCodeError(SQLiteError::makeErrorCode(result));

        columns.emplace_back(stmt.getColumn(1).getText());
    }

    return columns;
}

ll::Expected<void> SQLiteStorage::exec(std::string_view sql) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, sql](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->exec(conn, sql);
        });
}

ll::Expected<void> SQLiteStorage::create(std::string_view table, CreateCallback callback) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table, callback](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->create(conn, table, callback);
        });
}

ll::Expected<void> SQLiteStorage::remove(std::string_view table) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->remove(conn, table);
        });
}

ll::Expected<void> SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view column, std::string_view value) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table, key, column, value](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->set(conn, table, key, column, value);
        });
}

ll::Expected<void> SQLiteStorage::set(std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table, key, values](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->set(conn, table, key, values);
        });
}

ll::Expected<void> SQLiteStorage::del(std::string_view table, std::string_view key) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table, key](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->del(conn, table, key);
        });
}

ll::Expected<void> SQLiteStorage::del(std::string_view table, std::vector<std::string> keys) {
    return this->writeConnectionPool->getConnection()
        .and_then([this, table, keys](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<void> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->writeConnectionPool->returnConnection(conn); 
            });

            return this->del(conn, table, keys);
        });
}

ll::Expected<bool> SQLiteStorage::has(std::string_view table, std::string_view key) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, key](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<bool> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->has(conn, table, key);
        });
}

ll::Expected<bool> SQLiteStorage::has(std::string_view table) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<bool> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->has(conn, table);
        });
}

ll::Expected<std::unordered_map<std::string, std::string>> SQLiteStorage::get(std::string_view table, std::string_view key) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, key](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::unordered_map<std::string, std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->get(conn, table, key);
        });
}

ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> SQLiteStorage::get(std::string_view table, std::vector<std::string> keys) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, keys](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->get(conn, table, keys);
        });
}

ll::Expected<std::string> SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, key, column, defaultValue](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::string> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->get(conn, table, key, column, defaultValue);
        });
}

ll::Expected<std::string> SQLiteStorage::find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue, FindCondition match) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, conditions, defaultValue, match](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::string> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->find(conn, table, conditions, defaultValue, match);
        });
}

ll::Expected<std::vector<std::string>> SQLiteStorage::find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, conditions, match](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::vector<std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->find(conn, table, conditions, match);
        });
}

ll::Expected<std::vector<std::string>> SQLiteStorage::find(std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table, column, conditions, match](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::vector<std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->find(conn, table, column, conditions, match);
        });
}

ll::Expected<std::vector<std::string>> SQLiteStorage::list(std::string_view table) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::vector<std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->list(conn, table);
        });
}

ll::Expected<std::vector<std::string>> SQLiteStorage::list() {
    return this->readConnectionPool->getConnection()
        .and_then([this](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::vector<std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->list(conn);
        });
}

ll::Expected<std::vector<std::string>> SQLiteStorage::columns(std::string_view table) {
    return this->readConnectionPool->getConnection()
        .and_then([this, table](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<std::vector<std::string>> {
            auto guard = make_scope_guard([this, conn]() -> void { 
                this->readConnectionPool->returnConnection(conn); 
            });

            return this->columns(conn, table);
        });
}

SQLiteStorageTransaction::SQLiteStorageTransaction(SQLiteStorage& storage, std::shared_ptr<SQLiteStorage::ConnectionContext> conn) : mStorage(storage), mConnection(std::move(conn)) {
    this->mTransaction = std::make_unique<SQLite::Transaction>(*this->mConnection->database);
}

SQLiteStorageTransaction::~SQLiteStorageTransaction() {
    if (!this->mTransaction) 
        return;

    [[maybe_unused]] auto _ = this->rollback();
}

ll::Expected<SQLiteStorageTransaction> SQLiteStorageTransaction::create(SQLiteStorage& storage) {
    return storage.writeConnectionPool->getConnection()
        .and_then([&storage](std::shared_ptr<SQLiteStorage::ConnectionContext> conn) -> ll::Expected<SQLiteStorageTransaction> {
            return SQLiteStorageTransaction(storage, std::move(conn));
        });
}

ll::Expected<bool> SQLiteStorageTransaction::commit() {
    if (!this->mTransaction) 
        return false;

    try {
        this->mTransaction->commit();
        this->mTransaction.reset();
    } catch (...) {
        this->mTransaction.reset(); 
        
        this->mStorage.writeConnectionPool->returnConnection(this->mConnection);
        this->mConnection.reset();

        return ll::makeErrorCodeError(makeErrorCode(SQLiteError::SQLiteStorageErrorCode::TransactionCommitFailed));
    }

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
    this->mConnection.reset();

    return true;
}

ll::Expected<bool> SQLiteStorageTransaction::rollback() {
    if (!this->mTransaction) 
        return false;

    try {
        this->mTransaction->rollback();
        this->mTransaction.reset();
    } catch(...) {
        this->mTransaction.reset(); 

        this->mStorage.writeConnectionPool->returnConnection(mConnection);
        this->mConnection.reset();

        return ll::makeErrorCodeError(makeErrorCode(SQLiteError::SQLiteStorageErrorCode::TransactionRollbackFailed));
    }

    this->mStorage.writeConnectionPool->returnConnection(mConnection);
    this->mConnection.reset();

    return true;
}

std::shared_ptr<SQLiteStorage::ConnectionContext> SQLiteStorageTransaction::connection() const {
    return this->mConnection;
}
