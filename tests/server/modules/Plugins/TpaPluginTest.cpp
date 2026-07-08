#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/TpaPlugin.h"

#include "common/coro/MockExecutor.h"
#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class TpaPluginTest : public testing::Test {
protected:
    std::string mBlacklistId{};

protected:
    void SetUp() override {
        if (!TpaPlugin::getInstance().isValid())
            GTEST_SKIP() << "TpaPlugin is not valid";
    }

    void TearDown() override {
        TpaPlugin::getInstance().getDatabase()->exec("DELETE FROM Blacklist;");

        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Tpa;");

        TpaPlugin::getInstance().setExecutor(ll::thread::ServerThreadExecutor::getDefault());
    }

    bool CreateBlacklistEntry() {
        auto sp2 = ll::service::getLevel()->getPlayer("test_player");

        TestSimulatedPlayer sp("test_player2");
        if (!sp.create() || !sp2)
            return false;

        TpaPlugin::getInstance().addBlacklist(*sp2, *sp.getPlayer());

        this->mBlacklistId = TpaPlugin::getInstance().getBlacklist(*sp2, *sp.getPlayer());
        if (this->mBlacklistId.empty() || !TpaPlugin::getInstance().hasBlacklist(*sp2, this->mBlacklistId))
            return false;

        return sp.destroy();
    }
};

TEST_F(TpaPluginTest, InviteSetting) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_FALSE(TpaPlugin::getInstance().isInvite(*sp));

    TpaPlugin::getInstance().setInvite(*sp, true);

    EXPECT_TRUE(TpaPlugin::getInstance().isInvite(*sp));
}

TEST_F(TpaPluginTest, AddPlayerToBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());
}

TEST_F(TpaPluginTest, DeletePlayerFromBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TpaPlugin::getInstance().delBlacklist(*sp, this->mBlacklistId);

    EXPECT_FALSE(TpaPlugin::getInstance().hasBlacklist(*sp, this->mBlacklistId));
}

TEST_F(TpaPluginTest, GetBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_TRUE(TpaPlugin::getInstance().getBlacklist(*sp).size() > 0);
}

TEST_F(TpaPluginTest, GetBlacklistFromTarget) {
    EXPECT_TRUE(CreateBlacklistEntry());

    EXPECT_TRUE(TpaPlugin::getInstance().getBlacklistFromTarget({ this->mBlacklistId }).size() > 0);
}

TEST_F(TpaPluginTest, GetBlacklistData) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto data = TpaPlugin::getInstance().getBlacklistData(this->mBlacklistId);
    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data["author"] == sp->getUuid().asString());
}

TEST_F(TpaPluginTest, ForTpaContent) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    Config::C_Tpa config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Tpa;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, config.RequestRequired);

    EXPECT_TRUE(TpaPlugin::getInstance().forTpaContent(*sp));
    EXPECT_FALSE(TpaPlugin::getInstance().forTpaContent(*sp));
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(TpaPluginTest, SendRequest) {
    MockExecutor executor;
    TpaPlugin::getInstance().setExecutor(executor);

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player2");
    EXPECT_TRUE(sp2.create());

    TpaPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), "test_request", TpaType::tpa);
    EXPECT_TRUE(TpaPlugin::getInstance().hasRequest(sp->getUuid().asString(), sp2.getPlayer()->getUuid().asString()));

    Config::C_Tpa config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Tpa;

    executor.advanceTime(std::chrono::seconds(config.RequestTimeout + 1));
    EXPECT_FALSE(TpaPlugin::getInstance().hasRequest(sp->getUuid().asString(), sp2.getPlayer()->getUuid().asString()));
}

TEST_F(TpaPluginTest, AcceptRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player2");
    EXPECT_TRUE(sp2.create());

    Config::C_Tpa config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Tpa;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, config.RequestRequired);

    TpaPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), "test_request", TpaType::tpa);

    sp2.getPlayer()->teleport(sp2.getPlayer()->getPosition() + Vec3(1, 1, 1), sp2.getPlayer()->getDimensionId());

    EXPECT_TRUE(TpaPlugin::getInstance().acceptRequest(*sp2.getPlayer(), "test_request"));
    EXPECT_FALSE(TpaPlugin::getInstance().hasRequest(sp->getUuid().asString(), sp2.getPlayer()->getUuid().asString()));

    Vec3 spPos = sp->getPosition();
    Vec3 sp2Pos = sp2.getPlayer()->getPosition();

    EXPECT_EQ(static_cast<int>(spPos.x), static_cast<int>(sp2Pos.x));
    EXPECT_EQ(static_cast<int>(spPos.y), static_cast<int>(sp2Pos.y) + 1);
    EXPECT_EQ(static_cast<int>(spPos.z), static_cast<int>(sp2Pos.z));

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(TpaPluginTest, RejectRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player2");
    EXPECT_TRUE(sp2.create());

    TpaPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), "test_request", TpaType::tpa);

    sp2.getPlayer()->teleport(sp2.getPlayer()->getPosition() + Vec3(1, 1, 1), sp2.getPlayer()->getDimensionId());

    EXPECT_TRUE(TpaPlugin::getInstance().rejectRequest(*sp2.getPlayer(), "test_request"));
    EXPECT_FALSE(TpaPlugin::getInstance().hasRequest(sp->getUuid().asString(), sp2.getPlayer()->getUuid().asString()));
    EXPECT_NE(sp->getPosition(), sp2.getPlayer()->getPosition());
}

TEST_F(TpaPluginTest, CancelRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player2");
    EXPECT_TRUE(sp2.create());

    TpaPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), "test_request", TpaType::tpa);

    sp2.getPlayer()->teleport(sp2.getPlayer()->getPosition() + Vec3(1, 1, 1), sp2.getPlayer()->getDimensionId());

    EXPECT_TRUE(TpaPlugin::getInstance().cancelRequest("test_request"));
    EXPECT_FALSE(TpaPlugin::getInstance().hasRequest(sp->getUuid().asString(), sp2.getPlayer()->getUuid().asString()));
    EXPECT_NE(sp->getPosition(), sp2.getPlayer()->getPosition());
}