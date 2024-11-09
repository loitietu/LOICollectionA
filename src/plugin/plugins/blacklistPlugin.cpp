#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <numeric>

#include <ll/api/Logger.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>
#include <mc/enums/connection/DisconnectFailReason.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>

#include "include/APIUtils.h"
#include "include/languagePlugin.h"
#include "include/HookPlugin.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/blacklistPlugin.h"

using I18nUtils::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::blacklist {
    struct BlacklistOP {
        enum SelectorType {
            ip, uuid
        } selectorType;
        CommandSelector<Player> target;
        std::string targetName;
        std::string cause;
        int time = -1;
    };

    std::unique_ptr<SQLiteStorage> db;
    ll::Logger logger("LOICollectionA - Blacklist");

    namespace MainGui {
        void info(void* player_ptr, std::string target) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", db->get("OBJECT$" + target, "cause"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "time")));

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.info.remove"), [target](Player& pl) {
                delBlacklist(target);

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

            ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendDropdown("dropdown", tr(mObjectLanguage, "blacklist.gui.add.dropdown"), { "ip", "uuid" });
            form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), "", "0");
            form.sendTo(*player, [target](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) {
                if (!dt) return MainGui::add(&pl);

                std::string PlayerSelectType = std::get<std::string>(dt->at("dropdown"));
                std::string PlayerInputCause = std::get<std::string>(dt->at("Input1"));
                int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                Player* pl2 = McUtils::getPlayerFromName(target);

                switch (ll::hash_utils::doHash(PlayerSelectType)) {
                    case ll::hash_utils::doHash("ip"):
                        addBlacklist(pl2, PlayerInputCause, time, BLACKLIST_TYPE_IP);
                        break;
                    case ll::hash_utils::doHash("uuid"):
                        addBlacklist(pl2, PlayerInputCause, time, BLACKLIST_TYPE_UUID);
                        break;
                };

                McUtils::Gui::submission(&pl, [](Player* player) {
                    return MainGui::add(player);
                });
            });
        }

        void add(void* player_ptr) {
            Player* player = static_cast<Player*>(player_ptr);
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.add.title"), tr(mObjectLanguage, "blacklist.gui.add.label"));
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

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), tr(mObjectLanguage, "blacklist.gui.remove.label"));
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
                if (!isBlacklist(pl) && (int) pl->getPlayerPermissionLevel() < 2 && !pl->isSimulatedPlayer()) {
                    output.addMessage(fmt::format("Add player {} to blacklist.", 
                        pl->getRealName()), {}, CommandOutputMessageType::Success);
                    switch (param.selectorType) {
                        case BlacklistOP::ip:
                            addBlacklist(pl, param.cause, param.time, BLACKLIST_TYPE_IP);
                            break;
                        case BlacklistOP::uuid:
                            addBlacklist(pl, param.cause, param.time, BLACKLIST_TYPE_UUID);
                            break;
                        default:
                            logger.error("Unknown selector type: {}", param.selectorType);
                            break;
                    }
                }
            }
        };

        void registerCommand() {
            auto commandRegistery = ll::service::getCommandRegistry();
            if (!commandRegistery)
                throw std::runtime_error("Failed to get command registry.");
            auto& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("blacklist", "§e§lLOICollection -> §b服务器黑名单", CommandPermissionLevel::GameDirectors);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("time").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("cause").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("add").required("selectorType").required("target").required("cause").required("time").execute(BlacklistCommandADD);
            command.overload<BlacklistOP>().text("remove").required("targetName").execute([](CommandOrigin const& /*unused*/, CommandOutput& output, BlacklistOP const& param) {
                if (!db->has("OBJECT$" + param.targetName))
                    return output.error("Object {} is not in blacklist.", param.targetName);
                delBlacklist(param.targetName);
                output.success("Remove object {} from blacklist.", param.targetName);
            });
            command.overload().text("list").execute([](CommandOrigin const& /*unused*/, CommandOutput& output) {
                std::vector<std::string> mObjectList = db->list();
                std::string result = std::accumulate(mObjectList.begin(), mObjectList.end(), std::string(),
                    [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + ll::string_utils::replaceAll(b, "OBJECT$", "");
                });
                output.success("Blacklist: {}", result);
            });
            command.overload().text("gui").execute([](CommandOrigin const& origin, CommandOutput& output) {
                auto* entity = origin.getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return output.error("No player selected.");
                Player* player = static_cast<Player*>(entity);
                output.success("The UI has been opened to player {}", player->getRealName());
                MainGui::open(player);
            });
        }

        void listenEvent() {
            LOICollection::HookPlugin::Event::onLoginPacketSendEvent([](void* identifier_ptr, std::string mUuid, std::string mIpAndPort) {
                NetworkIdentifier* identifier = static_cast<NetworkIdentifier*>(identifier_ptr);
                std::string mObjectTips = tr(getLanguage(ll::service::getLevel()->getPlayer(mUuid)), "blacklist.tips");
                std::replace(mUuid.begin(), mUuid.end(), '-', '_');
                if (db->has("OBJECT$" + mUuid)) {
                    if (SystemUtils::isReach(db->get("OBJECT$" + mUuid, "time")))
                        return delBlacklist(mUuid);
                    ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("OBJECT$" + mUuid, "cause"));
                    ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mUuid, "time")));
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        *identifier, Connection::DisconnectFailReason::Kicked, mObjectTips, false
                    );
                    return;
                }
                std::string mObjectIP = std::string(ll::string_utils::splitByPattern(mIpAndPort, ":")[0]);
                std::replace(mObjectIP.begin(), mObjectIP.end(), '.', '_');
                if (db->has("OBJECT$" + mObjectIP)) {
                    if (SystemUtils::isReach(db->get("OBJECT$" + mObjectIP, "time")))
                        return delBlacklist(mObjectIP);
                    ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("OBJECT$" + mObjectIP, "cause"));
                    ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObjectIP, "time")));
                    ll::service::getServerNetworkHandler()->disconnectClient(
                        *identifier, Connection::DisconnectFailReason::Kicked, mObjectTips, false
                    );
                    return;
                }
            });
        }
    }

    void addBlacklist(void* player_ptr, std::string cause, int time, int type) {
        Player* player = static_cast<Player*>(player_ptr);

        if ((int) player->getPlayerPermissionLevel() >= 2)
            return;

        cause = cause.empty() ? "None" : cause;
        std::string mObjectLanguage = getLanguage(player);
        std::string mObject = type == BLACKLIST_TYPE_UUID ? player->getUuid().asString()
            : std::string(ll::string_utils::splitByPattern(player->getIPAndPort(), ":")[0]);
        std::replace(mObject.begin(), mObject.end(), '.', '_');
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0");
        }
        std::string mObjectTips = tr(mObjectLanguage, "blacklist.tips");
        ll::string_utils::replaceAll(mObjectTips, "${cause}", cause);
        ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObject, "time")));
        ll::service::getServerNetworkHandler()->disconnectClient(
            player->getNetworkIdentifier(), Connection::DisconnectFailReason::Kicked, mObjectTips, false
        );
        std::string logString = tr(mObjectLanguage, "blacklist.log1");
        logger.info(LOICollection::LOICollectionAPI::translateString(logString, player));
    }

    void delBlacklist(std::string target) {
        if (db->has("OBJECT$" + target))
            db->remove("OBJECT$" + target);
        std::string logString = tr(getLanguage(nullptr), "blacklist.log2");
        logger.info(ll::string_utils::replaceAll(logString, "${target}", target));
    }

    bool isBlacklist(void* player_ptr) {
        Player* player = static_cast<Player*>(player_ptr);
        std::string mObjectUuid = player->getUuid().asString();
        std::string mObjectIP = std::string(ll::string_utils::splitByPattern(player->getIPAndPort(), ":")[0]);
        std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
        std::replace(mObjectIP.begin(), mObjectIP.end(), '.', '_');
        if (db->has("OBJECT$" + mObjectUuid))
            return true;
        return db->has("OBJECT$" + mObjectIP);
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger.setFile("./logs/LOICollectionA.log");
        registerCommand();
        listenEvent();
    }
}