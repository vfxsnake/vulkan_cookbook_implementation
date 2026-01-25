# Chapter 11: Advanced Rendering Techniques and Optimizations

In this chapter, we'll scratch the surface of more advanced rendering topics and optimizations. We'll also show how to combine multiple effects and techniques into a single graphical application.

## Chapter Contents

- Refactoring indirect rendering
- Doing frustum culling on the CPU
- Doing frustum culling on the GPU with compute shaders
- Implementing shadows for directional lights
- Implementing order-independent transparency
- Loading texture assets asynchronously
- Putting it all together into a Vulkan demo

## Technical Requirements

To run the examples in this chapter, you will need a computer with a graphics card supporting Vulkan 1.3. Read Chapter 1 for guidance on how to build demo applications from this book.

This chapter depends on the geometry loading code discussed in Chapter 8, and the post-processing effects covered in the previous chapter, Chapter 10. Be sure to review both chapters before continuing.

Source code: https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition/tree/main/Chapter11

---

## 11.1 Refactoring Indirect Rendering

### Overview

In Chapter 8, we introduced the `VKMesh` helper class, which provided a straightforward way to encapsulate scene geometry, GPU buffers, and rendering pipelines. However, its simplicity comes with a trade-off: it tightly couples scene data, rendering pipelines, and buffers.

This strong coupling poses challenges when attempting to render the same scene data with different pipelines or when selectively rendering parts of a scene. The new `VKMesh11` class is designed to resolve these issues and enable more flexible rendering setups.

### VKIndirectBuffer11 Class

A new helper class to manage an indirect commands buffer along with an array of `DrawIndexedIndirectCommand` structures:

```cpp
class VKIndirectBuffer11 final {
public:
    const std::unique_ptr<lvk::IContext>& ctx_;
    Holder<BufferHandle> bufferIndirect_;
    vector<DrawIndexedIndirectCommand> drawCommands_;
    
    VKIndirectBuffer11(
        const std::unique_ptr<lvk::IContext>& ctx,
        size_t maxDrawCommands,
        lvk::StorageType indirectBufferStorage = lvk::StorageType_Device)
        : ctx_(ctx)
        , drawCommands_(maxDrawCommands) {
        
        bufferIndirect_ = ctx->createBuffer({
            .usage = lvk::BufferUsageBits_Indirect | lvk::BufferUsageBits_Storage,
            .storage = indirectBufferStorage,
            .size = sizeof(uint32_t) + sizeof(DrawIndexedIndirectCommand) * maxDrawCommands,
            .debugName = "Buffer: indirect"
        });
    }
};
```

**Indirect Buffer Layout:**
```
uint32_t numCommands;
DrawIndexedIndirectCommand cmd0;
DrawIndexedIndirectCommand cmd1;
...
DrawIndexedIndirectCommand cmdMaxDrawCommands;
```

### Key Methods

**Upload Indirect Buffer:**
```cpp
void uploadIndirectBuffer() {
    const uint32_t numCommands = drawCommands_.size();
    ctx_->upload(bufferIndirect_, &numCommands, sizeof(uint32_t));
    ctx_->upload(bufferIndirect_,
        drawCommands_.data(),
        sizeof(VkDrawIndexedIndirectCommand) * numCommands, sizeof(uint32_t));
}
```

**Select To (Filter Draw Commands):**
```cpp
void selectTo(VKIndirectBuffer11& buf,
    const std::function<bool(const DrawIndexedIndirectCommand&)>& pred) {
    buf.drawCommands_.clear();
    for (const auto& c : drawCommands_)
        if (pred(c)) buf.drawCommands_.push_back(c);
    buf.uploadIndirectBuffer();
}
```

### VKPipeline11 Class

Encapsulates graphics pipeline creation:

```cpp
class VKPipeline11 final {
public:
    Holder<ShaderModuleHandle> vert_;
    Holder<ShaderModuleHandle> frag_;
    Holder<RenderPipelineHandle> pipeline_;
    Holder<RenderPipelineHandle> pipelineWireframe_;
    
    VKPipeline11(
        const std::unique_ptr<lvk::IContext>& ctx,
        const lvk::VertexInput& streams,
        lvk::Format colorFormat,
        lvk::Format depthFormat,
        uint32_t numSamples = 1,
        Holder<ShaderModuleHandle>&& vert = {},
        Holder<ShaderModuleHandle>&& frag = {});
};
```

### VKMesh11 Class

The refactored mesh class with decoupled pipelines:

