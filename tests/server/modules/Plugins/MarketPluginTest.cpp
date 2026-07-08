#include <gtest/gtest.h>

#include <string>
#include <unordered_map>

#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <mc/world/level/Level.h>
#include <mc/world/item/ItemStack.h>

#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/MarketPlugin.h"

#include "common/coro/MockExecutor.h"
#include "server/TestSimulatedPlayer.h"

using namespace LOICollection::server::Plugins;

class MarketPluginTest : public testing::Test {
protected:
    std::string mBlacklistId{};
    std::string mItemId{};

protected:
    void SetUp() override {
        if (!MarketPlugin::getInstance().isValid())
            GTEST_SKIP() << "MarketPlugin is not valid";
    }

    void TearDown() override {
        MarketPlugin::getInstance().getDatabase()->exec("DELETE FROM Blacklist;");
        MarketPlugin::getInstance().getDatabase()->exec("DELETE FROM Item;");

        ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Market;");

        MarketPlugin::getInstance().setExecutor(ll::thread::ServerThreadExecutor::getDefault());
    }

    bool CreateBlacklistEntry() {
        auto sp2 = ll::service::getLevel()->getPlayer("test_player");

        TestSimulatedPlayer sp("test_player6");
        if (!sp.create() || !sp2)
            return false;

        MarketPlugin::getInstance().addBlacklist(*sp2, *sp.getPlayer());

        this->mBlacklistId = MarketPlugin::getInstance().getBlacklist(*sp2, *sp.getPlayer());
        if (this->mBlacklistId.empty() || !MarketPlugin::getInstance().hasBlacklist(*sp2, this->mBlacklistId))
            return false;
        
        return sp.destroy();
    }

    bool CreateItemEntry() {
        auto sp = ll::service::getLevel()->getPlayer("test_player");
        if (!sp)
            return false;

        auto itemStack = std::make_unique<ItemStack>();
        itemStack->reinit("minecraft:grass_block", 1, 0);

        MarketPlugin::getInstance().addItem(*sp, *itemStack, "grass_block", "", "A block of grass.", 100);

        this->mItemId = MarketPlugin::getInstance().getItems(*sp).front();
        if (this->mItemId.empty() || !MarketPlugin::getInstance().hasItem(this->mItemId))
            return false;
        
        return true;
    }
};

TEST_F(MarketPluginTest, CreateBlacklistEntry) {
    EXPECT_TRUE(CreateBlacklistEntry());
}

TEST_F(MarketPluginTest, DeletePlayerFromBlacklist) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    MarketPlugin::getInstance().delBlacklist(*sp, this->mBlacklistId);

    EXPECT_FALSE(MarketPlugin::getInstance().hasBlacklist(*sp, this->mBlacklistId));
}

TEST_F(MarketPluginTest, GetBlacklist) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    MarketPlugin::getInstance().addBlacklist(*sp, *sp2.getPlayer());

    EXPECT_TRUE(MarketPlugin::getInstance().getBlacklist(*sp2.getPlayer()).size() > 0);
}

TEST_F(MarketPluginTest, GetBlacklistData) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto data = MarketPlugin::getInstance().getBlacklistData(this->mBlacklistId);
    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data["author"] == sp->getUuid().asString());
}

TEST_F(MarketPluginTest, CreateItemEntry) {
    EXPECT_TRUE(CreateItemEntry());
}

TEST_F(MarketPluginTest, DeleteItemEntry) {
    EXPECT_TRUE(CreateItemEntry());

    MarketPlugin::getInstance().delItem(this->mItemId);

    EXPECT_FALSE(MarketPlugin::getInstance().hasItem(this->mItemId));
}

TEST_F(MarketPluginTest, GetItemData) {
    EXPECT_TRUE(CreateItemEntry());

    auto data = MarketPlugin::getInstance().getItemData(this->mItemId);
    EXPECT_FALSE(data.empty());
    EXPECT_TRUE(data["name"] == "grass_block");

    EXPECT_TRUE(MarketPlugin::getInstance().getItemsData({ this->mItemId }).contains(this->mItemId));
}

TEST_F(MarketPluginTest, GetItems) {
    EXPECT_TRUE(CreateItemEntry());

    EXPECT_TRUE(MarketPlugin::getInstance().getItems().size() > 0);

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_TRUE(MarketPlugin::getInstance().getItems(*sp).size() > 0);
}

