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

static ll::schedule::GameTickAsyncScheduler scheduler;

namespace LOICollection::Plugins::monitor {
    ll::event::ListenerPtr PlayerJoinEventListener;
    ll::event::ListenerPtr PlayerLeaveEventListener;
    ll::event::ListenerPtr ExecuteCommandEvent;

    void registery(std::map<std::string, std::variant<std::string, std::vector<std::string>, int, bool>>& options) {
        if (std::get<bool>(options.at("BelowName_Enabled"))) {
            scheduler.add<ll::schedule::RepeatTask>(ll::chrono_literals::operator""_tick(std::get<int>(options.at("BelowName_RefreshInterval"))), [options] {
                for (Player*& player : McUtils::getAllPlayers()) {
                    std::string mMonitorString = std::get<std::string>(options.at("BelowName_Text"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, *player);
                    player->setNameTag(mMonitorString);
                }
            });
        }
        auto& eventBus = ll::event::EventBus::getInstance();
        if (std::get<bool>(options.at("ServerToast_Enabled"))) {
            PlayerJoinEventListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
                [options](ll::event::PlayerJoinEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mMonitorString = std::get<std::string>(options.at("ServerToast_JoinText"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, event.self());
                    McUtils::broadcastText(mMonitorString);
                }
            );
            PlayerLeaveEventListener = eventBus.emplaceListener<ll::event::PlayerLeaveEvent>(
                [options](ll::event::PlayerLeaveEvent& event) {
                    if (event.self().isSimulatedPlayer())
                        return;
                    std::string mMonitorString = std::get<std::string>(options.at("ServerToast_ExitText"));
                    LOICollection::LOICollectionAPI::translateString(mMonitorString, event.self());
                    McUtils::broadcastText(mMonitorString);
                }
            );
        }
        if (std::get<bool>(options.at("ChangeScore_Enabled"))) {
            std::vector<std::string> mObjectScoreboards = std::get<std::vector<std::string>>(options.at("ChangeScore_Scores"));
            LOICollection::HookPlugin::Event::onPlayerScoreChangedEvent([options, mObjectScoreboards](Player* player, int score, std::string id, ScoreChangedType type) {
                if (id.empty() || player == nullptr)
                    return;
                if (mObjectScoreboards.empty() || std::find(mObjectScoreboards.begin(), mObjectScoreboards.end(), id) != mObjectScoreboards.end()) {
                    int mOriScore = McUtils::scoreboard::getScore(*player, id);
                    std::string mChangedString = std::get<std::string>(options.at("ChangeScore_Text"));
                    ll::string_utils::replaceAll(mChangedString, "${Object}", id);
                    ll::string_utils::replaceAll(mChangedString, "${OriMoney}", std::to_string(mOriScore));
                    ll::string_utils::replaceAll(mChangedString, "${SetMoney}", 
                        (type == ScoreChangedType::add ? "+" : (type == ScoreChangedType::reduce ? "-" : "")) + std::to_string(score)
                    );
                    ll::string_utils::replaceAll(mChangedString, "${GetMoney}", 
                        (type == ScoreChangedType::add ? std::to_string(mOriScore + score) :
                        (type == ScoreChangedType::reduce ? std::to_string(mOriScore - score) : std::to_string(score)))
                    );
                    player->sendMessage(mChangedString);
                }
            });
        }
        if (std::get<bool>(options.at("DisableCommand_Enabled"))) {
            std::vector<std::string> mObjectCommands = std::get<std::vector<std::string>>(options.at("DisableCommand_List"));
            ExecuteCommandEvent = eventBus.emplaceListener<ll::event::ExecutingCommandEvent>(
                [mObjectCommands, options](ll::event::ExecutingCommandEvent& event) {
                    std::string mCommand = ll::string_utils::replaceAll(std::string(ll::string_utils::splitByPattern(event.commandContext().mCommand, " ")[0]), "/", "");
                    if (std::find(mObjectCommands.begin(), mObjectCommands.end(), mCommand) != mObjectCommands.end()) {
                        event.cancel();

                        auto* entity = event.commandContext().getCommandOrigin().getEntity();
                        if (entity == nullptr || !entity->isPlayer())
                            return;
                        Player* player = static_cast<Player*>(entity);
                        player->sendMessage(std::get<std::string>(options.at("DisableCommand_Text")));
                    }
                }
            );
        }
        LOICollection::HookPlugin::Event::onLoginPacketSendEvent([options](void* /*unused*/, std::string mUuid, std::string /*unused*/) {
            if (std::get<bool>(options.at("ServerToast_Enabled")))
                LOICollection::HookPlugin::interceptTextPacket(mUuid);
            LOICollection::HookPlugin::interceptGetNameTag(mUuid);
        });
        LOICollection::HookPlugin::Event::onPlayerDisconnectBeforeEvent([options](std::string mUuid) {
            if (std::get<bool>(options.at("ServerToast_Enabled")))
                LOICollection::HookPlugin::uninterceptTextPacket(mUuid);
            LOICollection::HookPlugin::uninterceptGetNameTag(mUuid);
        });
    }

    void unregistery() {
        auto& eventBus = ll::event::EventBus::getInstance();
        if (PlayerJoinEventListener) eventBus.removeListener(PlayerJoinEventListener);
        if (PlayerLeaveEventListener) eventBus.removeListener(PlayerLeaveEventListener);
        if (ExecuteCommandEvent) eventBus.removeListener(ExecuteCommandEvent);
    }
}