#pragma once

#include <ll/api/event/Cancellable.h>
#include <ll/api/event/player/PlayerEvent.h>

#include "LOICollectionA/base/Macro.h"

class Player;
class BlockPos;

namespace LOICollection::ServerEvents {
    class PlayerOpenContainerEvent final : public ll::event::Cancellable<ll::event::PlayerEvent> {
    protected:
        BlockPos& mPosition;

        int mDimensionId;
    
    public:
        constexpr explicit PlayerOpenContainerEvent(
            Player& player,
            BlockPos& position,
            int dimensionId
        ) : Cancellable(player), mPosition(position), mDimensionId(dimensionId) {}

        LOICOLLECTION_A_NDAPI BlockPos& getPosition() const;

        LOICOLLECTION_A_NDAPI int getDimensionId() const;
    };
}