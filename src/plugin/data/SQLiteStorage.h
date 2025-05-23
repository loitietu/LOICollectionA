#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <filesystem>

#include <SQLiteCpp/SQLiteCpp.h>

#include "base/Macro.h"

class SQLiteStorage {
public:
    LOICOLLECTION_A_API   explicit SQLiteStorage(const std::filesystem::path& dbPath);
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