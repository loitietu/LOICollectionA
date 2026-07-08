#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/SimulatedPlayer.h>

#include "LOICollectionA/data/JsonStorage.h"

#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"

#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

#include "LOICollectionA/include/server/Plugins/CdkPlugin.h"

using namespace LOICollection::server::Plugins;

class CdkPluginTest : public testing::Test {
protected:
    void SetUp() override {
        if (!CdkPlugin::getInstance().isValid())
            GTEST_SKIP() << "CdkPlugin is not valid";
    }

    void TearDown() override {
        CdkPlugin::getInstance().getDatabase()->write({});
        CdkPlugin::getInstance().getDatabase()->save();
    }
};

TEST_F(CdkPluginTest, CreateCdk) {
    CdkPlugin::getInstance().create("test", 3600, false);

    EXPECT_TRUE(CdkPlugin::getInstance().has("test"));
}

TEST_F(CdkPluginTest, DeleteCdk) {
    CdkPlugin::getInstance().create("test", 3600, false);
    CdkPlugin::getInstance().remove("test");

    EXPECT_FALSE(CdkPlugin::getInstance().has("test"));
}

TEST_F(CdkPluginTest, GetCdks) {
    CdkPlugin::getInstance().create("test", 3600, false);

    EXPECT_TRUE(CdkPlugin::getInstance().getCdks().size() >= 1);
}

TEST_F(CdkPluginTest, ConvertCdkNoPersonal) {
    CdkPlugin::getInstance().create("test", 3600, false);

    ScoreboardUtils::create("test_tietu1");
    ScoreboardUtils::create("test_tietu2");

    nlohmann::ordered_json data = CdkPlugin::getInstance().getDatabase()->get("test", nlohmann::ordered_json{});
    data["scores"]["test_tietu1"] = 100;
    data["scores"]["test_tietu2"] = 200;
    data["title"]["test_tietu1"] = 0;
    data["title"]["test_tietu2"] = 0;
    data["item"] = nlohmann::ordered_json::array({
        {
            { "id", "minecraft:apple" },
            { "name", "apple" },
            { "quantity", 1 },
            { "specialvalue", 0 },
            { "type", "universal" }
        }, {
            { "id", "{Count:2b,Damage:0s,Name:'minecraft:diamond',WasPickedUp:0b}" },
            { "type", "nbt" }
        } 
    });

    CdkPlugin::getInstance().getDatabase()->set("test", data);

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    CdkPlugin::getInstance().convert(*sp, "test");

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 100);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 200);
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:apple", 1));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:diamond", 2));
    
    if (ChatPlugin::getInstance().isValid()) {
        EXPECT_TRUE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu1"));
        EXPECT_TRUE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu2"));
    }

    auto playerData = CdkPlugin::getInstance().getDatabase()->get_ptr("/test/player", nlohmann::ordered_json::array());
    EXPECT_TRUE(std::find(playerData.begin(), playerData.end(), sp->getUuid().asString()) != playerData.end());

    if (CdkPlugin::getInstance().isValid()) {
        ChatPlugin::getInstance().delTitle(*sp, "test_tietu1");
        ChatPlugin::getInstance().delTitle(*sp, "test_tietu2");
    }

    ScoreboardUtils::remove("test_tietu1");
    ScoreboardUtils::remove("test_tietu2");
}

TEST_F(CdkPluginTest, ConvertCdkWithPersonal) {
    CdkPlugin::getInstance().create("test", 3600, true);

    ScoreboardUtils::create("test_tietu1");
    ScoreboardUtils::create("test_tietu2");

    nlohmann::ordered_json data = CdkPlugin::getInstance().getDatabase()->get("test", nlohmann::ordered_json{});
    data["scores"]["test_tietu1"] = 100;
    data["scores"]["test_tietu2"] = 200;
    data["title"]["test_tietu1"] = 0;
    data["title"]["test_tietu2"] = 0;
    data["item"] = nlohmann::ordered_json::array({
        {
            { "id", "minecraft:apple" },
            { "name", "apple" },
            { "quantity", 1 },
            { "specialvalue", 0 },
            { "type", "universal" }
        }, {
            { "id", "{Count:2b,Damage:0s,Name:'minecraft:diamond',WasPickedUp:0b}" },
            { "type", "nbt" }
        } 
    });

    CdkPlugin::getInstance().getDatabase()->set("test", data);

    auto sp = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(sp);

    CdkPlugin::getInstance().convert(*sp, "test");

    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu1"), 100);
    EXPECT_EQ(ScoreboardUtils::getScore(*sp, "test_tietu2"), 200);
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:apple", 1));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*sp, "minecraft:diamond", 2));
    
    if (ChatPlugin::getInstance().isValid()) {
        EXPECT_TRUE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu1"));
        EXPECT_TRUE(ChatPlugin::getInstance().hasTitle(*sp, "test_tietu2"));
    }

    EXPECT_FALSE(CdkPlugin::getInstance().has("test"));

    if (CdkPlugin::getInstance().isValid()) {
        ChatPlugin::getInstance().delTitle(*sp, "test_tietu1");
        ChatPlugin::getInstance().delTitle(*sp, "test_tietu2");
    }

    ScoreboardUtils::remove("test_tietu1");
    ScoreboardUtils::remove("test_tietu2");
}
