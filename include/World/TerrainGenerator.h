#ifndef TERRAINGENERATOR_H
#define TERRAINGENERATOR_H

class Chunk;  // Forward declaration

class TerrainGenerator {
public:
    static void generateTerrain(Chunk& chunk);  // CHANGED from generateFlatTerrain
};

#endif