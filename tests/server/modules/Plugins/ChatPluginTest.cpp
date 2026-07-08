#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class ChatPluginTest : public testing::Test {
protected:
    std::string mBlacklistId{};

protected:
    void SetUp() override {
        if (!ChatPlugin::getInstance().isValid())
            GTEST_SKIP() << "ChatPlugin is not valid";
    }

    void TearDown() override {
        ChatPlugin::getInstance().getDatabase()->exec("DELETE FROM Blacklist;");
        ChatPlugin::getInstance().getDatabase()->exec("DELETE FROM Titles;");

        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Chat;");
    }

    bool CreateBlacklistEntry() {
        auto sp2 = ll::service::getLevel()->getPlayer("test_player");

        TestSimulatedPlayer sp("test_player2");
        if (!sp.create() || !sp2)
            return false;

        ChatPlugin::getInstance().addBlacklist(*sp2, *sp.getPlayer());

        this->mBlacklistId = ChatPlugin::getInstance().getBlacklist(*sp2, *sp.getPlayer());
        if (this->mBlacklistId.empty() || !ChatPlugin::getInstance().hasBlacklist(*sp2, this->mBlacklistId))
            return false;

        return sp.destroy();
    }

    bool CreateTitleEntry() {
        auto sp = ll::service::getLevel()->getPlayer("test_player");
        if (!sp)
            return false;

        ChatPlugin::getInstance().addTitle(*sp, "Test Title", 24);
        if (!ChatPlugin::getInstance().hasTitle(*sp, "Test Title"))
            return false;

        return true;
    }
};

TEST_F(ChatPluginTest, AddPlayerToBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());
}

TEST_F(ChatPluginTest, DeletePlayerFromBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ChatPlugin::getInstance().delBlacklist(*sp, this->mBlacklistId);

    EXPECT_FALSE(ChatPlugin::getInstance().hasBlacklist(*sp, this->mBlacklistId));
}

TEST_F(ChatPluginTest, GetBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_TRUE(ChatPlugin::getInstance().getBlacklist(*sp).size() > 0);
}

TEST_F(ChatPluginTest, GetBlacklistData) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto data = ChatPlugin::getInstance().getBlacklistData(this->mBlacklistId);
    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data["author"] == sp->getUuid().asString());
}

TEST_F(ChatPluginTest, AddTitleToPlayer) {
    EXPECT_TRUE(CreateTitleEntry());
}

TEST_F(ChatPluginTest, DeleteTitleFromPlayer) {
    EXPECT_TRUE(CreateTitleEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ChatPlugin::getInstance().delTitle(*sp, "Test Title");

    EXPECT_FALSE(ChatPlugin::getInstance().hasTitle(*sp, "Test Title"));
}

TEST_F(ChatPluginTest, GetTitleFromPlayer) {
    EXPECT_TRUE(CreateTitleEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ChatPlugin::getInstance().setTitle(*sp, "Test Title");

    std::string title = ChatPlugin::getInstance().getTitle(*sp);
    EXPECT_FALSE(title.empty());
    EXPECT_TRUE(title == "Test Title");
}

TEST_F(ChatPluginTest, GetTitleTimeFromPlayer) {
    EXPECT_TRUE(CreateTitleEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    std::string time = ChatPlugin::getInstance().getTitleTime(*sp, "Test Title");
    EXPECT_FALSE(time.empty());
}

TEST_F(ChatPluginTest, GetTitlesFromPlayer) {
    EXPECT_TRUE(CreateTitleEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    std::vector<std::string> titles = ChatPlugin::getInstance().getTitles(*sp);
    EXPECT_FALSE(titles.empty());
    EXPECT_TRUE(std::find(titles.begin(), titles.end(), "Test Title") != titles.end());
}

TEST_F(ChatPluginTest, ChatFormatting) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    static_cast<SimulatedPlayer*>(sp)->simulateChat("Test chat");
}
