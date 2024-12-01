#ifndef SQLITESTORAGE_H
#define SQLITESTORAGE_H

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>

#include <SQLiteCpp/SQLiteCpp.h>

#include "ExportLib.h"

class SQLiteStorage {
public:
    LOICOLLECTION_A_API   SQLiteStorage(const std::string& dbName) : database(dbName, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {};
    LOICOLLECTION_A_API   SQLiteStorage(const std::filesystem::path& dbPath) : database(dbPath.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {};
    LOICOLLECTION_A_API   ~SQLiteStorage() = default;

    LOICOLLECTION_A_API   void create(std::string_view table);
    LOICOLLECTION_A_API   void remove(std::string_view table);
    LOICOLLECTION_A_API   void set(std::string_view table, std::string_view key, std::string_view val);
    LOICOLLECTION_A_API   void update(std::string_view table, std::string_view key, std::string_view val);
    LOICOLLECTION_A_API   void del(std::string_view table, std::string_view key);
    
    LOICOLLECTION_A_NDAPI bool has(std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::string_view table);

    LOICOLLECTION_A_NDAPI std::string get(std::string_view table, std::string_view key, std::string_view default_val = "");

    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::string_view table);
    LOICOLLECTION_A_NDAPI std::vector<std::string> list();

private:
    SQLite::Database database;
};

#endif