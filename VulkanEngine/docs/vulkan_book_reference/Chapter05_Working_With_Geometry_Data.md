# Chapter 5: Working with Geometry Data

## Overview

This chapter covers storing and handling mesh geometry data in an organized way. Focus is on practical implementation rather than pure efficiency, covering LOD generation, vertex pulling, instanced rendering, tessellation, indirect rendering, and compute shaders for geometry.

## Technical Requirements

- GPU with up-to-date drivers supporting Vulkan 1.3
- Amazon Lumberyard Bistro dataset from McGuire Computer Graphics Archive
- Source code: https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition/

## Recipes Covered

1. Generating level-of-detail meshes using MeshOptimizer
2. Implementing programmable vertex pulling
3. Rendering instanced geometry
4. Implementing instanced meshes with compute shaders
5. Implementing an infinite grid GLSL shader
6. Integrating tessellation into the graphics pipeline
7. Organizing the mesh data storage
8. Implementing automatic geometry conversion
9. Indirect rendering in Vulkan
10. Generating textures in Vulkan using compute shaders
11. Implementing computed meshes

---

## Recipe 1: LOD Meshes with MeshOptimizer

MeshOptimizer provides algorithms to optimize meshes for modern GPU vertex and index processing pipelines.

### Key Functions

```cpp
// Generate remap table
std::vector<uint32_t> remap(indices.size());
const size_t vertexCount = meshopt_generateVertexRemap(
    remap.data(), indices.data(), indices.size(),
    positions.data(), indices.size(), sizeof(vec3));

// Remap buffers
meshopt_remapIndexBuffer(remappedIndices.data(),
    indices.data(), indices.size(), remap.data());
meshopt_remapVertexBuffer(remappedVertices.data(),
    positions.data(), positions.size(), sizeof(vec3), remap.data());

// Optimize vertex cache
meshopt_optimizeVertexCache(remappedIndices.data(),
    remappedIndices.data(), indices.size(), vertexCount);

// Optimize overdraw
meshopt_optimizeOverdraw(remappedIndices.data(),
    remappedIndices.data(), indices.size(),
    glm::value_ptr(remappedVertices[0]), vertexCount,
    sizeof(vec3), 1.05f);

// Optimize vertex fetch
meshopt_optimizeVertexFetch(remappedVertices.data(),
    remappedIndices.data(), indices.size(),
    remappedVertices.data(), vertexCount, sizeof(vec3));

// Simplify mesh for LOD
const float threshold = 0.2f;
const size_t target_index_count = size_t(remappedIndices.size() * threshold);
const float target_error = 0.01f;
std::vector<uint32_t> indicesLod(remappedIndices.size());
indicesLod.resize(meshopt_simplify(&indicesLod[0],
    remappedIndices.data(), remappedIndices.size(),
    &remappedVertices[0].x, vertexCount, sizeof(vec3),
    target_index_count, target_error));
```

### LOD Rendering

Store one vertex buffer and multiple index buffers (one per LOD):

```cpp
Holder<BufferHandle> vertexBuffer = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Vertex,
    .storage = lvk::StorageType_Device,
    .size = sizeof(vec3) * positions.size(),
    .data = positions.data(),
});

Holder<BufferHandle> indexBufferLOD0 = ctx->createBuffer({...});
Holder<BufferHandle> indexBufferLOD1 = ctx->createBuffer({...});
```

---

## Recipe 2: Programmable Vertex Pulling (PVP)

### Concept

Instead of using fixed-function vertex input, vertices are stored in storage buffers and fetched manually in the vertex shader using `gl_VertexIndex`.

### GLSL Implementation

```glsl
struct Vertex {
    float p[3];  // position
    float n[3];  // normal
    float tc[2]; // texture coordinates
};

layout(std430, buffer_reference) readonly buffer Vertices {
    Vertex vertices[];
};

layout(push_constant) uniform PerFrameData {
    mat4 MVP;
    Vertices vtx;
};

void main() {
    vec3 pos = vec3(vtx.vertices[gl_VertexIndex].p[0],
                    vtx.vertices[gl_VertexIndex].p[1],
                    vtx.vertices[gl_VertexIndex].p[2]);
    gl_Position = MVP * vec4(pos, 1.0);
}
```

### C++ Buffer Creation

