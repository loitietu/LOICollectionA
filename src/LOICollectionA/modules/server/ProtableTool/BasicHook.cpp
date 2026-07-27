#include <atomic>
#include <memory>
#include <string>

#include <ll/api/Expected.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/ListenerBase.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/network/MinecraftPacketIds.h>
#include <mc/network/packet/StartGamePacket.h>

#include "LOICollectionA/include/server/Events/server/NetworkPacketEvent.h"

#include "LOICollectionA/base/Wrapper.h"
#include "LOICollectionA/base/ServiceProvider.h"

#include "LOICollectionA/ConfigPlugin.h"

#include "LOICollectionA/include/server/ProtableTool/BasicHook.h"

namespace LOICollection::server::ProtableTool {
    struct BasicHook::Impl {
        std::atomic<bool> mRegistered{ false };

        Config::C_BasicHook options;

        ll::event::ListenerPtr NetworkPacketEventListener;
    };  

    BasicHook::BasicHook() : mImpl(std::make_unique<Impl>()) {};
    BasicHook::~BasicHook() = default;

    std::shared_ptr<BasicHook> BasicHook::getShared() {
        static auto instance = std::shared_ptr<BasicHook>(new BasicHook());
        return instance;
    }

    void BasicHook::listenEvent() {
        ll::event::EventBus& eventBus = ll::event::EventBus::getInstance();
        this->mImpl->NetworkPacketEventListener = eventBus.emplaceListener<LOICollection::server::Events::NetworkPacketBeforeEvent>([this](LOICollection::server::Events::NetworkPacketBeforeEvent& event) -> void {
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

    std::string BasicHook::getName() {
        return "BasicHook";
    }

    modules::ModulePriority BasicHook::getPriority() {
        return modules::ModulePriority::Normal;
    }

    ll::Expected<bool> BasicHook::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.ProtableTool.BasicHook.ModuleEnabled)
            return false;

        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<Config::C_Config>>("Config")->get().ServerConfig.ProtableTool.BasicHook;

        return true;
    }

    ll::Expected<bool> BasicHook::unload() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->mImpl->options = {};

        if (this->mImpl->mRegistered.load(std::memory_order_acquire))
            this->unlistenEvent();
        
        return true;
    }

    ll::Expected<bool> BasicHook::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        this->listenEvent();

        this->mImpl->mRegistered.store(true, std::memory_order_release);
        
        return true;
    }

    ll::Expected<bool> BasicHook::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        this->unlistenEvent();

        this->mImpl->mRegistered.store(false, std::memory_order_release);

        return true;
    }
}
