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

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include "Include/APIUtils.h"
#include "Include/languagePlugin.h"
#include "Include/HookPlugin.h"

#include "Utils/I18nUtils.h"
#include "Utils/toolUtils.h"
#include "Utils/SQLiteStorage.h"

#include "Include/mutePlugin.h"

using I18nUtils::tr;
using languagePlugin::getLanguage;

namespace mutePlugin {
    struct MuteOP {
        CommandSelector<Player> target;
        std::string cause;
        int time = -1;
    };

    std::unique_ptr<SQLiteStorage> db;
    ll::Logger logger("LOICollectionA - Mute");

    namespace MainGui {
        void add(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "mute.gui.add.dropdown"), toolUtils::getAllPlayerName());
            form.appendInput("Input1", tr(mObjectLanguage, "mute.gui.add.input1"), "", tr(mObjectLanguage, "mute.cause"));
            form.appendInput("Input2", tr(mObjectLanguage, "mute.gui.add.input2"), "", "0");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string PlayerSelectName = std::get<std::string>(dt->at("dropdown"));
                std::string PlayerInputCause = std::get<std::string>(dt->at("Input1"));
                int time = toolUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                addMute(toolUtils::getPlayerFromName(PlayerSelectName), PlayerInputCause, time);
            });
        }

        void remove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::CustomForm form(tr(mObjectLanguage, "mute.gui.remove.title"));
            form.appendLabel(tr(mObjectLanguage, "mute.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "mute.gui.remove.dropdown"), toolUtils::getAllPlayerName());
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string PlayerSelectName = std::get<std::string>(dt->at("dropdown"));
                delMute(toolUtils::getPlayerFromName(PlayerSelectName));
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
                if (!isMute(pl)) {
                    output.addMessage(fmt::format("Add player {}({}) to mute.", 
                        pl->getRealName(), pl->getUuid().asString()), 
                        {}, CommandOutputMessageType::Success);
                    addMute(pl, param.cause, param.time);
                }
            }
        };

        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("mute", "§e§lLOICollection -> §b服务器禁言", CommandPermissionLevel::Admin);
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
            HookPlugin::Event::onTextPacketSendEvent([](void* player_ptr, std::string message) {
                Player* player = static_cast<Player*>(player_ptr);
                std::string mObject = player->getUuid().asString();
                std::replace(mObject.begin(), mObject.end(), '-', '_');
                if (db->has("OBJECT$" + mObject)) {
                    if (toolUtils::isReach(db->get("OBJECT$" + mObject, "time"))) {
                        delMute(player);
                        return false;
                    }
                    std::string cause = db->get("OBJECT$" + mObject, "cause");
                    std::string logString = tr(getLanguage(player), "mute.log3");
                    toolUtils::replaceString2(logString, "${message}", message);
                    logger.info(LOICollectionAPI::translateString(logString, player, true));
                    player->sendMessage(cause);
                    return true;
                }
                return false;
            });
        }
    }

    void addMute(void* player_ptr, std::string cause, int time) {
        Player* player = static_cast<Player*>(player_ptr);
        if (cause.empty()) cause = tr(getLanguage(player), "mute.cause");
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", toolUtils::timeCalculate(time));
        }
        std::string logString = tr(getLanguage(player), "mute.log1");
        toolUtils::replaceString2(logString, "${cause}", cause);
        logger.info(LOICollectionAPI::translateString(logString, player, true));
    }

    void delMute(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObject = player->getUuid().asString();
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (db->has("OBJECT$" + mObject) && mObject != "None") {
            db->remove("OBJECT$" + mObject);
        }
        std::string logString = tr(getLanguage(player), "mute.log2");
        logger.info(LOICollectionAPI::translateString(logString, player, true));
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