#include <gtest/gtest.h>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

TEST(ScoreboardUtilsTest, CreateScoreboard) {
    ScoreboardUtils::create("test_plugin_1");
    ScoreboardUtils::create("test_plugin_2");
    ScoreboardUtils::create("test_plugin_3");

    EXPECT_TRUE(ScoreboardUtils::hasScoreboard("test_plugin_1"));
    EXPECT_TRUE(ScoreboardUtils::hasScoreboard("test_plugin_2"));
    EXPECT_TRUE(ScoreboardUtils::hasScoreboard("test_plugin_3"));
}

TEST(ScoreboardUtilsTest, ModifyAndGetScore) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    ScoreboardUtils::addScore(*player, "test_plugin_1", 10);
    ScoreboardUtils::addScore(*player, "test_plugin_2", 20);
    ScoreboardUtils::addScore(*player, "test_plugin_3", 30);

    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_1"), 10);
    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_2"), 20);
    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_3"), 30);

    ScoreboardUtils::reduceScore(*player, "test_plugin_1", 5);
    ScoreboardUtils::reduceScore(*player, "test_plugin_2", 10);
    ScoreboardUtils::reduceScore(*player, "test_plugin_3", 15);

    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_1"), 5);
    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_2"), 10);
    EXPECT_EQ(ScoreboardUtils::getScore(*player, "test_plugin_3"), 15);
}