#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>
#include <functional>

// Primary block categories (ROWS)
enum class BlockCategory : uint16_t {
    AIR = 0,
    STONE = 1,
    DIRT = 2,
    GRASS = 3,
    SAND = 4,
    LOG = 5,
    LEAVES = 6,
    LIGHT = 7,
};

// Full block type (ROW, COLUMN)
struct BlockType {
    BlockCategory category;  // ROW
    uint8_t variant;         // COLUMN

    BlockType() : category(BlockCategory::AIR), variant(0) {}
    BlockType(BlockCategory cat, uint8_t var = 0) : category(cat), variant(var) {}

    bool operator==(const BlockType& other) const {
        return category == other.category && variant == other.variant;
    }

    bool operator!=(const BlockType& other) const {
        return !(*this == other);
    }

    bool operator<(const BlockType& other) const {
        if (category != other.category) return category < other.category;
        return variant < other.variant;
    }

    uint32_t toID() const {
        return (static_cast<uint32_t>(category) << 8) | variant;
    }

    static BlockType fromID(uint32_t id) {
        return BlockType(static_cast<BlockCategory>(id >> 8), id & 0xFF);
    }
};

// Hash specialization - MUST come after BlockType definition
namespace std {
    template<>
    struct hash<BlockType> {
        size_t operator()(const BlockType& b) const {
            return std::hash<uint32_t>{}(b.toID());
        }
    };
}

namespace Blocks {
    const BlockType AIR = BlockType(BlockCategory::AIR, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType STONE = BlockType(BlockCategory::STONE, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType DIRT = BlockType(BlockCategory::DIRT, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType GRASS = BlockType(BlockCategory::GRASS, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType SAND = BlockType(BlockCategory::SAND, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType OAK_LOG = BlockType(BlockCategory::LOG, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType OAK_LEAVES = BlockType(BlockCategory::LEAVES, 0);
    ///////////////////////////////////////////////////////////////////////////
    const BlockType BLOCK_OF_WHITE_LIGHT = BlockType(BlockCategory::LIGHT, 0);
    const BlockType BLOCK_OF_RED_LIGHT = BlockType(BlockCategory::LIGHT, 1);
    const BlockType BLOCK_OF_GREEN_LIGHT = BlockType(BlockCategory::LIGHT, 2);
    const BlockType BLOCK_OF_BLUE_LIGHT = BlockType(BlockCategory::LIGHT, 3);
    ///////////////////////////////////////////////////////////////////////////
}

struct Block {
    BlockType type;

    Block() : type(Blocks::AIR) {}
    Block(BlockType t) : type(t) {}

    bool isAir() const {
        return type.category == BlockCategory::AIR;
    }
};

#endif