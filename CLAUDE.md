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

### Chapter Review Process (Before Starting Any Chapter)

When beginning a new chapter, follow these steps **before any implementation**:

1. **Check Learning Plan**: Read the chapter section in `Vulkan_Learning_Plan.md` — note the session breakdown, key concepts, and milestone goals
2. **Check Book Reference**: Read the corresponding `docs/vulkan_book_reference/ChapterXX_*.md` — identify all topics covered
3. **Gap Analysis**: Compare what the learning plan expects vs what the book reference covers. Identify:
   - Topics already completed from previous sessions
   - Topics in the book reference that the learning plan doesn't explicitly mention (these may still be relevant)
   - Topics that are not relevant yet (future chapters will cover them)
4. **Record Chapter Scope**: Add a "Chapter X Scope" entry to the Session Log listing:
   - What must be covered (from learning plan + book reference)
   - What was already done
   - What is deferred to later chapters
5. **Then proceed** with the per-session workflow below

### Per-Session Workflow

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

**Current session: 1.2 - (NEXT)**
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

### Future Tasks (Non-blocking)

These are improvements to tackle after the learning plan is further along:

- [ ] **Migrate dependencies to CMake FetchContent** — Replace relative-path references to the book repo's `deps/` with `FetchContent_Declare`/`FetchContent_MakeAvailable` so the project can fetch its own deps (GLFW, GLM, GLSLang, Assimp, STB, ImGui)
- [ ] **Add git submodules as an alternative** — Evaluate using git submodules for deps that don't support FetchContent well, so the project is fully self-contained and cloneable

### Chapter Scopes

#### Chapter 1 Scope — Build Environment

**From Learning Plan**: CMake setup, GLFW window + input, GLSLang runtime shader compilation
**From Book Reference** (`Chapter01_BuildEnvironment.md`):
- [x] CMake project configuration (`SETUP_APP` macro patterns, C++20, generators)
- [x] GLFW window creation (`GLFW_NO_API`, event loop)
- [x] GLSLang runtime shader compilation (handled internally by LVK)
- [ ] `VS_DEBUGGER_WORKING_DIRECTORY` — set debugger working directory so shader paths are relative to project root *(missed, fixing now)*
- [ ] `SETUP_GROUPS` macro — organize source files into VS Solution Explorer folders *(deferred, only 1 source file currently)*
- [ ] Output name per build config (`_Debug`, `_Release` suffixes) *(deferred, nice-to-have)*
- Taskflow multithreading — **deferred to Chapter 11** (async loading)
- BC7 texture compression — **deferred to Chapter 3** (textures)

---

### Session Log

Each entry records what was done so the next conversation can continue seamlessly.

#### Session 1.1 — Environment Setup (DONE)

**Goal**: Create the basic project structure and get a minimal build compiling before adding Vulkan dependencies.

**Status**: COMPLETE — all phases done, triangle rendering on screen

**Prerequisites completed**:
- [x] Ran `deploy_deps.py` in book repo root — all deps cached/ready
- [x] Ran `deploy_deps.py` in `deps/src/lightweightvk/` — fixed by enabling `git config --global core.longpaths true` and re-running

**Session 1.1 Checklist**:

Phase A - Minimal Build (no dependencies): **COMPLETE**
- [x] Create `VulkanEngine/src/` directory
- [x] Create `VulkanEngine/shaders/` directory
- [x] Create `VulkanEngine/CMakeLists.txt` (minimal: cmake version, project, C++20, add_executable)
- [x] Create `VulkanEngine/src/main.cpp` (just prints "Hello Vulkan")
- [x] Test build with CMake + Visual Studio
- [x] Run the executable to verify it works

Phase B - Add GLFW Window: **COMPLETE**
- [x] Add GLFW dependency to CMakeLists.txt
- [x] Update main.cpp to create a GLFW window
- [x] Build and test window appears

Phase C - Add Vulkan via LightweightVK: **COMPLETE**
- [x] Add LightweightVK dependency to CMakeLists.txt
- [x] Add GLM dependency
- [x] Update main.cpp with basic Vulkan initialization
- [x] Build and test Vulkan context works

Phase D - Shader Compilation: **COMPLETE**
- [x] Study reference code (Chapter01/04_GLSLang, shared/Utils.cpp, Chapter02/02_HelloTriangle)
- [x] Comprehension questions completed
- [x] Create vertex/fragment shaders in `VulkanEngine/shaders/`
- [x] Add `readShaderFile()` and `loadShaderModule()` helpers to main.cpp
- [x] Create render pipeline with both shader modules
- [x] Add draw commands in render loop (cmdBeginRendering, cmdBindRenderPipeline, cmdDraw, cmdEndRendering)
- [x] Build and test — triangle renders on screen

