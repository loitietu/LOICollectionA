#include <unordered_map>

#include <ll/api/memory/Hook.h>
#include <ll/api/schedule/Task.h>
#include <ll/api/schedule/Scheduler.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/chrono/GameChrono.h>

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

using namespace ll::chrono_literals;

std::unordered_map<DimensionType, std::unordered_map<BlockPos, int>> mRedStoneMap;

#define RedStoneUpdateHookMacro(NAME, TYPE, SYMBOL, VAL, ...)                               \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, void, __VA_ARGS__) {    \
        int mDimensionId = region.getDimensionId();                                         \
        if (mRedStoneMap.find(mDimensionId) == mRedStoneMap.end())                          \
            mRedStoneMap[mDimensionId] = std::unordered_map<BlockPos, int>();               \
        if (mRedStoneMap[mDimensionId].find(pos) == mRedStoneMap[mDimensionId].end())       \
            mRedStoneMap[mDimensionId][pos] = 1;                                            \
        else mRedStoneMap[mDimensionId][pos]++;                                             \
        return origin VAL;                                                                  \
    };                                                                                      \

RedStoneUpdateHookMacro(RedStoneWireBlockHook, RedStoneWireBlock,
    "?onRedstoneUpdate@RedStoneWireBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@H_N@Z",
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(RedStoneTorchBlockHook, RedstoneTorchBlock,
    "?onRedstoneUpdate@RedstoneTorchBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@H_N@Z",
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(DiodeBlockHook, DiodeBlock,
    "?onRedstoneUpdate@DiodeBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@H_N@Z",
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(ComparatorBlockHook, ComparatorBlock,
    "?onRedstoneUpdate@ComparatorBlock@@UEBAXAEAVBlockSource@@AEBVBlockPos@@H_N@Z",
    (region, pos, strength, isFirstTime),
    BlockSource& region, BlockPos const& pos,
    int strength, bool isFirstTime
)

RedStoneUpdateHookMacro(ObserverBlockHook, ObserverBlock, &ObserverBlock::_updateState,
    (region, pos, component, turnOn),
    BlockSource& region, BlockPos const& pos, 
    PulseCapacitor& component, bool turnOn
)

BlockSource& getBlockSource(DimensionType mDimensionId) {
    return ll::service::getLevel()->getOrCreateDimension(mDimensionId)->getBlockSourceFromMainChunkSource();
}

namespace LOICollection::ProtableTool::RedStone {
    int mRedStoneTick;

    void registery(int mTick) {
        mRedStoneTick = mTick;

        RedStoneWireBlockHook::hook();
        RedStoneTorchBlockHook::hook();
        DiodeBlockHook::hook();
        ComparatorBlockHook::hook();
        ObserverBlockHook::hook();

        static ll::schedule::ServerTimeScheduler scheduler;
        scheduler.add<ll::schedule::RepeatTask>(1s, [] {
            if (mRedStoneMap.empty()) return;
            for (auto it = mRedStoneMap.begin(); it != mRedStoneMap.end(); ++it) {
                for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2)
                    if (it2->second >= mRedStoneTick) ll::service::getLevel()->destroyBlock(getBlockSource(it->first), it2->first, true);
            }
            mRedStoneMap.clear();
        });
    }

    void unregistery() {
        RedStoneWireBlockHook::unhook();
        RedStoneTorchBlockHook::unhook();
        DiodeBlockHook::unhook();
        ComparatorBlockHook::unhook();
        ObserverBlockHook::unhook();
    }
}