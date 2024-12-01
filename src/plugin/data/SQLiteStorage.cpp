#include <vector>
#include <string>
#include <string_view>

#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/Statement.h>

#include "SQLiteStorage.h"

void SQLiteStorage::create(std::string_view table) {
    database.exec("CREATE TABLE IF NOT EXISTS " + std::string(table) + " (key TEXT PRIMARY KEY, value TEXT);");
}

void SQLiteStorage::remove(std::string_view table) {
    SQLite::Statement query(database, "DROP TABLE IF EXISTS " + std::string(table) + ";");
    query.exec();
}

void SQLiteStorage::set(std::string_view table, std::string_view key, std::string_view value) {
    if (has(table, key)) return update(table, key, value);

    SQLite::Statement query(database, "INSERT OR REPLACE INTO " + std::string(table) + " VALUES (?, ?);");
    query.bind(1, std::string(key));
    query.bind(2, std::string(value));
    query.exec();
}

void SQLiteStorage::update(std::string_view table, std::string_view key, std::string_view value) {
    SQLite::Statement query(database, "UPDATE " + std::string(table) + " SET value = ? WHERE key = ?;");
    query.bind(1, std::string(value));
    query.bind(2, std::string(key));
    query.exec();
}

void SQLiteStorage::del(std::string_view table, std::string_view key) {
    SQLite::Statement query(database, "DELETE FROM " + std::string(table) + " WHERE key = ?;");
    query.bind(1, std::string(key));
    query.exec();
}

bool SQLiteStorage::has(std::string_view table, std::string_view key) {
    SQLite::Statement query(database, "SELECT * FROM " + std::string(table) + " WHERE key = ?;");
    query.bind(1, std::string(key));
    return query.executeStep();
}

bool SQLiteStorage::has(std::string_view table) {
    SQLite::Statement query(database, "SELECT name FROM sqlite_master WHERE type='table' AND name=?;");
    query.bind(1, std::string(table));
    return query.executeStep();
}

std::string SQLiteStorage::get(std::string_view table, std::string_view key, std::string_view default_val) {
    if (!has(table) || !has(table, key))
        return std::string(default_val);

    SQLite::Statement query(database, "SELECT value FROM " + std::string(table) + " WHERE key = ?;");
    query.bind(1, std::string(key));
    if (query.executeStep())
        return query.getColumn(0).getText();
    return std::string(default_val);
}

std::vector<std::string> SQLiteStorage::list(std::string_view table) {
    std::vector<std::string> keys;
    SQLite::Statement query(database, "SELECT key FROM " + std::string(table) + ";");
    while (query.executeStep())
        keys.push_back(query.getColumn(0).getText());
    return keys;
}

std::vector<std::string> SQLiteStorage::list() {
    std::vector<std::string> tables;
    SQLite::Statement query(database, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;");
    while (query.executeStep())
        tables.push_back(query.getColumn(0).getText());
    return tables;
}