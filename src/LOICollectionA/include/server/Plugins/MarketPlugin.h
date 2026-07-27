#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <ll/api/Expected.h>

#include "LOICollectionA/base/Macro.h"

#include "LOICollectionA/include/ModuleBase.h"
#include "LOICollectionA/include/ModManager.h"

#include "LOICollectionA/include/server/Plugins/gui/MarketGui.h"
#include "LOICollectionA/include/server/Plugins/types/MarketType.h"

class Player;
class ItemStack;
class SQLiteStorage;

namespace ll {
    namespace io {
        class Logger;
    }

    namespace coro {
        class Executor;
    }
}

namespace LOICollection::server::Plugins {
    enum class MarketPluginErrorCode : int {
        Invalid = 1,
        TradeNotFound = 2,
        RequestNotFound = 3,
        ItemNotFound = 4,
        BlacklistNotFound = 5
    };

    struct MarketPluginErrorCategory : std::error_category {
        [[nodiscard]] const char* name() const noexcept override {
            return "MarketPluginError";
        }

        [[nodiscard]] std::string message(int ev) const override {
            switch (static_cast<MarketPluginErrorCode>(ev)) {
                case MarketPluginErrorCode::Invalid: return "Plugin is invalid";
                case MarketPluginErrorCode::TradeNotFound: return "Trade not found";
                case MarketPluginErrorCode::RequestNotFound: return "Trade request not found";
                case MarketPluginErrorCode::ItemNotFound: return "Item data not found";
                case MarketPluginErrorCode::BlacklistNotFound: return "Blacklist data not found";
                default:
                    return "Unknown";
            }
        }
    };

    class MarketPlugin : public std::enable_shared_from_this<MarketPlugin>,
                         public modules::ModuleBase,
                         public modules::AutoRegister<MarketPlugin> {
    public:
        ~MarketPlugin();

        MarketPlugin(MarketPlugin const&) = delete;
        MarketPlugin(MarketPlugin&&) = delete;
        MarketPlugin& operator=(MarketPlugin const&) = delete;
        MarketPlugin& operator=(MarketPlugin&&) = delete;

    public:
        LOICOLLECTION_A_NDAPI static std::shared_ptr<MarketPlugin> getShared();
        LOICOLLECTION_A_NDAPI static std::error_code makeErrorCode(MarketPluginErrorCode e);

        LOICOLLECTION_A_NDAPI std::shared_ptr<SQLiteStorage> getDatabase();
        LOICOLLECTION_A_NDAPI std::shared_ptr<ll::io::Logger> getLogger();

        LOICOLLECTION_A_NDAPI ll::Expected<bool> buyItem(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> offshelfItem(Player& player, const std::string& id, bool returnItem = false);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> sellItem(Player& player, int slot, const std::string& name, const std::string& icon, const std::string& intr, int score);

        LOICOLLECTION_A_NDAPI ll::Expected<void> addBlacklist(Player& player, Player& target);
        LOICOLLECTION_A_NDAPI ll::Expected<void> addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score);
        LOICOLLECTION_A_NDAPI ll::Expected<void> delBlacklist(Player& player, const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<void> delItem(const std::string& id);

        LOICOLLECTION_A_NDAPI ll::Expected<void> setExecutor(const ll::coro::Executor& executor);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> acceptRequest(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> rejectRequest(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> cancelRequest(Player& player);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> acceptTrade(Player& player, int slot, int score);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> cancelTrade(Player& player);

        LOICOLLECTION_A_NDAPI ll::Expected<void> sendRequest(Player& player, Player& target, MarketTradeType type);
        LOICOLLECTION_A_NDAPI ll::Expected<void> sendTrade(Player& player, Player& target, MarketTradeType type);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> hasTrade(Player& player);

        LOICOLLECTION_A_NDAPI ll::Expected<std::string> getBlacklist(Player& player, Player& target);

        LOICOLLECTION_A_NDAPI ll::Expected<std::vector<std::string>> getBlacklist(Player& player);
        LOICOLLECTION_A_NDAPI ll::Expected<std::vector<std::string>> getBlacklist(const std::string& target);
        LOICOLLECTION_A_NDAPI ll::Expected<std::vector<std::string>> getItems();
        LOICOLLECTION_A_NDAPI ll::Expected<std::vector<std::string>> getItems(Player& player);

        LOICOLLECTION_A_NDAPI ll::Expected<std::unordered_map<std::string, std::string>> getItemData(const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<std::unordered_map<std::string, std::string>> getBlacklistData(const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> getItemsData(const std::vector<std::string>& ids);

        LOICOLLECTION_A_NDAPI ll::Expected<bool> hasItem(const std::string& id);
        LOICOLLECTION_A_NDAPI ll::Expected<bool> hasBlacklist(Player& player, const std::string& id);

        LOICOLLECTION_A_NDAPI bool isValid();

    public:
        LOICOLLECTION_A_NDAPI std::vector<std::string> getProhibitedItems();

        LOICOLLECTION_A_NDAPI int getBlacklistUpload();
        LOICOLLECTION_A_NDAPI int getMaximumUpload();

    public:
        LOICOLLECTION_A_NDAPI std::string getName() override;

        LOICOLLECTION_A_NDAPI modules::ModulePriority getPriority() override;

        LOICOLLECTION_A_API   ll::Expected<bool> load() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unload() override;
        LOICOLLECTION_A_API   ll::Expected<bool> registry() override;
        LOICOLLECTION_A_API   ll::Expected<bool> unregistry() override;

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