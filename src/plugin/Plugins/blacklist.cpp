#include <memory>

#include <fmt/format.h>

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
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>
#include <mc/enums/connection/DisconnectFailReason.h>
#include <mc/network/ServerNetworkHandler.h>

#include "../Utils/I18nUtils.h"
#include "../Utils/toolUtils.h"
#include "../Include/language.h"

#include "../Include/blacklist.h"

namespace blacklistPlugin {
    namespace {
        using I18nUtils::tr;
        std::unique_ptr<ll::data::KeyValueDB> db;
        ll::event::ListenerPtr PlayerJoinEventListener;
        ll::Logger logger("LOICollectionA - Blacklist");

        struct BlacklistOP {
            enum SelectorType {
                ip, uuid
            } selectorType;
            CommandSelector<Player> target;
            std::string targetName = "";
            std::string cause = "";
            int time = 0;
        };

        const auto BlacklistCommandADD = [](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) {
            for (auto& pl : param.target.results(origin)) {
                output.addMessage(fmt::format("Add player {}({}) to blacklist.", 
                    pl->getRealName(), pl->getUuid().asString()), 
                    {}, CommandOutputMessageType::Success);
                std::string cause(param.cause);
                if (cause.empty()) cause = tr(languagePlugin::getLanguage(pl), "blacklist.cause");
                if (param.selectorType == BlacklistOP::ip) {
                    addBlacklist(pl, cause, param.time, 0);
                } else if (param.selectorType == BlacklistOP::uuid) {
                    addBlacklist(pl, cause, param.time, 1);
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
            command.overload<BlacklistOP>().text("remove").required("targetName").execute([](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) {
                delBlacklist(param.targetName);
                output.success("Remove player {} from blacklist.", param.targetName);
            });
        }

        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [&](ll::event::PlayerJoinEvent& event) {
                    std::string uuid = event.self().getUuid().asString();
                    if (db->has("list." + uuid + ".end.time") || db->has("list." + uuid + ".end.cause")) {
                        if (toolUtils::isReach(db->get("list." + uuid + ".end.time").value())) {
                            delBlacklist(uuid);
                            return;
                        }
                        ll::service::getServerNetworkHandler()->disconnectClient(
                            event.self().getNetworkIdentifier(),
                            Connection::DisconnectFailReason::Kicked,
                            db->get("list." + uuid + ".end.cause").value(), false
                        );
                        return;
                    }
                    std::string ip = toolUtils::split(event.self().getIPAndPort(), ':')[0];
                    if (db->has("list." + ip + ".end.time") || db->has("list." + ip + ".end.cause")) {
                        if (toolUtils::isReach(db->get("list." + ip + ".end.time").value())) {
                            delBlacklist(ip);
                            return;
                        }
                        ll::service::getServerNetworkHandler()->disconnectClient(
                            event.self().getNetworkIdentifier(),
                            Connection::DisconnectFailReason::Kicked,
                            db->get("list." + ip + ".end.cause").value(), false
                        );
                        return;
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

    void addBlacklist(void* player_ptr, std::string cause, int time, int type) {
        Player* player = static_cast<Player*>(player_ptr);
        switch (type) {
            case 0: {
                std::string ip = toolUtils::split(player->getIPAndPort(), ':')[0];
                db->set("list." + ip + ".end.cause", cause);
                db->set("list." + ip + ".end.time", toolUtils::timeCalculate(time));
            }
            case 1: {
                std::string uuid = player->getUuid().asString();
                db->set("list." + uuid + ".end.cause", cause);
                db->set("list." + uuid + ".end.time", toolUtils::timeCalculate(time));
            }
            default:
                break;
        }
        ll::service::getServerNetworkHandler()->disconnectClient(
            player->getNetworkIdentifier(),
            Connection::DisconnectFailReason::Kicked,
            cause, false
        );
    }

    void delBlacklist(std::string target) {
        if (db->has("list." + target + ".end.cause") && db->has("list." + target + ".end.time")) {
            db->del("list." + target + ".end.cause");
            db->del("list." + target + ".end.time");
        }
    }
}