```cpp
class VKMesh11 {
public:
    const std::unique_ptr<lvk::IContext>& ctx;
    uint32_t numIndices_ = 0;
    uint32_t numMeshes_ = 0;
    Holder<BufferHandle> bufferIndices_;
    Holder<BufferHandle> bufferVertices_;
    Holder<BufferHandle> bufferTransforms_;
    Holder<BufferHandle> bufferDrawData_;
    Holder<BufferHandle> bufferMaterials_;
    std::vector<DrawData> drawData_;
    VKIndirectBuffer11 indirectBuffer_;
    TextureFiles textureFiles_;
    mutable TextureCache textureCache_;
    std::vector<Material> materialsCPU_;
    std::vector<GLTFMaterialDataGPU> materialsGPU_;
};
```

**Draw Function with Custom Pipeline:**
```cpp
void draw(
    lvk::ICommandBuffer& buf,
    const VKPipeline11& pipeline,
    const mat4& view, const mat4& proj,
    TextureHandle texSkyboxIrradiance = {},
    bool wireframe = false,
    const VKIndirectBuffer11* indirectBuffer = nullptr);
```

**Draw Function with Custom Push Constants:**
```cpp
void draw(
    lvk::ICommandBuffer& buf,
    const VKPipeline11& pipeline,
    const void* pushConstants, size_t pcSize,
    const lvk::DepthState depthState = {
        .compareOp = lvk::CompareOp_Less,
        .isDepthWriteEnabled = true },
    bool wireframe = false,
    const VKIndirectBuffer11* indirectBuffer = nullptr);
```

### Source Code Location
`Chapter11/VKMesh11.h`

---

## 11.2 Doing Frustum Culling on the CPU

### Overview

Frustum culling determines whether a part of the scene is visible within the viewing frustum. Many implementations have a significant drawback: they only check if a mesh's AABB is outside the 6 frustum planes, resulting in false positives for large AABBs.

The solution is to incorporate a reverse culling test, where the 8 corner points of the viewing frustum are checked against the 6 planes of the AABB.

### Extracting Frustum Planes

```cpp
void getFrustumPlanes(mat4 vp, vec4* planes) {
    vp = glm::transpose(vp);
    planes[0] = vec4(vp[3] + vp[0]); // left
    planes[1] = vec4(vp[3] - vp[0]); // right
    planes[2] = vec4(vp[3] + vp[1]); // bottom
    planes[3] = vec4(vp[3] - vp[1]); // top
    planes[4] = vec4(vp[3] + vp[2]); // near
    planes[5] = vec4(vp[3] - vp[2]); // far
}
```

### Extracting Frustum Corners

```cpp
void getFrustumCorners(glm::mat4 vp, glm::vec4* points) {
    const vec4 corners[] = {
        vec4(-1, -1, -1, 1), vec4( 1, -1, -1, 1),
        vec4( 1,  1, -1, 1), vec4(-1,  1, -1, 1),
        vec4(-1, -1,  1, 1), vec4( 1, -1,  1, 1),
        vec4( 1,  1,  1, 1), vec4(-1,  1,  1, 1)
    };
    const glm::mat4 invVP = glm::inverse(vp);
    for (int i = 0; i != 8; i++) {
        const vec4 q = invVP * corners[i];
        points[i] = q / q.w;
    }
}
```

### AABB-Frustum Test

```cpp
bool isBoxInFrustum(glm::vec4* frPlanes, glm::vec4* frCorners, const BoundingBox& b) {
    using glm::dot;
    
    // Phase 1: Check if box is entirely outside any frustum plane
    for (int i = 0; i < 6; i++) {
        int r = 0;
        r += (dot(frPlanes[i], vec4(b.min_.x, b.min_.y, b.min_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.max_.x, b.min_.y, b.min_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.min_.x, b.max_.y, b.min_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.max_.x, b.max_.y, b.min_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.min_.x, b.min_.y, b.max_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.max_.x, b.min_.y, b.max_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.min_.x, b.max_.y, b.max_.z, 1.f)) < 0) ? 1 : 0;
        r += (dot(frPlanes[i], vec4(b.max_.x, b.max_.y, b.max_.z, 1.f)) < 0) ? 1 : 0;
        if (r == 8) return false;
    }
    
    // Phase 2: Reverse test - check if frustum is entirely outside any box plane
    int r = 0;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].x > b.max_.x) ? 1 : 0);
    if (r == 8) return false;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].x < b.min_.x) ? 1 : 0);
    if (r == 8) return false;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].y > b.max_.y) ? 1 : 0);
    if (r == 8) return false;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].y < b.min_.y) ? 1 : 0);
    if (r == 8) return false;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].z > b.max_.z) ? 1 : 0);
    if (r == 8) return false;
    r = 0; for (int i = 0; i < 8; i++) r += ((frCorners[i].z < b.min_.z) ? 1 : 0);
    if (r == 8) return false;
    
    return true;
}
```

### Usage in Rendering Loop

