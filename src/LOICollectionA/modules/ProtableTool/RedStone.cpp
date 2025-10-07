#include <memory>
#include <unordered_map>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

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

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/ProtableTool/RedStone.h"

namespace LOICollection::ProtableTool {
    struct RedStone::Impl {
        std::unordered_map<DimensionType, std::unordered_map<BlockPos, int>> mRedStoneMap;

        bool ModuleEnabled = false;
        int mRedStoneTick = 0;

        std::shared_ptr<ll::io::Logger> logger;

        bool RedStoneTaskRunning = true;
    };
}

#define RedStoneUpdateHookMacro(NAME, TYPE, SYMBOL, VAL, ...)                               \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, void, __VA_ARGS__) {    \
        auto& instance = LOICollection::ProtableTool::RedStone::getInstance();              \
        int mDimensionId = region.getDimensionId();                                         \
        auto& mDimensionMap = instance.mImpl->mRedStoneMap[mDimensionId];                   \
        mDimensionMap[pos]++;                                                               \
        origin VAL;                                                                         \
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
    return ll::service::getLevel()->getOrCreateDimension(mDimensionId).lock()->getBlockSourceFromMainChunkSource();
}

namespace LOICollection::ProtableTool {
    RedStone::RedStone() : mImpl(std::make_unique<Impl>()) {};
    RedStone::~RedStone() = default;

    ll::io::Logger* RedStone::getLogger() {
        return this->mImpl->logger.get();
    }

    bool RedStone::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.RedStone)
            return false;

        this->mImpl->mRedStoneTick = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.RedStone;
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool RedStone::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        RedStoneWireBlockHook::hook();
        RedStoneTorchBlockHook::hook();
        DiodeBlockHook::hook();
        ComparatorBlockHook::hook();
        ObserverBlockHook::hook();

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->RedStoneTaskRunning) {
                co_await ll::chrono::ticks(20);
                if (this->mImpl->mRedStoneMap.empty()) 
                    continue;

                for (auto& it : this->mImpl->mRedStoneMap) {
                    for (auto it2 = it.second.begin(); it2 != it.second.end(); ++it2) {
                        if (it2->second >= this->mImpl->mRedStoneTick) ll::service::getLevel()->destroyBlock(getBlockSource(it.first), it2->first, true);

                        this->getLogger()->info("RedStone: {}({})", it2->first.toString(), it2->second);
                    }
                }

                this->mImpl->mRedStoneMap.clear();
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        return true;
    }

    bool RedStone::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        RedStoneWireBlockHook::unhook();
        RedStoneTorchBlockHook::unhook();
        DiodeBlockHook::unhook();
        ComparatorBlockHook::unhook();
        ObserverBlockHook::unhook();

        this->mImpl->RedStoneTaskRunning = false;

        return true;
    }
}

REGISTRY_HELPER("RedStone", LOICollection::ProtableTool::RedStone, LOICollection::ProtableTool::RedStone::getInstance())
