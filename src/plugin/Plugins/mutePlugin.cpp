#include <memory>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>

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

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::mute {
    struct MuteOP {
        CommandSelector<Player> target;
        std::string cause;
        int time = -1;
    };

    std::unique_ptr<SQLiteStorage> db;
    ll::Logger logger("LOICollectionA - Mute");

    namespace MainGui {
        void info(void* player_ptr, std::string target) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            
            std::string mObjectLabel = tr(mObjectLanguage, "mute.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", db->get("OBJECT$" + target, "cause"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "time")));

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "mute.gui.info.remove"), [target](Player& pl) {
                delMute(target);

                McUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::remove(player);
                });
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::remove(&pl);
            });
        }

        void content(void* player_ptr, std::string target) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), "", tr(mObjectLanguage, "mute.cause"));
            form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), "", "0");
            form.sendTo(*player, [target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    MainGui::add(&pl);
                    return;
                }
                std::string PlayerInputCause = std::get<std::string>(dt->at("Input1"));
                int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                addMute(McUtils::getPlayerFromName(target), PlayerInputCause, time);
                
                McUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::add(player);
                });
            });
        }

        void add(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.add.title"), tr(mObjectLanguage, "mute.gui.add.label"));
            for (auto& mTarget : McUtils::getAllPlayerName()) {
                form.appendButton(mTarget, [mTarget](Player& pl) {
                    MainGui::content(&pl, mTarget);
                });
            }
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(&pl);
            });
        }

        void remove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.remove.title"), tr(mObjectLanguage, "mute.gui.remove.label"));
            for (auto& mItem : db->list()) {
                ll::string_utils::replaceAll(mItem, "OBJECT$", "");
                form.appendButton(mItem, [mItem](Player& pl) {
                    MainGui::info(&pl, mItem);
                });
            }
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) MainGui::open(&pl);
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "mute.gui.title"), tr(mObjectLanguage, "mute.gui.label"));
            form.appendButton(tr(mObjectLanguage, "mute.gui.addMute"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::add(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "mute.gui.removeMute"), "textures/ui/free_download_symbol", "path", [](Player& pl) {
                MainGui::remove(&pl);
            });
            form.sendTo(*player, [](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace {
        const auto MuteCommandADD = [](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) {
            for (auto& pl : param.target.results(origin)) {
                if (!isMute(pl) && (int) pl->getPlayerPermissionLevel() < 2 && !pl->isSimulatedPlayer()) {
                    output.addMessage(fmt::format("Add player {}({}) to mute.", 
                        pl->getRealName(), pl->getUuid().asString()), 
                        {}, CommandOutputMessageType::Success);
                    addMute(pl, param.cause, param.time);
                }
            }
        };

        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("mute", "§e§lLOICollection -> §b服务器禁言", CommandPermissionLevel::GameDirectors);
            command.overload<MuteOP>().text("add").required("target").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("time").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("cause").execute(MuteCommandADD);
            command.overload<MuteOP>().text("add").required("target").required("cause").required("time").execute(MuteCommandADD);
            command.overload<MuteOP>().text("remove").required("target").execute([](CommandOrigin const& origin, CommandOutput& output, MuteOP const& param) {
                for (auto& pl : param.target.results(origin)) {
                    if (isMute(pl)) {
                        output.addMessage(fmt::format("Remove player {}({}) form mute.", 
                            pl->getRealName(), pl->getUuid().asString()), 
                            {}, CommandOutputMessageType::Success);
                        delMute(pl);
                    }
                }
            });
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
            LOICollection::HookPlugin::Event::onTextPacketSendEvent([](void* player_ptr, std::string message) {
                Player* player = static_cast<Player*>(player_ptr);
                std::string mObject = player->getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                if (db->has("OBJECT$" + mObject)) {
                    if (SystemUtils::isReach(db->get("OBJECT$" + mObject, "time"))) {
                        delMute(player);
                        return false;
                    }
                    std::string mObjectTips = tr(getLanguage(player), "mute.tips");
                    std::string logString = tr(getLanguage(player), "mute.log3");

                    ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("OBJECT$" + mObject, "cause"));
                    ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObject, "time")));
                    ll::string_utils::replaceAll(logString, "${message}", message);

                    logger.info(LOICollection::LOICollectionAPI::translateString(logString, player));
                    player->sendMessage(mObjectTips);
                    return true;
                }
                return false;
            });
        }
    }

    void addMute(void* player_ptr, std::string cause, int time) {
        Player* player = static_cast<Player*>(player_ptr);

        if ((int) player->getPlayerPermissionLevel() >= 2)
            return;

        cause = cause.empty() ? "None" : cause;
        std::string mObjectLanguage = getLanguage(player);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", SystemUtils::timeCalculate(SystemUtils::getNowTime(), time));
        }
        std::string logString = tr(mObjectLanguage, "mute.log1");
        ll::string_utils::replaceAll(logString, "${cause}", cause);
        logger.info(LOICollection::LOICollectionAPI::translateString(logString, player));
    }

    void delMute(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        delMute(mObject);
    }

    void delMute(std::string target) {
        if (db->has("OBJECT$" + target))
            db->remove("OBJECT$" + target);
        std::string logString = tr(getLanguage(nullptr), "mute.log2");
        logger.info(ll::string_utils::replaceAll(logString, "${target}", target));
    }

    bool isMute(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        return db->has("OBJECT$" + mObject);
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }
}