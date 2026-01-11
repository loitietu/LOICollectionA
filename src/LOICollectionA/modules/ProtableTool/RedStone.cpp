#include <atomic>
#include <memory>
#include <unordered_map>

#include <ll/api/io/Logger.h>
#include <ll/api/io/LoggerRegistry.h>

#include <ll/api/coro/CoroTask.h>
#include <ll/api/coro/InterruptableSleep.h>
#include <ll/api/thread/ServerThreadExecutor.h>

#include <ll/api/chrono/GameChrono.h>
#include <ll/api/service/Bedrock.h>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/BlockChangeContext.h>
#include <mc/world/level/dimension/Dimension.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/ServerEvents/world/RedStoneEvent.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/ProtableTool/RedStone.h"

BlockSource& getBlockSource(int mDimensionId) {
    return ll::service::getLevel()->getOrCreateDimension(mDimensionId).lock()->getBlockSourceFromMainChunkSource();
}

namespace LOICollection::ProtableTool {
    struct RedStone::Impl {
        std::unordered_map<int, std::unordered_map<BlockPos, int>> mRedStoneMap;

        std::atomic<bool> mRegistered{ false };

        bool ModuleEnabled = false;
        int mRedStoneTick = 0;

        std::shared_ptr<ll::io::Logger> logger;

        ll::event::ListenerPtr mRedStoneEventListener;

        ll::coro::InterruptableSleep RedStoneTaskSleep;

        std::atomic<bool> RedStoneTaskRunning{ false };
    };

    RedStone::RedStone() : mImpl(std::make_unique<Impl>()) {};
    RedStone::~RedStone() = default;

    RedStone& RedStone::getInstance() {
        static RedStone instance;
        return instance;
    }

    ll::io::Logger* RedStone::getLogger() {
        return this->mImpl->logger.get();
    }

    void RedStone::listenEvent() {
        this->mImpl->RedStoneTaskRunning.store(true, std::memory_order_release);

        ll::coro::keepThis([this]() -> ll::coro::CoroTask<> {
            while (this->mImpl->RedStoneTaskRunning.load(std::memory_order_acquire)) {
                co_await this->mImpl->RedStoneTaskSleep.sleepFor(ll::chrono::ticks(20));

                if (this->mImpl->mRedStoneMap.empty()) 
                    continue;

                for (auto& it : this->mImpl->mRedStoneMap) {
                    for (auto it2 = it.second.begin(); it2 != it.second.end(); ++it2) {
                        if (it2->second >= this->mImpl->mRedStoneTick) {
                            ll::service::getLevel()->destroyBlock(getBlockSource(it.first), it2->first, true, BlockChangeContext());
                            
                            this->getLogger()->info("RedStone: {}({})", it2->first.toString(), it.first);
                        }
                    }
                }

                this->mImpl->mRedStoneMap.clear();
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());

        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->mRedStoneEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::RedStoneEvent>([this](LOICollection::ServerEvents::RedStoneEvent& event) mutable -> void {
            this->mImpl->mRedStoneMap[event.getDimensionId()][event.getPosition()]++;
        });
    }
    
    void RedStone::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->mRedStoneEventListener);

        this->mImpl->RedStoneTaskRunning.store(false, std::memory_order_release);

        this->mImpl->RedStoneTaskSleep.interrupt();
    }

    bool RedStone::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ProtableTool.RedStone)
            return false;

        this->mImpl->mRedStoneTick = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ProtableTool.RedStone;
        this->mImpl->logger = ll::io::LoggerRegistry::getInstance().getOrCreate("LOICollectionA");
        this->mImpl->ModuleEnabled = true;

        return true;
    }

    bool RedStone::unload() {
        if (!this->mImpl->ModuleEnabled)
            return false;
        
        this->mImpl->logger.reset();
        this->mImpl->ModuleEnabled = false;

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();
        
        return true;
    }

    bool RedStone::registry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);

        return true;
    }

    bool RedStone::unregistry() {
        if (!this->mImpl->ModuleEnabled)
            return false;

        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("RedStone", LOICollection::ProtableTool::RedStone, LOICollection::ProtableTool::RedStone::getInstance(), LOICollection::modules::ModulePriority::Normal)
