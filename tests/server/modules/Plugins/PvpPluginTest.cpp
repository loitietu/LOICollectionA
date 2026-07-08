#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/include/server/Plugins/PvpPlugin.h"

#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class PvpPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!PvpPlugin::getInstance().isValid())
            GTEST_SKIP() << "PvpPlugin is not valid";
    }

    void TearDown() override {
        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Pvp;");
    }
};

TEST_F(PvpPluginTest, EnableAndIsPvp) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    PvpPlugin::getInstance().enable(*sp, true);

    EXPECT_TRUE(PvpPlugin::getInstance().isEnable(*sp));

    PvpPlugin::getInstance().enable(*sp, false);

    EXPECT_FALSE(PvpPlugin::getInstance().isEnable(*sp));
}

TEST_F(PvpPluginTest, CheckPvp) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player3");
    EXPECT_TRUE(sp2.create());

    PvpPlugin::getInstance().enable(*sp, true);
    PvpPlugin::getInstance().enable(*sp2.getPlayer(), true);

    sp2.getPlayer()->teleport(sp->getPosition(), sp->getDimensionId());

    static_cast<SimulatedPlayer*>(sp)->simulateAttack(sp2.getPlayer());
    EXPECT_TRUE(sp2.getPlayer()->getHealth() < 20);

    sp2.getPlayer()->heal(20);

    PvpPlugin::getInstance().enable(*sp2.getPlayer(), false);

    static_cast<SimulatedPlayer*>(sp)->simulateAttack(sp2.getPlayer());
    EXPECT_TRUE(sp2.getPlayer()->getHealth() == 20);
}
