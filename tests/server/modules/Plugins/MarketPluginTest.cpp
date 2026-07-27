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
        if (!MarketPlugin::getShared()->isValid())
            GTEST_SKIP() << "MarketPlugin is not valid";
    }

    void TearDown() override {
        auto db = MarketPlugin::getShared()->getDatabase();

        auto r1 = db->exec("DELETE FROM Blacklist;");
        if (!r1.has_value())
            GTEST_FAIL() << "Unable to clear data";

        auto r2 = db->exec("DELETE FROM Item;");
        if (!r2.has_value())
            GTEST_FAIL() << "Unable to clear data";

        auto r3 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB")->exec("DELETE FROM Market;");
        if (!r3.has_value())
            GTEST_FAIL() << "Unable to clear data";

        EXPECT_TRUE(MarketPlugin::getShared()->setExecutor(ll::thread::ServerThreadExecutor::getDefault()).has_value());
    }

    bool CreateBlacklistEntry() {
        auto sp2 = ll::service::getLevel()->getPlayer("test_player");

        TestSimulatedPlayer sp("test_player6");
        if (!sp.create() || !sp2)
            return false;

        auto addResult = MarketPlugin::getShared()->addBlacklist(*sp2, *sp.getPlayer());
        if (!addResult.has_value()) return false;

        auto id = MarketPlugin::getShared()->getBlacklist(*sp2, *sp.getPlayer());
        if (!id.has_value()) return false;

        this->mBlacklistId = id.value();

        auto has = MarketPlugin::getShared()->hasBlacklist(*sp2, this->mBlacklistId);
        if (!has.has_value()) return false;

        if (this->mBlacklistId.empty() || !has.value())
            return false;

        return sp.destroy();
    }

    bool CreateItemEntry() {
        auto sp = ll::service::getLevel()->getPlayer("test_player");
        if (!sp)
            return false;

        auto itemStack = std::make_unique<ItemStack>();
        itemStack->reinit("minecraft:grass_block", 1, 0);

        auto addResult = MarketPlugin::getShared()->addItem(*sp, *itemStack, "grass_block", "", "A block of grass.", 100);
        if (!addResult.has_value()) return false;

        auto items = MarketPlugin::getShared()->getItems(*sp);
        if (!items.has_value()) return false;
        if (items.value().empty()) return false;

        this->mItemId = items.value().front();

        auto has = MarketPlugin::getShared()->hasItem(this->mItemId);
        if (!has.has_value()) return false;

        if (this->mItemId.empty() || !has.value())
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

    EXPECT_TRUE(MarketPlugin::getShared()->delBlacklist(*sp, this->mBlacklistId).has_value());

    auto has = MarketPlugin::getShared()->hasBlacklist(*sp, this->mBlacklistId);
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
}

TEST_F(MarketPluginTest, GetBlacklist) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    EXPECT_TRUE(MarketPlugin::getShared()->addBlacklist(*sp, *sp2.getPlayer()).has_value());

    auto blacklists = MarketPlugin::getShared()->getBlacklist(*sp2.getPlayer());
    EXPECT_TRUE(blacklists.has_value());
    EXPECT_TRUE(blacklists.value().size() > 0);
}

TEST_F(MarketPluginTest, GetBlacklistData) {
    EXPECT_TRUE(CreateBlacklistEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto data = MarketPlugin::getShared()->getBlacklistData(this->mBlacklistId);
    EXPECT_TRUE(data.has_value());
    EXPECT_FALSE(data.value().empty());
    EXPECT_TRUE(data.value()["author"] == sp->getUuid().asString());
}

TEST_F(MarketPluginTest, CreateItemEntry) {
    EXPECT_TRUE(CreateItemEntry());
}

TEST_F(MarketPluginTest, DeleteItemEntry) {
    EXPECT_TRUE(CreateItemEntry());

    EXPECT_TRUE(MarketPlugin::getShared()->delItem(this->mItemId).has_value());

    auto has = MarketPlugin::getShared()->hasItem(this->mItemId);
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
}

TEST_F(MarketPluginTest, GetItemData) {
    EXPECT_TRUE(CreateItemEntry());

    auto data = MarketPlugin::getShared()->getItemData(this->mItemId);
    EXPECT_TRUE(data.has_value());
    EXPECT_FALSE(data.value().empty());
    EXPECT_TRUE(data.value()["name"] == "grass_block");

    auto itemsData = MarketPlugin::getShared()->getItemsData({ this->mItemId });
    EXPECT_TRUE(itemsData.has_value());
    EXPECT_TRUE(itemsData.value().contains(this->mItemId));
}

TEST_F(MarketPluginTest, GetItems) {
    EXPECT_TRUE(CreateItemEntry());

    auto items1 = MarketPlugin::getShared()->getItems();
    EXPECT_TRUE(items1.has_value());
    EXPECT_TRUE(items1.value().size() > 0);

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto items2 = MarketPlugin::getShared()->getItems(*sp);
    EXPECT_TRUE(items2.has_value());
    EXPECT_TRUE(items2.value().size() > 0);
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

    auto buy = MarketPlugin::getShared()->buyItem(*sp2.getPlayer(), this->mItemId);
    EXPECT_TRUE(buy.has_value());
    EXPECT_TRUE(buy.value());

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

    EXPECT_TRUE(MarketPlugin::getShared()->offshelfItem(*sp, this->mItemId).has_value());

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));
}

