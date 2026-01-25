# Chapter 3: Working with Vulkan Objects

## Overview
This chapter covers GPU resource management: buffers, staging, textures, and bindless descriptors.

## Buffer Types in Vulkan

| Type | Usage Flag | Purpose |
|------|------------|---------|
| Vertex | `BufferUsageBits_Vertex` | Vertex attributes |
| Index | `BufferUsageBits_Index` | Triangle indices |
| Uniform | `BufferUsageBits_Uniform` | Shader constants |
| Storage | `BufferUsageBits_Storage` | Large read/write data |

## Creating Buffers with LightweightVK

```cpp
// Device-local buffer (GPU only, fastest)
lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Vertex,
    .storage = lvk::StorageType_Device,
    .size = vertexDataSize,
    .data = vertexData,  // Initial data (uses staging internally)
    .debugName = "Vertex Buffer"
});

// Host-visible buffer (CPU accessible)
lvk::Holder<lvk::BufferHandle> uniformBuffer = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Uniform,
    .storage = lvk::StorageType_HostVisible,
    .size = sizeof(UniformData),
    .debugName = "Uniform Buffer"
});
```

## Staging Buffer Pattern

For device-local buffers, data must be uploaded via staging:

```
CPU Memory → Staging Buffer (host-visible) → Device Buffer (device-local)
```

LightweightVK handles this automatically when you provide `data` in `createBuffer()`.

### Manual Staging (Internal LVK Pattern)

```cpp
// 1. Create staging buffer
VkBufferCreateInfo stagingCI = {
    .size = dataSize,
    .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
};
// Allocate with VMA using HOST_VISIBLE | HOST_COHERENT

// 2. Copy data to staging
void* mapped;
vmaMapMemory(allocator, stagingAllocation, &mapped);
memcpy(mapped, srcData, dataSize);
vmaUnmapMemory(allocator, stagingAllocation);

// 3. Copy staging to device buffer
VkBufferCopy copyRegion = { .size = dataSize };
vkCmdCopyBuffer(cmdBuffer, stagingBuffer, deviceBuffer, 1, &copyRegion);

// 4. Memory barrier
VkBufferMemoryBarrier barrier = {
    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    .buffer = deviceBuffer,
    .size = VK_WHOLE_SIZE,
};
vkCmdPipelineBarrier(cmdBuffer,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
    0, 0, nullptr, 1, &barrier, 0, nullptr);
```

## Buffer Device Address (BDA)

Vulkan 1.2+ feature for accessing buffers via 64-bit GPU pointers:

```cpp
// Get GPU address
uint64_t gpuAddress = ctx->gpuAddress(buffer);

// Pass to shader via push constants
struct PushConstants {
    uint64_t vertexBuffer;
    uint64_t indexBuffer;
};

// In GLSL
layout(push_constant) uniform PC {
    uint64_t vertexBufferAddress;
};

// Access via buffer reference
layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

void main() {
    VertexBuffer vb = VertexBuffer(vertexBufferAddress);
    Vertex v = vb.vertices[gl_VertexIndex];
}
```

## Loading Meshes with Assimp

```cpp
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

const aiScene* scene = aiImportFile("model.gltf",
    aiProcess_Triangulate |
    aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_JoinIdenticalVertices);

if (!scene || !scene->mNumMeshes) {
    // Handle error
}

aiMesh* mesh = scene->mMeshes[0];

// Extract vertices
std::vector<Vertex> vertices(mesh->mNumVertices);
for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
    vertices[i].position = {
        mesh->mVertices[i].x,
        mesh->mVertices[i].y,
        mesh->mVertices[i].z
    };
    if (mesh->mNormals) {
        vertices[i].normal = {
            mesh->mNormals[i].x,
            mesh->mNormals[i].y,
            mesh->mNormals[i].z
        };
    }
    if (mesh->mTextureCoords[0]) {
        vertices[i].uv = {
            mesh->mTextureCoords[0][i].x,
            mesh->mTextureCoords[0][i].y
        };
    }
}

// Extract indices
std::vector<uint32_t> indices;
for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
    aiFace& face = mesh->mFaces[i];
    for (uint32_t j = 0; j < face.mNumIndices; j++) {
        indices.push_back(face.mIndices[j]);
    }
}

aiReleaseImport(scene);
```

## Texture Creation

### Creating a 2D Texture

```cpp
lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_UN8,
    .dimensions = { width, height },
    .usage = lvk::TextureUsageBits_Sampled,
    .numMipLevels = lvk::calcNumMipLevels(width, height),
    .data = pixelData,
    .generateMipmaps = true,
    .debugName = "Albedo Texture"
});
```

