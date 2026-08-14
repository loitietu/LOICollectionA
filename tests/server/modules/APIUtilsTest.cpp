#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/CallbackUtils.h"

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

    auto r1 = APIUtils::getInstance().getValueForVariable("test_var_one");
    EXPECT_TRUE(r1.has_value());
    EXPECT_EQ(std::get<std::string>(r1.value()), "test_value_one");

    auto r2 = APIUtils::getInstance().getValueForVariable("test_var_two", *player);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(std::get<std::string>(r2.value()), "test_value_two_test_player");

    auto r3 = APIUtils::getInstance().getValueForVariable("test_var_three", { 10 });
    EXPECT_TRUE(r3.has_value());
    EXPECT_EQ(std::get<std::string>(r3.value()), "test_value_three_10");

    auto r4 = APIUtils::getInstance().getValueForVariable("test_var_four", *player, { 20 });
    EXPECT_TRUE(r4.has_value());
    EXPECT_EQ(std::get<std::string>(r4.value()), "test_value_four_test_player_20");
}

TEST(APIUtilsTest, TranslateString) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    EXPECT_EQ(APIUtils::getInstance().translate("'Test ' + {test_var_one}"), "Test test_value_one");
    EXPECT_EQ(APIUtils::getInstance().translate("'Test ' + {test_var_two}", *player), "Test test_value_two_test_player");
    EXPECT_EQ(APIUtils::getInstance().translate("'Test ' + {test_var_one} + ' ' + {test_var_two}", *player), "Test test_value_one test_value_two_test_player");

    EXPECT_EQ(APIUtils::getInstance().translate("'Hello ' + {player_realname}", *player), "Hello test_player");
}
