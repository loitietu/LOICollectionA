#include <memory>
#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/network/packet/StartGamePacket.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include "include/RegistryHelper.h"

#include "base/Wrapper.h"
#include "base/ServiceProvider.h"

#include "ConfigPlugin.h"

#include "include/ProtableTool/BasicHook.h"

namespace LOICollection::ProtableTool {
    struct BasicHook::Impl {
        C_Config::C_ProtableTool::C_BasicHook options;
    };
}

LL_TYPE_INSTANCE_HOOK(
    ServerStartGamePacketHook,
    HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    if (std::string mFakeSeed = LOICollection::ProtableTool::BasicHook::getInstance().mImpl->options.FakeSeed; !mFakeSeed.empty()) {
        const char* ptr = mFakeSeed.data();
        char* endpt{};
        int64 result = std::strtoll(ptr, &endpt, 10);
        this->mSettings->mSeed = LevelSeed64(ptr == endpt ? ll::random_utils::rand<int64_t>() : result);
    }
    return origin(stream);
};

namespace LOICollection::ProtableTool {
    BasicHook::BasicHook() : mImpl(std::make_unique<Impl>()) {};
    BasicHook::~BasicHook() = default;

    bool BasicHook::load() {
        if (!ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.BasicHook.ModuleEnabled)
            return false;

        this->mImpl->options = ServiceProvider::getInstance().getService<ReadOnlyWrapper<C_Config>>("Config")->get().ProtableTool.BasicHook;

        return true;
    }

    bool BasicHook::registry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;

        ServerStartGamePacketHook::hook();
        return true;
    }

    bool BasicHook::unregistry() {
        if (!this->mImpl->options.ModuleEnabled)
            return false;
        
        ServerStartGamePacketHook::unhook();

        return true;
    }
}

REGISTRY_HELPER("BasicHook", LOICollection::ProtableTool::BasicHook, LOICollection::ProtableTool::BasicHook::getInstance())
