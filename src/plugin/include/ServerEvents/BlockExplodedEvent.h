#pragma once

#include <ll/api/event/Event.h>

#include "base/Macro.h"

class Actor;
class Block;
class BlockPos;
class Dimension;

namespace LOICollection::ServerEvents {
    class BlockExplodedEvent final : public ll::event::Event {
    protected:
        const BlockPos& mPosition;
        const Block& mBlock;

        Dimension& mDimension;
        Actor* mSource;
    
    public:
        constexpr explicit BlockExplodedEvent(
            const BlockPos& position,
            const Block& block,
            Dimension& dimension,
            Actor* source
        ) : mPosition(position), mBlock(block), mDimension(dimension), mSource(source) {}

        LOICOLLECTION_A_NDAPI const BlockPos& getPosition() const;
        LOICOLLECTION_A_NDAPI const Block& getBlock() const;
        
        LOICOLLECTION_A_NDAPI Dimension& getDimension() const;
        LOICOLLECTION_A_NDAPI Actor* getSource() const;
    };
}