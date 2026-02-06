# __Vulkan 3D Graphics Rendering Cookbook__

*Incremental Learning Plan for Windows Development*

# Project Overview

This learning plan transforms the cookbook's chapter\-based approach into an incremental, iterative project\. Each 2\-hour session builds upon previous work, creating a continuously evolving Vulkan rendering engine\. Progress will be saved to GitHub after each milestone\.

## Project Architecture Strategy

Unlike the book's standalone demos per chapter, your project will maintain a single evolving codebase with these layers:

- Core Layer: VulkanApp wrapper, LightweightVK context management, window handling
- Resource Layer: Buffer management, texture loading, shader compilation
- Rendering Layer: Pipeline creation, render passes, command buffer management
- Scene Layer: Scene graph, materials, mesh storage, transformations
- Effects Layer: PBR, shadows, SSAO, HDR, post\-processing
- Optimization Layer: Culling, indirect rendering, async loading

## Prerequisites & Setup

- Visual Studio 2022 with C\+\+ workload
- Vulkan SDK 1\.4\+ \(supports Vulkan 1\.3\)
- CMake 3\.19\+
- Git with Git LFS installed
- Python 3\.x
- GPU: GeForce RTX 2060 or newer recommended
- ~15GB disk space for assets

## Estimated Timeline

__Phase__

__Chapters__

__Sessions__

__Hours__

Foundation

1\-2

4\-5

8\-10

Core Systems

3\-4

5\-6

10\-12

Geometry & Rendering

5\-6

7\-8

14\-16

Advanced PBR

7\-8

6\-7

12\-14

Animation & Effects

9\-10

6\-7

12\-14

Optimization & Integration

11

4\-5

8\-10

TOTAL

1\-11

32\-38

64\-76

# Chapter 1: Establishing a Build Environment

*Foundation phase \- Set up the development infrastructure that will support all future work\.*

## Key Concepts

- CMake project configuration with CommonMacros\.txt
- GLFW window creation and event handling
- Taskflow for multithreading
- GLSLang for runtime shader compilation
- BC7 texture compression

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

1\.1

Environment Setup

Install VS2022, Vulkan SDK, CMake, Git LFS\. Clone repo, run deploy\_deps\.py\. Create your project folder with CMakeLists\.txt

Build any Ch01 demo

1\.2

GLFW Integration

Create VulkanEngine project\. Add GLFW wrapper class for window creation\. Implement input handling callbacks

Window opens, responds to input

1\.3

Shader System

Integrate GLSLang\. Create ShaderCompiler class for runtime GLSL compilation\. Add shader hot\-reloading support

Compile test\.vert/\.frag at runtime

## Git Milestone: v0\.1\-build\-environment

- Working CMake project structure
- GLFW window with input handling
- Runtime shader compilation

# Chapter 2: Getting Started with Vulkan

*Core Vulkan initialization \- The critical foundation for all rendering\.*

## Key Concepts

- VkInstance and VkDevice creation
- Swapchain management
- Validation layers and debugging
- Command buffers and queues
- Shader modules and pipelines
- LightweightVK \(lvk\) wrapper patterns

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

2\.1

Vulkan Context

Study lvk::VulkanContext\. Create VulkanRenderer class wrapping instance, device, swapchain creation\. Enable validation layers

Black window renders, validation works

2\.2

Command Buffers

Implement CommandBufferManager using lvk patterns\. Create submit/present flow\. Add synchronization

Clean frame loop, no validation errors

2\.3

First Triangle

Create pipeline abstraction\. Implement vertex/fragment shader loading\. Render colored triangle

Triangle renders correctly

2\.4

GLM & Transforms

Add GLM integration\. Implement MVP matrices\. Create rotating 3D cube with perspective

Animated cube with proper depth

## Git Milestone: v0\.2\-vulkan\-basics

- VulkanRenderer class with clean initialization/shutdown
- Command buffer management
- Basic pipeline creation
- 3D rendering with transforms

# Chapter 3: Working with Vulkan Objects

*Resource management \- Buffers, textures, and the staging pattern\.*

## Key Concepts

- GPU buffer types \(vertex, index, uniform, storage\)
- Staging buffers for CPU to GPU transfers
- VkImage and VkImageView
- Texture loading with STB
- Descriptor indexing \(bindless\)

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

