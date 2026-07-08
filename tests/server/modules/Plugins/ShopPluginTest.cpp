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
        if (!ShopPlugin::getInstance().isValid())
            GTEST_SKIP() << "ShopPlugin is not valid";
    }

    void TearDown() override {
        ShopPlugin::getInstance().getDatabase()->write({});
        ShopPlugin::getInstance().getDatabase()->save();
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

        ShopPlugin::getInstance().create("shop_item", shopItemData);
        ShopPlugin::getInstance().create("shop_title", shopTitleData);
    }
};

TEST_F(ShopPluginTest, ShopCreate) {
    CreateShopEntry();

    EXPECT_TRUE(ShopPlugin::getInstance().has("shop_item"));
    EXPECT_TRUE(ShopPlugin::getInstance().has("shop_title"));
}

TEST_F(ShopPluginTest, ShopRemove) {
    CreateShopEntry();

    ShopPlugin::getInstance().remove("shop_item");
    ShopPlugin::getInstance().remove("shop_title");

    EXPECT_FALSE(ShopPlugin::getInstance().has("shop_item"));
    EXPECT_FALSE(ShopPlugin::getInstance().has("shop_title"));
}

TEST_F(ShopPluginTest, CheckModifiedData) {
    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");
    ScoreboardUtils::create("test_tietu_money2");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 200);
    ScoreboardUtils::setScore(*sp, "test_tietu_money2", 200);

    auto data = ShopPlugin::getInstance().getDatabase()->get("shop_item", nlohmann::ordered_json{});
    EXPECT_TRUE(ShopPlugin::getInstance().checkModifiedData(*sp, data["classiflcation"].at(0), 2));

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

    auto data = ShopPlugin::getInstance().getDatabase()->get("shop_item", nlohmann::ordered_json{});
    EXPECT_TRUE(ShopPlugin::getInstance().commodity(*sp, 2, data["classiflcation"].at(0), ShopType::buy));
    EXPECT_TRUE(ShopPlugin::getInstance().commodity(*sp, 1, data["classiflcation"].at(1), ShopType::buy));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 0);
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:apple", 2));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:diamond", 1));

    EXPECT_FALSE(ShopPlugin::getInstance().commodity(*sp, 2, data["classiflcation"].at(0), ShopType::buy));

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

    auto data = ShopPlugin::getInstance().getDatabase()->get("shop_item", nlohmann::ordered_json{});
    EXPECT_TRUE(ShopPlugin::getInstance().commodity(*sp, 2, data["classiflcation"].at(2), ShopType::sell));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 200);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money2"), 200);
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*sp, "minecraft:stone", 2));

    EXPECT_FALSE(ShopPlugin::getInstance().commodity(*sp, 2, data["classiflcation"].at(2), ShopType::sell));

    ScoreboardUtils::remove("test_tietu_money");
    ScoreboardUtils::remove("test_tietu_money2");
}

TEST_F(ShopPluginTest, ShopBuyTitle) {
    if (!ChatPlugin::getInstance().isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ScoreboardUtils::setScore(*sp, "test_tietu_money", 100);

    auto data = ShopPlugin::getInstance().getDatabase()->get("shop_title", nlohmann::ordered_json{});
    EXPECT_TRUE(ShopPlugin::getInstance().title(*sp, data["classiflcation"].at(0), ShopType::buy));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 0);
    EXPECT_TRUE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu_title"));

    EXPECT_FALSE(ShopPlugin::getInstance().title(*sp, data["classiflcation"].at(0), ShopType::buy));

    ScoreboardUtils::remove("test_tietu_money");

    ChatPlugin::getInstance().delTitle(*sp, "test_tietu_title");
}

TEST_F(ShopPluginTest, ShopSellTitle) {
    if (!ChatPlugin::getInstance().isValid())
        GTEST_SKIP() << "ChatPlugin is not valid";

    CreateShopEntry();

    ScoreboardUtils::create("test_tietu_money");

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    ChatPlugin::getInstance().addTitle(*sp, "test_tietu_title", 24);

    auto data = ShopPlugin::getInstance().getDatabase()->get("shop_title", nlohmann::ordered_json{});
    EXPECT_TRUE(ShopPlugin::getInstance().title(*sp, data["classiflcation"].at(0), ShopType::sell));

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu_money"), 100);
    EXPECT_FALSE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu_title"));

    EXPECT_FALSE(ShopPlugin::getInstance().title(*sp, data["classiflcation"].at(0), ShopType::sell));

    ScoreboardUtils::remove("test_tietu_money");
}
