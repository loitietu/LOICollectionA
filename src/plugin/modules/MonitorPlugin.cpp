#include <memory>
#include <string>
#include <algorithm>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/StringUtils.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/scores/Objective.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/level/Level.h>
#include <mc/world/actor/DataItem.h>
#include <mc/world/actor/ActorDataIDs.h>
#include <mc/world/actor/SynchedActorDataEntityWrapper.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/SetActorDataPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include "include/APIUtils.h"

#include "include/ServerEvents/TextPacketEvent.h"
#include "include/ServerEvents/LoginPacketEvent.h"
#include "include/ServerEvents/PlayerScoreChangedEvent.h"

#include "utils/ScoreboardUtils.h"

#include "ConfigPlugin.h"

#include "include/MonitorPlugin.h"

std::vector<std::string> mInterceptTextObjectPacket;

namespace LOICollection::Plugins::monitor {
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerDisconnectEventListener;
    ll::event::ListenerPtr PlayerScoreChangedEventListener;
    ll::event::ListenerPtr LoginPacketEventListener;
    ll::event::ListenerPtr ExecuteCommandEventListener;

    ll::event::ListenerPtr TextPacketEventListener;

    bool BelowNameTaskRunning = true;

    void registery() {
        C_Config::C_Plugins::C_Monitor& options = Config::GetBaseConfigContext().Plugins.Monitor;

        if (options.BelowName.ModuleEnabled) {
            ll::coro::keepThis([option = options.BelowName]() -> ll::coro::CoroTask<> {
                while (BelowNameTaskRunning) {
                    co_await ll::chrono::ticks(option.RefreshInterval);

                    ll::service::getLevel()->forEachPlayer([mName = option.FormatText](Player& mTarget) -> bool {
                        if (mTarget.isSimulatedPlayer())
                            return true;

                        LOICollectionAPI::translateString(mName, mTarget);

                        SetActorDataPacket packet(mTarget.getRuntimeID(), mTarget.mEntityData, 
                            nullptr, mTarget.mLevel->getCurrentTick().tickID, false
                        );
                        packet.mPackedItems.clear();
                        packet.mPackedItems.emplace_back(DataItem::create(ActorDataIDs::FilteredName, mName));
                        packet.sendToClients();

                        return true;
                    });
                }
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
        }

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>([option = options.ServerToast](ll::event::PlayerJoinEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled)
                return;

            std::string mMessage = option.FormatText.join;

            LOICollectionAPI::translateString(mMessage, event.self());
            TextPacket::createSystemMessage(mMessage).sendToClients();
        });

        PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([option = options.ServerToast](ll::event::PlayerDisconnectEvent& event) -> void {
            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled)
                return;

            std::string mMessage = option.FormatText.exit;

            LOICollectionAPI::translateString(mMessage, event.self());
            TextPacket::createSystemMessage(mMessage).sendToClients();

            mInterceptTextObjectPacket.erase(std::remove(mInterceptTextObjectPacket.begin(), mInterceptTextObjectPacket.end(), event.self().getUuid().asString()), mInterceptTextObjectPacket.end());
        });

        PlayerScoreChangedEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerScoreChangedEvent>([option = options.ChangeScore](LOICollection::ServerEvents::PlayerScoreChangedEvent& event) -> void {
            using LOICollection::ServerEvents::ScoreChangedType;

            if (event.self().isSimulatedPlayer() || !option.ModuleEnabled || !event.getScore())
                return;

            int mScore = event.getScore();
            std::string mId = event.getObjective().mName;
            ScoreChangedType mType = event.getScoreChangedType();

            std::vector<std::string> mObjectScoreboards = option.ScoreboardLists;
            if (mObjectScoreboards.empty() || std::find(mObjectScoreboards.begin(), mObjectScoreboards.end(), mId) != mObjectScoreboards.end()) {
                int mOriScore = ScoreboardUtils::getScore(event.self(), mId);

                std::string mMessage = option.FormatText;
                ll::string_utils::replaceAll(mMessage, "${Object}", mId);
                ll::string_utils::replaceAll(mMessage, "${OriMoney}", 
                    (mType == ScoreChangedType::add ? std::to_string(mOriScore - mScore) :
                        (mType == ScoreChangedType::reduce ? std::to_string(mOriScore + mScore) : std::to_string(mScore)))
                );
                ll::string_utils::replaceAll(mMessage, "${SetMoney}", 
                    (mType == ScoreChangedType::add ? "+" : (mType == ScoreChangedType::reduce ? "-" : "")) + std::to_string(mScore)
                );
                ll::string_utils::replaceAll(mMessage, "${GetMoney}", std::to_string(mOriScore));

                LOICollectionAPI::translateString(mMessage, event.self());
                
                event.self().sendMessage(mMessage);
            }
        });

        ExecuteCommandEventListener = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>([option = options.DisableCommand](ll::event::ExecutingCommandEvent& event) -> void {
            std::string mCommandOriginal = event.commandContext().mCommand;
            std::string mCommand = ll::string_utils::replaceAll(mCommandOriginal, "/", "");

            if (!option.ModuleEnabled || event.commandContext().mOrigin == nullptr || mCommand.empty())
                return;

            std::vector<std::string> mObjectCommands = option.CommandLists;
            if (std::find(mObjectCommands.begin(), mObjectCommands.end(), ll::string_utils::splitByPattern(mCommand, " ")[0]) != mObjectCommands.end()) {
                event.cancel();

                Actor* entity = event.commandContext().mOrigin->getEntity();
                if (entity == nullptr || !entity->isPlayer())
                    return;
                auto player = static_cast<Player*>(entity);

                player->sendMessage(LOICollectionAPI::translateString(option.FormatText, *player));
            }
        });

        LoginPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::LoginPacketEvent>([option = options.ServerToast](LOICollection::ServerEvents::LoginPacketEvent& event) -> void {
            if (option.ModuleEnabled)
                mInterceptTextObjectPacket.push_back(event.getUUID().asString());
        });

        std::vector<std::string> mTextPacketType{"multiplayer.player.joined", "multiplayer.player.left"};
        TextPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::TextPacketEvent>([mTextPacketType, option = options.ServerToast](LOICollection::ServerEvents::TextPacketEvent& event) -> void {
            if (!option.ModuleEnabled)
                return;
            
            bool result = std::any_of(mTextPacketType.begin(), mTextPacketType.end(), [target = event.getPacket().mMessage](const std::string& item) {
                return target.find(item) != std::string::npos;
            });

            if (!result || !(event.getPacket().params.size() > 0))
                return;
            
            if (Player* player = ll::service::getLevel()->getPlayer(event.getPacket().params.at(0)); player) {
                std::string mUuid = player->getUuid().asString();
                if (std::find(mInterceptTextObjectPacket.begin(), mInterceptTextObjectPacket.end(), mUuid) != mInterceptTextObjectPacket.end())
                    event.cancel();
            }
        });
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerDisconnectEventListener);
        eventBus.removeListener(PlayerScoreChangedEventListener);
        eventBus.removeListener(ExecuteCommandEventListener);
        eventBus.removeListener(LoginPacketEventListener);

        eventBus.removeListener(TextPacketEventListener);

        BelowNameTaskRunning = false;
    }
}