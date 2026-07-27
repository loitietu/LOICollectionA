#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

class Player;

namespace LOICollection::server::Plugins {
    class CdkPlugin;

    class CdkGui {
    private:
        CdkPlugin& mParent;

    public:
        CdkGui(CdkPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> convert(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkNew(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkRemoveInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkRemove(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardScore(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardItemCommon(Player& player, const std::string& id, const std::string& type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardItemType(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardInventoryConfirm(Player& player, const std::string& id, int slot);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardInventory(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardItem(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardTitle(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAwardInfo(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> cdkAward(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}
