#include <map>
#include <string>
#include <vector>
#include <variant>

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

namespace monitorPlugin {
    std::vector<std::string> mObjectCommands;
    std::map<std::string, std::variant<std::string, std::vector<std::string>>> mObjectOptions;
    
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr ExecuteCommandEvent;

    namespace {
        void listenEvent() {
            auto& eventBus = ll::event::EventBus::getInstance();
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer()) return;
                    std::string mMonitorString = std::get<std::string>(mObjectOptions.at("join"));
                    LOICollectionAPI::translateString2(mMonitorString, &event.self(), false);
                    toolUtils::broadcastText(mMonitorString);
                }
            );
            ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
                [](ll::event::ExecutingCommandEvent& event) {
                    std::string mCommand = toolUtils::replaceString(toolUtils::split(event.commandContext().mCommand, " ")[0], "/", "");
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
            HookPlugin::Event::onPlayerScoreChangedEvent([](void* player_ptr, int score, std::string id) -> void {
                Player* player = static_cast<Player*>(player_ptr);

                if (player == nullptr) {
                    return;
                }

                std::string target = std::get<std::string>(mObjectOptions.at("target"));
                if (id == target) {
                    std::string mChangedString = std::get<std::string>(mObjectOptions.at("changed"));
                    toolUtils::replaceString2(mChangedString, "${GetScore}", std::to_string(score));
                    player->sendMessage(mChangedString);
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