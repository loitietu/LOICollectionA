#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"
#include "LOICollectionA/include/server/Plugins/ShopPlugin.h"

using namespace LOICollection::server::Plugins;

class ShopPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!ShopPlugin::getShared()->isValid())
            GTEST_SKIP() << "ShopPlugin is not valid";
    }

    void TearDown() override {
        ShopPlugin::getShared()->getDatabase()->write({});

        auto saveResult = ShopPlugin::getShared()->getDatabase()->save();
        if (!saveResult.has_value())
            GTEST_FAIL() << "Unable to save data";
    }

    void CreateShopEntry() {
        nlohmann::ordered_json shopItemData = {
            { "title", "'Buy Shop Test'" },
            { "content", "'This is a shop test'" },
            { "info", {
                { "exit", "execute as ${player} run say test" },
                { "score", "execute as ${player} run say test" }
            } },
            { "classiflcation", {
                {
                    { "title", "'Apple'" },
                    { "image", "" },
                    { "introduce", "'A red apple\nscores: 100'" },
                    { "number", "'Buy number'" },
                    { "id", "minecraft:apple" },
                    { "scores", {
                        { "test_tietu_money", 100 },
                        { "test_tietu_money2", 100 }
                    } },
                    { "type", "commodity" }
                },
                {
                    { "title", "'Nbt Apple'" },
                    { "image", "" },
                    { "introduce", "'A red apple\nscores: 100'" },
                    { "number", "'Buy number'" },
                    { "nbt", "{Count:2b,Damage:0s,Name:'minecraft:diamond',WasPickedUp:0b}" },
                    { "scores", {
                        { "test_tietu_money", 100 }
                    } },
                    { "type", "commodity" }
                },
                {
                    { "title", "'Stone'" },
                    { "image", "" },
                    { "introduce", "'A stone\nscores: 100'" },
                    { "number", "'Buy number'" },
                    { "id", "minecraft:stone" },
                    { "scores", {
                        { "test_tietu_money", 100 },
                        { "test_tietu_money2", 100 }
                    } },
                    { "type", "commodity" }
                }
            } },
            { "type", "buy" }
        };

        nlohmann::ordered_json shopTitleData = {
            { "title", "'Buy Title Shop Test'" },
            { "content", "'This is a title shop test'" },
            { "info", {
                { "exit", "execute as ${player} run say test" },
                { "score", "execute as ${player} run say test" }
            } },
            { "classiflcation", {
                {
                    { "title", "'Test Title'" },
                    { "image", "" },
                    { "introduce", "'This is a test title\nscores: 100'" },
                    { "confirmButton", "'Confirm'" },
                    { "cancelButton", "'Cancel'" },
                    { "id", "test_tietu_title" },
                    { "time", 24 },
                    { "scores", {
                        { "test_tietu_money", 100 },
                    }},
                    { "type", "title" }
                }
            } },
            { "type", "buy" }
        };

        ASSERT_TRUE(ShopPlugin::getShared()->create("shop_item", shopItemData).has_value());
        ASSERT_TRUE(ShopPlugin::getShared()->create("shop_title", shopTitleData).has_value());
    }
};

TEST_F(ShopPluginTest, ShopCreate) {
    CreateShopEntry();

    auto has1 = ShopPlugin::getShared()->has("shop_item");
    EXPECT_TRUE(has1.has_value());
    EXPECT_TRUE(has1.value());

    auto has2 = ShopPlugin::getShared()->has("shop_title");
    EXPECT_TRUE(has2.has_value());
    EXPECT_TRUE(has2.value());
}

TEST_F(ShopPluginTest, ShopRemove) {
    CreateShopEntry();

    EXPECT_TRUE(ShopPlugin::getShared()->remove("shop_item").has_value());
    EXPECT_TRUE(ShopPlugin::getShared()->remove("shop_title").has_value());

    auto has1 = ShopPlugin::getShared()->has("shop_item");
    EXPECT_TRUE(has1.has_value());
    EXPECT_FALSE(has1.value());

    auto has2 = ShopPlugin::getShared()->has("shop_title");
    EXPECT_TRUE(has2.has_value());
    EXPECT_FALSE(has2.value());
}

