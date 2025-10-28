#include <memory>
#include <vector>
#include <string>
#include <numeric>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>
#include <SQLiteCpp/SQLiteCpp.h>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>
#include <ll/api/form/CustomForm.h>
#include <ll/api/form/SimpleForm.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/command/EnumName.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>

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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/ServerEvents/NetworkPacketEvent.h"

#include "LOICollectionA/utils/SystemUtils.h"
#include "LOICollectionA/utils/I18nUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/BlacklistPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    enum class BlacklistObject;

    constexpr inline auto BlacklistObjectName = ll::command::enum_name_v<BlacklistObject>;

    struct BlacklistPlugin::operation {
        ll::command::SoftEnum<BlacklistObject> Object;
        
        CommandSelector<Player> Target;

        std::string Cause;
        int Time = 0;
    };

    struct BlacklistPlugin::Impl {
        bool ModuleEnabled = false;
        
        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr NetworkPacketEventListener;
    };

    BlacklistPlugin::BlacklistPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    BlacklistPlugin::~BlacklistPlugin() = default;
    
    SQLiteStorage* BlacklistPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* BlacklistPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void BlacklistPlugin::gui::info(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), 
            fmt::format(fmt::runtime(mObjectLabel), id, 
                this->mParent.getDatabase()->get("Blacklist", id + ".NAME", "None"), 
                this->mParent.getDatabase()->get("Blacklist", id + ".CAUSE", "None"), 
                SystemUtils::toFormatTime(this->mParent.getDatabase()->get("Blacklist", id + ".SUBTIME", "None"), "None"), 
                SystemUtils::toFormatTime(this->mParent.getDatabase()->get("Blacklist", id + ".TIME", "None"), "None")
            )
        );
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.info.remove"), [this, id](Player&) -> void {
            this->mParent.delBlacklist(id);
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->remove(pl);
        });
    }

    void BlacklistPlugin::gui::content(Player& player, Player& target) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::CustomForm form(tr(mObjectLanguage, "blacklist.gui.add.title"));
        form.appendLabel(tr(mObjectLanguage, "blacklist.gui.label"));
        form.appendInput("Input1", tr(mObjectLanguage, "blacklist.gui.add.input1"), tr(mObjectLanguage, "blacklist.gui.add.input1.placeholder"));
        form.appendInput("Input2", tr(mObjectLanguage, "blacklist.gui.add.input2"), tr(mObjectLanguage, "blacklist.gui.add.input2.placeholder"));
        form.sendTo(player, [this, &target, mObjectLanguage](Player& pl, ll::form::CustomFormResult const& dt, ll::form::FormCancelReason) -> void {
            if (!dt) return this->add(pl);

            std::string mCause = std::get<std::string>(dt->at("Input1"));
            
            if (mCause.empty()) {
                pl.sendMessage(tr(mObjectLanguage, "generic.tips.noinput"));
                return this->add(pl);
            }

            int time = SystemUtils::toInt(std::get<std::string>(dt->at("Input2")), 0);
            
            this->mParent.addBlacklist(target, mCause, time);
        });
    }

    void BlacklistPlugin::gui::add(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.add.title"), tr(mObjectLanguage, "blacklist.gui.add.label"));
        ll::service::getLevel()->forEachPlayer([this, &form](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            form.appendButton(mTarget.getRealName(), [this, &mTarget](Player& pl) -> void  {
                this->content(pl, mTarget);
            });
            return true;
        });
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void BlacklistPlugin::gui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), tr(mObjectLanguage, "blacklist.gui.remove.label"));
        for (std::string& mItem : this->mParent.getBlacklists()) {
            form.appendButton(mItem, [this, mItem](Player& pl) {
                this->info(pl, mItem);
            });
        }
        form.sendTo(player, [this](Player& pl, int id, ll::form::FormCancelReason) -> void {
            if (id == -1) this->open(pl);
        });
    }

    void BlacklistPlugin::gui::open(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.title"), tr(mObjectLanguage, "blacklist.gui.label"));
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.addBlacklist"), "textures/ui/backup_replace", "path", [this](Player& pl) -> void {
            this->add(pl);
        });
        form.appendButton(tr(mObjectLanguage, "blacklist.gui.removeBlacklist"), "textures/ui/free_download_symbol", "path", [this](Player& pl) -> void {
            this->remove(pl);
        });
        form.sendTo(player);
    }

    void BlacklistPlugin::registeryCommand() {
        ll::command::CommandRegistrar::getInstance().tryRegisterSoftEnum(BlacklistObjectName, getBlacklists());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("blacklist", tr({}, "commands.blacklist.description"), CommandPermissionLevel::GameDirectors);
        command.overload<operation>().text("add").required("Target").optional("Cause").optional("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param) -> void {
            CommandSelectorResults<Player> results = param.Target.results(origin);
            if (results.empty())
                return output.error(tr({}, "commands.generic.target"));

            for (Player*& pl : results) {
                if (this->isBlacklist(*pl) || pl->getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors || pl->isSimulatedPlayer()) {
                    output.error(fmt::runtime(tr({}, "commands.blacklist.error.add")), pl->getRealName());
                    continue;
                }
                this->addBlacklist(*pl, param.Cause, param.Time);

                output.success(fmt::runtime(tr({}, "commands.blacklist.success.add")), pl->getRealName());
            }
        });
        command.overload<operation>().text("remove").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            if (!this->getDatabase()->hasByPrefix("Blacklist", param.Object + ".", 7))
                return output.error(fmt::runtime(tr({}, "commands.blacklist.error.remove")), param.Object);
            this->delBlacklist(param.Object);

            output.success(fmt::runtime(tr({}, "commands.blacklist.success.remove")), param.Object);
        });
        command.overload<operation>().text("info").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->getByPrefix("Blacklist", param.Object + ".");
            
            if (mEvent.empty())
                return output.error(tr({}, "commands.blacklist.error.info"));

            output.success(tr({}, "commands.blacklist.success.info"));
            std::for_each(mEvent.begin(), mEvent.end(), [&output, id = param.Object](const std::pair<std::string, std::string>& mPair) {
                std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                output.success("{0}: {1}", mKey, mPair.second);
            });
        });
        command.overload().text("list").execute([this](CommandOrigin const&, CommandOutput& output) -> void {
            std::vector<std::string> mObjectList = this->getBlacklists();
            std::string result = std::accumulate(mObjectList.cbegin(), mObjectList.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.blacklist.success.list")), result.empty() ? "None" : result);
        });
        command.overload().text("gui").execute([this](CommandOrigin const& origin, CommandOutput& output) -> void {
            Actor* entity = origin.getEntity();
            if (entity == nullptr || !entity->isPlayer())
                return output.error(tr({}, "commands.generic.target"));
            Player& player = *static_cast<Player*>(entity);
            
            this->mGui->open(player);

            output.success(fmt::runtime(tr({}, "commands.generic.ui")), player.getRealName());
        });
    }

    void BlacklistPlugin::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->NetworkPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([this](LOICollection::ServerEvents::NetworkPacketEvent& event) -> void {
            if (event.getPacket().getId() != MinecraftPacketIds::Login)
                return;

            auto& packet = static_cast<LoginPacket&>(const_cast<Packet&>(event.getPacket()));

            std::string mUuid = packet.mConnectionRequest->mLegacyMultiplayerToken->getIdentity().asString();
            std::string mIp = event.getNetworkIdentifier().getIPAndPort().substr(0, event.getNetworkIdentifier().getIPAndPort().find_last_of(':' - 1));
            std::string mClientId = packet.mConnectionRequest->getDeviceId();

            std::vector<std::string> mKeys = getBlacklists();
            auto it = std::find_if(mKeys.begin(), mKeys.end(), [&](const std::string& mId) -> bool {
                return isBlacklist(mId, mUuid, mIp, mClientId);
            });

            std::string mId = it != mKeys.end() ? *it : "";

            if (mId.empty())
                return;

            std::unordered_map<std::string, std::string> mData = this->getDatabase()->getByPrefix("Blacklist", mId + ".");

            if (SystemUtils::isPastOrPresent(mData.at(mId + ".TIME")))
                return delBlacklist(mId);

            std::replace(mUuid.begin(), mUuid.end(), '-', '_');

            std::string mObjectTips = tr(LanguagePlugin::getInstance().getLanguage(mUuid), "blacklist.tips");
            ll::service::getServerNetworkHandler()->disconnectClientWithMessage(
                event.getNetworkIdentifier(), event.getSubClientId(), Connection::DisconnectFailReason::Kicked,
                fmt::format(fmt::runtime(mObjectTips), 
                    mData.at(mId + ".CAUSE"),
                    SystemUtils::toFormatTime(mData.at(mId + ".TIME"), "None")
                ),
                std::nullopt, false
            );
        });
    }

    void BlacklistPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->NetworkPacketEventListener);
    }

    void BlacklistPlugin::addBlacklist(Player& player, const std::string& cause, int time) {
        if (!this->isValid() || player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors)
            return;

        std::string mCause = cause.empty() ? "None" : cause;

        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        SQLite::Transaction transaction(*this->getDatabase()->getDatabase());
        this->getDatabase()->set("Blacklist", mTismestamp + ".NAME", player.getRealName());
        this->getDatabase()->set("Blacklist", mTismestamp + ".CAUSE", mCause);
        this->getDatabase()->set("Blacklist", mTismestamp + ".TIME", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time, "None") : "None");
        this->getDatabase()->set("Blacklist", mTismestamp + ".SUBTIME", SystemUtils::getNowTime("%Y%m%d%H%M%S"));
        this->getDatabase()->set("Blacklist", mTismestamp + ".DATA_UUID", player.getUuid().asString());
        this->getDatabase()->set("Blacklist", mTismestamp + ".DATA_IP", player.getIPAndPort().substr(0, player.getIPAndPort().find_last_of(':') - 1));
        this->getDatabase()->set("Blacklist", mTismestamp + ".DATA_CLIENTID", player.getConnectionRequest()->getDeviceId());
        transaction.commit();

        ll::command::CommandRegistrar::getInstance().addSoftEnumValues(BlacklistObjectName, { mTismestamp });

        std::string mObjectTips = tr(LanguagePlugin::getInstance().getLanguage(player), "blacklist.tips");
        ll::service::getServerNetworkHandler()->disconnectClientWithMessage(
            player.getNetworkIdentifier(), player.getClientSubId(), Connection::DisconnectFailReason::Kicked,
            fmt::format(fmt::runtime(mObjectTips), mCause,
                SystemUtils::toFormatTime(this->getDatabase()->get("Blacklist", mTismestamp + ".TIME"), "None")
            ),
            std::nullopt, false
        );

        this->getLogger()->info(LOICollectionAPI::getVariableString(tr({}, "blacklist.log1"), player));
    }

    void BlacklistPlugin::delBlacklist(const std::string& id) {
        if (!this->isValid()) 
            return;

        this->getDatabase()->delByPrefix("Blacklist", id + ".");

        ll::command::CommandRegistrar::getInstance().removeSoftEnumValues(BlacklistObjectName, { id });

        this->getLogger()->info(fmt::runtime(tr({}, "blacklist.log2")), id);
    }

    std::string BlacklistPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return {};

        std::string mUuid = player.getUuid().asString();
        std::string mIp = player.getIPAndPort().substr(0, player.getIPAndPort().find_last_of(':') - 1);
        std::string mClientId = player.getConnectionRequest()->getDeviceId();

        std::vector<std::string> mKeys = this->getBlacklists();
        auto it = std::find_if(mKeys.begin(), mKeys.end(), [this, mUuid, mIp, mClientId](const std::string& mId) -> bool {
            return this->isBlacklist(mId, mUuid, mIp, mClientId);
        });

        return it != mKeys.end() ? *it : "";
    }

    std::vector<std::string> BlacklistPlugin::getBlacklists() {
        if (!this->isValid())
            return {};

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Blacklist", "%.NAME");

        std::vector<std::string> mResult(mKeys.size());
        std::transform(mKeys.begin(), mKeys.end(), mResult.begin(), [](const std::string& mKey) -> std::string {
            size_t mPos = mKey.find('.');
            return mPos != std::string::npos ? mKey.substr(0, mPos) : "";
        });

        return mResult;
    }

    bool BlacklistPlugin::isBlacklist(const std::string& mId, const std::string& uuid, const std::string& ip, const std::string& clientId) {
        if (!this->isValid())
            return false;

        std::unordered_map<std::string, std::string> mData = this->getDatabase()->getByPrefix("Blacklist", mId + ".");

        return mData.at(mId + ".DATA_UUID") == uuid || mData.at(mId + ".DATA_IP") == ip || mData.at(mId + ".DATA_CLIENTID") == clientId;
    }

    bool BlacklistPlugin::isBlacklist(Player& player) {
        if (!this->isValid())
            return false;

        std::string mId = this->getBlacklist(player);

        return !mId.empty();
    }

    bool BlacklistPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool BlacklistPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Blacklist)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "blacklist.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool BlacklistPlugin::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->getDatabase()->create("Blacklist");
        
        this->registeryCommand();
        this->listenEvent();

        return true;
    }

    bool BlacklistPlugin::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        return true;
    }
}

REGISTRY_HELPER("BlacklistPlugin", LOICollection::Plugins::BlacklistPlugin, LOICollection::Plugins::BlacklistPlugin::getInstance())
