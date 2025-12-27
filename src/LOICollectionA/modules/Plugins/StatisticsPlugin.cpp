#include <atomic>
#include <memory>
#include <string>
#include <filesystem>
#include <unordered_map>

#include <fmt/format.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/base/Containers.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ThreadPoolExecutor.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/entity/MobDieEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerRespawnEvent.h>
#include <ll/api/event/player/PlayerPlaceBlockEvent.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/event/player/PlayerDestroyBlockEvent.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/StatisticsPlugin.h"

template <typename T>
struct Allocator : public std::allocator<T> {
    using std::allocator<T>::allocator;

    using is_always_equal = typename std::allocator_traits<std::allocator<T>>::is_always_equal;
};

template <
    class Key,
    class Value,
    class Hash  = phmap::priv::hash_default_hash<Key>,
    class Eq    = phmap::priv::hash_default_eq<Key>,
    class Alloc = Allocator<::std::pair<Key const, Value>>,
    size_t N    = 4,
    class Mutex = std::shared_mutex>
using ConcurrentDenseMap = phmap::parallel_flat_hash_map<Key, Value, Hash, Eq, Alloc, N, Mutex>;

namespace LOICollection::Plugins {
    struct StatisticsPlugin::Impl {
        std::unordered_map<std::string, std::string> mOnilneTime;

        ConcurrentDenseMap<std::string, ConcurrentDenseMap<std::string, int>> mCache;

        std::atomic<bool> mRegistered{ false };

        C_Config::C_Plugins::C_Statistics options;

        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        std::unordered_map<std::string, ll::event::ListenerPtr> mListeners;

        ll::thread::ThreadPoolExecutor mExecutor{ "StatisticsPlugin", std::max(static_cast<size_t>(std::thread::hardware_concurrency()) - 2, static_cast<size_t>(2)) };

        ll::coro::InterruptableSleep WirteDatabaseTaskSleep;

        std::atomic<bool> WriteDatabaseTaskRunning{ true };
    };

    StatisticsPlugin::StatisticsPlugin() : mImpl(std::make_unique<Impl>()) {}
    StatisticsPlugin::~StatisticsPlugin() = default;

    StatisticsPlugin& StatisticsPlugin::getInstance() {
        static StatisticsPlugin instance;
        return instance;
    }

