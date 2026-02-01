#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/DiodeBlock.h>
#include <mc/world/level/block/ObserverBlock.h>
#include <mc/world/level/block/ComparatorBlock.h>
#include <mc/world/level/block/RedStoneWireBlock.h>
#include <mc/world/level/block/RedstoneTorchBlock.h>
#include <mc/world/level/block/actor/PistonBlockActor.h>
#include <mc/world/level/block/block_events/BlockRedstoneUpdateEvent.h>
#include <mc/world/level/dimension/Dimension.h>

#include "LOICollectionA/include/ServerEvents/world/RedStoneEvent.h"

namespace LOICollection::ServerEvents {
    const BlockPos& RedStoneEvent::getPosition() const {
        return mPosition;
    }

    const BlockSource& RedStoneEvent::getSource() const {
        return mSource;
    }

    int RedStoneEvent::getDimensionId() const {
        return mDimensionId;
    }

    int RedStoneEvent::getStrength() const {
        return mStrength;
    }

    bool RedStoneEvent::isFirstTime() const {
        return mIsFirstTime;
    }

    LL_TYPE_INSTANCE_HOOK(
        RedStoneWireBlockHook,
        HookPriority::Normal,
        RedStoneWireBlock,
        &RedStoneWireBlock::_onRedstoneUpdate,
        void,
        BlockEvents::BlockRedstoneUpdateEvent& blockEvent
    ) {
        RedStoneEvent event(blockEvent.mPos, blockEvent.mRegion, blockEvent.mRegion.getDimensionId(), blockEvent.mSignalLevel, blockEvent.mIsFirstTime);
        ll::event::EventBus::getInstance().publish(event);
        
        origin(blockEvent);
    }

    LL_TYPE_INSTANCE_HOOK(
        RedStoneTorchBlockHook,
        HookPriority::Normal,
        RedstoneTorchBlock,
        &RedstoneTorchBlock::_onRedstoneUpdate,
        void,
        BlockEvents::BlockRedstoneUpdateEvent& blockEvent
    ) {
        RedStoneEvent event(blockEvent.mPos, blockEvent.mRegion, blockEvent.mRegion.getDimensionId(), blockEvent.mSignalLevel, blockEvent.mIsFirstTime);
        ll::event::EventBus::getInstance().publish(event);
        
        origin(blockEvent);
    }

    LL_TYPE_INSTANCE_HOOK(
        ComparatorBlockHook,
        HookPriority::Normal,
        ComparatorBlock,
        &ComparatorBlock::_onRedstoneUpdate,
        void,
        BlockEvents::BlockRedstoneUpdateEvent& blockEvent
    ) {
        RedStoneEvent event(blockEvent.mPos, blockEvent.mRegion, blockEvent.mRegion.getDimensionId(), blockEvent.mSignalLevel, blockEvent.mIsFirstTime);
        ll::event::EventBus::getInstance().publish(event);
        
        origin(blockEvent);
    }

    LL_TYPE_INSTANCE_HOOK(
        ObserverBlockHook,
        HookPriority::Normal,
        ObserverBlock,
        &ObserverBlock::_updateState,
        void,
        BlockSource& region,
        BlockPos const& pos,
        PulseCapacitor& component,
        bool turnOn
    ) {
        RedStoneEvent event(pos, region, region.getDimensionId(), 15, false);
        ll::event::EventBus::getInstance().publish(event);
        
        origin(region, pos, component, turnOn);
    }

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class RedStoneEventEmitter : public ll::event::Emitter<emitterFactory, RedStoneEvent> {
        ll::memory::HookRegistrar<RedStoneWireBlockHook, RedStoneTorchBlockHook, ComparatorBlockHook, ObserverBlockHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<RedStoneEventEmitter>();
    }
}