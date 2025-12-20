cmake_minimum_required(VERSION 3.15)
project(MinecraftClone VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to SDL3
set(SDL3_DIR "C:/dev/libs/SDL3-3.2.28")
set(SDL3_INCLUDE_DIRS "${SDL3_DIR}/include/SDL3")

# Check if you have x64 or x86 build
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(SDL3_LIBRARIES "${SDL3_DIR}/lib/x64/SDL3.lib")
    set(SDL3_DLL "${SDL3_DIR}/lib/x64/SDL3.dll")
else()
    set(SDL3_LIBRARIES "${SDL3_DIR}/lib/x86/SDL3.lib")
    set(SDL3_DLL "${SDL3_DIR}/lib/x86/SDL3.dll")
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add GLAD source
set(GLAD_SOURCES "external/glad/src/glad.c")

# Source files
set(SOURCES
    src/main.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/Texture.cpp
    src/Shader.cpp
    src/Window.cpp
    src/GUI/GUI.cpp
    src/GUI/DebugOverlay.cpp
    src/Chunk.cpp
    src/ChunkManager.cpp
    src/TerrainGenerator.cpp
    src/PerlinNoise.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${GLAD_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDL3_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    ${SDL3_LIBRARIES}
)

# Copy SDL3.dll to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${SDL3_DLL}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
)

# Copy assets folder to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)