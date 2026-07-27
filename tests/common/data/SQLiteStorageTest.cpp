#include <gtest/gtest.h>

#include <vector>
#include <string>
#include <filesystem>
#include <string_view>
#include <unordered_map>

#include "LOICollectionA/data/SQLiteStorage.h"

class SQLiteStorageTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        tempDir = std::filesystem::temp_directory_path() / "sqlite_storage_test";

        std::filesystem::create_directories(tempDir);

        storage = std::make_unique<SQLiteStorage>((tempDir / "test.db").string(), 2, 1, 10);
    }

    static void TearDownTestSuite() {
        storage.reset();

        std::filesystem::remove_all(tempDir);
    }

    static std::filesystem::path tempDir;
    static std::unique_ptr<SQLiteStorage> storage;
};

std::filesystem::path SQLiteStorageTest::tempDir;
std::unique_ptr<SQLiteStorage> SQLiteStorageTest::storage;

TEST_F(SQLiteStorageTest, CreateTableAndSetGetSingleColumn) {
    ASSERT_TRUE(storage->create("users", [](SQLiteStorage::ColumnCallback add) -> void {
        add("name");
        add("age");
    }).has_value());

    auto has = storage->has("users");
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());

    ASSERT_TRUE(storage->set("users", "user1", "name", "Alice").has_value());
    ASSERT_TRUE(storage->set("users", "user1", "age", "30").has_value());

    auto result = storage->get("users", "user1");
    EXPECT_TRUE(result.has_value());

    auto& maps = result.value();
    EXPECT_EQ(maps["name"], "Alice");
    EXPECT_EQ(maps["age"], "30");
}

TEST_F(SQLiteStorageTest, SetMultipleColumnsAndGet) {
    ASSERT_TRUE(storage->create("products", [](SQLiteStorage::ColumnCallback add) -> void {
        add("price");
        add("stock");
    }).has_value());

    std::unordered_map<std::string, std::string> values = {
        {"price", "9.99"},
        {"stock", "100"}
    };
    ASSERT_TRUE(storage->set("products", "item1", values).has_value());

    auto result = storage->get("products", "item1");
    EXPECT_TRUE(result.has_value());

    auto& maps = result.value();
    EXPECT_EQ(maps["price"], "9.99");
    EXPECT_EQ(maps["stock"], "100");
}

TEST_F(SQLiteStorageTest, DeleteSingleKey) {
    ASSERT_TRUE(storage->create("cache", [](SQLiteStorage::ColumnCallback add) -> void {
        add("value");
    }).has_value());

    ASSERT_TRUE(storage->set("cache", "key1", "value", "data").has_value());

    auto has = storage->has("cache", "key1");
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());

    ASSERT_TRUE(storage->del("cache", "key1").has_value());

    auto has2 = storage->has("cache", "key1");
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(SQLiteStorageTest, DeleteMultipleKeys) {
    ASSERT_TRUE(storage->create("temp", [](SQLiteStorage::ColumnCallback add) -> void {
        add("info");
    }).has_value());

    ASSERT_TRUE(storage->set("temp", "k1", "info", "a").has_value());
    ASSERT_TRUE(storage->set("temp", "k2", "info", "b").has_value());
    ASSERT_TRUE(storage->set("temp", "k3", "info", "c").has_value());

    auto has1 = storage->has("temp", "k1");
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    auto has2 = storage->has("temp", "k2");
    EXPECT_TRUE(has2.has_value());
    EXPECT_TRUE(has2.value());

    auto has3 = storage->has("temp", "k3");
    EXPECT_TRUE(has3.has_value());
    EXPECT_TRUE(has3.value());

    ASSERT_TRUE(storage->del("temp", std::vector<std::string>{"k1", "k3"}).has_value());

    auto del1 = storage->has("temp", "k1");
    EXPECT_TRUE(del1.has_value());
    EXPECT_FALSE(del1.value());

    auto del2 = storage->has("temp", "k2");
    EXPECT_TRUE(del2.has_value());
    EXPECT_TRUE(del2.value());

    auto del3 = storage->has("temp", "k3");
    EXPECT_TRUE(del3.has_value());
    EXPECT_FALSE(del3.value());
}

