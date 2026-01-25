# CLAUDE.md - Vulkan Rendering Engine Project

## Project Context

This project is an incremental implementation of concepts from "Vulkan 3D Graphics Rendering Cookbook, 2nd Edition" by Sergey Kosarevsky, Viktor Latypov, and Alexey Medvedev.

**Important**: The full PDF book is too large for Claude Code to open directly. Reference documentation has been extracted to the `docs/book_reference/` folder.

## Reference Documentation

```
docs/vulkan_book_reference/
├── 00_QuickReference.md      # API summary, build commands, common patterns
├── Chapter01_BuildEnvironment.md
├── Chapter02_GettingStarted.md
├── Chapter03_VulkanObjects.md
├── Chapter04_DevTools.md
├── Chapter05_Geometry.md
├── Chapter06_PBR.md
├── Chapter07_AdvancedPBR.md
├── Chapter08_RenderingPipeline.md
├── Chapter09_Animations.md
├── Chapter10_ImageTechniques.md
└── Chapter11_Optimization.md
```

## Book Repository

The original book code is at:
https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition

Clone it alongside this project for reference code.
it has been cloned by the user under: 3D-Graphics-Rendering-Cookbook-Second-Edition/

## Project Architecture

Unlike the book's standalone demos, this project maintains a single evolving codebase:

```
VulkanEngine/
├── src/
│   ├── core/           # Vulkan context, window, input
│   ├── resources/      # Buffers, textures, shaders
│   ├── rendering/      # Pipelines, render passes
│   ├── scene/          # Scene graph, materials, meshes
│   ├── effects/        # PBR, shadows, SSAO, HDR
│   └── main.cpp
├── shaders/            # GLSL shaders
├── docs/
│   └── book_reference/ # Extracted book content
└── CMakeLists.txt
```

## Key Technologies

- **Vulkan 1.3** - Modern graphics API
- **LightweightVK** - Thin Vulkan wrapper (from book repo deps/)
- **GLFW** - Window/input
- **GLM** - Math library
- **GLSLang** - Runtime shader compilation
- **Assimp** - Model loading
- **STB** - Image loading
- **ImGui** - Debug UI

## Common Tasks

### Build
```bash
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Run (from project root!)
```bash
./build/Release/VulkanEngine.exe
```

### Add New Shader
1. Create `.vert`/`.frag` in `shaders/`
2. Shaders compile at runtime via GLSLang

## Current Progress

Track progress via Git tags:
- `v0.1-build-environment` - Chapter 1 complete
- `v0.2-vulkan-basics` - Chapter 2 complete
- ... (see Vulkan_Learning_Plan.docx)

## When Asked About Book Content

If the user asks about specific book topics:

1. First check `docs/book_reference/` for extracted content
2. If not found, explain that the full PDF is too large and offer to:
   - Search the book repo for relevant code
   - Extract the specific section needed
   - Provide guidance based on available reference docs

## Code Style

- C++20 features (designated initializers, concepts)
- Use `lvk::` namespace for LightweightVK types
- Use `Holder<T>` for RAII resource management
- Prefer push constants over uniform buffers for small data
- Use bindless textures (descriptor indexing)

## Useful Commands for Claude Code

```bash
# Search book repo for specific topic
grep -r "topic" /path/to/book/repo --include="*.cpp" --include="*.h"

# Find shader examples
find /path/to/book/repo -name "*.vert" -o -name "*.frag" | xargs grep "keyword"

# Check Vulkan validation
# Run the app and look for validation layer output
```

## Contact for Full Book Access

For detailed explanations beyond the reference docs, the user should:
1. Refer to their physical/digital copy of the book
2. Ask Claude (web) which has the full PDF in project knowledge
3. Check the book's official resources
