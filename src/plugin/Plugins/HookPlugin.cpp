#include <vector>
#include <cstdint>
#include <functional>

#include <ll/api/memory/Hook.h>

#include <mc/world/level/LevelSeed64.h>
#include <mc/deps/core/utility/BinaryStream.h>
#include <mc/network/packet/StartGamePacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>

#include "../Utils/toolUtils.h"

#include "../Include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::function<bool(void*, std::string)>> mTextPacketSendEventCallbacks;

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

LL_TYPE_INSTANCE_HOOK(
    TextPacketSeedHook,
    HookPriority::Low,
    ServerNetworkHandler,
    "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVTextPacket@@@Z",
    void,
    NetworkIdentifier& identifier,
    TextPacket& packet
) {
    for (auto& callback : mTextPacketSendEventCallbacks) {
        if (callback(toolUtils::getPlayerFromName(packet.mAuthor), packet.mMessage)) {
            return;
        }
    }
    return origin(identifier, packet);
};

namespace HookPlugin {
    namespace Event {
        void onTextPacketSendEvent(const std::function<bool(void*, const std::string&)>& callback) {
            mTextPacketSendEventCallbacks.push_back(callback);
        }
    }

    void registery() {
        FakeSeedHook::hook();
        TextPacketSeedHook::hook();
    }

    void setFakeSeed(int64_t fakeSeed) {
        mFakeSeed = fakeSeed;
    }
}