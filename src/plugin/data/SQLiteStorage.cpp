#include <vector>
#include <string>
#include <string_view>

#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/Statement.h>

#include "SQLiteStorage.h"

void SQLiteStorage::create(std::string_view table) {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + std::string(table) + " (key TEXT PRIMARY KEY, value TEXT);";
    database.exec(sql);
}

void SQLiteStorage::remove(std::string_view table) {
    std::string sql = "DROP TABLE IF EXISTS " + std::string(table) + ";";
    SQLite::Statement query(database, sql);
    query.exec();
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    if (has(table, key)) return update(table, key, value);

    std::string sql = "INSERT OR REPLACE INTO " + std::string(table) + " VALUES (?, ?);";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(key));
    query.bind(2, std::string(value));
    query.exec();
}

void SQLiteStorage::update(std::string_view table, std::string_view key, std::string_view value) {
    std::string sql = "UPDATE " + std::string(table) + " SET value = ? WHERE key = ?;";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(value));
    query.bind(2, std::string(key));
    query.exec();
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    std::string sql = "DELETE FROM " + std::string(table) + " WHERE key = ?;";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(key));
    query.exec();
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    std::string sql = "SELECT * FROM " + std::string(table) + " WHERE key = ?;";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(key));
    return query.executeStep();
}

bool SQLiteStorage::has(std::string_view table) {
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?;";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(table));
    return query.executeStep();
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view default_val) {
    if (!has(table) || !has(table, key))
        return std::string(default_val);
    
    std::string sql = "SELECT value FROM " + std::string(table) + " WHERE key = ?;";
    SQLite::Statement query(database, sql);
    query.bind(1, std::string(key));
    if (query.executeStep())
        return query.getColumn(0).getText();
    return std::string(default_val);
}

std::string SQLiteStorage::find(std::string_view table, std::string_view key, int index) {
    std::string mTarget = std::string(key);
    for (auto& item : list(table)) {
        if (mTarget + std::to_string(index) != item)
            break;
        index++;
    }
    return mTarget + std::to_string(index);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    std::vector<std::string> keys{};
    std::string sql = "SELECT key FROM " + std::string(table) + ";";
    SQLite::Statement query(database, sql);
    while (query.executeStep())
        keys.push_back(query.getColumn(0).getString());
    return keys;
}

std::vector<std::string> SQLiteStorage::list() {
    std::vector<std::string> tables{};
    std::string sql = "SELECT name FROM sqlite_master WHERE type='table';";
    SQLite::Statement query(database, sql);
    while (query.executeStep())
        tables.push_back(query.getColumn(0).getString());
    return tables;
}