```cpp
Holder<BufferHandle> bufferVertices = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Storage,
    .storage = lvk::StorageType_Device,
    .size = sizeof(Vertex) * vertices.size(),
    .data = vertices.data(),
});
```

---

## Recipe 3: Instanced Geometry Rendering

### Rendering 1 Million Cubes

```cpp
const uint32_t kNumCubes = 1024 * 1024;
std::vector<vec4> centers(kNumCubes);
for (vec4& p : centers) {
    p = vec4(glm::linearRand(-vec3(500.0f), +vec3(500.0f)),
             glm::linearRand(0.0f, 3.14159f));
}

Holder<BufferHandle> bufferPosAngle = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Storage,
    .storage = lvk::StorageType_Device,
    .size = sizeof(vec4) * kNumCubes,
    .data = centers.data(),
});

// Render call - 36 vertices per cube, kNumCubes instances
buf.cmdDraw(36, kNumCubes);
```

### GLSL Vertex Shader - Generate Cube in Shader

```glsl
const int indices[36] = int[36](
    0, 2, 1, 2, 3, 1, 5, 4, 1, 1, 4, 0,
    0, 4, 6, 0, 6, 2, 6, 5, 7, 6, 4, 5,
    2, 6, 3, 6, 7, 3, 7, 1, 3, 7, 5, 1);

const vec3 colors[7] = vec3[7](
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0),
    vec3(1.0, 1.0, 0.0), vec3(0.0, 1.0, 1.0), vec3(1.0, 0.0, 1.0),
    vec3(1.0, 1.0, 1.0));

void main() {
    vec4 center = Positions(bufId).pos[gl_InstanceIndex];
    mat4 model = rotate(translate(mat4(1.0f), center.xyz),
                        time + center.w, vec3(1.0f, 1.0f, 1.0f));
    
    uint idx = indices[gl_VertexIndex];
    vec3 xyz = vec3(idx & 1, (idx & 4) >> 2, (idx & 2) >> 1);
    
    const float edge = 1.0;
    gl_Position = viewproj * model * vec4(edge * (xyz - vec3(0.5)), 1.0);
    color = colors[gl_InstanceIndex % 7];
}
```

---

## Recipe 4: Instanced Meshes with Compute Shaders

### Pre-calculate Model Matrices Using Compute Shader

```cpp
const uint32_t kNumMeshes = 32 * 1024;

// Double-buffered matrices for even/odd frames
Holder<BufferHandle> bufferMatrices[] = {
    ctx->createBuffer({
        .usage = lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_Device,
        .size = sizeof(mat4) * kNumMeshes,
    }),
    ctx->createBuffer({...}),
};
```

### Compute Shader

```glsl
layout (local_size_x = 64) in;

layout(push_constant) uniform ComputeData {
    float time;
    uint64_t inBuf;   // positions/angles
    uint64_t outBuf;  // matrices
};

void main() {
    uint index = gl_GlobalInvocationID.x;
    vec4 posAngle = Positions(inBuf).pos[index];
    
    mat4 model = rotate(translate(mat4(1.0), posAngle.xyz),
                        time + posAngle.w, vec3(1,1,1));
    
    Matrices(outBuf).matrices[index] = model;
}
```

### Dispatch and Render

```cpp
// Compute pass
buf.cmdBindComputePipeline(computePipeline);
buf.cmdPushConstants(computePC);
buf.cmdDispatchThreadGroups({kNumMeshes / 64, 1, 1});

// Pipeline barrier
buf.cmdBeginRendering(renderPass, framebuffer, {
    .buffers = {{ bufferMatrices[frameIndex] }}
});

// Render pass
buf.cmdDrawIndexed(numIndices, kNumMeshes);
```

---

## Recipe 5: Infinite Grid GLSL Shader

### Grid Parameters (GridParameters.h)

```glsl
float gridSize = 100.0;
float gridCellSize = 0.025;
vec4 gridColorThin = vec4(0.5, 0.5, 0.5, 1.0);
vec4 gridColorThick = vec4(0.0, 0.0, 0.0, 1.0);
const float gridMinPixelsBetweenCells = 2.0;
```

### Vertex Shader

```glsl
const vec3 pos[4] = vec3[4](
    vec3(-1.0, 0.0, -1.0), vec3(1.0, 0.0, -1.0),
    vec3(1.0, 0.0, 1.0), vec3(-1.0, 0.0, 1.0));
const int indices[6] = int[6](0, 1, 2, 2, 3, 0);

void main() {
    int idx = indices[gl_VertexIndex];
    vec3 position = pos[idx] * gridSize;
    position.x += cameraPos.x;
    position.z += cameraPos.z;
    position += origin.xyz;
    gl_Position = MVP * vec4(position, 1.0);
    uv = position.xz;
}
```

