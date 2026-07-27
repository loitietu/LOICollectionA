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

        LOICOLLECTION_A_NDAPI ll::Expected<void> editNewInfo(Player& player, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editNew(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editRemove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardSetting(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardNewInfo(Player& player, const std::string& id, ShopType type, ShopAwardType awardType);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardNew(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardRemoveInfo(Player& player, const std::string& id, const std::string& packageid);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardRemove(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAwardContent(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> editAward(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> edit(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> menu(Player& player, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> commodity(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> title(Player& player, int index, const std::string& id, ShopType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player, const std::string& id);
    };
}