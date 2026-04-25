#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/BlacklistGui.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class BlacklistPlugin {
    public:
        ~BlacklistPlugin();

        BlacklistPlugin(BlacklistPlugin const&) = delete;
        BlacklistPlugin(BlacklistPlugin&&) = delete;
        BlacklistPlugin& operator=(BlacklistPlugin const&) = delete;
        BlacklistPlugin& operator=(BlacklistPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static BlacklistPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void addBlacklist(Player& player, const std::string& cause, int time);
        LOICOLLECTION_A_API   void delBlacklist(const std::string& id);

        LOICOLLECTION_A_NDAPI std::string getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBlacklistData(const std::string& id);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklists(int limit = -1);
        
        LOICOLLECTION_A_NDAPI bool hasBlacklist(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI bool isValid();
    
    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();
        
    private:
        BlacklistPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<BlacklistGui> mGui;
    };
}