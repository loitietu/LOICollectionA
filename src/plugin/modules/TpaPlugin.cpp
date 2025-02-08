#include <memory>
#include <vector>
#include <string>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
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

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "utils/McUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/TpaPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::tpa {
    enum SelectorType : int {
        tpa = 0,
        tphere = 1
    };
    struct TpaOP {
        CommandSelector<Player> target;
        SelectorType type;
    };

    std::shared_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    
    ll::event::ListenerPtr PlayerJoinEventListener;

    namespace MainGui {
        void setting(Player& player) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.setting.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.setting.label"));
            form.appendToggle("Toggle1", tr(mObjectLanguage, "tpa.gui.setting.switch1"), isInvite(player));
            form.sendTo(player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return;

                std::string mObject = pl.getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                db->set("OBJECT$" + mObject, "Tpa_Toggle1",
                    std::get<uint64>(dt->at("Toggle1")) ? "true" : "false"
                );
            });
        }

        void tpa(Player& player, Player& target, TpaType type) {
            std::string mObjectLanguage = getLanguage(target);

            if (isInvite(target)) {
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"), "${player}", target.getRealName()
                ));
                return;
            }

            ll::form::ModalForm form(tr(mObjectLanguage, "tpa.gui.title"), (type == TpaType::tpa)
                ? LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.there"), player)
                : LOICollection::LOICollectionAPI::translateString(tr(mObjectLanguage, "tpa.here"), player),
                tr(mObjectLanguage, "tpa.yes"), tr(mObjectLanguage, "tpa.no"));
            form.sendTo(target, [type, &player](Player& pl, ll::form::ModalFormResult result, ll::form::FormCancelReason) -> void {
                if (result == ll::form::ModalFormSelectedButton::Upper) {
                    std::string logString = tr({}, "tpa.log");
                    if (type == TpaType::tpa) {
                        player.teleport(pl.getPosition(), pl.getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", pl.getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", player.getRealName());
                    } else {
                        pl.teleport(player.getPosition(), player.getDimensionId());
                        ll::string_utils::replaceAll(logString, "${player1}", player.getRealName());
                        ll::string_utils::replaceAll(logString, "${player2}", pl.getRealName());
                    }
                    logger->info(logString);
                    return;
                }
                player.sendMessage(ll::string_utils::replaceAll(
                    tr(getLanguage(player), "tpa.no.tips"), "${player}", pl.getRealName()
                ));
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);
            
            ll::form::CustomForm form(tr(mObjectLanguage, "tpa.gui.title"));
            form.appendLabel(tr(mObjectLanguage, "tpa.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "tpa.gui.dropdown"), { "tpa", "tphere" });
            form.sendTo(player, [&target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::open(pl);

                if (std::get<std::string>(dt->at("dropdown")) == "tpa")
                    return MainGui::tpa(pl, target, TpaType::tpa);
                MainGui::tpa(pl, target, TpaType::tphere);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "tpa.gui.title"), tr(mObjectLanguage, "tpa.gui.label2"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("tpa", tr({}, "commands.tpa.description"), CommandPermissionLevel::Any);
            command.overload<TpaOP>().text("invite").required("type").required("target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, TpaOP const& param) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);

                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    MainGui::tpa(player, *pl, param.type == SelectorType::tpa
                        ? TpaType::tpa : TpaType::tphere
                    );

                    output.success(fmt::runtime(tr({}, "commands.tpa.success.invite")), pl->getRealName());
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::open(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
            });
            command.overload().text("setting").execute([](CommandOrigin const& origin, CommandOutput& output) -> void {
                Actor* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error(tr({}, "commands.generic.target"));
                Player& player = *static_cast<Player*>(entity);
                MainGui::setting(player);

                output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
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
                    if (!db->has("OBJECT$" + mObject, "Tpa_Toggle1"))
                        db->set("OBJECT$" + mObject, "Tpa_Toggle1", "false");
                }
            );
        }
    }

    bool isInvite(Player& player) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject))
            return db->get("OBJECT$" + mObject, "Tpa_Toggle1") == "true";
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
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
    }
}