```cpp
// Create mesh with host-visible indirect buffer
const VKMesh11 mesh(ctx, meshData, scene, lvk::StorageType_HostVisible);

// In render loop:
if (!freezeCullingView)
    cullingView = app.camera_.getViewMatrix();

vec4 frustumPlanes[6];
getFrustumPlanes(proj * cullingView, frustumPlanes);
vec4 frustumCorners[8];
getFrustumCorners(proj * cullingView, frustumCorners);

// Perform culling
int numVisibleMeshes = 0;
DrawIndexedIndirectCommand* cmd = mesh.getDrawIndexedIndirectCommandPtr();
for (auto& p : scene.meshForNode) {
    const BoundingBox box = meshData.boxes[p.second]
        .getTransformed(scene.globalTransform[p.first]);
    const uint32_t count = isBoxInFrustum(frustumPlanes, frustumCorners, box) ? 1 : 0;
    (cmd++)->instanceCount = count;
    numVisibleMeshes += count;
}

// Flush to GPU
ctx->flushMappedMemory(mesh.indirectBuffer_.bufferIndirect_, 0,
    mesh.numMeshes_ * sizeof(DrawIndexedIndirectCommand));
```

### Source Code Location
`Chapter11/01_CullingCPU`

### Additional Notes

- CPU culling is useful for large objects or clusters of objects
- On mobile/handheld devices, CPU culling can prevent costly transfer of vertex data
- Can be parallelized using multithreading (parallel-brute-force approach)

---

## 11.3 Doing Frustum Culling on the GPU with Compute Shaders

### Overview

Port the CPU frustum culling to a GLSL compute shader for GPU-based culling.

### GLSL Structures

```glsl
struct AABB {
    float pt[6];  // min xyz, max xyz
};

struct DrawIndexedIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int baseVertex;
    uint baseInstance;
};

struct DrawData {
    uint transformId;
    uint materialId;
};
```

### Buffer Declarations

```glsl
layout(std430, buffer_reference) readonly buffer BoundingBoxes {
    AABB boxes[];
};

layout(std430, buffer_reference) readonly buffer DrawDataBuffer {
    DrawData dd[];
};

layout(std430, buffer_reference) buffer DrawCommands {
    uint numDraws;
    DrawIndexedIndirectCommand dc[];
};

layout(std430, buffer_reference) buffer CullingData {
    vec4 planes[6];
    vec4 corners[8];
    uint numShapesToCull;
    uint numVisibleMeshes;
};

layout(std430, push_constant) uniform PushConstants {
    DrawCommands commands;
    DrawDataBuffer drawData;
    BoundingBoxes AABBs;
    CullingData frustum;
};
```

### Compute Shader Main Function

```glsl
#define min_x box.pt[0]
#define min_y box.pt[1]
#define min_z box.pt[2]
#define max_x box.pt[3]
#define max_y box.pt[4]
#define max_z box.pt[5]

bool isAABBinFrustum(AABB box) {
    // Same logic as CPU version
    // ...
}

void main() {
    const uint idx = gl_GlobalInvocationID.x;
    if (idx < frustum.numShapesToCull) {
        uint baseInstance = commands.dc[idx].baseInstance;
        AABB box = AABBs.boxes[drawData.dd[baseInstance].transformId];
        uint numInstances = isAABBinFrustum(box) ? 1 : 0;
        commands.dc[idx].instanceCount = numInstances;
        atomicAdd(frustum.numVisibleMeshes, numInstances);
    }
}
```

### C++ Setup

```cpp
enum CullingMode {
    CullingMode_None = 0,
    CullingMode_CPU = 1,
    CullingMode_GPU = 2,
};

struct CullingData {
    vec4 frustumPlanes[6];
    vec4 frustumCorners[8];
    uint32_t numMeshesToCull = 0;
    uint32_t numVisibleMeshes = 0;
};

// Pre-transform bounding boxes (since scene is static)
std::vector<BoundingBox> reorderedBoxes;
reorderedBoxes.resize(scene.globalTransform.size());
for (auto& p : scene.meshForNode)
    reorderedBoxes[p.first] = meshData.boxes[p.second]
        .getTransformed(scene.globalTransform[p.first]);

// Create buffers for round-robin access
Holder<BufferHandle> bufferCullingData[] = {
    ctx->createBuffer(cullingDataDesc),
    ctx->createBuffer(cullingDataDesc),
};
```

### Dispatching Compute Shader

```cpp
if (cullingMode == CullingMode_GPU) {
    buf.cmdBindComputePipeline(pipelineCulling);
    pcCulling.meshes = ctx->gpuAddress(bufferCullingData[bufferId]);
    buf.cmdPushConstants(pcCulling);
    buf.cmdUpdateBuffer(bufferCullingData[bufferId], cullingData);
    buf.cmdDispatchThreadGroups(
        { 1 + cullingData.numMeshesToCull / 64 },
        { .buffers = { BufferHandle(mesh.bufferIndirect_) } });
}
```