TEST_F(SQLiteStorageTest, ListKeysAndTables) {
    ASSERT_TRUE(storage->create("notes", [](SQLiteStorage::ColumnCallback add) -> void {
        add("content");
    }).has_value());

    ASSERT_TRUE(storage->set("notes", "n1", "content", "hello").has_value());
    ASSERT_TRUE(storage->set("notes", "n2", "content", "world").has_value());

    auto keys = storage->list("notes");
    EXPECT_TRUE(keys.has_value());
    EXPECT_EQ(keys.value().size(), 2);
    EXPECT_NE(std::find(keys.value().begin(), keys.value().end(), "n1"), keys.value().end());
    EXPECT_NE(std::find(keys.value().begin(), keys.value().end(), "n2"), keys.value().end());

    auto tables = storage->list();
    EXPECT_TRUE(tables.has_value());
    EXPECT_GE(tables.value().size(), 1);
    EXPECT_NE(std::find(tables.value().begin(), tables.value().end(), "notes"), tables.value().end());
}

TEST_F(SQLiteStorageTest, GetColumns) {
    ASSERT_TRUE(storage->create("columns_test", [](SQLiteStorage::ColumnCallback add) -> void {
        add("col_a");
        add("col_b");
    }).has_value());

    auto cols = storage->columns("columns_test");
    EXPECT_TRUE(cols.has_value());

    auto& vecs = cols.value();
    ASSERT_EQ(vecs.size(), 5);
    EXPECT_EQ(vecs[0], "key");
    EXPECT_EQ(vecs[1], "created_at");
    EXPECT_EQ(vecs[2], "updated_at");
    EXPECT_EQ(vecs[3], "col_a");
    EXPECT_EQ(vecs[4], "col_b");
}

TEST_F(SQLiteStorageTest, FindByCondition) {
    ASSERT_TRUE(storage->create("books", [](SQLiteStorage::ColumnCallback add) -> void {
        add("author");
        add("year");
    }).has_value());

    ASSERT_TRUE(storage->set("books", "b1", "author", "Orwell").has_value());
    ASSERT_TRUE(storage->set("books", "b1", "year", "1949").has_value());
    ASSERT_TRUE(storage->set("books", "b2", "author", "Huxley").has_value());
    ASSERT_TRUE(storage->set("books", "b2", "year", "1932").has_value());

    auto keys = storage->find("books", {{"author", "Orwell"}}, SQLiteStorage::FindCondition::AND);
    EXPECT_TRUE(keys.has_value());
    ASSERT_EQ(keys.value().size(), 1);
    EXPECT_EQ(keys.value()[0], "b1");

    auto keys2 = storage->find("books", {{"author", "Huxley"}, {"year", "1932"}}, SQLiteStorage::FindCondition::AND);
    EXPECT_TRUE(keys2.has_value());
    ASSERT_EQ(keys2.value().size(), 1);
    EXPECT_EQ(keys2.value()[0], "b2");

    auto keys3 = storage->find("books", {{"author", "Orwell"}, {"year", "1932"}}, SQLiteStorage::FindCondition::OR);
    EXPECT_TRUE(keys3.has_value());
    EXPECT_EQ(keys3.value().size(), 2);
}

TEST_F(SQLiteStorageTest, FindColumnValues) {
    ASSERT_TRUE(storage->create("scores", [](SQLiteStorage::ColumnCallback add) -> void {
        add("points");
    }).has_value());

    ASSERT_TRUE(storage->set("scores", "p1", "points", "100").has_value());
    ASSERT_TRUE(storage->set("scores", "p2", "points", "200").has_value());
    ASSERT_TRUE(storage->set("scores", "p3", "points", "100").has_value());

    auto values = storage->find("scores", "points", {{"points", "100"}});
    EXPECT_TRUE(values.has_value());

    auto& vecs = values.value();
    ASSERT_EQ(vecs.size(), 1);
    EXPECT_EQ(vecs[0], "100");
}

