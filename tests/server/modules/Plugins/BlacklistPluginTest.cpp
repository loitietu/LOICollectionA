#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/server/Plugins/BlacklistPlugin.h"

#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class BlacklistPluginTest : public testing::Test {
protected:
    std::string mBlacklistId{};

protected:
    void SetUp() override {
        if (!BlacklistPlugin::getInstance().isValid())
            GTEST_SKIP() << "BlacklistPlugin is not valid";
    }

    void TearDown() override {
        BlacklistPlugin::getInstance().getDatabase()->exec("DELETE FROM Blacklist;");
    }

    bool CreateBlacklistEntry() {
        TestSimulatedPlayer sp("test_player1");
        if (!sp.create())
            return false;

        BlacklistPlugin::getInstance().addBlacklist(*sp.getPlayer(), "Test cause", 3600);

        this->mBlacklistId = BlacklistPlugin::getInstance().getBlacklist(*sp.getPlayer());
        if (this->mBlacklistId.empty() || !BlacklistPlugin::getInstance().hasBlacklist(this->mBlacklistId))
            return false;
        
        return sp.destroy();
    }
};

TEST_F(BlacklistPluginTest, AddPlayerToBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());
}

TEST_F(BlacklistPluginTest, GetBlacklistData) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto data = BlacklistPlugin::getInstance().getBlacklistData(this->mBlacklistId);

    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data.size() == 7);
}

TEST_F(BlacklistPluginTest, GetBlacklists) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto blacklists = BlacklistPlugin::getInstance().getBlacklists();

    EXPECT_FALSE(blacklists.empty());
    EXPECT_TRUE(blacklists.size() >= 1);
}

TEST_F(BlacklistPluginTest, DeleteBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    BlacklistPlugin::getInstance().delBlacklist(this->mBlacklistId);

    EXPECT_FALSE(BlacklistPlugin::getInstance().hasBlacklist(this->mBlacklistId));
}