### Reading Back Results

```cpp
if (cullingMode == CullingMode_GPU && app.fpsCounter_.numFrames_ > 1) {
    ctx->wait(submitHandle[currentBufferId]);
    ctx->download(bufferCullingData[currentBufferId],
        &numVisibleMeshes, sizeof(uint32_t),
        offsetof(CullingData, numVisibleMeshes));
}
```

### Source Code Location
`Chapter11/02_CullingGPU`

### Optimization Notes

- Use subgroup operations (`subgroupBallot()`, `subgroupBallotBitCount()`) for efficient atomic counting
- Consider bounding sphere culling for simpler, branchless dot product calculations
- Setting instanceCount to 0 doesn't completely eliminate GPU cost; consider buffer compaction

### References
- GDC 2011: "Culling the Battlefield" by Daniel Collin
- SIGGRAPH 2015: "GPU-Driven Rendering Pipelines" by Ulrich Haar and Sebastian Aaltonen

---

## 11.4 Implementing Shadows for Directional Lights

### Overview

Implementing shadows for directional lights (simulating sunlight) requires constructing a projection matrix that accounts for the scene's bounds using an orthographic frustum.

### Light Direction Setup

```cpp
struct LightParams {
    float theta = +90.0f;
    float phi = -26.0f;
    float depthBiasConst = 1.1f;
    float depthBiasSlope = 2.0f;
    bool operator==(const LightParams&) const = default;
} light;
```

### Constructing Light View Matrix

```cpp
const mat4 rot1 = glm::rotate(mat4(1.f), glm::radians(light.theta), vec3(0, 1, 0));
const mat4 rot2 = glm::rotate(rot1, glm::radians(light.phi), vec3(1, 0, 0));
const vec3 lightDir = glm::normalize(vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)));
const mat4 lightView = glm::lookAt(vec3(0, 0, 0), lightDir, vec3(0, 0, 1));
```

### Scene Bounding Box Calculation

```cpp
// Pre-transform bounding boxes
std::vector<BoundingBox> reorderedBoxes;
reorderedBoxes.resize(scene.globalTransform.size());
for (auto& p : scene.meshForNode)
    reorderedBoxes[p.first] = meshData.boxes[p.second]
        .getTransformed(scene.globalTransform[p.first]);

// Combine into scene AABB
BoundingBox bigBoxWS = reorderedBoxes.front();
for (const auto& b : reorderedBoxes) {
    bigBoxWS.combinePoint(b.min_);
    bigBoxWS.combinePoint(b.max_);
}
```

### Light Projection Matrix

```cpp
// Transform scene AABB to light space
const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);

// Create orthographic projection (note: flip z for Vulkan)
const mat4 lightProj = glm::orthoLH_ZO(
    boxLS.min_.x, boxLS.max_.x,
    boxLS.min_.y, boxLS.max_.y,
    boxLS.max_.z, boxLS.min_.z);
```

### Light Data Buffer

```cpp
struct LightData {
    mat4 viewProjBias;
    vec4 lightDir;
    uint32_t shadowTexture;
    uint32_t shadowSampler;
};

Holder<BufferHandle> bufferLight = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Storage,
    .storage = lvk::StorageType_Device,
    .size = sizeof(LightData),
    .debugName = "Buffer: light"
});
```

### Rendering Shadow Map

```cpp
const VKPipeline11 pipelineShadow(
    ctx, meshData.streams, lvk::Format_Invalid,
    ctx->getFormat(shadowMap), 1,
    loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.vert"),
    loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.frag"));

// Only update shadow map when light parameters change
if (prevLight != light) {
    prevLight = light;
    // Render scene into shadow map
    mesh.draw(buf, pipelineShadow, lightView, lightProj);
    
    buf.cmdUpdateBuffer(bufferLight, LightData{
        .viewProjBias = scaleBias * lightProj * lightView,
        .lightDir = vec4(lightDir, 0.0f),
        .shadowTexture = shadowMap.index(),
        .shadowSampler = samplerShadow.index()
    });
}
```

### Source Code Location
`Chapter11/03_DirectionalShadows`

### Advanced Shadow Techniques
- **Perspective Shadow Maps (PSMs)**: Objects near camera occupy more shadow map space
- **Light Space Perspective Shadow Maps**: Further refinement for better quality
- **Cascaded Shadow Maps (CSMs)**: Multiple shadow maps for different depth ranges
- **Parallel Split Shadow Maps (PSSMs)**: Variation with evenly distributed splits

### Reference
"Real-Time Shadows" book: https://www.realtimeshadows.com

---

## 11.5 Implementing Order-Independent Transparency

### Overview

