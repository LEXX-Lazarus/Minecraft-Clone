#include "Rendering/TextureAtlas.h"
#include <glad/glad.h>
#include "stb_image.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstring>

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
    // Quick early return for common cases based on first character
    if (name.empty()) return Blocks::AIR;

    // Use switch for faster branching on first char
    switch (name[0]) {
    case 'S':
        if (name.find("StoneBlock") != std::string::npos) return Blocks::STONE;
        if (name.find("SandBlock") != std::string::npos) return Blocks::SAND;
        break;
    case 'D':
        if (name.find("DirtBlock") != std::string::npos) return Blocks::DIRT;
        break;
    case 'G':
        if (name.find("GrassBlock") != std::string::npos) return Blocks::GRASS;
        break;
    case 'O':
        if (name.find("OakLog") != std::string::npos) return Blocks::OAK_LOG;
        if (name.find("OakLeaves") != std::string::npos) return Blocks::OAK_LEAVES;
        break;
    case 'B':
        if (name.find("BlockOfWhiteLight") != std::string::npos) return Blocks::BLOCK_OF_WHITE_LIGHT;
        if (name.find("BlockOfRedLight") != std::string::npos) return Blocks::BLOCK_OF_RED_LIGHT;
        if (name.find("BlockOfGreenLight") != std::string::npos) return Blocks::BLOCK_OF_GREEN_LIGHT;
        if (name.find("BlockOfBlueLight") != std::string::npos) return Blocks::BLOCK_OF_BLUE_LIGHT;
        break;
    }

    return Blocks::AIR;
}

// Optimized: Use memcpy for full rows instead of pixel-by-pixel
void extractFace(const unsigned char* src, unsigned char* dst, int srcWidth, int faceX, int faceY) {
    const int bytesPerRow = 16 * 4; // 16 pixels * 4 channels
    for (int y = 0; y < 16; y++) {
        const unsigned char* srcRow = &src[((faceY + y) * srcWidth + faceX) * 4];
        unsigned char* dstRow = &dst[y * bytesPerRow];
        std::memcpy(dstRow, srcRow, bytesPerRow);
    }
}

// Optimized: Reduce divisions using bit shifts where possible
void downsample(const unsigned char* src, unsigned char* dst, int srcSize) {
    int dstSize = srcSize >> 1; // srcSize / 2
    for (int y = 0; y < dstSize; y++) {
        for (int x = 0; x < dstSize; x++) {
            int r = 0, g = 0, b = 0, a = 0;

            // Average 2x2 block
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int srcIdx = ((y * 2 + dy) * srcSize + (x * 2 + dx)) * 4;
                    r += src[srcIdx + 0];
                    g += src[srcIdx + 1];
                    b += src[srcIdx + 2];
                    a += src[srcIdx + 3];
                }
            }

            int dstIdx = (y * dstSize + x) * 4;
            dst[dstIdx + 0] = r >> 2; // Divide by 4 using bit shift
            dst[dstIdx + 1] = g >> 2;
            dst[dstIdx + 2] = b >> 2;
            dst[dstIdx + 3] = a >> 2;
        }
    }
}

// Optimized: Use memcpy for full rows
void writeFace(unsigned char* atlas, const unsigned char* face, int atlasWidth, int faceSize, int faceX, int faceY) {
    const int bytesPerRow = faceSize * 4;
    for (int y = 0; y < faceSize; y++) {
        unsigned char* atlasRow = &atlas[((faceY + y) * atlasWidth + faceX) * 4];
        const unsigned char* faceRow = &face[y * bytesPerRow];
        std::memcpy(atlasRow, faceRow, bytesPerRow);
    }
}

