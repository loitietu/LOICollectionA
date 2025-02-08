#include <memory>
#include <vector>
#include <string>
#include <numeric>

#include <fmt/core.h>
#include <magic_enum.hpp>

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
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include <mc/network/ConnectionRequest.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/connection/DisconnectFailReason.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"
#include "include/ServerEvents/LoginPacketEvent.h"

#include "utils/McUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/BlacklistPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::blacklist {
    enum SelectorType : int {
        ip = 0,
        uuid = 1,
        clientid = 2
    };
    struct BlacklistOP {
        CommandSelector<Player> target;
        SelectorType type;
        std::string object;
        std::string cause;
        int time = 0;
    };

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr LoginPacketEventListener;

    namespace MainGui {
        void info(Player& player, std::string target) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${target}", target);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("OBJECT$" + target, "player", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", db->get("OBJECT$" + target, "cause", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${subtime}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "subtime", "None")));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + target, "time", "None")));

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.info.remove"), [target](Player&) -> void {
                delBlacklist(target);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::remove(pl);
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), "", "None");
            form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), "", "0");
            form.appendDropdown("dropdown", tr(mObjectLanguage, "blacklist.gui.add.dropdown"), { "ip", "uuid", "clientid" });
            form.sendTo(player, [&](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::add(pl);

                std::string PlayerSelectType = std::get<std::string>(dt->at("dropdown"));
                std::string PlayerInputCause = std::get<std::string>(dt->at("Input1"));
                int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                
                addBlacklist(target, PlayerInputCause, time, getType(PlayerSelectType));
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.add.title"),
                tr(mObjectLanguage, "blacklist.gui.add.label"));
            for (Player*& mTarget : McUtils::getAllPlayers()) {
                form.appendButton(mTarget->getRealName(), [mTarget](Player& pl) -> void  {
                    MainGui::content(pl, *mTarget);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"),
                tr(mObjectLanguage, "blacklist.gui.remove.label"));
            for (std::string& mItem : db->list()) {
                ll::string_utils::replaceAll(mItem, "OBJECT$", "");
                form.appendButton(mItem, [mItem](Player& pl) {
                    MainGui::info(pl, mItem);
                });
            }
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void open(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.title"), tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.addBlacklist"), "textures/ui/backup_replace", "path", [](Player& pl) -> void {
                MainGui::add(pl);
            });
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.removeBlacklist"), "textures/ui/free_download_symbol", "path", [](Player& pl) -> void {
                MainGui::remove(pl);
            });
            form.sendTo(player);
        }
    }

    namespace {
        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("blacklist", tr({}, "commands.blacklist.description"), CommandPermissionLevel::GameDirectors);
            command.overload<BlacklistOP>().text("add").required("type").required("target").optional("cause").optional("time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) -> void {
                CommandSelectorResults<Player> results = param.target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    if (isBlacklist(*pl) || (int) pl->getPlayerPermissionLevel() >= 2 || pl->isSimulatedPlayer()) {
                        output.error(fmt::runtime(tr({}, "commands.blacklist.error.add")), pl->getRealName());
                        continue;
                    }
                    addBlacklist(*pl, param.cause, param.time, 
                        getType(magic_enum::enum_name(param.type).data())
                    );

                    output.success(fmt::runtime(tr({}, "commands.blacklist.success.add")), pl->getRealName());
                }
            });
            command.overload<BlacklistOP>().text("remove").required("object").execute(
                [](CommandOrigin const&, CommandOutput& output, BlacklistOP const& param) -> void {
                if (!db->has("OBJECT$" + param.object))
                    return output.error(fmt::runtime(tr({}, "commands.blacklist.error.remove")), param.object);
                delBlacklist(param.object);

                output.success(fmt::runtime(tr({}, "commands.blacklist.success.remove")), param.object);
            });
            command.overload().text("list").execute([](CommandOrigin const&, CommandOutput& output) -> void {
                std::vector<std::string> mObjectList = db->list();
                std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), 
                    [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });
                ll::string_utils::replaceAll(result, "OBJECT$", "");

                output.success(fmt::runtime(tr({}, "commands.blacklist.success.list")), result.empty() ? "None" : result);
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
            LoginPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::LoginPacketEvent>(
                [](LOICollection::ServerEvents::LoginPacketEvent& event) -> void {
                    std::string mObjectTips = tr(getLanguage(event.getUUID().asString()), "blacklist.tips");

                    for (BlacklistType mType : magic_enum::enum_values<BlacklistType>()) {
                        std::string mObject = getResult(
                            event.getIp(), event.getUUID().asString(), 
                            event.getConnectionRequest().getDeviceId(), mType
                        );
                        std::replace(mObject.begin(), mObject.end(), '.', '_');
                        std::replace(mObject.begin(), mObject.end(), '-', '_');
                        if (db->has("OBJECT$" + mObject)) {
                            if (SystemUtils::isReach(db->get("OBJECT$" + mObject, "time")))
                                return delBlacklist(mObject);
                            ll::string_utils::replaceAll(mObjectTips, "${cause}", db->get("OBJECT$" + mObject, "cause"));
                            ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObject, "time")));
                            ll::service::getServerNetworkHandler()->disconnectClient(
                                event.getNetworkIdentifier(), Connection::DisconnectFailReason::Kicked,
                                mObjectTips, {}, false
                            );
                            return;
                        }
                    }
                }
            );
        }
    }

    BlacklistType getType(std::string type) {
        switch (ll::hash_utils::doHash(type)) {
            case ll::hash_utils::doHash("ip"):
                return BlacklistType::ip;
            case ll::hash_utils::doHash("uuid"):
                return BlacklistType::uuid;
            case ll::hash_utils::doHash("clientid"):
                return BlacklistType::clientid;
        }
        return BlacklistType::ip;
    }

    std::string getResult(std::string ip, std::string uuid, std::string clientid, BlacklistType type) {
        switch (type) {
            case BlacklistType::ip:
                return ip;
            case BlacklistType::uuid:
                return uuid;
            case BlacklistType::clientid:
                return clientid;
        }
        return uuid;
    }

    std::string getResult(Player& player, BlacklistType type) {
        std::string mObject = player.getIPAndPort();
        std::string ip = mObject.substr(0, mObject.find(':'));
        std::string uuid = player.getUuid().asString();
        std::string clientid = player.getConnectionRequest()->getDeviceId();
        return getResult(ip, uuid, clientid, type);
    }

    void addBlacklist(Player& player, std::string cause, int time, BlacklistType type) {
        if ((int) player.getPlayerPermissionLevel() >= 2 || !isValid())
            return;

        cause = cause.empty() ? "None" : cause;
        std::string mObject = getResult(player, type);
        std::replace(mObject.begin(), mObject.end(), '.', '_');
        std::replace(mObject.begin(), mObject.end(), '-', '_');
        if (!db->has("OBJECT$" + mObject)) {
            db->create("OBJECT$" + mObject);
            db->set("OBJECT$" + mObject, "player", player.getRealName());
            db->set("OBJECT$" + mObject, "cause", cause);
            db->set("OBJECT$" + mObject, "time", time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time) : "0");
            db->set("OBJECT$" + mObject, "subtime", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        }
        std::string mObjectTips = tr(getLanguage(player), "blacklist.tips");
        ll::string_utils::replaceAll(mObjectTips, "${cause}", cause);
        ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("OBJECT$" + mObject, "time")));
        ll::service::getServerNetworkHandler()->disconnectClient(
            player.getNetworkIdentifier(), Connection::DisconnectFailReason::Kicked,
            mObjectTips, {}, false
        );
        logger->info(LOICollection::LOICollectionAPI::translateString(tr({}, "blacklist.log1"), player));
    }

    void delBlacklist(std::string target) {
        if (!isValid()) return;

        if (db->has("OBJECT$" + target))
            db->remove("OBJECT$" + target);
        logger->info(ll::string_utils::replaceAll(tr({}, "blacklist.log2"), "${target}", target));
    }

    bool isBlacklist(Player& player) {
        if (!isValid()) return false;

        std::string mObjectUuid = player.getUuid().asString();
        std::string mObjectIP = std::string(ll::string_utils::splitByPattern(player.getIPAndPort(), ":")[0]);
        std::replace(mObjectUuid.begin(), mObjectUuid.end(), '-', '_');
        std::replace(mObjectIP.begin(), mObjectIP.end(), '.', '_');
        if (db->has("OBJECT$" + mObjectUuid))
            return true;
        return db->has("OBJECT$" + mObjectIP);
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
        eventBus.removeListener(LoginPacketEventListener);
    }
}