TEST_F(ShopPluginTest, CheckModifiedData) {
    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 200);
    ScoreboardUtils::setScore(*sp, "test_tietu_money2", 200);

    auto dataResult = ShopPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("shop_item");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    auto check = ShopPlugin::getShared()->checkModifiedData(*sp, data["classiflcation"].at(0), 2);
    EXPECT_TRUE(check.has_value());
    EXPECT_TRUE(check.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 0);

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopBuyItem) {
    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 300);
    ScoreboardUtils::setScore(*sp, "test_tietu_money2", 200);

    auto dataResult = ShopPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("shop_item");
    ASSERT_TRUE(dataResult.has_value());

    auto& data = dataResult.value();

    auto buy1 = ShopPlugin::getShared()->commodity(*sp, 2, data["classiflcation"].at(0), ShopType::buy);
    EXPECT_TRUE(buy1.has_value());
    EXPECT_TRUE(buy1.value());

    auto buy2 = ShopPlugin::getShared()->commodity(*sp, 1, data["classiflcation"].at(1), ShopType::buy);
    EXPECT_TRUE(buy2.has_value());
    EXPECT_TRUE(buy2.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 0);
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:apple", 2));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:diamond", 1));

    auto buy3 = ShopPlugin::getShared()->commodity(*sp, 2, data["classiflcation"].at(0), ShopType::buy);
    EXPECT_TRUE(buy3.has_value());
    EXPECT_FALSE(buy3.value());

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopSellItem) {
    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    auto itemStack = std::make_unique<ItemStack>();
    itemStack->reinit("minecraft:stone", 1, 0);
    InventoryUtils::giveItem(*sp, *itemStack, 2);

    auto dataResult = ShopPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("shop_item");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    auto sell1 = ShopPlugin::getShared()->commodity(*sp, 2, data["classiflcation"].at(2), ShopType::sell);
    EXPECT_TRUE(sell1.has_value());
    EXPECT_TRUE(sell1.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 200);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 200);
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:stone", 2));

    auto sell2 = ShopPlugin::getShared()->commodity(*sp, 2, data["classiflcation"].at(2), ShopType::sell);
    EXPECT_TRUE(sell2.has_value());
    EXPECT_FALSE(sell2.value());

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopBuyTitle) {
    if (!ChatPlugin::getShared()->isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto dataResult = ShopPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("shop_title");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    auto buy = ShopPlugin::getShared()->title(*sp, data["classiflcation"].at(0), ShopType::buy);
    EXPECT_TRUE(buy.has_value());
    EXPECT_TRUE(buy.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);

    auto hasTitle = ChatPlugin::getShared()->hasTitle(*sp, "test_tietu_title");
    EXPECT_TRUE(hasTitle.has_value());
    EXPECT_TRUE(hasTitle.value());

    auto buy2 = ShopPlugin::getShared()->title(*sp, data["classiflcation"].at(0), ShopType::buy);
    EXPECT_TRUE(buy2.has_value());
    EXPECT_FALSE(buy2.value());

    ScoreboardUtils::remove("test_tietu_money");

    EXPECT_TRUE(ChatPlugin::getShared()->delTitle(*sp, "test_tietu_title").has_value());
}

TEST_F(ShopPluginTest, ShopSellTitle) {
    if (!ChatPlugin::getShared()->isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    EXPECT_TRUE(ChatPlugin::getShared()->addTitle(*sp, "test_tietu_title", 24).has_value());

    auto dataResult = ShopPlugin::getShared()->getDatabase()->get<nlohmann::ordered_json>("shop_title");
    ASSERT_TRUE(dataResult.has_value());
    auto& data = dataResult.value();

    auto sell = ShopPlugin::getShared()->title(*sp, data["classiflcation"].at(0), ShopType::sell);
    EXPECT_TRUE(sell.has_value());
    EXPECT_TRUE(sell.value());

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 100);

    auto hasTitle = ChatPlugin::getShared()->hasTitle(*sp, "test_tietu_title");
    EXPECT_TRUE(hasTitle.has_value());
    EXPECT_FALSE(hasTitle.value());

    auto sell2 = ShopPlugin::getShared()->title(*sp, data["classiflcation"].at(0), ShopType::sell);
    EXPECT_TRUE(sell2.has_value());
    EXPECT_FALSE(sell2.value());

    ScoreboardUtils::remove("test_tietu_money");
}
