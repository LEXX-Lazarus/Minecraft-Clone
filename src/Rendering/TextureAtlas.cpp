#include "Rendering/TextureAtlas.h"
#include <glad/glad.h>
#include "stb_image.h"
#include <iostream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TextureAtlas::TextureAtlas(int cw, int ch)
    : textureID(0), atlasWidth(0), atlasHeight(0), cellWidth(cw), cellHeight(ch) {
}

TextureAtlas::~TextureAtlas() {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

BlockType TextureAtlas::stringToBlockType(const std::string& name) {
    // STONE (1, 0)
    if (name.find("StoneBlock") != std::string::npos) return Blocks::STONE;

    // DIRT (2, 0)
    if (name.find("DirtBlock") != std::string::npos) return Blocks::DIRT;

    // GRASS (3, 0)
    if (name.find("GrassBlock") != std::string::npos) return Blocks::GRASS;

    // SAND (4, 0)
    if (name.find("SandBlock") != std::string::npos) return Blocks::SAND;

    // WOOD (5, 0)
    if (name.find("OakLog") != std::string::npos) return Blocks::OAK_LOG;

    // LIGHT (6, x)
    if (name.find("BlockOfWhiteLight") != std::string::npos) return Blocks::BLOCK_OF_WHITE_LIGHT;
    if (name.find("BlockOfRedLight") != std::string::npos) return Blocks::BLOCK_OF_RED_LIGHT;
    if (name.find("BlockOfGreenLight") != std::string::npos) return Blocks::BLOCK_OF_GREEN_LIGHT;
    if (name.find("BlockOfBlueLight") != std::string::npos) return Blocks::BLOCK_OF_BLUE_LIGHT;

    return Blocks::AIR;
}

bool TextureAtlas::buildAtlas(const std::string& directoryPath) {
    std::vector<std::string> textureFiles;

    for (const auto& entry : fs::directory_iterator(directoryPath)) {
        if (entry.path().extension() == ".png") {
            std::string filename = entry.path().filename().string();
            BlockType type = stringToBlockType(filename);

            if (type.category != BlockCategory::AIR) {
                textureFiles.push_back(entry.path().string());
            }
            else {
                std::cout << "Skipping unknown: " << filename << std::endl;
            }
        }
    }

    if (textureFiles.empty()) {
        std::cerr << "No recognized blocks found!" << std::endl;
        return false;
    }

    // Sort by (category, variant) - groups blocks by row, then by column
    std::sort(textureFiles.begin(), textureFiles.end(),
        [this](const std::string& a, const std::string& b) {
            std::string nameA = fs::path(a).filename().string();
            std::string nameB = fs::path(b).filename().string();
            BlockType typeA = stringToBlockType(nameA);
            BlockType typeB = stringToBlockType(nameB);
            return typeA < typeB;
        });

    std::cout << "\n=== TEXTURE ATLAS (2D Block System) ===" << std::endl;
    std::cout << "Format: Atlas Row -> BlockType(Category, Variant)" << std::endl;
    std::cout << "===========================================" << std::endl;
    for (size_t i = 0; i < textureFiles.size(); ++i) {
        std::string filename = fs::path(textureFiles[i]).filename().string();
        BlockType type = stringToBlockType(filename);

        const char* catName = "UNKNOWN";
        if (type.category == BlockCategory::STONE) catName = "STONE";
        else if (type.category == BlockCategory::DIRT) catName = "DIRT";
        else if (type.category == BlockCategory::GRASS) catName = "GRASS";
        else if (type.category == BlockCategory::SAND) catName = "SAND";
        else if (type.category == BlockCategory::LOG) catName = "LOG";
        else if (type.category == BlockCategory::LIGHT) catName = "LIGHT";

        std::cout << "  Atlas Row " << i << " = " << filename
            << " -> BlockType(" << static_cast<int>(type.category)
            << "," << static_cast<int>(type.variant) << ") ["
            << catName << ":" << static_cast<int>(type.variant)
            << "] ID=" << type.toID() << std::endl;
    }
    std::cout << "===========================================" << std::endl;
    std::cout << "\nNOW BUILDING BLOCKTYPEMAP..." << std::endl;

    atlasWidth = cellWidth;
    atlasHeight = cellHeight * static_cast<int>(textureFiles.size());

    std::vector<unsigned char> atlasData(atlasWidth * atlasHeight * 4, 0);

    // IMPORTANT FIX:
    // Pack atlas rows from BOTTOM to TOP so row 0 is at the bottom (OpenGL-friendly)
    for (size_t i = 0; i < textureFiles.size(); ++i) {
        int w, h, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(textureFiles[i].c_str(), &w, &h, &channels, 4);

        if (data) {
            int flippedRow = static_cast<int>(textureFiles.size() - 1 - i);
            int rowOffset = flippedRow * cellHeight;

            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int atlasIdx = ((rowOffset + y) * atlasWidth + x) * 4;
                    int localIdx = (y * w + x) * 4;

                    for (int c = 0; c < 4; ++c) {
                        atlasData[atlasIdx + c] = data[localIdx + c];
                    }
                }
            }

            std::string filename = fs::path(textureFiles[i]).filename().string();
            BlockType type = stringToBlockType(filename);
            blockRowMap[type] = static_cast<int>(i);

            std::cout << "  blockRowMap[BlockType(" << static_cast<int>(type.category)
                << "," << static_cast<int>(type.variant)
                << ")] = Atlas Row " << i << std::endl;

            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load " << textureFiles[i] << std::endl;
        }
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, atlasData.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    std::cout << "Atlas built: " << atlasWidth << "x" << atlasHeight
        << " (" << textureFiles.size() << " blocks)" << std::endl;

    return true;
}


