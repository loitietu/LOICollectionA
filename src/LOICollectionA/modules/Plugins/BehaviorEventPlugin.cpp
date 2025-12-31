#include <atomic>
#include <chrono>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <utility>
#include <concepts>
#include <algorithm>
#include <execution>
#include <functional>
#include <filesystem>
#include <unordered_map>

#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ThreadPoolExecutor.h>
#include <ll/api/base/Containers.h>

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
#include <ll/api/event/player/PlayerPickUpItemEvent.h>
#include <ll/api/event/player/PlayerInteractBlockEvent.h>
#include <ll/api/event/player/PlayerRespawnEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>

#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/deps/shared_types/legacy/actor/ActorDamageCause.h>

#include <mc/nbt/Tag.h>
#include <mc/nbt/CompoundTag.h>

#include <mc/world/level/BlockPos.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/actor/BlockActor.h>
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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/ServerEvents/BlockExplodedEvent.h"
#include "LOICollectionA/include/ServerEvents/PlayerContainerEvent.h"

#include "LOICollectionA/utils/I18nUtils.h"
#include "LOICollectionA/utils/mc/BlockUtils.h"
#include "LOICollectionA/utils/core/SystemUtils.h"

#include "LOICollectionA/data/SQLiteStorage.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/Plugins/BehaviorEventPlugin.h"

using I18nUtilsTools::tr;

namespace LOICollection::Plugins {
    struct BehaviorEventPlugin::operation {
        CommandPosition PositionOrigin;
        CommandPosition PositionTarget;
        DimensionType Dimension;
        std::string EventId;
        std::string EventName;
        std::string Target;
        std::string Value;
        int Time = 1;
        int Radius = 1;
        int Limit = 100;
    };

    struct BehaviorEventPlugin::Impl {
        ll::ConcurrentQueue<Event> mEvents;

        std::atomic<bool> mRegistered{ false };

        Config::C_BehaviorEvent options;

        std::unique_ptr<SQLiteStorage> db;
        std::shared_ptr<ll::io::Logger> logger;

        std::unordered_map<std::string, ll::event::ListenerPtr> mListeners;

        ll::thread::ThreadPoolExecutor mExecutor{ "BehavorEventPlugin", std::max(static_cast<size_t>(std::thread::hardware_concurrency()) - 2, static_cast<size_t>(2)) };

        ll::coro::InterruptableSleep WirteDatabaseTaskSleep;
        ll::coro::InterruptableSleep CleanDatabaseTaskSleep;

        std::atomic<bool> WriteDatabaseTaskRunning{ true };
        std::atomic<bool> CleanDatabaseTaskRunning{ true };
    };

    BehaviorEventPlugin::BehaviorEventPlugin() : mImpl(std::make_unique<Impl>()) {};
    BehaviorEventPlugin::~BehaviorEventPlugin() = default;

    BehaviorEventPlugin& BehaviorEventPlugin::getInstance() {
        static BehaviorEventPlugin instance;
        return instance;
    }

    SQLiteStorage* BehaviorEventPlugin::getDatabase() {
        return this->mImpl->db.get();
    }

    ll::io::Logger* BehaviorEventPlugin::getLogger() {
        return this->mImpl->logger.get();
    }

    template<typename T>
    void BehaviorEventPlugin::registeryEvent(
        const std::string& name,
        const std::string& type,
        const std::string& id,
        std::function<bool(BehaviorEventConfig)> config,
        std::function<void(ll::event::Event&, Event&)> process,
        std::function<std::string(std::string, ll::event::Event&)> formatter
    ) {
        auto& eventBus = ll::event::EventBus::getInstance();

        auto listener = eventBus.emplaceListener<T>([this, name, type, id, config, process, formatter](T& event) mutable -> void {
            if (!config(BehaviorEventConfig::ModuleEnabled))
                return;
            
            Vec3 mPosition;
            int mDimension;

            if constexpr (requires (const T& t) {
                { t.self() } -> std::convertible_to<Player&>;
            }) {
                if (event.self().isSimulatedPlayer())
                    return;

                mPosition = event.self().getPosition();
                mDimension = event.self().getDimensionId();
            } else {
                mPosition = event.getPosition();
                mDimension = event.getDimensionId();
            }

            if (config(BehaviorEventConfig::RecordDatabase)) {
                Event mEvent = this->getBasicEvent(name, type, mPosition, mDimension);

                process(event, mEvent);

                if (!this->mImpl->mEvents.try_enqueue(mEvent)) {
                    this->getLogger()->error(fmt::runtime(tr({}, "console.log.error.containter")), "BehaviorEventPlugin");
                    return;
                }
            }

            if (config(BehaviorEventConfig::OutputConsole))
                this->getLogger()->info(formatter(tr({}, id), event));
        }, ll::event::EventPriority::Highest);

        mImpl->mListeners.emplace(name, listener);
    }

