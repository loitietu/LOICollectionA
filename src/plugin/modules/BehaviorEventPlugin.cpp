#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <ll/api/command/Command.h>
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/event/player/PlayerConnectEvent.h>
#include <ll/api/event/player/PlayerDisconnectEvent.h>
#include <ll/api/event/player/PlayerChatEvent.h>
#include <ll/api/event/player/PlayerAddExperienceEvent.h>
#include <ll/api/event/player/PlayerAttackEvent.h>
#include <ll/api/event/player/PlayerChangePermEvent.h>
#include <ll/api/event/player/PlayerDestroyBlockEvent.h>
#include <ll/api/event/player/PlayerPlaceBlockEvent.h>
#include <ll/api/event/player/PlayerDieEvent.h>
#include <ll/api/event/Player/PlayerPickUpItemEvent.h>
#include <ll/api/event/player/PlayerInteractBlockEvent.h>
#include <ll/api/event/player/PlayerRespawnEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>

#include <ll/api/utils/HashUtils.h>

#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/shared_types/legacy/actor/ActorDamageCause.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/level/BlockPos.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/dimension/Dimension.h>

#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/item/ItemActor.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/item/ItemStack.h>
#include <mc/world/item/SaveContext.h>
#include <mc/world/item/SaveContextFactory.h>

#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandSelector.h>
#include <mc/server/commands/CommandPosition.h>
#include <mc/server/commands/CommandPermissionLevel.h>
#include <mc/server/commands/CommandOutputMessageType.h>
#include <mc/server/commands/PlayerPermissionLevel.h>

#include "utils/BlockUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "ConfigPlugin.h"

#include "include/BehaviorEventPlugin.h"

std::vector<std::unordered_map<std::string, std::string>> mEvents;

using I18nUtilsTools::tr;

namespace LOICollection::Plugins::behaviorevent {
    struct BehaviorEventOP {
        CommandPosition PositionOrigin;
        CommandPosition PositionTarget;
        DimensionType Dimension;
        std::string EventId;
        std::string EventName;
        std::string Target;
        std::string Value;
        int Time;
        int Radius;
    };

    C_Config::C_Plugins::C_BehaviorEvent options;

    std::unique_ptr<SQLiteStorage> db;
    std::shared_ptr<ll::io::Logger> logger;

    ll::event::ListenerPtr PlayerConnectEventListener;
    ll::event::ListenerPtr PlayerDisconnectEventListener;
    ll::event::ListenerPtr PlayerChatEventListener;
    ll::event::ListenerPtr PlayerAddExperienceEventListener;
    ll::event::ListenerPtr PlayerAttackEventListener;
    ll::event::ListenerPtr PlayerChangePermEventListener;
    ll::event::ListenerPtr PlayerDestroyBlockEventListener;
    ll::event::ListenerPtr PlayerPlaceBlockEventListener;
    ll::event::ListenerPtr PlayerDieEventListener;
    ll::event::ListenerPtr PlayerPickUpItemEventListener;
    ll::event::ListenerPtr PlayerInteractBlockEventListener;
    ll::event::ListenerPtr PlayerRespawnEventListener;
    ll::event::ListenerPtr PlayerUseItemEventListener;

    bool WirteDatabaseTaskRunning = true;
    bool CleanDatabaseTaskRunning = true;

    namespace {
        struct UniversalHasher {
            size_t operator()(const auto& mValue) const {
                ll::hash_utils::HashCombiner mHasher;
                for (const auto& [k, v] : mValue)
                    mHasher.add(k).add(v);
                return mHasher.add(mValue.size()).hash();
            }
        };

