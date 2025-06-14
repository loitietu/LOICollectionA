#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <unordered_map>

#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/Statement.h>

#include "base/ScopeGuard.h"

#include "SQLiteStorage.h"

SQLite::Statement& SQLiteStorage::getCachedStatement(const std::string& sql) {
    if (auto it = stmtCache.find(sql); it != stmtCache.end()) 
        return *it->second;
    
    auto stmt = std::make_unique<SQLite::Statement>(database, sql);
    return *stmtCache.emplace(sql, std::move(stmt)).first->second;
}

SQLiteStorage::SQLiteStorage(const std::filesystem::path& dbPath) : database(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
    database.exec("VACUUM;");

    database.exec("PRAGMA journal_mode = MEMORY;");
    database.exec("PRAGMA synchronous = NORMAL;");
    database.exec("PRAGMA temp_store = MEMORY;");
    database.exec("PRAGMA cache_size = 8096;"); 
}

void SQLiteStorage::create(std::string_view table) {
    database.exec("CREATE TABLE IF NOT EXISTS " + std::string(table) + " (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;");
}

void SQLiteStorage::remove(std::string_view table) {
    database.exec("DROP TABLE IF EXISTS " + std::string(table) + ";");

    stmtCache.clear();
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    auto& stmt = getCachedStatement(
        "INSERT INTO " + std::string(table) + " (key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value;"
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

void SQLiteStorage::del(std::string_view table, const std::vector<std::string>& keys) {
    std::string mBaseSql = "DELETE FROM " + std::string(table) + " WHERE key IN (";

    SQLite::Transaction transaction(database);
    
    size_t processed = 0;
    while (processed < keys.size()) {
        const size_t remaining = keys.size() - processed;
        const size_t batchSize = std::min(remaining, (size_t)500);

        std::string sql = mBaseSql;
        for (size_t i = 0; i < batchSize; ++i) {
            sql += "?";
            if (i < batchSize - 1)
                sql += ", ";
        }
        sql += ")";

        SQLite::Statement stmt(database, sql);
        for (size_t i = 0; i < batchSize; ++i)
            stmt.bind((int)i + 1, keys[processed + i]);
        stmt.exec();
    }

    transaction.commit();
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

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view default_val) {
    if (!has(table))
        return std::string(default_val);
    
    auto& stmt = getCachedStatement(
        "SELECT value FROM " + std::string(table) + " WHERE key = ?;"
    );

    auto guard = make_scope_guard([&] { stmt.reset(); });
    
    stmt.bind(1, std::string(key));
    if (stmt.executeStep()) 
        return stmt.getColumn(0).getText();
    return std::string(default_val);
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