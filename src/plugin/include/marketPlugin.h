#ifndef LOICOLLECTION_A_MARKETPLUGIN_H
#define LOICOLLECTION_A_MARKETPLUGIN_H

#include <map>
#include <string>
#include <variant>

#include "ExportLib.h"

namespace LOICollection::Plugins::market {
    namespace MainGui {
        LOICOLLECTION_A_API void buyItem(void* player_ptr, std::string mItemId);
        LOICOLLECTION_A_API void itemContent(void* player_ptr, std::string mItemId);
        LOICOLLECTION_A_API void sellItem(void* player_ptr, std::string mNbt, int mSlot);
        LOICOLLECTION_A_API void sellItemInventory(void* player_ptr);
        LOICOLLECTION_A_API void sellItemContent(void* player_ptr);
        LOICOLLECTION_A_API void sell(void* player_ptr);
        LOICOLLECTION_A_API void buy(void* player_ptr);
    }

    LOICOLLECTION_A_API void delItem(std::string mItemId);

    LOICOLLECTION_A_API void registery(void* database, std::map<std::string, std::variant<std::string, int>>& options);
    LOICOLLECTION_A_API void unregistery();
}

#endif