Order-independent transparency (OIT) using per-pixel linked lists eliminates the need to sort transparent geometry before rendering. The algorithm:

1. Construct a linked list of fragments for each screen pixel
2. Store color and depth values in each node
3. Sort and blend the lists using a fullscreen fragment shader

### Data Structures

```cpp
struct TransparentFragment {
    uint64_t rgba;  // f16vec4 half-float in GLSL
    float depth;
    uint32_t next;
};
```

### Buffer Setup

```cpp
// Fragment storage buffer
const uint32_t kMaxOITFragments = sizeFb.width * sizeFb.height * 4; // 4 overdraw layers
Holder<BufferHandle> bufferTransparencyLists = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Storage,
    .storage = lvk::StorageType_Device,
    .size = sizeof(TransparentFragment) * kMaxOITFragments,
    .debugName = "Buffer: transparency lists"
});

// Per-pixel head indices
Holder<TextureHandle> textureHeadsOIT = ctx->createTexture({
    .format = lvk::Format_R_UI32,
    .dimensions = sizeFb,
    .usage = lvk::TextureUsageBits_Storage,
    .debugName = "oitHeads"
});

// Atomic counter for allocation
Holder<BufferHandle> bufferAtomicCounter = ctx->createBuffer({
    .usage = lvk::BufferUsageBits_Storage,
    .storage = lvk::StorageType_Device,
    .size = sizeof(uint32_t),
    .debugName = "Buffer: atomic counter"
});
```

### Clear Buffers Each Frame

```cpp
auto clearTransparencyBuffers = [&](lvk::ICommandBuffer& buf) {
    buf.cmdClearColorImage(textureHeadsOIT, { .uint32 = { 0xffffffff } });
    buf.cmdFillBuffer(bufferAtomicCounter, 0, sizeof(uint32_t), 0);
};
```

### Separating Opaque and Transparent Meshes

```cpp
VKIndirectBuffer11 meshesOpaque(ctx, mesh.numMeshes_);
VKIndirectBuffer11 meshesTransparent(ctx, mesh.numMeshes_);

auto isTransparent = [&](const DrawIndexedIndirectCommand& c) -> bool {
    const uint32_t mtlIndex = mesh.drawData_[c.baseInstance].materialId;
    const Material& mtl = meshData.materials[mtlIndex];
    return (mtl.flags & sMaterialFlags_Transparent) > 0;
};

mesh.indirectBuffer_.selectTo(meshesOpaque, [&](const DrawIndexedIndirectCommand& c) {
    return !isTransparent(c);
});
mesh.indirectBuffer_.selectTo(meshesTransparent, [&](const DrawIndexedIndirectCommand& c) {
    return isTransparent(c);
});
```

### Transparent Fragment Shader (Key Parts)

```glsl
#include <Chapter11/04_OIT/src/common_oit.sp>
layout (early_fragment_tests) in;
layout (set = 0, binding = 2, r32ui) uniform uimage2D kTextures2DInOut[];

void main() {
    // ... shade fragment ...
    
    float alpha = clamp(baseColor.a * mat.clearcoatTransmissionThickness.z, 0.0, 1.0);
    bool isTransparent = (alpha > 0.01) && (alpha < 0.99);
    
    // Only store for MSAA samples fully covered by this invocation
    if (isTransparent &&
        !gl_HelperInvocation &&
        gl_SampleMaskIn[0] == (1 << gl_SampleID)) {
        
        // Allocate fragment
        uint index = atomicAdd(pc.atomicCounter.numFragments, 1);
        
        if (index < pc.maxOITFragments) {
            // Get old head and replace with new
            uint prevIndex = imageAtomicExchange(
                kTextures2DInOut[pc.texHeadsOIT], ivec2(gl_FragCoord.xy), index);
            
            TransparentFragment frag;
            frag.color = f16vec4(color, alpha);
            frag.depth = gl_FragCoord.z;
            frag.next = prevIndex;
            pc.oitLists.frags[index] = frag;
        }
    }
}
```

### OIT Compositing Shader

```glsl
#define MAX_FRAGMENTS 64

void main() {
    TransparentFragment frags[MAX_FRAGMENTS];
    uint numFragments = 0;
    
    // Extract linked list into array
    uint idx = imageLoad(kTextures2DIn[pc.texHeadsOIT], ivec2(gl_FragCoord.xy)).r;
    while (idx != 0xFFFFFFFF && numFragments < MAX_FRAGMENTS) {
        frags[numFragments] = pc.oitLists.frags[idx];
        numFragments++;
        idx = pc.oitLists.frags[idx].next;
    }
    
    // Insertion sort by depth (back to front)
    for (int i = 1; i < numFragments; i++) {
        TransparentFragment toInsert = frags[i];
        uint j = i;
        while (j > 0 && toInsert.depth > frags[j-1].depth) {
            frags[j] = frags[j-1];
            j--;
        }
        frags[j] = toInsert;
    }
    
    // Blend fragments
    vec4 color = textureBindless2D(pc.texColor, 0, uv);
    for (uint i = 0; i < numFragments; i++) {
        color = mix(color, vec4(frags[i].color),
            clamp(float(frags[i].color.a + pc.opacityBoost), 0.0, 1.0));
    }
    
    out_FragColor = color;
}
```

