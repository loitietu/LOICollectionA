#pragma once

#include <ll/api/event/Cancellable.h>
#include <ll/api/event/player/PlayerEvent.h>

#include "base/Macro.h"

class Player;
class Objective;

namespace LOICollection::ServerEvents {
    enum class ScoreChangedType {
        add,
        reduce,
        set
    };

    class PlayerScoreChangedEvent final : public ll::event::Cancellable<ll::event::PlayerEvent> {
    protected:
        const Objective& mObjective;
        ScoreChangedType mScoreChangedType;
        int mScore;
    
    public:
        constexpr explicit PlayerScoreChangedEvent(
            Player& player,
            const Objective& objective,
            ScoreChangedType scoreChangedType,
            int score = 0
        ) : Cancellable(player), mObjective(objective), mScoreChangedType(scoreChangedType), mScore(score) {}

        LOICOLLECTION_A_NDAPI const Objective& getObjective() const;
        LOICOLLECTION_A_NDAPI ScoreChangedType getScoreChangedType() const;
        LOICOLLECTION_A_NDAPI int getScore() const;
    };
}