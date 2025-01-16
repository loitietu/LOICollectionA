#include <vector>
#include <cstdint>
#include <cstdlib>
#include <functional>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/RandomUtils.h>

#include <mc/world/actor/Mob.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/actor/ActorDamageSource.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/scores/Objective.h>
#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/PlayerScoreboardId.h>
#include <mc/world/scores/IdentityDefinition.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>

#include <mc/world/level/Level.h>
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/LevelSettings.h>

#include <mc/deps/core/utility/BinaryStream.h>

#include <mc/network/ConnectionRequest.h>
#include <mc/network/LoopbackPacketSender.h>
#include <mc/network/packet/StartGamePacket.h>
#include <mc/network/packet/LoginPacket.h>
#include <mc/network/packet/TextPacket.h>
#include <mc/network/packet/TextPacketType.h>
#include <mc/network/ServerNetworkHandler.h>
#include <mc/network/NetworkIdentifier.h>

#include <mc/certificates/Certificate.h>
#include <mc/certificates/ExtendedCertificate.h>

#include <mc/common/SubClientId.h>
#include <mc/common/ActorUniqueID.h>

#include <mc/server/ServerPlayer.h>

#include "include/HookPlugin.h"

int64_t mFakeSeed = 0;
std::vector<std::string> mInterceptedTextPacketTargets;
std::vector<std::string> mInterceptedGetNameTagTargets;
std::vector<std::function<void(NetworkIdentifier*, std::string, std::string)>> mLoginPacketSendEventCallbacks;
std::vector<std::function<void(Player*, int, std::string, ScoreChangedType)>> mPlayerScoreChangedEventCallbacks;
std::vector<std::function<bool(Mob*, Actor*, float)>> mPlayerHurtEventCallbacks;

LL_TYPE_INSTANCE_HOOK(
    ServerStartGamePacketHook,
    HookPriority::Normal,
    StartGamePacket,
    &StartGamePacket::$write,
    void,
    BinaryStream& stream
) {
    if (mFakeSeed) this->mSettings->setRandomSeed(LevelSeed64(mFakeSeed));
    return origin(stream);
};

#define InterceptPacketHookMacro(NAME, TYPE, SYMBOL, LIMIT, VAL, ...)                                           \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, void,  __VA_ARGS__) {                       \
        if (packet.getId() == MinecraftPacketIds::Text) {                                                       \
            TextPacket mTextPacket = static_cast<TextPacket const&>(packet);                                    \
            if (mTextPacket.mType == TextPacketType::Translate && mTextPacket.params.size() <= 1) {             \
                if (mTextPacket.mMessage.find(LIMIT) == std::string::npos)                                      \
                    return origin VAL;                                                                          \
                Player* player = ll::service::getLevel()->getPlayer(mTextPacket.params.at(0));                  \
                if (player == nullptr) return origin VAL;                                                       \
                if (std::find(mInterceptedTextPacketTargets.begin(), mInterceptedTextPacketTargets.end(),       \
                    player->getUuid().asString()) != mInterceptedTextPacketTargets.end())                       \
                    return;                                                                                     \
            }                                                                                                   \
        }                                                                                                       \
        return origin VAL;                                                                                      \
    };                                                                                                          \

InterceptPacketHookMacro(InterceptPacketHook1, LoopbackPacketSender, 
    &LoopbackPacketSender::$sendBroadcast,
    "multiplayer.player.joined", (identifier, subId, packet),
    NetworkIdentifier const& identifier, SubClientId subId, Packet const& packet
)

InterceptPacketHookMacro(InterceptPacketHook2, LoopbackPacketSender,
    &LoopbackPacketSender::$sendBroadcast,
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
    LoginPacketSendHook,
    HookPriority::Normal,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const& identifier,
    LoginPacket const& packet
) {
    origin(identifier, packet);
    std::string mIpAndPort = identifier.getIPAndPort();
    std::string mObjectUuid = ExtendedCertificate::getIdentity(*packet.mConnectionRequest->getCertificate()).asString();
    for (auto& callback : mLoginPacketSendEventCallbacks)
        callback(&const_cast<NetworkIdentifier&>(identifier), mObjectUuid, mIpAndPort);
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
                (action == PlayerScoreSetFunction::Add ? ScoreChangedType::add : 
                    (action == PlayerScoreSetFunction::Subtract ? ScoreChangedType::reduce : ScoreChangedType::set)
                )
            );
        }
    }
    return origin(success, id, objective, scoreValue, action);
};