TextureRect TextureAtlas::getFaceUVs(BlockType type, int faceIndex) const {
    auto it = blockRowMap.find(type);
    if (it == blockRowMap.end()) {
        std::cout << "ERROR: BlockType(" << static_cast<int>(type.category)
            << "," << static_cast<int>(type.variant)
            << ") NOT FOUND in blockRowMap!" << std::endl;
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    int row = it->second;

    static int debugCount = 0;
    if (debugCount < 10) {  // Only print first 10 lookups
        std::cout << "getFaceUVs: BlockType(" << static_cast<int>(type.category)
            << "," << static_cast<int>(type.variant)
            << ") face=" << faceIndex << " -> Atlas Row " << row << std::endl;
        debugCount++;
    }

    int localX = 0, localY = 0;

    // Texture layout: [Top] / [L][F][R][B] / [Bottom]
    // Each face is 16x16 within 64x48 texture

    if (faceIndex == 0) { // Top
        localX = 16;
        localY = 0;
    }
    else if (faceIndex == 1) { // Bottom
        localX = 16;
        localY = 32;
    }
    else { // Sides - ALL use FRONT face (middle texture at x=16)
        localX = 16;  // FIXED: was (faceIndex-2)*16 which gave 0,16,32,48
        localY = 16;
    }

    int pixelY = (row * cellHeight) + localY;

    float uMin = static_cast<float>(localX) / static_cast<float>(atlasWidth);
    float vMin = 1.0f - (static_cast<float>(pixelY + 16) / static_cast<float>(atlasHeight));
    float uMax = uMin + (16.0f / static_cast<float>(atlasWidth));
    float vMax = vMin + (16.0f / static_cast<float>(atlasHeight));

    if (debugCount < 10) {
        std::cout << "  -> UV coords: (" << uMin << "," << vMin << ") to (" << uMax << "," << vMax << ")" << std::endl;
        std::cout << "  -> pixelY=" << pixelY << ", atlasHeight=" << atlasHeight << std::endl;
    }

    return { uMin, vMin, uMax, vMax };
}

void TextureAtlas::bind() const {
    glBindTexture(GL_TEXTURE_2D, textureID);
}