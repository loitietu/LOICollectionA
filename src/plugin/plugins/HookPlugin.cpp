#include <vector>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/ActorUniqueID.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/actor/player/PlayerScoreSetFunction.h>

#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/LevelSeed64.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include <mc/network/packet/StartGamePacket.h>
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>

#include <mc/certificates/Certificate.h>
#include <mc/certificates/ExtendedCertificate.h>

#include <mc/server/ServerPlayer.h>
#include <mc/server/LoopbackPacketSender.h>

#include <mc/enums/MinecraftPacketIds.h>
#include <mc/enums/TextPacketType.h>
#include <mc/enums/SubClientId.h>

#include "utils/McUtils.h"

#include "include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::string> mInterceptedTextPacketTargets;
std::vector<std::string> mInterceptedGetNameTagTargets;
std::vector<std::function<bool(void*, std::string)>> mTextPacketSendEventCallbacks;
std::vector<std::function<void(void*, std::string, std::string)>> mLoginPacketSendEventCallbacks;
std::vector<std::function<void(std::string)>> mPlayerDisconnectBeforeEventCallbacks;
std::vector<std::function<void(void*, int, std::string, int)>> mPlayerScoreChangedEventCallbacks;
std::vector<std::function<bool(void*, void*, float)>> mPlayerHurtEventCallbacks;

LL_TYPE_INSTANCE_HOOK(
    ServerStartGamePacketHook,
    HookPriority::Normal,
    StartGamePacket,
    "?write@StartGamePacket@@UEBAXAEAVBinaryStream@@@Z",
    void,
    BinaryStream& stream
) {
    if (!mFakeSeed) this->mSettings.setRandomSeed(LevelSeed64(mFakeSeed));
    return origin(stream);
};

#define InterceptPacketHookMacro(NAME, TYPE, SYMBOL, LIMIT, VAL, ...)                                           \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, void,  __VA_ARGS__) {                       \
        if (packet.getId() == MinecraftPacketIds::Text) {                                                       \
            auto mTextPacket = static_cast<TextPacket const&>(packet);                                          \
            if (mTextPacket.mType == TextPacketType::Translate && mTextPacket.mParams.size() <= 1) {            \
                if (mTextPacket.mMessage.find(LIMIT) == std::string::npos)                                      \
                    return origin VAL;                                                                          \
                Player* player = static_cast<Player*>(McUtils::getPlayerFromName(mTextPacket.mParams.at(0)));   \
                if (player == nullptr) return origin VAL;                                                       \
                if (std::find(mInterceptedTextPacketTargets.begin(), mInterceptedTextPacketTargets.end(),       \
                    player->getUuid().asString()) != mInterceptedTextPacketTargets.end())                       \
                    return;                                                                                     \
            }                                                                                                   \
        }                                                                                                       \
        return origin VAL;                                                                                      \
    };                                                                                                          \

InterceptPacketHookMacro(InterceptPacketHook1, LoopbackPacketSender, 
    "?sendBroadcast@LoopbackPacketSender@@UEAAXAEBVNetworkIdentifier@@W4SubClientId@@AEBVPacket@@@Z",
    "multiplayer.player.joined", (identifier, subId, packet),
    NetworkIdentifier const& identifier, SubClientId subId, Packet const& packet
)

InterceptPacketHookMacro(InterceptPacketHook2, LoopbackPacketSender,
    "?sendBroadcast@LoopbackPacketSender@@UEAAXAEBVPacket@@@Z",
    "multiplayer.player.left", (packet),
    Packet const& packet
)

LL_TYPE_INSTANCE_HOOK(
    InterceptGetNameTagHook,
    HookPriority::Normal,
    Actor,
    &Actor::getNameTag,
    std::string const&
) {
    Player* player = (Player*) this;
    if (this->isPlayer() && std::find(mInterceptedGetNameTagTargets.begin(), mInterceptedGetNameTagTargets.end(),
        player->getUuid().asString()) != mInterceptedGetNameTagTargets.end()) {
        static std::string mName;
        mName = player->getRealName();
        return mName;
    }
    return origin();
};

