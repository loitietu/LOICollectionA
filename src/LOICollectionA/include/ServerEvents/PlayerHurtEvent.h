#pragma once

#include <ll/api/event/Cancellable.h>
#include <ll/api/event/player/PlayerEvent.h>

#include "LOICollectionA/base/Macro.h"

class Actor;
class Player;

namespace LOICollection::ServerEvents {
    class PlayerHurtEvent final : public ll::event::Cancellable<ll::event::PlayerEvent> {
    protected:
        Actor& mSource;
        
        int mDamage;
        bool mKnock;
        bool mIgnite;
    
    public:
        constexpr explicit PlayerHurtEvent(
            Player& player,
            Actor& source,
            int damage = 0,
            bool knock = false,
            bool ignite = false
        ) : Cancellable(player), mSource(source), mDamage(damage), mKnock(knock), mIgnite(ignite) {}

        LOICOLLECTION_A_NDAPI Actor& getSource() const;
        LOICOLLECTION_A_NDAPI int getDamage() const;
        LOICOLLECTION_A_NDAPI bool isKnock() const;
        LOICOLLECTION_A_NDAPI bool isIgnite() const;
    };
}