### Rendering Depth States

```cpp
// Opaque: write to depth buffer
if (drawMeshesOpaque)
    mesh.draw(buf, pipelineOpaque, &pc, sizeof(pc),
        { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true },
        drawWireframe, &meshesOpaque);

// Transparent: read depth but don't write
if (drawMeshesTransparent)
    mesh.draw(buf, pipelineTransparent, &pc, sizeof(pc),
        { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false },
        drawWireframe, &meshesTransparent);
```

### Source Code Location
`Chapter11/04_OIT`

### Memory Considerations
- At 4K resolution with 4 overdraw layers: ~225MB
- Consider using packed 10-bit formats for color to save bandwidth
- For mobile: look into Per-Fragment Layered Sorting (PLS) and Layered Depth Images (LDI)

---

## 11.6 Loading Texture Assets Asynchronously

### Overview

Implement lazy-loading/streaming of textures using the Taskflow library and C++20 capabilities. The `VKMesh11Lazy` class loads textures asynchronously while the application continues rendering.

### LoadedTextureData Structure

```cpp
struct LoadedTextureData {
    uint32_t index = 0;
    ktxTexture1* ktxTex = nullptr;
    lvk::TextureDesc desc;
};
```

### Load Texture Data Function

```cpp
LoadedTextureData loadTextureData(const char* fileName) {
    ktxTexture1* ktxTex = nullptr;
    ktxTexture1_CreateFromNamedFile(fileName,
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);
    
    const lvk::Format format = [](uint32_t glInternalFormat) {
        switch (glInternalFormat) {
            case GL_COMPRESSED_RGBA_BPTC_UNORM:
                return lvk::Format_BC7_RGBA;
            case GL_RGBA8:
                return lvk::Format_RGBA_UN8;
            // ... other formats
        }
        return lvk::Format_Invalid;
    }(ktxTex->glInternalformat);
    
    return LoadedTextureData{
        .ktxTex = ktxTex,
        .desc = {
            .type = lvk::TextureType_2D,
            .format = format,
            .dimensions = { ktxTex->baseWidth, ktxTex->baseHeight, 1 },
            .usage = lvk::TextureUsageBits_Sampled,
            .numMipLevels = ktxTex->numLevels,
            .data = ktxTex->pData,
            .dataNumMipLevels = ktxTex->numLevels,
            .debugName = fileName
        }
    };
}
```

### VKMesh11Lazy Class

```cpp
class VKMesh11Lazy final : public VKMesh11 {
public:
    std::mutex loadingMutex_;
    std::vector<LoadedTextureData> loadedTextureData_;
    tf::Taskflow taskflow_;
    tf::Executor executor_{ size_t(2) };  // 2 loader threads
    
    VKMesh11Lazy(
        const std::unique_ptr<lvk::IContext>& ctx,
        const MeshData& meshData, const Scene& scene,
        lvk::StorageType indirectBufferStorage = lvk::StorageType_Device)
        : VKMesh11(ctx, meshData, scene, indirectBufferStorage, false) {
        
        materialsGPU_.resize(materialsCPU_.size());
        
        // Set up parallel loading
        taskflow_.for_each_index(0u,
            static_cast<uint32_t>(materialsCPU_.size()), 1u,
            [&](int i) {
                materialsGPU_[i] = convertToGPUMaterialLazy(
                    ctx, materialsCPU_[i], textureFiles_,
                    textureCache_, loadedTextureData_, loadingMutex_);
            });
        
        // Start async execution
        executor_.run(taskflow_);
    }
    
    bool processLoadedTextures() {
        LoadedTextureData tex;
        
        {
            std::lock_guard lock(loadingMutex_);
            if (loadedTextureData_.empty()) return false;
            tex = loadedTextureData_.back();
            loadedTextureData_.pop_back();
        }
        
        // Create texture on main thread
        Holder<TextureHandle> texture = ctx->createTexture(tex.desc);
        ktxTexture_Destroy(ktxTexture(tex.ktxTex));
        
        // Update cache and materials
        std::lock_guard lock(loadingMutex_);
        textureCache_[tex.index] = std::move(texture);
        
        auto getTextureFromCache = [this](int textureId) -> uint32_t {
            return textureCache_.size() > textureId ?
                textureCache_[textureId].index() : 0;
        };
        
        for (size_t i = 0; i != materialsCPU_.size(); i++) {
            const Material& mtl = materialsCPU_[i];
            GLTFMaterialDataGPU& m = materialsGPU_[i];
            m.baseColorTexture = getTextureFromCache(mtl.baseColorTexture);
            m.emissiveTexture = getTextureFromCache(mtl.emissiveTexture);
            m.normalTexture = getTextureFromCache(mtl.normalTexture);
            m.transmissionTexture = getTextureFromCache(mtl.opacityTexture);
        }
        
        ctx->upload(bufferMaterials_,
            materialsGPU_.data(),
            materialsGPU_.size() * sizeof(decltype(materialsGPU_)::value_type));
        return true;
    }
};
```