### Fragment Shader - LOD-based Grid Lines

```glsl
vec4 gridColor(vec2 uv, vec2 camPos) {
    vec2 dudv = vec2(length(vec2(dFdx(uv.x), dFdy(uv.x))),
                     length(vec2(dFdx(uv.y), dFdy(uv.y))));
    
    float lodLevel = max(0.0, log10((length(dudv) *
        gridMinPixelsBetweenCells) / gridCellSize) + 1.0);
    float lodFade = fract(lodLevel);
    
    float lod0 = gridCellSize * pow(10.0, floor(lodLevel));
    float lod1 = lod0 * 10.0;
    float lod2 = lod1 * 10.0;
    
    // Calculate alpha for each LOD level
    // Blend colors and apply opacity falloff
    return c;
}
```

---

## Recipe 6: Tessellation in Graphics Pipeline

### Tessellation Stages

1. **Tessellation Control Shader (TCS)** - Works with control points, determines tessellation level
2. **Tessellation Evaluation Shader (TES)** - Uses barycentric coordinates to interpolate attributes

### Vertex Shader

```glsl
layout(std430, buffer_reference) readonly buffer Vertices {
    Vertex in_Vertices[];
};

layout (location=0) out vec2 uv_in;
layout (location=1) out vec3 worldPos_in;

void main() {
    vec4 pos = vec4(getPosition(gl_VertexIndex), 1.0);
    gl_Position = pc.proj * pc.view * pc.model * pos;
    worldPos_in = (pc.model * pos).xyz;
}
```

### Tessellation Control Shader

```glsl
layout (vertices = 3) out;
layout (location=0) in vec2 uv_in[];
layout (location=1) in vec3 worldPos_in[];
layout (location=0) out vec2 uv_out[];
layout (location=1) out vec3 worldPos_out[];

float getTessLevel(float d) {
    float tessLevel = mix(pc.tesselationScale, 1.0, 
        clamp(d / 10.0, 0.0, 1.0));
    return clamp(tessLevel, 1.0, 64.0);
}

void main() {
    uv_out[gl_InvocationID] = uv_in[gl_InvocationID];
    worldPos_out[gl_InvocationID] = worldPos_in[gl_InvocationID];
    
    if (gl_InvocationID == 0) {
        vec3 cp = pc.cameraPos.xyz;
        float d0 = distance(cp, (worldPos_in[1] + worldPos_in[2]) / 2.0);
        // ... calculate distances for other edges
        
        gl_TessLevelOuter[0] = getTessLevel(d0);
        // ... set other tessellation levels
    }
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
```

### Tessellation Evaluation Shader

```glsl
layout (triangles, fractional_odd_spacing, ccw) in;

void main() {
    vec2 tc = gl_TessCoord.x * uv_out[0] + 
              gl_TessCoord.y * uv_out[1] + 
              gl_TessCoord.z * uv_out[2];
    // ... interpolate other attributes
}
```

---

## Recipe 7: Mesh Data Storage Organization

### Mesh Structure

```cpp
constexpr const uint32_t kMaxLODs = 8;

struct Mesh final {
    uint32_t lodCount = 1;
    uint32_t indexOffset = 0;
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t materialID = 0;
    uint32_t lodOffset[kMaxLODs+1] = { 0 };
    
    uint32_t getLODIndicesCount(uint32_t lod) const {
        return lod < lodCount ? lodOffset[lod + 1] - lodOffset[lod] : 0;
    }
};
```

### MeshData Container

```cpp
struct MeshData {
    lvk::VertexInput streams = {};
    std::vector<uint32_t> indexData;
    std::vector<uint8_t> vertexData;
    std::vector<Mesh> meshes;
    std::vector<BoundingBox> boxes;
    std::vector<Material> materials;
    std::vector<std::string> textureFiles;
};
```

### File Header

```cpp
struct MeshFileHeader {
    uint32_t magicValue = 0x12345678;
    uint32_t meshCount = 0;
    uint32_t indexDataSize = 0;
    uint32_t vertexDataSize = 0;
};
```

### Loading Function

