#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/event/player/PlayerJoinEvent.h>

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
#include "LOICollectionA/include/server/Plugins/MutePlugin.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/ChatPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct ChatPlugin::operation {
        CommandSelector<Player> Target;
        std::string Title;
        int Time = 0;
    };

    struct ChatPlugin::Impl {
        LRUKCache<std::string, std::vector<std::string>> BlacklistCache;

        std::atomic<bool> mRegistered{ false };

        Config::C_Chat options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerChatEventListener;
        ll::event::ListenerPtr PlayerJoinEventListener;

        Impl() : BlacklistCache(100, 100) {}
    };

    ChatPlugin::ChatPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<ChatGui>(*this)) {};
    ChatPlugin::~ChatPlugin() = default;

    ChatPlugin& ChatPlugin::getInstance() {
        static ChatPlugin instance;
        return instance;
    }

    std::shared_ptr<SQLiteStorage> ChatPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> ChatPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void ChatPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("chat", tr({}, "commands.chat.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("add").required("Target").required("Title").optional("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->addTitle(*pl, param.Title, param.Time);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.add")), param.Title, pl->getRealName());
                }
            });
        command.overload<operation>().text("remove").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->delTitle(*pl, param.Title);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.remove")), pl->getRealName(), param.Title);
                }
            });
        command.overload<operation>().text("set").required("Target").required("Title").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& pl : results) {
                    this->setTitle(*pl, param.Title);

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.set")), pl->getRealName(), param.Title);
                }
            });
        command.overload<operation>().text("list").required("Target").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));

                for (Player*& player : results) {
                    std::vector<std::string> mObjectList = this->getTitles(*player);
                    
                    if (mObjectList.empty())
                        return output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), player->getRealName(), "None");

                    output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), player->getRealName(), fmt::join(mObjectList, ", "));
                }
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

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

    void ChatPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            
            if (!this->mImpl->db2->has("Chat", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", event.self().getRealName() },
                    { "title", "None" }
                };

                this->mImpl->db2->set("Chat", mObject, mData);
            }
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || MutePlugin::getInstance().isMute(event.self()))
                return;

            event.cancel();

            std::string mChat = LOICollectionAPI::APIUtils::getInstance().translate(this->mImpl->options.FormatText, event.self());
            
            TextPacket packet = TextPacket::createChat("", 
                fmt::format(fmt::runtime(mChat), event.message()), 
                "", event.self().getXuid(), ""
            );

            ll::service::getLevel()->forEachPlayer([this, packet, &player = event.self()](Player& mTarget) -> bool {
                if (!mTarget.isSimulatedPlayer() && !this->isBlacklist(mTarget, player))
                    packet.sendTo(mTarget);
                return true;
            });
        }, ll::event::EventPriority::Normal);
    }

    void ChatPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
    }

    void ChatPlugin::setTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return;

        this->mImpl->db2->set("Chat", player.getUuid().asString(), "title", text);
    }

    void ChatPlugin::addTitle(Player& player, const std::string& text, int time) {
        if (!this->isValid()) 
            return;

        std::string mObject = player.getUuid().asString();
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "title", text },
            { "author", mObject },
            { "time", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 60, "None") : "None" }
        };

        this->getDatabase()->set("Titles", mTismestamp, mData);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log2"), player)), text);
    }

    void ChatPlugin::addBlacklist(Player& player, Player& target) {
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

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log5"), player)), mTargetObject);

        if (this->mImpl->BlacklistCache.contains(mObject))
            this->mImpl->BlacklistCache.update(mObject, [mTargetObject](std::shared_ptr<std::vector<std::string>> mList) -> void {
                mList->push_back(mTargetObject);
            });
    }

    void ChatPlugin::delTitle(Player& player, const std::string& text) {
        if (!this->isValid())
             return;

        if (!this->hasTitle(player, text)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "ChatPlugin");

            return;
        }

        std::string mId = this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Titles", mId);

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->db2->get("Chat", mObject, "title", "None") == text)
            this->setTitle(player, "None");

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log3"), player)), text);
    }

    void ChatPlugin::delBlacklist(Player& player, const std::string& target) {
        if (!this->isValid())
            return;

        if (!this->hasBlacklist(player, target)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "ChatPlugin");

            return;
        }

        std::string mId = this->getDatabase()->find("Blacklist", {
            { "name", target },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return;

        this->getDatabase()->del("Blacklist", mId);

        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log6"), player)), target);

        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [target](std::shared_ptr<std::vector<std::string>> mList) -> void {
            mList->erase(std::remove(mList->begin(), mList->end(), target), mList->end());
        });
    }

    std::string ChatPlugin::getTitle(Player& player) {
        if (!this->isValid())
            return "None";

        std::string mObject = player.getUuid().asString();

        std::string mTitle = this->mImpl->db2->get("Chat", mObject, "title", "None");
        std::string mId = this->getDatabase()->find("Titles", {
            { "title", mTitle },
            { "author", mObject }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return "None";

        std::unordered_map<std::string, std::string> mData = this->getDatabase()->get("Titles", mId);
        if (SystemUtils::isPastOrPresent(mData.at("time"))) {
            this->setTitle(player, "None");

            this->getDatabase()->del("Titles", mId);
            return "None";
        }

        return mTitle;
    }

    std::string ChatPlugin::getTitleTime(Player& player, const std::string& text) {
        if (!this->isValid())
            return "None";

        std::string mId = this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);

        if (mId.empty())
            return "None";

        return this->getDatabase()->get("Titles", mId, "title", "None");
    }

    std::vector<std::string> ChatPlugin::getTitles(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Titles", "title", {
            { "author", player.getUuid().asString() }
        });
    }

    std::vector<std::string> ChatPlugin::getBlacklist(Player& player) {
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

    std::unordered_map<std::string, std::string> ChatPlugin::getBlacklistData(const std::string& target) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->get("Blacklist", target);
    }

    bool ChatPlugin::hasTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return false;

        return !this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::hasBlacklist(Player& player, const std::string& uuid) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mList = this->mImpl->BlacklistCache.get(mObject).value();

            return std::find(mList->begin(), mList->end(), uuid) != mList->end();
        }

        return !this->getDatabase()->find("Blacklist", {
            { "target", uuid },
            { "author", mObject }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::isTitle(Player& player, const std::string& text) {
        if (!this->isValid()) 
            return false;

        return !this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND).empty();
    }

    bool ChatPlugin::isBlacklist(Player& player, Player& target) {
        if (!this->isValid()) 
            return false;

        return this->hasBlacklist(player, target.getUuid().asString());
    }

    bool ChatPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    int ChatPlugin::getBlacklistUpload() {
        if (!this->isValid())
            return 0;

        return this->mImpl->options.BlacklistUpload;
    }

    bool ChatPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Chat.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "chat.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Chat;

        return true;
    }

    bool ChatPlugin::unload() {
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

    bool ChatPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db2->create("Chat", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("title");
        });

        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("target");
            ctor("author");
            ctor("time");
        });
        this->getDatabase()->create("Titles", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("title");
            ctor("author");
            ctor("time");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool ChatPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("ChatPlugin", LOICollection::server::Plugins::ChatPlugin, LOICollection::server::Plugins::ChatPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
