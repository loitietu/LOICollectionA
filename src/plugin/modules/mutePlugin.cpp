#include <memory>
#include <string>

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
            form.appendButton(tr(mObjectLanguage, "mute.gui.info.remove"), [target](Player& /*unused*/) {
                delMute(target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::remove(pl);
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), "", "0");
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::add(pl);

                addMute(target, std::get<std::string>(dt->at("Input1")), 
                    SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.add.title"), tr(mObjectLanguage, "mute.gui.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), tr(mObjectLanguage, "mute.gui.remove.label"));
            for (auto& mItem : db->list()) {
                ll::string_utils::replaceAll(mItem, "OBJECT$", "");
                form.appendButton(mItem, [mItem](Player& pl) {
                    MainGui::info(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.title"), tr(mObjectLanguage, "mute.gui.label"));
            form.appendButton(tr(mObjectLanguage, "mute.gui.addMute"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "mute.gui.removeMute"), "textures/ui/free_download_symbol", "path", [](Player& pl) {
                MainGui::remove(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        const auto MuteCommandADD = [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) {
            for (auto& pl : param.target.results(origin)) {
                if (isMute(*pl) || (int) pl->getPlayerPermissionLevel() >= 2 || pl->isSimulatedPlayer())
                    continue;
                addMute(*pl, param.cause, param.time);

                output.addMessage(fmt::format("Add player {}({}) to mute.", pl->getRealName(),
                    pl->getUuid().asString()), {}, CommandOutputMessageType::Success);
            }
        };

        void registerCommand() {
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("mute", "§e§lLOICollection -> §b服务器禁言", CommandPermissionLevel::GameDirectors);
            command.overload<MuteOP>().text("add").required("target").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("time").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("cause").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("cause").required("time").execute(MuteCommandADD);
            command.overload<MuteOP>().text("remove").required("target").execute([](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) {
                for (auto& pl : param.target.results(origin)) {
                    if (!isMute(*pl)) 
                        continue;
                    delMute(*pl);

                    output.addMessage(fmt::format("Remove player {}({}) form mute.", pl->getRealName(),
                        pl->getUuid().asString()), {}, CommandOutputMessageType::Success);
                }
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
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
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [](ll::event::PlayerChatEvent& event) {
                    if (event.self().isSimulatedPlayer() || mute::isMute(event.self()))
                        return;
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

                        logger->info(LOICollection::LOICollectionAPI::translateString(ll::string_utils::replaceAll(
                            tr({}, "mute.log3"), "${message}", event.message()), event.self()));
                        return event.cancel();
                    }
                }, ll::event::EventPriority::High
            );
        }
    }

    void addMute(Player& player, std::string cause, int time) {
        if ((int) player.getPlayerPermissionLevel() >= 2)
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
        logger->info(LOICollection::LOICollectionAPI::translateString(ll::string_utils::replaceAll(
            tr({}, "mute.log1"), "${cause}", cause), player));
    }

    void delMute(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        delMute(mObject);
    }

    void delMute(std::string target) {
        if (db->has("OBJECT$" + target))
            db->remove("OBJECT$" + target);
        logger->info(ll::string_utils::replaceAll(tr({}, "mute.log2"), "${target}", target));
    }

    bool isMute(Player& player) {
        std::string mObject = player.getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->has("OBJECT$" + mObject);
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        registerCommand();
        listenEvent();
    }
    
    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerChatEventListener);
    }
}