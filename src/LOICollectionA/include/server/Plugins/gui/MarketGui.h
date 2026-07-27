#pragma once

#include <string>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/MarketType.h"

class Player;

namespace LOICollection::server::Plugins {
    class MarketPlugin;

    class MarketGui {
    private:
        MarketPlugin& mParent;

    public:
        MarketGui(MarketPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_NDAPI ll::Expected<void> buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> itemContent(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> sellItem(Player& player, int mSlot);
        LOICOLLECTION_A_NDAPI ll::Expected<void> sellItemInventory(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> sellItemContent(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklistAdd(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> blacklist(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> tradeConfirm(Player& player, Player& target, int mSlot, int score);
        LOICOLLECTION_A_NDAPI ll::Expected<void> tradeItem(Player& player, Player& target, int mSlot);
        LOICOLLECTION_A_NDAPI ll::Expected<void> tradeContent(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> tradeRequest(Player& player, Player& target, MarketTradeType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> tradeType(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> trade(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> personal(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> buy(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<void> open(Player& player);
    };
}