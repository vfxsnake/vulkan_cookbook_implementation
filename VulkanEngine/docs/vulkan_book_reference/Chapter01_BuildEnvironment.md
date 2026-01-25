# Chapter 1: Establishing a Build Environment

## Overview
This chapter covers setting up a Vulkan 1.3 development environment on Windows and Linux.

## Required Tools (Windows)

1. **Visual Studio 2022** - Community Edition with C++ workload
2. **Git** - With Git LFS for large files
3. **CMake 3.19+** - Build system
4. **Python 3.x** - For dependency scripts
5. **Vulkan SDK 1.4+** - From https://vulkan.lunarg.com

## Project Setup Commands

```bash
# Clone the repository
git lfs install
git clone https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition.git

# Download dependencies (~15GB)
python deploy_deps.py

# Build on Windows
cd .build
cmake .. -G "Visual Studio 17 2022" -A x64
start RenderingCookbook2.sln
```

## CMake Configuration (CommonMacros.txt)

### SETUP_GROUPS Macro
Organizes files into Visual Studio solution folders:

```cmake
macro(SETUP_GROUPS src_files)
  foreach(FILE ${src_files})
    get_filename_component(PARENT_DIR "${FILE}" PATH)
    set(GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    source_group("${GROUP}" FILES "${FILE}")
  endforeach()
endmacro()
```

### SETUP_APP Macro
Creates a CMake project with standard properties:

```cmake
macro(SETUP_APP projname chapter)
  set(FOLDER_NAME ${chapter})
  set(PROJECT_NAME ${projname})
  project(${PROJECT_NAME} CXX)
  
  file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.c??)
  file(GLOB_RECURSE HEADER_FILES LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.h)
  
  include_directories(src)
  add_executable(${PROJ_NAME} ${SRC_FILES} ${HEADER_FILES})
  
  SETUP_GROUPS("${SRC_FILES}")
  SETUP_GROUPS("${HEADER_FILES}")
  
  # Build configuration names
  set_target_properties(${PROJ_NAME}
    PROPERTIES OUTPUT_NAME_DEBUG ${PROJ_NAME}_Debug)
  set_target_properties(${PROJ_NAME}
    PROPERTIES OUTPUT_NAME_RELEASE ${PROJ_NAME}_Release)
  set_target_properties(${PROJ_NAME}
    PROPERTIES OUTPUT_NAME_RELWITHDEBINFO ${PROJ_NAME}_ReleaseDebInfo)
  
  # C++20 standard
  set_property(TARGET ${PROJ_NAME} PROPERTY CXX_STANDARD 20)
  set_property(TARGET ${PROJ_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
  
  # Visual Studio debugging
  if(MSVC)
    add_definitions(-D_CONSOLE)
    set_property(TARGET ${PROJ_NAME} PROPERTY
      VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endif()
endmacro()
```

### Example CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.19)
project(Chapter01)
include(../../CMake/CommonMacros.txt)
SETUP_APP(Ch01_Sample01_CMake "Chapter 01")
```

## GLFW Window Creation

Basic window setup pattern:

```cpp
#include <GLFW/glfw3.h>

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL context
    
    GLFWwindow* window = glfwCreateWindow(960, 540, "Vulkan App", nullptr, nullptr);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // Render frame
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

## Runtime Shader Compilation (GLSLang)

The book uses GLSLang to compile GLSL shaders to SPIR-V at runtime:

```cpp
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

// Initialize GLSLang once at startup
glslang::InitializeProcess();

// Compile shader
glslang::TShader shader(EShLangVertex); // or EShLangFragment
shader.setStrings(&shaderSource, 1);
shader.setEnvInput(glslang::EShSourceGlsl, EShLangVertex, glslang::EShClientVulkan, 100);
shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_3);
shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

if (!shader.parse(...)) {
    // Handle error: shader.getInfoLog()
}

// Link program
glslang::TProgram program;
program.addShader(&shader);
program.link(EShMsgDefault);

// Generate SPIR-V
std::vector<uint32_t> spirv;
glslang::GlslangToSpv(*program.getIntermediate(EShLangVertex), spirv);

// Cleanup at shutdown
glslang::FinalizeProcess();
```

## Taskflow (Multithreading)

```cpp
#include <taskflow/taskflow.hpp>

tf::Executor executor;
tf::Taskflow taskflow;

auto [A, B, C] = taskflow.emplace(
    []() { /* Task A */ },
    []() { /* Task B */ },
    []() { /* Task C */ }
);

A.precede(B, C);  // A runs before B and C
executor.run(taskflow).wait();
```

## BC7 Texture Compression

For compressing textures to BC7 format (GPU-friendly):

```cpp
#include <bc7enc.h>

bc7enc_compress_block_init();

// For each 4x4 block of pixels:
bc7enc_compress_block(outputBlock, inputPixels, &params);
```

## Demo Applications

| Demo | Description |
|------|-------------|
| 01_CMake | Basic CMake project setup |
| 02_GLFW | Window creation with GLFW |
| 03_Taskflow | Multithreading example |
| 04_GLSLang | Runtime shader compilation |
| 05_BC7Compression | Texture compression |

## Key Dependencies

- **GLFW 3.4** - Window/input handling
- **GLM** - Math library (vectors, matrices)
- **GLSLang** - Shader compilation
- **STB** - Image loading
- **Assimp** - 3D model loading
- **Taskflow** - Task-based parallelism
- **bc7enc** - Texture compression

## Important Notes

1. **Run demos from repository root** - Assets are in `data/` folder
2. **Vulkan 1.3 required** - The book uses modern Vulkan features
3. **C++20 required** - Designated initializers, etc.
4. **LightweightVK** - The book's Vulkan wrapper library (in deps/)
