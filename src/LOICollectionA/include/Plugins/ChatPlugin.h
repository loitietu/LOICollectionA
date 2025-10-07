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
    class ChatPlugin {
    public:
        static ChatPlugin& getInstance() {
            static ChatPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void addTitle(Player& player, const std::string& text, int time);
        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void delTitle(Player& player, const std::string& text);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

        LOICOLLECTION_A_NDAPI std::string getTitle(Player& player);
        LOICOLLECTION_A_NDAPI std::string getTitleTime(Player& player, const std::string& text);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getTitles(Player& player);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI bool isTitle(Player& player, const std::string& text); 
        LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        ChatPlugin();
        ~ChatPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class ChatPlugin::gui {
    private:
        ChatPlugin& mParent;

    public:
        gui(ChatPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void contentAdd(Player& player, Player& target);
        LOICOLLECTION_A_API void contentRemove(Player& player, Player& target);
        LOICOLLECTION_A_API void add(Player& player);
        LOICOLLECTION_A_API void remove(Player& player);
        LOICOLLECTION_A_API void title(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void setting(Player& player);
        LOICOLLECTION_A_API void open(Player& player);
    };
}