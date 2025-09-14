#include <memory>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <unordered_map>

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
#include <ll/api/utils/StringUtils.h>
#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/string/HashedString.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/ActorDefinitionIdentifier.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/ConnectionRequest.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/connection/DisconnectFailReason.h>

#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include <mc/common/SubClientId.h>

#include "include/APIUtils.h"
#include "include/LanguagePlugin.h"

#include "include/ServerEvents/NetworkPacketEvent.h"

#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "include/BlacklistPlugin.h"

using I18nUtilsTools::tr;
using LOICollection::Plugins::language::getLanguage;

namespace LOICollection::Plugins::blacklist {
    struct BlacklistOP {
        CommandSelector<Player> Target;
        std::string Id;
        std::string Cause;
        int Time = 0;
    };

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr NetworkPacketEventListener;

    namespace MainGui {
        void info(Player& player, const std::string& id) {
            std::string mObjectLanguage = getLanguage(player);

            std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
            ll::string_utils::replaceAll(mObjectLabel, "${id}", id);
            ll::string_utils::replaceAll(mObjectLabel, "${player}", db->get("Blacklist", id + ".NAME", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${cause}", db->get("Blacklist", id + ".CAUSE", "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${subtime}", SystemUtils::formatDataTime(db->get("Blacklist", id + ".SUBTIME", "None"), "None"));
            ll::string_utils::replaceAll(mObjectLabel, "${time}", SystemUtils::formatDataTime(db->get("Blacklist", id + ".TIME", "None"), "None"));

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), mObjectLabel);
            form.appendButton(tr(mObjectLanguage, "blacklist.gui.info.remove"), [id](Player&) -> void {
                delBlacklist(id);
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::remove(pl);
            });
        }

