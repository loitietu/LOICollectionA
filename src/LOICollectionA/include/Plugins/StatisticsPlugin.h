#pragma once

#include <memory>
#include <string>
#include <vector>

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
        ~StatisticsPlugin();

        StatisticsPlugin(StatisticsPlugin const&) = delete;
        StatisticsPlugin(StatisticsPlugin&&) = delete;
        StatisticsPlugin& operator=(StatisticsPlugin const&) = delete;
        StatisticsPlugin& operator=(StatisticsPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static StatisticsPlugin& getInstance();

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_NDAPI std::string getStatisticName(StatisticType type);
        LOICOLLECTION_A_NDAPI std::string getPlayerInfo(const std::string& uuid);

        LOICOLLECTION_A_NDAPI std::vector<std::pair<std::string, int>> getRankingList(StatisticType type, int limit = -1);
        LOICOLLECTION_A_NDAPI std::vector<std::pair<std::string, int>> getStatistics(StatisticType type, int limit = -1);

        LOICOLLECTION_A_NDAPI int getStatistic(const std::string& uuid, StatisticType type);
        LOICOLLECTION_A_NDAPI int getStatistic(Player& player, StatisticType type);

        LOICOLLECTION_A_API   void addStatistic(Player& player, StatisticType type, int value);
        
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        StatisticsPlugin();
        
        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class StatisticsPlugin::gui {
    private:
        StatisticsPlugin& mParent;

    public:
        gui(StatisticsPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void open(Player& player, StatisticType type);
        LOICOLLECTION_A_API void open(Player& player);
    };
}
