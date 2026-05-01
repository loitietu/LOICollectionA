#include <gtest/gtest.h>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/utils/mc-server/CommandUtils.h"

TEST(CommandUtilsTest, ExecuteCommand) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    CommandUtils::executeCommand(*player, "say Hello ${player}!");
}
