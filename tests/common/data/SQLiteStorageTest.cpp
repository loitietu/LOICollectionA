#include <gtest/gtest.h>

#include <vector>
#include <string>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "LOICollectionA/data/SQLiteStorage.h"

class SQLiteStorageTest : public testing::Test {
protected:
    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "sqlite_storage_test";

        std::filesystem::create_directories(tempDir);

        storage = std::make_unique<SQLiteStorage>((tempDir / "test.db").string(), 2, 1, 10);
    }

    void TearDown() override {
        storage.reset();

        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    std::filesystem::path tempDir;
    std::unique_ptr<SQLiteStorage> storage;
};

TEST_F(SQLiteStorageTest, CreateTableAndSetGetSingleColumn) {
    storage->create("users", [](SQLiteStorage::ColumnCallback add) -> void {
        add("name");
        add("age");
    });

    EXPECT_TRUE(storage->has("users"));

    storage->set("users", "user1", "name", "Alice");
    storage->set("users", "user1", "age", "30");

    auto result = storage->get("users", "user1");
    EXPECT_EQ(result["name"], "Alice");
    EXPECT_EQ(result["age"], "30");
}

TEST_F(SQLiteStorageTest, SetMultipleColumnsAndGet) {
    storage->create("products", [](SQLiteStorage::ColumnCallback add) -> void {
        add("price");
        add("stock");
    });

    std::unordered_map<std::string, std::string> values = {
        {"price", "9.99"},
        {"stock", "100"}
    };
    storage->set("products", "item1", values);

    auto result = storage->get("products", "item1");
    EXPECT_EQ(result["price"], "9.99");
    EXPECT_EQ(result["stock"], "100");
}

TEST_F(SQLiteStorageTest, DeleteSingleKey) {
    storage->create("cache", [](SQLiteStorage::ColumnCallback add) -> void {
        add("value");
    });

    storage->set("cache", "key1", "value", "data");
    EXPECT_TRUE(storage->has("cache", "key1"));

    storage->del("cache", "key1");
    EXPECT_FALSE(storage->has("cache", "key1"));
}

TEST_F(SQLiteStorageTest, DeleteMultipleKeys) {
    storage->create("temp", [](SQLiteStorage::ColumnCallback add) -> void {
        add("info");
    });

    storage->set("temp", "k1", "info", "a");
    storage->set("temp", "k2", "info", "b");
    storage->set("temp", "k3", "info", "c");

    EXPECT_TRUE(storage->has("temp", "k1"));
    EXPECT_TRUE(storage->has("temp", "k2"));
    EXPECT_TRUE(storage->has("temp", "k3"));

    storage->del("temp", std::vector<std::string>{"k1", "k3"});
    EXPECT_FALSE(storage->has("temp", "k1"));
    EXPECT_TRUE(storage->has("temp", "k2"));
    EXPECT_FALSE(storage->has("temp", "k3"));
}

TEST_F(SQLiteStorageTest, ListKeysAndTables) {
    storage->create("notes", [](SQLiteStorage::ColumnCallback add) -> void {
        add("content");
    });

    storage->set("notes", "n1", "content", "hello");
    storage->set("notes", "n2", "content", "world");

    auto keys = storage->list("notes");
    EXPECT_EQ(keys.size(), 2);
    EXPECT_NE(std::find(keys.begin(), keys.end(), "n1"), keys.end());
    EXPECT_NE(std::find(keys.begin(), keys.end(), "n2"), keys.end());

    auto tables = storage->list();
    EXPECT_GE(tables.size(), 1);
    EXPECT_NE(std::find(tables.begin(), tables.end(), "notes"), tables.end());
}

TEST_F(SQLiteStorageTest, GetColumns) {
    storage->create("columns_test", [](SQLiteStorage::ColumnCallback add) -> void {
        add("col_a");
        add("col_b");
    });

    auto cols = storage->columns("columns_test");

    ASSERT_EQ(cols.size(), 5);
    EXPECT_EQ(cols[0], "key");
    EXPECT_EQ(cols[1], "created_at");
    EXPECT_EQ(cols[2], "updated_at");
    EXPECT_EQ(cols[3], "col_a");
    EXPECT_EQ(cols[4], "col_b");
}

