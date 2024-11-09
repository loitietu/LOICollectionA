#include <map>
#include <string>
#include <vector>
#include <variant>

#include <ll/api/schedule/Task.h>
#include <ll/api/schedule/Scheduler.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/utils/StringUtils.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerLeaveEvent.h>
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include "include/APIUtils.h"
#include "include/HookPlugin.h"

#include "utils/McUtils.h"

#include "include/monitorPlugin.h"

using namespace ll::chrono_literals;

namespace LOICollection::Plugins::monitor {
    std::vector<std::string> mObjectCommands;
    std::map<std::string, std::variant<std::string, std::vector<std::string>>> mObjectOptions;
    
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerLeaveEventListener;
    ll::event::ListenerPtr ExecuteCommandEvent;

    namespace {
        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mMonitorString = std::get<std::string>(mObjectOptions.at("join"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, &event.self());
                    McUtils::broadcastText(mMonitorString);
                }
            );
            PlayerLeaveEventListener = eventBus.emplaceListener<ll::event::PlayerLeaveEvent>(
                [](ll::event::PlayerLeaveEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mMonitorString = std::get<std::string>(mObjectOptions.at("exit"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, &event.self());
                    McUtils::broadcastText(mMonitorString);
                }
            );
            ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
                [](ll::event::ExecutingCommandEvent& event) {
                    std::string mCommand = ll::string_utils::replaceAll(std::string(ll::string_utils::splitByPattern(event.commandContext().mCommand, " ")[0]), "/", "");
                    if (std::find(mObjectCommands.begin(), mObjectCommands.end(), mCommand) != mObjectCommands.end()) {
                        event.cancel();

                        auto* entity = event.commandContext().getCommandOrigin().getEntity();
                        if (entity == nullptr || !entity->isPlayer())
                            return;
                        Player* player = static_cast<Player*>(entity);
                        player->sendMessage(std::get<std::string>(mObjectOptions.at("tips")));
                    }
                }
            );
            LOICollection::HookPlugin::Event::onLoginPacketSendEvent([](void* /*unused*/, std::string mUuid, std::string /*unused*/) {
                LOICollection::HookPlugin::interceptTextPacket(mUuid);
                LOICollection::HookPlugin::interceptGetNameTag(mUuid);
            });
            LOICollection::HookPlugin::Event::onPlayerDisconnectBeforeEvent([](std::string mUuid) {
                LOICollection::HookPlugin::interceptTextPacket(mUuid);
                LOICollection::HookPlugin::interceptGetNameTag(mUuid);
            });
            LOICollection::HookPlugin::Event::onPlayerScoreChangedEvent([](void* player_ptr, int score, std::string id, int type) {
                Player* player = static_cast<Player*>(player_ptr);
                std::string target = std::get<std::string>(mObjectOptions.at("target"));
                if (id.empty() || player == nullptr)
                    return;
                if (id == target || target == "$all") {
                    int mOriScore = McUtils::scoreboard::getScore(player, id);
                    std::string mChangedString = std::get<std::string>(mObjectOptions.at("changed"));
                    ll::string_utils::replaceAll(mChangedString, "${Object}", id);
                    ll::string_utils::replaceAll(mChangedString, "${OriMoney}", std::to_string(mOriScore));
                    ll::string_utils::replaceAll(mChangedString, "${SetMoney}", 
                        (type == SCORECHANGED_ADD ? "+" : (type == SCORECHANGED_REDUCE ? "-" : "")) + std::to_string(score)
                    );
                    ll::string_utils::replaceAll(mChangedString, "${GetMoney}", 
                        (type == SCORECHANGED_ADD ? std::to_string(mOriScore + score) :
                        (type == SCORECHANGED_REDUCE ? std::to_string(mOriScore - score) : std::to_string(score)))
                    );
                    player->sendMessage(mChangedString);
                }
            });

            static ll::schedule::GameTickAsyncScheduler scheduler;
            scheduler.add<ll::schedule::RepeatTask>(3_tick, [] {
                for (auto& player : McUtils::getAllPlayers()) {
                    std::string mMonitorString = std::get<std::string>(mObjectOptions.at("show"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, player);
                    player->setNameTag(mMonitorString);
                }
            });
        }
    }

    void registery(std::map<std::string, std::variant<std::string, std::vector<std::string>>>& options) {
        mObjectCommands = std::get<std::vector<std::string>>(options.at("command"));
        mObjectOptions = options;
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(PlayerLeaveEventListener);
        eventBus.removeListener(ExecuteCommandEvent);
    }
}