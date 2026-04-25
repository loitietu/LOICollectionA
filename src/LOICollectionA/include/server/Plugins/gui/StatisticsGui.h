#pragma once

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/types/StatisticType.h"

class Player;

namespace LOICollection::server::Plugins {
    class StatisticsPlugin;

    class StatisticsGui {
    private:
        StatisticsPlugin& mParent;

    public:
        StatisticsGui(StatisticsPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player, StatisticType type);
        LOICOLLECTION_A_API void open(Player& player);
    };
}
