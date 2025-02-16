#include <map>
#include <string>
#include <vector>
#include <variant>
#include <algorithm>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/utils/StringUtils.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/scores/Objective.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>

#include <mc/network/packet/TextPacket.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include "include/APIUtils.h"
#include "include/ServerEvents/TextPacketEvent.h"
#include "include/ServerEvents/LoginPacketEvent.h"
#include "include/ServerEvents/PlayerScoreChangedEvent.h"

#include "utils/McUtils.h"

#include "include/MonitorPlugin.h"

std::vector<std::string> mInterceptTextObjectPacket;
std::vector<std::string> mInterceptTextObjectName;

LL_TYPE_INSTANCE_HOOK(
    InterceptGetNameTagHook,
    HookPriority::Normal,
    Actor,
    &Actor::getNameTag,
    std::string const&
) {
    if (!this->isPlayer() || mInterceptTextObjectName.empty())
        return origin();

    auto player = (Player*) this;
    if (std::find(mInterceptTextObjectName.begin(), mInterceptTextObjectName.end(), player->getUuid().asString()) != mInterceptTextObjectName.end()) {
        static std::string mName;
        mName = player->getRealName();
        return mName;
    }
    return origin();
};

namespace LOICollection::Plugins::monitor {
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerDisconnectEventListener;
    ll::event::ListenerPtr PlayerScoreChangedEventListener;
    ll::event::ListenerPtr LoginPacketEventListener;
    ll::event::ListenerPtr ExecuteCommandEvent;

    ll::event::ListenerPtr TextPacketEventListener;

    bool BelowNameTaskRunning = true;

