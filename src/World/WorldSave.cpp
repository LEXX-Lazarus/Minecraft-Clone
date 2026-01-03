#include "World/WorldSave.h"
#include <filesystem>
#include <iostream>

WorldSave::WorldSave(const std::string& worldName)
    : worldName(worldName), isDirty(false), lastSaveTime(std::chrono::steady_clock::now()) {
    // Reserve space for typical world modifications
    modifiedBlocks.reserve(10000);
    loadFromDisk();
}

WorldSave::~WorldSave() {
    flush();
}

std::string WorldSave::getSaveFilePath() {
    std::filesystem::create_directories("SavedData/" + worldName);
    return "SavedData/" + worldName + "/world_blocks.dat";
}

// Inline this hot function - called frequently during chunk loading
inline long long WorldSave::makeBlockKey(int x, int y, int z) {
    return (((long long)x & 0x1FFFFF) << 33) |
        (((long long)y & 0xFFF) << 21) |
        ((long long)z & 0x1FFFFF);
}

void WorldSave::saveBlockChange(int x, int y, int z, BlockType type) {
    std::lock_guard<std::mutex> lock(saveMutex);
    long long key = makeBlockKey(x, y, z);
    modifiedBlocks[key] = type;
    isDirty = true;
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

    // Pre-calculate bounds once
    const int minX = chunkX * 16;
    const int maxX = minX + 16;
    const int minZ = chunkZ * 16;
    const int maxZ = minZ + 16;

    // Reserve space to avoid reallocations (typical chunk has 10-100 modifications)
    modifications.reserve(100);

    // Use const reference to avoid copies
    for (const auto& [key, type] : modifiedBlocks) {
        // Extract coordinates using bit operations
        int x = (int)((key >> 33) & 0x1FFFFF);
        int z = (int)(key & 0x1FFFFF);

        // Sign extend for negative coordinates
        if (x & 0x100000) x |= 0xFFE00000;
        if (z & 0x100000) z |= 0xFFE00000;

        // Quick rejection test - check X and Z first (most likely to fail)
        if (x < minX || x >= maxX || z < minZ || z >= maxZ) {
            continue;
        }

        // Only extract Y if X and Z are in range (saves work)
        int y = (int)((key >> 21) & 0xFFF);

        modifications.push_back({ x, y, z, type });
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

    // Reserve space for all blocks at once to avoid rehashing
    modifiedBlocks.reserve(count);

    // Pre-allocate buffer for batch reading (read multiple blocks at once)
    struct BlockData {
        int x, y, z;
        BlockType type;
    };

    std::vector<BlockData> buffer;
    const int bufferSize = 1024; // Read 1024 blocks at a time
    buffer.resize(bufferSize);

    int remaining = count;
    while (remaining > 0) {
        int toRead = std::min(remaining, bufferSize);
        file.read((char*)buffer.data(), toRead * sizeof(BlockData));

        for (int i = 0; i < toRead; i++) {
            long long key = makeBlockKey(buffer[i].x, buffer[i].y, buffer[i].z);
            modifiedBlocks[key] = buffer[i].type;
        }

        remaining -= toRead;
    }

    file.close();
    std::cout << "Loaded " << count << " block modifications from: " << filepath << std::endl;
}

void WorldSave::saveToDisk() {
    if (!isDirty) return;

    std::string filepath = getSaveFilePath();
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to save world to: " << filepath << std::endl;
        return;
    }

    int count = modifiedBlocks.size();
    file.write((char*)&count, sizeof(int));

    // Pre-allocate buffer for batch writing (write multiple blocks at once)
    struct BlockData {
        int x, y, z;
        BlockType type;
    };

    std::vector<BlockData> buffer;
    const int bufferSize = 1024; // Write 1024 blocks at a time
    buffer.reserve(bufferSize);

    for (const auto& [key, type] : modifiedBlocks) {
        int x = (int)((key >> 33) & 0x1FFFFF);
        int y = (int)((key >> 21) & 0xFFF);
        int z = (int)(key & 0x1FFFFF);

        // Sign extend for negative coordinates
        if (x & 0x100000) x |= 0xFFE00000;
        if (z & 0x100000) z |= 0xFFE00000;

        buffer.push_back({ x, y, z, type });

        // Write buffer when full
        if (buffer.size() >= bufferSize) {
            file.write((char*)buffer.data(), buffer.size() * sizeof(BlockData));
            buffer.clear();
        }
    }

    // Write remaining blocks
    if (!buffer.empty()) {
        file.write((char*)buffer.data(), buffer.size() * sizeof(BlockData));
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