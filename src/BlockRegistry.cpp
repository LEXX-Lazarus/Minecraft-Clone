#include "BlockRegistry.h"
#include <iostream>

void BlockRegistry::loadTextures() {
    textures[BlockType::GRASS] = new Texture("assets/textures/blocks/GrassBlock.png");
    textures[BlockType::DIRT] = new Texture("assets/textures/blocks/DirtBlock.png");
    textures[BlockType::STONE] = new Texture("assets/textures/blocks/StoneBlock.png");
    textures[BlockType::STONE] = new Texture("assets/textures/blocks/SandBlock.png");

    std::cout << "Block textures loaded" << std::endl;
}

void BlockRegistry::bindTexture(BlockType type) {
    if (textures.find(type) != textures.end()) {
        textures[type]->bind();
    }
}