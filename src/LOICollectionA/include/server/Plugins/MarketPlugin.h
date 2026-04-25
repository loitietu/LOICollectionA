#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/server/Plugins/gui/MarketGui.h"
#include "LOICollectionA/include/server/Plugins/types/MarketType.h"

class Player;
class ItemStack;
class SQLiteStorage;

namespace ll::io {
    class Logger;
}

namespace LOICollection::server::Plugins {
    class MarketPlugin {
    public:
        ~MarketPlugin();

        MarketPlugin(MarketPlugin const&) = delete;
        MarketPlugin(MarketPlugin&&) = delete;
        MarketPlugin& operator=(MarketPlugin const&) = delete;
        MarketPlugin& operator=(MarketPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static MarketPlugin& getInstance();

        LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_API   bool buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_API   bool offshelfItem(Player& player, const std::string& id, bool returnItem = false);
        LOICOLLECTION_A_API   bool sellItem(Player& player, int slot, const std::string& name, const std::string& icon, const std::string& intr, int score);

        LOICOLLECTION_A_API   void addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_API   void addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score);
        LOICOLLECTION_A_API   void delBlacklist(Player& player, const std::string& target);
        LOICOLLECTION_A_API   void delItem(const std::string& id);

        LOICOLLECTION_A_API   bool acceptRequest(Player& player);
        LOICOLLECTION_A_API   bool rejectRequest(Player& player);
        LOICOLLECTION_A_API   bool cancelRequest(Player& player);

        LOICOLLECTION_A_API   bool acceptTrade(Player& player, int slot, int score);
        LOICOLLECTION_A_API   bool cancelTrade(Player& player);

        LOICOLLECTION_A_API   void sendRequest(Player& player, Player& target, MarketTradeType type);
        LOICOLLECTION_A_API   void sendTrade(Player& player, Player& target, MarketTradeType type);

        LOICOLLECTION_A_NDAPI bool hasTrade(Player& player);

        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(Player& player);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getBlacklist(const std::string& target);
        LOICOLLECTION_A_NDAPI std::vector<std::string> getItems();
        LOICOLLECTION_A_NDAPI std::vector<std::string> getItems(Player& player);

        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getItemData(const std::string& id);
        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::string> getBlacklistData(const std::string& id);
        LOICOLLECTION_A_NDAPI std::unordered_map<std::string, std::unordered_map<std::string, std::string>> getItemsData(const std::vector<std::string>& ids);

        LOICOLLECTION_A_NDAPI bool hasItem(const std::string& id);
        LOICOLLECTION_A_NDAPI bool hasBlacklist(Player& player, const std::string& uuid);
        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::vector<std::string> getProhibitedItems();

        LOICOLLECTION_A_NDAPI int getBlacklistUpload();
        LOICOLLECTION_A_NDAPI int getMaximumUpload();

    public:
        LOICOLLECTION_A_API bool load();
        LOICOLLECTION_A_API bool unload();
        LOICOLLECTION_A_API bool registry();
        LOICOLLECTION_A_API bool unregistry();

    private:
        MarketPlugin();

        void registeryCommand();
        void listenEvent();
        void unlistenEvent();

        struct TradeEntry;

        struct Impl;
        std::unique_ptr<Impl> mImpl;
        std::unique_ptr<MarketGui> mGui;
    };
}