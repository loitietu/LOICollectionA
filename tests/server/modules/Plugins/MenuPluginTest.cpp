#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/LayeredAbilities.h>

#include <mc/server/SimulatedPlayer.h>
#include <mc/server/commands/PlayerPermissionLevel.h>

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"

using namespace LOICollection::server::Plugins;

class MenuPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!MenuPlugin::getShared()->isValid())
            GTEST_SKIP() << "MenuPlugin is not valid";
    }

    void TearDown() override {
        MenuPlugin::getShared()->getDatabase()->write({});

        auto saveResult = MenuPlugin::getShared()->getDatabase()->save();
        if (!saveResult.has_value())
            GTEST_FAIL() << "Unable to save data";
    }

    void CreateMenuEntry() {
        nlohmann::ordered_json menuSimpleData = {
            { "title", "Menu Test" },
            { "content", "This is a menu test" },
            { "info", {
                { "exit", "execute as ${player} run scoreboard players set @s test_tietu1 0" },
                { "permission", "execute as ${player} run scoreboard players set @s test_tietu1 1" },
                { "score", "execute as ${player} run scoreboard players set @s test_tietu1 2" }
            } },
            { "type", "Simple" },
            { "customize", {
                {
                    { "title", "Button 1" },
                    { "image", "" },
                    { "id", "Button1" },
                    { "scores", {
                        { "test_tietu_money", 100 }
                    } },
                    { "run", {
                        "execute as ${player} run scoreboard players set @s test_tietu1 4",
                        "execute as ${player} run scoreboard players set @s test_tietu2 4"
                    } },
                    { "type", "button" },
                    { "permission", 0 }
                },
                {
                    { "title", "Button 2" },
                    { "image", "" },
                    { "id", "Button2" },
                    { "scores", {} },
                    { "run", {
                        "execute as ${player} run scoreboard players set @s test_tietu1 5",
                        "execute as ${player} run scoreboard players set @s test_tietu2 5"
                    } },
                    { "type", "button" },
                    { "permission", 1 }
                }
            } },
            { "permission", 0 }
        };

        nlohmann::ordered_json menuModalData = {
            { "title", "Menu Test Modal" },
            { "content", "This is a menu test modal" },
            { "info", {
                { "permission", "execute as ${player} run scoreboard players set @s test_tietu1 1" },
                { "score", "execute as ${player} run scoreboard players set @s test_tietu1 2" }
            } },
            { "type", "Modal" },
            { "confirmButton", {
                { "title", "Confirm" },
                { "scores", {} },
                { "run", "test_menu_simple" },
                { "type", "from" },
                { "permission", 0 }
            } },
            { "cancelButton", {
                { "title", "Cancel" },
                { "scores", {} },
                { "run", {
                    "execute as ${player} run scoreboard players set @s test_tietu1 7"
                } },
                { "type", "button" },
                { "permission", 0 }
            } },
            { "permission", 0 }
        };

        ASSERT_TRUE(MenuPlugin::getShared()->create("test_menu_simple", menuSimpleData).has_value());
        ASSERT_TRUE(MenuPlugin::getShared()->create("test_menu_modal", menuModalData).has_value());
    }
};

TEST_F(MenuPluginTest, MenuCreate) {
    CreateMenuEntry();

    auto has1 = MenuPlugin::getShared()->has("test_menu_simple");
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    auto has2 = MenuPlugin::getShared()->has("test_menu_modal");
    EXPECT_TRUE(has2.has_value());
    EXPECT_TRUE(has2.value());
}

TEST_F(MenuPluginTest, MenuRemove) {
    CreateMenuEntry();

    EXPECT_TRUE(MenuPlugin::getShared()->remove("test_menu_simple").has_value());

    auto has1 = MenuPlugin::getShared()->has("test_menu_simple");
    EXPECT_TRUE(has1.has_value());
    EXPECT_FALSE(has1.value());

    auto has2 = MenuPlugin::getShared()->has("test_menu_modal");
    EXPECT_TRUE(has2.has_value());
    EXPECT_TRUE(has2.value());
}

TEST_F(MenuPluginTest, MenuHandleActionSimple) {
    CreateMenuEntry();

    ScoreboardUtils::create("test_tietu1");
    ScoreboardUtils::create("test_tietu2");
    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto dataResult = MenuPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("test_menu_simple");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    EXPECT_TRUE(MenuPlugin::getShared()->handleAction(*sp, data["customize"].at(0), data).has_value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);

    ScoreboardUtils::reduceScore(*sp, "test_tietu2", 4);

    EXPECT_FALSE(MenuPlugin::getShared()->handleAction(*sp, data["customize"].at(0), data).has_value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 2);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 0);

    sp->getAbilities().setPlayerPermissions(PlayerPermissionLevel::Member);

    EXPECT_FALSE(MenuPlugin::getShared()->handleAction(*sp, data["customize"].at(1), data).has_value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 1);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 0);

    ScoreboardUtils::remove("test_tietu1");
    ScoreboardUtils::remove("test_tietu2");
    ScoreboardUtils::remove("test_tietu_money");
}

TEST_F(MenuPluginTest, MenuHandleActionModal) {
    CreateMenuEntry();

    ScoreboardUtils::create("test_tietu1");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto dataResult = MenuPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("test_menu_modal");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    EXPECT_TRUE(MenuPlugin::getShared()->handleAction(*sp, data["cancelButton"], data).has_value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 7);

    ScoreboardUtils::remove("test_tietu1");
}
