#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/ShopType.h"

class Player;

namespace LOICollection::server::Plugins {
    class ShopPlugin;

    class ShopGui {
    private:
        ShopPlugin& mParent;

    public:
        ShopGui(ShopPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> menu(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> commodity(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> title(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player, const std::string& id);
    };
}
