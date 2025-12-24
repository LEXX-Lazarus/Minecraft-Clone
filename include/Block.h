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

    Block() : type(BlockType::AIR) {}
    Block(BlockType t) : type(t) {}

    bool isAir() const {
        return type == BlockType::AIR;
    }
};

#endif