TEST_F(MarketPluginTest, OffshelfItemReturn) {
    EXPECT_TRUE(CreateItemEntry());

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    InventoryUtils::clearItem(*sp, "minecraft:grass_block", 2304);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));

    EXPECT_TRUE(MarketPlugin::getShared()->offshelfItem(*sp, this->mItemId, true).has_value());

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

    auto sell = MarketPlugin::getShared()->sellItem(*sp, slot, "grass_block", "", "A block of grass.", 100);
    EXPECT_TRUE(sell.has_value());
    EXPECT_TRUE(sell.value());

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:grass_block", 1));

    auto items = MarketPlugin::getShared()->getItems(*sp);
    ASSERT_TRUE(items.has_value());
    ASSERT_FALSE(items.value().empty());

    std::string id = items.value().front();
    EXPECT_FALSE(id.empty());

    auto has = MarketPlugin::getShared()->hasItem(id);
    EXPECT_TRUE(has.has_value());
    EXPECT_TRUE(has.value());
}

TEST_F(MarketPluginTest, SendRequestTimeout) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    EXPECT_TRUE(MarketPlugin::getShared()->setExecutor(executor).has_value());
    EXPECT_TRUE(MarketPlugin::getShared()->sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto has1 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    executor.advanceTime(std::chrono::seconds(config.TradeRequestTimeout + 1));

    auto has2 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(MarketPluginTest, SendTradeTimeout) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    EXPECT_TRUE(MarketPlugin::getShared()->setExecutor(executor).has_value());
    EXPECT_TRUE(MarketPlugin::getShared()->sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto has1 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    executor.advanceTime(std::chrono::seconds(config.TradeTimeout + 1));

    auto has2 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(MarketPluginTest, AcceptRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    Config::C_Market config = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

    MockExecutor executor;

    EXPECT_TRUE(MarketPlugin::getShared()->setExecutor(executor).has_value());
    EXPECT_TRUE(MarketPlugin::getShared()->sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto accept = MarketPlugin::getShared()->acceptRequest(*sp2.getPlayer());
    EXPECT_TRUE(accept.has_value());
    EXPECT_TRUE(accept.value());

    auto has1 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    executor.advanceTime(std::chrono::seconds(config.TradeTimeout + 1));

    auto has2 = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(MarketPluginTest, RejectRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    EXPECT_TRUE(MarketPlugin::getShared()->sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto reject = MarketPlugin::getShared()->rejectRequest(*sp2.getPlayer());
    EXPECT_TRUE(reject.has_value());
    EXPECT_TRUE(reject.value());

    auto has = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
}

TEST_F(MarketPluginTest, CancelRequest) {
    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    TestSimulatedPlayer sp2("test_player6");
    EXPECT_TRUE(sp2.create());

    EXPECT_TRUE(MarketPlugin::getShared()->sendRequest(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto cancel = MarketPlugin::getShared()->cancelRequest(*sp);
    EXPECT_TRUE(cancel.has_value());
    EXPECT_TRUE(cancel.value());

    auto has = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
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

    EXPECT_TRUE(MarketPlugin::getShared()->sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto accept = MarketPlugin::getShared()->acceptTrade(*sp2.getPlayer(), 0, 100);
    EXPECT_TRUE(accept.has_value());
    EXPECT_TRUE(accept.value());

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

    EXPECT_TRUE(MarketPlugin::getShared()->sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::sell).has_value());

    auto accept = MarketPlugin::getShared()->acceptTrade(*sp, 0, 100);
    EXPECT_TRUE(accept.has_value());
    EXPECT_TRUE(accept.value());

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

    EXPECT_TRUE(MarketPlugin::getShared()->sendTrade(*sp, *sp2.getPlayer(), MarketTradeType::buy).has_value());

    auto cancel = MarketPlugin::getShared()->cancelTrade(*sp);
    EXPECT_TRUE(cancel.has_value());
    EXPECT_TRUE(cancel.value());

    auto has = MarketPlugin::getShared()->hasTrade(*sp);
    EXPECT_TRUE(has.has_value());
    EXPECT_FALSE(has.value());
}
