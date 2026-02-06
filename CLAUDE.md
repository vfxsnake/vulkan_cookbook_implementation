# CLAUDE.md - Vulkan Rendering Engine Project

## Project Context

This project is an incremental implementation of concepts from "Vulkan 3D Graphics Rendering Cookbook, 2nd Edition" by Sergey Kosarevsky, Viktor Latypov, and Alexey Medvedev.

**Important**: The full PDF book is too large for Claude Code to open directly. Reference documentation has been extracted to the `docs/book_reference/` folder.

## Reference Documentation

```
VulkanEngine/docs/vulkan_book_reference/
├── 00_QuickReference.md
├── Chapter01_BuildEnvironment.md
├── Chapter02_GettingStarted.md
├── Chapter03_VulkanObjects.md
├── Chapter04_User_Interaction_Productivity.md
├── Chapter05_Working_With_Geometry_Data.md
├── Chapter06_PBR_glTF_Shading.md
├── Chapter07_Advanced_PBR_Extensions.md
├── Chapter08_Graphics_Rendering_Pipeline.md
├── chapter09_Gltf_Animations.md
├── Chapter10_Image_Based_Techniques.md
└── Chapter11_Reference.md
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

## Working Agreement

This section defines how Claude and the user collaborate on this project.

### Learning-First Approach
- **User writes all code** — Claude provides step-by-step instructions and guidance
- **User asks for help when stuck** — Claude doesn't write code unless explicitly requested
- **Inspect and test early** — User shares code for review, we test as soon as possible
- **Incremental progress** — Small steps, verify each works before moving on

### Master Learning Plan
The detailed session breakdown is in **`Vulkan_Learning_Plan.md`** at project root.
- Contains all sessions (1.1, 1.2, ... 11.6) with goals and test criteria
- Use this to understand what each session should accomplish
- Claude references this when planning session tasks

### Reference-Based Learning
Claude has three reference sources to guide the user:

1. **Book code repo**: `3D-Graphics-Rendering-Cookbook-Second-Edition/`
   - Actual working code examples from the book
   - User studies these before implementing their own version

2. **Book chapter summaries**: `VulkanEngine/docs/vulkan_book_reference/`
   - Markdown extracts with explanations, concepts, and context
   - Claude points to specific sections for deeper understanding

3. **User's book copy**: Physical/digital PDF
   - For detailed explanations beyond the summaries
   - User can look up specific pages/sections as needed

**How it works**:
- Claude points user to relevant code files AND markdown docs
- Claude asks comprehension questions to verify understanding
- User implements based on what they learned
- This ensures learning through reading real code and explanations, not just following instructions

### Workflow
1. **Reference**: Claude points to relevant files in `3D-Graphics-Rendering-Cookbook-Second-Edition/` for the user to study
2. **Comprehension check**: Claude asks questions to verify understanding of the reference code
3. **Instructions**: Claude gives instructions for the next task (what to create, key concepts)
4. **Implementation**: User writes the code themselves
5. **Review**: User shares code, Claude inspects and provides feedback
6. **Fix**: User addresses any issues
7. **Test**: Build and run to verify it works
8. **Next**: Move to next task

### Environment
- **User builds on**: Windows (Visual Studio 2022)
- **Claude runs on**: WSL2 (can read files, run commands for inspection)
- **Build system**: CMake with VS 2022 generator

### Session Continuity
- This CLAUDE.md file is the source of truth for project state
- Each session updates the Session Log with progress and next steps
- When resuming, say: "Let's continue the Vulkan learning plan. Check CLAUDE.md for current progress."

---

## Current Progress & Session Log

**Current session: 1.1 - Environment Setup (IN PROGRESS)**
**Current milestone: v0.1-build-environment (Chapter 1)**

### How to Resume

When starting a new Claude Code conversation, say:
> "Let's continue the Vulkan learning plan. Check CLAUDE.md for current progress."

Claude will read this file and pick up where you left off.

### Milestone Checklist

- [ ] `v0.1-build-environment` — Chapter 1: CMake project, GLFW window, shader compilation
- [ ] `v0.2-vulkan-basics` — Chapter 2: Vulkan init, command buffers, first triangle, 3D transforms
- [ ] `v0.3-resource-management` — Chapter 3: Buffers, textures, bindless descriptors, mesh loading
- [ ] `v0.4-dev-tools` — Chapter 4: ImGui, Tracy, camera system, skybox, debug rendering
- [ ] `v0.5-geometry-pipeline` — Chapter 5: LOD, vertex pulling, instancing, compute, tessellation, indirect draw
- [ ] `v0.6-pbr-foundation` — Chapter 6: Cook-Torrance BRDF, IBL, BRDF LUT, glTF PBR
- [ ] `v0.7-advanced-pbr` — Chapter 7: Clearcoat, sheen, transmission, volume, punctual lights
- [ ] `v0.8-scene-system` — Chapter 8: DOD scene graph, material system, large scenes, editor
- [ ] `v0.9-animation` — Chapter 9: Node animation, skeletal/GPU skinning, morph targets, blending
- [ ] `v0.10-post-processing` — Chapter 10: Shadows, MSAA, SSAO, HDR, tone mapping, eye adaptation
- [ ] `v1.0-complete-engine` — Chapter 11: GPU culling, CSM, OIT, async loading, final integration

### Session Log

Each entry records what was done so the next conversation can continue seamlessly.

#### Session 1.1 — Environment Setup (IN PROGRESS)

**Goal**: Create the basic project structure and get a minimal build compiling before adding Vulkan dependencies.

**Status**: Phase A complete, ready for Phase B

**Prerequisites completed**:
- [x] Ran `deploy_deps.py` in book repo root — all deps cached/ready
- [x] Ran `deploy_deps.py` in `deps/src/lightweightvk/` — succeeded (taskflow bootstrap failed but not needed)

**Session 1.1 Checklist**:

Phase A - Minimal Build (no dependencies): **COMPLETE**
- [x] Create `VulkanEngine/src/` directory
- [x] Create `VulkanEngine/shaders/` directory
- [x] Create `VulkanEngine/CMakeLists.txt` (minimal: cmake version, project, C++20, add_executable)
- [x] Create `VulkanEngine/src/main.cpp` (just prints "Hello Vulkan")
- [x] Test build with CMake + Visual Studio
- [x] Run the executable to verify it works

Phase B - Add GLFW Window:
- [ ] Add GLFW dependency to CMakeLists.txt
- [ ] Update main.cpp to create a GLFW window
- [ ] Build and test window appears

Phase C - Add Vulkan via LightweightVK:
- [ ] Add LightweightVK dependency to CMakeLists.txt
- [ ] Add GLM dependency
- [ ] Update main.cpp with basic Vulkan initialization
- [ ] Build and test Vulkan context works

Phase D - Shader Compilation:
- [ ] Add GLSLang dependency
- [ ] Create a simple vertex/fragment shader
- [ ] Add runtime shader compilation code
- [ ] Build and test shaders compile

**Current task**: Phase B - Add GLFW Window

**Session notes**:
- User studied `Chapter01/01_CMake/CMakeLists.txt` from book repo before implementing
- Learned CMake build process: `-G` (generator), `-A` (architecture), `--config` (build configuration)
- Fixed glob pattern issue in add_executable (changed `src/*.cpp` to explicit `src/main.cpp`)

**Files to create**:
- `VulkanEngine/CMakeLists.txt`
- `VulkanEngine/src/main.cpp`

**Next action for user**: Create the directories and files as instructed, then share for review.

---
*Update this log after each session. Mark sessions DONE and add a summary of what was accomplished, files changed, and any issues encountered.*

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
