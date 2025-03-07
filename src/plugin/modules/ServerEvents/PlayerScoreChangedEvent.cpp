#include <map>
#include <cmath>
#include <memory>
#include <string>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/scores/ScoreInfo.h>
#include <mc/world/scores/Objective.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/ServerScoreboard.h>
#include <mc/world/scores/IdentityDefinition.h>
#include <mc/world/scores/PlayerScoreboardId.h>

#include <mc/server/ServerPlayer.h>

#include <mc/network/NetworkIdentifier.h>
#include <mc/network/ConnectionRequest.h>
#include <mc/network/ServerNetworkHandler.h>

#include <mc/legacy/ActorUniqueID.h>

#include "include/ServerEvents/PlayerScoreChangedEvent.h"

std::map<int64, std::map<std::string, int>> mScores;

namespace LOICollection::ServerEvents {
    const Objective& PlayerScoreChangedEvent::getObjective() const {
        return this->mObjective;
    }

    ScoreChangedType PlayerScoreChangedEvent::getScoreChangedType() const {
        return this->mScoreChangedType;
    }

    int PlayerScoreChangedEvent::getScore() const {
        return this->mScore;
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerScoreInitializeHook,
        HookPriority::Lowest,
        ServerNetworkHandler,
        &ServerNetworkHandler::sendLoginMessageLocal,
        void,
        NetworkIdentifier const& source,
        ConnectionRequest const& connectionRequest,
        ServerPlayer& player
    ) {
        int64 mActorUniqueId = player.getOrCreateUniqueID().rawID;

        optional_ref<Level> level = ll::service::getLevel();
        ScoreboardId identity = level->getScoreboard().getScoreboardId(player);
        for (ScoreInfo& info : level->getScoreboard().getIdScores(identity))
            mScores[mActorUniqueId][info.mObjective->mName] = info.mValue;

        origin(source, connectionRequest, player);
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerScoreChangedHook,
        HookPriority::Normal,
        ServerScoreboard,
        &ServerScoreboard::$onScoreChanged,
        void,
        ScoreboardId const& id,
        Objective const& obj
    ) {
        if (id.mIdentityDef->mIdentityType != IdentityDefinition::Type::Player)
            return origin(id, obj);
        int64 mActorUniqueId = id.mIdentityDef->mPlayerId->mActorUniqueId;

        int mScoreValue = obj.getPlayerScore(id).mValue;
        if (!mScores.contains(mActorUniqueId))
            mScores[mActorUniqueId][obj.mName] = mScoreValue;

        int mOriginScoreValue = mScores[mActorUniqueId][obj.mName];
        int mChangedScoreValue = std::abs(mOriginScoreValue - mScoreValue);
        Player* player = ll::service::getLevel()->getPlayer(ActorUniqueID(mActorUniqueId));
        ScoreChangedType type = mScoreValue > mOriginScoreValue ? ScoreChangedType::add : ScoreChangedType::reduce;

        PlayerScoreChangedEvent event(*player, obj, type, mChangedScoreValue);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return;

        mScores[mActorUniqueId][obj.mName] = mScoreValue;
        origin(id, obj);
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class PlayerScoreChangedEventEmitter : public ll::event::Emitter<emitterFactory, PlayerScoreChangedEvent> {
        ll::memory::HookRegistrar<PlayerScoreInitializeHook> hook1;
        ll::memory::HookRegistrar<PlayerScoreChangedHook> hook2;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<PlayerScoreChangedEventEmitter>();
    }
}