**Session notes**:
- User studied `Chapter01/01_CMake/CMakeLists.txt` from book repo before implementing
- Learned CMake build process: `-G` (generator), `-A` (architecture), `--config` (build configuration)
- Fixed glob pattern issue in add_executable (changed `src/*.cpp` to explicit `src/main.cpp`)
- Phase B: User studied `shared/HelpersGLFW.cpp` and `Chapter01/02_GLFW/src/main.cpp` from book repo
- Learned GLFW_INCLUDE_NONE, GLFW_NO_API, error callbacks vs null checks, glfwSetKeyCallback vs glfwGetKey polling
- Used `add_subdirectory` with relative path to book repo's GLFW dep (via lightweightvk third-party)
- Added defensive null checks for glfwInit and glfwCreateWindow
- Phase C: User studied `Chapter02/01_Swapchain/src/main.cpp`, book root `CMakeLists.txt`, and `shared/CMakeLists.txt`
- Learned distinction between `add_subdirectory` binary dir alias vs CMake link target (`LVKLibrary`)
- Learned `lvk::initWindow()` replaces manual GLFW init, `lvk::createVulkanContextWithSwapchain()` creates Vulkan context
- LVK link target is `LVKLibrary` (not `lightweightvk`)
- GLM needs explicit include path + `GLM_ENABLE_EXPERIMENTAL` definition
- Tracy disabled for now (LVK_WITH_TRACY OFF, LVK_WITH_TRACY_GPU OFF)
- Phase C build fix: `deploy_deps.py` was failing on `taskflow` due to Windows 260-char path limit. Fixed with `git config --global core.longpaths true`, deleting broken taskflow clone, and re-running script. All LVK deps (minilog, imgui, volk, vma, vulkan-headers, ldrutils) then cloned successfully.

**Files modified**:
- `VulkanEngine/CMakeLists.txt` — added LVK, GLM, definitions
- `VulkanEngine/src/main.cpp` — replaced GLFW init with LVK calls, added render loop with command buffer

**Key learnings from Phase D study**:
- GLSLang is already bundled inside LVK — no separate CMake dependency needed
- `ctx->createShaderModule()` auto-detects GLSL vs SPIR-V based on `desc.dataSize` (0 = GLSL text, >0 = SPIR-V binary)
- `readShaderFile()` in shared/Utils.cpp supports recursive `#include` resolution
- `lvk::Holder<>` provides RAII cleanup for Vulkan resources (like `std::unique_ptr` for GPU handles)
- Need both vertex + fragment shader modules to create a `RenderPipelineDesc`

**Key learnings from Phase D implementation**:
- `ShaderModuleDesc` has explicit constructors → NOT an aggregate → designated initializers `{.field=...}` don't work, must use positional constructor: `{source, stage, debugName}`
- C++20 designated initializers only work on aggregate types (no user-defined constructors, no private members, no virtual functions)
- Runtime shader compilation (GLSL text → SPIR-V at startup) vs pre-compiled (offline `.spv` binary): LVK handles runtime compilation via GLSLang internally when `dataSize == 0`
- Shader paths are relative to working directory at runtime — plan accordingly
- Added naming convention to Code Style section (PascalCase types, camelCase methods/members, snake_case locals/params)

**Files modified this session**:
- `VulkanEngine/shaders/triangle.vert` — vertex shader with hardcoded triangle positions + per-vertex RGB colors
- `VulkanEngine/shaders/triangle.frag` — fragment shader with color interpolation
- `VulkanEngine/src/main.cpp` — added `readShaderFile()`, shader module loading with `lvk::Holder`
- `.vscode/c_cpp_properties.json` — updated include paths for IntelliSense
- `CLAUDE.md` — added naming convention to Code Style

**Phase D final learnings**:
- `RenderPipelineDesc` uses designated initializers: `.smVert`, `.smFrag`, `.color` with swapchain format
- Draw command sequence: `cmdBeginRendering` → `cmdBindRenderPipeline` → `cmdDraw` → `cmdEndRendering`
- `cmdBeginRendering` takes a `RenderPass` (load op + clear color) and a `Framebuffer` (target texture)
- `LoadOp_Clear` fills the framebuffer with `clearColor` before drawing; `LoadOp_Load` preserves previous contents
- Debug label commands (`cmdPushDebugGroupLabel`/`cmdPopDebugGroupLabel`) are optional, for tools like RenderDoc
- Known issue: validation layer warnings on shutdown about undestroyed shader modules — holders are destroyed after `ctx.reset()`. Fix later with scoped `{}` block or explicit holder reset.

**Next session**: 1.2 — check `Vulkan_Learning_Plan.md` for goals

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

**ENFORCE THIS CONVENTION THROUGHOUT THE ENTIRE PROJECT. All code reviews must check for naming consistency.**

### Naming Convention

| Element | Style | Example |
|---|---|---|
| Classes, Types, Structs | PascalCase | `ShaderLoader`, `RenderState` |
| Functions, Methods | camelCase | `readShaderFile()`, `loadModule()` |
| Member variables | camelCase | `windowWidth`, `shaderCode` |
| Local variables | snake_case | `file_size`, `shader_source` |
| Parameters | snake_case | `file_path`, `stage_flags` |

- Member variables use camelCase to visually distinguish them from local variables (snake_case) without needing `m_` prefixes
- This convention aligns with LVK's API style (PascalCase types, camelCase methods)

### General Style

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
