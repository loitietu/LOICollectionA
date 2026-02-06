#include <atomic>
#include <memory>
#include <string>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/SimpleForm.h>
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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/ServerEvents/player/PlayerHurtEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Cache.h"
#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/Throttle.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPLugin.h"

#include "LOICollectionA/include/Plugins/PvpPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
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

    PvpPlugin::PvpPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    PvpPlugin::~PvpPlugin() = default;

    PvpPlugin& PvpPlugin::getInstance() {
        static PvpPlugin instance;
        return instance;
    }

    std::shared_ptr<ll::io::Logger> PvpPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void PvpPlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);
        
        ll::form::SimpleForm form(tr(mObjectLanguage, "pvp.gui.title"), tr(mObjectLanguage, "pvp.gui.label"));
        form.appendButton(tr(mObjectLanguage, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [this](Player& pl) -> void {
            this->mParent.enable(pl, true);
        });
        form.appendButton(tr(mObjectLanguage, "pvp.gui.off"), "textures/ui/cancel", "path", [this](Player& pl) -> void {
            this->mParent.enable(pl, false);
        });
        form.sendTo(player);
    }

    void PvpPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("pvp", tr({}, "commands.pvp.description"), CommandPermissionLevel::Any, CommandFlagValue::NotCheat | CommandFlagValue::Async);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr(origin.getLocaleCode(), "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("off").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->enable(player, false);

            output.success(tr(origin.getLocaleCode(), "commands.pvp.success.disable"));
        });
        command.overload().text("on").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr(origin.getLocaleCode(), "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->enable(player, true);

            output.success(tr(origin.getLocaleCode(), "commands.pvp.success.enable"));
        });
    }

    void PvpPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();

            if (!this->mImpl->db->has("Pvp", mObject)) {
                std::unordered_map<std::string, std::string> mData = {
                    { "name", mObject },
                    { "enable", "false" }
                };

                this->mImpl->db->set("Pvp", mObject, mData);
            }
        });
        this->mImpl->PlayerHurtEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerHurtEvent>([this](LOICollection::ServerEvents::PlayerHurtEvent& event) mutable -> void {
            if (!event.getSource().isPlayer() || event.getSource().isSimulatedPlayer() || event.self().isSimulatedPlayer())
                return;

            switch (event.getReason()) {
                case LOICollection::ServerEvents::PlayerHurtReason::Hurt: if (!this->mImpl->options.ExtraListener.onActorHurt) return;
                case LOICollection::ServerEvents::PlayerHurtReason::Effect: if (!this->mImpl->options.ExtraListener.onSplashPotion) return;
                case LOICollection::ServerEvents::PlayerHurtReason::Projectile: if (!this->mImpl->options.ExtraListener.onProjectileHit) return;
            }

            auto& source = static_cast<Player&>(event.getSource());

            bool isPvp = this->isEnable(event.self()) && this->isEnable(source);
            if (!isPvp)
                event.cancel();

            this->mImpl->mThrottle([isPvp, &source]() -> void {
                if (isPvp)
                    return;

                source.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(source), "pvp.off"));
            });
        });
    }

    void PvpPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerHurtEventListener);
    }

    void PvpPlugin::enable(Player& player, bool value) {
        if (!this->isValid())
            return;
        
        this->mImpl->db->set("Pvp", player.getUuid().asString(), "enable", (value ? "true" : "false"));

        if (value) {
            this->getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "pvp.log1"), player));

            return;
        }
        
        this->getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "pvp.log2"), player));
    }

    bool PvpPlugin::isEnable(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();

        if (this->mImpl->PvpCache.contains(mObject)) 
            return *this->mImpl->PvpCache.get(mObject).value();

        bool result = this->mImpl->db->get("Pvp", mObject, "enable", "false") == "true";

        this->mImpl->PvpCache.put(mObject, result);
        return result;
    }

    bool PvpPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    bool PvpPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Pvp.ModuleEnabled)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Pvp;

        return true;
    }

    bool PvpPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool PvpPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->mImpl->db->create("Pvp", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("enable");
        });

        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool PvpPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("PvpPlugin", LOICollection::Plugins::PvpPlugin, LOICollection::Plugins::PvpPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
