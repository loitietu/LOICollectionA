#pragma once

#include <string>

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

        LOICOLLECTION_A_API void editNewInfo(Player& player, ShopType type);
        LOICOLLECTION_A_API void editNew(Player& player);
        LOICOLLECTION_A_API void editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void editRemove(Player& player);
        LOICOLLECTION_A_API void editAwardSetting(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardNewInfo(Player& player, const std::string& id, ShopType type, ShopAwardType awardType);
        LOICOLLECTION_A_API void editAwardNew(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_API void editAwardRemove(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAwardContent(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void editAward(Player& player);
        LOICOLLECTION_A_API void edit(Player& player);
        LOICOLLECTION_A_API void menu(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void commodity(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void title(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_API void open(Player& player, const std::string& id);
    };
}