```cpp
MeshFileHeader loadMeshData(const char* meshFile, MeshData& out) {
    FILE* f = fopen(meshFile, "rb");
    
    MeshFileHeader header;
    fread(&header, 1, sizeof(header), f);
    fread(&out.streams, 1, sizeof(out.streams), f);
    
    out.meshes.resize(header.meshCount);
    fread(out.meshes.data(), sizeof(Mesh), header.meshCount, f);
    out.boxes.resize(header.meshCount);
    fread(out.boxes.data(), sizeof(BoundingBox), header.meshCount, f);
    
    out.indexData.resize(header.indexDataSize / sizeof(uint32_t));
    out.vertexData.resize(header.vertexDataSize);
    fread(out.indexData.data(), 1, header.indexDataSize, f);
    fread(out.vertexData.data(), 1, header.vertexDataSize, f);
    
    fclose(f);
    return header;
}
```

---

## Recipe 8: Automatic Geometry Conversion

### Convert aiMesh to Runtime Format

```cpp
Mesh convertAIMesh(const aiMesh* m, MeshData& meshData,
                   uint32_t& indexOffset, uint32_t& vertexOffset)
{
    std::vector<uint8_t>& vertices = meshData.vertexData;
    
    for (size_t i = 0; i != m->mNumVertices; i++) {
        const aiVector3D v = m->mVertices[i];
        const aiVector3D n = m->mNormals[i];
        const aiVector2D t = m->mTextureCoords[0][i];
        
        // Store position as vec3
        put(vertices, v);
        // Store UV as half-float vec2 (packed into uint32_t)
        put(vertices, glm::packHalf2x16(vec2(t.x, t.y)));
        // Store normal as 2_10_10_10_REV (packed into uint32_t)
        put(vertices, glm::packSnorm3x10_1x2(vec4(n.x, n.y, n.z, 0)));
    }
    
    // Describe vertex streams
    meshData.streams = {
        .attributes = {
            {.location = 0, .format = lvk::VertexFormat::Float3, .offset = 0},
            {.location = 1, .format = lvk::VertexFormat::HalfFloat2, 
             .offset = sizeof(vec3)},
            {.location = 2, .format = lvk::VertexFormat::Int_2_10_10_10_REV,
             .offset = sizeof(vec3) + sizeof(uint32_t)}
        },
        .inputBindings = {{
            .stride = sizeof(vec3) + sizeof(uint32_t) + sizeof(uint32_t)
        }},
    };
    
    // Process indices and LODs...
    return result;
}
```

---

## Recipe 9: Indirect Rendering in Vulkan

### DrawIndexedIndirectCommand Structure

```cpp
struct DrawIndexedIndirectCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t baseVertex;
    uint32_t baseInstance;
};
```

### VKMesh Class

```cpp
class VKMesh {
    Holder<BufferHandle> bufferIndices_;
    Holder<BufferHandle> bufferVertices_;
    Holder<BufferHandle> bufferIndirect_;
    Holder<RenderPipelineHandle> pipeline_;
    
public:
    VKMesh(const std::unique_ptr<lvk::IContext>& ctx,
           const MeshFileHeader& header,
           const MeshData& meshData,
           lvk::Format depthFormat)
    {
        const uint32_t numCommands = header.meshCount;
        
        // Create indirect commands buffer
        std::vector<uint8_t> drawCommands(
            sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(uint32_t));
        memcpy(drawCommands.data(), &numCommands, sizeof(numCommands));
        
        DrawIndexedIndirectCommand* cmd = /* pointer to commands */;
        for (uint32_t i = 0; i != numCommands; i++) {
            *cmd++ = {
                .count = meshData.meshes[i].getLODIndicesCount(0),
                .instanceCount = 1,
                .firstIndex = meshData.meshes[i].indexOffset,
                .baseVertex = meshData.meshes[i].vertexOffset,
                .baseInstance = 0,
            };
        }
        
        bufferIndirect_ = ctx->createBuffer({
            .usage = lvk::BufferUsageBits_Indirect,
            .storage = lvk::StorageType_Device,
            .size = drawCommands.size(),
            .data = drawCommands.data(),
        });
    }
    
    void draw(lvk::ICommandBuffer& buf, const MeshFileHeader& header) {
        buf.cmdBindIndexBuffer(bufferIndices_, lvk::IndexFormat_UI32);
        buf.cmdBindVertexBuffer(0, bufferVertices_);
        buf.cmdBindRenderPipeline(pipeline_);
        buf.cmdDrawIndexedIndirect(bufferIndirect_, sizeof(uint32_t), 
                                   header.meshCount);
    }
};
```