3\.1

Buffer Management

Create BufferManager class\. Implement vertex/index/uniform buffer creation\. Add staging buffer support

Load mesh from Assimp, render it

3\.2

Texture System

Create TextureManager\. Implement 2D texture loading via STB\. Add sampler creation

Textured mesh renders correctly

3\.3

Descriptor Indexing

Enable bindless textures\. Create global descriptor set\. Implement texture array access in shaders

Multiple textures via bindless

## Git Milestone: v0\.3\-resource\-management

- BufferManager with staging support
- TextureManager with STB loading
- Bindless descriptor setup
- Mesh loading via Assimp

# Chapter 4: Adding User Interaction and Productivity Tools

*Developer tools \- Debugging, profiling, and user interaction\.*

## Key Concepts

- ImGui integration
- Tracy profiler
- FPS counter and graphs
- Cube map textures \(environment maps\)
- Camera systems \(first\-person, orbit\)
- Debug line rendering

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

4\.1

ImGui Setup

Integrate ImGui with Vulkan\. Create debug UI panel\. Add FPS counter and memory stats

UI overlay works, shows stats

4\.2

Profiling

Integrate Tracy\. Add CPU/GPU profiling zones\. Create performance graphs with ImPlot

Tracy captures frame data

4\.3

Camera System

Implement CameraController class\. Add first\-person and orbit modes\. Implement smooth movement

Navigate scene with mouse/keyboard

4\.4

Environment Maps

Load HDR equirectangular maps\. Convert to cube maps\. Implement skybox rendering

Skybox renders correctly

4\.5

Debug Rendering

Create LineCanvas for debug drawing\. Add 3D grid shader\. Implement frustum visualization

Debug lines and grid visible

## Git Milestone: v0\.4\-dev\-tools

- ImGui\-based debug UI
- Tracy profiling integration
- Flexible camera system
- Skybox with cube map
- Debug line rendering

# Chapter 5: Working with Geometry Data

*Geometry pipeline \- LOD, instancing, tessellation, and mesh optimization\.*

## Key Concepts

- MeshOptimizer for LOD generation
- Programmable vertex pulling
- Instanced rendering
- Compute shaders
- Tessellation shaders
- Unified mesh data format \(VtxData\)
- Indirect rendering basics

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

5\.1

Mesh Format

Implement VtxData format from shared/Scene/\. Create MeshStorage class\. Add LOD generation with MeshOptimizer

Load mesh with multiple LODs

5\.2

Vertex Pulling

Implement programmable vertex pulling\. Remove vertex input descriptions\. Access vertices via storage buffers

Same mesh renders via pulling

5\.3

Instancing

Implement InstanceBuffer class\. Create per\-instance data\. Render 1000\+ instances efficiently

Million cubes demo runs >30 FPS

5\.4

Compute Shaders

Create ComputePipeline class\. Implement instance matrix calculation in compute\. Generate procedural textures

Compute\-generated content renders

5\.5

Tessellation

Add tessellation control/evaluation shaders\. Implement PN triangles or displacement\. Create tessellation demo

Tessellated sphere looks smooth

5\.6

Indirect Rendering

Implement indirect draw commands\. Create DrawIndirectBuffer\. Prepare for GPU\-driven rendering

Indirect draw produces same results

## Git Milestone: v0\.5\-geometry\-pipeline

- Unified mesh storage with LOD
- Vertex pulling implementation
- Efficient instanced rendering
- Compute shader infrastructure
- Indirect drawing foundation

# Chapter 6: Physically Based Rendering \(glTF 2\.0\)

*PBR fundamentals \- BRDF, IBL, and the metallic\-roughness model\.*

## Key Concepts

- BRDF fundamentals \(Cook\-Torrance\)
- BRDF LUT precomputation
- Environment map filtering \(specular/diffuse\)
- Irradiance convolution
- glTF metallic\-roughness model
- glTF specular\-glossiness model

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

6\.1

BRDF Foundation

Study Cook\-Torrance BRDF\. Implement Fresnel, GGX distribution, geometry functions in GLSL

Unlit glTF model loads

6\.2

BRDF LUT

Implement BRDF LUT generation via compute shader\. Store as 2D texture\. Verify lookup correctness

LUT texture generates correctly

6\.3

Environment Filtering

