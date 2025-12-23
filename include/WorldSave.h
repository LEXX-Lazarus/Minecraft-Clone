#ifndef WORLD_SAVE_H
#define WORLD_SAVE_H

#include "Block.h"
#include <string>
#include <unordered_map>
#include <fstream>
#include <mutex>
#include <chrono>

struct ModifiedBlock {
    int x, y, z;
    BlockType type;
};

class WorldSave {
public:
    WorldSave(const std::string& worldName);
    ~WorldSave();

    void saveBlockChange(int x, int y, int z, BlockType type);
    bool getBlockChange(int x, int y, int z, BlockType& outType);
    bool hasBlockChange(int x, int y, int z);
    void loadChunkModifications(int chunkX, int chunkZ, std::vector<ModifiedBlock>& modifications);

    void flush();
    void autoSaveCheck();  // Call this every frame to auto-save periodically

private:
    std::string worldName;
    std::string getSaveFilePath();

    std::unordered_map<long long, BlockType> modifiedBlocks;
    std::mutex saveMutex;

    bool isDirty;  // Track if we have unsaved changes
    std::chrono::steady_clock::time_point lastSaveTime;
    const float autoSaveInterval = 30.0f;  // Auto-save every 30 seconds

    long long makeBlockKey(int x, int y, int z);
    void loadFromDisk();
    void saveToDisk();
};

#endif