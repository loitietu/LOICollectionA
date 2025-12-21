#pragma once

#include <ll/api/event/Cancellable.h>
#include <ll/api/event/player/PlayerEvent.h>

#include "LOICollectionA/base/Macro.h"

class Actor;
class Player;

namespace LOICollection::ServerEvents {
    enum class PlayerHurtReason {
        Hurt,
        Effect,
        Projectile
    };

    class PlayerHurtEvent final : public ll::event::Cancellable<ll::event::PlayerEvent> {
    protected:
        Actor& mSource;
        
        int mDamage;
        bool mKnock;
        bool mIgnite;

        PlayerHurtReason mReason;
    
    public:
        constexpr explicit PlayerHurtEvent(
            Player& player,
            Actor& source,
            int damage = 0,
            bool knock = false,
            bool ignite = false,
            PlayerHurtReason reason = PlayerHurtReason::Hurt
        ) : Cancellable(player), mSource(source), mDamage(damage), mKnock(knock), mIgnite(ignite), mReason(reason) {}

        LOICOLLECTION_A_NDAPI Actor& getSource() const;
        LOICOLLECTION_A_NDAPI int getDamage() const;
        LOICOLLECTION_A_NDAPI bool isKnock() const;
        LOICOLLECTION_A_NDAPI bool isIgnite() const;
        LOICOLLECTION_A_NDAPI PlayerHurtReason getReason() const;
    };
}