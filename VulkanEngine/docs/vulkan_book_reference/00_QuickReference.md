# Vulkan 3D Graphics Rendering Cookbook - Quick Reference

## Book Structure Overview

| Chapter | Topic | Key Classes/Concepts |
|---------|-------|---------------------|
| 1 | Build Environment | CMake, GLFW, GLSLang, Taskflow |
| 2 | Vulkan Basics | VulkanContext, Swapchain, Pipelines |
| 3 | Vulkan Objects | Buffers, Textures, Descriptor Indexing |
| 4 | Dev Tools | ImGui, Tracy, Camera, Cube Maps |
| 5 | Geometry | LOD, Instancing, Compute, Tessellation |
| 6 | PBR (glTF) | BRDF, IBL, Metallic-Roughness |
| 7 | PBR Extensions | Clearcoat, Sheen, Transmission, Volume |
| 8 | Rendering Pipeline | Scene Graph, Materials, Large Scenes |
| 9 | Animations | Skeletal, Morph Targets, Blending |
| 10 | Image Techniques | Shadows, SSAO, HDR, MSAA |
| 11 | Optimization | GPU Culling, OIT, Async Loading |

## LightweightVK Core API

### Context & Resources
```cpp
// Create context
std::unique_ptr<lvk::IContext> ctx = lvk::createVulkanContextWithSwapchain(window, w, h, {});

// Create buffer
lvk::Holder<lvk::BufferHandle> buf = ctx->createBuffer({...});

// Create texture  
lvk::Holder<lvk::TextureHandle> tex = ctx->createTexture({...});

// Create pipeline
lvk::Holder<lvk::RenderPipelineHandle> pipe = ctx->createRenderPipeline({...});

// Load shader
lvk::Holder<lvk::ShaderModuleHandle> shader = loadShaderModule(ctx, "shader.vert");
```

### Rendering Loop
```cpp
while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    
    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    
    buf.cmdBeginRendering(renderPass, framebuffer);
    buf.cmdBindRenderPipeline(pipeline);
    buf.cmdPushConstants(pushConstants);
    buf.cmdBindVertexBuffer(0, vertexBuffer);
    buf.cmdBindIndexBuffer(indexBuffer, lvk::IndexFormat_UI32);
    buf.cmdDrawIndexed(indexCount);
    buf.cmdEndRendering();
    
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
}
```

## Common Shader Patterns

### Push Constants
```glsl
layout(push_constant) uniform PC {
    mat4 mvp;
    uint textureId;
    uint64_t bufferAddress;
};
```

### Bindless Textures
```glsl
#extension GL_EXT_nonuniform_qualifier : require
layout(set = 0, binding = 0) uniform sampler2D textures[];

vec4 color = texture(textures[nonuniformEXT(textureId)], uv);
```

### Buffer Device Address
```glsl
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

VertexBuffer vb = VertexBuffer(bufferAddress);
Vertex v = vb.vertices[gl_VertexIndex];
```

### Programmable Vertex Pulling
```glsl
// No vertex input - fetch from storage buffer
void main() {
    Vertex v = vertices[gl_VertexIndex];
    gl_Position = mvp * vec4(v.position, 1.0);
}
```

## Key Data Structures

### Vertex Format (VtxData.h)
```cpp
struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec4 tangent;  // w = handedness
};
```

### Material (glTF PBR)
```cpp
struct Material {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    uint32_t baseColorTexture;
    uint32_t metallicRoughnessTexture;
    uint32_t normalTexture;
    uint32_t occlusionTexture;
    uint32_t emissiveTexture;
    // Extensions...
};
```

### Scene Node
```cpp
struct SceneNode {
    mat4 localTransform;
    int32_t parent;
    int32_t firstChild;
    int32_t nextSibling;
    int32_t mesh;
    int32_t material;
};
```

## File Locations in Repository

```
3D-Graphics-Rendering-Cookbook-Second-Edition/
├── CMake/
│   └── CommonMacros.txt       # CMake helpers
├── data/
│   ├── shaders/               # GLSL shaders
│   │   ├── Bindless.h         # Bindless macros
│   │   ├── Grid.vert/frag     # Infinite grid
│   │   └── gltf/              # PBR shaders
│   └── [models/textures]
├── deps/
│   └── src/
│       └── lightweightvk/     # LVK source
├── shared/
│   ├── VulkanApp.h            # App wrapper
│   ├── Camera.h               # Camera classes
│   ├── LineCanvas.h           # Debug drawing
│   ├── UtilsGLTF.cpp          # glTF loader
│   └── Scene/
│       ├── VtxData.h          # Mesh format
│       └── Scene.h            # Scene graph
└── Chapter01-11/              # Demo source
```

## Build Commands

### Windows
```bash
cd .build
cmake .. -G "Visual Studio 17 2022" -A x64
start RenderingCookbook2.sln
# Or: cmake --build . --config Release
```

### Linux
```bash
cd .build
cmake ..
cmake --build . -j$(nproc)
```

### Run Demos
```bash
# MUST run from repo root (where data/ folder is)
cd 3D-Graphics-Rendering-Cookbook-Second-Edition
.build/Debug/Ch02_Sample02_HelloTriangle.exe
```

## Important URLs

- Book repo: https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition
- LightweightVK: https://github.com/corporateshark/lightweightvk
- Vulkan SDK: https://vulkan.lunarg.com
- glTF spec: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

## Common Issues & Solutions

| Issue | Solution |
|-------|----------|
| Black screen | Check validation layers for errors |
| Validation errors about layout | Ensure proper image transitions |
| Crash on resize | Handle zero-size framebuffer |
| Textures look wrong | Check sRGB vs linear format |
| Slow loading | Use device-local buffers with staging |
| Shader compile fail | Check GLSL version (#version 460) |

## Performance Tips

1. Use **bindless descriptors** - avoid per-draw binding overhead
2. Use **indirect rendering** - reduce CPU-GPU sync
3. Use **device-local memory** - staging is one-time cost
4. Use **compute for culling** - GPU-driven rendering
5. Use **async texture loading** - don't block rendering
6. Use **BC7 compression** - reduce memory bandwidth
