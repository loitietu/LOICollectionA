#pragma once

#include <mutex>
#include <queue>
#include <memory>
#include <vector>
#include <string>
#include <functional>
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

    struct ConnectionContext;
    
    SQLite::Statement& getCachedStatement(ConnectionContext& context, const std::string& sql);

public:
    using ColumnCallback = std::function<void(std::string)>;
    using CreateCallback = std::function<void(ColumnCallback)>;

    enum class FindCondition { 
        AND,
        OR
    };

public:
    LOICOLLECTION_A_API   explicit SQLiteStorage(const std::string& path, size_t readSize = 10, size_t writeSize = 2, size_t cacheSize = 100);
    LOICOLLECTION_A_API   ~SQLiteStorage();

    LOICOLLECTION_A_API   void exec(std::shared_ptr<ConnectionContext> context, std::string_view sql);
    LOICOLLECTION_A_API   void create(std::shared_ptr<ConnectionContext> context, std::string_view table, CreateCallback callback);
    LOICOLLECTION_A_API   void remove(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_API   void set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view value);
    LOICOLLECTION_A_API   void set(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values);
    LOICOLLECTION_A_API   void del(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key);

    LOICOLLECTION_A_NDAPI bool has(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::shared_ptr<ConnectionContext> context, std::string_view table);

    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::unordered_map<std::string, std::string>> get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::string> keys);

    LOICOLLECTION_A_NDAPI std::string get(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue = "");
    
    LOICOLLECTION_A_NDAPI std::string find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue = "", FindCondition match = FindCondition::AND);
   
    LOICOLLECTION_A_NDAPI std::vector<std::string> find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match = FindCondition::AND);
    LOICOLLECTION_A_NDAPI std::vector<std::string> find(std::shared_ptr<ConnectionContext> context, std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match = FindCondition::AND);

    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::shared_ptr<ConnectionContext> context, std::string_view table);
    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::shared_ptr<ConnectionContext> context);

    LOICOLLECTION_A_NDAPI std::vector<std::string> columns(std::shared_ptr<ConnectionContext> context, std::string_view table);

    LOICOLLECTION_A_API   void exec(std::string_view sql);

    LOICOLLECTION_A_API   void create(std::string_view table, CreateCallback callback);
    LOICOLLECTION_A_API   void remove(std::string_view table);
    LOICOLLECTION_A_API   void set(std::string_view table, std::string_view key, std::string_view column, std::string_view value);
    LOICOLLECTION_A_API   void set(std::string_view table, std::string_view key, std::unordered_map<std::string, std::string> values);
    LOICOLLECTION_A_API   void del(std::string_view table, std::string_view key);
    
    LOICOLLECTION_A_NDAPI bool has(std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI bool has(std::string_view table);

    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> get(std::string_view table, std::string_view key);
    LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::unordered_map<std::string, std::string>> get(std::string_view table, std::vector<std::string> keys);

    LOICOLLECTION_A_NDAPI std::string get(std::string_view table, std::string_view key, std::string_view column, std::string_view defaultValue = "");

    LOICOLLECTION_A_NDAPI std::string find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, std::string_view defaultValue = "", FindCondition match = FindCondition::AND);

    LOICOLLECTION_A_NDAPI std::vector<std::string> find(std::string_view table, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match = FindCondition::AND);
    LOICOLLECTION_A_NDAPI std::vector<std::string> find(std::string_view table, std::string_view column, std::vector<std::pair<std::string, std::string>> conditions, FindCondition match = FindCondition::AND);

    LOICOLLECTION_A_NDAPI std::vector<std::string> list(std::string_view table);
    LOICOLLECTION_A_NDAPI std::vector<std::string> list();

    LOICOLLECTION_A_NDAPI std::vector<std::string> columns(std::string_view table);

public:
    friend class SQLiteConnectionPool;
    friend class SQLiteStorageBatch;
    friend class SQLiteStorageTransaction;
};

class SQLiteConnectionPool final {
public:
    LOICOLLECTION_A_API   explicit SQLiteConnectionPool(const std::string& path, size_t size, size_t cacheSize = 100, bool readOnly = false);
    LOICOLLECTION_A_API   ~SQLiteConnectionPool();
    
    LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage::ConnectionContext> getConnection(int timeout = 5000);
    
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
    LOICOLLECTION_A_API   explicit SQLiteStorageTransaction(SQLiteStorage& storage, bool readOnly = false);
    LOICOLLECTION_A_API   ~SQLiteStorageTransaction();

    LOICOLLECTION_A_API   bool commit();
    LOICOLLECTION_A_API   bool rollback();

    LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage::ConnectionContext> connection() const;

private:
    SQLiteStorage& mStorage;

    std::shared_ptr<SQLiteStorage::ConnectionContext> mConnection;
    std::unique_ptr<SQLite::Transaction> mTransaction;
};