    void registery(std::map<std::string, std::variant<std::string, std::vector<std::string>, int, bool>>& options) {
        if (std::get<bool>(options.at("BelowName_Enabled"))) {
            ll::coro::keepThis([options]() -> ll::coro::CoroTask<> {
                while (BelowNameTaskRunning) {
                    co_await ll::chrono::ticks(std::get<int>(options.at("BelowName_RefreshInterval")));
                    for (Player*& player : McUtils::getAllPlayers()) {
                        std::string mMonitorString = std::get<std::string>(options.at("BelowName_Text"));
                        LOICollection::LOICollectionAPI::translateString(mMonitorString, *player);
                        player->setNameTag(mMonitorString);
                    }
                }
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
        }

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
            [options](ll::event::PlayerJoinEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                if (std::get<bool>(options.at("ServerToast_Enabled"))) {
                    std::string mMonitorString = std::get<std::string>(options.at("ServerToast_JoinText"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, event.self());
                    McUtils::broadcastText(mMonitorString);
                }
            }
        );
        PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
            [options](ll::event::PlayerDisconnectEvent& event) -> void {
                if (event.self().isSimulatedPlayer())
                    return;

                std::string mUuid = event.self().getUuid().asString();
                if (std::get<bool>(options.at("ServerToast_Enabled"))) {
                    std::string mMonitorString = std::get<std::string>(options.at("ServerToast_ExitText"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, event.self());
                    McUtils::broadcastText(mMonitorString);

                    mInterceptTextObjectPacket.erase(std::remove(mInterceptTextObjectPacket.begin(), mInterceptTextObjectPacket.end(), mUuid), mInterceptTextObjectPacket.end());
                }
                if (std::get<bool>(options.at("BelowName_Enabled")))
                    mInterceptTextObjectName.erase(std::remove(mInterceptTextObjectName.begin(), mInterceptTextObjectName.end(), mUuid), mInterceptTextObjectName.end());
            }
        );
        std::vector<std::string> mObjectScoreboards = std::get<std::vector<std::string>>(options.at("ChangeScore_Scores"));
        PlayerScoreChangedEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::PlayerScoreChangedEvent>(
            [options, mObjectScoreboards](LOICollection::ServerEvents::PlayerScoreChangedEvent& event) -> void {
                using LOICollection::ServerEvents::ScoreChangedType;

                if (event.self().isSimulatedPlayer() || !std::get<bool>(options.at("ChangeScore_Enabled")) || (event.getScoreChangedType() != ScoreChangedType::set && !event.getScore()))
                    return;

                int mScore = event.getScore();
                std::string mId = event.getObjective().getName();
                ScoreChangedType mType = event.getScoreChangedType();
                if (mObjectScoreboards.empty() || std::find(mObjectScoreboards.begin(), mObjectScoreboards.end(), mId) != mObjectScoreboards.end()) {
                    int mOriScore = McUtils::scoreboard::getScore(event.self(), mId);
                    std::string mChangedString = std::get<std::string>(options.at("ChangeScore_Text"));
                    ll::string_utils::replaceAll(mChangedString, "${Object}", mId);
                    ll::string_utils::replaceAll(mChangedString, "${OriMoney}", std::to_string(mOriScore));
                    ll::string_utils::replaceAll(mChangedString, "${SetMoney}", 
                        (mType == ScoreChangedType::add ? "+" : (mType == ScoreChangedType::reduce ? "-" : "")) + std::to_string(mScore)
                    );
                    ll::string_utils::replaceAll(mChangedString, "${GetMoney}", 
                        (mType == ScoreChangedType::add ? std::to_string(mOriScore + mScore) :
                        (mType == ScoreChangedType::reduce ? std::to_string(mOriScore - mScore) : std::to_string(mScore)))
                    );
                    event.self().sendMessage(mChangedString);
                }
            }
        );
        std::vector<std::string> mObjectCommands = std::get<std::vector<std::string>>(options.at("DisableCommand_List"));
        ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
            [mObjectCommands, options](ll::event::ExecutingCommandEvent& event) -> void {
                if (!std::get<bool>(options.at("DisableCommand_Enabled")))
                    return;

                std::string mCommand = ll::string_utils::replaceAll(std::string(ll::string_utils::splitByPattern(event.commandContext().mCommand, " ")[0]), "/", "");
                if (std::find(mObjectCommands.begin(), mObjectCommands.end(), mCommand) != mObjectCommands.end()) {
                    event.cancel();

                    Actor* entity = event.commandContext().getCommandOrigin().getEntity();
                    if (entity == nullptr || !entity->isPlayer())
                        return;
                    auto player = static_cast<Player*>(entity);
                    player->sendMessage(std::get<std::string>(options.at("DisableCommand_Text")));
                }
            }
        );
        LoginPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::LoginPacketEvent>(
            [options](LOICollection::ServerEvents::LoginPacketEvent& event) -> void {
                if (std::get<bool>(options.at("ServerToast_Enabled")))
                    mInterceptTextObjectPacket.push_back(event.getUUID().asString());
                if (std::get<bool>(options.at("BelowName_Enabled")))
                    mInterceptTextObjectName.push_back(event.getUUID().asString());
            }
        );

        std::vector<std::string> mTextPacketType{"multiplayer.player.joined", "multiplayer.player.left"};
        TextPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::TextPacketEvent>(
            [mTextPacketType](LOICollection::ServerEvents::TextPacketEvent& event) -> void {
                bool result = std::any_of(mTextPacketType.begin(), mTextPacketType.end(), [target = event.getPacket().mMessage](const std::string& item) {
                    return target.find(item) != std::string::npos;
                });

                if (!result || !(event.getPacket().params.size() > 0))
                    return;
                if (Player* player = ll::service::getLevel()->getPlayer(event.getPacket().params.at(0)); player) {
                    std::string mUuid = player->getUuid().asString();
                    if (std::find(mInterceptTextObjectName.begin(), mInterceptTextObjectName.end(), mUuid) != mInterceptTextObjectName.end())
                        event.cancel();
                }
            }
        );
        InterceptGetNameTagHook::hook();
    }

    void unregistery() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerDisconnectEventListener);
        eventBus.removeListener(PlayerScoreChangedEventListener);
        eventBus.removeListener(ExecuteCommandEvent);
        eventBus.removeListener(LoginPacketEventListener);

        eventBus.removeListener(TextPacketEventListener);
        InterceptGetNameTagHook::unhook();

        BelowNameTaskRunning = false;
    }
}