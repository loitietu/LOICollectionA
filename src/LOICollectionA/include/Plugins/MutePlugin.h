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
    class MutePlugin {
    public:
        ~MutePlugin();

        MutePlugin(MutePlugin const&) = delete;
        MutePlugin(MutePlugin&&) = delete;
        MutePlugin& operator=(MutePlugin const&) = delete;
        MutePlugin& operator=(MutePlugin&&) = delete;    

    public:
        LOICOLLECTION_A_NDAPI static MutePlugin& getInstance();

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void addMute(Player& player, const std::string& cause, int time);
        LOICOLLECTION_A_API   void delMute(Player& player);
        LOICOLLECTION_A_API   void delMute(const std::string& id);

        LOICOLLECTION_A_NDAPI std::string getMute(Player& player);
        
        LOICOLLECTION_A_NDAPI std::vector<std::string> getMutes(int limit = -1);
        
        LOICOLLECTION_A_NDAPI bool hasMute(const std::string& id);
        LOICOLLECTION_A_NDAPI bool isMute(Player& player);
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
        MutePlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class MutePlugin::gui {
    private:
        MutePlugin& mParent;

    public:
        gui(MutePlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void info(Player& player, const std::string& id);
        LOICOLLECTION_A_API void content(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}