#include <atomic>
#include <memory>
#include <string>

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
#include <ll/api/event/player/PlayerJoinEvent.h>

#include <mc/world/level/Level.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/server/APIUtils.h"
#include "LOICollectionA/include/server/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/server/Events/player/PlayerHurtEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/Throttle.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/Plugins/PvpPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::server::Plugins {
    struct PvpPlugin::Impl {
        LRUKCache<std::string, bool> PvpCache;

        Throttle mThrottle;

        std::atomic<bool> mRegistered{ false };

        Config::C_Pvp options;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerHurtEventListener;
        
        Impl() : PvpCache(100, 100), mThrottle(std::chrono::seconds(1)) {}
    };

    PvpPlugin::PvpPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<PvpGui>(*this)) {};
    PvpPlugin::~PvpPlugin() = default;

    std::shared_ptr<PvpPlugin> PvpPlugin::getShared() {
        static auto instance = std::shared_ptr<PvpPlugin>(new PvpPlugin());
        return instance;
    }

    std::error_code PvpPlugin::makeErrorCode(PvpPluginErrorCode e) {
        static PvpPluginErrorCategory cat;
        return std::error_code{ static_cast<int>(e), cat };
    }

    std::shared_ptr<ll::io::Logger> PvpPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void PvpPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("pvp", tr({}, "commands.pvp.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player).or_else(modules::defaultErrorHandler<PvpPlugin>);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("off").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->enable(player, false).or_else(modules::defaultErrorHandler<PvpPlugin>);

            output.success(tr(origin.getLocaleCode(), "commands.pvp.success.disable"));
        });
        command.overload().text("on").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isType(ActorType::Player))
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->enable(player, true).or_else(modules::defaultErrorHandler<PvpPlugin>);

            output.success(tr(origin.getLocaleCode(), "commands.pvp.success.enable"));
        });
    }

    void PvpPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();

            this->mImpl->db->has("Pvp", mObject)
                .and_then([this, mObject](bool exists) -> ll::Expected<void> {
                    if (!exists) {
                        std::unordered_map<std::string, std::string> mData = {
                            { "name", mObject },
                            { "enable", "false" }
                        };

                        return this->mImpl->db->set("Pvp", mObject, mData);
                    }

                    return {};
                })
                .or_else(modules::defaultErrorHandler<PvpPlugin>);
        });
        this->mImpl->PlayerHurtEventListener = eventBus.emplaceListener<LOICollection::server::Events::PlayerHurtEvent>([this](LOICollection::server::Events::PlayerHurtEvent& event) mutable -> void {
            if (!event.getSource().isRemotePlayer() || event.getSource().isSimulatedPlayer() || event.self().isSimulatedPlayer())
                return;

            switch (event.getReason()) {
                case LOICollection::server::Events::PlayerHurtReason::Hurt: if (!this->mImpl->options.ExtraListener.onActorHurt) return;
                case LOICollection::server::Events::PlayerHurtReason::Effect: if (!this->mImpl->options.ExtraListener.onSplashPotion) return;
                case LOICollection::server::Events::PlayerHurtReason::Projectile: if (!this->mImpl->options.ExtraListener.onProjectileHit) return;
            }

            auto& source = static_cast<Player&>(event.getSource());

            this->isEnable(event.self())
                .and_then([this, &source](bool exists) -> ll::Expected<bool> {
                    return exists ? this->isEnable(source) : false;
                })
                .transform([this, &source, &event](bool enable) -> void {
                    if (!enable)
                        event.cancel();

                    this->mImpl->mThrottle([enable, &source]() -> void {
                        if (enable)
                            return;

                        LanguagePlugin::getShared()->getLanguage(source)
                            .transform([&source](const std::string& language) -> void {
                                source.sendMessage(tr(language, "pvp.off"));
                            })
                            .or_else(modules::defaultErrorHandler<PvpPlugin>);
                    });
                })
                .or_else(modules::defaultErrorHandler<PvpPlugin>);
        });
    }

    void PvpPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerHurtEventListener);
    }

    ll::Expected<void> PvpPlugin::enable(Player& player, bool value) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(PvpPluginErrorCode::Invalid));
        
        return this->mImpl->db->set("Pvp", player.getUuid().asString(), "enable", (value ? "true" : "false"))
            .transform([this, value, &player]() -> void {
                if (value) {
                    this->getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "pvp.log1"), player));

                    return;
                }
                
                this->getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "pvp.log2"), player));
            });
    }

    ll::Expected<bool> PvpPlugin::isEnable(Player& player) {
        if (!this->isValid())
            return ll::makeErrorCodeError(makeErrorCode(PvpPluginErrorCode::Invalid));

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->PvpCache.contains(mObject)) 
            return *this->mImpl->PvpCache.get(mObject).value();

        return this->mImpl->db->get("Pvp", mObject, "enable", "false")
            .transform([this, mObject](const std::string& value) -> bool {
                bool result = (value == "true");

                this->mImpl->PvpCache.put(mObject, result);
                return result;
            });
    }

    bool PvpPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    std::string PvpPlugin::getName() {
        return "PvpPlugin";
    }

    modules::ModulePriority PvpPlugin::getPriority() {
        return modules::ModulePriority::High;
    }

    ll::Expected<bool> PvpPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Pvp.ModuleEnabled)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.Plugins.Pvp;

        return true;
    }

    ll::Expected<bool> PvpPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    ll::Expected<bool> PvpPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        return this->mImpl->db->create("Pvp", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("enable");
        }).transform([this]() -> bool {
            this->registeryCommand();
            this->listenEvent();

            this->mImpl->mRegistered.store(true, std::memory_order_release);

            return true;
        });
    }

    ll::Expected<bool> PvpPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}
