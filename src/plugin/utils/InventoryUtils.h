#pragma once

#include <string>

class Player;
class ItemStack;

namespace InventoryUtils {
    void clearItem(Player& player, const std::string& mTypeName, int mNumber);
    void giveItem(Player& player, ItemStack& item, int mNumber);

    bool isItemInInventory(Player& player, const std::string& mTypeName, int mNumber);
}