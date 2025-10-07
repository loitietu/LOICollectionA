#include <map>
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
#include <ll/api/event/player/PlayerDisconnectEvent.h>

#include <mc/world/level/Level.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/ServerEvents/PlayerHurtEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPLugin.h"

#include "LOICollectionA/include/Plugins/PvpPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct PvpPlugin::Impl {
        std::map<std::string, std::string> mPlayerPvpLists;

        bool ModuleEnabled = false;

        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::event::ListenerPtr PlayerDisconnectEventListener;
        ll::event::ListenerPtr PlayerHurtEventListener;
    };

    PvpPlugin::PvpPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    PvpPlugin::~PvpPlugin() = default;

    ll::io::Logger* PvpPlugin::getLogger() {
        return this->mImpl->logger.get();
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
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("pvp", tr({}, "commands.pvp.description"), CommandPermissionLevel::Any);
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
        command.overload().text("off").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);

            this->enable(player, false);

            output.success(tr({}, "commands.pvp.success.disable"));
        });
        command.overload().text("on").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->enable(player, true);

            output.success(tr({}, "commands.pvp.success.enable"));
        });
    }

    void PvpPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([this](ll::event::PlayerJoinEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;

            std::string mObject = event.self().getUuid().asString();
            std::replace(mObject.begin(), mObject.end(), '-', '_');
            
            if (!this->mImpl->db->has("OBJECT$" + mObject, "Pvp_Enable"))
                this->mImpl->db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
        });
        this->mImpl->PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer())
                return;
            
            this->mImpl->mPlayerPvpLists.erase(event.self().getUuid().asString());
        });
        this->mImpl->PlayerHurtEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerHurtEvent>([this](LOICollection::ServerEvents::PlayerHurtEvent& event) mutable -> void {
            if (!event.getSource().isPlayer() || event.getSource().isSimulatedPlayer() || event.self().isSimulatedPlayer())
                return;
            auto& source = static_cast<Player&>(event.getSource());

            if (!this->isEnable(event.self()) || !this->isEnable(source)) {
                if (!this->mImpl->mPlayerPvpLists.contains(source.getUuid().asString()) || this->mImpl->mPlayerPvpLists[source.getUuid().asString()] != SystemUtils::getNowTime("%Y%m%d%H%M%S"))
                    source.sendMessage(tr(LanguagePlugin::getInstance().getLanguage(source), "pvp.off"));
                event.cancel();
            }

            this->mImpl->mPlayerPvpLists[source.getUuid().asString()] = SystemUtils::getNowTime("%Y%m%d%H%M%S");
        });
    }

    void PvpPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerJoinEventListener);
        eventBus.removeListener(this->mImpl->PlayerDisconnectEventListener);
        eventBus.removeListener(this->mImpl->PlayerHurtEventListener);
    }

    void PvpPlugin::enable(Player& player, bool value) {
        if (!this->isValid())
            return;
        
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');

        if (value) {
            if (this->mImpl->db->has("OBJECT$" + mObject))
                this->mImpl->db->set("OBJECT$" + mObject, "Pvp_Enable", "true");
            this->getLogger()->info(LOICollectionAPI::getVariableString(tr({}, "pvp.log1"), player));
            return;
        }

        if (this->mImpl->db->has("OBJECT$" + mObject))
            this->mImpl->db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
        this->getLogger()->info(LOICollectionAPI::getVariableString(tr({}, "pvp.log2"), player));
    }

    bool PvpPlugin::isEnable(Player& player) {
        if (!this->isValid())
            return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (this->mImpl->db->has("OBJECT$" + mObject))
            return this->mImpl->db->get("OBJECT$" + mObject, "Pvp_Enable") == "true";
        return false;
    }

    bool PvpPlugin::isValid() {
        return this->getLogger() != nullptr && this->mImpl->db != nullptr;
    }

    bool PvpPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Pvp)
            return false;

        this->mImpl->db = ServiceProvider::getInstance().getService<SQLiteStorage>("SettingsDB");
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool PvpPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;
        
        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool PvpPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        return true;
    }
}

REGISTRY_HELPER("PvpPlugin", LOICollection::Plugins::PvpPlugin, LOICollection::Plugins::PvpPlugin::getInstance())
