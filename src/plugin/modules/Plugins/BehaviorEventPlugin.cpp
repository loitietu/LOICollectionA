#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>
#include <SQLiteCpp/SQLiteCpp.h>

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

#include "include/RegistryHelper.h"

#include "utils/BlockUtils.h"
#include "utils/SystemUtils.h"
#include "utils/I18nUtils.h"

#include "data/SQLiteStorage.h"

#include "base/Wrapper.h"
#include "base/ServiceProvider.h"

#include "ConfigPlugin.h"

#include "include/Plugins/BehaviorEventPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct UniversalHasher {
        size_t operator()(const auto& mValue) const {
            ll::hash_utils::HashCombiner mHasher;
            for (const auto& [k, v] : mValue)
                mHasher.add(k).add(v);
            return mHasher.add(mValue.size()).hash();
        }
    };

    struct BehaviorEventPlugin::operation {
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

    struct BehaviorEventPlugin::Impl {
        std::vector<std::unordered_map<std::string, std::string>> mEvents;

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
    };

    BehaviorEventPlugin::BehaviorEventPlugin() : mImpl(std::make_unique<Impl>()) {};
    BehaviorEventPlugin::~BehaviorEventPlugin() = default;

    SQLiteStorage* BehaviorEventPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* BehaviorEventPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    void BehaviorEventPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("behaviorevent", tr({}, "commands.behaviorevent.description"), CommandPermissionLevel::GameDirectors);
        command.overload().text("clean").execute([this](CommandOrigin const&, CommandOutput& output) -> void {
            this->clean(this->mImpl->options.OrganizeDatabaseInterval);

            output.success(tr({}, "commands.behaviorevent.success.clean"));
        });
        command.overload<operation>().text("query").text("event").text("info").required("EventId").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->getByPrefix("Events", param.EventId + ".");
            
            if (mEvent.empty())
                return output.error(tr({}, "commands.behaviorevent.error.query"));

            output.success(tr({}, "commands.behaviorevent.success.query.info"));
            std::for_each(mEvent.begin(), mEvent.end(), [&output, id = param.EventId](const std::pair<std::string, std::string>& mPair) {
                std::string mKey = mPair.first.substr(mPair.first.find_first_of('.') + 1);

                output.success("{0}: {1}", mKey, mPair.second);
            });
        });
        command.overload<operation>().text("query").text("event").text("name").required("EventName").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "EventName", param.EventName }});
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("time").required("Time").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                return !SystemUtils::isReach(mTime);
            });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("foundation").required("EventName").required("Time").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mNames = this->getEvents({{ "EventName", param.EventName }});
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                return !SystemUtils::isReach(mTime);
            });
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mNames, mTimes });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("position").required("PositionOrigin").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mResult = this->getEvents({ 
                { "Position.x", std::to_string((int) mPosition.x) },
                { "Position.y", std::to_string((int) mPosition.y) },
                { "Position.z", std::to_string((int) mPosition.z) }
            });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("dimension").required("Dimension").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "Position.dimension", std::to_string(param.Dimension) }});
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("site").required("PositionOrigin").required("Dimension").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mPositions = this->getEvents({ 
                { "Position.x", std::to_string((int) mPosition.x) },
                { "Position.y", std::to_string((int) mPosition.y) },
                { "Position.z", std::to_string((int) mPosition.z) }
            });
            std::vector<std::string> mDimensions = this->getEvents({{ "Position.dimension", std::to_string(param.Dimension) }});
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mPositions, mDimensions });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("event").text("custom").required("Target").required("Value").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({ { param.Target, param.Value } });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("action").text("range").required("PositionOrigin").required("Radius").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mResult = this->getEventsByPoition(origin.getDimension()->getDimensionId(), [mPosition, radius = param.Radius](int x, int y, int z) -> bool {
                return Vec3(x, y, z).distanceTo(mPosition) <= radius;
            });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("query").text("action").text("position").required("PositionOrigin").required("PositionTarget").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
            Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
            Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

            std::vector<std::string> mResult = this->getEventsByPoition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                return x >= (double) mPositionMin.x && x <= (double) mPositionMax.x && y >= (double) mPositionMin.y && y <= (double) mPositionMax.y && z >= (double) mPositionMin.z && z <= (double) mPositionMax.z;
            });
            std::string result = std::accumulate(mResult.cbegin(), mResult.cend(), std::string(), [](const std::string& a, const std::string& b) {
                return a + (a.empty() ? "" : ", ") + b;
            });

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), result.empty() ? "None" : result);
        });
        command.overload<operation>().text("back").text("range").required("PositionOrigin").required("Radius").required("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mAreas = this->getEventsByPoition(origin.getDimension()->getDimensionId(), [mPosition, radius = param.Radius](int x, int y, int z) -> bool {
                return Vec3(x, y, z).distanceTo(mPosition) <= radius;
            });
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                return !SystemUtils::isReach(mTime);
            });
            std::vector<std::string> mTypes = this->getEvents({{ "EventType", "Operable" }});
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mAreas, mTimes, mTypes });

            if (mResult.empty())
                return output.error(tr({}, "commands.behaviorevent.error.back"));

            std::for_each(mResult.begin(), mResult.end(), [this](const std::string& mId) {
                SQLite::Transaction transaction(*this->getDatabase()->getDatabase());
                
                this->back(mId);

                transaction.commit();
            });

            output.success(tr({}, "commands.behaviorevent.success.back"));
        });
        command.overload<operation>().text("back").text("position").required("PositionOrigin").required("PositionTarget").required("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
            Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
            Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

            std::vector<std::string> mAreas = this->getEventsByPoition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                return x >= (double) mPositionMin.x && x <= (double) mPositionMax.x && y >= (double) mPositionMin.y && y <= (double) mPositionMax.y && z >= (double) mPositionMin.z && z <= (double) mPositionMax.z;
            });
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::timeCalculate(value, time, "0");
                return !SystemUtils::isReach(mTime);
            });
            std::vector<std::string> mTypes = this->getEvents({{ "EventType", "Operable" }});
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mAreas, mTimes, mTypes });

            if (mResult.empty())
                return output.error(tr({}, "commands.behaviorevent.error.back"));

            std::for_each(mResult.begin(), mResult.end(), [this](const std::string& mId) {
                SQLite::Transaction transaction(*this->getDatabase()->getDatabase());

                this->back(mId);

                transaction.commit();
            });

            output.success(tr({}, "commands.behaviorevent.success.back"));
        });
    }

    void BehaviorEventPlugin::listenEvent() {
        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            auto* batch = static_cast<SQLiteStorageBatch*>(this->getDatabase());

            while (this->mImpl->WirteDatabaseTaskRunning) {
                co_await std::chrono::minutes(this->mImpl->options.RefreshIntervalInMinutes);

                std::unordered_set<std::unordered_map<std::string, std::string>, UniversalHasher> mEventSets;
                for (auto it = this->mImpl->mEvents.begin(); it!= this->mImpl->mEvents.end(); )
                    mEventSets.insert(*it).second ? ++it : it = this->mImpl->mEvents.erase(it);

                SQLite::Transaction transaction(*this->getDatabase()->getDatabase());
                std::for_each(this->mImpl->mEvents.begin(), this->mImpl->mEvents.end(), [batch](const std::unordered_map<std::string, std::string>& mEvent) {
                    std::string mTismestamp = SystemUtils::getCurrentTimestamp();

                    batch->set("Events", mTismestamp, mEvent);
                });
                transaction.commit();

                this->mImpl->mEvents.clear();
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->CleanDatabaseTaskRunning) {
                co_await std::chrono::hours(this->mImpl->options.CleanDatabaseInterval);

                this->clean(this->mImpl->options.OrganizeDatabaseInterval);
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->PlayerConnectEventListener = eventBus.emplaceListener<ll::event::PlayerConnectEvent>([this](ll::event::PlayerConnectEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerConnect.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerConnect", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerConnect.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerConnect.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerconnect")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerDisconnectEventListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerDisconnect.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerDisconnect", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerDisconnect.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerDisconnect.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdisconnect")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerChatEventListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>([this](ll::event::PlayerChatEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerChat.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerChat", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventMessage"] = event.message();
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerChat.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerChat.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerchat")), 
                    mEvent["PlayerName"], mEvent["EventMessage"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerAddExperienceEventListener = eventBus.emplaceListener<ll::event::PlayerAddExperienceEvent>([this](ll::event::PlayerAddExperienceEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerAddExperience.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerAddExperience", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventExperience"] = std::to_string(event.experience());
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerAddExperience.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerAddExperience.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playeraddexperience")), 
                    mEvent["PlayerName"], mEvent["EventExperience"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerAttackEventListener = eventBus.emplaceListener<ll::event::PlayerAttackEvent>([this](ll::event::PlayerAttackEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerAttack.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerAttack", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventTarget"] = event.target().getTypeName();
            mEvent["EventCause"] = magic_enum::enum_name(event.cause()).data();
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerAttack.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerAttack.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerattack")), 
                    mEvent["PlayerName"], mEvent["EventCause"], mEvent["EventTarget"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerChangePermEventListener = eventBus.emplaceListener<ll::event::PlayerChangePermEvent>([this](ll::event::PlayerChangePermEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerChangePerm.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerChangePerm", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventPerm"] = magic_enum::enum_name(event.newPerm()).data();
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerChangePerm.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerChangePerm.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerchangeperm")), 
                    mEvent["PlayerName"], mEvent["EventPerm"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerDestroyBlockEventListener = eventBus.emplaceListener<ll::event::PlayerDestroyBlockEvent>([this](ll::event::PlayerDestroyBlockEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerDestroyBlock.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerDestroyBlock", "Operable", event.pos(), event.self().getDimensionId()
            );
            mEvent["EventOperable"] = BlockUtils::getBlock(event.pos(), event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerDestroyBlock.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerDestroyBlock.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdestroyblock")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventOperable"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerPlaceBlockEventListener = eventBus.emplaceListener<ll::event::PlayerPlacingBlockEvent>([this](ll::event::PlayerPlacingBlockEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerPlaceBlock.ModuleEnabled)
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

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerPlaceBlock", "Operable", mPosition, event.self().getDimensionId()
            );
            mEvent["EventOperable"] = BlockUtils::getBlock(mPosition, event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerPlaceBlock.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerPlaceBlock.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerplaceblock")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventOperable"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerDieEventListener = eventBus.emplaceListener<ll::event::PlayerDieEvent>([this](ll::event::PlayerDieEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerDie.ModuleEnabled)
                return;

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerDie", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventSource"] = magic_enum::enum_name(event.source().mCause).data();
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerDie.RecordDatabase)
                this->mImpl->mEvents.push_back(mEvent);
            
            if (this->mImpl->options.Events.onPlayerDie.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerdie")), 
                    mEvent["PlayerName"], mEvent["EventSource"] 
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerPickUpItemEventListener = eventBus.emplaceListener<ll::event::PlayerPickUpItemEvent>([this](ll::event::PlayerPickUpItemEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerPickUpItem.ModuleEnabled)
                return;
            
            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerPickUpItem", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventItem"] = event.itemActor().item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0);
            mEvent["EventOrgCount"] = std::to_string(event.orgCount());
            mEvent["EventFavoredSlot"] = std::to_string(event.favoredSlot());
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerPickUpItem.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerPickUpItem.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerpickupitem")), 
                    mEvent["PlayerName"], mEvent["EventItem"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerInteractBlockEventListener = eventBus.emplaceListener<ll::event::PlayerInteractBlockEvent>([this](ll::event::PlayerInteractBlockEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerInteractBlock.ModuleEnabled)
                return; 

            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerInteractBlock", "Normal", event.blockPos(), event.self().getDimensionId()
            );
            mEvent["EventBlock"] = BlockUtils::getBlock(event.blockPos(), event.self().getDimensionId()).value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0);
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerInteractBlock.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerInteractBlock.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerinteractblock")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"], mEvent["EventBlock"]
                );
        }, ll::event::EventPriority::Highest);

        this->mImpl->PlayerRespawnEventListener = eventBus.emplaceListener<ll::event::PlayerRespawnEvent>([this](ll::event::PlayerRespawnEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerRespawn.ModuleEnabled)
                return;
            
            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerRespawn", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerRespawn.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerRespawn.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playerrespawn")), 
                    mEvent["PlayerName"], mEvent["Position.dimension"], mEvent["Position.x"], mEvent["Position.y"], mEvent["Position.z"]
                );
        }, ll::event::EventPriority::Highest);
        
        this->mImpl->PlayerUseItemEventListener = eventBus.emplaceListener<ll::event::PlayerUseItemEvent>([this](ll::event::PlayerUseItemEvent& event) mutable -> void {
            if (event.self().isSimulatedPlayer() || !this->mImpl->options.Events.onPlayerUseItem.ModuleEnabled)
                return;
            
            std::unordered_map<std::string, std::string> mEvent = this->getBasicEvent(
                "PlayerUseItem", "Normal", event.self().getPosition(), event.self().getDimensionId()
            );
            mEvent["EventItem"] = event.item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0);
            mEvent["PlayerName"] = event.self().getRealName();

            if (this->mImpl->options.Events.onPlayerUseItem.RecordDatabase) 
                this->mImpl->mEvents.push_back(mEvent);

            if (this->mImpl->options.Events.onPlayerUseItem.OutputConsole)
                this->mImpl->logger->info(fmt::runtime(tr({}, "behaviorevent.event.playeruseitem")), 
                    mEvent["PlayerName"], mEvent["EventItem"]
                );
        }, ll::event::EventPriority::Highest);
    }

    void BehaviorEventPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->PlayerConnectEventListener);
        eventBus.removeListener(this->mImpl->PlayerDisconnectEventListener);
        eventBus.removeListener(this->mImpl->PlayerChatEventListener);
        eventBus.removeListener(this->mImpl->PlayerAddExperienceEventListener);
        eventBus.removeListener(this->mImpl->PlayerAttackEventListener);
        eventBus.removeListener(this->mImpl->PlayerChangePermEventListener);
        eventBus.removeListener(this->mImpl->PlayerDestroyBlockEventListener);
        eventBus.removeListener(this->mImpl->PlayerPlaceBlockEventListener);
        eventBus.removeListener(this->mImpl->PlayerDieEventListener);
        eventBus.removeListener(this->mImpl->PlayerPickUpItemEventListener);
        eventBus.removeListener(this->mImpl->PlayerInteractBlockEventListener);
        eventBus.removeListener(this->mImpl->PlayerRespawnEventListener);
        eventBus.removeListener(this->mImpl->PlayerUseItemEventListener);

        this->mImpl->WirteDatabaseTaskRunning = false;
        this->mImpl->CleanDatabaseTaskRunning = false;
    }

    std::unordered_map<std::string, std::string> BehaviorEventPlugin::getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension) {
        if (!this->isValid())
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

    std::vector<std::string> BehaviorEventPlugin::getEvents() {
        if (!this->isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Events", "%.");
        std::for_each(mKeys.begin(), mKeys.end(), [&mResult](const std::string& mId) -> void {
            std::string mData = mId.substr(0, mId.find_first_of('.'));

            mResult.push_back(mData);
        });

        std::sort(mResult.begin(), mResult.end());
        mResult.erase(std::unique(mResult.begin(), mResult.end()), mResult.end());

        return mResult;
    }

    std::vector<std::string> BehaviorEventPlugin::getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(conditions.begin(), conditions.end(), [this, &mResult, &mKeys, filter](const std::pair<std::string, std::string>& mCondition) -> void {
            auto mView = mKeys | std::views::filter([this, &mCondition, filter](const std::string& mId) -> bool {
                std::string mTarget = this->getDatabase()->get("Events", mId + "." + mCondition.first);
                return (!filter && mTarget == mCondition.second) || (filter && filter(mTarget));
            });
            std::copy(mView.begin(), mView.end(), std::back_inserter(mResult));
        });

        return mResult;
    }

    std::vector<std::string> BehaviorEventPlugin::getEventsByPoition(int dimension, std::function<bool(int x, int y, int z)> filter) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mResult;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(mKeys.begin(), mKeys.end(), [this, &mResult, dimension, filter](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> data = this->getDatabase()->getByPrefix("Events", mId + ".");

            if (SystemUtils::toInt(data.at(mId + ".Position.dimension"), 0) != dimension)
                return;
            
            int x = SystemUtils::toInt(data.at(mId + ".Position.x"), 0);
            int y = SystemUtils::toInt(data.at(mId + ".Position.y"), 0);
            int z = SystemUtils::toInt(data.at(mId + ".Position.z"), 0);

            if (filter(x, y, z)) mResult.push_back(mId);
        });

        return mResult;
    }

    void BehaviorEventPlugin::back(const std::string& id) {
        if (!this->isValid())
            return;

        std::unordered_map<std::string, std::string> data = this->getDatabase()->getByPrefix("Events", id + ".");

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

        this->getDatabase()->delByPrefix("Events", id + ".");
    }

    void BehaviorEventPlugin::clean(int hours) {
        if (!this->isValid())
            return;

        std::vector<std::string> mKeys = getEvents();
        std::for_each(mKeys.begin(), mKeys.end(), [this, hours](const std::string& mId) mutable {
            std::string mEventTime = this->getDatabase()->get("Events", mId + ".EventTime");
            
            std::string mTime = SystemUtils::timeCalculate(mEventTime, hours, "0");
            if (SystemUtils::isReach(mTime))
                this->getDatabase()->delByPrefix("Events", mId + ".");
        });
    }

    bool BehaviorEventPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool BehaviorEventPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.BehaviorEvent.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "behaviorevent.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().Plugins.BehaviorEvent;

        return true;
    }

    bool BehaviorEventPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->getDatabase()->create("Events");

        this->registeryCommand();
        this->listenEvent();

        return true;
    }
    
    bool BehaviorEventPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        return true;
    }
}

REGISTRY_HELPER("BehaviorEventPlugin", LOICollection::Plugins::BehaviorEventPlugin, LOICollection::Plugins::BehaviorEventPlugin::getInstance())
