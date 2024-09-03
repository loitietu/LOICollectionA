#ifndef SQLITESTORAGE_H
#define SQLITESTORAGE_H

#include <string>
#include <vector>
#include <filesystem>

#include <SQLiteCpp/SQLiteCpp.h>

class SQLiteStorage {
public:
    SQLiteStorage(const std::string& dbName) : database(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {};
    SQLiteStorage(const std::filesystem::path& dbPath) : database(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {};

    void create(std::string_view table);
    void remove(std::string_view table);
    void set(std::string_view table, std::string_view key, std::string_view val);
    void update(std::string_view table, std::string_view key, std::string_view val);
    void del(std::string_view table, std::string_view key);
    
    bool has(std::string_view table, std::string_view key);
    bool has(std::string_view table);

    std::string get(std::string_view table, std::string_view key);
    std::string find(std::string_view table, std::string_view key, int index);

    std::vector<std::string> list(std::string_view table);
    std::vector<std::string> list();
    std::vector<std::string> list2(std::string_view target);

private:
    SQLite::Database database;
};

#endif