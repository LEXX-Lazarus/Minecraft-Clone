#include "Rendering/TextureAtlas.h"
#include <glad/glad.h>
#include "stb_image.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

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
    if (name.find("BlockOfPureWhiteLight") != std::string::npos) return Blocks::BLOCK_OF_WHITE_LIGHT;
    if (name.find("BlockOfPureRedLight") != std::string::npos) return Blocks::BLOCK_OF_RED_LIGHT;
    if (name.find("BlockOfPureGreenLight") != std::string::npos) return Blocks::BLOCK_OF_GREEN_LIGHT;
    if (name.find("BlockOfPureBlueLight") != std::string::npos) return Blocks::BLOCK_OF_BLUE_LIGHT;

    return Blocks::AIR;
}

// Helper: Extract a 16x16 face from 64x48 texture
void extractFace(const unsigned char* src, unsigned char* dst, int srcWidth, int faceX, int faceY) {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            int srcIdx = ((faceY + y) * srcWidth + (faceX + x)) * 4;
            int dstIdx = (y * 16 + x) * 4;
            for (int c = 0; c < 4; c++) {
                dst[dstIdx + c] = src[srcIdx + c];
            }
        }
    }
}

// Helper: Downsample 16x16 -> 8x8 (simple box filter)
void downsample(const unsigned char* src, unsigned char* dst, int srcSize) {
    int dstSize = srcSize / 2;
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
            dst[dstIdx + 0] = r / 4;
            dst[dstIdx + 1] = g / 4;
            dst[dstIdx + 2] = b / 4;
            dst[dstIdx + 3] = a / 4;
        }
    }
}

// Helper: Write face back into atlas at specific location
void writeFace(unsigned char* atlas, const unsigned char* face, int atlasWidth, int faceSize, int faceX, int faceY) {
    for (int y = 0; y < faceSize; y++) {
        for (int x = 0; x < faceSize; x++) {
            int atlasIdx = ((faceY + y) * atlasWidth + (faceX + x)) * 4;
            int faceIdx = (y * faceSize + x) * 4;
            for (int c = 0; c < 4; c++) {
                atlas[atlasIdx + c] = face[faceIdx + c];
            }
        }
    }
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

    // Level 0 (full resolution)
    mipLevels[0].resize(atlasWidth * atlasHeight * 4, 0);

    // Lower mip levels (half size each time)
    for (int level = 1; level < 5; level++) {
        int mipWidth = atlasWidth >> level;
        int mipHeight = atlasHeight >> level;
        mipLevels[level].resize(mipWidth * mipHeight * 4, 0);
    }

    // Load textures and build level 0
    for (size_t i = 0; i < textureFiles.size(); ++i) {
        int w, h, channels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(textureFiles[i].c_str(), &w, &h, &channels, 4);

        if (data) {
            int flippedRow = static_cast<int>(textureFiles.size() - 1 - i);
            int rowOffset = flippedRow * cellHeight;

            // Copy full 64x48 texture to level 0
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    int atlasIdx = ((rowOffset + y) * atlasWidth + x) * 4;
                    int localIdx = (y * w + x) * 4;
                    for (int c = 0; c < 4; ++c) {
                        mipLevels[0][atlasIdx + c] = data[localIdx + c];
                    }
                }
            }

            // Now generate mipmaps for EACH 16x16 face independently
            // Faces: Top(16,0), Bottom(16,32), Sides(16,16)
            int facePositions[3][2] = { {16, 0}, {16, 32}, {16, 16} };

            for (int faceIdx = 0; faceIdx < 3; faceIdx++) {
                int baseFaceX = facePositions[faceIdx][0];
                int baseFaceY = facePositions[faceIdx][1] + rowOffset;

                // Extract 16x16 face
                std::vector<unsigned char> face16(16 * 16 * 4);
                extractFace(mipLevels[0].data(), face16.data(), atlasWidth, baseFaceX, baseFaceY);

                // Generate mip chain: 16->8->4->2->1
                std::vector<unsigned char> face8(8 * 8 * 4);
                std::vector<unsigned char> face4(4 * 4 * 4);
                std::vector<unsigned char> face2(2 * 2 * 4);
                std::vector<unsigned char> face1(1 * 1 * 4);

                downsample(face16.data(), face8.data(), 16);
                downsample(face8.data(), face4.data(), 8);
                downsample(face4.data(), face2.data(), 4);
                downsample(face2.data(), face1.data(), 2);

                // Write downsampled faces to corresponding mip levels
                int mipFaceX1 = baseFaceX / 2;
                int mipFaceY1 = baseFaceY / 2;
                writeFace(mipLevels[1].data(), face8.data(), atlasWidth / 2, 8, mipFaceX1, mipFaceY1);

                int mipFaceX2 = baseFaceX / 4;
                int mipFaceY2 = baseFaceY / 4;
                writeFace(mipLevels[2].data(), face4.data(), atlasWidth / 4, 4, mipFaceX2, mipFaceY2);

                int mipFaceX3 = baseFaceX / 8;
                int mipFaceY3 = baseFaceY / 8;
                writeFace(mipLevels[3].data(), face2.data(), atlasWidth / 8, 2, mipFaceX3, mipFaceY3);

                int mipFaceX4 = baseFaceX / 16;
                int mipFaceY4 = baseFaceY / 16;
                writeFace(mipLevels[4].data(), face1.data(), atlasWidth / 16, 1, mipFaceX4, mipFaceY4);
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
        << " (" << textureFiles.size() << " blocks) with 5 manual mip levels (LOD bias: -3.0)" << std::endl;

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

    // No padding - use exact UV coordinates for 16x16 face
    float uMin = static_cast<float>(localX) / static_cast<float>(atlasWidth);
    float vMin = 1.0f - (static_cast<float>(pixelY + 16) / static_cast<float>(atlasHeight));
    float uMax = static_cast<float>(localX + 16) / static_cast<float>(atlasWidth);
    float vMax = 1.0f - (static_cast<float>(pixelY) / static_cast<float>(atlasHeight));

    return { uMin, vMin, uMax, vMax };
}

void TextureAtlas::bind() const {
    glBindTexture(GL_TEXTURE_2D, textureID);
}