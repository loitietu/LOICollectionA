#include <memory>
#include <string>

#include <fmt/core.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerChatEvent.h>
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
#include "include/languagePlugin.h"
#include "include/HookPlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/mutePlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::mute {
    struct MuteOP {
        CommandSelector<Player> target;
        std::string cause;
        int time = 0;
    };

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;
    ll::event::ListenerPtr PlayerChatEventListener;

    namespace MainGui {
        void info(Player& player, std::string target) {
            std::string mObjectLanguage = getLanguage(player);
            
            std::string mObjectLabel = tr(mObjectLanguage, "mute.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("OBJECT$" + target, "player", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", db->get("OBJECT$" + target, "cause", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${subtime}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "subtime", "None")));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "time", "None")));

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "mute.gui.info.remove"), [target](Player& /*unused*/) -> void {
                delMute(target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::remove(pl);
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), "", "0");
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::add(pl);

                addMute(target, std::get<std::string>(dt->at("Input1")), 
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.add.title"), tr(mObjectLanguage, "mute.gui.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), tr(mObjectLanguage, "mute.gui.remove.label"));
            for (std::string& mItem : db->list()) {
                ll::string_utils::replaceAll(mItem, "OBJECT$", "");
                form.appendButton(mItem, [mItem](Player& pl) -> void {
                    MainGui::info(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.title"), tr(mObjectLanguage, "mute.gui.label"));
            form.appendButton(tr(mObjectLanguage, "mute.gui.addMute"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "mute.gui.removeMute"), "textures/ui/free_download_symbol", "path", [](Player& pl) -> void {
                MainGui::remove(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("mute", tr({}, "commands.mute.description"), CommandPermissionLevel::GameDirectors);
            command.overload<MuteOP>().text("add").required("target").optional("cause").optional("time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) -> void {
                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    if (isMute(*pl) || (int) pl->getPlayerPermissionLevel() >= 2 || pl->isSimulatedPlayer()) {
                        output.error(fmt::runtime(tr({}, "commands.mute.error.add")), pl->getRealName());
                        continue;
                    }
                    addMute(*pl, param.cause, param.time);

                    output.success(fmt::runtime(tr({}, "commands.mute.success.add")), pl->getRealName());
                }
            });
            command.overload<MuteOP>().text("remove").required("target").execute(
                [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) -> void {
                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));
                
                for (Player*& pl : results) {
                    if (!isMute(*pl)) {
                        output.error(fmt::runtime(tr({}, "commands.mute.error.remove")), pl->getRealName());
                        continue;
                    }
                    delMute(*pl);

                    output.success(fmt::runtime(tr({}, "commands.mute.success.remove")), pl->getRealName());
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
        }

        void listenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) -> void {
                    std::string mObject = event.self().getUuid().asString();
                    std::replace(mObject.begin(), mObject.end(), '-', '_');
                    if (db->has("OBJECT$" + mObject)) {
                        if (SystemUtils::isReach(db->get("OBJECT$" + mObject, "time"))) {
                            delMute(event.self());
                            return;
                        }

                        std::string mObjectTips = tr(getLanguage(event.self()), "mute.tips");
                        ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("OBJECT$" + mObject, "cause"));
                        ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObject, "time")));
                        event.self().sendMessage(mObjectTips);
                        
                        return event.cancel();
                    }
                }, ll::event::EventPriority::Highest
            );
        }
    }

    void addMute(Player& player, std::string cause, int time) {
        if ((int) player.getPlayerPermissionLevel() >= 2 || !isValid())
            return;

        cause = cause.empty() ? "None" : cause;
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "player", player.getRealName());
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0");
            db->set("OBJECT$" + mObject, "subtime", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        }
        logger->info(LOICollection::LOICollectionAPI::translateString(
            ll::string_utils::replaceAll(tr({}, "mute.log1"), "${cause}", cause), player)
        );
    }

    void delMute(Player& player) {
        if (!isValid()) return;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        delMute(mObject);
    }

    void delMute(std::string target) {
        if (!isValid()) return;

        if (db->has("OBJECT$" + target))
            db->remove("OBJECT$" + target);
        logger->info(ll::string_utils::replaceAll(tr({}, "mute.log2"), "${target}", target));
    }

    bool isMute(Player& player) {
        if (!isValid()) return false;

        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->has("OBJECT$" + mObject);
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }
    
    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerChatEventListener);
    }
}