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
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "include/APIUtils.h"

#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "Include/languagePlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins::language {
    std::shared_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - language");

    namespace MainGui {
        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
            form.appendLabel(ll::string_utils::replaceAll(tr(mObjectLanguage, "language.gui.lang"), "${language}", tr(mObjectLanguage, "name")));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), I18nUtilsTools::keys());
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return;

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db->set("OBJECT$" + mObject, "language", std::get<std::string>(dt->at("dropdown")));
                
                logger.info(LOICollection::LOICollectionAPI::translateString(tr({}, "language.log"), pl));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("setting", "§e§lLOICollection -> §b个人设置", CommandPermissionLevel::Any);
            command.overload().text("language").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success("The UI has been opened to player {}", player.getRealName());
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
                    if (!db->has("OBJECT$" + mObject, "language"))
                        db->set("OBJECT$" + mObject, "language", "zh_CN");
                }
            );
        }
    }

    std::string getLanguage(std::string mObject) {
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->get("OBJECT$" + mObject, "language", "zh_CN");
    }

    std::string getLanguage(Player& player) {
        return getLanguage(player.getUuid().asString());
    }

    void registery(void* database) {
        db = *static_cast<std::shared_ptr<SQLiteStorage>*>(database);
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}