#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/server/APIUtils.h"

using namespace LOICollection::frontend;
using namespace LOICollection::server::LOICollectionAPI;

TEST(APIUtilsTest, RegisterAngGetVariable) {
    APIUtils::getInstance().registerVariable("test_var_one", []() -> std::string {
        return "test_value_one";
    });

    APIUtils::getInstance().registerVariable("test_var_two", [](Player& player) -> std::string {
        return "test_value_two_" + player.getRealName();
    });

    APIUtils::getInstance().registerVariable("test_var_three", [](const CallbackTypeValues& args) -> std::string {
        return "test_value_three_" + std::to_string(std::get<int>(args[0]));
    }, { ParamType::INT });

    APIUtils::getInstance().registerVariable("test_var_four", [](Player& player, const CallbackTypeValues& args) -> std::string {
        return "test_value_four_" + player.getRealName() + "_" + std::to_string(std::get<int>(args[0]));
    }, { ParamType::INT });

    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    EXPECT_EQ(APIUtils::getInstance().getValueForVariable("test_var_one"), "test_value_one");
    EXPECT_EQ(APIUtils::getInstance().getValueForVariable("test_var_two", *player), "test_value_two_test_player");
    EXPECT_EQ(APIUtils::getInstance().getValueForVariable("test_var_three", { 10 }), "test_value_three_10");
    EXPECT_EQ(APIUtils::getInstance().getValueForVariable("test_var_four", *player, { 20 }), "test_value_four_test_player_20");
}

TEST(APIUtilsTest, TranslateString) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    EXPECT_EQ(APIUtils::getInstance().translate("'Test '{test_var_one}"), "Test test_value_one");
    EXPECT_EQ(APIUtils::getInstance().translate("'Test '{test_var_two}", *player), "Test test_value_two_test_player");
    EXPECT_EQ(APIUtils::getInstance().translate("'Test '{test_var_one}' '{test_var_two}", *player), "Test test_value_one test_value_two_test_player");

    EXPECT_EQ(APIUtils::getInstance().translate("'Hello '{player_realname}", *player), "Hello test_player");
}
