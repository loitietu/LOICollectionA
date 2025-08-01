#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/network/packet/StartGamePacket.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include "include/ProtableTool/BasicHook.h"

std::string mFakeSeed;

LL_TYPE_INSTANCE_HOOK(
    ServerStartGamePacketHook,
    HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    if (!mFakeSeed.empty()) {
        const char* ptr = mFakeSeed.data();
        char* endpt{};
        int64 result = std::strtoll(ptr, &endpt, 10);
        this->mSettings->mSeed = LevelSeed64(ptr == endpt ? ll::random_utils::rand<int64_t>() : result);
    }
    return origin(stream);
};

namespace LOICollection::ProtableTool::BasicHook {
    void registery(const std::string& fakeSeed) {
        mFakeSeed = fakeSeed;

        ServerStartGamePacketHook::hook();
    }

    void unregistery() {
        ServerStartGamePacketHook::unhook();
    }
}