    SQLiteStorage* StatisticsPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* StatisticsPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void StatisticsPlugin::listenEvent() {
        this->mImpl->WriteDatabaseTaskRunning.store(true, std::memory_order_release);

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->WriteDatabaseTaskRunning.load(std::memory_order_acquire)) {
                co_await this->mImpl->WirteDatabaseTaskSleep.sleepFor(std::chrono::minutes(this->mImpl->options.RefreshIntervalInMinutes));

                if (!this->mImpl->WriteDatabaseTaskRunning.load(std::memory_order_acquire))
                    break;

                SQLiteStorageTransaction transaction(*this->getDatabase());

                auto connection = transaction.connection();
                for (const auto& it : this->mImpl->mCache) {
                    for (const auto& it2 : it.second)
                        this->getDatabase()->set(connection, it2.first, it.first, std::to_string(it2.second));
                }

                transaction.commit();

                this->mImpl->mCache.clear();
            }
        }).launch(this->mImpl->mExecutor);

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->mListeners.emplace("PlayerJoin", eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (option.OnlineTime)
                this->mImpl->mOnilneTime.emplace(event.self().getUuid().asString(), SystemUtils::getNowTime());
            
            if (option.Join)
                this->addStatistic(event.self(), StatisticType::join, 1);
        }));

        this->mImpl->mListeners.emplace("PlayerDisconnect", eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::PlayerDisconnectEvent& event) mutable -> void {
            if (option.OnlineTime) {
                std::string mUuid = event.self().getUuid().asString();

                int mOnlineTime = SystemUtils::toInt(
                    SystemUtils::getTimeSpan(SystemUtils::getNowTime(), this->mImpl->mOnilneTime.at(mUuid), "0")
                );

                this->addStatistic(event.self(), StatisticType::onlinetime, mOnlineTime);

                this->mImpl->mOnilneTime.erase(mUuid);
            }
        }));

        this->mImpl->mListeners.emplace("MobDie", eventBus.emplaceListener<ll::event::MobDieEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::MobDieEvent& event) mutable -> void {
            if (option.Kill) {
                Actor* mSource = ll::service::getLevel()->fetchEntity(
                    event.source().isChildEntitySource() ? event.source().getEntityUniqueID() : event.source().getDamagingEntityUniqueID(), false
                );

                if (mSource && mSource->isPlayer())
                    this->addStatistic(*static_cast<Player*>(mSource), StatisticType::kills, 1);
            }

            if (event.self().isPlayer() && option.Death)
                this->addStatistic(static_cast<Player&>(event.self()), StatisticType::deaths, 1);
        }));

        this->mImpl->mListeners.emplace("PlayerPlaceBlock", eventBus.emplaceListener<ll::event::PlayerPlacedBlockEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::PlayerPlacedBlockEvent& event) mutable -> void {
            if (option.Place)
                this->addStatistic(event.self(), StatisticType::place, 1);
        }));

        this->mImpl->mListeners.emplace("PlayerDestroyBlock", eventBus.emplaceListener<ll::event::PlayerDestroyBlockEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::PlayerDestroyBlockEvent& event) mutable -> void {
            if (option.Destroy)
                this->addStatistic(event.self(), StatisticType::destroy, 1);
        }));

        this->mImpl->mListeners.emplace("PlayerRespawn", eventBus.emplaceListener<ll::event::PlayerRespawnEvent>([this, option = this->mImpl->options.DatabaseInfo](ll::event::PlayerRespawnEvent& event) mutable -> void {
            if (option.Respawn)
                this->addStatistic(event.self(), StatisticType::respawn, 1);
        }));
    }

    void StatisticsPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        for (auto& listener : this->mImpl->mListeners)
            eventBus.removeListener(listener.second);

        this->mImpl->mListeners.clear();

        this->mImpl->WriteDatabaseTaskRunning.store(false, std::memory_order_release);

        this->mImpl->WirteDatabaseTaskSleep.interrupt();
    }

    std::string StatisticsPlugin::getStatisticName(StatisticType type) {
        if (!this->isValid())
            return "";

        switch (type) {
            case StatisticType::onlinetime: return "OnlineTime";
            case StatisticType::kills: return "Kill";
            case StatisticType::deaths: return "Death";
            case StatisticType::place: return "Place";
            case StatisticType::destroy: return "Destroy";
            case StatisticType::respawn: return "Respawn";
            case StatisticType::join: return "Joins";
        }

        return "";
    }

    int StatisticsPlugin::getStatistic(Player& player, StatisticType type) {
        if (!this->isValid())
            return 0;

        std::string mUuid = player.getUuid().asString();
        std::string mTable = this->getStatisticName(type);

        if (mTable.empty())
            return 0;

        if (this->mImpl->mCache.contains(mUuid))
            return this->mImpl->mCache[mUuid][mTable];

        int result = SystemUtils::toInt(
            this->getDatabase()->get(mTable, mUuid, "0")
        );

        this->mImpl->mCache[mUuid][mTable] = result;

        return result;
    }

    void StatisticsPlugin::addStatistic(Player& player, StatisticType type, int value) {
        if (!this->isValid())
            return;

        std::string mUuid = player.getUuid().asString();
        std::string mTable = this->getStatisticName(type);

        if (mTable.empty())
            return;

        int mValue = this->getStatistic(player, type) + value;

        this->mImpl->mCache[mUuid][mTable] = mValue;
    }

    bool StatisticsPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool StatisticsPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Statistics.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "statistics.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Statistics;

        return true;
    }

    bool StatisticsPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool StatisticsPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->getDatabase()->create("OnlineTime");
        this->getDatabase()->create("Kill");
        this->getDatabase()->create("Death");
        this->getDatabase()->create("Place");
        this->getDatabase()->create("Destroy");
        this->getDatabase()->create("Respawn");
        this->getDatabase()->create("Joins");

        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool StatisticsPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("StatisticsPlugin", LOICollection::Plugins::StatisticsPlugin, LOICollection::Plugins::StatisticsPlugin::getInstance())