---

## Recipe 10: Compute Shaders for Texture Generation

### Compute Shader Structure

```glsl
layout (local_size_x = 16, local_size_y = 16) in;
layout (set = 0, binding = 2, rgba8) uniform writeonly image2D kTextures2DOut[];

layout(push_constant) uniform uPushConstant {
    uint tex;
    float time;
} pc;

void main() {
    ivec2 dim = imageSize(kTextures2DOut[pc.tex]);
    vec2 uv = vec2(gl_GlobalInvocationID.xy) / dim;
    
    // Calculate color (e.g., gradient, ShaderToy port, etc.)
    vec4 color = vec4(uv, 0.0, 1.0);
    
    imageStore(kTextures2DOut[pc.tex], ivec2(gl_GlobalInvocationID.xy), color);
}
```

### C++ Usage

```cpp
// Create texture with storage usage
Holder<TextureHandle> texture = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_UN8,
    .dimensions = {1280, 720},
    .usage = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
});

// Dispatch compute shader
buf.cmdBindComputePipeline(computePipeline);
buf.cmdPushConstants(pc);
buf.cmdDispatchThreadGroups({1280 / 16, 720 / 16, 1});

// Render with barrier for texture dependency
buf.cmdBeginRendering(renderPass, framebuffer,
    {.textures = {{TextureHandle(texture)}}});
```

---

## Recipe 11: Computed Meshes (Torus Knot Example)

### Parametric Surface Generation

```glsl
struct VertexData {
    vec4 pos;
    vec4 tc;
    vec4 norm;
};

VertexData torusKnot(vec2 uv, float p, float q) {
    // Mathematical parametrization of torus knot
    const float baseRadius = 10.0;
    const float segmentRadius = 3.0;
    const float tubeRadius = 0.5;
    
    // Calculate position and normal on parametric surface
    VertexData res;
    res.pos = vec4(r + tubeRadius * (v1 * sv + v2 * cv), 1);
    res.norm = vec4(cross(v1 * cv - v2 * sv, drdt), 0);
    return res;
}

void main() {
    uint index = gl_GlobalInvocationID.x;
    vec2 ij = vec2(float(index / pc.numV), float(index % pc.numV));
    
    // Compute two vertex positions for morphing
    VertexData v1 = torusKnot(uv1, pc.P1, pc.Q1);
    VertexData v2 = torusKnot(uv2, pc.P2, pc.Q2);
    
    // Linear blend between them
    vec3 pos = mix(v1.pos.xyz, v2.pos.xyz, pc.morph);
    vec3 norm = mix(v1.norm.xyz, v2.norm.xyz, pc.morph);
    
    // Apply rotation and store
    mat3 modelMatrix = rotY(0.5 * pc.time) * rotZ(0.5 * pc.time);
    VertexData vtx;
    vtx.pos = vec4(modelMatrix * pos, 1);
    vtx.tc = vec4(ij / numUV, 0, 0);
    vtx.norm = vec4(modelMatrix * norm, 0);
    
    VertexBuffer(pc.bufferId).vertices[index] = vtx;
}
```

---

## Source Code Locations

| Recipe | Demo Location |
|--------|---------------|
| MeshOptimizer | Chapter05/01_MeshOptimizer |
| Vertex Pulling | Chapter05/02_VertexPulling |
| Million Cubes | Chapter05/03_MillionCubes |
| Instanced Meshes | Chapter05/04_InstancedMeshes |
| Grid | Chapter05/05_Grid |
| Tessellation | Chapter05/06_Tessellation |
| Mesh Renderer | Chapter05/07_MeshRenderer |
| Compute Texture | Chapter05/08_ComputeTexture |
| Compute Mesh | Chapter05/09_ComputeMesh |

## Key Shared Files

- `shared/Scene/VtxData.h` / `shared/Scene/VtxData.cpp` - Mesh data structures and I/O
- `shared/VulkanApp.h` - Application helper with `drawGrid()` function
- `data/shaders/Grid.vert` / `data/shaders/Grid.frag` - Grid shaders
- `data/shaders/GridParameters.h` - Grid configuration
- `data/shaders/GridCalculation.h` - Grid helper functions