Implement specular map prefiltering\. Generate mip chain for roughness levels\. Create irradiance map

Filtered env maps look correct

6\.4

Metallic\-Roughness

Implement full glTF PBR shader\. Add IBL contributions\. Support all material textures

DamagedHelmet renders correctly

6\.5

Specular\-Glossiness

Add KHR\_materials\_pbrSpecularGlossiness support\. Create unified material system

Both workflows render correctly

## Git Milestone: v0\.6\-pbr\-foundation

- Complete PBR shader implementation
- IBL with BRDF LUT
- Environment map filtering pipeline
- glTF model loading with materials

# Chapter 7: Advanced PBR Extensions

*Extended materials \- Clearcoat, sheen, transmission, and more\.*

## Key Concepts

- KHR\_materials\_clearcoat
- KHR\_materials\_sheen
- KHR\_materials\_transmission
- KHR\_materials\_volume
- KHR\_materials\_ior
- KHR\_materials\_specular
- KHR\_lights\_punctual

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

7\.1

Clearcoat & Sheen

Implement clearcoat layer in shader\. Add sheen for fabric materials\. Update material system

Car paint and velvet render

7\.2

Transmission

Implement transmission for glass/water\. Add screen\-space refraction\. Handle thin surfaces

Glass sphere looks correct

7\.3

Volume & IOR

Add volume absorption/attenuation\. Implement custom IOR\. Handle thick glass volumes

Colored glass renders

7\.4

Specular & Emissive

Add specular extension for colored reflections\. Implement emissive strength\. Complete material system

All extensions work together

7\.5

Analytical Lights

Implement KHR\_lights\_punctual\. Add point, spot, directional lights\. Create lighting system

Multiple light types work

## Git Milestone: v0\.7\-advanced\-pbr

- All glTF material extensions
- Unified material system
- Punctual light support
- Complete glTF viewer

# Chapter 8: Graphics Rendering Pipeline

*Engine architecture \- Scene graphs, materials, and large scene handling\.*

## Key Concepts

- Data\-oriented scene graph design
- Transformation hierarchy
- Material system architecture
- Automatic material conversion
- Descriptor indexing for large scenes
- Indirect rendering integration
- Scene editing capabilities

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

8\.1

Scene Graph

Implement DOD scene graph from shared/Scene/\. Add node hierarchy\. Create transformation tree

Scene loads with hierarchy

8\.2

Material System

Create MaterialSystem class\. Implement automatic conversion\. Handle material variants

Materials auto\-convert correctly

8\.3

Large Scenes

Optimize for Bistro scene \(~3M triangles\)\. Use descriptor arrays\. Implement batching

Bistro runs >30 FPS

8\.4

Scene Editor

Add node manipulation UI\. Implement scene graph merging\. Add save/load functionality

Can edit and save scenes

## Git Milestone: v0\.8\-scene\-system

- DOD scene graph implementation
- Complete material system
- Large scene support
- Basic scene editing

# Chapter 9: glTF Animations

*Animation systems \- Node animations, skeletal, morph targets, and blending\.*

## Key Concepts

- Node\-based animations
- Keyframe interpolation
- Skeletal animation \(skinning\)
- GPU skinning with compute shaders
- Morph targets \(blend shapes\)
- Animation blending

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

9\.1

Animation Data

Parse glTF animation data\. Implement AnimationPlayer class\. Add keyframe interpolation \(step, linear, cubic\)

Simple animation plays

9\.2

Node Animation

Apply animations to scene graph\. Handle TRS channels\. Implement animation timing

Objects animate in scene

9\.3

Skeletal Animation

Load skeleton data\. Implement joint matrices\. Create skinning compute shader

Character mesh deforms

9\.4

Morph Targets

Load morph target data\. Implement blend weights\. Add morph in vertex shader

Facial expressions work

9\.5

Animation Blending

Implement animation state machine\. Add crossfade blending\. Create blend tree basics

Smooth transitions between anims

## Git Milestone: v0\.9\-animation

- Complete animation system
- GPU skinning
- Morph target support
- Animation blending

# Chapter 10: Image\-Based Techniques

*Post\-processing \- Shadows, SSAO, HDR, and offscreen rendering\.*

## Key Concepts

