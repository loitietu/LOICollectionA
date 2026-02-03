#include <atomic>
#include <memory>
#include <vector>
#include <ranges>
#include <string>
#include <algorithm>
#include <filesystem>
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
#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/packet/DisconnectPacket.h>
#include <mc/network/connection/DisconnectFailReason.h>

#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>

#include <mc/common/SubClientId.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/Form/PaginatedForm.h"

#include "LOICollectionA/include/APIUtils.h"
#include "LOICollectionA/include/Plugins/LanguagePlugin.h"

#include "LOICollectionA/include/ServerEvents/modules/BlacklistEvent.h"
#include "LOICollectionA/include/ServerEvents/server/NetworkPacketEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

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
        int Limit = 100;
    };

    struct BlacklistPlugin::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_Blacklist options;
        
        std::shared_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr NetworkPacketEventListener;
        ll::event::ListenerPtr BlacklistAddEventListener;
        ll::event::ListenerPtr BlacklistRemoveEventListener;
    };

    BlacklistPlugin::BlacklistPlugin() : mImpl(std::make_unique<Impl>()), mGui(std::make_unique<gui>(*this)) {};
    BlacklistPlugin::~BlacklistPlugin() = default;

    BlacklistPlugin& BlacklistPlugin::getInstance() {
        static BlacklistPlugin instance;
        return instance;
    }
    
    std::shared_ptr<SQLiteStorage> BlacklistPlugin::getDatabase() {
        return this->mImpl->db;
    }

    std::shared_ptr<ll::io::Logger> BlacklistPlugin::getLogger() {
        return this->mImpl->logger;
    }

    void BlacklistPlugin::gui::info(Player& player, const std::string& id) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        if (!this->mParent.hasBlacklist(id)) {
            player.sendMessage(tr(mObjectLanguage, "blacklist.gui.error"));
            
            this->remove(player);
            return;
        }

        std::unordered_map<std::string, std::string> mData = this->mParent.getDatabase()->get("Blacklist", id);

        std::string mObjectLabel = tr(mObjectLanguage, "blacklist.gui.info.label");
        ll::form::SimpleForm form(tr(mObjectLanguage, "blacklist.gui.remove.title"), 
            fmt::format(fmt::runtime(mObjectLabel), id, 
                mData.at("name"),
                mData.at("cause"),
                SystemUtils::toFormatTime(mData.at("subtime"), "None"),
                SystemUtils::toFormatTime(mData.at("time"), "None")
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

        std::vector<std::string> mPlayers;
        std::vector<std::string> mPlayerUuids;

        ll::service::getLevel()->forEachPlayer([&mPlayers, &mPlayerUuids](Player& mTarget) -> bool {
            if (mTarget.isSimulatedPlayer())
                return true;

            mPlayers.push_back(mTarget.getRealName());
            mPlayerUuids.push_back(mTarget.getUuid().asString());
            return true;
        });

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "blacklist.gui.add.title"),
            tr(mObjectLanguage, "blacklist.gui.add.label"),
            mPlayers
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this, mObjectLanguage, mPlayerUuids = std::move(mPlayerUuids)](Player& pl, int index) -> void {
            Player* mPlayer = ll::service::getLevel()->getPlayer(mPlayerUuids.at(index));
            if (!mPlayer) {
                pl.sendMessage(tr(mObjectLanguage, "blacklist.gui.error"));

                this->open(pl);
                return;
            }

            this->content(pl, *mPlayer);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
    }

    void BlacklistPlugin::gui::remove(Player& player) {
        std::string mObjectLanguage = LanguagePlugin::getInstance().getLanguage(player);

        std::shared_ptr<Form::PaginatedForm> form = std::make_shared<Form::PaginatedForm>(
            tr(mObjectLanguage, "blacklist.gui.remove.title"),
            tr(mObjectLanguage, "blacklist.gui.remove.label"),
            this->mParent.getBlacklists()
        );
        form->setPreviousButton(tr(mObjectLanguage, "generic.gui.page.previous"));
        form->setNextButton(tr(mObjectLanguage, "generic.gui.page.next"));
        form->setChooseButton(tr(mObjectLanguage, "generic.gui.page.choose"));
        form->setChooseInput(tr(mObjectLanguage, "generic.gui.page.choose.input"));
        form->setCallback([this](Player& pl, const std::string& response) -> void {
            this->info(pl, response);
        });
        form->setCloseCallback([this](Player& pl) -> void {
            this->open(pl);
        });

        form->sendPage(player, 1);
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
        ll::command::CommandRegistrar::getInstance(false).tryRegisterSoftEnum(BlacklistObjectName, this->getBlacklists());

        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance(false)
            .getOrCreateCommand("blacklist", tr({}, "commands.blacklist.description"), CommandPermissionLevel::GameDirectors, CommandFlagValue::NotCheat | CommandFlagValue::Async);
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
            if (!this->hasBlacklist(param.Object))
                return output.error(fmt::runtime(tr({}, "commands.blacklist.error.remove")), param.Object);
            
            this->delBlacklist(param.Object);

            output.success(fmt::runtime(tr({}, "commands.blacklist.success.remove")), param.Object);
        });
        command.overload<operation>().text("info").required("Object").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->get("Blacklist", param.Object);
            
            if (mEvent.empty())
                return output.error(tr({}, "commands.blacklist.error.info"));

            output.success(tr({}, "commands.blacklist.success.info"));
            std::for_each(mEvent.begin(), mEvent.end(), [&output](const std::pair<std::string, std::string>& mPair) {
                std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                output.success("{0}: {1}", mKey, mPair.second);
            });
        });
        command.overload<operation>().text("list").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mObjectList = this->getBlacklists(param.Limit);
            
            if (mObjectList.empty())
                return output.success(fmt::runtime(tr({}, "commands.blacklist.success.list")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.blacklist.success.list")), param.Limit, fmt::join(mObjectList, ", "));
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
        this->mImpl->NetworkPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketBeforeEvent>([this](LOICollection::ServerEvents::NetworkPacketBeforeEvent& event) -> void {
            if (event.getPacket().getId() != MinecraftPacketIds::Login)
                return;

            auto packet = static_cast<LoginPacket const&>(event.getPacket());

            std::string mUuid = packet.mConnectionRequest->mCertificateData->mRawToken.mDataInfo.get("extraData", {}).get("identity", "None").asString("None");
            std::string mIp = event.getNetworkIdentifier().getIPAndPort().substr(0, event.getNetworkIdentifier().getIPAndPort().find_last_of(':'));
            std::string mClientId = packet.mConnectionRequest->getDeviceId();

            std::string mId = this->getDatabase()->find("Blacklist", {
                { "data_uuid", mUuid },
                { "data_ip", mIp },
                { "data_clientid", mClientId }
            }, "", SQLiteStorage::FindCondition::OR);

            if (mId.empty())
                return;

            std::unordered_map<std::string, std::string> mData = this->getDatabase()->get("Blacklist", mId);

            if (SystemUtils::isPastOrPresent(mData.at("time")))
                return this->delBlacklist(mId);

            std::string mObjectTips = tr(LanguagePlugin::getInstance().getLanguage(mUuid), "blacklist.tips");
            ll::service::getServerNetworkHandler()->disconnectClientWithMessage(
                event.getNetworkIdentifier(), event.getSubClientId(), Connection::DisconnectFailReason::Kicked,
                fmt::format(fmt::runtime(mObjectTips),
                    SystemUtils::toFormatTime(mData.at("time"), "None"), mData.at("cause")
                ),
                std::nullopt, false
            );
        });

        this->mImpl->BlacklistAddEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::BlacklistAddEvent>([this](LOICollection::ServerEvents::BlacklistAddEvent& event) -> void {
            std::string mId = this->getBlacklist(event.self());

            if (mId.empty())
                return;

            ll::command::CommandRegistrar::getInstance(false).addSoftEnumValues(BlacklistObjectName, { mId });
        });

        this->mImpl->BlacklistRemoveEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::BlacklistRemoveEvent>([](LOICollection::ServerEvents::BlacklistRemoveEvent& event) -> void {
            ll::command::CommandRegistrar::getInstance(false).removeSoftEnumValues(BlacklistObjectName, { event.getTarget() });
        });
    }

    void BlacklistPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->NetworkPacketEventListener);
        eventBus.removeListener(this->mImpl->BlacklistAddEventListener);
        eventBus.removeListener(this->mImpl->BlacklistRemoveEventListener);
    }

    void BlacklistPlugin::addBlacklist(Player& player, const std::string& cause, int time) {
        if (!this->isValid() || player.getCommandPermissionLevel() >= CommandPermissionLevel::GameDirectors)
            return;

        std::string mCause = cause.empty() ? "None" : cause;
        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

        std::unordered_map<std::string, std::string> mData = {
            { "name", player.getRealName() },
            { "cause", mCause },
            { "time", time ? SystemUtils::toTimeCalculate(SystemUtils::getNowTime(), time * 3600, "None") : "None" },
            { "subtime", SystemUtils::getNowTime("%Y%m%d%H%M%S") },
            { "data_uuid", player.getUuid().asString() },
            { "data_ip", player.getIPAndPort().substr(0, player.getIPAndPort().find_last_of(':')) },
            { "data_clientid", player.getConnectionRequest()->getDeviceId() }
        };

        this->getDatabase()->set("Blacklist", mTismestamp, mData);

        std::string mObjectTips = tr(LanguagePlugin::getInstance().getLanguage(player), "blacklist.tips");
        ll::service::getServerNetworkHandler()->disconnectClientWithMessage(
            player.getNetworkIdentifier(), player.getClientSubId(), Connection::DisconnectFailReason::Unknown,
            fmt::format(fmt::runtime(mObjectTips),
                SystemUtils::toFormatTime(this->getDatabase()->get("Blacklist", mTismestamp, "time"), "None"),
                mCause
            ),
            std::nullopt, false
        );

        if (this->mImpl->options.BroadcastMessage) {
            ll::service::getLevel()->forEachPlayer([&player](Player& mTarget) -> bool {
                std::string mMessage = tr(LanguagePlugin::getInstance().getLanguage(mTarget), "blacklist.broadcast");

                TextPacket::createRawMessage(
                    LOICollectionAPI::APIUtils::getInstance().translate(mMessage, player)
                ).sendTo(mTarget);

                return true;
            });
        }

        this->getLogger()->info(LOICollectionAPI::APIUtils::getInstance().translate(tr({}, "blacklist.log1"), player));
    }

    void BlacklistPlugin::delBlacklist(const std::string& id) {
        if (!this->isValid()) 
            return;

        if (!this->hasBlacklist(id)) {
            this->getLogger()->warn(fmt::runtime(tr({}, "console.log.error.object")), "BlacklistPlugin");

            return;
        }

        this->getDatabase()->del("Blacklist", id);

        this->getLogger()->info(fmt::runtime(tr({}, "blacklist.log2")), id);
    }

    std::string BlacklistPlugin::getBlacklist(Player& player) {
        if (!this->isValid())
            return {};

        return this->getDatabase()->find("Blacklist", {
            { "data_uuid", player.getUuid().asString() },
            { "data_ip", player.getIPAndPort().substr(0, player.getIPAndPort().find_last_of(':')) },
            { "data_clientid", player.getConnectionRequest()->getDeviceId() }
        }, "", SQLiteStorage::FindCondition::OR);
    }

    std::vector<std::string> BlacklistPlugin::getBlacklists(int limit) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mKeys = this->getDatabase()->list("Blacklist");

        return mKeys
            | std::views::take(limit > 0 ? limit : static_cast<int>(mKeys.size()))
            | std::ranges::to<std::vector<std::string>>();
    }

    bool BlacklistPlugin::hasBlacklist(const std::string& id) {
        if (!this->isValid())
            return false;

        return this->getDatabase()->has("Blacklist", id);
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
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Blacklist.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_shared<SQLiteStorage>((mDataPath / "blacklist.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.Blacklist;

        return true;
    }

    bool BlacklistPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool BlacklistPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->getDatabase()->create("Blacklist", [](SQLiteStorage::ColumnCallback ctor) -> void {
            ctor("name");
            ctor("cause");
            ctor("time");
            ctor("subtime");
            ctor("data_uuid");
            ctor("data_ip");
            ctor("data_clientid");
        });
        
        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool BlacklistPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("BlacklistPlugin", LOICollection::Plugins::BlacklistPlugin, LOICollection::Plugins::BlacklistPlugin::getInstance(), LOICollection::modules::ModulePriority::High)
