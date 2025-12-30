#ifndef TERRAIN_GENERATOR_H
#define TERRAIN_GENERATOR_H

#include "Chunk.h"

class TerrainGenerator {
public:
    static void generateFlatTerrain(Chunk& chunk);
};

#endif