#pragma once

#include <mutex>
#include <queue>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

namespace SQLite {
    class Database;
    class Statement;
    class Transaction;
}

class SQLiteConnectionPool;

class SQLiteStorage {
protected:
    std::unique_ptr<SQLiteConnectionPool> writeConnectionPool;
    std::unique_ptr<SQLiteConnectionPool> readConnectionPool;

    struct ConnectionContext {
        std::unique_ptr<SQLite::Database> database;
        
        ConnectionContext(const std::string& path, bool readOnly = false);
    };
    
    SQLite::Statement& getCachedStatement(ConnectionContext& context, const std::string& sql);

public:
    LOICOLLECTION_A_API   explicit SQLiteStorage(const std::string& path, size_t size = 5);
    LOICOLLECTION_A_API   ~SQLiteStorage();

    LOICOLLECTION_A_API   void exec(std::shared_ptr<ConnectionContext> context, std::string_view sql);
    LOICOLLECTION_A_API   void create(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_API   void remove(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_API   void set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view value);
    LOICOLLECTION_A_API   void del(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key);
    LOICOLLECTION_A_API   void delByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix);

    LOICOLLECTION_A_NDAPI bool has(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_NDAPI bool hasByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix, int size);

    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> get(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix);

    LOICOLLECTION_A_NDAPI std::string get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view defaultValue = "");

    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::shared_ptr<ConnectionContext> context);
    LOICOLLECTION_A_NDAPI std::vector<std::string> listByPrefix(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view prefix);

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

public:
    friend class SQLiteConnectionPool;
    friend class SQLiteStorageTransaction;
};

class SQLiteConnectionPool final {
public:
    LOICOLLECTION_A_API   explicit SQLiteConnectionPool(const std::string& path, size_t size, bool readOnly = false);
    LOICOLLECTION_A_API   ~SQLiteConnectionPool();
    
    LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage::ConnectionContext> getConnection();
    
    LOICOLLECTION_A_API   void returnConnection(std::shared_ptr<SQLiteStorage::ConnectionContext> conn);

private:
    mutable std::mutex mMutex;

    std::vector<std::shared_ptr<SQLiteStorage::ConnectionContext>> mConnections;
    std::queue<std::shared_ptr<SQLiteStorage::ConnectionContext>> mAvailableConnections;
    
    std::condition_variable mConnectionAvailable;

    std::atomic<size_t> mActiveConnections{ 0 };
};

class SQLiteStorageTransaction final {
public:
    LOICOLLECTION_A_API explicit SQLiteStorageTransaction(SQLiteStorage& storage);
    LOICOLLECTION_A_API ~SQLiteStorageTransaction();

    LOICOLLECTION_A_API   bool commit();
    LOICOLLECTION_A_API   bool rollback();

    LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage::ConnectionContext> connection() const;

private:
    SQLiteStorage& mStorage;

    std::shared_ptr<SQLiteStorage::ConnectionContext> mConnection;
    std::unique_ptr<SQLite::Transaction> mTransaction;
};
