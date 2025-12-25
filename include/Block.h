#ifndef BLOCK_H
#define BLOCK_H

enum class BlockType {
    AIR = 0,
    GRASS = 1,
    DIRT = 2,
    STONE = 3,
    SAND = 4
};

struct Block {
    BlockType type;
    unsigned char skyLight;  // 0-15 sky light level

    Block() : type(BlockType::AIR), skyLight(0) {}
    Block(BlockType t) : type(t), skyLight(0) {}

    bool isAir() const {
        return type == BlockType::AIR;
    }

    bool isTransparent() const {
        return type == BlockType::AIR;
    }
};

#endif