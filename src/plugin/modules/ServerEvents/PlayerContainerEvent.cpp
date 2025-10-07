#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/deps/core/math/Vec3.h>

#include <mc/world/actor/Actor.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/level/BlockPos.h>
#include <mc/world/level/dimension/Dimension.h>

#include <mc/world/events/EventResult.h>
#include <mc/world/events/PlayerOpenContainerEvent.h>

#include <mc/server/module/VanillaServerGameplayEventListener.h>

#include "include/ServerEvents/PlayerContainerEvent.h"

namespace LOICollection::ServerEvents {
    BlockPos& PlayerOpenContainerEvent::getPosition() const {
        return mPosition;
    }

    int PlayerOpenContainerEvent::getDimensionId() const {
        return mDimensionId;
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerOpenContainerHook,
        HookPriority::Normal,
        VanillaServerGameplayEventListener,
        &VanillaServerGameplayEventListener::$onEvent,
        EventResult,
        ::PlayerOpenContainerEvent const& playerOpenContainerEvent
    ) {
        Actor* actor = static_cast<WeakEntityRef*>((void*)&playerOpenContainerEvent)->tryUnwrap<Actor>();

        if (!actor || !actor->isPlayer())
            return origin(playerOpenContainerEvent);

        BlockPos mPosition = playerOpenContainerEvent.mBlockPos;

        PlayerOpenContainerEvent event(*static_cast<Player*>(actor), mPosition, actor->getDimensionId());
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled())
            return EventResult::StopProcessing;

        return origin(playerOpenContainerEvent);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class PlayerOpenContainerEventEmitter : public ll::event::Emitter<emitterFactory, PlayerOpenContainerEvent> {
        ll::memory::HookRegistrar<PlayerOpenContainerHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<PlayerOpenContainerEventEmitter>();
    }
}