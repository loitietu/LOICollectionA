#include <memory>
#include <string>

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

#include <mc/world/actor/Mob.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"
#include "include/HookPlugin.h"

#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/pvpPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::pvp {
    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::SimpleForm form(tr(mObjectLanguage, "pvp.gui.title"), tr(mObjectLanguage, "pvp.gui.label"));
            form.appendButton(tr(mObjectLanguage, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) {
                enable(pl, true);
            });
            form.appendButton(tr(mObjectLanguage, "pvp.gui.off"), "textures/ui/cancel", "path", [](Player& pl) {
                enable(pl, false);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("pvp", "§e§lLOICollection -> §b服务器PVP", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success("The UI has been opened to player {}", player.getRealName());
            });
            command.overload().text("off").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                enable(player, false);

                output.success("The PVP has been disabled");
            });
            command.overload().text("on").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                enable(player, true);

                output.success("The PVP has been enabled");
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db->has("OBJECT$" + mObject)) db->create("OBJECT$" + mObject);
                    if (!db->has("OBJECT$" + mObject, "Pvp_Enable"))
                        db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
                }
            );
            LOICollection::HookPlugin::Event::onPlayerHurtEvent([](Mob* target, Actor* source, float /*unused*/) {
                Player& targetPlayer = *static_cast<Player*>(target);
                Player& sourcePlayer = *static_cast<Player*>(source);

                if (!isEnable(targetPlayer) && !targetPlayer.isSimulatedPlayer()) {
                    sourcePlayer.sendMessage(tr(getLanguage(sourcePlayer), "pvp.off1"));
                    return true;
                } else if (!isEnable(sourcePlayer) && !sourcePlayer.isSimulatedPlayer()) {
                    sourcePlayer.sendMessage(tr(getLanguage(sourcePlayer), "pvp.off2"));
                    return true;
                }
                return false;
            });
        }
    }

    bool isEnable(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "Pvp_Enable") == "true";
        return false;
    }

    void enable(Player& player, bool value) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (value) {
            if (db->has("OBJECT$" + mObject))
                db->set("OBJECT$" + mObject, "Pvp_Enable", "true");
            logger->info(LOICollection::LOICollectionAPI::translateString(tr({}, "pvp.log1"), player));
            return;
        }
        if (db->has("OBJECT$" + mObject))
            db->set("OBJECT$" + mObject, "Pvp_Enable", "false");
        logger->info(LOICollection::LOICollectionAPI::translateString(tr({}, "pvp.log2"), player));
    }

    void registery(void* database) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(database);
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}