#define PlayerHurtHookMacro(NAME, TYPE, SYMBOL, RET, RETV, VAL, ...)                                                                                    \
    LL_TYPE_INSTANCE_HOOK(NAME, HookPriority::Normal, TYPE, SYMBOL, RET, __VA_ARGS__) {                                                                 \
        if (this->isPlayer()) {                                                                                                                         \
            if (!source.isEntitySource())                                                                                                               \
                return origin VAL;                                                                                                                      \
            Actor* damgeSource = nullptr;                                                                                                               \
            source.isChildEntitySource() ? damgeSource = ll::service::getLevel()->fetchEntity(source.getEntityUniqueID(), false)                        \
                : damgeSource = ll::service::getLevel()->fetchEntity(source.getDamagingEntityUniqueID(), false);                                        \
            if (damgeSource == nullptr || !damgeSource->isPlayer() || damgeSource->getOrCreateUniqueID().rawID == this->getOrCreateUniqueID().rawID)    \
                return origin VAL;                                                                                                                      \
            for (auto& callback : mPlayerHurtEventCallbacks)                                                                                            \
                if (callback(this, damgeSource, damage)) return RETV;                                                                                   \
        }                                                                                                                                               \
        return origin VAL;                                                                                                                              \
    };                                                                                                                                                  \

PlayerHurtHookMacro(PlayerHurtHook1, Mob,
    &Mob::getDamageAfterResistanceEffect,
    float, 0, (source, damage),
    ActorDamageSource const& source,
    float damage
)
PlayerHurtHookMacro(PlayerHurtHook2, Mob,
    &Mob::$_hurt, bool, false,
    (source, damage, knock, ignite),
    ActorDamageSource const& source,
    float damage, bool knock, bool ignite
)
PlayerHurtHookMacro(PlayerHurtHook3, Mob,
    &Mob::$hurtEffects, void, void(),
    (source, damage, knock, ignite),
    ActorDamageSource const& source,
    float damage, bool knock, bool ignite
)

LL_TYPE_INSTANCE_HOOK(
    PlayerAddItemHook,
    HookPriority::Highest,
    Player,
    &Player::$add,
    bool,
    ItemStack& item
) {
    bool result = origin(item);
    if (!result)
        this->drop(item, false);
    return result;
}

namespace LOICollection::HookPlugin {
    namespace Event {
        void onLoginPacketSendEvent(const std::function<void(NetworkIdentifier*, std::string, std::string)>& callback) {
            mLoginPacketSendEventCallbacks.push_back(callback);
        }

        void onPlayerScoreChangedEvent(const std::function<void(Player*, int, std::string, ScoreChangedType)>& callback) {
            mPlayerScoreChangedEventCallbacks.push_back(callback);
        }

        void onPlayerHurtEvent(const std::function<bool(Mob*, Actor*, float)>& callback) {
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
        int64 result = std::strtoll(ptr, &endpt, 10);
        ptr == endpt ? mFakeSeed = ll::random_utils::rand<int64_t>() : mFakeSeed = result;
    }

    void registery() {
        ServerStartGamePacketHook::hook();
        InterceptPacketHook1::hook();
        InterceptPacketHook2::hook();
        InterceptGetNameTagHook::hook();
        LoginPacketSendHook::hook();
        PlayerScoreChangedHook::hook();
        PlayerHurtHook1::hook();
        PlayerHurtHook2::hook();
        PlayerHurtHook3::hook();
        PlayerAddItemHook::hook();
    }

    void unregistery() {
        ServerStartGamePacketHook::unhook();
        InterceptPacketHook1::unhook();
        InterceptPacketHook2::unhook();
        InterceptGetNameTagHook::unhook();
        LoginPacketSendHook::unhook();
        PlayerScoreChangedHook::unhook();
        PlayerHurtHook1::unhook();
        PlayerHurtHook2::unhook();
        PlayerHurtHook3::unhook();
        PlayerAddItemHook::unhook();
    }
}
