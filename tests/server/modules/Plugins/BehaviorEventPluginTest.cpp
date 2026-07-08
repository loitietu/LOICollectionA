#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ThreadPoolExecutor.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/server/Plugins/BehaviorEventPlugin.h"

using namespace LOICollection::server::Plugins;

class BehaviorEventPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!BehaviorEventPlugin::getInstance().isValid())
            GTEST_SKIP() << "BehaviorEventPlugin is not valid";
    }

    void TearDown() override {
        BehaviorEventPlugin::getInstance().getDatabase()->exec("DELETE FROM Events;");
    }

    bool CreateDatabaseEntry(const std::string& id = "test") {
        BehaviorEventPlugin::Event mEvent = BehaviorEventPlugin::getInstance().getBasicEvent("test", "test", Vec3(-114514, -114514, -114514), 0);
        BehaviorEventPlugin::getInstance().write(id, mEvent);

        return BehaviorEventPlugin::getInstance().getDatabase()->has("Events", id);
    }
};

TEST_F(BehaviorEventPluginTest, WriteDatabase) {
    EXPECT_TRUE(CreateDatabaseEntry());
}

TEST_F(BehaviorEventPluginTest, GetEvents) {
    EXPECT_TRUE(CreateDatabaseEntry());

    auto data = BehaviorEventPlugin::getInstance().getEvents();
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], "test");
}

TEST_F(BehaviorEventPluginTest, GetEventByConditions) {
    EXPECT_TRUE(CreateDatabaseEntry());

    auto data = BehaviorEventPlugin::getInstance().getEvents({
        { "event_name", "test" },
        { "event_type", "test" }
    });

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], "test");
}

TEST_F(BehaviorEventPluginTest, GetEventByFilter) {
    EXPECT_TRUE(CreateDatabaseEntry());

    auto data = BehaviorEventPlugin::getInstance().getEvents({
        { "event_type", "" }
    }, [](std::string value) -> bool {
        return value == "test";
    });

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], "test");
}

TEST_F(BehaviorEventPluginTest, GetEventByPosition) {
    EXPECT_TRUE(CreateDatabaseEntry());

    auto data = BehaviorEventPlugin::getInstance().getEventsByPosition(0, [](int x, int y, int z) -> bool {
        return x == -114514 && y == -114514 && z == -114514;
    });

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], "test");
}

TEST_F(BehaviorEventPluginTest, Filter) {
    EXPECT_TRUE(CreateDatabaseEntry());
    EXPECT_TRUE(CreateDatabaseEntry("test2"));

    auto data = BehaviorEventPlugin::getInstance().filter({ "test", "test2" });

    EXPECT_FALSE(data.empty());
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], "test2");
}