- Offscreen rendering
- Full\-screen triangle technique
- Shadow mapping
- MSAA
- Screen\-space ambient occlusion \(SSAO\)
- HDR rendering and tone mapping
- Light adaptation

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

10\.1

Offscreen Rendering

Create RenderTarget class\. Implement render\-to\-texture\. Add full\-screen triangle shader

Scene renders to texture

10\.2

Shadow Mapping

Implement shadow map generation\. Create shadow sampling\. Add PCF filtering

Shadows appear correctly

10\.3

MSAA

Create multisampled render targets\. Implement resolve\. Add MSAA toggle

Edges are smoother with MSAA

10\.4

SSAO

Implement SSAO pass\. Generate sample kernel\. Add blur pass for quality

Ambient occlusion visible

10\.5

HDR Pipeline

Switch to HDR render targets\. Implement tone mapping \(ACES, Reinhard\)\. Add exposure control

Bright areas don't clip

10\.6

Light Adaptation

Calculate average luminance\. Implement eye adaptation\. Add bloom effect

Auto\-exposure works

## Git Milestone: v0\.10\-post\-processing

- Complete post\-processing pipeline
- Shadow mapping
- SSAO
- HDR with tone mapping
- Eye adaptation

# Chapter 11: Advanced Rendering Techniques

*Optimization \- GPU culling, OIT, async loading, and final integration\.*

## Key Concepts

- CPU frustum culling
- GPU frustum culling \(compute\)
- Indirect rendering with culling
- Cascaded shadow maps
- Order\-independent transparency \(OIT\)
- Asynchronous texture loading

## Session Breakdown

__Session__

__Focus__

__Tasks & Goals__

__Test__

11\.1

CPU Culling

Implement bounding box hierarchy\. Add frustum culling on CPU\. Track culled object count

Culling reduces draw calls

11\.2

GPU Culling

Move culling to compute shader\. Use indirect draw count\. Implement GPU\-driven pipeline

Same results, better perf

11\.3

Cascaded Shadows

Implement CSM for directional lights\. Add cascade blending\. Handle large scenes

Sharp shadows at all distances

11\.4

Transparency \(OIT\)

Implement weighted\-blended OIT\. Handle transparent materials\. Add per\-pixel linked lists \(optional\)

Transparency order\-correct

11\.5

Async Loading

Implement texture streaming\. Add placeholder textures\. Create loading queue

Textures stream in smoothly

11\.6

Final Integration

Combine all systems\. Optimize hot paths\. Profile and tune\. Create showcase demo

Full engine runs >60 FPS

## Git Milestone: v1\.0\-complete\-engine

- GPU\-driven rendering
- Complete culling system
- OIT transparency
- Async resource loading
- Production\-ready demo

# Testing Strategy

## Verification Methods

- Visual Comparison: Compare renders against book screenshots
- Validation Layers: Zero Vulkan validation errors
- Performance: FPS targets for each milestone
- Functionality: Each feature works as described in book

## Test Scenes

__Scene__

__Use Case__

__Triangle Count__

Rubber Duck

Basic rendering, textures

~33K

Damaged Helmet

PBR materials, IBL

~15K

Sponza

Medium complexity, lighting

~262K

Amazon Bistro

Large scene, optimization

~3M

Animated Character

Skeletal animation

~50K

# Recommended Workflow

## Session Structure \(2 hours\)

- 0:00\-0:15 \- Review previous session, plan today's goals
- 0:15\-0:30 \- Read relevant book sections
- 0:30\-1:30 \- Implementation \(code along with book\)
- 1:30\-1:45 \- Testing and debugging
- 1:45\-2:00 \- Git commit, document progress

## Git Conventions

- Commit after each working feature
- Use semantic versioning for milestones
- Tag each chapter completion
- Keep README updated with current capabilities

## Resources

- Book GitHub: github\.com/PacktPublishing/3D\-Graphics\-Rendering\-Cookbook\-Second\-Edition
- LightweightVK: github\.com/corporateshark/lightweightvk
- Vulkan Tutorial: vulkan\-tutorial\.com
- glTF Reference: registry\.khronos\.org/glTF/specs/2\.0/glTF\-2\.0\.html

*Note: Session estimates assume familiarity with C\+\+ and basic graphics concepts\. Adjust timing based on your experience level\. Don't hesitate to spend extra sessions on complex topics like PBR or GPU culling\.*

