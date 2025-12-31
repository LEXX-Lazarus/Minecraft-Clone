#include "World/TerrainGenerator.h"
#include "World/Chunk.h"
#include "Block.h"

void TerrainGenerator::generateTerrain(Chunk& chunk) {
    int chunkWorldYMin = chunk.chunkY * CHUNK_SIZE_Y;

    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int y = 0; y < CHUNK_SIZE_Y; y++) {
                int worldY = chunkWorldYMin + y;

                if (worldY <= 255) {
                    chunk.setBlock(x, y, z, Blocks::STONE);
                }
                else if (worldY >= 256 && worldY <= 266) {
                    chunk.setBlock(x, y, z, Blocks::DIRT);
                }
                else if (worldY == 267) {
                    chunk.setBlock(x, y, z, Blocks::GRASS);
                }
                else {
                    chunk.setBlock(x, y, z, Blocks::AIR);
                }
            }
        }
    }
}