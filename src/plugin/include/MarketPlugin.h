#pragma once

#include <map>
#include <string>
#include <variant>

#include "base/Macro.h"

class Player;

namespace LOICollection::Plugins::market {
    namespace MainGui {
        LOICOLLECTION_A_API void buyItem(Player& player, std::string mItemId);
        LOICOLLECTION_A_API void itemContent(Player& player, std::string mItemId);
        LOICOLLECTION_A_API void sellItem(Player& player, std::string mNbt, int mSlot);
        LOICOLLECTION_A_API void sellItemInventory(Player& player);
        LOICOLLECTION_A_API void sellItemContent(Player& player);
        LOICOLLECTION_A_API void sell(Player& player);
        LOICOLLECTION_A_API void buy(Player& player);
    }

    LOICOLLECTION_A_API   void delItem(std::string mItemId);

    LOICOLLECTION_A_NDAPI bool isValid();

    LOICOLLECTION_A_API   void registery(void* database, std::map<std::string, std::variant<std::string, int>>& options);
    LOICOLLECTION_A_API   void unregistery();
}