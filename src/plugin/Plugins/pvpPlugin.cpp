#include <memory>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerAttackEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/pvpPlugin.h"

using I18nUtils::tr;
using languagePlugin::getLanguage;

namespace pvpPlugin {
    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerAttackEventListener;
    ll::Logger logger("LOICollectionA - PVP");

    namespace MainGui {
        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "pvp.gui.title"), tr(mObjectLanguage, "pvp.gui.label"));
            form.appendButton(tr(mObjectLanguage, "pvp.gui.on"), "textures/ui/book_addtextpage_default", "path", [](Player& pl) {
                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                if (db->has("OBJECT$" + mObject)) {
                    db->set("OBJECT$" + mObject, "enable", "true");
                }
                logger.info(LOICollectionAPI::translateString(tr(getLanguage(&pl), "pvp.log1"), &pl, true));
            });
            form.appendButton(tr(mObjectLanguage, "pvp.gui.off"), "textures/ui/cancel", "path", [](Player& pl) {
                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                if (db->has("OBJECT$" + mObject)) {
                    db->set("OBJECT$" + mObject, "enable", "false");
                }
                logger.info(LOICollectionAPI::translateString(tr(getLanguage(&pl), "pvp.log2"), &pl, true));
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("pvp", "§e§lLOICollection -> §b服务器PVP", CommandPermissionLevel::Any);
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player);
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (!db->has("OBJECT$" + mObject)) {
                        db->create("OBJECT$" + mObject);
                        db->set("OBJECT$" + mObject, "enable", "false");
                    }
                }
            );
            PlayerAttackEventListener = eventBus.emplaceListener<ll::event::PlayerAttackEvent>(
                [](ll::event::PlayerAttackEvent& event) {
                    auto* entity = &event.target();
                    if (entity == nullptr || !entity->isType(ActorType::Player))
                        return;
                    Player* player = static_cast<Player*>(entity);

                    std::string mObjectLanguage = getLanguage(&event.self());
                    if (!isEnable(player)) {
                        event.self().sendMessage(tr(mObjectLanguage, "pvp.off1"));
                        event.cancel();
                    } else if (!isEnable(&event.self())) {
                        event.self().sendMessage(tr(mObjectLanguage, "pvp.off2"));
                        event.cancel();
                    }
                }
            );
        }
    }

    bool isEnable(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "enable") == "true";
        return false;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerAttackEventListener);
    }
}