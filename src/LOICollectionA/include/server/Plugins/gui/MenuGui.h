#pragma once

#include <string>

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

        LOICOLLECTION_A_API void editNewInfo(Player& player, MenuType type);
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAwardCommand(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& id, MenuType type);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void custom(Player& player, const std::string& id);
        LOICOLLECTION_A_API void simple(Player& player, const std::string& id);
        LOICOLLECTION_A_API void modal(Player& player, const std::string& id);
        LOICOLLECTION_A_API void open(Player& player, const std::string& id);
    };
} 
