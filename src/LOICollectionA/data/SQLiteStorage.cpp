#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>

#include "LOICollectionA/base/ScopeGuard.h"

#include "LOICollectionA/data/SQLiteStorage.h"

SQLite::Statement& SQLiteStorage::getCachedStatement(const std::string& sql) {
    if (auto it = stmtCache.find(sql); it != stmtCache.end()) 
        return *it->second;
    
    auto stmt = std::make_unique<SQLite::Statement>(*database, sql);
    return *stmtCache.emplace(sql, std::move(stmt)).first->second;
}

SQLiteStorage::SQLiteStorage(const std::string& path) : database(std::make_unique<SQLite::Database>(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    database->exec("PRAGMA journal_mode = MEMORY;");
    database->exec("PRAGMA synchronous = NORMAL;");
    database->exec("PRAGMA temp_store = MEMORY;");
    database->exec("PRAGMA cache_size = 8096;"); 
}
SQLiteStorage::~SQLiteStorage() = default;

SQLite::Database* SQLiteStorage::getDatabase() {
    return database.get();
}

void SQLiteStorage::exec(std::string_view sql) {
    database->exec(std::string(sql));
}

void SQLiteStorage::create(std::string_view table) {
    database->exec("CREATE TABLE IF NOT EXISTS " + std::string(table) + " (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;");
}

void SQLiteStorage::remove(std::string_view table) {
    database->exec("DROP TABLE IF EXISTS " + std::string(table) + ";");

    stmtCache.clear();
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    auto& stmt = getCachedStatement(
        "INSERT INTO " + std::string(table) + " (key, value) VALUES (?, ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });

    stmt.bind(1, std::string(key));
    stmt.bind(2, std::string(value));
    stmt.exec();
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    auto& stmt = getCachedStatement(
        "DELETE FROM " + std::string(table) + " WHERE key = ?;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });

    stmt.bind(1, std::string(key));
    stmt.exec();
}

void SQLiteStorage::delByPrefix(std::string_view table, std::string_view prefix) {
    auto& stmt = getCachedStatement(
        "DELETE FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });

    stmt.bind(1, std::string(prefix) + "%");
    stmt.exec();
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    auto& stmt = getCachedStatement(
        "SELECT 1 FROM " + std::string(table) + " WHERE key = ? LIMIT 1;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    stmt.bind(1, std::string(key));

    return stmt.executeStep();
}

bool SQLiteStorage::has(std::string_view table) {
    auto& stmt = getCachedStatement(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ? LIMIT 1;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    stmt.bind(1, std::string(table));

    return stmt.executeStep();
}

bool SQLiteStorage::hasByPrefix(std::string_view table, std::string_view prefix, int size) {
    auto& stmt = getCachedStatement(
        "SELECT COUNT(*) FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    stmt.bind(1, std::string(prefix) + "%");

    if (!stmt.executeStep())
        return false;

    return stmt.getColumn(0).getInt() == size;
}

std::unordered_map<std::string, std::string> SQLiteStorage::get(std::string_view table) {
    std::unordered_map<std::string, std::string> result;
    
    auto& stmt = getCachedStatement(
        "SELECT key, value FROM " + std::string(table) + ";"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    while (stmt.executeStep()) {
        result.emplace(
            stmt.getColumn(0).getText(),
            stmt.getColumn(1).getText()
        );
    }
    
    return result;
}

std::unordered_map<std::string, std::string> SQLiteStorage::getByPrefix(std::string_view table, std::string_view prefix) {
    std::unordered_map<std::string, std::string> mResult;

    auto& stmt = getCachedStatement(
        "SELECT key, value FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    stmt.bind(1, std::string(prefix) + "%");
    while (stmt.executeStep()) {
        mResult.emplace(
            stmt.getColumn(0).getText(),
            stmt.getColumn(1).getText()
        );
    }

    return mResult;
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view defaultValue) {
    if (!has(table))
        return std::string(defaultValue);
    
    auto& stmt = getCachedStatement(
        "SELECT value FROM " + std::string(table) + " WHERE key = ?;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    stmt.bind(1, std::string(key));
    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();

    return std::string(defaultValue);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    std::vector<std::string> keys;
    
    auto& stmt = getCachedStatement(
        "SELECT key FROM " + std::string(table) + ";"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

std::vector<std::string> SQLiteStorage::list() {
    std::vector<std::string> tables;
    
    auto& stmt = getCachedStatement(
        "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    while (stmt.executeStep())
        tables.emplace_back(stmt.getColumn(0).getText());

    return tables;
}

std::vector<std::string> SQLiteStorage::listByPrefix(std::string_view table, std::string_view prefix) {
    std::vector<std::string> keys;

    auto& stmt = getCachedStatement(
        "SELECT key FROM " + std::string(table) + " WHERE key LIKE ? ESCAPE '\\';"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });

    stmt.bind(1, std::string(prefix) + "%");
    while (stmt.executeStep())
        keys.emplace_back(stmt.getColumn(0).getText());

    return keys;
}

void SQLiteStorageBatch::set(std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> value) {
    const std::string mBaseSql = "INSERT INTO " + std::string(table) + " (key, value) VALUES ";
        
    auto it = value.begin();
    while (it != value.end()) {
        const size_t remaining = std::distance(it, value.end());
        const size_t batchSize = std::min(remaining, (size_t) 500);

        std::string placeholders;
        placeholders.reserve(batchSize * 8);

        for (size_t i = 0; i < batchSize; ++i)
            placeholders += (i == 0) ? "(?, ?)" : ", (?, ?)";

        SQLite::Statement stmt(*database, mBaseSql + placeholders + " ON CONFLICT(key) DO UPDATE SET value = excluded.value;");

        for (size_t i = 0; i < batchSize; ++i) {
            stmt.bind(static_cast<int>(i * 2 + 1), std::string(key) + "." + it->first);
            stmt.bind(static_cast<int>(i * 2 + 2), it->second);
            ++it;
        }

        stmt.exec();
    }
}
