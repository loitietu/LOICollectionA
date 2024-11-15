#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/form/SimpleForm.h>
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
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"

#include "utils/McUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/tpaPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::tpa {
    struct TpaOP {
        enum SelectorType {
            tpa, tphere
        } selectorType;
        CommandSelector<Player> target;
    };

    std::shared_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::Logger logger("LOICollectionA - TPA");

    namespace MainGui {
        void setting(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.switch1"), isInvite(player));
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return pl.sendMessage(tr(getLanguage(&pl), "exit"));

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db->set("OBJECT$" + mObject, "Tpa_Toggle1",
                    std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
                );

                McUtils::Gui::submission(&pl, [](void* player_ptr) {
                    return MainGui::setting(player_ptr);
                });
            });
        }

        void tpa(void* player_ptr, void* target_ptr, int type) {
            Player* player = static_cast<Player*>(player_ptr);
            Player* target = static_cast<Player*>(target_ptr);
            std::string mObjectLanguage = getLanguage(target);

            if (isInvite(target_ptr)) {
                player->sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"),
                    "${player}", target->getRealName()
                ));
                return;
            }

            ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), (type == TPA_TYPE_TPA)
                ? LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.there"), player)
                : LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.here"), player),
                tr(mObjectLanguage, "tpa.yes"), tr(mObjectLanguage, "tpa.no"));
            form.sendTo(*target, [type, player](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string logString = tr({}, "tpa.log");
                    if (type == TPA_TYPE_TPA) {
                        player->teleport(pl.getPosition(), pl.getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", player->getRealName());
                    } else {
                        pl.teleport(player->getPosition(), player->getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", player->getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", pl.getRealName());
                    }
                    logger.info(logString);
                    return;
                }
                player->sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"),
                    "${player}", pl.getRealName()
                ));
            });
        }

        void content(void* player_ptr, std::string target) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "tpa.gui.dropdown"), { "tpa", "tphere" });
            form.sendTo(*player, [target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::open(&pl);

                Player* pl2 = static_cast<Player*>(McUtils::getPlayerFromName(target));
                if (std::get<std::string>(dt->at("dropdown")) == "tpa")
                    return MainGui::tpa(&pl, pl2, TPA_TYPE_TPA);
                MainGui::tpa(&pl, pl2, TPA_TYPE_HERE);

                McUtils::Gui::submission(&pl, [target](void* player_ptr) {
                    return MainGui::content(player_ptr, target);
                });
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.title"), tr(mObjectLanguage, "tpa.gui.label2"));
            for (auto& mTarget : McUtils::getAllPlayerName()) {
                form.appendButton(mTarget, [mTarget](Player& pl) {
                    MainGui::content(&pl, mTarget);
                });
            }
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace {
        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("tpa", "§e§lLOICollection -> §b玩家互传", CommandPermissionLevel::Any);
            command.overload<TpaOP>().text("invite").required("selectorType").required("target").execute([](CommandOrigin const& origin, CommandOutput& output, TpaOP const& param) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                for (auto& pl : param.target.results(origin)) {
                    output.addMessage(fmt::format("{} has been invited.", 
                        pl->getRealName()), {}, CommandOutputMessageType::Success);
                    MainGui::tpa(player, pl, param.selectorType == TpaOP::tpa
                        ? TPA_TYPE_TPA : TPA_TYPE_HERE
                    );
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player);
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::setting(player);
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
                    if (!db->has("OBJECT$" + mObject, "Tpa_Toggle1"))
                        db->set("OBJECT$" + mObject, "Tpa_Toggle1", "false");
                }
            );
        }
    }

    bool isInvite(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "Tpa_Toggle1") == "true";
        return false;
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