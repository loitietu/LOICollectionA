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
        if (!MenuPlugin::getInstance().isValid())
            GTEST_SKIP() << "MenuPlugin is not valid";
    }

    void TearDown() override {
        MenuPlugin::getInstance().getDatabase()->write({});
        MenuPlugin::getInstance().getDatabase()->save();
    }

    void CreateMenuEntry() {
        nlohmann::json menuSimpleData = {
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

        MenuPlugin::getInstance().create("test_menu_simple", menuSimpleData);
        MenuPlugin::getInstance().create("test_menu_modal", menuModalData);
    }
};

TEST_F(MenuPluginTest, MenuCreate) {
    CreateMenuEntry();

    EXPECT_TRUE(MenuPlugin::getInstance().has("test_menu_simple"));
    EXPECT_TRUE(MenuPlugin::getInstance().has("test_menu_modal"));
}

TEST_F(MenuPluginTest, MenuRemove) {
    CreateMenuEntry();

    MenuPlugin::getInstance().remove("test_menu_simple");

    EXPECT_FALSE(MenuPlugin::getInstance().has("test_menu_simple"));
    EXPECT_TRUE(MenuPlugin::getInstance().has("test_menu_modal"));
}

TEST_F(MenuPluginTest, MenuHandleActionSimple) {
    CreateMenuEntry();

    ScoreboardUtils::create("test_tietu1");
    ScoreboardUtils::create("test_tietu2");
    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto data = MenuPlugin::getInstance().getDatabase()->get("test_menu_simple", nlohmann::ordered_json{});
    MenuPlugin::getInstance().handleAction(*sp, data["customize"].at(0), data);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);

    ScoreboardUtils::reduceScore(*sp, "test_tietu2", 4);

    MenuPlugin::getInstance().handleAction(*sp, data["customize"].at(0), data);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 2);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 0);

    sp->getAbilities().setPlayerPermissions(PlayerPermissionLevel::Member);

    MenuPlugin::getInstance().handleAction(*sp, data["customize"].at(1), data);

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

    auto data = MenuPlugin::getInstance().getDatabase()->get("test_menu_modal", nlohmann::ordered_json{});
    MenuPlugin::getInstance().handleAction(*sp, data["cancelButton"], data);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 7);

    ScoreboardUtils::remove("test_tietu1");
}
