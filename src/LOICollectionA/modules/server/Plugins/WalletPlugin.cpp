#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/base/Containers.h>

#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/utils/RandomUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/WalletPlugin.h"

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

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct WalletPlugin::RedEnvelopeEntry {
        std::string id;
        std::string sender;

        std::unordered_map<std::string, int> receivers;
        std::unordered_map<std::string, std::string> names;

        int count;
        int capacity;
        int people;
    };

    struct WalletPlugin::operation {
        CommandSelector<Player> Target;
        int Score = 0;
    };

    struct WalletPlugin::Impl {
        std::unordered_map<std::string, std::shared_ptr<ll::coro::InterruptableSleep>> mTimers;

        ConcurrentDenseMap<std::string, std::vector<RedEnvelopeEntry>> mRedEnvelopeMap;

        std::atomic<bool> mRegistered{ false };

        Config::C_Wallet options;
        
        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerChatEventListener;
    };

    WalletPlugin::WalletPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<WalletGui>(*this)) {};
    WalletPlugin::~WalletPlugin() = default;

    WalletPlugin& WalletPlugin::getInstance() {
        static WalletPlugin instance;
        return instance;
    }

    std::shared_ptr<ll::io::Logger> WalletPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void WalletPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("wallet", tr({}, "commands.wallet.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("transfer").required("Target").required("Score").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                std::string mScoreboard = this->mImpl->options.TargetScoreboard;

                int mMoney = param.Score * static_cast<int>(results.size());
                if (ScoreboardUtils::getScore(player, mScoreboard) < mMoney || param.Score < 0)
                    return output.error(tr(origin.getLocaleCode(), "commands.wallet.error.score"));

                ScoreboardUtils::reduceScore(player, mScoreboard, mMoney);

                int mTargetMoney = static_cast<int>(param.Score * (1 - this->mImpl->options.ExchangeRate));
                for (Player*& target : results)
                    ScoreboardUtils::addScore(*target, mScoreboard, mTargetMoney);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.wallet.success.transfer")), param.Score, results.size());
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("wealth").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->wealth(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void WalletPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db->has("Wallet", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "score", "0" }
                };

                this->mImpl->db->set("Wallet", mObject, mData);
            }

            if (int mScore = SystemUtils::toInt(this->mImpl->db->get("Wallet", mObject, "score", "0"), 0); mScore > 0) {
                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mScore);

                this->mImpl->db->set("Wallet", mObject, "score", "0");
            }
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            auto it = this->mImpl->mRedEnvelopeMap.find(event.message());
            if (it == this->mImpl->mRedEnvelopeMap.end())
                return;

            std::string mObjectUuid = event.self().getUuid().asString();
            for (auto& mObject : it->second) {
                if (mObject.receivers.find(mObjectUuid) != mObject.receivers.end())
                    continue;

                bool mLast = (mObject.people + 1) == mObject.count;

                int mTargetMoney = mLast ?
                    mObject.capacity :
                    ll::random_utils::rand(0, 
                        (mObject.capacity / (mObject.count - mObject.people + 2)) * 2
                    );

                ScoreboardUtils::addScore(event.self(), this->mImpl->options.TargetScoreboard, mTargetMoney);

                std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "wallet.tips.redenvelope.receive"), event.self());

                TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage),
                    mObject.id, mTargetMoney, (mObject.people + 1), mObject.count
                )).sendToClients();

                mObject.people++;
                mObject.capacity -= mTargetMoney;
                mObject.receivers.insert({ mObjectUuid, mTargetMoney });
                mObject.names.insert({ mObjectUuid, event.self().getRealName() });

                if (mLast) {
                    auto mKingIt = std::max_element(mObject.receivers.begin(), mObject.receivers.end(), [](const auto& a, const auto& b) -> bool {
                        return a.second < b.second;
                    });

                    std::string mMessageOver = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(event.self()), "wallet.tips.redenvelope.receive.over"), event.self());
                    
                    TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessageOver),
                        mObject.id, mObject.names.at(mKingIt->first), mKingIt->second
                    )).sendToClients();

                    it->second.erase(std::remove_if(it->second.begin(), it->second.end(), [mObjectId = mObject.id](auto& mObject) -> bool {
                        return mObject.id == mObjectId;
                    }), it->second.end());
                    
                    continue;
                }
            }
        });
    }

    void WalletPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);

        for (auto& it : this->mImpl->mTimers)
            it.second->interrupt();

        this->mImpl->mTimers.clear();
    }

    std::string WalletPlugin::getPlayerInfo(const std::string& uuid) {
        if (!this->isValid())
            return "";

        return this->mImpl->db->get("Wallet", uuid, "name", "Unknown");
    }

    std::vector<std::pair<std::string, std::string>> WalletPlugin::getPlayerInfo() {
        if (!this->isValid())
            return {};

        std::vector<std::pair<std::string, std::string>> mResult;
        for (auto& [id, data] : this->mImpl->db->get("Wallet", this->mImpl->db->list("Wallet")))
            mResult.emplace_back(id, data.at("name"));

        return mResult;
    }

    bool WalletPlugin::forTransfer(Player& player, const std::string& target, const std::string& name, int score) {
        if (!this->isValid())
            return false;

        std::string mScoreboard = this->mImpl->options.TargetScoreboard;

        if (ScoreboardUtils::getScore(player, mScoreboard) < score || score <= 0)
            return false;

        ScoreboardUtils::reduceScore(player, mScoreboard, score);

        this->transfer(target, static_cast<int>(score * (1 - this->mImpl->options.ExchangeRate)));
        this->getLogger()->info(fmt::runtime(tr({}, "wallet.log")), player.getRealName(), name, score);

        return true;
    }

    void WalletPlugin::transfer(const std::string& target, int score) {
        if (!this->isValid())
            return; 

        if (Player* mObject = ll::service::getLevel()->getPlayer(mce::UUID::fromString(target)); mObject) {
            ScoreboardUtils::addScore(*mObject, this->mImpl->options.TargetScoreboard, score);
            
            return;
        }
        
        int mWalletScore = SystemUtils::toInt(this->mImpl->db->get("Wallet", target, "score", "0"));
        this->mImpl->db->set("Wallet", target, "score", std::to_string(mWalletScore + score));
    }

    void WalletPlugin::wealth(Player& player) {
        std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(player), "wallet.showOff"), player);

        TextPacket::createRawMessage(
            fmt::format(fmt::runtime(mMessage), ScoreboardUtils::getScore(player, this->mImpl->options.TargetScoreboard))
        ).sendToClients();
    }

    void WalletPlugin::redenvelope(Player& player, const std::string& key, int score, int count) {
        if (!this->isValid())
            return; 

        ScoreboardUtils::reduceScore(player, this->getTargetScoreboard(), score * count);

        std::string mObjectId = SystemUtils::getCurrentTimestamp();

        this->mImpl->mRedEnvelopeMap[key].push_back({
            mObjectId,
            player.getUuid().asString(),
            {},
            {},
            count,
            score * count,
            0,
        });

        ll::coro::keepThis([this, mObjectId, key]() -> ll::coro::CoroTask<> {
            this->mImpl->mTimers[key] = std::make_shared<ll::coro::InterruptableSleep>();

            co_await this->mImpl->mTimers[key]->sleepFor(std::chrono::seconds(this->mImpl->options.RedEnvelopeTimeout));
            
            std::vector<RedEnvelopeEntry>& mEntries = this->mImpl->mRedEnvelopeMap[key];
            auto mIt = std::find_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            });

            if (mIt == mEntries.end())
                co_return;

            this->transfer(mIt->sender, mIt->capacity);

            TextPacket::createRawMessage(
                fmt::format(fmt::runtime(tr({}, "wallet.tips.redenvelope.timeout")), mObjectId)
            ).sendToClients();

            mEntries.erase(std::remove_if(mEntries.begin(), mEntries.end(), [mObjectId](RedEnvelopeEntry& entry) -> bool {
                return entry.id  == mObjectId;
            }), mEntries.end());

            this->mImpl->mTimers.erase(key);
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        std::string mMessage = LOICollectionAPI::APIUtils::getInstance().translate(tr(LanguagePlugin::getInstance().getLanguage(player), "wallet.tips.redenvelope.content"), player);

        TextPacket::createRawMessage(fmt::format(fmt::runtime(mMessage), 
            mObjectId, score, count, this->mImpl->options.RedEnvelopeTimeout, key
        )).sendToClients();
    }

    bool WalletPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    std::string WalletPlugin::getTargetScoreboard() {
        if (!this->isValid())
            return {};

        return this->mImpl->options.TargetScoreboard;
    }

    double WalletPlugin::getExchangeRate() {
        if (!this->isValid())
            return 0.0f;

        return this->mImpl->options.ExchangeRate;
    }

    bool WalletPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet.ModuleEnabled)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Wallet;

        return true;
    }

    bool WalletPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool WalletPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db->create("Wallet", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("score");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool WalletPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        for (auto& it : this->mImpl->mRedEnvelopeMap) {
            for (auto& mObject : it.second)
                this->transfer(mObject.sender, mObject.capacity);
        }

        this->mImpl->mRedEnvelopeMap.clear();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("WalletPlugin", LOICollection::server::Plugins::WalletPlugin, LOICollection::server::Plugins::WalletPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
