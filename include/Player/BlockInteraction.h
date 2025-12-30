#ifndef BLOCK_INTERACTION_H
#define BLOCK_INTERACTION_H

#include "Player/Camera.h"
#include "World/ChunkManager.h"
#include "Block.h"

class BlockInteraction {
public:
    BlockInteraction();
    ~BlockInteraction() = default;

    // Break the block the player is looking at
    bool breakBlock(Camera& camera, ChunkManager* chunkManager);

    // Place a block on the face the player is looking at
    bool placeBlock(Camera& camera, ChunkManager* chunkManager, BlockType blockType);

    // Get info about the block being targeted (for UI/debugging)
    bool getTargetedBlock(Camera& camera, ChunkManager* chunkManager,
        int& outBlockX, int& outBlockY, int& outBlockZ,
        int& outFaceX, int& outFaceY, int& outFaceZ);

private:
    const float maxReach = 5.0f;  // Maximum reach distance
    const float rayStep = 0.05f;   // Raycast step size

    // Raycast to find targeted block
    bool raycastBlock(Camera& camera, ChunkManager* chunkManager,
        int& hitBlockX, int& hitBlockY, int& hitBlockZ,
        int& faceX, int& faceY, int& faceZ);
};

#endif