LL_TYPE_INSTANCE_HOOK(
    TextPacketSendHook,
    HookPriority::High,
    ServerNetworkHandler,
    &ServerNetworkHandler::handle,
    void,
    NetworkIdentifier const& identifier,
    TextPacket const& packet
) {
    Player* player = this->getServerPlayer(identifier, packet.mClientSubId);
    
    if (!player) return origin(identifier, packet);
    for (auto& callback : mTextPacketSendEventCallbacks)
        if (callback(player, packet.mMessage)) return;
    return origin(identifier, packet);
};

LL_TYPE_INSTANCE_HOOK(
    LoginPacketSendHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::handle,
    void,
    NetworkIdentifier const& identifier,
    LoginPacket const& packet
) {
    origin(identifier, packet);
    auto certificate = packet.mConnectionRequest->getCertificate();
    auto uuid = ExtendedCertificate::getIdentity(*certificate).asString();
    auto ipAndPort = identifier.getIPAndPort();    
    for (auto& callback : mLoginPacketSendEventCallbacks)
        callback(&const_cast<NetworkIdentifier&>(identifier), uuid, ipAndPort);
};

LL_TYPE_INSTANCE_HOOK(
    PlayerDisconnectBeforeHook,
    HookPriority::Normal,
    ServerPlayer,
    &ServerPlayer::disconnect,
    void
) {
    std::string mUuid = this->getUuid().asString();
    
    origin();
    for (auto& callback : mPlayerDisconnectBeforeEventCallbacks)
        callback(mUuid);
};

LL_TYPE_INSTANCE_HOOK(
    PlayerScoreChangedHook,
    HookPriority::Normal,
    Scoreboard,
    &Scoreboard::modifyPlayerScore,
    int,
    bool& success,
    ScoreboardId const& id,
    Objective& objective,
    int scoreValue,
    PlayerScoreSetFunction action
) {
    if (id.getIdentityDef().isPlayerType()) {
        for (auto& callback : mPlayerScoreChangedEventCallbacks) {
            callback(
                ll::service::getLevel()->getPlayer(ActorUniqueID(id.getIdentityDef().getPlayerId().mActorUniqueId)),
                scoreValue,
                objective.getName(),
                (action == PlayerScoreSetFunction::Add ? 0 : (action == PlayerScoreSetFunction::Subtract ? 1 : 2))
            );
        }
    }
    return origin(success, id, objective, scoreValue, action);
};

#define PlayerHurtHookMacro(NAME, TYPE, SYMBOL, RET, VAL, ...)                                                                                  \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, RET, __VA_ARGS__) {                                                         \
        if (this->isPlayer()) {                                                                                                                 \
            if (!source.isEntitySource())                                                                                                       \
                return origin VAL;                                                                                                              \
            Actor* damgeSource = nullptr;                                                                                                       \
            source.isChildEntitySource() ? damgeSource = ll::service::getLevel()->fetchEntity(source.getEntityUniqueID())                       \
                : damgeSource = ll::service::getLevel()->fetchEntity(source.getDamagingEntityUniqueID());                                       \
            if (damgeSource == nullptr || !damgeSource->isPlayer() || damgeSource->getOrCreateUniqueID().id == this->getOrCreateUniqueID().id)  \
                return origin VAL;                                                                                                              \
            for (auto& callback : mPlayerHurtEventCallbacks)                                                                                    \
                if (callback(this, damgeSource, damage)) return false;                                                                          \
        }                                                                                                                                       \
        return origin VAL;                                                                                                                      \
    };                                                                                                                                          \

