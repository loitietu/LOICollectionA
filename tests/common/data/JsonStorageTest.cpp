#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <string_view>

#include "LOICollectionA/data/JsonStorage.h"

class JsonStorageTest : public testing::Test {
protected:
    std::filesystem::path tempDir;
    std::filesystem::path filePath;

    void SetUp() override {
        tempDir = std::filesystem::temp_directory_path() / "json_storage_test";

        std::filesystem::create_directories(tempDir);
        
        filePath = tempDir / "test.json";
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    void writeJsonFile(const std::string& content) {
        std::ofstream out(filePath);
        ASSERT_TRUE(out.is_open());
        out << content;
        out.close();
    }
};

TEST_F(JsonStorageTest, ConstructorCreatesFileWithEmptyJson) {
    ASSERT_FALSE(std::filesystem::exists(filePath));

    JsonStorage storage(filePath);

    EXPECT_TRUE(std::filesystem::exists(filePath));
    EXPECT_EQ(storage.get(), nlohmann::ordered_json::object());
}

TEST_F(JsonStorageTest, ConstructorLoadsExistingJson) {
    nlohmann::ordered_json expected = {{"name", "test"}, {"value", 42}};
    writeJsonFile(expected.dump());

    JsonStorage storage(filePath);

    EXPECT_EQ(storage.get(), expected);
}

TEST_F(JsonStorageTest, SetAndGetValue) {
    JsonStorage storage(filePath);

    storage.set("intKey", 100);
    EXPECT_EQ(storage.get<int>("intKey"), 100);
    EXPECT_TRUE(storage.has("intKey"));

    storage.set("strKey", std::string("hello"));
    EXPECT_EQ(storage.get<std::string>("strKey"), "hello");

    EXPECT_EQ(storage.get<int>("missing"), 0);
    EXPECT_FALSE(storage.has("missing"));
}

TEST_F(JsonStorageTest, SetAndGetWithPointer) {
    JsonStorage storage(filePath);

    storage.set_ptr("/user/name", std::string("Alice"));
    storage.set_ptr("/user/age", 30);

    EXPECT_EQ(storage.get_ptr<std::string>("/user/name"), "Alice");
    EXPECT_EQ(storage.get_ptr<int>("/user/age"), 30);
    EXPECT_TRUE(storage.has_ptr("/user/name"));

    EXPECT_EQ(storage.get_ptr<std::string>("/user/nick", "unknown"), "unknown");
}

TEST_F(JsonStorageTest, RemoveAndRemovePtr) {
    JsonStorage storage(filePath);
    storage.set("a", 1);
    storage.set_ptr("/x/y", 2);

    storage.remove("a");
    EXPECT_FALSE(storage.has("a"));

    storage.remove_ptr("/x/y");
    EXPECT_FALSE(storage.has_ptr("/x/y"));
}

TEST_F(JsonStorageTest, HasAndHasPtr) {
    JsonStorage storage(filePath);
    storage.set("visible", true);
    storage.set_ptr("/deep/hidden", 123);

    EXPECT_TRUE(storage.has("visible"));
    EXPECT_FALSE(storage.has("invisible"));
    EXPECT_TRUE(storage.has_ptr("/deep/hidden"));
    EXPECT_FALSE(storage.has_ptr("/deep/nowhere"));
}

TEST_F(JsonStorageTest, KeysReturnsTopLevelKeys) {
    JsonStorage storage(filePath);
    storage.set("first", 1);
    storage.set("second", 2);
    storage.set("third", 3);

    auto k = storage.keys();
    std::sort(k.begin(), k.end());

    std::vector<std::string> expected = {"first", "second", "third"};
    EXPECT_EQ(k, expected);
}

TEST_F(JsonStorageTest, WriteReplacesEntireJson) {
    JsonStorage storage(filePath);
    storage.set("old", "data");

    nlohmann::ordered_json newJson = {{"updated", true}, {"count", 10}};
    storage.write(newJson);

    EXPECT_EQ(storage.get(), newJson);
    EXPECT_FALSE(storage.has("old"));
}

TEST_F(JsonStorageTest, SavePersistsAndReloadsCorrectly) {
    nlohmann::ordered_json data = {{"persist", 123}, {"arr", {1, 2, 3}}};
    {
        JsonStorage storage(filePath);
        storage.write(data);
        storage.save(); 
    }

    JsonStorage reloaded(filePath);
    EXPECT_EQ(reloaded.get(), data);
}

TEST_F(JsonStorageTest, ThreadSafetySetAndGet) {
    JsonStorage storage(filePath);
    const int numThreads = 10;
    const int iterations = 100;
    std::vector<std::thread> threads;

    threads.reserve(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&storage, i]() -> void {
            for (int j = 0; j < iterations; ++j) {
                storage.set("shared_key", i * 1000 + j);
                
                volatile int val = storage.get<int>("shared_key");
                (void)val;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();
}
