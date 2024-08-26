#include <cstdint>
#include <ll/api/memory/Hook.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/deps/core/utility/BinaryStream.h>
#include <mc/network/packet/StartGamePacket.h>

#include "../Include/plugin/HookPlugin.h"

namespace HookPlugin {
    int64_t mFakeSeed = 0;

    LL_TYPE_INSTANCE_HOOK(
        FakeSeedHook,
        HookPriority::Normal,
        StartGamePacket,
        "?write@StartGamePacket@@UEBAXAEAVBinaryStream@@@Z",
        void,
        class BinaryStream& stream
    ) {
        this->mSettings.setRandomSeed(LevelSeed64(mFakeSeed));
        return origin(stream);
    };

    void registery() {
        FakeSeedHook::hook();
    }

    void setFakeSeed(int64_t fakeSeed) {
        mFakeSeed = fakeSeed;
    }
}