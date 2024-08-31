#include <map>
#include <vector>
#include <string>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/command/ExecuteCommandEvent.h>

#include <mc/world/actor/player/Player.h>
#include <mc/entity/utilities/ActorType.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandContext.h>

#include "../Include/API.hpp"

#include "../Utils/toolUtils.h"

#include "../Include/monitorPlugin.h"

namespace monitorPlugin {
    std::vector<std::string> mObjectCommands;
    std::map<std::string, std::string> mObjectOptions;
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr ExecuteCommandEvent;

    namespace {
        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer()) return;
                    std::string mMonitorString = LOICollectionAPI::translateString(mObjectOptions.at("join"), &event.self(), false);
                    toolUtils::broadcastText(mMonitorString);
                }
            );
            ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
                [](ll::event::ExecutingCommandEvent& event) {
                    std::string mCommand = toolUtils::replaceString(toolUtils::split(event.commandContext().mCommand, ' ')[0], "/", "");
                    if (std::find(mObjectCommands.begin(), mObjectCommands.end(), mCommand) != mObjectCommands.end()) {
                        event.cancel();

                        auto* entity = event.commandContext().getCommandOrigin().getEntity();
                        if (entity == nullptr || !entity->isType(ActorType::Player))
                            return;
                        Player* player = static_cast<Player*>(entity);
                        player->sendMessage(mObjectOptions.at("tips"));
                    }
                }
            );
        }
    }

    void registery(std::map<std::string, std::string>& options, std::vector<std::string>& commands) {
        mObjectOptions = options;
        mObjectCommands = commands;
        listenEvent();
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(PlayerJoinEventListener);
        eventBus.removeListener(ExecuteCommandEvent);
    }
}