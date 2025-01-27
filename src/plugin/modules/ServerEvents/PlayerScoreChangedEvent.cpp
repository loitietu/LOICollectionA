#include <memory>

#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/Emitter.h>
#include <ll/api/event/EmitterBase.h>
#include <ll/api/event/EventBus.h>

#include <mc/world/level/Level.h>
#include <mc/world/actor/player/Player.h>

#include <mc/world/scores/Objective.h>
#include <mc/world/scores/Scoreboard.h>
#include <mc/world/scores/ScoreboardId.h>
#include <mc/world/scores/IdentityDefinition.h>
#include <mc/world/scores/PlayerScoreboardId.h>
#include <mc/world/scores/PlayerScoreSetFunction.h>

#include <mc/common/ActorUniqueID.h>

#include "include/ServerEvents/PlayerScoreChangedEvent.h"

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
        if (!id.mIdentityDef->isPlayerType())
            return origin(success, id, objective, scoreValue, action);

        Player* player = ll::service::getLevel()->getPlayer(ActorUniqueID(id.getIdentityDef().getPlayerId().mActorUniqueId));
        ScoreChangedType type = (action == PlayerScoreSetFunction::Add ? ScoreChangedType::add :
            (action == PlayerScoreSetFunction::Subtract ? ScoreChangedType::reduce : ScoreChangedType::set)
        );

        PlayerScoreChangedEvent event(*player, objective, type, scoreValue);
        ll::event::EventBus::getInstance().publish(event);
        if (event.isCancelled()) 
            return 0;
        return origin(success, id, objective, scoreValue, action);;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory();
    class PlayerScoreChangedEventEmitter : public ll::event::Emitter<emitterFactory, PlayerScoreChangedEvent> {
        ll::memory::HookRegistrar<PlayerScoreChangedHook> hook;
    };

    static std::unique_ptr<ll::event::EmitterBase> emitterFactory() {
        return std::make_unique<PlayerScoreChangedEventEmitter>();
    }
}