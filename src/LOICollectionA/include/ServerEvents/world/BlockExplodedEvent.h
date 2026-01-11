#pragma once

#include <ll/api/event/Event.h>

#include "LOICollectionA/base/Macro.h"

class Actor;
class Block;
class BlockPos;
class Dimension;

namespace LOICollection::ServerEvents {
    class BlockExplodedEvent final : public ll::event::Event {
    protected:
        const BlockPos& mPosition;
        const Block& mBlock;

        int mDimensionId;

        Actor* mSource;
    
    public:
        constexpr explicit BlockExplodedEvent(
            const BlockPos& position,
            const Block& block,
            int dimension,
            Actor* source
        ) : mPosition(position), mBlock(block), mDimensionId(dimension), mSource(source) {}

        LOICOLLECTION_A_NDAPI const BlockPos& getPosition() const;
        LOICOLLECTION_A_NDAPI const Block& getBlock() const;
        
        LOICOLLECTION_A_NDAPI int getDimensionId() const;
        
        LOICOLLECTION_A_NDAPI Actor* getSource() const;
    };
}