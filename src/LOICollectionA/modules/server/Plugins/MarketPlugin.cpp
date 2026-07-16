#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/base/Containers.h>

#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/deps/nbt/Tag.h>
#include <mc/deps/nbt/CompoundTag.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/coro/TimerManager.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/InventoryUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/MarketPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct MarketPlugin::TradeEntry {
        std::string source;
        std::string target;

        MarketTradeType type = MarketTradeType::sell;
    };

    struct MarketPlugin::Impl {
        std::unique_ptr<TimerManager> mTimerManager;

        ll::ConcurrentDenseMap<std::string, TradeEntry> mTrades;
        ll::ConcurrentDenseMap<std::string, TradeEntry> mTradeRequests;

        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Market options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : mTimerManager(std::make_unique<TimerManager>(ll::thread::ServerThreadExecutor::getDefault())),
            BlacklistCache(100, 100) {}
    };

    MarketPlugin::MarketPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<MarketGui>(*this)) {};
    MarketPlugin::~MarketPlugin() = default;

    MarketPlugin& MarketPlugin::getInstance() {
        static MarketPlugin instance;
        return instance;
    }

    std::shared_ptr<SQLiteStorage> MarketPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> MarketPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void MarketPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("market", tr({}, "commands.market.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void MarketPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db2->has("Market", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "score", "0" }
                };

                this->mImpl->db2->set("Market", mObject, mData);
            }

            if (int mScore = SystemUtils::toInt(this->mImpl->db2->get("Market", mObject, "score"), 0); mScore > 0) {
                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                this->mImpl->db2->set("Market", mObject, "score", "0");
            }
        });
    }

    void MarketPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);

        this->mImpl->mTimerManager->cancelAll();
    }

    bool MarketPlugin::buyItem(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        std::string mScoreboard = this->mImpl->options.TargetScoreboard;

        std::unordered_map<std::string, std::string> mData = this->getItemData(id);

        int mScore = SystemUtils::toInt(mData.at("score"), 0);
        if (ScoreboardUtils::getScore(player, mScoreboard) < mScore) {
            player.sendMessage(tr(mObjectLanguage, "market.gui.sell.sellItem.tips3"));

            return false;
        }

        ScoreboardUtils::reduceScore(player, mScoreboard, mScore);

        ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at("data"))->mTags);
        InventoryUtils::giveItem(player, mItemStack, static_cast<int>(mItemStack.mCount));

        player.refreshInventory();

        std::string mObject = mData.at("player_uuid");
        if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer) {
            mPlayer->sendMessage(fmt::format(fmt::runtime(tr(mObjectLanguage, "market.gui.sell.sellItem.tips1")), mData.at("name")));

            ScoreboardUtils::addScore(*mPlayer, mScoreboard, mScore);
        } else {
            int mMarketScore = SystemUtils::toInt(this->mImpl->db2->get("Market", mObject, "Score", "0"), 0);

            this->mImpl->db2->set("Market", mObject, "Score", std::to_string(mMarketScore + mScore));
        }

        this->delItem(id);
        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), player)), mData.at("name"));
    
        return true;
    }

    bool MarketPlugin::offshelfItem(Player& player, const std::string& id, bool returnItem) {
        if (!this->isValid())
            return false;

        std::unordered_map<std::string, std::string> mData = this->getItemData(id);

        if (returnItem) {
            ItemStack mItemStack = ItemStack::fromTag(CompoundTag::fromSnbt(mData.at("data"))->mTags);
            InventoryUtils::giveItem(player, mItemStack, static_cast<int>(mItemStack.mCount));
        }

        player.sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(player), "market.gui.sell.sellItem.tips2")), mData.at("name")));

        this->delItem(id);
        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log3"), player)), mData.at("name"));

        return true;
    }
    
    bool MarketPlugin::sellItem(Player& player, int slot, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return false;

        ItemStack mItemStack = player.mInventory->mInventory->getItem(slot);
        if (!mItemStack || mItemStack.isNull())
            return false;

        this->addItem(player, mItemStack, name, icon, intr, score);

        player.mInventory->mInventory->removeItem(slot, 64);
        player.refreshInventory();

        return true;
    }

    void MarketPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();
        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", mTimestamp }
        };

        this->getDatabase()->set("Blacklist", mTimestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log4"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTimestamp](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTimestamp);
            });
    }

    void MarketPlugin::addItem(Player& player, ItemStack& item, const std::string& name, const std::string& icon, const std::string& intr, int score) {
        if (!this->isValid())
            return;

        std::string mTimestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", name },
            { "icon", icon },
            { "introduce", intr },
            { "score", std::to_string(score) },
            { "data", item.save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0) },
            { "player_name", player.getRealName() },
            { "player_uuid", player.getUuid().asString() }
        };

        this->getDatabase()->set("Item", mTimestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log2"), player)), name);
    }

    void MarketPlugin::delBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return;

        if (!this->hasBlacklist(player, id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "MarketPlugin");

            return;
        }

        this->getDatabase()->del("Blacklist", id);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "market.log5"), player)), id);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [id](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), id), mList->end());
        });
    }

    void MarketPlugin::delItem(const std::string& id) {
        if (!this->isValid())
            return;

        if (!this->hasItem(id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "MarketPlugin");

            return;
        }

        this->getDatabase()->del("Item", id);
    }

    void MarketPlugin::setExecutor(const ll::coro::Executor& executor) {
        if (!this->isValid())
            return;

        this->mImpl->mTimerManager->setExecutor(executor);
    }

    bool MarketPlugin::acceptRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);

        Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));
        if (!sourcePlayer) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.gui.error"));

            return false;
        }

        sourcePlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "market.yes.tips"));

        auto& mover = (mEntry.type == MarketTradeType::sell) ? player : *sourcePlayer;
        auto& dest  = (mEntry.type == MarketTradeType::sell) ? *sourcePlayer : player;

        mover.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(mover), "market.tips4"));

        this->mGui->tradeContent(dest, mover);

        this->sendTrade(*sourcePlayer, player, mEntry.type);

        this->mImpl->mTradeRequests.erase(mObject);
        this->mImpl->mTradeRequests.erase(mEntry.source);

        this->mImpl->mTimerManager->cancel(mEntry.source);

        return true;

    }

    bool MarketPlugin::rejectRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);

        if (Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source)); sourcePlayer)
            sourcePlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "market.no.tips"));
        
        this->mImpl->mTradeRequests.erase(mObject);
        this->mImpl->mTradeRequests.erase(mEntry.source);

        this->mImpl->mTimerManager->cancel(mEntry.source);

        return true;
    }

    bool MarketPlugin::cancelRequest(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTradeRequests.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTradeRequests.at(mObject);
        
        this->mImpl->mTradeRequests.erase(mObject);
        this->mImpl->mTradeRequests.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source);

        return true;
    }

    bool MarketPlugin::acceptTrade(Player& player, int slot, int score) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTrades.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTrades.at(mObject);

        ItemStack mItemStack = player.getInventory().getItem(slot);
        if (!mItemStack || mItemStack.isNull())
            return false;

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));

        if (!mPlayer)
            return false;

        if (ScoreboardUtils::getScore(*mPlayer, this->mImpl->options.TargetScoreboard) < score || score <= 0)
            return false;

        player.mInventory->mInventory->removeItem(slot, 64);
        mPlayer->getInventory().addItem(mItemStack);

        player.refreshInventory();
        mPlayer->refreshInventory();

        ScoreboardUtils::addScore(player, this->mImpl->options.TargetScoreboard, score);
        ScoreboardUtils::reduceScore(*mPlayer, this->mImpl->options.TargetScoreboard, score);

        this->mImpl->mTrades.erase(mObject);
        this->mImpl->mTrades.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source + "_trade");

        return true;
    }

    bool MarketPlugin::cancelTrade(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (!this->mImpl->mTrades.contains(mObject))
            return false;

        TradeEntry mEntry = this->mImpl->mTrades.at(mObject);

        Player* mPlayer = (mEntry.source == mObject) ?
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.target)) :
            ll::service::getLevel()->getPlayer(mce::UUID::fromString(mEntry.source));

        if (mPlayer)
            mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips6"));

        this->mImpl->mTrades.erase(mEntry.source);
        this->mImpl->mTrades.erase(mEntry.target);

        this->mImpl->mTimerManager->cancel(mEntry.source + "_trade");

        this->getLogger()->info(fmt::runtime(tr({}, "market.log8")), player.getRealName());

        return true;
    }

    void MarketPlugin::sendRequest(Player& player, Player& target, MarketTradeType type) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTradeRequests[mObject] = mEntry;
        this->mImpl->mTradeRequests[mTargetObject] = mEntry;

        this->mImpl->mTimerManager->schedule(mObject, std::chrono::seconds(this->mImpl->options.TradeRequestTimeout), [this, mObject, mTargetObject]() -> void {
            if (!this->mImpl->mTradeRequests.contains(mObject) || !this->mImpl->mTradeRequests.contains(mTargetObject))
                return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips2"));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips2"));

            this->mImpl->mTradeRequests.erase(mObject);
            this->mImpl->mTradeRequests.erase(mTargetObject);
        });

        player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "market.tips1"));

        this->getLogger()->info(fmt::runtime(tr({}, "market.log6")), player.getRealName(), target.getRealName());
    }

    void MarketPlugin::sendTrade(Player& player, Player& target, MarketTradeType type) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        TradeEntry mEntry{ mObject, mTargetObject, type };
        this->mImpl->mTrades[mObject] = mEntry;
        this->mImpl->mTrades[mTargetObject] = mEntry;

        this->mImpl->mTimerManager->schedule(mObject + "_trade", std::chrono::seconds(this->mImpl->options.TradeTimeout), [this, mObject, mTargetObject]() -> void {
            if (!this->mImpl->mTrades.contains(mObject) || !this->mImpl->mTrades.contains(mTargetObject))
                return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips5"));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer)
                mPlayer->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "market.tips5"));

            this->mImpl->mTrades.erase(mObject);
            this->mImpl->mTrades.erase(mTargetObject);
        });

        this->getLogger()->info(fmt::runtime(tr({}, "market.log7")), player.getRealName(), target.getRealName());
    }

    bool MarketPlugin::hasTrade(Player& player) {
        if (!this->isValid())
            return false;

        return this->mImpl->mTradeRequests.contains(player.getUuid().asString()) || this->mImpl->mTrades.contains(player.getUuid().asString());
    }

    std::string MarketPlugin::getBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Blacklist", {
            { "target", target.getUuid().asString() },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);
    }

    std::vector<std::string> MarketPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return {};

        return this->getBlacklist(player.getUuid().asString());
    }

    std::vector<std::string> MarketPlugin::getBlacklist(const std::string& target) {
        if (!this->isValid())
            return {};

        if (this->mImpl->BlacklistCache.contains(target))
            return *this->mImpl->BlacklistCache.get(target).value();

        std::vector<std::string> mResult = this->getDatabase()->find("Blacklist", {
            { "target", target }
        }, SQLiteStorage::FindCondition::AND);

        this->mImpl->BlacklistCache.put(target, mResult);
        return mResult;
    }

    std::vector<std::string> MarketPlugin::getItems() {
        if (!this->isValid())
            return {};

        return this->getDatabase()->list("Item");
    }

    std::vector<std::string> MarketPlugin::getItems(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Item", {
            { "player_uuid", player.getUuid().asString() }
        }, SQLiteStorage::FindCondition::AND);
    }

    std::unordered_map<std::string, std::string> MarketPlugin::getItemData(const std::string& id) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Item", id);
    }

    std::unordered_map<std::string, std::string> MarketPlugin::getBlacklistData(const std::string& id) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Blacklist", id);
    }

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> MarketPlugin::getItemsData(const std::vector<std::string>& ids) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Item", ids);
    }

    bool MarketPlugin::hasItem(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has("Item", id);
    }

    bool MarketPlugin::hasBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mKeys = this->mImpl->BlacklistCache.get(mObject).value();
            return std::find(mKeys->begin(), mKeys->end(), id) != mKeys->end();
        }

        return this->getDatabase()->has("Blacklist", id);
    }

    bool MarketPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    std::vector<std::string> MarketPlugin::getProhibitedItems() {
        if (!this->isValid())
            return {};

        return this->mImpl->options.ProhibitedItems;
    }

    int MarketPlugin::getBlacklistUpload() {
        if (!this->isValid())
            return 0;

        return this->mImpl->options.BlacklistUpload;
    }

    int MarketPlugin::getMaximumUpload() {
        if (!this->isValid())
            return 0;

        return this->mImpl->options.MaximumUpload;
    }

    bool MarketPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market.ModuleEnabled)
            return false;
        
        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "market.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Market;

        return true;
    }

    bool MarketPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool MarketPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Market", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        });

        this->getDatabase()->create("Item", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("icon");
            ctor("introduce");
            ctor("score");
            ctor("data");
            ctor("player_name");
            ctor("player_uuid");
        });
        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("target");
            ctor("author");
            ctor("time");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool MarketPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER(MarketPlugin, LOICollection::server::Plugins::MarketPlugin, LOICollection::server::Plugins::MarketPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
