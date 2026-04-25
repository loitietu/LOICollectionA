#pragma once

#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class CdkPlugin;

    class CdkGui {
    private:
        CdkPlugin& mParent;

    public:
        CdkGui(CdkPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void convert(Player& player);
        LOICOLLECTION_A_API void cdkNew(Player& player);
        LOICOLLECTION_A_API void cdkRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkRemove(Player& player);
        LOICOLLECTION_A_API void cdkAwardScore(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardItemCommon(Player& player, const std::string& id, const std::string& type);
        LOICOLLECTION_A_API void cdkAwardItemType(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardInventoryConfirm(Player& player, const std::string& id, int slot);
        LOICOLLECTION_A_API void cdkAwardInventory(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardTitle(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAwardInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_API void cdkAward(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}