PlayerHurtHookMacro(PlayerHurtHook1, Mob, &Mob::getDamageAfterResistanceEffect,
    float, (source, damage),
    ActorDamageSource const& source,
    float damage
)
PlayerHurtHookMacro(PlayerHurtHook2, Mob, "?_hurt@Mob@@MEAA_NAEBVActorDamageSource@@M_N1@Z",
    bool, (source, damage, knock, ignite),
    ActorDamageSource const& source,
    float damage, bool knock, bool ignite
)
PlayerHurtHookMacro(PlayerHurtHook3, Mob, "?hurtEffects@Mob@@UEAAXAEBVActorDamageSource@@M_N1@Z",
    bool, (source, damage, knock, ignite),
    ActorDamageSource const& source,
    float damage, bool knock, bool ignite
)

namespace LOICollection::HookPlugin {
    namespace Event {
        void onTextPacketSendEvent(const std::function<bool(void*, std::string)>& callback) {
            mTextPacketSendEventCallbacks.push_back(callback);
        }

        void onLoginPacketSendEvent(const std::function<void(void*, std::string, std::string)>& callback) {
            mLoginPacketSendEventCallbacks.push_back(callback);
        }

        void onPlayerDisconnectBeforeEvent(const std::function<void(std::string)>& callback) {
            mPlayerDisconnectBeforeEventCallbacks.push_back(callback);
        }

        void onPlayerScoreChangedEvent(const std::function<void(void*, int, std::string, int)>& callback) {
            mPlayerScoreChangedEventCallbacks.push_back(callback);
        }

        void onPlayerHurtEvent(const std::function<bool(void*, void*, float)>& callback) {
            mPlayerHurtEventCallbacks.push_back(callback);
        }
    }

    void interceptTextPacket(const std::string& uuid) {
        if (std::find(mInterceptedTextPacketTargets.begin(), mInterceptedTextPacketTargets.end(), uuid) != mInterceptedTextPacketTargets.end())
            return;
        mInterceptedTextPacketTargets.push_back(uuid);
    }

    void interceptGetNameTag(const std::string& uuid) {
        if (std::find(mInterceptedGetNameTagTargets.begin(), mInterceptedGetNameTagTargets.end(), uuid) != mInterceptedGetNameTagTargets.end())
            return;
        mInterceptedGetNameTagTargets.push_back(uuid);
    }

    void uninterceptTextPacket(const std::string& uuid) {
        auto it = std::find(mInterceptedTextPacketTargets.begin(), mInterceptedTextPacketTargets.end(), uuid);
        if (it != mInterceptedTextPacketTargets.end())
            mInterceptedTextPacketTargets.erase(it);
    }

    void uninterceptGetNameTag(const std::string& uuid) {
        auto it = std::find(mInterceptedGetNameTagTargets.begin(), mInterceptedGetNameTagTargets.end(), uuid);
        if (it!= mInterceptedGetNameTagTargets.end())
            mInterceptedGetNameTagTargets.erase(it);
    }

    void setFakeSeed(const std::string& str) {
        const char* ptr = str.data();
        char* endpt{};
        auto result = std::strtoll(ptr, &endpt, 10);
        ptr == endpt ? mFakeSeed = ll::random_utils::rand<int64_t>() : mFakeSeed = result;
    }

    void registery() {
        ServerStartGamePacketHook::hook();
        InterceptPacketHook1::hook();
        InterceptPacketHook2::hook();
        InterceptGetNameTagHook::hook();
        TextPacketSendHook::hook();
        LoginPacketSendHook::hook();
        PlayerDisconnectBeforeHook::hook();
        PlayerScoreChangedHook::hook();
        PlayerHurtHook1::hook();
        PlayerHurtHook2::hook();
        PlayerHurtHook3::hook();
    }

    void unregistery() {
        ServerStartGamePacketHook::unhook();
        InterceptPacketHook1::unhook();
        InterceptPacketHook2::unhook();
        InterceptGetNameTagHook::unhook();
        TextPacketSendHook::unhook();
        LoginPacketSendHook::unhook();
        PlayerDisconnectBeforeHook::unhook();
        PlayerScoreChangedHook::unhook();
        PlayerHurtHook1::unhook();
        PlayerHurtHook2::unhook();
        PlayerHurtHook3::unhook();
    }
}