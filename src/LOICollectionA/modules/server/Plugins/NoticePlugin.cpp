#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/form/PaginatedForm.h"

#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"
#include "LOICollectionA/include/server/Events/modules/NoticeEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/JsonStorage.h"
#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/NoticePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    enum class NoticeObject;

    constexpr inline auto NoticeObjectName = ll::command::enum_name_v<NoticeObject>;

    struct NoticePlugin::operation {
        ll::command::SoftEnum<NoticeObject> Object;
    };

    struct NoticePlugin::Impl {
        LRUKCache<std::string, bool> CloseCache;

        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;

        std::shared_ptr<JsonStorage> db;
        std::shared_ptr<SQLiteStorage> db2;
        std::shared_ptr<ll::io::Logger> logger;
        
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr NoticeCreateEventListener;
        ll::event::ListenerPtr NoticeDeleteEventListener;

        Impl() : CloseCache(100, 100) {}
    };

    NoticePlugin::NoticePlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<NoticeGui>(*this)) {};
    NoticePlugin::~NoticePlugin() = default;

    std::shared_ptr<NoticePlugin> NoticePlugin::getShared() {
        static auto instance = std::shared_ptr<NoticePlugin>(new NoticePlugin());
        return instance;
    }

    std::error_code NoticePlugin::makeErrorCode(NoticePluginErrorCode e) {
        static NoticePluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }
    
    std::shared_ptr<JsonStorage> NoticePlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> NoticePlugin::getLogger() {
        return this->mImpl->logger;
    }

    void NoticePlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(NoticeObjectName, this->getDatabase()->keys());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("notice", tr({}, "commands.notice.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload<operation>().text("gui").optional("Object").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player))
                    return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                (param.Object.empty() ? this->mGui->open(player) : this->mGui->notice(player, param.Object))
                    .or_else(modules::defaultErrorHandler<NoticePlugin>);

                output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
            });
        command.overload().text("edit").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            if (origin.getPermissionsLevel() < CommandPermissionLevel::GameDirectors)
                return output.error(tr(origin.getLocaleCode(), "commands.generic.permission"));

            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->edit(player).or_else(modules::defaultErrorHandler<NoticePlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("setting").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->setting(player).or_else(modules::defaultErrorHandler<NoticePlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
    }

    void NoticePlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string uuid = event.self().getUuid().asString();

            this->mImpl->db2->has("Notice", uuid)
                .and_then([this, uuid, name = event.self().getRealName()](bool exists) -> ll::Expected<void> {
                    if (!exists) {
                        std::unordered_map<std::string, std::string> data = {
                            { "name", name },
                            { "close", "false" }
                        };

                        return this->mImpl->db2->set("Notice", uuid, data);
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<NoticePlugin>);
            
            this->isClose(event.self())
                .and_then([this, &event](bool exists) -> ll::Expected<void> {
                    if (exists)
                        return {};

                    return this->mGui->notice(event.self());
                })
                .or_else(modules::defaultErrorHandler<NoticePlugin>);
        });

        this->mImpl->NoticeCreateEventListener = eventBus.emplaceListener<LOICollection::server::Events::NoticeCreateEvent>([](LOICollection::server::Events::NoticeCreateEvent& event) mutable -> void {
            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(NoticeObjectName, { event.getTarget() });
        });

        this->mImpl->NoticeDeleteEventListener = eventBus.emplaceListener<LOICollection::server::Events::NoticeDeleteEvent>([](LOICollection::server::Events::NoticeDeleteEvent& event) mutable -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(NoticeObjectName, { event.getTarget() });
        });
    }

    void NoticePlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->NoticeCreateEventListener);
        eventBus.removeListener(this->mImpl->NoticeDeleteEventListener);
    }

    ll::Expected<void> NoticePlugin::create(const std::string& id, const std::string& title, int priority, bool poiontout) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(NoticePluginErrorCode::Invalid));

        nlohmann::ordered_json data = {
            { "title", title },
            { "content", nlohmann::ordered_json::array() },
            { "priority", priority },
            { "poiontout", poiontout }
        };

        this->getDatabase()->set(id, data);

        return this->getDatabase()->save();
    }

    ll::Expected<void> NoticePlugin::remove(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(NoticePluginErrorCode::Invalid));

        this->getDatabase()->remove(id);

        return this->getDatabase()->save();
    }

    ll::Expected<void> NoticePlugin::setClose(Player& player, bool enable) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(NoticePluginErrorCode::Invalid));

        return this->mImpl->db2->set("Notice", player.getUuid().asString(), "close", enable ? "true" : "false");
    }

    ll::Expected<bool> NoticePlugin::has(const std::string& id) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(NoticePluginErrorCode::Invalid));

        return this->getDatabase()->has(id);
    }

    ll::Expected<bool> NoticePlugin::isClose(Player& player) {
        if (!this->isValid()) 
            return ll::makeErrorCodeError(makeErrorCode(NoticePluginErrorCode::Invalid));

        std::string uuid = player.getUuid().asString();

        if (this->mImpl->CloseCache.contains(uuid))
            return *this->mImpl->CloseCache.get(uuid).value();

        return this->mImpl->db2->get("Notice", uuid, "close", "false")
            .transform([this, uuid](const std::string& value) -> bool {
                bool result = (value == "true");
                
                this->mImpl->CloseCache.put(uuid, result);

                return result;
            });
    }

    bool NoticePlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr && this->mImpl->db2 != nullptr;
    }

    std::string NoticePlugin::getName() {
        return "NoticePlugin";
    }

    modules::ModulePriority NoticePlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> NoticePlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Notice)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("ConfigPath")->data());

        this->mImpl->db = std::make_shared<JsonStorage>(mDataPath / "notice.json");
        this->mImpl->db2 = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return this->mImpl->db->load()
            .transform([]() -> bool {
                return true;
            });
    }

    ll::Expected<bool> NoticePlugin::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->db2.reset();
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    ll::Expected<bool> NoticePlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        return this->mImpl->db2->create("Notice", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("close");
        }).transform([this]() -> bool {
            this->registeryCommand();
            this->listenEvent();

            this->mImpl->mRegistered.store(true, std::memory_order_release);

            return true;
        });
    }

    ll::Expected<bool> NoticePlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        return this->getDatabase()->save()
            .transform([this]() -> bool {
                this->mImpl->mRegistered.store(false, std::memory_order_release);

                return true;
            });
    }
}
