#include "BlockRegistry.h"
#include <iostream>

void BlockRegistry::loadTextures() {
    textures[BlockType::GRASS] = new Texture("assets/textures/blocks/GrassBlock.png");
    textures[BlockType::DIRT] = new Texture("assets/textures/blocks/DirtBlock.png");
    textures[BlockType::STONE] = new Texture("assets/textures/blocks/StoneBlock.png");
	textures[BlockType::SAND] = new Texture("assets/textures/blocks/SandBlock.png");
    textures[BlockType::BLOCKOFPUREWHITELIGHT] = new Texture("assets/textures/blocks/BlockOfPureWhiteLight.png");
    textures[BlockType::BLOCKOFPUREREDLIGHT] = new Texture("assets/textures/blocks/BlockOfPureRedLight.png");
    textures[BlockType::BLOCKOFPUREGREENLIGHT] = new Texture("assets/textures/blocks/BlockOfPureGreenLight.png");
    textures[BlockType::BLOCKOFPUREBLUELIGHT] = new Texture("assets/textures/blocks/BlockOfPureBlueLight.png");

    std::cout << "Block textures loaded" << std::endl;
}

void BlockRegistry::bindTexture(BlockType type) {
    if (textures.find(type) != textures.end()) {
        textures[type]->bind();
    }
}