TEST_F(SQLiteStorageTest, FindByCondition) {
    storage->create("books", [](SQLiteStorage::ColumnCallback add) -> void {
        add("author");
        add("year");
    });

    storage->set("books", "b1", "author", "Orwell");
    storage->set("books", "b1", "year", "1949");
    storage->set("books", "b2", "author", "Huxley");
    storage->set("books", "b2", "year", "1932");

    auto keys = storage->find("books", {{"author", "Orwell"}}, SQLiteStorage::FindCondition::AND);
    ASSERT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], "b1");

    auto keys2 = storage->find("books", {{"author", "Huxley"}, {"year", "1932"}}, SQLiteStorage::FindCondition::AND);
    ASSERT_EQ(keys2.size(), 1);
    EXPECT_EQ(keys2[0], "b2");

    auto keys3 = storage->find("books", {{"author", "Orwell"}, {"year", "1932"}}, SQLiteStorage::FindCondition::OR);
    EXPECT_EQ(keys3.size(), 2);
}

TEST_F(SQLiteStorageTest, FindColumnValues) {
    storage->create("scores", [](SQLiteStorage::ColumnCallback add) -> void {
        add("points");
    });

    storage->set("scores", "p1", "points", "100");
    storage->set("scores", "p2", "points", "200");
    storage->set("scores", "p3", "points", "100");

    auto values = storage->find("scores", "points", {{"points", "100"}});
    
    ASSERT_EQ(values.size(), 1);
    EXPECT_EQ(values[0], "100");
}

TEST_F(SQLiteStorageTest, FindOrDefault) {
    storage->create("inventory", [](SQLiteStorage::ColumnCallback add) -> void {
        add("qty");
    });

    storage->set("inventory", "a", "qty", "5");

    std::string key = storage->find("inventory", {{"qty", "5"}}, "");
    EXPECT_EQ(key, "a");

    std::string notFound = storage->find("inventory", {{"qty", "999"}}, "default_key");
    EXPECT_EQ(notFound, "default_key");
}

TEST_F(SQLiteStorageTest, RemoveTable) {
    storage->create("to_delete", [](SQLiteStorage::ColumnCallback add) -> void {
        add("field");
    });
    EXPECT_TRUE(storage->has("to_delete"));

    storage->remove("to_delete");
    EXPECT_FALSE(storage->has("to_delete"));
}

TEST_F(SQLiteStorageTest, ExecRawSQL) {
    storage->exec("CREATE TABLE raw_test (id INTEGER PRIMARY KEY);");
    EXPECT_TRUE(storage->has("raw_test"));

    storage->exec("DROP TABLE raw_test;");
    EXPECT_FALSE(storage->has("raw_test"));
}

TEST_F(SQLiteStorageTest, TransactionCommit) {
    storage->create("tx_test", [](SQLiteStorage::ColumnCallback add) -> void {
        add("val");
    });

    {
        SQLiteStorageTransaction txn(*storage);
        ASSERT_TRUE(txn.connection() != nullptr);

        storage->set(txn.connection(), "tx_test", "tx_key", "val", "committed");
        EXPECT_TRUE(txn.commit());
    }

    auto result = storage->get("tx_test", "tx_key");
    EXPECT_EQ(result["val"], "committed");
}

TEST_F(SQLiteStorageTest, TransactionRollback) {
    storage->create("tx_rollback", [](SQLiteStorage::ColumnCallback add) -> void {
        add("val");
    });

    {
        SQLiteStorageTransaction txn(*storage);
        storage->set(txn.connection(), "tx_rollback", "r_key", "val", "should_rollback");
    }

    EXPECT_FALSE(storage->has("tx_rollback", "r_key"));
}

TEST_F(SQLiteStorageTest, ReadOnlyOperations) {
    storage->create("readonly", [](SQLiteStorage::ColumnCallback add) -> void {
        add("data");
    });

    storage->set("readonly", "ro_key", "data", "ro_value");

    EXPECT_TRUE(storage->has("readonly", "ro_key"));

    auto val = storage->get("readonly", "ro_key", "data", "default");
    EXPECT_EQ(val, "ro_value");
}

TEST_F(SQLiteStorageTest, BatchGetKeys) {
    storage->create("batch", [](SQLiteStorage::ColumnCallback add) -> void {
        add("info");
    });

    storage->set("batch", "b1", "info", "first");
    storage->set("batch", "b2", "info", "second");
    storage->set("batch", "b3", "info", "third");

    std::vector<std::string> keys = {"b1", "b2", "b3", "b4"};
    auto result = storage->get("batch", keys);

    ASSERT_TRUE(result.contains("b1"));
    ASSERT_TRUE(result.contains("b2"));
    ASSERT_TRUE(result.contains("b3"));
    ASSERT_FALSE(result.contains("b4"));

    EXPECT_EQ(result["b1"]["info"], "first");
    EXPECT_EQ(result["b2"]["info"], "second");
    EXPECT_EQ(result["b3"]["info"], "third");
}
