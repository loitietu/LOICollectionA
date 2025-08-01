#pragma once

#include <optional>

class Block;
class BlockPos;
class BlockActor;
class CompoundTag;

namespace BlockUtils {
    std::optional<Block*> getBlock(const BlockPos& pos, int dimension);

    std::optional<BlockActor*> getBlockEntity(const BlockPos& pos, int dimension);

    void setBlock(const BlockPos& pos, int dimension, const CompoundTag& nbt);
    void setBlockEntity(const BlockPos& pos, int dimension, const CompoundTag& nbt);
}