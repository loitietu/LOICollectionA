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
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>
#include <mc/certificates/Certificate.h>
#include <mc/certificates/ExtendedCertificate.h>

#include "Utils/toolUtils.h"

#include "Include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::function<bool(void*, std::string)>> mTextPacketSendEventCallbacks;
std::vector<std::function<void(void*, std::string, std::string)>> mLoginPacketSendEventCallbacks;
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
    TextPacketSendHook,
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

LL_TYPE_INSTANCE_HOOK(
    LoginPacketSendHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVLoginPacket@@@Z",
    void,
    NetworkIdentifier& identifier,
    LoginPacket& packet
) {
    origin(identifier, packet);
    auto certificate = packet.mConnectionRequest->getCertificate();
    auto uuid = ExtendedCertificate::getIdentity(*certificate).asString();
    auto ipAndPort = identifier.getIPAndPort();
    for (auto& callback : mLoginPacketSendEventCallbacks) {
        callback(&identifier, uuid, ipAndPort);
    }
};

namespace HookPlugin {
    namespace Event {
        void onTextPacketSendEvent(const std::function<bool(void*, const std::string&)>& callback) {
            mTextPacketSendEventCallbacks.push_back(callback);
        }

        void onLoginPacketSendEvent(const std::function<void(void*, std::string, std::string)>& callback) {
            mLoginPacketSendEventCallbacks.push_back(callback);
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
        TextPacketSendHook::hook();
        PlayerScoreChangedHook::hook();
        LoginPacketSendHook::hook();
    }
}