### Usage

```cpp
// Replace VKMesh11 with VKMesh11Lazy
VKMesh11Lazy mesh(ctx, meshData, scene);

// In render loop
app.run([&](...) {
    mesh.processLoadedTextures();  // Process one texture per frame
    // ... rest of rendering
});
```

### Source Code Location
`Chapter11/05_LazyLoading`

### Improvements to Consider
- Pre-create empty textures and load directly into memory regions
- Implement a "load balancer" for multiple textures per frame based on size
- Integrate texture compression with async loading

---

## 11.7 Putting It All Together into a Vulkan Demo

### Overview

The final demo combines all techniques from Chapters 8, 10, and 11:

- **Multisample anti-aliasing (MSAA)**
- **Screen-space ambient occlusion (SSAO)**
- **HDR rendering with light adaptation**
- **Directional shadow mapping with PCF**
- **GPU frustum culling using compute shaders**
- **Order-independent transparency (OIT)**
- **Asynchronous texture loading**

### Frame Graph Overview

```
[Cull Opaque] → [Shadow Map] → [MSAA Render] → [SSAO Compute] → 
[Combine SSAO] → [OIT Composite] → [HDR/Bloom/Tonemap] → [Swapchain]
```

### Key Configuration Parameters

```cpp
bool drawMeshesOpaque = true;
bool drawMeshesTransparent = true;
bool drawWireframe = false;
bool drawBoxes = false;
bool drawLightFrustum = false;

// SSAO
bool ssaoEnable = true;
bool ssaoEnableBlur = true;
int ssaoNumBlurPasses = 1;
float ssaoDepthThreshold = 30.0f;

// OIT
bool oitShowHeatmap = false;
float oitOpacityBoost = 0.0f;

// HDR
bool hdrDrawCurves = false;
bool hdrEnableBloom = true;
float hdrBloomStrength = 0.01f;
int hdrNumBloomPasses = 2;
float hdrAdaptationSpeed = 1.0f;

// Culling
int cullingMode = CullingMode_CPU;
bool freezeCullingView = false;

// Light
struct LightParams {
    float theta = +90.0f;
    float phi = -26.0f;
    float depthBiasConst = 1.1f;
    float depthBiasSlope = 2.0f;
} light;
```

### Texture Setup

```cpp
const lvk::Format kOffscreenFormat = lvk::Format_RGBA_F16;

// MSAA textures
Holder<TextureHandle> msaaColor = ctx->createTexture({
    .format = kOffscreenFormat,
    .dimensions = sizeFb,
    .numSamples = kNumSamples,
    .usage = lvk::TextureUsageBits_Attachment,
    .storage = lvk::StorageType_Memoryless
});

// Resolved color/depth
Holder<TextureHandle> texOpaqueDepth = ctx->createTexture({
    .format = app.getDepthFormat(),
    .dimensions = sizeFb,
    .usage = lvk::TextureUsageBits_Attachment |
             lvk::TextureUsageBits_Sampled |
             lvk::TextureUsageBits_Storage
});

// Shadow map (16-bit depth, 4096x4096)
Holder<TextureHandle> texShadowMap = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_Z_UN16,
    .dimensions = { 4096, 4096 },
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
    .swizzle = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R,
                 .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 }
});
```

### Pipeline Setup

```cpp
VKMesh11Lazy mesh(ctx, meshData, scene);

const VKPipeline11 pipelineOpaque(ctx, meshData.streams, kOffscreenFormat,
    app.getDepthFormat(), kNumSamples,
    loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/main.vert"),
    loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/opaque.frag"));

const VKPipeline11 pipelineTransparent(ctx, meshData.streams, kOffscreenFormat,
    app.getDepthFormat(), kNumSamples,
    loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/main.vert"),
    loadShaderModule(ctx, "Chapter11/06_FinalDemo/src/transparent.frag"));

const VKPipeline11 pipelineShadow(ctx, meshData.streams, lvk::Format_Invalid,
    ctx->getFormat(texShadowMap), 1,
    loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.vert"),
    loadShaderModule(ctx, "Chapter11/03_DirectionalShadows/src/shadow.frag"));

VKIndirectBuffer11 meshesOpaque(ctx, mesh.numMeshes_, lvk::StorageType_HostVisible);
VKIndirectBuffer11 meshesTransparent(ctx, mesh.numMeshes_, lvk::StorageType_HostVisible);
```