        void registerCommand() {
            ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
                .getOrCreateCommand("behaviorevent", tr({}, "commands.behaviorevent.description"), CommandPermissionLevel::GameDirectors);
            command.overload().text("clean").execute([](CommandOrigin const&, CommandOutput& output) -> void {
                clean(options.OrganizeDatabaseInterval);

                output.success(tr({}, "commands.behaviorevent.success.clean"));
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("info").required("EventId").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::unordered_map<std::string, std::string> mEvent = db->getByPrefix("Events", param.EventId + ".");
                
                if (mEvent.empty())
                    return output.error(tr({}, "commands.behaviorevent.error.query"));

                output.success(tr({}, "commands.behaviorevent.success.query.info"));
                std::for_each(mEvent.begin(), mEvent.end(), [&output, id = param.EventId](const std::pair<std::string, std::string>& mPair) {
                    std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                    output.success("{0}: {1}", mKey, mPair.second);
                });
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("name").required("EventName").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::vector<std::string> mResult = getEvents({{ "EventName", param.EventName }});
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("time").required("Time").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::vector<std::string> mResult = getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                    std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                    return !SystemUtils::isReach(mTime);
                });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("foundation").required("EventName").required("Time").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::vector<std::string> mNames = getEvents({{ "EventName", param.EventName }});
                std::vector<std::string> mTimes = getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                    std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                    return !SystemUtils::isReach(mTime);
                });
                std::vector<std::string> mResult = SystemUtils::getIntersection({ mNames, mTimes });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("position").required("PositionOrigin").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                std::vector<std::string> mResult = getEvents({ 
                    { "Position.x", std::to_string((int) mPosition.x) },
                    { "Position.y", std::to_string((int) mPosition.y) },
                    { "Position.z", std::to_string((int) mPosition.z) }
                });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("dimension").required("Dimension").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::vector<std::string> mResult = getEvents({{ "Position.dimension", std::to_string(param.Dimension) }});
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("site").required("PositionOrigin").required("Dimension").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                std::vector<std::string> mPositions = getEvents({ 
                    { "Position.x", std::to_string((int) mPosition.x) },
                    { "Position.y", std::to_string((int) mPosition.y) },
                    { "Position.z", std::to_string((int) mPosition.z) }
                });
                std::vector<std::string> mDimensions = getEvents({{ "Position.dimension", std::to_string(param.Dimension) }});
                std::vector<std::string> mResult = SystemUtils::getIntersection({ mPositions, mDimensions });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("event").text("custom").required("Target").required("Value").execute(
                [](CommandOrigin const&, CommandOutput& output, BehaviorEventOP const& param) -> void {
                std::vector<std::string> mResult = getEvents({ { param.Target, param.Value } });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("action").text("range").required("PositionOrigin").required("Radius").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                std::vector<std::string> mResult = getEventsByPoition(origin.getDimension()->getDimensionId(), [mPosition, radius = param.Radius](int x, int y, int z) -> bool {
                    return Vec3(x, y, z).distanceTo(mPosition) <= radius;
                });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("query").text("action").text("position").required("PositionOrigin").required("PositionTarget").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
                Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
                Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

                std::vector<std::string> mResult = getEventsByPoition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                    return x >= (double) mPositionMin.x && x <= (double) mPositionMax.x && y >= (double) mPositionMin.y && y <= (double) mPositionMax.y && z >= (double) mPositionMin.z && z <= (double) mPositionMax.z;
                });
                std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                    return a + (a.empty() ? "" : ", ") + b;
                });

                output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
            });
            command.overload<BehaviorEventOP>().text("back").text("range").required("PositionOrigin").required("Radius").required("Time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                std::vector<std::string> mAreas = getEventsByPoition(origin.getDimension()->getDimensionId(), [mPosition, radius = param.Radius](int x, int y, int z) -> bool {
                    return Vec3(x, y, z).distanceTo(mPosition) <= radius;
                });
                std::vector<std::string> mTimes = getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                    std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                    return !SystemUtils::isReach(mTime);
                });
                std::vector<std::string> mTypes = getEvents({{ "EventType", "Operable" }});
                std::vector<std::string> mResult = SystemUtils::getIntersection({ mAreas, mTimes, mTypes });

                if (mResult.empty())
                    return output.error(tr({}, "commands.behaviorevent.error.back"));

                std::for_each(mResult.begin(), mResult.end(), [](const std::string& mId) {
                    SQLite::Transaction transaction(db->getDatabase());
                    
                    back(mId);

                    transaction.commit();
                });

                output.success(tr({}, "commands.behaviorevent.success.back"));
            });
            command.overload<BehaviorEventOP>().text("back").text("position").required("PositionOrigin").required("PositionTarget").required("Time").execute(
                [](CommandOrigin const& origin, CommandOutput& output, BehaviorEventOP const& param, Command const& cmd) -> void {
                Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
                Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

                Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
                Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

                std::vector<std::string> mAreas = getEventsByPoition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                    return x >= (double) mPositionMin.x && x <= (double) mPositionMax.x && y >= (double) mPositionMin.y && y <= (double) mPositionMax.y && z >= (double) mPositionMin.z && z <= (double) mPositionMax.z;
                });
                std::vector<std::string> mTimes = getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                    std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                    return !SystemUtils::isReach(mTime);
                });
                std::vector<std::string> mTypes = getEvents({{ "EventType", "Operable" }});
                std::vector<std::string> mResult = SystemUtils::getIntersection({ mAreas, mTimes, mTypes });

                if (mResult.empty())
                    return output.error(tr({}, "commands.behaviorevent.error.back"));

                std::for_each(mResult.begin(), mResult.end(), [](const std::string& mId) {
                    SQLite::Transaction transaction(db->getDatabase());

                    back(mId);

                    transaction.commit();
                });

                output.success(tr({}, "commands.behaviorevent.success.back"));
            });
        }

        void listenEvent() {
            ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
                auto* batch = static_cast<SQLiteStorageBatch*>(db.get());

                while (WirteDatabaseTaskRunning) {
                    co_await std::chrono::minutes(options.RefreshIntervalInMinutes);

                    std::unordered_set<std::unordered_map<std::string, std::string>, UniversalHasher> mEventSets;
                    for (auto it = mEvents.begin(); it!= mEvents.end(); )
                        mEventSets.insert(*it).second ? ++it : it = mEvents.erase(it);

                    SQLite::Transaction transaction(db->getDatabase());
                    std::for_each(mEvents.begin(), mEvents.end(), [batch](const std::unordered_map<std::string, std::string>& mEvent) {
                        std::string mTismestamp = SystemUtils::getCurrentTimestamp();

                        batch->set("Events", mTismestamp, mEvent);
                    });
                    transaction.commit();

                    mEvents.clear();
                }
            }).launch(ll::thread::ServerThreadExecutor::getDefault());
            ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
                while (CleanDatabaseTaskRunning) {
                    co_await std::chrono::hours(options.CleanDatabaseInterval);

                    clean(options.OrganizeDatabaseInterval);
                }
            }).launch(ll::thread::ServerThreadExecutor::getDefault());

            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            PlayerConnectEventListener = eventBus.emplaceListener<ll::event::PlayerConnectEvent>([](ll::event::PlayerConnectEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerConnect.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerConnect", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerConnect.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerConnect.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerconnect")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerDisconnect.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerDisconnect", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerDisconnect.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerDisconnect.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdisconnect")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([](ll::event::PlayerChatEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerChat.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerChat", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventMessage"] = event.message();
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerChat.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerChat.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerchat")), 
                        mEvent["PlayerName"], mEvent["EventMessage"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerAddExperienceEventListener = eventBus.emplaceListener<ll::event::PlayerAddExperienceEvent>([](ll::event::PlayerAddExperienceEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerAddExperience.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerAddExperience", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventExperience"] = std::to_string(event.experience());
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerAddExperience.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerAddExperience.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playeraddexperience")), 
                        mEvent["PlayerName"], mEvent["EventExperience"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerAttackEventListener = eventBus.emplaceListener<ll::event::PlayerAttackEvent>([](ll::event::PlayerAttackEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerAttack.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerAttack", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventTarget"] = event.target().getTypeName();
                mEvent["EventCause"] = magic_enum::enum_name(event.cause()).data();
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerAttack.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerAttack.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerattack")), 
                        mEvent["PlayerName"], mEvent["EventCause"], mEvent["EventTarget"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerChangePermEventListener = eventBus.emplaceListener<ll::event::PlayerChangePermEvent>([](ll::event::PlayerChangePermEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerChangePerm.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerChangePerm", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventPerm"] = magic_enum::enum_name(event.newPerm()).data();
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerChangePerm.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerChangePerm.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerchangeperm")), 
                        mEvent["PlayerName"], mEvent["EventPerm"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerDestroyBlockEventListener = eventBus.emplaceListener<ll::event::PlayerDestroyBlockEvent>([](ll::event::PlayerDestroyBlockEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerDestroyBlock.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerDestroyBlock", "Operable", event.pos(), event.self().getDimensionId()
                );
                mEvent["EventOperable"] = BlockUtils::getBlock(event.pos(), event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerDestroyBlock.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerDestroyBlock.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdestroyblock")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventOperable"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerPlaceBlockEventListener = eventBus.emplaceListener<ll::event::PlayerPlacingBlockEvent>([](ll::event::PlayerPlacingBlockEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerPlaceBlock.ModuleEnabled)
                    return;

                BlockPos mPosition = event.pos();
                switch (event.face()) {
                    case 0:
                        --mPosition.y;
                        break;
                    case 1:
                        ++mPosition.y;
                        break;
                    case 2:
                        --mPosition.z;
                        break;
                    case 3:
                        ++mPosition.z;
                        break;
                    case 4:
                        --mPosition.x;
                        break;
                    case 5:
                        ++mPosition.x;
                        break;
                }

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerPlaceBlock", "Operable", mPosition, event.self().getDimensionId()
                );
                mEvent["EventOperable"] = BlockUtils::getBlock(mPosition, event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerPlaceBlock.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerPlaceBlock.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerplaceblock")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventOperable"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerDieEventListener = eventBus.emplaceListener<ll::event::PlayerDieEvent>([](ll::event::PlayerDieEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerDie.ModuleEnabled)
                    return;

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerDie", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventSource"] = magic_enum::enum_name(event.source().mCause).data();
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerDie.RecordDatabase)
                    mEvents.push_back(mEvent);
                
                if (options.Events.onPlayerDie.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdie")), 
                        mEvent["PlayerName"], mEvent["EventSource"] 
                    );
            }, ll::event::EventPriority::Highest);
            PlayerPickUpItemEventListener = eventBus.emplaceListener<ll::event::PlayerPickUpItemEvent>([](ll::event::PlayerPickUpItemEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerPickUpItem.ModuleEnabled)
                    return;
                
                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerPickUpItem", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventItem"] = event.itemActor().item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0);
                mEvent["EventOrgCount"] = std::to_string(event.orgCount());
                mEvent["EventFavoredSlot"] = std::to_string(event.favoredSlot());
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerPickUpItem.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerPickUpItem.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerpickupitem")), 
                        mEvent["PlayerName"], mEvent["EventItem"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerInteractBlockEventListener = eventBus.emplaceListener<ll::event::PlayerInteractBlockEvent>([](ll::event::PlayerInteractBlockEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerInteractBlock.ModuleEnabled)
                    return; 

                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerInteractBlock", "Normal", event.blockPos(), event.self().getDimensionId()
                );
                mEvent["EventBlock"] = BlockUtils::getBlock(event.blockPos(), event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerInteractBlock.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerInteractBlock.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerinteractblock")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventBlock"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerRespawnEventListener = eventBus.emplaceListener<ll::event::PlayerRespawnEvent>([](ll::event::PlayerRespawnEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerRespawn.ModuleEnabled)
                    return;
                
                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerRespawn", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerRespawn.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerRespawn.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerrespawn")), 
                        mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                    );
            }, ll::event::EventPriority::Highest);
            PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([](ll::event::PlayerUseItemEvent& event) -> void {
                if (event.self().isSimulatedPlayer() || !options.Events.onPlayerUseItem.ModuleEnabled)
                    return;
                
                std::unordered_map<std::string, std::string> mEvent = getBasicEvent(
                    "PlayerUseItem", "Normal", event.self().getPosition(), event.self().getDimensionId()
                );
                mEvent["EventItem"] = event.item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0);
                mEvent["PlayerName"] = event.self().getRealName();

                if (options.Events.onPlayerUseItem.RecordDatabase) 
                    mEvents.push_back(mEvent);

                if (options.Events.onPlayerUseItem.OutputConsole)
                    logger->info(fmt::runtime(tr({}, "behaviorevent.event.playeruseitem")), 
                        mEvent["PlayerName"], mEvent["EventItem"]
                    );
            }, ll::event::EventPriority::Highest);
        }

        void unlistenEvent() {
            ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener(PlayerConnectEventListener);
            eventBus.removeListener(PlayerDisconnectEventListener);
            eventBus.removeListener(PlayerChatEventListener);
            eventBus.removeListener(PlayerAddExperienceEventListener);
            eventBus.removeListener(PlayerAttackEventListener);
            eventBus.removeListener(PlayerChangePermEventListener);
            eventBus.removeListener(PlayerDestroyBlockEventListener);
            eventBus.removeListener(PlayerPlaceBlockEventListener);
            eventBus.removeListener(PlayerDieEventListener);
            eventBus.removeListener(PlayerPickUpItemEventListener);
            eventBus.removeListener(PlayerInteractBlockEventListener);
            eventBus.removeListener(PlayerRespawnEventListener);
            eventBus.removeListener(PlayerUseItemEventListener);

            WirteDatabaseTaskRunning = false;
            CleanDatabaseTaskRunning = false;
        }
    }

    std::unordered_map<std::string, std::string> getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension) {
        if (!isValid())
            return {};

        std::unordered_map<std::string, std::string> mEvent = {
            { "EventName", name },
            { "EventTime", SystemUtils::getNowTime() },
            { "EventType", type },
            { "Position.x", std::to_string((int) position.x) },
            { "Position.y", std::to_string((int) position.y) },
            { "Position.z", std::to_string((int) position.z) },
            { "Position.dimension", std::to_string(dimension) }
        };

        return mEvent;
    }

    std::vector<std::string> getEvents() {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = db->listByPrefix("Events", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_first_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter) {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(conditions.begin(), conditions.end(), [&mResult, &mKeys, filter](const std::pair<std::string, std::string>& mCondition) -> void {
            auto mView = mKeys | std::views::filter([&mCondition, filter](const std::string& mId) -> bool {
                std::string mTarget = db->get("Events", mId + "." + mCondition.first);
                return (!filter && mTarget == mCondition.second) || (filter && filter(mTarget));
            });
            std::copy(mView.begin(), mView.end(), std::back_inserter(mResult));
        });

        return mResult;
    }

    std::vector<std::string> getEventsByPoition(int dimension, std::function<bool(int x, int y, int z)> filter) {
        if (!isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult, dimension, filter](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> data = db->getByPrefix("Events", mId + ".");

            if (SystemUtils::toInt(data.at(mId + ".Position.dimension"), 0) != dimension)
                return;
            
            int x = SystemUtils::toInt(data.at(mId + ".Position.x"), 0);
            int y = SystemUtils::toInt(data.at(mId + ".Position.y"), 0);
            int z = SystemUtils::toInt(data.at(mId + ".Position.z"), 0);

            if (filter(x, y, z)) mResult.push_back(mId);
        });

        return mResult;
    }

    void back(const std::string& id) {
        if (!isValid())
            return;

        std::unordered_map<std::string, std::string> data = db->getByPrefix("Events", id + ".");

        int mDimension = SystemUtils::toInt(data.at(id + ".Position.dimension"), 0);

        BlockPos mPosition(
            SystemUtils::toInt(data.at(id + ".Position.x"), 0),
            SystemUtils::toInt(data.at(id + ".Position.y"), 0),
            SystemUtils::toInt(data.at(id + ".Position.z"), 0)
        );

        CompoundTag mNbt = CompoundTag::fromSnbt(data.at(id + ".EventOperable"))->mTags;
        if (auto mBlock = BlockUtils::getBlock(mPosition, mDimension); mBlock.has_value())
            BlockUtils::setBlock(mPosition, mDimension, mNbt);
        if (auto mBlockActor = BlockUtils::getBlockEntity(mPosition, mDimension); mBlockActor.has_value())
            BlockUtils::setBlockEntity(mPosition, mDimension, mNbt);

        db->delByPrefix("Events", id + ".");
    }

    void clean(int hours) {
        if (!isValid())
            return;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(mKeys.begin(), mKeys.end(), [hours](const std::string& mId) {
            std::string mEventTime = db->get("Events", mId + ".EventTime");
            
            std::string mTime = SystemUtils::timeCalculate(mEventTime, hours, "0");
            if (SystemUtils::isReach(mTime))
                db->delByPrefix("Events", mId + ".");
        });
    }

    bool isValid() {
        return logger != nullptr && db != nullptr;
    }

    void registery(void* database) {
        db = std::move(*static_cast<std::unique_ptr<SQLiteStorage>*>(database));
        logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        
        options = Config::GetBaseConfigContext().Plugins.BehaviorEvent;

        db->create("Events");

        registerCommand();
        listenEvent();
    }
    
    void unregistery() {
        unlistenEvent();

        db->exec("VACUUM;");
    }
}
