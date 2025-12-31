#include "Player/BlockInteraction.h"
#include <cmath>
#include <iostream>

BlockInteraction::BlockInteraction() {
}

bool BlockInteraction::raycastBlock(Camera& camera, ChunkManager* chunkManager,
    int& hitBlockX, int& hitBlockY, int& hitBlockZ,
    int& faceX, int& faceY, int& faceZ) {
    if (!chunkManager) return false;

    float rayX = camera.x + 0.0f;
    float rayY = camera.y + 0.5f;
    float rayZ = camera.z + 0.0f;

    float dirX = camera.frontX;
    float dirY = camera.frontY;
    float dirZ = camera.frontZ;

    int lastBlockX = INT_MAX;
    int lastBlockY = INT_MAX;
    int lastBlockZ = INT_MAX;

    for (float distance = 0.0f; distance < maxReach; distance += rayStep) {
        rayX += dirX * rayStep;
        rayY += dirY * rayStep;
        rayZ += dirZ * rayStep;

        int blockX = static_cast<int>(std::round(rayX));
        int blockY = static_cast<int>(std::floor(rayY));
        int blockZ = static_cast<int>(std::round(-rayZ));

        Block* block = chunkManager->getBlockAt(blockX, blockY, blockZ);
        if (block && !block->isAir()) {
            hitBlockX = blockX;
            hitBlockY = blockY;
            hitBlockZ = blockZ;

            if (lastBlockX != INT_MAX) {
                faceX = lastBlockX - blockX;
                faceY = lastBlockY - blockY;
                faceZ = lastBlockZ - blockZ;
            }
            else {
                float dx = camera.x - blockX;
                float dy = camera.y - blockY;
                float dz = camera.z + blockZ;

                float absDx = std::abs(dx);
                float absDy = std::abs(dy);
                float absDz = std::abs(dz);

                faceX = faceY = faceZ = 0;
                if (absDx > absDy && absDx > absDz) {
                    faceX = (dx > 0) ? 1 : -1;
                }
                else if (absDy > absDz) {
                    faceY = (dy > 0) ? 1 : -1;
                }
                else {
                    faceZ = (dz > 0) ? -1 : 1;
                }
            }

            delete block;
            return true;
        }
        if (block) delete block;

        lastBlockX = blockX;
        lastBlockY = blockY;
        lastBlockZ = blockZ;
    }

    return false;
}

bool BlockInteraction::breakBlock(Camera& camera, ChunkManager* chunkManager) {
    if (!chunkManager) return false;

    int hitBlockX, hitBlockY, hitBlockZ;
    int faceX, faceY, faceZ;

    if (raycastBlock(camera, chunkManager, hitBlockX, hitBlockY, hitBlockZ, faceX, faceY, faceZ)) {
        std::cout << "Breaking block at (" << hitBlockX << ", " << hitBlockY << ", " << hitBlockZ << ")" << std::endl;

        // Set block to air
        if (chunkManager->setBlockAt(hitBlockX, hitBlockY, hitBlockZ, Blocks::AIR)) {
            chunkManager->rebuildChunkMeshAt(hitBlockX, hitBlockY, hitBlockZ);
            std::cout << "Block broken!" << std::endl;
            return true;
        }
    }

    return false;
}

bool BlockInteraction::placeBlock(Camera& camera, ChunkManager* chunkManager, BlockType blockType) {
    if (!chunkManager) return false;

    int hitBlockX, hitBlockY, hitBlockZ;
    int faceX, faceY, faceZ;

    if (raycastBlock(camera, chunkManager, hitBlockX, hitBlockY, hitBlockZ, faceX, faceY, faceZ)) {
        int placeX = hitBlockX + faceX;
        int placeY = hitBlockY + faceY;
        int placeZ = hitBlockZ + faceZ;

        float playerX = camera.x;
        float playerFeetY = camera.y - 1.0f;
        float playerZ = -camera.z;

        int playerFeetBlockY = static_cast<int>(std::floor(playerFeetY));
        int playerHeadBlockY = playerFeetBlockY + 1;

        int playerBlockX = static_cast<int>(std::round(playerX));
        int playerBlockZ = static_cast<int>(std::round(playerZ));

        if (placeX == playerBlockX && placeZ == playerBlockZ) {
            if (placeY == playerHeadBlockY || placeY == playerFeetBlockY) {
                std::cout << "Cannot place block - player collision!" << std::endl;
                return false;
            }
        }

        std::cout << "Placing block at (" << placeX << ", " << placeY << ", " << placeZ << ")" << std::endl;

        if (chunkManager->setBlockAt(placeX, placeY, placeZ, blockType)) {
            chunkManager->rebuildChunkMeshAt(placeX, placeY, placeZ);
            std::cout << "Block placed!" << std::endl;
            return true;
        }
    }

    return false;
}

bool BlockInteraction::getTargetedBlock(Camera& camera, ChunkManager* chunkManager,
    int& outBlockX, int& outBlockY, int& outBlockZ,
    int& outFaceX, int& outFaceY, int& outFaceZ) {
    return raycastBlock(camera, chunkManager, outBlockX, outBlockY, outBlockZ,
        outFaceX, outFaceY, outFaceZ);
}