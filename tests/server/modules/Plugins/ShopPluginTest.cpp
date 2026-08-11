#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"
#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"
#include "LOICollectionA/include/server/Plugins/form/ShopData.h"

using namespace LOICollection::server::Plugins;

class ShopPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!ShopPlugin::getShared()->isValid())
            GTEST_SKIP() << "ShopPlugin is not valid";
    }

    static ShopItemData MakeAppleItem() {
        ShopItemData item;
        item.type = "commodity";
        item.title = "Apple";
        item.introduce = "A red apple";
        item.number = "Buy number";
        item.id = "minecraft:apple";
        item.scores = {
            ScoreRequirement{ "test_tietu_money", 100 },
            ScoreRequirement{ "test_tietu_money2", 100 }
        };
        return item;
    }

    static ShopItemData MakeNbtAppleItem() {
        ShopItemData item;
        item.type = "commodity";
        item.title = "Nbt Apple";
        item.introduce = "A red apple";
        item.number = "Buy number";
        item.nbt = "{Count:2b,Damage:0s,Name:'minecraft:diamond',WasPickedUp:0b}";
        item.scores = {
            ScoreRequirement{ "test_tietu_money", 100 }
        };
        return item;
    }

    static ShopItemData MakeStoneItem() {
        ShopItemData item;
        item.type = "commodity";
        item.title = "Stone";
        item.introduce = "A stone";
        item.number = "Buy number";
        item.id = "minecraft:stone";
        item.scores = {
            ScoreRequirement{ "test_tietu_money", 100 },
            ScoreRequirement{ "test_tietu_money2", 100 }
        };
        return item;
    }

    static ShopItemData MakeTitleItem() {
        ShopItemData item;
        item.type = "title";
        item.title = "Test Title";
        item.introduce = "This is a test title";
        item.confirmButton = "Confirm";
        item.cancelButton = "Cancel";
        item.id = "test_tietu_title";
        item.time = 24;
        item.scores = {
            ScoreRequirement{ "test_tietu_money", 100 }
        };
        return item;
    }

    static ShopData MakeShop() {
        ShopData shop;
        shop.id = "shop_item";
        shop.type = "buy";
        shop.title = "Buy Shop Test";
        shop.content = "This is a shop test";
        shop.items = { MakeAppleItem(), MakeNbtAppleItem(), MakeStoneItem() };
        return shop;
    }

    static ShopData MakeTitleShop() {
        ShopData shop;
        shop.id = "shop_title";
        shop.type = "buy";
        shop.title = "Buy Title Shop Test";
        shop.content = "This is a title shop test";
        shop.items = { MakeTitleItem() };
        return shop;
    }
};

TEST_F(ShopPluginTest, CheckModifiedData) {
    auto shop = MakeShop();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 200);
    ScoreboardUtils::setScore(*sp, "test_tietu_money2", 200);

    auto check = ShopPlugin::getShared()->checkModifiedData(*sp, shop.items.at(0), 2);
    EXPECT_TRUE(check.has_value());
    EXPECT_TRUE(check.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 0);

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopBuyItem) {
    auto shop = MakeShop();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 300);
    ScoreboardUtils::setScore(*sp, "test_tietu_money2", 200);

    auto buy1 = ShopPlugin::getShared()->commodity(*sp, 2, shop.items.at(0), ShopType::buy);
    EXPECT_TRUE(buy1.has_value());
    EXPECT_EQ(buy1.value(), ShopActionResult::Success);

    auto buy2 = ShopPlugin::getShared()->commodity(*sp, 1, shop.items.at(1), ShopType::buy);
    EXPECT_TRUE(buy2.has_value());
    EXPECT_EQ(buy2.value(), ShopActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 0);
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:apple", 2));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:diamond", 1));

    auto buy3 = ShopPlugin::getShared()->commodity(*sp, 2, shop.items.at(0), ShopType::buy);
    EXPECT_TRUE(buy3.has_value());
    EXPECT_EQ(buy3.value(), ShopActionResult::InsufficientScore);

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopSellItem) {
    auto shop = MakeShop();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto itemStack = std::make_unique<ItemStack>();
    itemStack->reinit("minecraft:stone", 1, 0);
    InventoryUtils::giveItem(*sp, *itemStack, 2);

    auto sell1 = ShopPlugin::getShared()->commodity(*sp, 2, shop.items.at(2), ShopType::sell);
    EXPECT_TRUE(sell1.has_value());
    EXPECT_EQ(sell1.value(), ShopActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 200);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 200);
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:stone", 2));

    auto sell2 = ShopPlugin::getShared()->commodity(*sp, 2, shop.items.at(2), ShopType::sell);
    EXPECT_TRUE(sell2.has_value());
    EXPECT_EQ(sell2.value(), ShopActionResult::MissingItem);

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopBuyTitle) {
    if (!ChatPlugin::getShared()->isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    auto shop = MakeTitleShop();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto buy = ShopPlugin::getShared()->title(*sp, shop.items.at(0), ShopType::buy);
    EXPECT_TRUE(buy.has_value());
    EXPECT_EQ(buy.value(), ShopActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);

    auto hasTitle = ChatPlugin::getShared()->hasTitle(*sp, "test_tietu_title");
    EXPECT_TRUE(hasTitle.has_value());
    EXPECT_TRUE(hasTitle.value());

    auto buy2 = ShopPlugin::getShared()->title(*sp, shop.items.at(0), ShopType::buy);
    EXPECT_TRUE(buy2.has_value());
    EXPECT_EQ(buy2.value(), ShopActionResult::InsufficientScore);

    ScoreboardUtils::remove("test_tietu_money");

    EXPECT_TRUE(ChatPlugin::getShared()->delTitle(*sp, "test_tietu_title").has_value());
}

TEST_F(ShopPluginTest, ShopSellTitle) {
    if (!ChatPlugin::getShared()->isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    auto shop = MakeTitleShop();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_TRUE(ChatPlugin::getShared()->addTitle(*sp, "test_tietu_title", 24).has_value());

    auto sell = ShopPlugin::getShared()->title(*sp, shop.items.at(0), ShopType::sell);
    EXPECT_TRUE(sell.has_value());
    EXPECT_EQ(sell.value(), ShopActionResult::Success);

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 100);

    auto hasTitle = ChatPlugin::getShared()->hasTitle(*sp, "test_tietu_title");
    EXPECT_TRUE(hasTitle.has_value());
    EXPECT_FALSE(hasTitle.value());

    auto sell2 = ShopPlugin::getShared()->title(*sp, shop.items.at(0), ShopType::sell);
    EXPECT_TRUE(sell2.has_value());
    EXPECT_EQ(sell2.value(), ShopActionResult::MissingTitle);

    ScoreboardUtils::remove("test_tietu_money");
}
