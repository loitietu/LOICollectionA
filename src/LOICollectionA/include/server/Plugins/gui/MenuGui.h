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

        LOICOLLECTION_A_NDAPI ll::Expected<void> editNewInfo(Player& player, MenuType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editNew(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editRemove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardSetting(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardNew(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardRemove(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardCommand(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardContent(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAward(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> edit(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> custom(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> simple(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> modal(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player, const std::string& id);
    };
} 
