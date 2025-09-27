#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base/Macro.h"

class Player;
class ItemStack;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::Plugins {
    class MarketPlugin {
    public:
        static MarketPlugin& getInstance() {
            static MarketPlugin instance;
            return instance;
        }

        LOICOLLECTION_A_NDAPI SQLiteStorage* getDatabase();
        LOICOLLECTION_A_NDAPI ll::io::Logger* getLogger();

        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);
        LOICOLLECTION_A_API   void addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score);
        LOICOLLECTION_A_API   void delItem(const std::string& id);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(std::string target);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getItems();
        LOICOLLECTION_A_NDAPI std::vector<std::string> getItems(Player& player);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    public:
        class gui;
        friend class gui;

    private:
        MarketPlugin();
        ~MarketPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<gui> mGui;
    };

    class MarketPlugin::gui {
    private:
        MarketPlugin& mParent;

    public:
        gui(MarketPlugin& plugin) : mParent(plugin) {}

        LOICOLLECTION_A_API void buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API void itemContent(Player& player, const std::string& id);
        LOICOLLECTION_A_API void sellItem(Player& player, int mSlot);
        LOICOLLECTION_A_API void sellItemInventory(Player& player);
        LOICOLLECTION_A_API void sellItemContent(Player& player);
        LOICOLLECTION_A_API void blacklistSet(Player& player, const std::string& target);
        LOICOLLECTION_A_API void blacklistAdd(Player& player);
        LOICOLLECTION_A_API void blacklist(Player& player);
        LOICOLLECTION_A_API void sell(Player& player);
        LOICOLLECTION_A_API void buy(Player& player);
    };
}