#include <vector>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>

#include <mc/world/ActorUniqueID.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/player/Player.h>
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
#include <mc/entity/utilities/ActorType.h>
#include <mc/certificates/Certificate.h>
#include <mc/certificates/ExtendedCertificate.h>
#include <mc/server/LoopbackPacketSender.h>
#include <mc/enums/MinecraftPacketIds.h>
#include <mc/enums/TextPacketType.h>
#include <mc/enums/SubClientId.h>

#include "Utils/toolUtils.h"

#include "Include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::string> mInterceptedTextPacketTargets;
std::vector<std::function<bool(void*, std::string)>> mTextPacketSendEventCallbacks;
std::vector<std::function<void(void*, std::string, std::string)>> mLoginPacketSendEventCallbacks;
std::vector<std::function<void(void*, int, std::string)>> mPlayerScoreChangedEventCallbacks;
std::vector<std::function<bool(void*, void*, float)>> mPlayerHurtEventCallbacks;

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
    InterceptPacketHook,
    HookPriority::Normal,
    LoopbackPacketSender,
    "?sendBroadcast@LoopbackPacketSender@@UEAAXAEBVNetworkIdentifier@@W4SubClientId@@AEBVPacket@@@Z",
    void,
    NetworkIdentifier const& identifier,
    SubClientId subId,
    Packet const& packet
) {
    if (packet.getId() == MinecraftPacketIds::Text) {
        auto mTextPacket = static_cast<TextPacket const&>(packet);
        if (mTextPacket.mType == TextPacketType::Translate && mTextPacket.mParams.size() <= 2) {
            std::string mUuid = toolUtils::getPlayerFromName(mTextPacket.mParams.at(0))->getUuid().asString();
            if (mInterceptedTextPacketTargets.size() >= 10000) {
                mInterceptedTextPacketTargets.clear();
                for (auto& player : toolUtils::getAllPlayers())
                    mInterceptedTextPacketTargets.push_back(player->getUuid().asString());
            }
            for (auto& target : mInterceptedTextPacketTargets)
                if (mUuid == target) return;
        }
    }
    return origin(identifier, subId, packet);
};

LL_TYPE_INSTANCE_HOOK(
    TextPacketSendHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    "?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVTextPacket@@@Z",
    void,
    NetworkIdentifier& identifier,
    TextPacket& packet
) {
    for (auto& callback : mTextPacketSendEventCallbacks)
        if (callback(toolUtils::getPlayerFromName(packet.mAuthor), packet.mMessage)) return;
    return origin(identifier, packet);
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
    for (auto& callback : mLoginPacketSendEventCallbacks)
        callback(&identifier, uuid, ipAndPort);
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
        for (auto& callback : mPlayerScoreChangedEventCallbacks) {
            callback(
                ll::service::getLevel()->getPlayer(ActorUniqueID(id.getIdentityDef().getPlayerId().mActorUniqueId)),
                objective.getPlayerScore(id).mScore,
                objective.getName()
            );
        }
    }
    return origin(id, objective);
};

LL_TYPE_INSTANCE_HOOK(
    PlayerHurtHook1,
    HookPriority::Normal,
    Mob,
    &Mob::getDamageAfterResistanceEffect,
    float,
    ActorDamageSource const& source,
    float damage
) {
    if (!this->isType(ActorType::Player))
        return origin(source, damage);
    if (source.getCause() == ActorDamageCause::Magic || source.getCause() == ActorDamageCause::Wither) {
        Actor* damgeSource = nullptr;
        if (source.isEntitySource()) {
            if (source.isChildEntitySource())
                damgeSource = ll::service::getLevel()->fetchEntity(source.getEntityUniqueID());
            else damgeSource = ll::service::getLevel()->fetchEntity(source.getDamagingEntityUniqueID());
        }
        if (damgeSource == nullptr || !damgeSource->isType(ActorType::Player))
            return origin(source, damage);
        if (damgeSource->getOrCreateUniqueID().id == this->getOrCreateUniqueID().id)
            return origin(source, damage);
        for (auto& callback : mPlayerHurtEventCallbacks) {
            if (callback(this, damgeSource, damage))
                return 0.0f;
        }
    }
    return origin(source, damage);
};

LL_TYPE_INSTANCE_HOOK(
    PlayerHurtHook2,
    HookPriority::Normal,
    Actor,
    &Actor::hurt,
    bool,
    ActorDamageSource const& source,
    float damage,
    bool knock,
    bool ignite
) {
    if (this->isType(ActorType::Player)) {
        Actor* damgeSource = nullptr;
        if (source.isEntitySource()) {
            if (source.isChildEntitySource())
                damgeSource = ll::service::getLevel()->fetchEntity(source.getEntityUniqueID());
            else damgeSource = ll::service::getLevel()->fetchEntity(source.getDamagingEntityUniqueID());
        }
        if (damgeSource == nullptr || !damgeSource->isType(ActorType::Player))
            return origin(source, damage, knock, ignite);
        if (damgeSource->getOrCreateUniqueID().id == this->getOrCreateUniqueID().id)
            return origin(source, damage, knock, ignite);
        for (auto& callback : mPlayerHurtEventCallbacks) {
            if (callback(this, damgeSource, damage))
                return false;
        }
    }
    return origin(source, damage, knock, ignite);
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

        void onPlayerHurtEvent(const std::function<bool(void*, void*, float)>& callback) {
            mPlayerHurtEventCallbacks.push_back(callback);
        }
    }

    void interceptTextPacket(const std::string& uuid) {
        mInterceptedTextPacketTargets.push_back(uuid);
    }

    void setFakeSeed(const std::string& fakeSeed) {
        srand((unsigned int) time(nullptr));
        try {
            if (fakeSeed.empty() || fakeSeed == "random") {
                mFakeSeed = static_cast<int64_t>(rand());
                return;
            }
            mFakeSeed = std::stoll(fakeSeed);
        } catch (...) {}
    }

    void registery() {
        FakeSeedHook::hook();
        InterceptPacketHook::hook();
        TextPacketSendHook::hook();
        LoginPacketSendHook::hook();
        PlayerScoreChangedHook::hook();
        PlayerHurtHook1::hook();
        PlayerHurtHook2::hook();
    }

    void unregistery() {
        FakeSeedHook::unhook();
        InterceptPacketHook::unhook();
        TextPacketSendHook::unhook();
        LoginPacketSendHook::unhook();
        PlayerScoreChangedHook::unhook();
        PlayerHurtHook1::unhook();
        PlayerHurtHook2::unhook();
    }
}