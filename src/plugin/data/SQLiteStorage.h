#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/Macro.h"

namespace SQLite {
    class Database;
    class Statement;
}

class SQLiteStorage {
public:
    LOICOLLECTION_A_API   explicit SQLiteStorage(const std::string& path);
    LOICOLLECTION_A_API   ~SQLiteStorage();

    LOICOLLECTION_A_NDAPI SQLite::Database* getDatabase();

    LOICOLLECTION_A_API   void exec(std::string_view sql);

    LOICOLLECTION_A_API   void create(std::string_view table);
    LOICOLLECTION_A_API   void remove(std::string_view table);
    LOICOLLECTION_A_API   void set(std::string_view table, std::string_view key, std::string_view value);
    LOICOLLECTION_A_API   void del(std::string_view table, std::string_view key);
    LOICOLLECTION_A_API   void delByPrefix(std::string_view table, std::string_view prefix);
    
    LOICOLLECTION_A_NDAPI bool has(std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::string_view table);
    LOICOLLECTION_A_NDAPI bool hasByPrefix(std::string_view table, std::string_view prefix, int size);

    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> get(std::string_view table);
    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getByPrefix(std::string_view table, std::string_view prefix);

    LOICOLLECTION_A_NDAPI std::string get(std::string_view table, std::string_view key, std::string_view defaultValue = "");

    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::string_view table);
    LOICOLLECTION_A_NDAPI std::vector<std::string> list();
    LOICOLLECTION_A_NDAPI std::vector<std::string> listByPrefix(std::string_view table, std::string_view prefix);

protected:
    std::unique_ptr<SQLite::Database> database;

    std::unordered_map<std::string, std::unique_ptr<SQLite::Statement>> stmtCache;
    
    SQLite::Statement& getCachedStatement(const std::string& sql);
};

class SQLiteStorageBatch : public SQLiteStorage {
public:
    LOICOLLECTION_A_API void set(std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> value);
};
