#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/include/server/Plugins/MutePlugin.h"

using namespace LOICollection::server::Plugins;

class MutePluginTest : public testing::Test {
protected:
    std::string mMuteId{};

protected:
    void SetUp() override {
        if (!MutePlugin::getInstance().isValid())
            GTEST_SKIP() << "MutePlugin is not valid";
    }

    void TearDown() override {
        MutePlugin::getInstance().getDatabase()->exec("DELETE FROM Mute;");
    }

    bool CreateMuteEntry() {
        auto sp = ll::service::getLevel()->getPlayer("test_player");
        if (!sp)
            return false;

        MutePlugin::getInstance().addMute(*sp, "Test Cause", 3600);

        this->mMuteId = MutePlugin::getInstance().getMute(*sp);
        if (this->mMuteId.empty() || !MutePlugin::getInstance().hasMute(this->mMuteId))
            return false;

        return true;
    }
};

TEST_F(MutePluginTest, AddPlayerToMute) {
    EXPECT_TRUE(CreateMuteEntry());
}

TEST_F(MutePluginTest, GetMuteData) {
    EXPECT_TRUE(CreateMuteEntry());

    auto data = MutePlugin::getInstance().getMuteData(this->mMuteId);

    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data.size() == 5);
    EXPECT_TRUE(data["name"] == "test_player");
}

TEST_F(MutePluginTest, GetMutes) {
    EXPECT_TRUE(CreateMuteEntry());

    auto mutes = MutePlugin::getInstance().getMutes();

    EXPECT_FALSE(mutes.empty());
    EXPECT_TRUE(mutes.size() >= 1);
}

TEST_F(MutePluginTest, DeleteMute) {
    EXPECT_TRUE(CreateMuteEntry());

    MutePlugin::getInstance().delMute(this->mMuteId);

    EXPECT_FALSE(MutePlugin::getInstance().hasMute(this->mMuteId));
}

TEST_F(MutePluginTest, InterceptPlayerMessage) {
    EXPECT_TRUE(CreateMuteEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    static_cast<SimulatedPlayer*>(sp)->simulateChat("Test Chat");
}
