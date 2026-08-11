#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/MenuType.h"

class Player;

namespace LOICollection::server::Plugins {
    class MenuPlugin;

    class MenuGui {
    private:
        MenuPlugin& mParent;

    public:
        MenuGui(MenuPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> custom(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> simple(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> modal(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player, const std::string& id);
    };
} 
