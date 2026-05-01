#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/utils/mc-server/InventoryUtils.h"

TEST(InventoryUtilsTest, GiveItemToPlayerAndCheckIsInInventory) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    auto itemStackStone = std::make_unique<ItemStack>();
    itemStackStone->reinit("minecraft:stone", 1, 0);

    auto itemStackDirt = std::make_unique<ItemStack>();
    itemStackDirt->reinit("minecraft:dirt", 5, 0);

    auto itemStackWood = std::make_unique<ItemStack>();
    itemStackWood->reinit("minecraft:grass_block", 10, 0);

    InventoryUtils::giveItem(*player, *itemStackStone, 1);
    InventoryUtils::giveItem(*player, *itemStackDirt, 2);
    InventoryUtils::giveItem(*player, *itemStackWood, 3);

    EXPECT_TRUE(InventoryUtils::isItemInInventory(*player, "minecraft:stone", 1));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*player, "minecraft:dirt", 2));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*player, "minecraft:grass_block", 3));
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*player, "minecraft:stone", 2));
}

TEST(InventoryUtilsTest, ClearItemFromPlayerAndCheckIsNotInInventory) {
    auto player = ll::service::getLevel()->getPlayer("test_player");
    EXPECT_TRUE(player);

    InventoryUtils::clearItem(*player, "minecraft:stone", 1);
    InventoryUtils::clearItem(*player, "minecraft:dirt", 2);
    InventoryUtils::clearItem(*player, "minecraft:grass_block", 2);

    EXPECT_FALSE(InventoryUtils::isItemInInventory(*player, "minecraft:stone", 1));
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*player, "minecraft:dirt", 2));
    EXPECT_FALSE(InventoryUtils::isItemInInventory(*player, "minecraft:grass_block", 2));
    EXPECT_TRUE(InventoryUtils::isItemInInventory(*player, "minecraft:grass_block", 1));
}