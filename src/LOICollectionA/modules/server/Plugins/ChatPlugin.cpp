#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

#include <fmt/core.h>

#include <ll/api/Expected.h>
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

    std::shared_ptr<ChatPlugin> ChatPlugin::getShared() {
        static auto instance = std::shared_ptr<ChatPlugin>(new ChatPlugin());
        return instance;
    }

    std::error_code ChatPlugin::makeErrorCode(ChatPluginErrorCode e) {
        static ChatPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
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
                    this->addTitle(*pl, param.Title, param.Time).or_else(modules::defaultErrorHandler<ChatPlugin>);

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
                    this->delTitle(*pl, param.Title)
                        .or_else([](ll::Error e) -> ll::Expected<void> {
                            if (e.isA<ll::ErrorCodeError>() && e.as<ll::ErrorCodeError>().ec == makeErrorCode(ChatPluginErrorCode::TitleNotFound))
                                return {};

                            return ll::Unexpected(e);
                        })
                        .or_else(modules::defaultErrorHandler<ChatPlugin>);

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
                    this->setTitle(*pl, param.Title).or_else(modules::defaultErrorHandler<ChatPlugin>);

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
                    this->getTitles(*player)
                        .transform([&output, &origin, name = player->getRealName()](const std::vector<std::string>& titles) -> void {
                             if (titles.empty())
                                return output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), name, "None");

                            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.chat.success.list")), name, fmt::join(titles, ", "));
                        })
                        .or_else(modules::defaultErrorHandler<ChatPlugin>);
                }
            });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player).or_else(modules::defaultErrorHandler<ChatPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isRemotePlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->setting(player).or_else(modules::defaultErrorHandler<ChatPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void ChatPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string uuid = event.self().getUuid().asString();

            this->mImpl->db2->has("Chat", uuid)
                .and_then([this, uuid, name = event.self().getRealName()](bool exists) -> ll::Expected<void> {
                    if (!exists) {
                        std::unordered_map<std::string, std::string> mData = {
                            { "name", name },
                            { "title", "None" }
                        };

                        return this->mImpl->db2->set("Chat", uuid, mData);
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<ChatPlugin>);
        
        });
        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) -> void {
            if (event.self().isSimulatedPlayer())
                return;

            MutePlugin::getShared()->isMute(event.self())
                .transform([this, &event](bool exists) -> void {
                    if (exists)
                        return;

                    event.cancel();

                    std::string mChat = LOICollectionAPI::APIUtils::getInstance().translate(this->mImpl->options.FormatText, event.self());
                    
                    TextPacket packet = TextPacket::createChat({}, 
                        fmt::format(fmt::runtime(mChat), event.message()), 
                        {}, event.self().getXuid(), {}
                    );

                    ll::service::getLevel()->forEachPlayer([this, &packet, &player = event.self()](Player& mTarget) -> bool {
                        if (mTarget.isSimulatedPlayer())
                            return true;

                        this->getBlacklist(mTarget, player)
                            .transform([&packet, &mTarget](const std::string& id) -> void {
                                if (!id.empty())
                                    return;

                                packet.sendTo(mTarget);
                            })
                            .or_else(modules::defaultErrorHandler<ChatPlugin>);
                        
                        return true;
                    });
                })
                .or_else(modules::defaultErrorHandler<ChatPlugin>);
        }, ll::event::EventPriority::Normal);
    }

    void ChatPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
    }

    ll::Expected<void> ChatPlugin::setTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->mImpl->db2->set("Chat", player.getUuid().asString(), "title", text);
    }

    ll::Expected<void> ChatPlugin::addTitle(Player& player, const std::string& text, int time) {
        if (!this->isValid()) 
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "title", text },
            { "author", player.getUuid().asString() },
            { "time", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 60, "None") : "None" }
        };

        return this->getDatabase()->set("Titles", mTismestamp, mData)
            .transform([this, text, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log2"), player)), text);
            });
    }

    ll::Expected<void> ChatPlugin::addBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        std::string mTargetObject = target.getUuid().asString();
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", target.getRealName() },
            { "target", mTargetObject },
            { "author", mObject },
            { "time", SystemUtils::getNowTime("%Y%m%d%H%M%S") }
        };

        return this->getDatabase()->set("Blacklist", mTismestamp, mData)
            .transform([this, mObject, mTargetObject, mTismestamp, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log5"), player)), mTargetObject);

                if (this->mImpl->BlacklistCache.contains(mObject)) {
                    this->mImpl->BlacklistCache.update(mObject, [mTismestamp](std::shared_ptr<std::vector<std::string>> mList) -> void {
                        mList->push_back(mTismestamp);
                    });
                }
            });
    }

    ll::Expected<void> ChatPlugin::delTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string uuid = player.getUuid().asString();

        return this->hasTitle(player, text)
            .and_then([this, text, uuid](bool exists) -> ll::Expected<std::string> {
                if (!exists) {
                    this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), this->getName());

                    return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::TitleNotFound));
                }

                return this->getDatabase()->find("Titles", {
                    { "title", text },
                    { "author", uuid }
                }, "", SQLiteStorage::FindCondition::AND);
            })
            .and_then([this](const std::string& id) -> ll::Expected<void> {
                if (id.empty())
                    return {};

                return this->getDatabase()->del("Titles", id);
            })
            .and_then([this, text, uuid, &player]() -> ll::Expected<void> {
                if (this->mImpl->db2->get("Chat", uuid, "title", "None") == text)
                    return this->setTitle(player, "None");

                return {};
            })
            .transform([this, text, &player]() -> void {
                this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log3"), player)), text);
            });
    }

    ll::Expected<void> ChatPlugin::delBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->hasBlacklist(player, id)
            .and_then([this, id, &player](bool exists) -> ll::Expected<void> {
                if (!exists) {
                    this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), this->getName());

                    return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::BlacklistNotFound));
                }

                return this->getDatabase()->del("Blacklist", id)
                    .transform([this, id, &player]() -> void { 
                        this->getLogger()->info(fmt::runtime(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "chat.log6"), player)), id);

                        this->mImpl->BlacklistCache.update(player.getUuid().asString(), [id](std::shared_ptr<std::vector<std::string>> mList) -> void {
                            mList->erase(std::remove(mList->begin(), mList->end(), id), mList->end());
                        });
                    });
            });
    }

    ll::Expected<std::string> ChatPlugin::getTitle(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string uuid = player.getUuid().asString();

        return this->mImpl->db2->get("Chat", uuid, "title", "None")
            .and_then([this, uuid, &player](const std::string& title) -> ll::Expected<std::string> {
                return this->getDatabase()->find("Titles", {
                    { "title", title },
                    { "author", uuid }
                }, "", SQLiteStorage::FindCondition::AND)
                    .and_then([this, title, &player](const std::string& id) -> ll::Expected<std::string> {
                        auto data = this->getDatabase()->get("Titles", id);
                        if (!data.has_value())
                            return ll::Unexpected(data.error());

                        if (data.value().empty())
                            return "None";

                        if (SystemUtils::isPastOrPresent(data.value().at("time"))) {
                            auto result = this->setTitle(player, "None").and_then([this, id]() -> ll::Expected<void> {
                                return this->getDatabase()->del("Titles", id);
                            });

                            if (!result.has_value())
                                return ll::Unexpected(result.error());

                            return "None";
                        }

                        return title;
                    });
            });
    }

    ll::Expected<std::string> ChatPlugin::getTitleTime(Player& player, const std::string& text) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND)
            .and_then([this](const std::string& id) -> ll::Expected<std::string> {
                if (id.empty())
                    return "None";

                return this->getDatabase()->get("Titles", id, "title", "None");
            });
    }

    ll::Expected<std::string> ChatPlugin::getBlacklist(Player& player, Player& target) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->getDatabase()->find("Blacklist", {
            { "target", target.getUuid().asString() },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND);
    }

    ll::Expected<std::vector<std::string>> ChatPlugin::getTitles(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->getDatabase()->find("Titles", "title", {
            { "author", player.getUuid().asString() }
        });
    }

    ll::Expected<std::vector<std::string>> ChatPlugin::getBlacklist(Player& player) {
        if (!this->isValid()) 
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string uuid = player.getUuid().asString();
        if (this->mImpl->BlacklistCache.contains(uuid))
            return *this->mImpl->BlacklistCache.get(uuid).value();

        return this->getDatabase()->find("Blacklist", {
            { "author", uuid }
        }, SQLiteStorage::FindCondition::AND)
            .transform([this, uuid](const std::vector<std::string>& keys) -> std::vector<std::string> {
                this->mImpl->BlacklistCache.put(uuid, keys);
                return keys;
            });
    }

    ll::Expected<std::unordered_map<std::string, std::string>> ChatPlugin::getBlacklistData(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->getDatabase()->get("Blacklist", id);
    }

    ll::Expected<bool> ChatPlugin::hasTitle(Player& player, const std::string& text) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        return this->getDatabase()->find("Titles", {
            { "title", text },
            { "author", player.getUuid().asString() }
        }, "", SQLiteStorage::FindCondition::AND)
            .transform([](const std::string& id) -> bool {
                return !id.empty(); 
            });
    }

    ll::Expected<bool> ChatPlugin::hasBlacklist(Player& player, const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(ChatPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();
        if (this->mImpl->BlacklistCache.contains(mObject)) {
            auto mKeys = this->mImpl->BlacklistCache.get(mObject).value();
            return std::find(mKeys->begin(), mKeys->end(), id) != mKeys->end();
        }

        return this->getDatabase()->has("Blacklist", id);
    }

    bool ChatPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    int ChatPlugin::getBlacklistUpload() {
        return this->mImpl->options.BlacklistUpload;
    }

    std::string ChatPlugin::getName() {
        return "ChatPlugin";
    }

    modules::ModulePriority ChatPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> ChatPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Chat.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "chat.db").string());
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Chat;

        return true;
    }

    ll::Expected<bool> ChatPlugin::unload() {
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

    ll::Expected<bool> ChatPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        return this->mImpl->db2->create("Chat", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("title");
        }).and_then([this]() -> ll::Expected<void> {
            return this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
                ctor("name");
                ctor("target");
                ctor("author");
                ctor("time");
            });
        }).and_then([this]() -> ll::Expected<void> {
            return this->getDatabase()->create("Titles", [](SQLiteStorage::ColumnCallback ctor) -> void {
                ctor("title");
                ctor("author");
                ctor("time");
            });
        }).transform([this]() -> bool {
            this->registeryCommand();
            this->listenEvent();

            this->mImpl->mRegistered.store(true, std::memory_order_release);

            return true;
        });
    }

    ll::Expected<bool> ChatPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        return this->getDatabase()->exec("VACUUM;")
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