bool TextureAtlas::buildAtlas(const std::string& directoryPath) {
    std::vector<std::string> textureFiles;
    textureFiles.reserve(16); // Pre-allocate reasonable size

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
    std::cout << "Building mipmaps for each 16x16 face..." << std::endl;

    atlasWidth = cellWidth;
    atlasHeight = cellHeight * static_cast<int>(textureFiles.size());

    // Create mipmap levels (5 levels: 64x432, 32x216, 16x108, 8x54, 4x27)
    std::vector<std::vector<unsigned char>> mipLevels(5);

    // Pre-allocate all mipmap levels at once to avoid reallocations
    mipLevels[0].resize(atlasWidth * atlasHeight * 4, 0);
    for (int level = 1; level < 5; level++) {
        int mipWidth = atlasWidth >> level;
        int mipHeight = atlasHeight >> level;
        mipLevels[level].resize(mipWidth * mipHeight * 4, 0);
    }

    // Pre-allocate face buffers outside loop for reuse
    std::vector<unsigned char> face16(16 * 16 * 4);
    std::vector<unsigned char> face8(8 * 8 * 4);
    std::vector<unsigned char> face4(4 * 4 * 4);
    std::vector<unsigned char> face2(2 * 2 * 4);
    std::vector<unsigned char> face1(1 * 1 * 4);

    // Load textures and build level 0
    for (size_t i = 0; i < textureFiles.size(); ++i) {
        int w, h, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(textureFiles[i].c_str(), &w, &h, &channels, 4);

        if (data) {
            int flippedRow = static_cast<int>(textureFiles.size() - 1 - i);
            int rowOffset = flippedRow * cellHeight;

            // Copy full texture to level 0 using memcpy per row for better performance
            for (int y = 0; y < h; ++y) {
                int atlasIdx = ((rowOffset + y) * atlasWidth) * 4;
                int localIdx = (y * w) * 4;
                std::memcpy(&mipLevels[0][atlasIdx], &data[localIdx], w * 4);
            }

            // Now generate mipmaps for EACH 16x16 face independently
            // Faces: Top(16,0), Bottom(16,32), Sides(16,16)
            int facePositions[3][2] = { {16, 0}, {16, 32}, {16, 16} };

            for (int faceIdx = 0; faceIdx < 3; faceIdx++) {
                int baseFaceX = facePositions[faceIdx][0];
                int baseFaceY = facePositions[faceIdx][1] + rowOffset;

                // Extract 16x16 face (reusing pre-allocated buffers)
                extractFace(mipLevels[0].data(), face16.data(), atlasWidth, baseFaceX, baseFaceY);

                // Generate mip chain: 16->8->4->2->1
                downsample(face16.data(), face8.data(), 16);
                downsample(face8.data(), face4.data(), 8);
                downsample(face4.data(), face2.data(), 4);
                downsample(face2.data(), face1.data(), 2);

                // Write downsampled faces to corresponding mip levels
                writeFace(mipLevels[1].data(), face8.data(), atlasWidth >> 1, 8, baseFaceX >> 1, baseFaceY >> 1);
                writeFace(mipLevels[2].data(), face4.data(), atlasWidth >> 2, 4, baseFaceX >> 2, baseFaceY >> 2);
                writeFace(mipLevels[3].data(), face2.data(), atlasWidth >> 3, 2, baseFaceX >> 3, baseFaceY >> 3);
                writeFace(mipLevels[4].data(), face1.data(), atlasWidth >> 4, 1, baseFaceX >> 4, baseFaceY >> 4);
            }

            std::string filename = fs::path(textureFiles[i]).filename().string();
            BlockType type = stringToBlockType(filename);
            blockRowMap[type] = static_cast<int>(i);

            stbi_image_free(data);
        }
        else {
            std::cerr << "Failed to load " << textureFiles[i] << std::endl;
        }
    }

    // Upload all mip levels to GPU
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    for (int level = 0; level < 5; level++) {
        int mipWidth = atlasWidth >> level;
        int mipHeight = atlasHeight >> level;
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mipWidth, mipHeight, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, mipLevels[level].data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);

    // Use NEAREST filtering for both texture and mipmaps - no blending, no dark lines
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // LOD bias: NEGATIVE = delays mipmap usage (stays at level 0 longer)
    // -3.0 should keep level 0 until ~48 blocks (3 chunks) distance
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.5f);

    std::cout << "Atlas built: " << atlasWidth << "x" << atlasHeight
        << " (" << textureFiles.size() << " blocks) with 5 manual mip levels (LOD bias: -0.5)" << std::endl;

    return true;
}

TextureRect TextureAtlas::getFaceUVs(BlockType type, int faceIndex) const {
    auto it = blockRowMap.find(type);
    if (it == blockRowMap.end()) {
        return { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    int row = it->second;
    int localX, localY;

    // Texture layout: [Top] / [L][F][R][B] / [Bottom]
    if (faceIndex == 0) { // Top
        localX = 16;
        localY = 0;
    }
    else if (faceIndex == 1) { // Bottom
        localX = 16;
        localY = 32;
    }
    else { // Sides
        localX = 16;
        localY = 16;
    }

    int pixelY = (row * cellHeight) + localY;

    // Cache common divisors to reduce float divisions (called frequently during mesh building)
    const float invAtlasWidth = 1.0f / static_cast<float>(atlasWidth);
    const float invAtlasHeight = 1.0f / static_cast<float>(atlasHeight);

    // No padding - use exact UV coordinates for 16x16 face
    float uMin = static_cast<float>(localX) * invAtlasWidth;
    float vMin = 1.0f - static_cast<float>(pixelY + 16) * invAtlasHeight;
    float uMax = static_cast<float>(localX + 16) * invAtlasWidth;
    float vMax = 1.0f - static_cast<float>(pixelY) * invAtlasHeight;

    return { uMin, vMin, uMax, vMax };
}

void TextureAtlas::bind() const {
    glBindTexture(GL_TEXTURE_2D, textureID);
}