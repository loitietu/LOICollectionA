#include <memory>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/utils/StringUtils.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "Include/APIUtils.h"

#include "Utils/I18nUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/languagePlugin.h"

using I18nUtils::tr;
using I18nUtils::keys;
using I18nUtils::getName;
using I18nUtils::getLocalFromName;

namespace LOICollection::Plugins::language {
    std::shared_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - language");

    namespace MainGui {
        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
            form.appendLabel(ll::string_utils::replaceAll(tr(mObjectLanguage, "language.gui.lang"), "${language}", getName(mObjectLanguage)));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys());
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db->set("OBJECT$" + mObject, "language", getLocalFromName(std::get<std::string>(dt->at("dropdown"))));
                
                logger.info(LOICollection::LOICollectionAPI::translateString(tr(getLanguage(&pl), "language.log"), &pl));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("language", "§e§lLOICollection -> §a语言设置", CommandPermissionLevel::Any);
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                MainGui::open(player);
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
                    if (!db->has("OBJECT$" + mObject))
                        db->create("OBJECT$" + mObject);
                    if (!db->has("OBJECT$" + mObject, "language"))
                        db->set("OBJECT$" + mObject, "language", "zh_CN");
                }
            );
        }
    }

    std::string getLanguage(void* player_ptr) {
        if (player_ptr == nullptr)
            return "zh_CN";
        Player* player = static_cast<class Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->get("OBJECT$" + mObject, "language");
    }

    void registery(void* database) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(database);
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}