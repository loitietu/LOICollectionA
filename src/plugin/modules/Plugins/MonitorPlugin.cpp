#include <memory>
#include <ranges>
#include <string>
#include <algorithm>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerConnectEvent.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/scores/Objective.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/level/Level.h>
#include <mc/world/actor/DataItem.h>
#include <mc/world/actor/ActorDataIDs.h>
#include <mc/world/actor/SynchedActorDataEntityWrapper.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/NetworkIdentifier.h>
#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/SetActorDataPacket.h>
#include <mc/network/packet/AvailableCommandsPacket.h>

#include <mc/server/ServerPlayer.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include <mc/common/SubClientId.h>

#include "include/RegistryHelper.h"

#include "include/APIUtils.h"

#include "include/ServerEvents/NetworkPacketEvent.h"
#include "include/ServerEvents/PlayerScoreChangedEvent.h"

#include "utils/ScoreboardUtils.h"

#include "base/Wrapper.h"
#include "base/ServiceProvider.h"

#include "ConfigPlugin.h"

#include "include/Plugins/MonitorPlugin.h"

std::vector<std::string> mInterceptTextObjectPacket;

namespace LOICollection::Plugins {
    struct MonitorPlugin::Impl {
        C_Config::C_Plugins::C_Monitor options;

        ll::event::ListenerPtr PlayerConnectEventListener;
        ll::event::ListenerPtr PlayerDisconnectEventListener;
        ll::event::ListenerPtr PlayerScoreChangedEventListener;
        ll::event::ListenerPtr ExecuteCommandEventListener;
        ll::event::ListenerPtr NetworkPacketEventCommandListener;

        ll::event::ListenerPtr NetworkPacketEventTextListener;

        bool BelowNameTaskRunning = true;
    };

    MonitorPlugin::MonitorPlugin() : mImpl(std::make_unique<Impl>()) {};
    MonitorPlugin::~MonitorPlugin() = default;

    void MonitorPlugin::listenEvent() {
        if (this->mImpl->options.BelowName.ModuleEnabled) {
            ll::coro::keepThis([this, option = this->mImpl->options.BelowName]() -> ll::coro::CoroTask<> {
                while (this->mImpl->BelowNameTaskRunning) {
                    co_await ll::chrono::ticks(option.RefreshInterval);

                    ll::service::getLevel()->forEachPlayer([mName = option.FormatText](Player& mTarget) -> bool {
                        if (mTarget.isSimulatedPlayer())
                            return true;

                        std::string mNameTag = mName;

                        LOICollectionAPI::translateString(mNameTag, mTarget);

                        SetActorDataPacket packet(mTarget.getRuntimeID(), mTarget.mEntityData, 
                            nullptr, mTarget.mLevel->getCurrentTick().tickID, false
                        );
                        packet.mPackedItems.clear();
                        packet.mPackedItems.emplace_back(DataItem::create(ActorDataIDs::FilteredName, mNameTag));
                        packet.sendToClients();

                        return true;
                    });
                }
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
        }

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerConnectEventListener = eventBus.emplaceListener<ll::event::PlayerConnectEvent>([option = this->mImpl->options.ServerToast](ll::event::PlayerConnectEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled)
                return;

            std::string mMessage = option.FormatText.join;

            LOICollectionAPI::translateString(mMessage, event.self());
            TextPacket::createSystemMessage(mMessage).sendToClients();

            mInterceptTextObjectPacket.push_back(event.self().getUuid().asString());
        });

        this->mImpl->PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([option = this->mImpl->options.ServerToast](ll::event::PlayerDisconnectEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled)
                return;

            std::string mMessage = option.FormatText.exit;

            LOICollectionAPI::translateString(mMessage, event.self());
            TextPacket::createSystemMessage(mMessage).sendToClients();

            mInterceptTextObjectPacket.erase(std::remove(mInterceptTextObjectPacket.begin(), mInterceptTextObjectPacket.end(), event.self().getUuid().asString()), mInterceptTextObjectPacket.end());
        });

        this->mImpl->PlayerScoreChangedEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerScoreChangedEvent>([option = this->mImpl->options.ChangeScore](LOICollection::ServerEvents::PlayerScoreChangedEvent& event) -> void {
            using LOICollection::ServerEvents::ScoreChangedType;

            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled || !event.getScore())
                return;

            int mScore = event.getScore();
            std::string mId = event.getObjective().mName;
            ScoreChangedType mType = event.getScoreChangedType();

            std::vector<std::string> mObjectScoreboards = option.ScoreboardLists;
            if (mObjectScoreboards.empty() || std::find(mObjectScoreboards.begin(), mObjectScoreboards.end(), mId) != mObjectScoreboards.end()) {
                int mOriScore = ScoreboardUtils::getScore(event.self(), mId);

                std::string mMessage = fmt::format(fmt::runtime(option.FormatText), mId,
                    (mType == ScoreChangedType::add ? (mOriScore - mScore) : (mType == ScoreChangedType::reduce ? (mOriScore + mScore) : mScore)),
                    (mType == ScoreChangedType::add ? "+" : (mType == ScoreChangedType::reduce ? "-" : "")) + std::to_string(mScore),
                    mOriScore
                );

                LOICollectionAPI::translateString(mMessage, event.self());
                
                event.self().sendMessage(mMessage);
            }
        });

