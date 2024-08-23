#include <memory>
#include <optional>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/data/KeyValueDB.h>
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

#include "../Include/language.h"

namespace languagePlugin {
    namespace {
        using I18nUtils::tr;
        using I18nUtils::keys;
        std::unique_ptr<ll::data::KeyValueDB> db;
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::Logger logger("LOICollectionA - language");

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
                ll::form::CustomForm form(tr(getLanguage(player), "language.gui.title"));
                form.appendLabel(tr(getLanguage(player), "language.gui.label"));
                form.appendLabel(tr(getLanguage(player), "language.gui.lang"));
                form.appendDropdown("dropdown", tr(getLanguage(player), "language.gui.dropdown"), keys());
                form.sendTo((*player), [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                    if (!dt) {
                        pl.sendMessage(tr(getLanguage(&pl), "exit"));
                        return;
                    }
                    auto playerUuid = pl.getUuid().asString();
                    auto dropdownValue = std::get<std::string>(dt->at("dropdown"));
                    db->set(playerUuid, dropdownValue);
                    
                    std::string logString = tr(getLanguage(&pl), "language.log");
                    logger.info(LOICollectionAPI::translateString(logString, &pl, true));
                });
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [&](ll::event::PlayerJoinEvent& event) {
                    auto playerUuid = event.self().getUuid().asString();
                    if (!db->has(playerUuid)) {
                        db->set(playerUuid, "zh_CN");
                    }
                }
            );
        }
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<ll::data::KeyValueDB>*>(database));
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }

    std::string getLanguage(void* player) {
        Player* Player = static_cast<class Player*>(player);
        std::optional<std::string> lang = db->get(Player->getUuid().asString());
        if (lang) return lang.value();
        return "zh_CN";
    }
}