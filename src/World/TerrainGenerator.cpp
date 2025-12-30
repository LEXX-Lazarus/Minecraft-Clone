#include "World/TerrainGenerator.h"
#include "World/Chunk.h"
#include "Block.h"

void TerrainGenerator::generateTerrain(Chunk& chunk) {
    // Calculate this chunk's Y range in world coordinates
    int chunkWorldYMin = chunk.chunkY * CHUNK_SIZE_Y;  // No Chunk:: prefix!

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                int worldY = chunkWorldYMin + y;

                // Flat terrain layers:
                if (worldY <= 255) {
                    // Stone layer: Y 0-255
                    chunk.setBlock(x, y, z, BlockType::STONE);
                }
                else if (worldY >= 256 && worldY <= 266) {
                    // Dirt layer: Y 256-266 (11 blocks)
                    chunk.setBlock(x, y, z, BlockType::DIRT);
                }
                else if (worldY == 267) {
                    // Grass surface: Y 267
                    chunk.setBlock(x, y, z, BlockType::GRASS);
                }
                else {
                    // Air above: Y 268+
                    chunk.setBlock(x, y, z, BlockType::AIR);
                }
            }
        }
    }
}