        this->mImpl->ExecuteCommandEventListener = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>([option = this->mImpl->options.DisableCommand](ll::event::ExecutingCommandEvent& event) -> void {
            std::string mCommand = event.commandContext().mCommand.substr(1);

            if (!option.ModuleEnabled || event.commandContext().mOrigin == nullptr || mCommand.empty())
                return;

            auto mCommandArgs = mCommand | std::views::split(' ');
            auto mCommandName = *mCommandArgs.begin();

            std::vector<std::string> mObjectCommands = option.CommandLists;
            if (std::find(mObjectCommands.begin(), mObjectCommands.end(), std::string(mCommandName.begin(), mCommandName.end())) != mObjectCommands.end()) {
                event.cancel();

                Actor* entity = event.commandContext().mOrigin->getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return;
                auto player = static_cast<Player*>(entity);

                player->sendMessage(LOICollectionAPI::translateString(option.FormatText, *player));
            }
        });

        this->mImpl->NetworkPacketEventCommandListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([option = this->mImpl->options.DisableCommand](LOICollection::ServerEvents::NetworkPacketEvent& event) -> void {
            if (!option.ModuleEnabled || event.getPacket().getId() != MinecraftPacketIds::AvailableCommands)
                return;

            auto& packet = static_cast<AvailableCommandsPacket&>(const_cast<Packet&>(event.getPacket()));

            std::vector<std::string> mObjectCommands = option.CommandLists;

            std::vector<size_t> mCommandsToRemove;
            for (size_t i = 0; i < packet.mCommands->size(); ++i) {
                if (std::find(mObjectCommands.begin(), mObjectCommands.end(), *packet.mCommands->at(i).name) == mObjectCommands.end()) 
                    continue;
                mCommandsToRemove.push_back(i);
            }

            std::sort(mCommandsToRemove.rbegin(), mCommandsToRemove.rend());

            for (size_t i : mCommandsToRemove) {
                if (i >= packet.mCommands->size()) 
                    continue;

                packet.mCommands->at(i).$dtor();
                if (i < packet.mCommands->size() - 1) {
                    new (&packet.mCommands->at(i)) AvailableCommandsPacket::CommandData(std::move(packet.mCommands->back()));
                    packet.mCommands->back().$dtor();
                }
                packet.mCommands->pop_back();
            }
        });

        std::vector<std::string> mTextPacketType{"multiplayer.player.joined", "multiplayer.player.left"};
        this->mImpl->NetworkPacketEventTextListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkBroadcastPacketEvent>([mTextPacketType, option = this->mImpl->options.ServerToast](LOICollection::ServerEvents::NetworkBroadcastPacketEvent& event) -> void {
            if (!option.ModuleEnabled || event.getPacket().getId() != MinecraftPacketIds::Text)
                return;

            auto packet = static_cast<TextPacket const&>(event.getPacket());
            
            bool result = std::any_of(mTextPacketType.begin(), mTextPacketType.end(), [target = packet.mMessage](const std::string& item) {
                return target.find(item) != std::string::npos;
            });

            if (!result || !(packet.params.size() > 0))
                return;
            
            if (Player* player = ll::service::getLevel()->getPlayer(packet.params.at(0)); player) {
                std::string mUuid = player->getUuid().asString();
                if (std::find(mInterceptTextObjectPacket.begin(), mInterceptTextObjectPacket.end(), mUuid) != mInterceptTextObjectPacket.end())
                    event.cancel();
            }
        });
    }

    void MonitorPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerConnectEventListener);
        eventBus.removeListener(this->mImpl->PlayerDisconnectEventListener);
        eventBus.removeListener(this->mImpl->PlayerScoreChangedEventListener);
        eventBus.removeListener(this->mImpl->ExecuteCommandEventListener);
        eventBus.removeListener(this->mImpl->NetworkPacketEventCommandListener);

        eventBus.removeListener(this->mImpl->NetworkPacketEventTextListener);

        this->mImpl->BelowNameTaskRunning = false;
    }

    bool MonitorPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Monitor.ModuleEnabled)
            return false;

        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.Monitor;

        return true;
    }

    bool MonitorPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->listenEvent();

        return true;
    }

    bool MonitorPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        return true;
    }
}

REGISTRY_HELPER("MonitorPlugin", LOICollection::Plugins::MonitorPlugin, LOICollection::Plugins::MonitorPlugin::getInstance())
