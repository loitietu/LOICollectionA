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
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include "Include/APIUtils.h"
#include "Include/HookPlugin.h"

#include "Utils/toolUtils.h"

#include "Include/monitorPlugin.h"

using namespace ll::chrono_literals;

namespace LOICollection::Plugins::monitor {
    std::vector<std::string> mObjectCommands;
    std::map<std::string, std::variant<std::string, std::vector<std::string>>> mObjectOptions;
    
    ll::event::ListenerPtr PlayerJoinEventListener;
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
                    toolUtils::broadcastText(mMonitorString);
                }
            );
            ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
                [](ll::event::ExecutingCommandEvent& event) {
                    std::string mCommand = ll::string_utils::replaceAll(toolUtils::split(event.commandContext().mCommand, " ")[0], "/", "");
                    if (std::find(mObjectCommands.begin(), mObjectCommands.end(), mCommand) != mObjectCommands.end()) {
                        event.cancel();

                        auto* entity = event.commandContext().getCommandOrigin().getEntity();
                        if (entity == nullptr || !entity->isType(ActorType::Player))
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
            LOICollection::HookPlugin::Event::onPlayerScoreChangedEvent([](void* player_ptr, int score, std::string id) {
                Player* player = static_cast<Player*>(player_ptr);
                std::string target = std::get<std::string>(mObjectOptions.at("target"));
                if (id == target) {
                    std::string mChangedString = std::get<std::string>(mObjectOptions.at("changed"));
                    ll::string_utils::replaceAll(mChangedString, "${GetScore}", std::to_string(score));
                    player->sendMessage(mChangedString);
                }
            });

            static ll::schedule::GameTickAsyncScheduler scheduler;
            scheduler.add<ll::schedule::RepeatTask>(3_tick, [] {
                for (auto& player : toolUtils::getAllPlayers()) {
                    std::string mMonitorString = std::get<std::string>(mObjectOptions.at("show"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, player);
                    player->setNameTag(mMonitorString);
                    player->_sendDirtyActorData();
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
        eventBus.removeListener(ExecuteCommandEvent);
    }
}