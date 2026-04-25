#pragma once

#include <string>

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

        LOICOLLECTION_A_API void buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API void itemContent(Player& player, const std::string& id);
        LOICOLLECTION_A_API void sellItem(Player& player, int mSlot);
        LOICOLLECTION_A_API void sellItemInventory(Player& player);
        LOICOLLECTION_A_API void sellItemContent(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void tradeConfirm(Player& player, Player& target, int mSlot, int score);
        LOICOLLECTION_A_API void tradeItem(Player& player, Player& target, int mSlot);
        LOICOLLECTION_A_API void tradeContent(Player& player, Player& target);
        LOICOLLECTION_A_API void tradeRequest(Player& player, Player& target, MarketTradeType type);
        LOICOLLECTION_A_API void tradeType(Player& player, Player& target);
        LOICOLLECTION_A_API void trade(Player& player);
        LOICOLLECTION_A_API void personal(Player& player);
        LOICOLLECTION_A_API void buy(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}