### Loading with STB

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int width, height, channels;
uint8_t* pixels = stbi_load("texture.png", &width, &height, &channels, 4);

lvk::Holder<lvk::TextureHandle> texture = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_UN8,
    .dimensions = { (uint32_t)width, (uint32_t)height },
    .usage = lvk::TextureUsageBits_Sampled,
    .data = pixels,
    .debugName = "texture.png"
});

stbi_image_free(pixels);
```

### Cube Map Textures

```cpp
// Load 6 faces into a single buffer (arranged vertically)
lvk::Holder<lvk::TextureHandle> cubemap = ctx->createTexture({
    .type = lvk::TextureType_Cube,
    .format = lvk::Format_RGBA_F32,  // HDR
    .dimensions = { faceWidth, faceHeight },
    .usage = lvk::TextureUsageBits_Sampled,
    .data = cubemapData,  // 6 faces stacked
    .debugName = "Environment Cubemap"
});
```

## Image Layout Transitions

Vulkan images have layouts that must match their usage:

| Layout | Usage |
|--------|-------|
| `UNDEFINED` | Initial state |
| `TRANSFER_DST_OPTIMAL` | Copy destination |
| `SHADER_READ_ONLY_OPTIMAL` | Sampling in shaders |
| `COLOR_ATTACHMENT_OPTIMAL` | Render target |
| `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | Depth buffer |

LightweightVK handles most transitions automatically.

## Descriptor Indexing (Bindless)

### Concept
Instead of binding individual textures per draw call, maintain a global array of all textures:

```
Traditional: Bind Texture A → Draw → Bind Texture B → Draw
Bindless:    Bind All Textures Once → Draw (pass texture ID) → Draw (pass texture ID)
```

### Enabling in LightweightVK

LightweightVK enables bindless by default. All textures are automatically added to a global descriptor array.

### Accessing in Shaders

```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : require

// Global texture array (set by LightweightVK)
layout(set = 0, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PC {
    uint textureId;
};

void main() {
    // Access texture by index
    vec4 color = texture(textures[nonuniformEXT(textureId)], uv);
}
```

### LightweightVK Helper Macros

```glsl
// In data/shaders/Bindless.h
#define textureBindless2D(id, lod, uv) \
    textureLod(sampler2D(kTextures2D[id], kSamplers[0]), uv, lod)

// Usage
vec4 color = textureBindless2D(textureId, 0, uv);
```

## VulkanImage Structure (LightweightVK Internal)

```cpp
struct VulkanImage {
    VkImage vkImage_ = VK_NULL_HANDLE;
    VkImageView imageView_ = VK_NULL_HANDLE;
    VkImageView imageViewStorage_ = VK_NULL_HANDLE;  // For compute
    VmaAllocation vmaAllocation_ = VK_NULL_HANDLE;
    
    VkFormat vkFormat_ = VK_FORMAT_UNDEFINED;
    VkImageLayout vkImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent3D vkExtent_ = {0, 0, 0};
    VkImageUsageFlags vkUsageFlags_ = 0;
    
    uint32_t numLevels_ = 1;  // Mip levels
    uint32_t numLayers_ = 1;  // Array layers or cube faces
};
```

## Demo Applications

| Demo | Description |
|------|-------------|
| 01_Assimp | Load and render 3D model |
| 02_STB | Textured mesh rendering |

## Common Texture Formats

| Format | Use Case |
|--------|----------|
| `Format_RGBA_UN8` | Standard color (sRGB) |
| `Format_RGBA_F16` | HDR color |
| `Format_RGBA_F32` | HDR / compute |
| `Format_R_UN8` | Grayscale / masks |
| `Format_Z_F32` | Depth buffer |
| `Format_BC7_RGBA` | Compressed color |

## Memory Management with VMA

LightweightVK uses Vulkan Memory Allocator (VMA):

```cpp
// Allocation types
VMA_MEMORY_USAGE_GPU_ONLY     // Device local (fastest GPU access)
VMA_MEMORY_USAGE_CPU_TO_GPU   // Host visible (CPU upload)
VMA_MEMORY_USAGE_GPU_TO_CPU   // Host visible (GPU readback)
```

## Key Takeaways

1. **Use staging for device-local resources** - Essential for performance
2. **Enable bindless from the start** - Simplifies material system
3. **Buffer device address** - Modern way to access buffers in shaders
4. **VMA handles allocation** - Don't manage Vulkan memory manually
5. **Image layouts matter** - Wrong layout = validation errors or corruption
