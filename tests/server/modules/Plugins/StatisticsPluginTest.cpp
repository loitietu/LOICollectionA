#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/server/Plugins/StatisticsPlugin.h"

using namespace LOICollection::server::Plugins;

class StatisticsPluginTest : public testing::Test { 
protected:
    void SetUp() override {
        if (!StatisticsPlugin::getInstance().isValid())
            GTEST_SKIP() << "StatisticsPlugin is not valid";
    }

    void TearDown() override {
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM OnlineTime;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Kill;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Death;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Place;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Destroy;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Respawn;");
        StatisticsPlugin::getInstance().getDatabase()->exec("DELETE FROM Joins;");
    }
};

TEST_F(StatisticsPluginTest, AddAndGetStatistic) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    StatisticsPlugin::getInstance().addStatistic(*sp, StatisticType::onlinetime, 100);
    StatisticsPlugin::getInstance().addStatistic(*sp, StatisticType::deaths, 100);

    EXPECT_EQ(StatisticsPlugin::getInstance().getStatistic(*sp, StatisticType::onlinetime), 100);
    EXPECT_EQ(StatisticsPlugin::getInstance().getStatistic(*sp, StatisticType::deaths), 100);
}
