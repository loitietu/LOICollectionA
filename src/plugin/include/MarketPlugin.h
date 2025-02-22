#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::market {
    namespace MainGui {
        LOICOLLECTION_A_API void buyItem(Player& player, const std::string& mItemId);
        LOICOLLECTION_A_API void itemContent(Player& player, const std::string& mItemId);
        LOICOLLECTION_A_API void sellItem(Player& player, const std::string& mNbt, int mSlot);
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
    LOICOLLECTION_A_API   void delItem(const std::string& mItemId);

    LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(std::string target);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, void* setting, std::map<std::string, std::variant<std::string, int, std::vector<std::string>>>& options);
    LOICOLLECTION_A_API   void unregistery();
}