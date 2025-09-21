#pragma once

#include <string>
#include <vector>

#include "base/Macro.h"

class Player;
class ItemStack;

namespace LOICollection::Plugins::market {
    namespace MainGui {
        LOICOLLECTION_A_API void buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API void itemContent(Player& player, const std::string& id);
        LOICOLLECTION_A_API void sellItem(Player& player, int mSlot);
        LOICOLLECTION_A_API void sellItemInventory(Player& player);
        LOICOLLECTION_A_API void sellItemContent(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void sell(Player& player);
        LOICOLLECTION_A_API void buy(Player& player);
    }

    LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
    LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);
    LOICOLLECTION_A_API   void addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score);
    LOICOLLECTION_A_API   void delItem(const std::string& id);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);
    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(std::string target);
    LOICOLLECTION_A_NDAPI std::vector<std::string> getItems();
    LOICOLLECTION_A_NDAPI std::vector<std::string> getItems(Player& player);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, void* setting);
    LOICOLLECTION_A_API   void unregistery();
}