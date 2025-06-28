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

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ServerEvents/PlayerHurtEvent.h"

#include "utils/I18nUtils.h"
#include "utils/SystemUtils.h"

#include "data/SQLiteStorage.h"

#include "include/PvpPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

std::map<std::string, std::string> mPlayerPvpLists;

namespace LOICollection::Plugins::pvp {
    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerDisconnectEventListener;
    ll::event::ListenerPtr PlayerHurtEventListener;

    namespace MainGui {
        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "pvp.gui.title"), tr(mObjectLanguage, "pvp.gui.label"));
            form.appendButton(tr(mObjectLanguage, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) -> void {
                enable(pl, true);
            });
            form.appendButton(tr(mObjectLanguage, "pvp.gui.off"), "textures/ui/cancel", "path", [](Player& pl) -> void {
                enable(pl, false);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("pvp", tr({}, "commands.pvp.description"), CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("off").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                enable(player, false);

                output.success(tr({}, "commands.pvp.success.disable"));
            });
            command.overload().text("on").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                enable(player, true);

                output.success(tr({}, "commands.pvp.success.enable"));
            });
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db->has("OBJECT$" + mObject)) db->create("OBJECT$" + mObject);
                    if (!db->has("OBJECT$" + mObject, "Pvp_Enable"))
                        db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
                }
            );
            PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
                [](ll::event::PlayerDisconnectEvent& event) -> void {
                    if (event.self().isSimulatedPlayer())
                        return;
                    mPlayerPvpLists.erase(event.self().getUuid().asString());
                }
            );
            PlayerHurtEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerHurtEvent>(
                [](LOICollection::ServerEvents::PlayerHurtEvent& event) -> void {
                    if (!event.getSource().isPlayer() || event.getSource().isSimulatedPlayer() || event.self().isSimulatedPlayer())
                        return;
                    auto& source = static_cast<Player&>(event.getSource());

                    if (!isEnable(event.self()) || !isEnable(source)) {
                        if (!mPlayerPvpLists.contains(source.getUuid().asString()) || mPlayerPvpLists[source.getUuid().asString()] != SystemUtils::getNowTime("%Y%m%d%H%M%S"))
                            source.sendMessage(tr(getLanguage(source), "pvp.off"));
                        event.cancel();
                    }

                    mPlayerPvpLists[source.getUuid().asString()] = SystemUtils::getNowTime("%Y%m%d%H%M%S");
                }
            );
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerJoinEventListener);
            eventBus.removeListener(PlayerDisconnectEventListener);
            eventBus.removeListener(PlayerHurtEventListener);
        }
    }

    void enable(Player& player, bool value) {
        if (!isValid()) return;
        
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (value) {
            if (db->has("OBJECT$" + mObject))
                db->set("OBJECT$" + mObject, "Pvp_Enable", "true");
            logger->info(LOICollectionAPI::translateString(tr({}, "pvp.log1"), player));
            return;
        }
        if (db->has("OBJECT$" + mObject))
            db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
        logger->info(LOICollectionAPI::translateString(tr({}, "pvp.log2"), player));
    }

    bool isEnable(Player& player) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "Pvp_Enable") == "true";
        return false;
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(database);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}