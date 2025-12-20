#ifndef BLOCK_REGISTRY_H
#define BLOCK_REGISTRY_H

#include <glad/glad.h>
#include <map>
#include "Block.h"
#include "Texture.h"

class BlockRegistry {
public:
    static BlockRegistry& getInstance() {
        static BlockRegistry instance;
        return instance;
    }

    void loadTextures();
    void bindTexture(BlockType type);

private:
    BlockRegistry() {}
    std::map<BlockType, Texture*> textures;
};

#endif