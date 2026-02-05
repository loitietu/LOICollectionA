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
    enum class TpaType {
        tpa,
        tphere
    };

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

        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

        LOICOLLECTION_A_API   bool acceptRequest(Player& player, const std::string& id);
        LOICOLLECTION_A_API   bool rejectRequest(Player& player, const std::string& id);
        LOICOLLECTION_A_API   bool cancelRequest(Player& player, const std::string& id);
        
        LOICOLLECTION_A_API   void sendRequest(Player& player, Player& target, const std::string& id, TpaType type);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI bool hasBlacklist(Player& player, const std::string& uuid);
        LOICOLLECTION_A_NDAPI bool isInvite(Player& player);
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
        TpaPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();
        
        struct RequestEntry;

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class TpaPlugin::gui {
    private:
        TpaPlugin& mParent;

    public:
        gui(TpaPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void generic(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void tpa(Player& player, Player& target, TpaType type);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void open(Player& player);
    };
}