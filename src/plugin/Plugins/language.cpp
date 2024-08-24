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

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandPermissionLevel.h>

#include "../Include/API.hpp"
#include "../Utils/I18nUtils.h"
#include "../Utils/toolUtils.h"
#include "../Utils/SQLiteStorage.h"

#include "../Include/language.h"

using I18nUtils::tr;
using I18nUtils::keys;

namespace languagePlugin {
    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - language");

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("language", "§e§lLOICollection -> §a语言设置", CommandPermissionLevel::Any);
            command.overload().text("setting").execute([&](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("LOICollection >> No player selected.");
                    return;
                }
                auto* player = static_cast<Player*>(entity);
                std::string mObjectLanguage = getLanguage(player);
                std::string lang = tr(mObjectLanguage, "language.gui.lang");

                ll::form::CustomForm form(tr(mObjectLanguage, "language.gui.title"));
                form.appendLabel(tr(mObjectLanguage, "language.gui.label"));
                form.appendLabel(toolUtils::replaceString(lang, "${language}", mObjectLanguage));
                form.appendDropdown("dropdown", tr(mObjectLanguage, "language.gui.dropdown"), keys());
                form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                    if (!dt) {
                        pl.sendMessage(tr(getLanguage(&pl), "exit"));
                        return;
                    }
                    std::string mObjectUuid = pl.getUuid().asString();
                    std::string dropdownValue = std::get<std::string>(dt->at("dropdown"));
                    std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
                    db->set("OBJECT$" + mObjectUuid, "language", dropdownValue);
                    std::string logString = tr(getLanguage(&pl), "language.log");
                    logger.info(LOICollectionAPI::translateString(logString, &pl, true));
                });
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [&](ll::event::PlayerJoinEvent& event) {
                    std::string mObjectUuid = event.self().getUuid().asString();
                    std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
                    if (!db->has("OBJECT$" + mObjectUuid)) {
                        db->create("OBJECT$" + mObjectUuid);
                        db->set("OBJECT$" + mObjectUuid, "language", "zh_CN");
                    }
                }
            );
        }
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }

    std::string getLanguage(void* player_ptr) {
        Player* player = static_cast<class Player*>(player_ptr);
        std::string mObjectUuid = player->getUuid().asString();
        std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
        return db->get("OBJECT$" + mObjectUuid, "language");
    }
}