TEST_F(MarketPluginTest, BuyItem) {
    EXPECT_TRUE(CreateItemEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 0);
    ScoreboardUtils::setScore(*sp2.getPlayer(), config.TargetScoreboard, 100);

    MarketPlugin::getInstance().buyItem(*sp2.getPlayer(), this->mItemId);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 100);

    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp2.getPlayer(), "minecraft:grass_block", 1));

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(MarketPluginTest, OffshelfItem) {
    EXPECT_TRUE(CreateItemEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    InventoryUtils::clearItem(*sp, "minecraft:grass_block", 2304);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
    
    MarketPlugin::getInstance().offshelfItem(*sp, this->mItemId);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
}

TEST_F(MarketPluginTest, OffshelfItemReturn) {
    EXPECT_TRUE(CreateItemEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    InventoryUtils::clearItem(*sp, "minecraft:grass_block", 2304);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
    
    MarketPlugin::getInstance().offshelfItem(*sp, this->mItemId, true);

    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
}

TEST_F(MarketPluginTest, SellItem) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto itemStack = std::make_unique<ItemStack>();
    itemStack->reinit("minecraft:grass_block", 1, 0);
    InventoryUtils::giveItem(*sp, *itemStack, 1);

    int slot = 0;
    for (int i = 0; i < sp->mInventory->mInventory->getContainerSize(); i++) {
        ItemStack mItemStack = sp->mInventory->mInventory->getItem(i);
        
        if (!mItemStack || mItemStack.isNull() || mItemStack.getTypeName() != "minecraft:grass_block")
            continue;

        slot = i;

        break;
    }

    MarketPlugin::getInstance().sellItem(*sp, slot, "grass_block", "", "A block of grass.", 100);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
    
    std::string id = MarketPlugin::getInstance().getItems(*sp).front();
    EXPECT_FALSE(id.empty());
    EXPECT_TRUE(MarketPlugin::getInstance().hasItem(id));
}

TEST_F(MarketPluginTest, SendRequestTimeout) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    MarketPlugin::getInstance().setExecutor(executor);
    MarketPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy);

    EXPECT_TRUE(MarketPlugin::getInstance().hasTrade(*sp));

    executor.advanceTime(std::chrono::seconds(config.TradeRequestTimeout + 1));

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}

TEST_F(MarketPluginTest, SendTradeTimeout) { 
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    MarketPlugin::getInstance().setExecutor(executor);
    MarketPlugin::getInstance().sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy);

    EXPECT_TRUE(MarketPlugin::getInstance().hasTrade(*sp));

    executor.advanceTime(std::chrono::seconds(config.TradeTimeout + 1));

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}

TEST_F(MarketPluginTest, AcceptRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    MarketPlugin::getInstance().setExecutor(executor);
    MarketPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy);
    MarketPlugin::getInstance().acceptRequest(*sp2.getPlayer());

    EXPECT_TRUE(MarketPlugin::getInstance().hasTrade(*sp));

    executor.advanceTime(std::chrono::seconds(config.TradeTimeout + 1));

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}

TEST_F(MarketPluginTest, RejectRequest) { 
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    MarketPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy);
    MarketPlugin::getInstance().rejectRequest(*sp2.getPlayer());

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}

TEST_F(MarketPluginTest, CancelRequest) { 
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    MarketPlugin::getInstance().sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy);
    MarketPlugin::getInstance().cancelRequest(*sp);

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}

TEST_F(MarketPluginTest, AcceptBuyTrade) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 100);
    ScoreboardUtils::setScore(*sp2.getPlayer(), config.TargetScoreboard, 0);

    auto itemStack = std::make_unique<ItemStack>();
    itemStack->reinit("minecraft:grass_block", 1, 0);

    sp2.getPlayer()->mInventory->mInventory->setItem(0, *itemStack);

    MarketPlugin::getInstance().sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy);
    MarketPlugin::getInstance().acceptTrade(*sp2.getPlayer(), 0, 100);

    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp2.getPlayer(), "minecraft:grass_block", 1));
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard), 100);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(MarketPluginTest, AcceptSellTrade) { 
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    bool hasScoreboard = true;
    if (!ScoreboardUtils::hasScoreboard(config.TargetScoreboard)) {
        hasScoreboard = false;

        ScoreboardUtils::create(config.TargetScoreboard);
    }

    InventoryUtils::clearItem(*sp, "minecraft:grass_block", 2304);
    InventoryUtils::clearItem(*sp2.getPlayer(), "minecraft:grass_block", 2304);

    ScoreboardUtils::setScore(*sp, config.TargetScoreboard, 0);
    ScoreboardUtils::setScore(*sp2.getPlayer(), config.TargetScoreboard, 100);

    auto itemStack = std::make_unique<ItemStack>();
    itemStack->reinit("minecraft:grass_block", 1, 0);
    sp->mInventory->mInventory->setItem(0, *itemStack);

    MarketPlugin::getInstance().sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::sell);
    MarketPlugin::getInstance().acceptTrade(*sp, 0, 100);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp2.getPlayer(), "minecraft:grass_block", 1));
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, config.TargetScoreboard), 100);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp2.getPlayer(), config.TargetScoreboard), 0);

    if (!hasScoreboard)
        ScoreboardUtils::remove(config.TargetScoreboard);
}

TEST_F(MarketPluginTest, CancelTrade) { 
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    MarketPlugin::getInstance().sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy);
    MarketPlugin::getInstance().cancelTrade(*sp);

    EXPECT_FALSE(MarketPlugin::getInstance().hasTrade(*sp));
}
