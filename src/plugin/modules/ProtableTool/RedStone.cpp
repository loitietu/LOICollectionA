#include <unordered_map>

#include <ll/api/memory/Hook.h>
#include <ll/api/coro/CoroTask.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/DiodeBlock.h>
#include <mc/world/level/block/ObserverBlock.h>
#include <mc/world/level/block/ComparatorBlock.h>
#include <mc/world/level/block/RedStoneWireBlock.h>
#include <mc/world/level/block/RedstoneTorchBlock.h>
#include <mc/world/level/block/actor/PistonBlockActor.h>
#include <mc/world/level/dimension/Dimension.h>
#include <mc/_HeaderOutputPredefine.h>

#include "include/ProtableTool/RedStone.h"

std::unordered_map<DimensionType, std::unordered_map<BlockPos, int>> mRedStoneMap;

#define RedStoneUpdateHookMacro(NAME, TYPE, SYMBOL, VAL, ...)                               \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, void, __VA_ARGS__) {    \
        int mDimensionId = region.getDimensionId();                                         \
        std::unordered_map<BlockPos, int>& mDimensionMap = mRedStoneMap[mDimensionId];      \
        mDimensionMap[pos]++;                                                               \
        return origin VAL;                                                                  \
    };                                                                                      \

RedStoneUpdateHookMacro(RedStoneWireBlockHook, RedStoneWireBlock,
    &RedStoneWireBlock::$onRedstoneUpdate,
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(RedStoneTorchBlockHook, RedstoneTorchBlock,
    &RedstoneTorchBlock::$onRedstoneUpdate,
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(DiodeBlockHook, DiodeBlock,
    &DiodeBlock::$onRedstoneUpdate,
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(ComparatorBlockHook, ComparatorBlock,
    &ComparatorBlock::$onRedstoneUpdate,
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(ObserverBlockHook, ObserverBlock,
    &ObserverBlock::_updateState,
    (region, pos, component, turnOn),
    BlockSource& region, BlockPos const& pos, 
    PulseCapacitor& component, bool turnOn
)

BlockSource& getBlockSource(DimensionType mDimensionId) {
    return ll::service::getLevel()->getOrCreateDimension(mDimensionId)->getBlockSourceFromMainChunkSource();
}

namespace LOICollection::ProtableTool::RedStone {
    int mRedStoneTick = 0;
    bool RedStoneTaskRunning = true;

    void registery(int mTick) {
        mRedStoneTick = mTick;

        RedStoneWireBlockHook::hook();
        RedStoneTorchBlockHook::hook();
        DiodeBlockHook::hook();
        ComparatorBlockHook::hook();
        ObserverBlockHook::hook();

        ll::coro::keepThis([]() -> ll::coro::CoroTask<> {
            while (RedStoneTaskRunning) {
                co_await ll::chrono::ticks(20);
                if (mRedStoneMap.empty()) 
                    continue;
                for (auto it = mRedStoneMap.begin(); it != mRedStoneMap.end(); ++it) {
                    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
                        if (it2->second >= mRedStoneTick) ll::service::getLevel()->destroyBlock(getBlockSource(it->first), it2->first, true);
                }
                mRedStoneMap.clear();
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());
    }

    void unregistery() {
        RedStoneWireBlockHook::unhook();
        RedStoneTorchBlockHook::unhook();
        DiodeBlockHook::unhook();
        ComparatorBlockHook::unhook();
        ObserverBlockHook::unhook();

        RedStoneTaskRunning = false;
    }
}