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
    class BlacklistPlugin {
    public:
        ~BlacklistPlugin();

        BlacklistPlugin(BlacklistPlugin const&) = delete;
        BlacklistPlugin(BlacklistPlugin&&) = delete;
        BlacklistPlugin& operator=(BlacklistPlugin const&) = delete;
        BlacklistPlugin& operator=(BlacklistPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static BlacklistPlugin& getInstance();

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void addBlacklist(Player& player, const std::string& cause, int time);
        LOICOLLECTION_A_API   void delBlacklist(const std::string& id);

        LOICOLLECTION_A_NDAPI std::string getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklists(int limit = -1);
        
        LOICOLLECTION_A_NDAPI bool hasBlacklist(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isBlacklist(const std::string& id, const std::string& uuid, const std::string& ip, const std::string& clientId);
        LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player);

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
        BlacklistPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class BlacklistPlugin::gui {
    private:
        BlacklistPlugin& mParent;

    public:
        gui(BlacklistPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void info(Player& player, const std::string& id);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}