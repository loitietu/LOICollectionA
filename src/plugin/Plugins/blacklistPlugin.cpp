#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerConnectEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>
#include <mc/enums/connection/DisconnectFailReason.h>
#include <mc/network/ServerNetworkHandler.h>

#include "../Include/API.hpp"
#include "../Include/languagePlugin.h"

#include "../Utils/I18nUtils.h"
#include "../Utils/toolUtils.h"
#include "../Utils/SQLiteStorage.h"

#include "../Include/blacklistPlugin.h"

using I18nUtils::tr;
using languagePlugin::getLanguage;

namespace blacklistPlugin {
    struct BlacklistOP {
        enum SubCommand {
            list, gui
        } subCommand;
        enum SelectorType {
            ip, uuid
        } selectorType;
        CommandSelector<Player> target;
        std::string targetName;
        std::string cause;
        int time = -1;
    };

    std::unique_ptr<SQLiteStorage> db;
    ll::event::ListenerPtr PlayerConnectEventListener;
    ll::Logger logger("LOICollectionA - Blacklist");

    namespace MainGui {
        void add(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendDropdown("dropdown1", tr(mObjectLanguage, "blacklist.gui.add.dropdown1"), toolUtils::getAllPlayerName());
            form.appendDropdown("dropdown2", tr(mObjectLanguage, "blacklist.gui.add.dropdown2"), { "ip", "uuid" });
            form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), "", tr(mObjectLanguage, "blacklist.cause"));
            form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), "", "0");
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string PlayerSelectName = std::get<std::string>(dt->at("dropdown1"));
                std::string PlayerSelectType = std::get<std::string>(dt->at("dropdown2"));
                std::string PlayerInputCause = std::get<std::string>(dt->at("Input1"));
                int time = toolUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                Player* pl2 = toolUtils::getPlayerFromName(PlayerSelectName);
                if (PlayerSelectType == "ip") {
                    blacklistPlugin::addBlacklist(pl2, PlayerInputCause, time, 0);
                } else if (PlayerSelectType == "uuid") {
                    blacklistPlugin::addBlacklist(pl2, PlayerInputCause, time, 1);
                }
            });
        }

        void remove(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            std::vector<std::string> list = db->list2("OBJECT$");
            ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"));
            form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "blacklist.gui.remove.dropdown"), list);
            form.sendTo(*player, [](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) {
                    pl.sendMessage(tr(getLanguage(&pl), "exit"));
                    return;
                }
                std::string PlayerSelectString = std::get<std::string>(dt->at("dropdown"));
                blacklistPlugin::delBlacklist(PlayerSelectString);
            });
        }

        void open(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);
            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.title"), tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.addBlacklist"), "textures/ui/backup_replace", "path", [](Player& pl) {
                MainGui::add(&pl);
            });
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.removeBlacklist"), "textures/ui/free_download_symbol", "path", [](Player& pl) {
                MainGui::remove(&pl);
            });
            form.sendTo(*player, [&](Player& pl, int id, ll::form::FormCancelReason) {
                if (id == -1) pl.sendMessage(tr(getLanguage(&pl), "exit"));
            });
        }
    }

    namespace {
        const auto BlacklistCommandADD = [](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) {
            for (auto& pl : param.target.results(origin)) {
                if (!isBlacklist(pl)) {
                    output.addMessage(fmt::format("Add player {}({}) to blacklist.", 
                        pl->getRealName(), pl->getUuid().asString()), 
                        {}, CommandOutputMessageType::Success);
                    if (param.selectorType == BlacklistOP::ip) {
                        addBlacklist(pl, param.cause, param.time, 0);
                    } else if (param.selectorType == BlacklistOP::uuid) {
                        addBlacklist(pl, param.cause, param.time, 1);
                    }
                }
            }
        };

        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery) {
                throw std::runtime_error("Failed to get command registry.");
            }
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("blacklist", "§e§lLOICollection -> §b服务器黑名单", CommandPermissionLevel::Admin);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("time").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("cause").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("cause").required("time").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("remove").required("targetName").execute([](CommandOrigin const& /*unused*/, CommandOutput& output, BlacklistOP const& param) {
                if (!db->has("OBJECT$" + param.targetName)) {
                    output.error("Object {} is not in blacklist.", param.targetName);
                    return;
                }
                delBlacklist(param.targetName);
                output.success("Remove object {} from blacklist.", param.targetName);
            });
            command.overload<BlacklistOP>().required("subCommand").execute([](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) {
                if (param.subCommand == BlacklistOP::list) {
                    std::string content("Blacklist: Add list");
                    for (auto& key : db->list()) {
                        content += toolUtils::replaceString(key, "OBJECT$", "!- ");
                    }
                    output.success(content);
                    return;
                }
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isType(ActorType::Player)) {
                    output.error("No player selected.");
                    return;
                }
                Player* player = static_cast<Player*>(entity);
                if (param.subCommand == BlacklistOP::gui) {
                    output.success("The UI has been opened to player {}", player->getRealName());
                    MainGui::open(player);
                }
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerConnectEventListener = eventBus.emplaceListener<ll::event::PlayerConnectEvent>(
                [](ll::event::PlayerConnectEvent& event) {
                    if (isBlacklist(&event.self())) {
                        std::string mObjectUuid = event.self().getUuid().asString();
                        std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
                        if (db->has("OBJECT$" + mObjectUuid)) {
                            if (toolUtils::isReach(db->get("OBJECT$" + mObjectUuid, "time"))) {
                                delBlacklist(mObjectUuid);
                                return;
                            }
                            event.cancel();
                            std::string logString = tr(getLanguage(&event.self()), "blacklist.log2");
                            logger.info(LOICollectionAPI::translateString(logString, &event.self(), true));
                            return;
                        }
                        std::string mObjectIP = toolUtils::split(event.self().getIPAndPort(), ':')[0];
                        std::replace(mObjectIP.begin(), mObjectIP.end(), '.', '_');
                        if (db->has("OBJECT$" + mObjectIP)) {
                            if (toolUtils::isReach(db->get("OBJECT$" + mObjectIP, "time"))) {
                                delBlacklist(mObjectIP);
                                return;
                            }
                            event.cancel();
                            std::string logString = tr(getLanguage(&event.self()), "blacklist.log2");
                            logger.info(LOICollectionAPI::translateString(logString, &event.self(), true));
                            return;
                        }
                    }
                }
            );
        }
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerConnectEventListener);
    }

    void addBlacklist(void* player_ptr, std::string cause, int time, int type) {
        Player* player = static_cast<Player*>(player_ptr);
        if (player->isSimulatedPlayer()) return;
        if (cause.empty()) cause = tr(getLanguage(player), "blacklist.cause");
        std::string mObject = player->getUuid().asString();
        if (!type) mObject = toolUtils::split(player->getIPAndPort(), ':')[0];
        std::replace(mObject.begin(), mObject.end(), '.', '_');
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", toolUtils::timeCalculate(time));
        }
        ll::service::getServerNetworkHandler()->disconnectClient(
            player->getNetworkIdentifier(),
            Connection::DisconnectFailReason::Kicked,
            cause, false
        );
        std::string logString = tr(getLanguage(player), "blacklist.log1");
        logger.info(LOICollectionAPI::translateString(logString, player, true));
    }

    void delBlacklist(std::string target) {
        if (db->has("OBJECT$" + target) && target != "None") {
            db->remove("OBJECT$" + target);
        }
    }

    bool isBlacklist(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObjectUuid = player->getUuid().asString();
        std::string mObjectIP = toolUtils::split(player->getIPAndPort(), ':')[0];
        std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
        std::replace(mObjectIP.begin(), mObjectIP.end(), '.', '_');
        if (db->has("OBJECT$" + mObjectUuid)) return true;
        return db->has("OBJECT$" + mObjectIP);
    }
}