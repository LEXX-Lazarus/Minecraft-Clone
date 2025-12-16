# Minecraft Clone - Setup Instructions

## Project Structure
```
Minecraft Clone/
├── src/
│   ├── core/
│   │   ├── window.h
│   │   └── window.cpp
│   ├── graphics/
│   │   ├── renderer.h
│   │   └── renderer.cpp
│   └── main.cpp
├── external/
└── README.md
```

## Setting up SDL2 in Visual Studio

### 1. Download SDL2
- Go to https://www.libsdl.org/download-2.0.php
- Download "SDL2-devel-2.x.x-VC.zip" (Development Libraries for Visual C++)
- Extract the zip file to your `external` folder or somewhere like `C:\SDL2`

### 2. Configure Visual Studio Project

#### Include Directories:
1. Right-click your project → Properties
2. Configuration: All Configurations, Platform: Active(Win32) or x64
3. Go to: C/C++ → General → Additional Include Directories
4. Add: `C:\SDL2\include` (or `$(SolutionDir)external\SDL2\include` if in external folder)

#### Library Directories:
1. In Project Properties
2. Go to: Linker → General → Additional Library Directories
3. Add: `C:\SDL2\lib\x64` (or `x86` for 32-bit)

#### Linker Input:
1. Go to: Linker → Input → Additional Dependencies
2. Add: `SDL2.lib;SDL2main.lib;opengl32.lib;`

#### Subsystem:
1. Go to: Linker → System → SubSystem
2. Select: Console (/SUBSYSTEM:CONSOLE)

### 3. Copy SDL2.dll
- Copy `SDL2.dll` from `C:\SDL2\lib\x64` to your project's output directory
- Or add the lib folder to your system PATH

### 4. Add Files to Visual Studio
Make sure to add the new files to your project:
- Right-click on `src/core` folder → Add → Existing Item → Add `window.h` and `window.cpp`
- Right-click on `src/graphics` folder → Add → Existing Item → Add `renderer.h` and `renderer.cpp`

### 5. Build and Run
- Press F7 to build
- Press F5 to run
- You should see a window titled "Minecraft Clone"
- Press ESC or close the window to exit

## Current Features
- ✅ Window management class
- ✅ Basic renderer class structure
- ✅ OpenGL context creation
- ✅ Clean game loop
- ✅ Event handling

## Next Steps
1. Add GLEW or GLAD for OpenGL function loading
2. Implement basic OpenGL rendering (clear color)
3. Create camera system
4. Add basic cube/voxel rendering
5. Implement chunk system

## Coding Standards
- Use camelCase for variables and functions
- Use PascalCase for class names
- Prefix member variables with `m_`
- Use descriptive names

## Controls (Current)
- ESC: Close window
- X button: Close window