        void content(Player& player, Player& target) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
            form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
            form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), tr(mObjectLanguage, "blacklist.gui.add.input1.placeholder"));
            form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), tr(mObjectLanguage, "blacklist.gui.add.input2.placeholder"));
            form.sendTo(player, [&target, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
                if (!dt) return MainGui::add(pl);

                std::string mCause = std::get<std::string>(dt->at("Input1"));
                
                if (mCause.empty()) {
                    pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                    return MainGui::add(pl);
                }

                int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
                
                addBlacklist(target, mCause, time);
            });
        }

        void add(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.add.title"), tr(mObjectLanguage, "blacklist.gui.add.label"));
            ll::service::getLevel()->forEachPlayer([&form](Player& mTarget) -> bool {
                if (mTarget.isSimulatedPlayer())
                    return true;

                form.appendButton(mTarget.getRealName(), [&mTarget](Player& pl) -> void  {
                    MainGui::content(pl, mTarget);
                });
                return true;
            });
            form.sendTo(player, [](Player& pl, int id, ll::form::FormCancelReason) -> void {
                if (id == -1) MainGui::open(pl);
            });
        }

        void remove(Player& player) {
            std::string mObjectLanguage = getLanguage(player);

            ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), tr(mObjectLanguage, "blacklist.gui.remove.label"));
            for (std::string& mItem : getBlacklists()) {
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
            command.overload<BlacklistOP>().text("add").required("Target").optional("Cause").optional("Time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BlacklistOP const& param) -> void {
                CommandSelectorResults<Player> results = param.Target.results(origin);
                if (results.empty())
                    return output.error(tr({}, "commands.generic.target"));

                for (Player*& pl : results) {
                    if (isBlacklist(*pl) || pl->getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || pl->isSimulatedPlayer()) {
                        output.error(fmt::runtime(tr({}, "commands.blacklist.error.add")), pl->getRealName());
                        continue;
                    }
                    addBlacklist(*pl, param.Cause, param.Time);

                    output.success(fmt::runtime(tr({}, "commands.blacklist.success.add")), pl->getRealName());
                }
            });
            command.overload<BlacklistOP>().text("remove").required("Id").execute(
                [](CommandOrigin const&, CommandOutput& output, BlacklistOP const& param) -> void {
                if (!db->hasByPrefix("Blacklist", param.Id + ".", 7))
                    return output.error(fmt::runtime(tr({}, "commands.blacklist.error.remove")), param.Id);
                delBlacklist(param.Id);

                output.success(fmt::runtime(tr({}, "commands.blacklist.success.remove")), param.Id);
            });
            command.overload<BlacklistOP>().text("info").required("Id").execute(
                [](CommandOrigin const&, CommandOutput& output, BlacklistOP const& param) -> void {
                std::unordered_map<std::string, std::string> mEvent = db->getByPrefix("Blacklist", param.Id + ".");
                
                if (mEvent.empty())
                    return output.error(tr({}, "commands.blacklist.error.info"));

                output.success(tr({}, "commands.blacklist.success.info"));
                std::for_each(mEvent.begin(), mEvent.end(), [&output, id = param.Id](const std::pair<std::string, std::string>& mPair) {
                    std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                    output.success("{0}: {1}", mKey, mPair.second);
                });
            });
            command.overload().text("list").execute([](CommandOrigin const&, CommandOutput& output) -> void {
                std::vector<std::string> mObjectList = getBlacklists();
                std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

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
            NetworkPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([](LOICollection::ServerEvents::NetworkPacketEvent& event) -> void {
                if (event.getPacket().getId() != MinecraftPacketIds::Login)
                    return;

                auto& packet = static_cast<LoginPacket&>(const_cast<Packet&>(event.getPacket()));

                std::string mUuid = packet.mConnectionRequest->mLegacyMultiplayerToken->getIdentity().asString();
                std::string mIp = event.getNetworkIdentifier().getIPAndPort().substr(0, event.getNetworkIdentifier().getIPAndPort().find(':'));
                std::string mClientId = packet.mConnectionRequest->getDeviceId();

                std::vector<std::string> mKeys = getBlacklists();
                auto it = std::find_if(mKeys.begin(), mKeys.end(), [&](const std::string& mId) -> bool {
                    return isBlacklist(mId, mUuid, mIp, mClientId);
                });

                std::string mId = it != mKeys.end() ? *it : "";

                if (mId.empty())
                    return;

                std::unordered_map<std::string, std::string> mData = db->getByPrefix("Blacklist", mId + ".");

                if (SystemUtils::isReach(mData.at(mId + ".TIME")))
                    return delBlacklist(mId);

                std::replace(mUuid.begin(), mUuid.end(), '-', '_');

                std::string mObjectTips = tr(getLanguage(mUuid), "blacklist.tips");
                ll::string_utils::replaceAll(mObjectTips, "${cause}", mData.at(mId + ".CAUSE"));
                ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(mData.at(mId + ".TIME"), "None"));

                ll::service::getServerNetworkHandler()->disconnectClient(
                    event.getNetworkIdentifier(), Connection::DisconnectFailReason::Kicked, mObjectTips, {}, false
                );
            });
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(NetworkPacketEventListener);
        }
    }

    void addBlacklist(Player& player, const std::string& cause, int time) {
        if (player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || !isValid())
            return;

        std::string mCause = cause.empty() ? "None" : cause;

        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        db->set("Blacklist", mTismestamp + ".NAME", player.getRealName());
        db->set("Blacklist", mTismestamp + ".CAUSE", mCause);
        db->set("Blacklist", mTismestamp + ".TIME", time ? SystemUtils::timeCalculate(SystemUtils::getNowTime(), time, "None") : "None");
        db->set("Blacklist", mTismestamp + ".SUBTIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        db->set("Blacklist", mTismestamp + ".DATA_UUID", player.getUuid().asString());
        db->set("Blacklist", mTismestamp + ".DATA_IP", player.getIPAndPort().substr(0, player.getIPAndPort().find(':')));
        db->set("Blacklist", mTismestamp + ".DATA_CLIENTID", player.getConnectionRequest()->getDeviceId());

        std::string mObjectTips = tr(getLanguage(player), "blacklist.tips");
        ll::string_utils::replaceAll(mObjectTips, "${cause}", mCause);
        ll::string_utils::replaceAll(mObjectTips, "${time}", SystemUtils::formatDataTime(db->get("Blacklist", mTismestamp + ".TIME"), "None"));

        ll::service::getServerNetworkHandler()->disconnectClient(
            player.getNetworkIdentifier(), Connection::DisconnectFailReason::Kicked,
            mObjectTips, {}, false
        );

        logger->info(LOICollectionAPI::getVariableString(tr({}, "blacklist.log1"), player));
    }

    void delBlacklist(const std::string& target) {
        if (!isValid()) 
            return;

        db->delByPrefix("Blacklist", target + ".");

        logger->info(ll::string_utils::replaceAll(tr({}, "blacklist.log2"), "${target}", target));
    }

    std::string getBlacklist(Player& player) {
        if (!isValid())
            return {};

        std::string mUuid = player.getUuid().asString();
        std::string mIp = player.getIPAndPort().substr(0, player.getIPAndPort().find(':'));
        std::string mClientId = player.getConnectionRequest()->getDeviceId();

        std::vector<std::string> mKeys = getBlacklists();
        auto it = std::find_if(mKeys.begin(), mKeys.end(), [&](const std::string& mId) -> bool {
            return isBlacklist(mId, mUuid, mIp, mClientId);
        });

        return it != mKeys.end() ? *it : "";
    }

    std::vector<std::string> getBlacklists() {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = db->listByPrefix("Blacklist", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_last_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    bool isBlacklist(const std::string& mId, const std::string& uuid, const std::string& ip, const std::string& clientId) {
        if (!isValid())
            return false;

        std::unordered_map<std::string, std::string> mData = db->getByPrefix("Blacklist", mId + ".");

        return mData.at(mId + ".DATA_UUID") == uuid || mData.at(mId + ".DATA_IP") == ip || mData.at(mId + ".DATA_CLIENTID") == clientId;
    }

    bool isBlacklist(Player& player) {
        if (!isValid())
            return false;

        std::string mId = getBlacklist(player);

        return !mId.empty();
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");

        db->create("Blacklist");
        
        registerCommand();
        listenEvent();
    }

    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}