### Render Loop Structure

```cpp
app.run([&](uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds) {
    // 1. Process async texture loading
    mesh.processLoadedTextures();
    
    // 2. Update culling data
    getFrustumPlanes(proj * cullingView, cullingData.frustumPlanes);
    getFrustumCorners(proj * cullingView, cullingData.frustumCorners);
    
    // 3. Update light matrices
    const mat4 lightView = glm::lookAt(vec3(0.0f), lightDir, vec3(0, 0, 1));
    const BoundingBox boxLS = bigBoxWS.getTransformed(lightView);
    const mat4 lightProj = glm::orthoLH_ZO(...);
    
    // 4. Clear OIT buffers
    clearTransparencyBuffers(buf);
    
    // 5. Perform culling (CPU or GPU)
    if (cullingMode == CullingMode_CPU) { /* ... */ }
    else if (cullingMode == CullingMode_GPU) { /* dispatch compute */ }
    
    // 6. Update shadow map if needed
    if (prevLight != light) {
        mesh.draw(buf, pipelineShadow, lightView, lightProj);
        buf.cmdUpdateBuffer(bufferLight, lightData);
    }
    
    // 7. Render scene with MSAA
    skyBox.draw(buf, view, proj);
    mesh.draw(buf, pipelineOpaque, &pc, sizeof(pc),
        { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true },
        drawWireframe, &meshesOpaque);
    mesh.draw(buf, pipelineTransparent, &pc, sizeof(pc),
        { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = false },
        drawWireframe, &meshesTransparent);
    
    // 8. SSAO computation
    if (ssaoEnable) {
        buf.cmdBindComputePipeline(pipelineSSAO);
        buf.cmdDispatchThreadGroups(...);
        // Blur passes...
    }
    
    // 9. Combine SSAO with opaque color
    buf.cmdBindRenderPipeline(pipelineCombineSSAO);
    buf.cmdDraw(3);
    
    // 10. OIT compositing
    buf.cmdBindRenderPipeline(pipelineOIT);
    buf.cmdDraw(3);
    
    // 11. HDR/Bloom/Tonemapping
    // ...
    
    // 12. Submit
    submitHandle[currentBufferId] = ctx->submit(buf, ctx->getCurrentSwapchainTexture());
});
```

### Source Code Location
`Chapter11/06_FinalDemo`

### Future Directions

- **More screen-space effects**: Temporal antialiasing (TAA), screen-space reflections
- **Ray tracing**: Replace SSAO with ray-traced ambient occlusion
- **Multiple light sources**: Store parameters in buffer, iterate in fragment shader
- **Advanced culling**: Tile deferred shading, clustered shading
- **Complex materials**: Convert Bistro materials to full PBR

### References
- Frame Graph Architecture: "FrameGraph: Extensible Rendering Architecture in Frostbite" by Yuriy O'Donnell (GDC)
- "Mastering Graphics Programming with Vulkan" by Marco Castorina and Gabriel Sassone (Packt)
- Filament engine documentation: https://google.github.io/filament/Filament.html
- Clustered shading: http://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf

---

## Session Planning for Chapter 11

### Recommended Session Breakdown (12-16 hours total)

| Session | Topic | Estimated Time |
|---------|-------|----------------|
| 11.1 | Refactoring indirect rendering (VKMesh11, VKPipeline11, VKIndirectBuffer11) | 2 hours |
| 11.2 | CPU frustum culling implementation | 2 hours |
| 11.3 | GPU frustum culling with compute shaders | 2-3 hours |
| 11.4 | Directional light shadows | 2 hours |
| 11.5 | Order-independent transparency | 2-3 hours |
| 11.6 | Asynchronous texture loading | 2 hours |
| 11.7 | Final demo integration | 2-3 hours |

### Key Dependencies
- Chapter 8: Scene graph, material system, indirect rendering
- Chapter 10: SSAO, HDR, tone mapping, shadow maps

### Testing Milestones

1. **Session 11.1**: VKMesh11 renders same output as VKMesh
2. **Session 11.2**: CPU culling shows visible mesh count, frozen frustum visualization works
3. **Session 11.3**: GPU culling matches CPU culling results, performance comparison
4. **Session 11.4**: Shadow map renders correctly, adjust bias parameters
5. **Session 11.5**: Transparent objects blend correctly, heatmap overlay works
6. **Session 11.6**: Scene renders while textures stream in progressively
7. **Session 11.7**: All effects combine correctly, UI controls work
