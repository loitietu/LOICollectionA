#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/TpaGui.h"
#include "LOICollectionA/include/server/Plugins/types/TpaType.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class TpaPlugin {
    public:
        ~TpaPlugin();

        TpaPlugin(TpaPlugin const&) = delete;
        TpaPlugin(TpaPlugin&&) = delete;
        TpaPlugin& operator=(TpaPlugin const&) = delete;
        TpaPlugin& operator=(TpaPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static TpaPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void setInvite(Player& player, bool invite);

        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

        LOICOLLECTION_A_API   bool acceptRequest(Player& player, const std::string& id);
        LOICOLLECTION_A_API   bool rejectRequest(Player& player, const std::string& id);
        LOICOLLECTION_A_API   bool cancelRequest(Player& player, const std::string& id);
        
        LOICOLLECTION_A_API   void sendRequest(Player& player, Player& target, const std::string& id, TpaType type);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBlacklistData(const std::string& id);

        LOICOLLECTION_A_NDAPI bool hasBlacklist(Player& player, const std::string& uuid);

        LOICOLLECTION_A_NDAPI bool forTpaContent(Player& player);
        
        LOICOLLECTION_A_NDAPI bool isInvite(Player& player);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI int getBlacklistUpload();
        LOICOLLECTION_A_NDAPI int getRequestUpload();
        LOICOLLECTION_A_NDAPI int getRequestCount(Player& player);

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        TpaPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();
        
        struct RequestEntry;

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<TpaGui> mGui;
    };
}