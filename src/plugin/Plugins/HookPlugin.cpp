#include <vector>
#include <cstdint>
#include <functional>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/ServerScoreboard.h>
#include <mc/deps/core/utility/BinaryStream.h>
#include <mc/network/packet/StartGamePacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>

#include "Utils/toolUtils.h"

#include "Include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::function<bool(void*, std::string)>> mTextPacketSendEventCallbacks;
std::vector<std::function<void(void*, int, std::string)>> mPlayerScoreChangedEventCallbacks;

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

LL_TYPE_INSTANCE_HOOK(
    PlayerScoreChangedHook,
    HookPriority::Normal,
    ServerScoreboard,
    "?onScoreChanged@ServerScoreboard@@UEAAXAEBUScoreboardId@@AEBVObjective@@@Z",
    void,
    ScoreboardId const& id,
    Objective const& objective
) {
    if (id.getIdentityDef().isPlayerType()) {
        Player* player = nullptr;
        for (auto& pls : toolUtils::getAllPlayers()) {
            if (ll::service::getLevel()->getScoreboard().getScoreboardId(*pls).mRawId == id.mRawId) {
                player = pls;
                break;
            }
        }
        for (auto& callback : mPlayerScoreChangedEventCallbacks) {
            callback(
                player,
                objective.getPlayerScore(id).mScore,
                objective.getName()
            );
        }
    }
    return origin(id, objective);
};

namespace HookPlugin {
    namespace Event {
        void onTextPacketSendEvent(const std::function<bool(void*, const std::string&)>& callback) {
            mTextPacketSendEventCallbacks.push_back(callback);
        }

        void onPlayerScoreChangedEvent(const std::function<void(void*, int, std::string)>& callback) {
            mPlayerScoreChangedEventCallbacks.push_back(callback);
        }
    }

    void setFakeSeed(int64_t fakeSeed) {
        mFakeSeed = fakeSeed;
    }

    void registery() {
        FakeSeedHook::hook();
        TextPacketSeedHook::hook();
        PlayerScoreChangedHook::hook();
    }
}