TEST_F(SQLiteStorageTest, FindOrDefault) {
    ASSERT_TRUE(storage->create("inventory", [](SQLiteStorage::ColumnCallback add) -> void {
        add("qty");
    }).has_value());

    ASSERT_TRUE(storage->set("inventory", "a", "qty", "5").has_value());

    auto key = storage->find("inventory", {{"qty", "5"}}, "");
    EXPECT_TRUE(key.has_value());
    EXPECT_EQ(key.value(), "a");

    auto notFound = storage->find("inventory", {{"qty", "999"}}, "default_key");
    EXPECT_TRUE(notFound.has_value());
    EXPECT_EQ(notFound.value(), "default_key");
}

TEST_F(SQLiteStorageTest, RemoveTable) {
    ASSERT_TRUE(storage->create("to_delete", [](SQLiteStorage::ColumnCallback add) -> void {
        add("field");
    }).has_value());

    auto has = storage->has("to_delete");
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());

    ASSERT_TRUE(storage->remove("to_delete").has_value());

    auto has2 = storage->has("to_delete");
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(SQLiteStorageTest, ExecRawSQL) {
    ASSERT_TRUE(storage->exec("CREATE TABLE raw_test (id INTEGER PRIMARY KEY);").has_value());

    auto has = storage->has("raw_test");
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());

    ASSERT_TRUE(storage->exec("DROP TABLE raw_test;").has_value());

    auto has2 = storage->has("raw_test");
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(SQLiteStorageTest, TransactionCommit) {
    ASSERT_TRUE(storage->create("tx_test", [](SQLiteStorage::ColumnCallback add) -> void {
        add("val");
    }).has_value());

    {
        auto txn = SQLiteStorageTransaction::create(*storage);
        ASSERT_TRUE(txn.has_value());

        ASSERT_TRUE(storage->set(txn.value().connection(), "tx_test", "tx_key", "val", "committed").has_value());
        EXPECT_TRUE(txn.value().commit().has_value());
    }

    auto result = storage->get("tx_test", "tx_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value()["val"], "committed");
}

TEST_F(SQLiteStorageTest, TransactionRollback) {
    ASSERT_TRUE(storage->create("tx_rollback", [](SQLiteStorage::ColumnCallback add) -> void {
        add("val");
    }).has_value());

    {
        auto txn = SQLiteStorageTransaction::create(*storage);
        ASSERT_TRUE(txn.has_value());

        ASSERT_TRUE(storage->set(txn.value().connection(), "tx_rollback", "r_key", "val", "should_rollback").has_value());
    }

    auto has = storage->has("tx_rollback", "r_key");
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
}

TEST_F(SQLiteStorageTest, ReadOnlyOperations) {
    ASSERT_TRUE(storage->create("readonly", [](SQLiteStorage::ColumnCallback add) -> void {
        add("data");
    }).has_value());

    ASSERT_TRUE(storage->set("readonly", "ro_key", "data", "ro_value").has_value());

    auto has = storage->has("readonly", "ro_key");
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());

    auto val = storage->get("readonly", "ro_key", "data", "default");
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "ro_value");
}

TEST_F(SQLiteStorageTest, BatchGetKeys) {
    ASSERT_TRUE(storage->create("batch", [](SQLiteStorage::ColumnCallback add) -> void {
        add("info");
    }).has_value());

    ASSERT_TRUE(storage->set("batch", "b1", "info", "first").has_value());
    ASSERT_TRUE(storage->set("batch", "b2", "info", "second").has_value());
    ASSERT_TRUE(storage->set("batch", "b3", "info", "third").has_value());

    std::vector<std::string> keys = {"b1", "b2", "b3", "b4"};
    auto result = storage->get("batch", keys);
    EXPECT_TRUE(result.has_value());

    auto& maps = result.value();
    ASSERT_TRUE(maps.contains("b1"));
    ASSERT_TRUE(maps.contains("b2"));
    ASSERT_TRUE(maps.contains("b3"));
    ASSERT_FALSE(maps.contains("b4"));

    EXPECT_EQ(maps["b1"]["info"], "first");
    EXPECT_EQ(maps["b2"]["info"], "second");
    EXPECT_EQ(maps["b3"]["info"], "third");
}
