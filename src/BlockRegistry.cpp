#include "BlockRegistry.h"
#include <iostream>

void BlockRegistry::loadTextures() {
    textures[BlockType::GRASS] = new Texture("assets/textures/GrassBlock.png");
    textures[BlockType::DIRT] = new Texture("assets/textures/DirtBlock.png");
    textures[BlockType::STONE] = new Texture("assets/textures/StoneBlock.png");

    std::cout << "Block textures loaded" << std::endl;
}

void BlockRegistry::bindTexture(BlockType type) {
    if (textures.find(type) != textures.end()) {
        textures[type]->bind();
    }
}