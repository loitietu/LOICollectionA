#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/ChatGui.h"

class Player;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class ChatPlugin {
    public:
        ~ChatPlugin();

        ChatPlugin(ChatPlugin const&) = delete;
        ChatPlugin(ChatPlugin&&) = delete;
        ChatPlugin& operator=(ChatPlugin const&) = delete;
        ChatPlugin& operator=(ChatPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static ChatPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   void setTitle(Player& player, const std::string& text);

        LOICOLLECTION_A_API   void addTitle(Player& player, const std::string& text, int time);
        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void delTitle(Player& player, const std::string& text);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);

        LOICOLLECTION_A_NDAPI std::string getTitle(Player& player);
        LOICOLLECTION_A_NDAPI std::string getTitleTime(Player& player, const std::string& text);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getTitles(Player& player);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);

        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBlacklistData(const std::string& target);
        
        LOICOLLECTION_A_NDAPI bool hasTitle(Player& player, const std::string& text);
        LOICOLLECTION_A_NDAPI bool hasBlacklist(Player& player, const std::string& uuid);
        LOICOLLECTION_A_NDAPI bool isTitle(Player& player, const std::string& text); 
        LOICOLLECTION_A_NDAPI bool isBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI int getBlacklistUpload();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        ChatPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct operation;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<ChatGui> mGui;
    };
}