    void BehaviorEventPlugin::registeryCommand() {
        ll::command::CommandHandle& command = ll::command::CommandRegistrar::getInstance()
            .getOrCreateCommand("behaviorevent", tr({}, "commands.behaviorevent.description"), CommandPermissionLevel::GameDirectors, CommandFlagValue::NotCheat | CommandFlagValue::Async);
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
        command.overload<operation>().text("query").text("event").text("name").required("EventName").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "EventName", param.EventName }}, {}, param.Limit);
            
            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("time").required("Time").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::toTimeCalculate(value, time * 3600, "0");
                return !SystemUtils::isPastOrPresent(mTime);
            }, param.Limit);
            
            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("foundation").required("EventName").required("Time").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mNames = this->getEvents({{ "EventName", param.EventName }}, {}, param.Limit);
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::toTimeCalculate(value, time * 3600, "0");
                return !SystemUtils::isPastOrPresent(mTime);
            }, param.Limit);
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mNames, mTimes });
            
            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("position").required("PositionOrigin").optional("Limit").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mResult = this->getEvents({ 
                { "Position.x", std::to_string(static_cast<int>(mPosition.x)) },
                { "Position.y", std::to_string(static_cast<int>(mPosition.y)) },
                { "Position.z", std::to_string(static_cast<int>(mPosition.z)) }
            }, {}, param.Limit);
            
            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("dimension").required("Dimension").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({{ "Position.dimension", std::to_string(param.Dimension) }}, {}, param.Limit);

            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("site").required("PositionOrigin").required("Dimension").optional("Limit").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mPositions = this->getEvents({ 
                { "Position.x", std::to_string(static_cast<int>(mPosition.x)) },
                { "Position.y", std::to_string(static_cast<int>(mPosition.y)) },
                { "Position.z", std::to_string(static_cast<int>(mPosition.z)) }
            }, {}, param.Limit);
            std::vector<std::string> mDimensions = this->getEvents({{ "Position.dimension", std::to_string(param.Dimension) }}, {}, param.Limit);
            std::vector<std::string> mResult = SystemUtils::getIntersection({ mPositions, mDimensions });

            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("event").text("custom").required("Target").required("Value").optional("Limit").execute(
            [this](CommandOrigin const&, CommandOutput& output, operation const& param) -> void {
            std::vector<std::string> mResult = this->getEvents({ { param.Target, param.Value } }, {}, param.Limit);

            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("action").text("range").required("PositionOrigin").required("Radius").optional("Limit").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mResult = this->getEventsByPosition(origin.getDimension()->getDimensionId(), [mPosition, radius = param.Radius](int x, int y, int z) -> bool {
                return Vec3(x, y, z).distanceToSqr(mPosition) <= radius;
            }, param.Limit);

            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("query").text("action").text("position").required("PositionOrigin").required("PositionTarget").optional("Limit").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
            Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
            Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

            std::vector<std::string> mResult = this->getEventsByPosition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                return x >= static_cast<double>(mPositionMin.x) && x <= static_cast<double>(mPositionMax.x) && y >= static_cast<double>(mPositionMin.y) && 
                    y <= static_cast<double>(mPositionMax.y) && z >= static_cast<double>(mPositionMin.z) && z <= static_cast<double>(mPositionMax.z);
            }, param.Limit);

            if (mResult.empty())
                return output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, "None");

            output.success(fmt::runtime(tr({}, "commands.behaviorevent.success.query")), param.Limit, fmt::join(mResult, ", "));
        });
        command.overload<operation>().text("back").text("range").required("PositionOrigin").required("Radius").required("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPosition = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            std::vector<std::string> mAreas = this->getEventsByPosition(origin.getDimension()->getDimensionId(), [mPosition, radius = (param.Radius * param.Radius)](int x, int y, int z) -> bool {
                return Vec3(x, y, z).distanceToSqr(mPosition) <= radius;
            });
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::toTimeCalculate(value, time * 3600, "0");
                return !SystemUtils::isPastOrPresent(mTime);
            });
            std::vector<std::string> mTypes = this->getEvents({{ "EventType", "Operable" }});
            std::vector<std::string> mResult = this->filter(
                SystemUtils::getIntersection({ mAreas, mTimes, mTypes })
            );

            if (mResult.empty())
                return output.error(tr({}, "commands.behaviorevent.error.back"));

            this->back(mResult);

            output.success(tr({}, "commands.behaviorevent.success.back"));
        });
        command.overload<operation>().text("back").text("position").required("PositionOrigin").required("PositionTarget").required("Time").execute(
            [this](CommandOrigin const& origin, CommandOutput& output, operation const& param, Command const& cmd) -> void {
            Vec3 mPositionOrigin = param.PositionOrigin.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));
            Vec3 mPositionTarget = param.PositionTarget.getPosition(cmd.mVersion, origin, Vec3(0, 0, 0));

            Vec3 mPositionMin(std::min(mPositionOrigin.x, mPositionTarget.x), std::min(mPositionOrigin.y, mPositionTarget.y), std::min(mPositionOrigin.z, mPositionTarget.z));
            Vec3 mPositionMax(std::max(mPositionOrigin.x, mPositionTarget.x), std::max(mPositionOrigin.y, mPositionTarget.y), std::max(mPositionOrigin.z, mPositionTarget.z));

            std::vector<std::string> mAreas = this->getEventsByPosition(origin.getDimension()->getDimensionId(), [mPositionMin, mPositionMax](int x, int y, int z) -> bool {
                return x >= static_cast<double>(mPositionMin.x) && x <= static_cast<double>(mPositionMax.x) && y >= static_cast<double>(mPositionMin.y) && 
                    y <= static_cast<double>(mPositionMax.y) && z >= static_cast<double>(mPositionMin.z) && z <= static_cast<double>(mPositionMax.z);
            });
            std::vector<std::string> mTimes = this->getEvents({{ "EventTime", "" }}, [time = param.Time](std::string value) -> bool {
                std::string mTime = SystemUtils::toTimeCalculate(value, time * 3600, "0");
                return !SystemUtils::isPastOrPresent(mTime);
            });
            std::vector<std::string> mTypes = this->getEvents({{ "EventType", "Operable" }});
            std::vector<std::string> mResult = this->filter(
                SystemUtils::getIntersection({ mAreas, mTimes, mTypes })
            );

            if (mResult.empty())
                return output.error(tr({}, "commands.behaviorevent.error.back"));

            this->back(mResult);

            output.success(tr({}, "commands.behaviorevent.success.back"));
        });
    }

    void BehaviorEventPlugin::listenEvent() {
        this->mImpl->WriteDatabaseTaskRunning.store(true, std::memory_order_release);
        this->mImpl->CleanDatabaseTaskRunning.store(true, std::memory_order_release);

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->WriteDatabaseTaskRunning.load(std::memory_order_acquire)) {
                co_await this->mImpl->WirteDatabaseTaskSleep.sleepFor(std::chrono::minutes(this->mImpl->options.RefreshIntervalInMinutes));

                if (!this->mImpl->WriteDatabaseTaskRunning.load(std::memory_order_acquire))
                    break;

                Event mEvent{};

                std::vector<Event> mEvents;
                while (this->mImpl->mEvents.try_dequeue(mEvent))
                    mEvents.emplace_back(mEvent);

                std::ranges::sort(mEvents.begin(), mEvents.end(), {}, [](const Event& mEvent) {
                    return std::tie(mEvent.eventName, mEvent.eventTime, mEvent.eventType, mEvent.posX, mEvent.posY, mEvent.posZ, mEvent.dimension);
                });
                auto [first, last] = std::ranges::unique(mEvents.begin(), mEvents.end(), {}, [](const Event& mEvent) {
                    return std::tie(mEvent.eventName, mEvent.eventTime, mEvent.eventType, mEvent.posX, mEvent.posY, mEvent.posZ, mEvent.dimension);
                });
                mEvents.erase(first, last);

                std::for_each(mEvents.begin(), mEvents.end(), [this](const Event& mEvent) mutable -> void {
                    std::string mTismestamp = SystemUtils::getCurrentTimestamp();

                    this->write(mTismestamp, mEvent);
                });
            }
        }).launch(this->mImpl->mExecutor);

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->CleanDatabaseTaskRunning.load(std::memory_order_acquire)) {
                co_await this->mImpl->CleanDatabaseTaskSleep.sleepFor(std::chrono::minutes(this->mImpl->options.CleanDatabaseInterval));

                if (!this->mImpl->CleanDatabaseTaskRunning.load(std::memory_order_acquire))
                    break;

                this->clean(this->mImpl->options.OrganizeDatabaseInterval);
            }
        }).launch(this->mImpl->mExecutor);

        this->registeryEvent<ll::event::PlayerConnectEvent>("PlayerConnect", "Normal", "behaviorevent.event.playerconnect", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerConnect.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerConnect.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerConnect.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerConnectEvent&>(event);

            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerConnectEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z);
        });

        this->registeryEvent<ll::event::PlayerDisconnectEvent>("PlayerDisconnect", "Normal", "behaviorevent.event.playerdisconnect", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerDisconnect.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerDisconnect.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerDisconnect.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerDisconnectEvent&>(event);

            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerDisconnectEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z);
        });

        this->registeryEvent<ll::event::PlayerChatEvent>("PlayerChat", "Normal", "behaviorevent.event.playerchat", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerChat.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerChat.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerChat.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerChatEvent&>(event);

            mEvent.extendedFields.emplace_back("EventMessage", sevent.message());
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerChatEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.message());
        });

        this->registeryEvent<ll::event::PlayerAddExperienceEvent>("PlayerAddExperience", "Normal", "behaviorevent.event.playeraddexperience", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerAddExperience.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerAddExperience.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerAddExperience.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerAddExperienceEvent&>(event);

            mEvent.extendedFields.emplace_back("EventExperience", std::to_string(sevent.experience()));
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerAddExperienceEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.experience());
        });

        this->registeryEvent<ll::event::PlayerAttackEvent>("PlayerAttack", "Normal", "behaviorevent.event.playerattack", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerAttack.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerAttack.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerAttack.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerAttackEvent&>(event);

            mEvent.extendedFields.emplace_back("EventTarget", sevent.target().getTypeName());
            mEvent.extendedFields.emplace_back("EventCause", magic_enum::enum_name(sevent.cause()).data());
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerAttackEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), magic_enum::enum_name(sevent.cause()).data(), sevent.target().getTypeName());
        });

        this->registeryEvent<ll::event::PlayerChangePermEvent>("PlayerChangePerm", "Normal", "behaviorevent.event.playerchangeperm", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerChangePerm.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerChangePerm.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerChangePerm.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerChangePermEvent&>(event);

            mEvent.extendedFields.emplace_back("EventPerm", magic_enum::enum_name(sevent.newPerm()).data());
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerChangePermEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), magic_enum::enum_name(sevent.newPerm()).data());
        });

        this->registeryEvent<ll::event::PlayerDestroyBlockEvent>("PlayerDestroyBlock", "Operable", "behaviorevent.event.playerdestroyblock", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerDestroyBlock.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerDestroyBlock.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerDestroyBlock.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerDestroyBlockEvent&>(event);

            if (auto mBlock = BlockUtils::getBlock(sevent.pos(), sevent.self().getDimensionId()); mBlock.has_value())
                mEvent.extendedFields.emplace_back("EventOperable", mBlock.value()->mSerializationId->toSnbt(SnbtFormat::Minimize, 0));
            if (auto mBlockEntity = BlockUtils::getBlockEntity(sevent.pos(), sevent.self().getDimensionId()); mBlockEntity.has_value()) {
                CompoundTag mTag;
                mBlockEntity.value()->save(mTag, *SaveContextFactory::createCloneSaveContext());

                mEvent.extendedFields.emplace_back("EventBlockEntity", mTag.toSnbt(SnbtFormat::Minimize, 0));
            }
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerDestroyBlockEvent&>(event);

            std::string mBlockType;
            if (auto mBlock = BlockUtils::getBlock(sevent.pos(), sevent.self().getDimensionId()); mBlock.has_value())
                mBlockType = mBlock.value()->getTypeName();
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.pos().x, sevent.pos().y, sevent.pos().z, mBlockType.empty() ? "Unknown" : mBlockType);
        });

        this->registeryEvent<ll::event::PlayerPlacedBlockEvent>("PlayerPlaceBlock", "Operable", "behaviorevent.event.playerplaceblock", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerPlaceBlock.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerPlaceBlock.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerPlaceBlock.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerPlacedBlockEvent&>(event);

            HashedString mHashedId("minecraft:air");

            mEvent.extendedFields.emplace_back("EventOperable", Block::tryGetFromRegistry(mHashedId).value().mSerializationId->toSnbt(SnbtFormat::Minimize, 0));
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerPlacedBlockEvent&>(event);

            BlockPos mPosition = sevent.pos();
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                mPosition.x, mPosition.y, mPosition.z, sevent.self().getCarriedItem().getTypeName());
        });

        this->registeryEvent<ll::event::PlayerDieEvent>("PlayerDie", "Normal", "behaviorevent.event.playerdie", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerDie.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerDie.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerDie.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerDieEvent&>(event);

            mEvent.extendedFields.emplace_back("EventSource", magic_enum::enum_name(sevent.source().mCause).data());
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerDieEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), magic_enum::enum_name(sevent.source().mCause).data(),
                sevent.self().getDimensionId().id, sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z);
        });

        this->registeryEvent<ll::event::PlayerPickUpItemEvent>("PlayerPickUpItem", "Normal", "behaviorevent.event.playerpickupitem", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerPickUpItem.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerPickUpItem.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerPickUpItem.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerPickUpItemEvent&>(event);

            mEvent.extendedFields.emplace_back("EventItem", sevent.itemActor().item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0));
            mEvent.extendedFields.emplace_back("EventOrgCount", std::to_string(sevent.orgCount()));
            mEvent.extendedFields.emplace_back("EventFavoredSlot", std::to_string(sevent.favoredSlot()));
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerPickUpItemEvent&>(event);

            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z, sevent.itemActor().item().getTypeName());
        });

        this->registeryEvent<ll::event::PlayerRespawnEvent>("PlayerRespawn", "Normal", "behaviorevent.event.playerrespawn", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerRespawn.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerRespawn.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerRespawn.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerRespawnEvent&>(event);

            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerRespawnEvent&>(event);
            
            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z);
        });
        
        this->registeryEvent<ll::event::PlayerUseItemEvent>("PlayerUseItem", "Normal", "behaviorevent.event.playeruseitem", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerUseItem.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerUseItem.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerUseItem.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<ll::event::PlayerUseItemEvent&>(event);

            mEvent.extendedFields.emplace_back("EventItem", sevent.item().save(*SaveContextFactory::createCloneSaveContext())->toSnbt(SnbtFormat::Minimize, 0));
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<ll::event::PlayerUseItemEvent&>(event);

            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.self().getDimensionId().id,
                sevent.self().getPosition().x, sevent.self().getPosition().y, sevent.self().getPosition().z, sevent.item().getTypeName());
        });

        this->registeryEvent<LOICollection::ServerEvents::PlayerOpenContainerEvent>("PlayerOpenContainer", "Operable", "behaviorevent.event.playeropencontainer", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onPlayerContainerInteract.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onPlayerContainerInteract.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onPlayerContainerInteract.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<LOICollection::ServerEvents::PlayerOpenContainerEvent&>(event);

            if (auto mBlockEntity = BlockUtils::getBlockEntity(sevent.getPosition(), sevent.getDimensionId()); mBlockEntity.has_value()) {
                CompoundTag mTag;
                mBlockEntity.value()->save(mTag, *SaveContextFactory::createCloneSaveContext());

                mEvent.extendedFields.emplace_back("EventOperableEntity", mTag.toSnbt(SnbtFormat::Minimize, 0));
            }
            mEvent.extendedFields.emplace_back("PlayerName", sevent.self().getRealName());
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<LOICollection::ServerEvents::PlayerOpenContainerEvent&>(event);

            return fmt::format(fmt::runtime(message), sevent.self().getRealName(), sevent.getDimensionId(),
                sevent.getPosition().x, sevent.getPosition().y, sevent.getPosition().z);
        });

        this->registeryEvent<LOICollection::ServerEvents::BlockExplodedEvent>("BlockExplode", "Operable", "behaviorevent.event.blockexplode", [this](BehaviorEventConfig config) -> bool {
            switch (config) {
                case BehaviorEventConfig::ModuleEnabled: return this->mImpl->options.Events.onBlockExplode.ModuleEnabled;
                case BehaviorEventConfig::RecordDatabase: return this->mImpl->options.Events.onBlockExplode.RecordDatabase;
                case BehaviorEventConfig::OutputConsole: return this->mImpl->options.Events.onBlockExplode.OutputConsole;
            }

            return false;
        }, [](ll::event::Event& event, Event& mEvent) -> void {
            auto& sevent = static_cast<LOICollection::ServerEvents::BlockExplodedEvent&>(event);

            mEvent.extendedFields.emplace_back("EventOperable", sevent.getBlock().mSerializationId->toSnbt(SnbtFormat::Minimize, 0));
            if (auto mBlockEntity = BlockUtils::getBlockEntity(sevent.getPosition(), sevent.getDimensionId()); mBlockEntity.has_value()) {
                CompoundTag mTag;
                mBlockEntity.value()->save(mTag, *SaveContextFactory::createCloneSaveContext());

                mEvent.extendedFields.emplace_back("EventBlockEntity", mTag.toSnbt(SnbtFormat::Minimize, 0));
            }
        }, [](std::string message, ll::event::Event& event) -> std::string {
            auto& sevent = static_cast<LOICollection::ServerEvents::BlockExplodedEvent&>(event);

            return fmt::format(fmt::runtime(message), sevent.getBlock().getTypeName(), sevent.getDimensionId(),
                sevent.getPosition().x, sevent.getPosition().y, sevent.getPosition().z);
        });
    }

    void BehaviorEventPlugin::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        for (auto& listener : this->mImpl->mListeners)
            eventBus.removeListener(listener.second);

        this->mImpl->mListeners.clear();

        this->mImpl->WriteDatabaseTaskRunning.store(false, std::memory_order_release);
        this->mImpl->CleanDatabaseTaskRunning.store(false, std::memory_order_release);

        this->mImpl->WirteDatabaseTaskSleep.interrupt();
        this->mImpl->CleanDatabaseTaskSleep.interrupt();
    }

    BehaviorEventPlugin::Event BehaviorEventPlugin::getBasicEvent(const std::string& name, const std::string& type, const Vec3& position, int dimension) {
        if (!this->isValid())
            return {};

        Event mEvent;
        mEvent.eventName = name;
        mEvent.eventTime = SystemUtils::getNowTime();
        mEvent.eventType = type;
        mEvent.posX = static_cast<int>(position.x);
        mEvent.posY = static_cast<int>(position.y);
        mEvent.posZ = static_cast<int>(position.z);
        mEvent.dimension = dimension;

        return mEvent;
    }

    std::vector<std::string> BehaviorEventPlugin::getEvents(int limit) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mKeys = this->getDatabase()->listByPrefix("Events", "%.EventName");

        auto mView = mKeys
            | std::views::take(limit > 0 ? limit : static_cast<int>(mKeys.size()))
            | std::views::transform([](const std::string& mKey) -> std::string {
                size_t mPos = mKey.find('.');
                return mPos != std::string::npos ? mKey.substr(0, mPos) : "";
            });

        return mView | std::ranges::to<std::vector<std::string>>();
    }

    std::vector<std::string> BehaviorEventPlugin::getEvents(std::vector<std::pair<std::string, std::string>> conditions, std::function<bool(std::string)> filter, int limit) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mKeys = this->getEvents(limit);

        std::unordered_map<size_t, std::vector<std::string>> mResultLocal;
        std::for_each(std::execution::par, mKeys.begin(), mKeys.end(), [this, &mResultLocal, conditions, filter](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> mEvent = this->getDatabase()->getByPrefix("Events", mId + ".");

            auto mView = conditions | std::views::filter([&mEvent, &mId, filter](const std::pair<std::string, std::string>& mCondition) -> bool {
                auto it = mEvent.find(mId + "." + mCondition.first);
                if (it == mEvent.end())
                    return false;

                return (!filter && it->second == mCondition.second) || (filter && filter(it->second));
            });

            if (static_cast<size_t>(std::ranges::distance(mView)) == conditions.size()) {
                size_t id = std::hash<std::thread::id>()(std::this_thread::get_id());

                mResultLocal[id].emplace_back(mId);
            }
        });

        std::vector<std::string> mResult;
        for (auto& mLocal : mResultLocal)
            mResult.insert(mResult.end(), mLocal.second.begin(), mLocal.second.end());

        return mResult;
    }

    std::vector<std::string> BehaviorEventPlugin::getEventsByPosition(int dimension, std::function<bool(int x, int y, int z)> filter, int limit) {
        if (!this->isValid())
            return {};

        std::vector<std::string> mKeys = this->getEvents(limit);

        std::unordered_map<size_t, std::vector<std::string>> mResultLocal;
        std::for_each(std::execution::par, mKeys.begin(), mKeys.end(), [this, &mResultLocal, dimension, filter](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> data = this->getDatabase()->getByPrefix("Events", mId + ".");

            if (SystemUtils::toInt(data.at(mId + ".Position.dimension"), 0) != dimension)
                return;
            
            int x = SystemUtils::toInt(data.at(mId + ".Position.x"), 0);
            int y = SystemUtils::toInt(data.at(mId + ".Position.y"), 0);
            int z = SystemUtils::toInt(data.at(mId + ".Position.z"), 0);

            if (filter(x, y, z)) {
                size_t id = std::hash<std::thread::id>()(std::this_thread::get_id());

                mResultLocal[id].emplace_back(mId);
            }
        });

        std::vector<std::string> mResult;
        for (auto& mLocal : mResultLocal)
            mResult.insert(mResult.end(), mLocal.second.begin(), mLocal.second.end());

        return mResult;
    }

    std::vector<std::string> BehaviorEventPlugin::filter(std::vector<std::string> ids) {
        if (!this->isValid())
            return {};

        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> mEvents;

        SQLiteStorageTransaction transaction(*this->getDatabase());
        std::for_each(ids.begin(), ids.end(), [this, connection = transaction.connection(), &mEvents](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> data = this->getDatabase()->getByPrefix(connection, "Events", mId + ".");

            auto it = mEvents.find(mId);
            if (it == mEvents.end())
                mEvents[mId] = data;
            else if (
                it->second.at(mId + ".Position.x") == data.at(mId + ".Position.x") &&
                it->second.at(mId + ".Position.y") == data.at(mId + ".Position.y") &&
                it->second.at(mId + ".Position.z") == data.at(mId + ".Position.z") &&
                it->second.at(mId + ".Position.dimension") == data.at(mId + ".Position.dimension")
            ) it->second = data;
        });
        transaction.commit();

        return std::views::keys(mEvents) | std::ranges::to<std::vector<std::string>>();
    }

    void BehaviorEventPlugin::write(const std::string& id, const Event& event) {
        if (!this->isValid())
            return;

        SQLiteStorageTransaction transaction(*this->getDatabase());

        auto connection = transaction.connection();

        this->getDatabase()->set(connection, "Events", id + ".EventName", event.eventName);
        this->getDatabase()->set(connection, "Events", id + ".EventTime", event.eventTime);
        this->getDatabase()->set(connection, "Events", id + ".EventType", event.eventType);
        this->getDatabase()->set(connection, "Events", id + ".Position.x", std::to_string(event.posX));
        this->getDatabase()->set(connection, "Events", id + ".Position.y", std::to_string(event.posY));
        this->getDatabase()->set(connection, "Events", id + ".Position.z", std::to_string(event.posZ));
        this->getDatabase()->set(connection, "Events", id + ".Position.dimension", std::to_string(event.dimension));
        
        for (auto& fieled : event.extendedFields)
            this->getDatabase()->set(connection, "Events", id + "." + fieled.first, fieled.second);

        transaction.commit();
    }

    void BehaviorEventPlugin::back(std::vector<std::string>& ids) {
        if (!this->isValid())
            return;

        SQLiteStorageTransaction transaction(*this->getDatabase());
        std::for_each(ids.begin(), ids.end(), [this, connection = transaction.connection()](const std::string& mId) -> void {
            std::unordered_map<std::string, std::string> data = this->getDatabase()->getByPrefix(connection, "Events", mId + ".");

            BlockPos mPosition(
                SystemUtils::toInt(data.at(mId + ".Position.x"), 0),
                SystemUtils::toInt(data.at(mId + ".Position.y"), 0),
                SystemUtils::toInt(data.at(mId + ".Position.z"), 0)
            );
            int mDimension = SystemUtils::toInt(data.at(mId + ".Position.dimension"), 0);

            if (data.contains(mId + ".EventOperable")) {
                CompoundTag mNbt = CompoundTag::fromSnbt(data.at(mId + ".EventOperable"))->mTags;

                BlockUtils::setBlock(mPosition, mDimension, mNbt);
            } else if (data.contains(mId + ".EventOperableEntity")) {
                CompoundTag mNbt = CompoundTag::fromSnbt(data.at(mId + ".EventOperableEntity"))->mTags;

                BlockUtils::setBlockEntity(mPosition, mDimension, mNbt);
            }

            this->getDatabase()->delByPrefix(connection, "Events", mId + ".");
        });
        transaction.commit();
    }

    void BehaviorEventPlugin::clean(int hours) {
        if (!this->isValid())
            return;

        std::vector<std::string> mKeys = this->getEvents();

        SQLiteStorageTransaction transaction(*this->getDatabase());
        std::for_each(mKeys.begin(), mKeys.end(), [this, connection = transaction.connection(), hours](const std::string& mId) mutable {
            std::string mEventTime = this->getDatabase()->get(connection, "Events", mId + ".EventTime");
            
            std::string mTime = SystemUtils::toTimeCalculate(mEventTime, hours * 3600, "0");
            if (SystemUtils::isPastOrPresent(mTime))
                this->getDatabase()->delByPrefix(connection, "Events", mId + ".");
        });
        transaction.commit();
    }

    bool BehaviorEventPlugin::isValid() {
        return this->getLogger() != nullptr && this->getDatabase() != nullptr;
    }

    bool BehaviorEventPlugin::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.BehaviorEvent.ModuleEnabled)
            return false;

        auto mDataPath = std::filesystem::path(ServiceProvider::getInstance().getService<std::string>("DataPath")->data());

        this->mImpl->db = std::make_unique<SQLiteStorage>((mDataPath / "behaviorevent.db").string());
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().Plugins.BehaviorEvent;

        return true;
    }

    bool BehaviorEventPlugin::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->db.reset();
        this->mImpl->logger.reset();
        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();

        return true;
    }

    bool BehaviorEventPlugin::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->getDatabase()->create("Events");

        this->registeryCommand();
        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }
    
    bool BehaviorEventPlugin::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->getDatabase()->exec("VACUUM;");

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("BehaviorEventPlugin", LOICollection::Plugins::BehaviorEventPlugin, LOICollection::Plugins::BehaviorEventPlugin::getInstance())
