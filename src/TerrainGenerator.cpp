#include "TerrainGenerator.h"
#include <iostream>

void TerrainGenerator::generateFlatTerrain(Chunk& chunk) {
    // Simple test generation: 1 layer grass, 10 layers dirt
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            // Top layer: grass at y=10
            chunk.setBlock(x, 10, z, BlockType::GRASS);

            // Dirt layers from y=9 down to y=0
            for (int y = 9; y >= 0; y--) {
                chunk.setBlock(x, y, z, BlockType::DIRT);
            }
        }
    }

    std::cout << "Flat terrain generated for chunk" << std::endl;
}