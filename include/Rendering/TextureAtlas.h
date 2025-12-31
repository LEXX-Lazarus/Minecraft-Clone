#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <glad/glad.h>
#include <string>
#include <vector>
#include <map>
#include "Block.h"

struct TextureRect {
    float uMin, vMin, uMax, vMax;
};

class TextureAtlas {
public:
    TextureAtlas(int cellWidth = 64, int cellHeight = 48);
    ~TextureAtlas();

    bool buildAtlas(const std::string& directoryPath);
    TextureRect getFaceUVs(BlockType type, int faceIndex) const;
    void bind() const;

private:
    unsigned int textureID;
    int atlasWidth;
    int atlasHeight;
    int cellWidth;
    int cellHeight;

    std::map<BlockType, int> blockRowMap;

    BlockType stringToBlockType(const std::string& name);
};

#endif