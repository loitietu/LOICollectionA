#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
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

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

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

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/TpaPlugin.h"

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
    struct TpaPlugin::RequestEntry {
        std::string id;
        std::string source;
        std::string target;

        TpaType type = TpaType::tpa;
    };

    enum class SelectorType : int {
        tpa = 0,
        tphere = 1
    };

    struct TpaPlugin::operation {
        CommandSelector<Player> Target;
        SelectorType Type;
        std::string Id;
    };

    struct TpaPlugin::Impl {
        std::unordered_map<std::string, std::shared_ptr<ll::coro::InterruptableSleep>> mTimers;

        ConcurrentDenseMap<std::string, std::vector<RequestEntry>> mRequestMap;

        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;
        LRUKCache<std::string, bool> InviteCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Tpa options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : BlacklistCache(100, 100), InviteCache(100, 100) {}
    };

    TpaPlugin::TpaPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<TpaGui>(*this)) {};
    TpaPlugin::~TpaPlugin() = default;

    TpaPlugin& TpaPlugin::getInstance() {
        static TpaPlugin instance;
        return instance;
    }
    
    std::shared_ptr<SQLiteStorage> TpaPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> TpaPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void TpaPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("tpa", tr({}, "commands.tpa.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("invite").required("Type").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                std::string mObject = player.getUuid().asString();
                auto mResults = results | std::views::filter([this, mObject](Player*& mTarget) -> bool {
                    std::vector<std::string> mList = this->getBlacklist(*mTarget);
                    return !mTarget->isSimulatedPlayer() && std::find(mList.begin(), mList.end(), mObject) == mList.end() && !this->isInvite(*mTarget) && mTarget->getUuid().asString() != mObject;
                });

                int mResultSize = static_cast<int>(std::ranges::distance(mResults));
                if (this->mImpl->options.RequestUpload > 0 && mResultSize > this->mImpl->options.RequestUpload) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.invite.request")), this->mImpl->options.RequestUpload);
                    return;
                }

                int mMoney = this->mImpl->options.RequestRequired * mResultSize;
                if (ScoreboardUtils::getScore(player, this->mImpl->options.TargetScoreboard) < mMoney) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.invite")), mMoney);
                    return;
                }

                ScoreboardUtils::reduceScore(player, this->mImpl->options.TargetScoreboard, mMoney);

                for (Player*& pl : mResults) {
                    this->mGui->tpa(player, *pl, param.Type == SelectorType::tpa
                        ? TpaType::tpa : TpaType::tphere
                    );

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.success.invite")), pl->getRealName());
                }
            });
        command.overload<operation>().text("accept").required("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                if (!this->acceptRequest(player, param.Id)) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.accept")), param.Id);
                    return;
                }

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.success.accept")), param.Id);
            });
        command.overload<operation>().text("reject").required("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                if (!this->rejectRequest(player, param.Id)) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.reject")), param.Id);
                    return;
                }

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.success.reject")), param.Id);
            });
        command.overload<operation>().text("cancel").required("Id").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isRemotePlayer())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                if (!this->cancelRequest(player, param.Id)) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.cancel")), param.Id);
                    return;
                }

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.success.cancel")), param.Id);
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->setting(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void TpaPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();

            if (!this->mImpl->db2->has("Tpa", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "invite", "false" }
                };

                this->mImpl->db2->set("Tpa", mObject, mData);
            }
        });
    }

    void TpaPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);

        for (auto& it : this->mImpl->mTimers)
            it.second->interrupt();

        this->mImpl->mTimers.clear();
    }

    void TpaPlugin::setInvite(Player& player, bool invite) {
        if (!this->isValid())
            return;

        this->mImpl->db2->set("Tpa", player.getUuid().asString(), "invite", invite ? "true" : "false");
    }

    void TpaPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", SystemUtils::getNowTime("%Y%m%d%H%M%S") }
        };

        this->getDatabase()->set("Blacklist", mTismestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "tpa.log2"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTargetObject);
            });
    }

    void TpaPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid()) 
            return;

        if (!this->hasBlacklist(player, target)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "TpaPlugin");

            return;
        }
        
        std::string mId = this->getDatabase()->find("Blacklist", {
            { "name", target },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Blacklist", mId);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "tpa.log3"), player)), target);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [target](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), target), mList->end());
        });
    }

    bool TpaPlugin::acceptRequest(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::vector<RequestEntry>& mEntries = this->mImpl->mRequestMap[player.getUuid().asString()];
        auto mIt = std::ranges::find_if(mEntries, [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        if (mIt == mEntries.end())
            return false;

        Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mIt->source));
        if (!sourcePlayer) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.gui.error"));
            return false;
        }

        sourcePlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "tpa.yes.tips")), player.getRealName(), mIt->id));

        if (mIt->type == TpaType::tpa) {
            sourcePlayer->teleport(player.getPosition(), player.getDimensionId());

            this->getLogger()->info(fmt::format(fmt::runtime(tr({}, "tpa.log1")), player.getRealName(), sourcePlayer->getRealName()));
        } else {
            player.teleport(sourcePlayer->getPosition(), sourcePlayer->getDimensionId());

            this->getLogger()->info(fmt::format(fmt::runtime(tr({}, "tpa.log1")), sourcePlayer->getRealName(), player.getRealName()));
        }

        if (auto timer = this->mImpl->mTimers[mIt->id])
            timer->interrupt();
        this->mImpl->mTimers.erase(mIt->id);

        std::erase_if(this->mImpl->mRequestMap[mIt->source], [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        mEntries.erase(mIt);

        return true;
    }

    bool TpaPlugin::rejectRequest(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::vector<RequestEntry>& mEntries = this->mImpl->mRequestMap[player.getUuid().asString()];
        auto mIt = std::ranges::find_if(mEntries, [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        if (mIt == mEntries.end())
            return false;

        if (Player* sourcePlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mIt->source)); sourcePlayer)
            sourcePlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*sourcePlayer), "tpa.no.tips")), player.getRealName()));

        if (auto timer = this->mImpl->mTimers[mIt->id])
            timer->interrupt();
        this->mImpl->mTimers.erase(mIt->id);

        std::erase_if(this->mImpl->mRequestMap[mIt->source], [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        mEntries.erase(mIt);
        
        return true;
    }

    bool TpaPlugin::cancelRequest(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::vector<RequestEntry>& mEntries = this->mImpl->mRequestMap[player.getUuid().asString()];
        auto mIt = std::ranges::find_if(mEntries, [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        if (mIt == mEntries.end())
            return false;

        if (auto timer = this->mImpl->mTimers[mIt->id])
            timer->interrupt();
        this->mImpl->mTimers.erase(mIt->id);

        std::erase_if(this->mImpl->mRequestMap[mIt->target], [id](RequestEntry& entry) -> bool {
            return entry.id == id;
        });

        mEntries.erase(mIt);

        return true;
    }

    void TpaPlugin::sendRequest(Player& player, Player& target, const std::string& id, TpaType type) {
        if (!this->isValid())
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();

        RequestEntry mEntry{ id, mObject, mTargetObject, type };
        this->mImpl->mRequestMap[mObject].push_back(mEntry);
        this->mImpl->mRequestMap[mTargetObject].push_back(mEntry);

        ll::coro::keepThis([this, id, mObject, mTargetObject]() -> ll::coro::CoroTask<> {
            this->mImpl->mTimers[id] = std::make_shared<ll::coro::InterruptableSleep>();

            co_await this->mImpl->mTimers[id]->sleepFor(std::chrono::seconds(this->mImpl->options.RequestTimeout));

            std::vector<RequestEntry>& mEntries = this->mImpl->mRequestMap[mObject];
            auto mIt = std::ranges::find_if(mEntries, [id](RequestEntry& entry) -> bool {
                return entry.id == id;
            });

            if (mIt == mEntries.end())
                co_return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mObject)); mPlayer)
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "tpa.tips4")), id));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(mTargetObject)); mPlayer)
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "tpa.tips4")), id));

            mEntries.erase(mIt);
            
            std::erase_if(this->mImpl->mRequestMap[mTargetObject], [id](RequestEntry& entry) -> bool {
                return entry.id == id;
            });

            this->mImpl->mTimers.erase(id);
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        player.sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.tips3")), id));

        this->getLogger()->info(fmt::runtime(tr({}, "tpa.log4")), player.getRealName(), target.getRealName(), id);
    }

    std::vector<std::string> TpaPlugin::getBlacklist(Player& player) {
        if (!this->isValid()) 
            return {};

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->BlacklistCache.contains(mObject))
            return *this->mImpl->BlacklistCache.get(mObject).value();

        std::vector<std::string> mKeys = this->getDatabase()->find("Blacklist", {
            { "author", mObject }
        }, SQLiteStorage::FindCondition::AND);

        this->mImpl->BlacklistCache.put(mObject, mKeys);
        return mKeys;
    }

    std::unordered_map<std::string, std::string> TpaPlugin::getBlacklistData(const std::string& id) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Blacklist", id);
    }

    bool TpaPlugin::hasBlacklist(Player& player, const std::string& uuid) {
        if (!this->isValid())
            return false;

        return !this->getDatabase()->find("Blacklist", {
            { "target", uuid },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool TpaPlugin::forTpaContent(Player& player) {
        if (!this->isValid())
            return false;

        std::string mScoreboard = this->mImpl->options.TargetScoreboard;

        int mRequestRequired = this->mImpl->options.RequestRequired;
        if (mRequestRequired && ScoreboardUtils::getScore(player, mScoreboard) < mRequestRequired)
            return false;

        ScoreboardUtils::reduceScore(player, mScoreboard, mRequestRequired);

        return true;
    }

    bool TpaPlugin::isInvite(Player& player) {
        if (!this->isValid()) 
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->InviteCache.contains(mObject))
            return *this->mImpl->InviteCache.get(mObject).value();
        
        bool result = this->mImpl->db2->get("Tpa", mObject, "invite", "false") == "true";

        this->mImpl->InviteCache.put(mObject, result);
        return result;
    }

    bool TpaPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    int TpaPlugin::getBlacklistUpload() {
        if (!this->isValid())
            return 0;

        return this->mImpl->options.BlacklistUpload;
    }
    
    int TpaPlugin::getRequestUpload() {
        if (!this->isValid())
            return 0;

        return this->mImpl->options.RequestUpload;
    }
    
    int TpaPlugin::getRequestCount(Player& player) {
        if (!this->isValid())
            return 0;

        return static_cast<int>(this->mImpl->mRequestMap[player.getUuid().asString()].size());
    }

    bool TpaPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Tpa.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "tpa.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Tpa;

        return true;
    }

    bool TpaPlugin::unload() {
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

    bool TpaPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Tpa", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("invite");
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

    bool TpaPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("TpaPlugin", LOICollection::server::Plugins::TpaPlugin, LOICollection::server::Plugins::TpaPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
