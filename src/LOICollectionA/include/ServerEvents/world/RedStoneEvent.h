#pragma once

#include <ll/api/event/Event.h>

#include "LOICollectionA/base/Macro.h"

class BlockPos;
class BlockSource;

namespace LOICollection::ServerEvents {
    class RedStoneEvent final : public ll::event::Event {
    protected:
        const BlockPos& mPosition;
        const BlockSource& mSource;

        int mDimensionId;
        int mStrength;

        bool mIsFirstTime;
    
    public:
        constexpr explicit RedStoneEvent(
            const BlockPos& position,
            const BlockSource& source,
            int dimensionId,
            int strength,
            bool isFirstTime
        ) : mPosition(position), mSource(source), mDimensionId(dimensionId), mStrength(strength), mIsFirstTime(isFirstTime) {}

        LOICOLLECTION_A_NDAPI const BlockPos& getPosition() const;
        LOICOLLECTION_A_NDAPI const BlockSource& getSource() const;

        LOICOLLECTION_A_NDAPI int getDimensionId() const;
        LOICOLLECTION_A_NDAPI int getStrength() const;

        LOICOLLECTION_A_NDAPI bool isFirstTime() const;
    };
}