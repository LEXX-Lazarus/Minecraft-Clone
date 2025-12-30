#include "World/WorldSave.h"
#include <filesystem>
#include <iostream>

WorldSave::WorldSave(const std::string& worldName)
    : worldName(worldName), isDirty(false), lastSaveTime(std::chrono::steady_clock::now()) {
    loadFromDisk();
}

WorldSave::~WorldSave() {
    flush();
}

std::string WorldSave::getSaveFilePath() {
    std::filesystem::create_directories("SavedData/" + worldName);
    return "SavedData/" + worldName + "/world_blocks.dat";
}

long long WorldSave::makeBlockKey(int x, int y, int z) {
    long long key = (((long long)x & 0x1FFFFF) << 33) |
        (((long long)y & 0xFFF) << 21) |
        ((long long)z & 0x1FFFFF);
    return key;
}

void WorldSave::saveBlockChange(int x, int y, int z, BlockType type) {
    std::lock_guard<std::mutex> lock(saveMutex);
    long long key = makeBlockKey(x, y, z);
    modifiedBlocks[key] = type;
    isDirty = true;  // Mark as having unsaved changes
}

bool WorldSave::getBlockChange(int x, int y, int z, BlockType& outType) {
    std::lock_guard<std::mutex> lock(saveMutex);
    long long key = makeBlockKey(x, y, z);
    auto it = modifiedBlocks.find(key);
    if (it != modifiedBlocks.end()) {
        outType = it->second;
        return true;
    }
    return false;
}

bool WorldSave::hasBlockChange(int x, int y, int z) {
    std::lock_guard<std::mutex> lock(saveMutex);
    long long key = makeBlockKey(x, y, z);
    return modifiedBlocks.find(key) != modifiedBlocks.end();
}

void WorldSave::loadChunkModifications(int chunkX, int chunkZ, std::vector<ModifiedBlock>& modifications) {
    std::lock_guard<std::mutex> lock(saveMutex);

    int minX = chunkX * 16;
    int maxX = minX + 16;
    int minZ = chunkZ * 16;
    int maxZ = minZ + 16;

    for (auto& [key, type] : modifiedBlocks) {
        int x = (int)((key >> 33) & 0x1FFFFF);
        int y = (int)((key >> 21) & 0xFFF);
        int z = (int)(key & 0x1FFFFF);

        if (x & 0x100000) x |= 0xFFE00000;
        if (z & 0x100000) z |= 0xFFE00000;

        if (x >= minX && x < maxX && z >= minZ && z < maxZ) {
            modifications.push_back({ x, y, z, type });
        }
    }
}

void WorldSave::loadFromDisk() {
    std::string filepath = getSaveFilePath();
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "No save file found at: " << filepath << std::endl;
        std::cout << "Starting fresh world!" << std::endl;
        return;
    }

    int count;
    file.read((char*)&count, sizeof(int));

    for (int i = 0; i < count; i++) {
        int x, y, z;
        BlockType type;
        file.read((char*)&x, sizeof(int));
        file.read((char*)&y, sizeof(int));
        file.read((char*)&z, sizeof(int));
        file.read((char*)&type, sizeof(BlockType));

        long long key = makeBlockKey(x, y, z);
        modifiedBlocks[key] = type;
    }

    file.close();
    std::cout << "Loaded " << count << " block modifications from: " << filepath << std::endl;
}

void WorldSave::saveToDisk() {
    if (!isDirty) return;  // Don't save if nothing changed

    std::string filepath = getSaveFilePath();
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to save world to: " << filepath << std::endl;
        return;
    }

    int count = modifiedBlocks.size();
    file.write((char*)&count, sizeof(int));

    for (auto& [key, type] : modifiedBlocks) {
        int x = (int)((key >> 33) & 0x1FFFFF);
        int y = (int)((key >> 21) & 0xFFF);
        int z = (int)(key & 0x1FFFFF);

        if (x & 0x100000) x |= 0xFFE00000;
        if (z & 0x100000) z |= 0xFFE00000;

        file.write((char*)&x, sizeof(int));
        file.write((char*)&y, sizeof(int));
        file.write((char*)&z, sizeof(int));
        file.write((char*)&type, sizeof(BlockType));
    }

    file.close();
    isDirty = false;
    std::cout << "Saved " << count << " block modifications to: " << filepath << std::endl;
}

void WorldSave::autoSaveCheck() {
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - lastSaveTime).count();

    if (elapsed >= autoSaveInterval && isDirty) {
        std::cout << "Auto-saving world..." << std::endl;
        saveToDisk();
        lastSaveTime = now;
    }
}

void WorldSave::flush() {
    saveToDisk();
}