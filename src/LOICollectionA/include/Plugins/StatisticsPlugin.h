#pragma once

#include <memory>
#include <string>

#include "LOICollectionA/base/Macro.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    enum class StatisticType {
        onlinetime,
        kills,
        deaths,
        place,
        destroy,
        respawn,
        join
    };

    class StatisticsPlugin {
    public:
        LOICOLLECTION_A_NDAPI static StatisticsPlugin& getInstance();

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI std::string getStatisticName(StatisticType type);

        LOICOLLECTION_A_NDAPI int getStatistic(Player& player, StatisticType type);

        LOICOLLECTION_A_API   void addStatistic(Player& player, StatisticType type, int value);
        
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        StatisticsPlugin();
        ~StatisticsPlugin();
        
        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}
