#include <atomic>
#include <memory>
#include <string>

#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/packet/StartGamePacket.h>

#include "LOICollectionA/include/RegistryHelper.h"

#include "LOICollectionA/include/ServerEvents/NetworkPacketEvent.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/ProtableTool/BasicHook.h"

namespace LOICollection::ProtableTool {
    struct BasicHook::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_BasicHook options;

        ll::event::ListenerPtr NetworkPacketEventListener;
    };  

    BasicHook::BasicHook() : mImpl(std::make_unique<Impl>()) {};
    BasicHook::~BasicHook() = default;

    BasicHook& BasicHook::getInstance() {
        static BasicHook instance;
        return instance;
    };

    void BasicHook::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->NetworkPacketEventListener = eventBus.emplaceListener<LOICollection::ServerEvents::NetworkPacketEvent>([this](LOICollection::ServerEvents::NetworkPacketEvent& event) -> void {
            if (event.getPacket().getId() != MinecraftPacketIds::StartGame)
                return;

            auto& packet = static_cast<StartGamePacket&>(const_cast<Packet&>(event.getPacket()));

            if (!this->mImpl->options.FakeSeed.empty()) {
                const char* ptr = this->mImpl->options.FakeSeed.data();
                char* endpt{};
                int64 result = std::strtoll(ptr, &endpt, 10);
                
                packet.mSettings->mSeed = LevelSeed64(ptr == endpt ? ll::random_utils::rand<int64_t>() : result);
            }
        });
    }

    void BasicHook::unlistenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener(this->mImpl->NetworkPacketEventListener);
    }

    bool BasicHook::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ProtableTool.BasicHook.ModuleEnabled)
            return false;

        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ProtableTool.BasicHook;

        return true;
    }

    bool BasicHook::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();
        
        return true;
    }

    bool BasicHook::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);
        
        return true;
    }

    bool BasicHook::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}

REGISTRY_HELPER("BasicHook", LOICollection::ProtableTool::BasicHook, LOICollection::ProtableTool::BasicHook::getInstance(), LOICollection::modules::ModulePriority::Normal)
