#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/LayeredAbilities.h>

#include <mc/server/SimulatedPlayer.h>
#include <mc/server/commands/PlayerPermissionLevel.h>

#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/include/server/Plugins/MenuPlugin.h"
#include "LOICollectionA/include/server/Plugins/form/MenuData.h"

using namespace LOICollection::server::Plugins;

class MenuPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!MenuPlugin::getShared()->isValid())
            GTEST_SKIP() << "MenuPlugin is not valid";
    }

    static MenuData MakeSimpleMenu() {
        MenuData simple;
        simple.id = "test_menu_simple";
        simple.type = "Simple";
        simple.title = "Menu Test";
        simple.content = "This is a menu test";

        MenuItemData button1;
        button1.type = "button";
        button1.title = "Button 1";
        button1.id = "Button1";
        button1.scores = { ScoreRequirement{ "test_tietu_money", 100 } };
        button1.run = {
            "execute as ${player} run scoreboard players set @s test_tietu1 4",
            "execute as ${player} run scoreboard players set @s test_tietu2 4"
        };

        MenuItemData button2;
        button2.type = "button";
        button2.title = "Button 2";
        button2.id = "Button2";
        button2.run = {
            "execute as ${player} run scoreboard players set @s test_tietu1 5",
            "execute as ${player} run scoreboard players set @s test_tietu2 5"
        };
        button2.permission = 1;

        simple.items = { button1, button2 };
        return simple;
    }

    static MenuData MakeModalMenu() {
        MenuData modal;
        modal.id = "test_menu_modal";
        modal.type = "Modal";
        modal.title = "Menu Test Modal";
        modal.content = "This is a menu test modal";

        MenuItemData confirm;
        confirm.type = "from";
        confirm.title = "Confirm";
        confirm.run = { "test_menu_simple" };

        MenuItemData cancel;
        cancel.type = "button";
        cancel.title = "Cancel";
        cancel.run = { "execute as ${player} run scoreboard players set @s test_tietu1 7" };

        modal.confirm = confirm;
        modal.cancel = cancel;
        return modal;
    }
};

TEST_F(MenuPluginTest, MenuHandleActionSimple) {
    auto menu = MakeSimpleMenu();

    ScoreboardUtils::create("test_tietu1");
    ScoreboardUtils::create("test_tietu2");
    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto action = MenuPlugin::getShared()->handleAction(*sp, menu.items.at(0), menu);
    EXPECT_TRUE(action.has_value());
    EXPECT_EQ(action.value(), MenuActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);

    ScoreboardUtils::reduceScore(*sp, "test_tietu2", 4);

    auto noScore = MenuPlugin::getShared()->handleAction(*sp, menu.items.at(0), menu);
    EXPECT_TRUE(noScore.has_value());
    EXPECT_EQ(noScore.value(), MenuActionResult::InsufficientScore);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 0);

    sp->getAbilities().setPlayerPermissions(PlayerPermissionLevel::Member);

    auto noPermission = MenuPlugin::getShared()->handleAction(*sp, menu.items.at(1), menu);
    EXPECT_TRUE(noPermission.has_value());
    EXPECT_EQ(noPermission.value(), MenuActionResult::PermissionDenied);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 4);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 0);

    ScoreboardUtils::remove("test_tietu1");
    ScoreboardUtils::remove("test_tietu2");
    ScoreboardUtils::remove("test_tietu_money");
}

TEST_F(MenuPluginTest, MenuHandleActionModal) {
    auto menu = MakeModalMenu();

    ScoreboardUtils::create("test_tietu1");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto action = MenuPlugin::getShared()->handleAction(*sp, menu.cancel, menu);
    EXPECT_TRUE(action.has_value());
    EXPECT_EQ(action.value(), MenuActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 7);

    ScoreboardUtils::remove("test_tietu1");
}
