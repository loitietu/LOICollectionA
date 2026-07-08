#include <gtest/gtest.h>

#include <string>

#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/WalletPlugin.h"

#include "common/coro/MockExecutor.h"
#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class WalletPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!WalletPlugin::getInstance().isValid())
            GTEST_SKIP() << "WalletPlugin is not valid";
    }

    void TearDown() override {
        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Wallet;");

        WalletPlugin::getInstance().setExecutor(ll::thread::ServerThreadExecutor::getDefault());
    }
};

TEST_F(WalletPluginTest, GetPlayerInfo) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_EQ(WalletPlugin::getInstance().getPlayerInfo(sp->getUuid().asString()), "Unknown");
    
    auto data = WalletPlugin::getInstance().getPlayerInfo();
    EXPECT_TRUE(std::find(data.begin(), data.end(), std::make_pair(sp->getUuid().asString(), sp->getRealName())) == data.end());
}

TEST_F(WalletPluginTest, ForTransfer) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player4");
    EXPECT_TRUE(sp2.create());

    Config::C_Wallet config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    EXPECT_FALSE(WalletPlugin::getInstance().forTransfer(*sp, "nonexistent_target", "test_name", 100));
    
    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 100);

    EXPECT_TRUE(WalletPlugin::getInstance().forTransfer(*sp, sp2.getPlayer()->getUuid().asString(), sp2.getPlayer()->getRealName(), 100));
    
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard), 100 * (1 - config.ExchangeRate));

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(WalletPluginTest, Wealth) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    WalletPlugin::getInstance().wealth(*sp);
}

TEST_F(WalletPluginTest, RedenvelopeTimeout) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    Config::C_Wallet config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 500);

    MockExecutor executor;

    WalletPlugin::getInstance().setExecutor(executor);
    WalletPlugin::getInstance().redenvelope(*sp, "test_key", 100, 5);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);

    executor.advanceTime(std::chrono::seconds(config.RedEnvelopeTimeout + 1));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 500);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(WalletPluginTest, RedenvelopeReceive) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player4");
    EXPECT_TRUE(sp2.create());

    Config::C_Wallet config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 500);

    MockExecutor executor;

    WalletPlugin::getInstance().setExecutor(executor);
    WalletPlugin::getInstance().redenvelope(*sp, "test_key", 100, 5);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);

    WalletPlugin::getInstance().tryGrabRedEnvelope(*sp2.getPlayer(), "test_key");

    executor.advanceTime(std::chrono::seconds(config.RedEnvelopeTimeout + 1));

    EXPECT_TRUE(ScoreboardUtils::getScore(*sp, config.TargetScoreboard) < 500);
    EXPECT_TRUE(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard) > 0);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(WalletPluginTest, RedenvelopeReceiveOver) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player4");
    EXPECT_TRUE(sp2.create());

    TestSimulatedPlayer sp3("test_player5");
    EXPECT_TRUE(sp3.create());

    Config::C_Wallet config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 200);

    MockExecutor executor;

    WalletPlugin::getInstance().setExecutor(executor);
    WalletPlugin::getInstance().redenvelope(*sp, "test_key", 100, 2);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);

    WalletPlugin::getInstance().tryGrabRedEnvelope(*sp2.getPlayer(), "test_key");
    WalletPlugin::getInstance().tryGrabRedEnvelope(*sp3.getPlayer(), "test_key");

    executor.advanceTime(std::chrono::seconds(config.RedEnvelopeTimeout + 1));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);
    EXPECT_TRUE(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard) > 0);
    EXPECT_TRUE(ScoreboardUtils::getScore(*sp3.getPlayer(), config.TargetScoreboard) > 0);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}
