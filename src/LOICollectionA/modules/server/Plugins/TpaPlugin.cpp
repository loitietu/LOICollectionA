#include <atomic>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

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

#include "LOICollectionA/coro/TimerManager.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc-server/ScoreboardUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/TpaPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct TpaPlugin::RequestEntry {
        std::string id;
        std::string source;
        std::string target;

        TpaType type = TpaType::tpa;
    };

    struct TpaPlugin::PlayerRequestSetEntry {
        std::unordered_set<std::string> sent;
        std::unordered_set<std::string> received;
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
        std::unique_ptr<TimerManager> mTimerManager;

        ll::ConcurrentDenseMap<std::string, RequestEntry> mRequests;
        ll::ConcurrentDenseMap<std::string, PlayerRequestSetEntry> mActiveRequest;
        ll::ConcurrentDenseMap<std::pair<std::string, std::string>, std::string> mRequestPending;

        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;
        LRUKCache<std::string, bool> InviteCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Tpa options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : mTimerManager(std::make_unique<TimerManager>(ll::thread::ServerThreadExecutor::getDefault())),
            BlacklistCache(100, 100), InviteCache(100, 100) {}
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
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                std::string mObject = player.getUuid().asString();
                auto mResults = results | std::views::filter([this, mObject](Player*& mTarget) -> bool {
                    std::vector<std::string> mList = this->getBlacklistFromTarget(this->getBlacklist(*mTarget));
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
                if (entity == nullptr || !entity->isType(ActorType::Player))
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
                if (entity == nullptr || !entity->isType(ActorType::Player))
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
                if (!this->cancelRequest(param.Id)) {
                    output.error(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.error.cancel")), param.Id);
                    return;
                }

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.tpa.success.cancel")), param.Id);
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
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

        this->mImpl->mTimerManager->cancelAll();
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
            this->mImpl->BlacklistCache.update(mObject, [mTismestamp](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTismestamp);
            });
    }

    void TpaPlugin::delBlacklist(Player& player, const std::string& id) {
        if (!this->isValid()) 
            return;

        if (!this->hasBlacklist(player, id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "TpaPlugin");

            return;
        }

        this->getDatabase()->del("Blacklist", id);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "tpa.log3"), player)), id);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [id](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), id), mList->end());
        });
    }

    void TpaPlugin::setExecutor(const ll::coro::Executor& executor) {
        if (!this->isValid())
            return;

        this->mImpl->mTimerManager->setExecutor(executor);
    }

    bool TpaPlugin::acceptRequest(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        auto it = this->mImpl->mRequests.find(id);
        if (it == this->mImpl->mRequests.end())
            return false;

        Player* origin = ll::service::getLevel()->getPlayer(mce::UUID::fromString(it->second.source));
        if (!origin) {
            player.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.gui.error"));
            return false;
        }

        if (!this->forTpaContent(*origin)) {
            origin->sendMessage(tr(LanguagePlugin::getInstance().getLanguage(*origin), "tpa.tips1"));
            return false;
        }

        origin->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*origin), "tpa.yes.tips")), player.getRealName(), id));
    
        auto& mover = (it->second.type == TpaType::tpa) ? *origin : player;
        auto& dest  = (it->second.type == TpaType::tpa) ? player : *origin;

        mover.teleport(dest.getPosition(), dest.getDimensionId());
        
        this->getLogger()->info(fmt::format(fmt::runtime(tr({}, "tpa.log1")), dest.getRealName(), mover.getRealName()));

        this->mImpl->mTimerManager->cancel(id);
        
        this->clearRequest(id);
        return true;
    }

    bool TpaPlugin::rejectRequest(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        auto it = this->mImpl->mRequests.find(id);
        if (it == this->mImpl->mRequests.end())
            return false;

        if (Player* origin = ll::service::getLevel()->getPlayer(mce::UUID::fromString(it->second.source)); origin)
            origin->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*origin), "tpa.no.tips")), player.getRealName()));

        this->mImpl->mTimerManager->cancel(id);
        
        this->clearRequest(id);
        return true;
    }

    bool TpaPlugin::cancelRequest(const std::string& id) {
        if (!this->isValid())
            return false;

        auto it = this->mImpl->mRequests.find(id);
        if (it == this->mImpl->mRequests.end())
            return false;

        this->mImpl->mTimerManager->cancel(id);
        
        this->clearRequest(id);
        return true;
    }

    bool TpaPlugin::hasRequest(const std::string& origin, const std::string& target) {
        if (!this->isValid())
            return false;

        return this->mImpl->mRequestPending.contains({ origin, target });
    }

    void TpaPlugin::clearRequest(const std::string& id){
        if (!this->isValid())
            return;

        auto it = this->mImpl->mRequests.find(id);
        if (it == this->mImpl->mRequests.end())
            return;

        this->mImpl->mActiveRequest[it->second.source].sent.erase(id);
        this->mImpl->mActiveRequest[it->second.target].received.erase(id);

        this->mImpl->mRequestPending.erase({ it->second.source, it->second.target });

        this->mImpl->mRequests.erase(id);
    }

    void TpaPlugin::sendRequest(Player& player, Player& target, const std::string& id, TpaType type) {
        if (!this->isValid())
            return;

        std::string originId = player.getUuid().asString();
        std::string targetId = target.getUuid().asString();
        if (this->hasRequest(originId, targetId))
            return;

        RequestEntry mEntry{ id, originId, targetId, type };

        this->mImpl->mRequests[id] = std::move(mEntry);

        this->mImpl->mActiveRequest[originId].sent.insert(id);
        this->mImpl->mActiveRequest[targetId].received.insert(id);

        this->mImpl->mRequestPending[{ originId, targetId }] = id;

        this->mImpl->mTimerManager->schedule(id, std::chrono::seconds(this->mImpl->options.RequestTimeout), [this, id, originId, targetId]() -> void {
            if (!this->hasRequest(originId, targetId))
                return;

            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(originId)); mPlayer)
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "tpa.tips4")), id));
            if (Player* mPlayer = ll::service::getLevel()->getPlayer(mce::UUID::fromString(targetId)); mPlayer)
                mPlayer->sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(*mPlayer), "tpa.tips4")), id));
        
            this->clearRequest(id);
        });

        player.sendMessage(fmt::format(fmt::runtime(tr(LanguagePlugin::getInstance().getLanguage(player), "tpa.tips3")), id));

        this->getLogger()->info(fmt::runtime(tr({}, "tpa.log4")), player.getRealName(), target.getRealName(), id);
    }

    std::string TpaPlugin::getBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Blacklist", {
            { "target", target.getUuid().asString() },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);
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

    std::vector<std::string> TpaPlugin::getBlacklistFromTarget(const std::vector<std::string>& ids) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Blacklist", ids)
            | std::views::values
            | std::views::transform([](const std::unordered_map<std::string, std::string>& mEntry) -> std::string {
                return mEntry.at("target");
            })
            | std::ranges::to<std::vector<std::string>>();
    }

    std::unordered_map<std::string, std::string> TpaPlugin::getBlacklistData(const std::string& id) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Blacklist", id);
    }

    bool TpaPlugin::hasBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mKeys = this->mImpl->BlacklistCache.get(mObject).value();
            return std::find(mKeys->begin(), mKeys->end(), id) != mKeys->end();
        }

        return this->getDatabase()->has("Blacklist", id);
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

        std::string originId = player.getUuid().asString();

        return static_cast<int>(this->mImpl->mActiveRequest[originId].sent.size()
            + this->mImpl->mActiveRequest[originId].received.size());
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

REGISTRY_HELPER(TpaPlugin, LOICollection::server::Plugins::TpaPlugin, LOICollection::server::Plugins::TpaPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
