# Chapter 9: glTF Animations

In this chapter, we will explore advanced glTF core specification features, including animations, skinning, and morphing. We will cover the basics of each technique and implement skinning animations and morphing using compute shaders. In the final recipe, we will introduce animation blending, showing how to create complex animation sequences.

Most of the C++ code provided here can be applied not only to these recipes but also throughout the rest of the book.

## Chapter Contents

- Introduction to node-based animations
- Introduction to skeletal animations
- Importing skeleton and animation data
- Implementing the glTF animation player
- Doing skeletal animations in compute shaders
- Introduction to morph targets
- Loading glTF morph target data
- Adding morph targets support
- Animation blending

> **Note:** We use the official Khronos Sample Viewer as a reference for applying glTF skinning and morphing data, though our implementation doesn't rely on this project. The official Khronos Sample Viewer uses JavaScript and vertex shaders to implement animation features, while in our recipes, we use compute shaders and a streamlined approach that favors simplicity of implementation over feature completeness.

## Technical Requirements

We assume readers have a basic understanding of linear algebra and calculus, and we recommend becoming familiar with the glTF 2.0 specification: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations

In this chapter, we continue refining our unified glTF Viewer sample code that we built in previous chapters. The variations in each recipe's main.cpp file include using different model files to demonstrate the specific glTF animation feature covered in each recipe, adjusting initial camera positions for optimal model presentation, and making minor UI changes to highlight the glTF features explored in each recipe.

**Key Source Files:**
- `shared/UtilsGLTF.cpp` - glTF Viewer source code
- `data/shaders/gltf/` - GLSL vertex and fragment shaders
- `shared/UtilsAnim.cpp` and `shared/UtilsAnim.h` - Animation-specific utilities

---

## Introduction to Node-Based Animations

In this recipe, we will explore node-based animations, how they're organized, and what the key principles of the glTF specification are. This builds on the code from Chapters 6 and 7.

### Overview of Computer Animation

At its core, computer animation is the process of creating moving images. This can be achieved in different ways, from playing a sequence of still images to using physics-based simulations for each pixel. Our focus will be on real-time computer graphics, which typically use efficient methods such as scene hierarchy transformations, skeletal animation, and morphing. Efficiency is key, as it allows the data to be represented and processed in real time on today's hardware.

### glTF Scene Structure

In Chapters 6 and 7, we looked at different scene graph representations and how a scene is represented in the glTF specification. Each scene is structured like a tree, with transformations applied to each node. By changing these node transformations, we can create motion, which is the basis of node-based animation. In addition to the scene graph hierarchy, glTF includes a way to store animation data. All animations in glTF are stored in the asset's animations array.

### Animation Components

An animation is defined by **channels** and **samplers**:
- **Samplers** refer to accessors that contain keyframe data and specify interpolation methods
- **Channels** link these keyframe outputs to specific nodes in the hierarchy

**Keyframes** assign specific transformations to particular time points. glTF supports translations, quaternion-based rotations, scaling, and weights to represent these transformations.

### Interpolation Types

In glTF, animation interpolation is the process of calculating the intermediate parameter values between keyframes, which creates specific transitions between distinct animation poses.

glTF supports several interpolation types:

1. **LINEAR**: The simplest type, where the value at a given time is linearly interpolated between the values of the previous and next keyframes.

2. **STEP**: The value stays constant until the next keyframe, at which point it jumps to the new value.

3. **CUBICSPLINE**: Uses cubic splines for smoother, more natural-looking animations and requires extra control points to shape the curve.

### Animation Interpolation Process

The glTF animation interpolation process works as follows:

1. Identify the keyframes surrounding the current time.
2. Calculate the weights for each keyframe based on the current time and the distance between them.
3. Interpolate the keyframe values using the specified interpolation type and calculated weights.

**Example with two keyframes:**
```
Keyframe 1: Time = 0, Value = 10
Keyframe 2: Time = 1, Value = 20
If the current time is 0.4, the interpolated value would be (1 - 0.4) * 10 + 0.4 * 20 = 14
```

glTF animations can contain multiple channels, each controlling a different property of a node (such as translation, rotation, or scale). Interpolation is applied independently to each channel. In hierarchical animations, transformations of parent nodes influence the transformations of child nodes, with interpolation applied recursively throughout the hierarchy.

---

## Introduction to Skeletal Animations

Vertex skinning animation is a technique for animating 3D characters by linking a polygonal mesh to a hierarchical skeleton. Bones in the skeleton control the mesh deformation, allowing for realistic movement. This method is widely used in animation, enabling animators to control complex characters through intuitive tools.

### Skeleton Creation Process

The process begins with creating a skeleton of bones, each with a defined position, scale, and orientation. These bones are arranged hierarchically, resembling a human or animal skeleton. The mesh is then associated with specific bones, often using weights to indicate how much influence each bone has on particular vertices of the mesh. As the bones move, the mesh deforms naturally, resulting in lifelike animations.

### Bone Transformations

To animate a mesh, we need to dynamically transform its vertices. Each bone defines a transformation, and the most flexible method is to store a 4x4 affine transformation matrix for each bone. This matrix encapsulates the combined effects of scaling, rotation, and translation. While it's possible to store separate translation, rotation, and scaling components, using a matrix simplifies shader operations, particularly when dealing with multiples of 4 components.

> **Note:** Unfortunately, Assimp only supports linear interpolation at the time of this book's writing; thus, our examples use this mode.

### Global vs Local Transforms

These transformations should already account for the effects of parent bones. We'll refer to these as **global transforms**, in contrast to **local transforms**, which are relative to their parent and do not take a parent transform into account.

The global transform of a bone is calculated recursively: If a bone has no parent, its global transform is the same as its local transform.

### Inverse Bind Matrices

Local bone transformations are usually defined in a specific coordinate system rather than in world coordinates. For example, when rotating an arm, it makes sense to use a local coordinate system centered around the shoulder joint, which eliminates the need for explicit translation.

Each bone has its own local coordinate system. To transform vertices from the model's coordinate system into bone-local space, we use an **inverse bind matrix**. This matrix effectively repositions the vertices relative to the origin of the bone.

### Vertex Transformation Steps

To transform each vertex, we follow these steps:

1. Transform it to the model's bind pose.
2. Use the inverse bind matrix to convert the vertex to the bone-local coordinate system.
3. Apply the animation transformations in the bone-local coordinate system.
4. Transform the vertex back from the bone-local space to the model's coordinate system.
5. If the bone has a parent, repeat steps 2-4 for the parent bone.

> **Note:** The bind pose is a pose in which the mesh is in its original, undeformed state, typically serving as the basis for skinning (attaching the mesh to a skeleton).

### glTF Node Structure

If a mesh node uses skeletal animation, it includes a list of joints, which are glTF node IDs representing the skeleton's bones. These joints form a hierarchy. Note that there is no explicit armature or skeleton node; instead, bone nodes are used directly.

For a mesh primitive using skeletal animation, each vertex requires bone ID and weight attributes. Alongside the skinned mesh data, glTF files can include animations, which provide instructions for updating transformations in the node hierarchy.

### glTF Transformation Organization

1. **Model Bind Pose**: Either applied to the model directly or pre-multiplied into the inverse bind matrices. In glTF, the bind pose can essentially be ignored.
2. **Inverse Bind Matrices**: Provided as an accessor containing an array of 4x4 matrices.
3. **Animations**: Can be defined externally or stored as keyframe splines for rotation, translation, and scale. These animations are combined with the transformation from their local coordinate system to the parent's local coordinate system.
4. **Parent Relationships**: While parent nodes are defined in the node hierarchy, the conversion to the parent's coordinate system has already been applied.

---

## Importing Skeleton and Animation Data

This recipe introduces two key improvements to our glTF Viewer: it now supports storing data for skeletons and animations, and it includes functionality to load this data from glTF assets using Assimp. These enhancements are put to use in the following recipes, where they enable the implementation of mesh skinning and animations.

### Getting Ready

This recipe does not include a separate code example. Instead, it explains the modifications made to the utility code, located in the files `shared/UtilsGLTF.h` and `shared/UtilsGLTF.cpp`.

> **Note:** Here's a link to the glTF 2.0 specification, which provides a detailed explanation of skinning: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#skins

### Key Data Structures

#### Transform Reference

```cpp
using GLTFNodeRef = uint32_t;
using GLTFMeshRef = uint32_t;
struct GLTFTransforms {
    uint32_t modelMtxId;
    uint32_t matId;
    GLTFNodeRef nodeRef; // for CPU only
    GLTFMeshRef meshRef; // for CPU only
    uint32_t sortingType;
};
```

#### Vertex Bone Data

```cpp
#define MAX_BONES_PER_VERTEX 8
struct VertexBoneData {
    vec4 position;
    vec4 normal;
    uint32_t boneId[MAX_BONES_PER_VERTEX] = {
        ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u };
    float weight[MAX_BONES_PER_VERTEX] = {};
    uint32_t meshId = ~0u;
};
```

> **Note:** We use vec4 for positions and normal vectors to ensure padding compatibility, as this structure will be shared between the CPU and GPU.

#### Bone Structure

```cpp
struct GLTFBone {
    uint32_t boneId = ~0u;
    mat4 transform = mat4(1.0f);
};
```

#### GLTFContext Additions

```cpp
class GLTFContext {
    // ...
    unordered_map<std::string, GLTFBone> bonesByName;
    std::vector<MorphTarget> morphTargets;
    Holder<BufferHandle> vertexSkinningBuffer;
    Holder<BufferHandle> vertexMorphingBuffer;
    // ...
};
```

### Loading Bone Data

```cpp
void loadGLTF(GLTFContext& gltf,
    const char* glTFName, const char* glTFDataPath)
{
    // ...
    uint32_t numBones = 0;
    uint32_t vertOffset = 0;
    // ...
    gltf.hasBones = mesh->mNumBones > 0;
    for (uint32_t id = 0; id < mesh->mNumBones; id++) {
        const aiBone& bone = *mesh->mBones[id];
        const char* boneName = bone.mName.C_Str();
        
        // Store bones in unordered_map
        const bool hasBone = gltf.bonesByName.contains(boneName);
        const uint32_t boneId = hasBone ?
            gltf.bonesByName[boneName].boneId : numBones++;
        if (!hasBone)
            gltf.bonesByName[boneName] = {
                .boneId = boneId,
                .transform = aiMatrix4x4ToMat4(bone.mOffsetMatrix) };
```

> **Note:** We're using a simpler approach with `std::unordered_map` to map bone names to unique GLTFBone objects. This choice was made to keep things straightforward. When performance becomes critical, it is recommended to replace string identifiers with indices and avoid accessing hash tables at runtime, similar to how scene nodes were managed in Chapter 8.

#### Gathering Bone Weights

```cpp
        for (uint32_t w = 0; w < bone.mNumWeights; w++) {
            const uint32_t vertexId = bone.mWeights[w].mVertexId;
            VertexBoneData& vtx = skinningData[vertexId + vertOffset];
            vtx.position = vec4(vertices[vertexId + vertOffset].position, 1.0f);
            vtx.normal = vec4(vertices[vertexId + vertOffset].normal, 0.0f);
            vtx.meshId = m;
            for (uint32_t i = 0; i < MAX_BONES_PER_VERTEX; i++) {
                if (vtx.boneId[i] == ~0u) {
                    vtx.weight[i] = bone.mWeights[w].mWeight;
                    vtx.boneId[i] = boneId;
                    break;
                }
            }
        }
    }
    vertOffset += mesh->mNumVertices;
```

#### Node Matrix Mapping

```cpp
    uint32_t nonBoneMtxId = numBones;
    // ...
    const char* rootName =
        scene->mRootNode->mName.C_Str() ?
        scene->mRootNode->mName.C_Str() : "root";
    gltf.nodesStorage.push_back({
        .name = rootName,
        .modelMtxId = getNextMtxId(gltf, rootName, nonBoneMtxId,
            aiMatrix4x4ToMat4(scene->mRootNode->mTransformation)),
        .transform = aiMatrix4x4ToMat4(scene->mRootNode->mTransformation),
    });
```

#### getNextMtxId Helper Function

```cpp
uint32_t getNextMtxId(GLTFContext& gltf,
    const char* name,
    uint32_t& nextEmptyId,
    const mat4& mtx)
{
    const auto it = gltf.bonesByName.find(name);
    const uint32_t mtxId =
        it == gltf.bonesByName.end() ?
        nextEmptyId++ : it->second.boneId;
    if (gltf.matrices.size() <= mtxId)
        gltf.matrices.resize(mtxId + 1);
    gltf.matrices[mtxId] = mtx;
    return mtxId;
}
```

#### GPU Buffer Upload

```cpp
    gltf.vertexSkinningBuffer = ctx->createBuffer({
        .usage = lvk::BufferUsageBits_Vertex |
            lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_Device,
        .size = sizeof(VertexBoneData) * skinningData.size(),
        .data = skinningData.data(),
        .debugName = "Buffer: skinning vertex data",
    });
```

### Animation Data Structures

Located in `shared/UtilsAnim.h`:

```cpp
struct AnimationKeyPosition {
    vec3 pos;
    float time;
};

struct AnimationKeyRotation {
    quat rot;
    float time;
};

struct AnimationKeyScale {
    vec3 scale;
    float time;
};

struct AnimationChannel {
    std::vector<AnimationKeyPosition> pos;
    std::vector<AnimationKeyRotation> rot;
    std::vector<AnimationKeyScale> scale;
};

struct Animation {
    unordered_map<int, AnimationChannel> channels;
    std::vector<MorphingChannel> morphChannels;
    float duration; // in seconds
    float ticksPerSecond;
    std::string name;
};
```

### Loading Animations from Assimp

```cpp
void initAnimations(GLTFContext& glTF, const aiScene* scene) {
    glTF.animations.resize(scene->mNumAnimations);
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        Animation& anim = glTF.animations[i];
        anim.name = scene->mAnimations[i]->mName.C_Str();
        anim.duration = scene->mAnimations[i]->mDuration;
        anim.ticksPerSecond = scene->mAnimations[i]->mTicksPerSecond;
        
        for (uint32_t c = 0; c < scene->mAnimations[i]->mNumChannels; c++) {
            const aiNodeAnim* channel = scene->mAnimations[i]->mChannels[c];
            uint32_t boneId = glTF.bonesByName[channel->mNodeName.data].boneId;
            
            if (boneId == ~0u) {
                for (const GLTFNode& node : glTF.nodesStorage) {
                    if (node.name != channel->mNodeName.data)
                        continue;
                    boneId = node.modelMtxId;
                    glTF.bonesByName[boneName] = {
                        .boneId = boneId,
                        .transform = glTF.hasBones ?
                            inverse(node.transform) : mat4(1) };
                    break;
                }
            }
            anim.channels[boneId] = initChannel(channel);
        }
    }
}
```

#### initChannel Helper Function

```cpp
AnimationChannel initChannel(const aiNodeAnim* anim)
{
    AnimationChannel channel;
    channel.pos.resize(anim->mNumPositionKeys);
    for (uint32_t i = 0; i < anim->mNumPositionKeys; ++i) {
        channel.pos[i] = {
            .pos = aiVector3DToVec3(anim->mPositionKeys[i].mValue),
            .time = (float)anim->mPositionKeys[i].mTime };
    }
    channel.rot.resize(anim->mNumRotationKeys);
    for (uint32_t i = 0; i < anim->mNumRotationKeys; ++i) {
        channel.rot[i] = {
            .rot = aiQuaternionToQuat(anim->mRotationKeys[i].mValue),
            .time = (float)anim->mRotationKeys[i].mTime };
    }
    channel.scale.resize(anim->mNumScalingKeys);
    for (uint32_t i = 0; i < anim->mNumScalingKeys; ++i) {
        channel.scale[i] = {
            .scale = aiVector3DToVec3(anim->mScalingKeys[i].mValue),
            .time = (float)anim->mScalingKeys[i].mTime };
    }
    return channel;
}
```

#### Integration in loadGLTF

```cpp
void loadGLTF(GLTFContext& gltf,
    const char* glTFName, const char* glTFDataPath)
{
    const aiScene* scene = aiImportFile(glTFName, aiProcess_Triangulate);
    // ...
    traverseTree(scene->mRootNode, gltf.root);
    initAnimations(gltf, scene);
    // ...
}
```

---

## Implementing the glTF Animation Player

Let's start by building a foundation for our glTF animation player. Before we do skinned animations, advanced morphing, or complex blending, we first need to implement the basic animation rendering capabilities using the glTF data we loaded in the previous recipes.

### Getting Ready

Make sure to read the previous recipe, Importing skeleton and animation data, to recap the data structures we use to store glTF animation data.

**Source code:** `Chapter09/01_AnimationPlayer`

### Animation State Structure

```cpp
struct AnimationState {
    uint32_t animId = ~0u;
    float currentTime = 0.0f;
    bool playOnce = false;
    bool active = false;
};
```

### updateAnimation Function

```cpp
void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt) {
    if (!anim.active || (anim.animId == ~0u)) {
        glTF.morphing = false;
        glTF.skinning = false;
        return;
    }
    
    const Animation& activeAnim = glTF.animations[anim.animId];
    anim.currentTime += activeAnim.ticksPerSecond * dt;
    
    if (anim.playOnce && anim.currentTime > activeAnim.duration) {
        anim.currentTime = activeAnim.duration;
        anim.active = false;
    } else {
        anim.currentTime = fmodf(anim.currentTime, activeAnim.duration);
    }
```

### Tree Traversal for Animation

```cpp
    std::function<void(GLTFNodeRef gltfNode, const mat4& parentTransform)>
    traverseTree = [&](GLTFNodeRef gltfNode, const mat4& parentTransform)
    {
        const GLTFBone& bone = glTF.bonesByName[glTF.nodesStorage[gltfNode].name];
        const uint32_t boneId = bone.boneId;
        
        if (boneId != ~0u) {
            auto channel = activeAnim.channels.find(boneId);
            const bool hasActiveChannel = channel != activeAnim.channels.end();
            glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                parentTransform * (hasActiveChannel ?
                animationTransform(channel->second, anim.currentTime) :
                glTF.nodesStorage[gltfNode].transform);
            glTF.skinning = true;
        } else {
            glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                parentTransform * glTF.nodesStorage[gltfNode].transform;
        }
        
        for (uint32_t i = 0; i < glTF.nodesStorage[gltfNode].children.size(); i++) {
            const GLTFNodeRef child = glTF.nodesStorage[gltfNode].children[i];
            traverseTree(child, glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
        }
    };
```

### animationTransform Helper

```cpp
mat4 animationTransform(const AnimationChannel& channel, float time)
{
    mat4 translation = glm::translate(mat4(1.0f),
        interpolatePosition(channel, time));
    mat4 rotation = glm::toMat4(
        glm::normalize(interpolateRotation(channel, time)));
    mat4 scale = glm::scale(mat4(1.0f),
        interpolateScaling(channel, time));
    return translation * rotation * scale;
}
```

> **Note:** The order of these transformations is defined by the glTF specification, ensuring consistent application across the animations.

### Interpolation Helper Functions

#### getTimeIndex (Binary Search)

```cpp
template <typename T> uint32_t getTimeIndex(const std::vector<T>& t, float time) {
    return std::max(0,
        (int)std::distance(t.begin(), std::lower_bound(
            t.begin(), t.end(), time,
            [&](const T& lhs, float rhs) { return lhs.time < rhs; })
        ) - 1);
}
```

#### interpolationVal

```cpp
float interpolationVal(float lastTimeStamp, float nextTimeStamp, float animationTime)
{
    return (animationTime - lastTimeStamp) / (nextTimeStamp - lastTimeStamp);
}
```

#### interpolatePosition

```cpp
vec3 interpolatePosition(const AnimationChannel& channel, float time)
{
    if (channel.pos.size() == 1)
        return channel.pos[0].pos;
    const uint32_t start = getTimeIndex<>(channel.pos, time);
    const uint32_t end = start + 1;
    const float factor = interpolationVal(
        channel.pos[start].time, channel.pos[end].time, time);
    return glm::mix(channel.pos[start].pos, channel.pos[end].pos, factor);
}
```

#### interpolateRotation

```cpp
quat interpolateRotation(const AnimationChannel& channel, float time)
{
    if (channel.rot.size() == 1)
        return channel.rot[0].rot;
    const uint32_t start = getTimeIndex<>(channel.rot, time);
    const uint32_t end = start + 1;
    const float factor = interpolationVal(
        channel.rot[start].time, channel.rot[end].time, time);
    return glm::slerp(channel.rot[start].rot, channel.rot[end].rot, factor);
}
```

> **Note:** Slerp is shorthand for spherical linear interpolation. It is an algorithm that interpolates a new quaternion between two given quaternions. https://en.wikipedia.org/wiki/Slerp

#### interpolateScaling

```cpp
vec3 interpolateScaling(const AnimationChannel& channel, float time)
{
    if (channel.scale.size() == 1)
        return channel.scale[0].scale;
    const uint32_t start = getTimeIndex<>(channel.scale, time);
    const uint32_t end = start + 1;
    const float factor = interpolationVal(
        channel.scale[start].time, channel.scale[end].time, time);
    return glm::mix(channel.scale[start].scale, channel.scale[end].scale, factor);
}
```

### Applying Bind Transformation

```cpp
void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt)
{
    // ... skipped code
    traverseTree(glTF.root, mat4(1.0f));
    for (const std::pair<std::string, GLTFBone>& b : glTF.bonesByName) {
        if (b.second.boneId != ~0u)
            glTF.matrices[b.second.boneId] =
                glTF.matrices[b.second.boneId] * b.second.transform;
    }
    // ...
}
```

### Rendering Updates

```cpp
void renderGLTF(GLTFContext& gltf,
    const mat4& model, const mat4& view, const mat4& proj,
    bool rebuildRenderList)
{
    // ...
    if (gltf.animated)
        buf.cmdUpdateBuffer(gltf.matricesBuffer, 0,
            gltf.matrices.size() * sizeof(mat4), gltf.matrices.data());
    // ...
}
```

### Demo Application

```cpp
int main() {
    VulkanApp app({
        .initialCameraPos = vec3(7.0f, 6.8f, -13.6f),
        .initialCameraTarget = vec3(1.7f, -1.0f, 0.0f),
        .showGLTFInspector = true });
    
    GLTFContext gltf(app);
    loadGLTF(gltf,
        "data/meshes/medieval_fantasy_book/scene.gltf",
        "data/meshes/medieval_fantasy_book/");
    gltf.enableMorphing = false;
    
    const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 2.1f, 0.0f)) *
        glm::scale(mat4(1.0f), vec3(0.2f));
    
    AnimationState anim = {
        .animId = 0,
        .currentTime = 0.0f,
        .playOnce = false,
        .active = true };
    
    gltf.inspector = { .activeAnim = { 0 }, .showAnimations = true };
    
    app.run([&](uint32_t width, uint32_t height,
        float aspectRatio, float deltaSeconds)
    {
        const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
        const mat4 v = app.camera_.getViewMatrix();
        const mat4 p = glm::perspective(45.f, aspectRatio, 0.01f, 100.f);
        
        animateGLTF(gltf, anim, deltaSeconds);
        renderGLTF(gltf, m, v, p);
        
        if (gltf.inspector.activeAnim[0] != anim.animId)
            anim = {
                .animId = gltf.inspector.activeAnim[0],
                .currentTime = 0.0f,
                .playOnce = false,
                .active = true };
    });
    return 0;
}
```

### Model Attribution

The 3D model used in this recipe is based on "Medieval Fantasy Book" (https://sketchfab.com/3d-models/medieval-fantasy-book-06d5a80a04fc4c5ab552759e9a97d91a) by Pixel (https://sketchfab.com/stefan.lengyel1) licensed under CC-BY-4.0 (http://creativecommons.org/licenses/by/4.0/).

---

## Doing Skeletal Animations in Compute Shaders

Skeletal animations imply moving vertices based on the weighted influence of multiple matrices, which represent a skeleton of bones. Each bone is represented as a matrix that influences nearby vertices based on its weight. Vertices near the wrist might be affected by both the hand and arm bones. In essence, skinning requires bones, a hierarchy of matrices, and weights.

Weights, like vertex colors, are assigned per vertex and range from 0 to 1, indicating how much a specific bone influences that vertex's position. These weights are stored in a buffer and passed as vertex attributes.

### Compute Shader vs Vertex Shader Approach

An older approach to applying skinning data to each vertex of a mesh is to use a vertex shader. This was once common practice, and the official Khronos glTF viewer still uses it. However, vertex shaders can be less efficient when vertices are reused multiple times with different indices, leading to redundant complex calculations. To improve efficiency, we can perform these calculations in a compute shader, avoiding the need to process each vertex instance individually.

### Getting Ready

Be sure to review the recipe Importing skeleton and animation data before moving forward.

**Demo source code:** `Chapter09/02_Skinning`  
**Compute shader:** `data/shaders/gltf/animation.comp`

### GLTFContext Additions

```cpp
struct GLTFContext {
    // ...
    Holder<ComputePipelineHandle> pipelineComputeAnimations;
    // ...
    Holder<ShaderModuleHandle> animation;
    // ...
};
```

### Loading the Compute Shader

```cpp
// In loadGLTF()
gltf.animation = loadShaderModule(ctx, "data/shaders/gltf/animation.comp");
```

### animateGLTF Function

```cpp
void animateGLTF(GLTFContext& gltf, AnimationState& anim, float dt)
{
    if (gltf.transforms.empty()) return;
    if (gltf.pipelineComputeAnimations.empty())
        gltf.pipelineComputeAnimations =
            gltf.app.ctx_->createComputePipeline({ .smComp = gltf.animation });
    anim.active = anim.animId != ~0;
    gltf.animated = anim.active;
    if (anim.active) updateAnimation(gltf, anim, dt);
}
```

### Compute Pass Setup in renderGLTF

```cpp
void renderGLTF(GLTFContext& gltf,
    const mat4& model, const mat4& view, const mat4& proj,
    bool rebuildRenderList)
{
    // ...
    if (gltf.animated) {
        buf.cmdUpdateBuffer(gltf.matricesBuffer, 0,
            gltf.matrices.size() * sizeof(mat4), gltf.matrices.data());
        if (gltf.morphing) {
            buf.cmdUpdateBuffer(gltf.morphStatesBuffer, 0,
                gltf.morphStates.size() * sizeof(MorphState),
                gltf.morphStates.data());
        }
        updateLights(gltf, buf);
        
        if ((gltf.skinning && gltf.hasBones) || gltf.morphing)
        {
            struct ComputeSetup {
                uint64_t matrices;
                uint64_t morphStates;
                uint64_t morphVertexBuffer;
                uint64_t inBuffer;
                uint64_t outBuffer;
                uint32_t numMorphStates;
            } pc = {
                .matrices = ctx->gpuAddress(gltf.matricesBuffer),
                .morphStates = ctx->gpuAddress(gltf.morphStatesBuffer),
                .morphVertexBuffer = ctx->gpuAddress(gltf.vertexMorphingBuffer),
                .inBuffer = ctx->gpuAddress(gltf.vertexSkinningBuffer),
                .outBuffer = ctx->gpuAddress(gltf.vertexBuffer),
                .numMorphStates = gltf.morphStates.size(),
            };
            buf.cmdBindComputePipeline(gltf.pipelineComputeAnimations);
            buf.cmdPushConstants(pc);
```

> **Note:** Padding with dummy vertices is done in loadGLTF():
> ```cpp
> gltf.maxVertices = (1 + (vertices.size() / 16)) * 16;
> vertices.resize(gltf.maxVertices);
> ```

### Dispatch with Buffer Dependencies

```cpp
            buf.cmdDispatchThreadGroups(
                { .width = gltf.maxVertices / 16, },
                { .buffers = {
                    BufferHandle(gltf.vertexBuffer),
                    BufferHandle(gltf.morphStatesBuffer),
                    BufferHandle(gltf.matricesBuffer),
                    BufferHandle(gltf.vertexSkinningBuffer)}
                });
        }
    }
```

### Compute Shader (animation.comp)

```glsl
#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
layout (local_size_x=16, local_size_y=1, local_size_z=1) in;
```

#### Data Structures

```glsl
struct TransformsBuffer {
    uint mtxId;
    uint matId;
    uint nodeRef;
    uint meshRef;
    uint opaque;
};

struct VertexSkinningData {
    vec4 pos;
    vec4 norm;
    uint bones[8];
    float weights[8];
    uint meshId;
};

struct VertexData {
    vec3 pos;
    vec3 norm;
    vec4 color;
    vec4 uv;
    float padding[2];
};

#define MAX_WEIGHTS 8
struct MorphState {
    uint meshId;
    uint morphTarget[MAX_WEIGHTS];
    float weights[MAX_WEIGHTS];
};
```

#### Buffer References

```glsl
layout (std430, buffer_reference) readonly buffer Matrices {
    mat4 matrix[];
};
layout (scalar, buffer_reference) readonly buffer MorphStates {
    MorphState morphStates[];
};
layout (scalar, buffer_reference) readonly buffer VertexSkinningBuffer {
    VertexSkinningData vertices[];
};
layout (scalar, buffer_reference) writeonly buffer VertexBuffer {
    VertexData vertices[];
};
layout (scalar, buffer_reference) readonly buffer MorphVertexBuffer {
    VertexData vertices[];
};
```

#### Push Constants

```glsl
layout (push_constant) uniform PerFrameData {
    Matrices matrices;
    MorphStates morphStates;
    MorphVertexBuffer morphTargets;
    VertexSkinningBuffer inBufferId;
    VertexBuffer outBufferId;
    uint numMorphStates;
} pc;
```

#### Main Function

```glsl
void main() {
    uint index = gl_GlobalInvocationID.x;
    VertexSkinningData inVtx = pc.inBufferId.vertices[index];
    vec4 inPos = vec4(inVtx.pos.xyz, 1.0);
    vec4 inNorm = vec4(inVtx.norm.xyz, 0.0);
    
    // Morphing (covered in later recipes)
    if (inVtx.meshId < pc.numMorphStates) {
        MorphState ms = pc.morphStates.morphStates[inVtx.meshId];
        if (ms.meshId != ~0)
            for (int m = 0; m != MAX_WEIGHTS; m++)
                if (ms.weights[m] > 0) {
                    VertexData mVtx = pc.morphTargets.vertices[
                        ms.morphTarget[m] + index];
                    inPos.xyz += mVtx.pos * ms.weights[m];
                    inNorm.xyz += mVtx.norm * ms.weights[m];
                }
    }
    
    // Skinning
    vec4 pos = vec4(0);
    vec4 norm = vec4(0);
    int i = 0;
    for (; i != MAX_WEIGHTS; i++) {
        if (inVtx.bones[i] == ~0) break;
        mat4 boneMat = pc.matrices.matrix[inVtx.bones[i]];
        pos += boneMat * inPos * inVtx.weights[i];
        norm += transpose(inverse(boneMat)) * inNorm * inVtx.weights[i];
    }
    
    // Fallback to bind pose if no weights
    if (i == 0) {
        pos.xyz = inPos.xyz;
        norm.xyz = inNorm.xyz;
    }
    
    pc.outBufferId.vertices[index].pos = pos.xyz;
    pc.outBufferId.vertices[index].norm = normalize(norm.xyz);
}
```

> **Note:** For readers seeking a deeper understanding of normal transformation, you can refer to this resource: https://terathon.com/blog/transforming-normals.html

### Running the Demo

The demo uses the Khronos model located at `deps/src/glTF-Sample-Assets/Models/Fox/glTF/Fox.gltf`. Try switching between different animations: Survey, Walk, and Run. When you click to switch animations, you will notice that the transition occurs quite abruptly and unnaturally. This can be improved by interpolating between multiple animations using a floating-point transition factor (covered in Animation blending).

---

## Introduction to Morph Targets

A morph target is a deformed version of a mesh. In glTF, morph targets are used to create mesh deformations by blending different sets of vertex positions, or other attributes, like normal vectors, according to specified weights. They are especially useful for facial animations, character expressions, and other types of mesh deformation in 3D models.

### Key Concepts

- **Base Mesh**: The original, unaltered mesh defined by its vertices, normals, texture coordinates, and other attributes.

- **Morph Targets**: Modified versions of the base mesh, where the positions of some or all vertices have been adjusted to create a specific deformation. Multiple morph targets can be defined for the same mesh, each representing a different shape variation.

- **Vertex Data**: Morph targets include offset data for vertices. A morph target usually adjusts the vertex positions (and sometimes other attributes like normals) of the base mesh. These offsets specify how much each vertex should move when the morph target is applied.

- **Weights**: Each morph target has an associated weight that controls its influence on the mesh. The weight is a floating-point value between 0 and 1:
  - 0 = no influence (mesh appears identical to base mesh)
  - 1 = full influence (mesh fully deforms to match the morph target)
  - Values between blend the base mesh with the morph target

### Final Position Formula

When multiple morph targets are used, the mesh's final shape is calculated as a weighted sum of the morph targets:

```
finalPosition = basePosition + w1*offset1 + w2*offset2 + ... + wN*offsetN
```

Where:
- `basePosition` is the original position of the vertex in the base mesh
- `w1, w2, ..., wN` are the weights for each morph target
- `offset1, offset2, ..., offsetN` are the vertex offsets from the morph targets

---

## Loading glTF Morph Targets Data

Similar to the recipe Importing skeleton and animation data, this one doesn't include a standalone example either. Instead, it focuses on the modifications needed in our glTF loading code to load morph target data from glTF files using the Assimp library.

### Getting Ready

Refer to the recipe Importing skeleton and animation data for a quick refresher on the data structures needed to support animations.

### Data Structures

#### MorphTarget Structure

```cpp
struct MorphTarget {
    uint32_t meshId = ~0u;
    std::vector<uint32_t> offset;
};
```

#### MorphState Structure (in shared/UtilsAnim.h)

```cpp
struct MorphState {
    uint32_t meshId = ~0u;
    uint32_t morphTarget[MAX_MORPH_WEIGHTS] = {};
    float weights[MAX_MORPH_WEIGHTS] = {};
};
```

#### GLTFContext Additions

```cpp
struct GLTFContext {
    // ...
    std::vector<MorphTarget> morphTargets;
    Holder<BufferHandle> morphStatesBuffer;
    Holder<BufferHandle> vertexMorphingBuffer;
    std::vector<MorphState> morphStates;
    bool morphing = false;
    // ...
};
```

### Loading Code

```cpp
void loadGLTF(GLTFContext& gltf,
    const char* glTFName, const char* glTFDataPath)
{
    const aiScene* scene = aiImportFile(glTFName, aiProcess_Triangulate);
    // ...
    for (uint32_t meshId = 0; meshId != scene->mNumMeshes; meshId++) {
        const aiMesh* m = scene->mMeshes[meshId];
        if (!m->mNumAnimMeshes) continue;
        
        MorphTarget& morphTarget = gltf.morphTargets[meshId];
        morphTarget.meshId = meshId;
        
        for (uint32_t a = 0; a < m->mNumAnimMeshes; a++) {
            const aiAnimMesh* mesh = m->mAnimMeshes[a];
            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                const aiVector3D srcNorm = m->mNormals ?
                    m->mNormals[i] : aiVector3D(0, 1, 0);
                const aiVector3D v = mesh->mVertices[i];
                const aiVector3D n = mesh->mNormals ?
                    mesh->mNormals[i] : aiVector3D(0, 1, 0);
                const aiColor4D c = mesh->mColors[0] ?
                    mesh->mColors[0][i] : aiColor4D(1);
                const aiVector3D uv0 = mesh->mTextureCoords[0] ?
                    mesh->mTextureCoords[0][i] : aiVector3D(0);
                const aiVector3D uv1 = mesh->mTextureCoords[1] ?
                    mesh->mTextureCoords[1][i] : aiVector3D(0);
                
                // Store as deltas (glTF spec defines morph targets as deltas)
                morphData.push_back({
                    .position = vec3(v.x - m->mVertices[i].x,
                        v.y - m->mVertices[i].y,
                        v.z - m->mVertices[i].z),
                    .normal = vec3(n.x - srcNorm.x,
                        n.y - srcNorm.y,
                        n.z - srcNorm.z),
                    .color = vec4(c.r, c.g, c.b, c.a),
                    .uv0 = vec2(uv0.x, 1.0f - uv0.y),
                    .uv1 = vec2(uv1.x, 1.0f - uv1.y),
                });
            }
            morphTarget.offset.push_back(morphTargetsOffset);
            morphTargetsOffset += mesh->mNumVertices;
        }
    }
    // ...
}
```

### GPU Buffer Creation

```cpp
    const bool hasMorphData = !morphData.empty();
    gltf.vertexMorphingBuffer = ctx->createBuffer({
        .usage = lvk::BufferUsageBits_Vertex | lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_Device,
        .size = hasMorphData ?
            sizeof(Vertex) * morphData.size() : sizeof(Vertex),
        .data = hasMorphData ? morphData.data() : nullptr,
        .debugName = "Buffer: morphing vertex data",
    });
```

---

## Adding Morph Targets Support

In the previous recipe, we loaded the morph target data from glTF. Now, let's use that data to create a demo application and render morphing meshes.

### Getting Ready

Be sure to read the previous recipe, Loading glTF morph targets data.

**Source code:** `Chapter09/03_Morphing`

### Morphing Data Structures

```cpp
#define MAX_MORPH_WEIGHTS 8
#define MAX_MORPHS 100

struct MorphingChannelKey {
    float time = 0.0f;
    uint32_t mesh[MAX_MORPH_WEIGHTS] = {};
    float weight[MAX_MORPH_WEIGHTS] = {};
};

struct MorphingChannel {
    std::string name;
    std::vector<MorphingChannelKey> key;
};
```

### Animation Structure Update

```cpp
struct Animation {
    unordered_map<int, AnimationChannel> channels;
    std::vector<MorphingChannel> morphChannels;
    float duration;
    float ticksPerSecond;
    std::string name;
};
```

### Loading Morphing Channels

```cpp
void initAnimations(GLTFContext& glTF, const aiScene* scene) {
    glTF.animations.resize(scene->mNumAnimations);
    for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
        // ...
        const uint32_t numMorphTargetChannels =
            scene->mAnimations[i]->mNumMorphMeshChannels;
        anim.morphChannels.resize(numMorphTargetChannels);
        
        for (int c = 0; c < numMorphTargetChannels; c++) {
            const aiMeshMorphAnim* channel =
                scene->mAnimations[i]->mMorphMeshChannels[c];
            MorphingChannel& morphChannel = anim.morphChannels[c];
            morphChannel.name = channel->mName.C_Str();
            morphChannel.key.resize(channel->mNumKeys);
            
            for (uint32_t k = 0; k < channel->mNumKeys; ++k) {
                MorphingChannelKey& key = morphChannel.key[k];
                key.time = channel->mKeys[k].mTime;
                for (uint32_t v = 0;
                    v < std::min(MAX_MORPH_WEIGHTS,
                        channel->mKeys[k].mNumValuesAndWeights); v++)
                {
                    key.mesh[v] = channel->mKeys[k].mValues[v];
                    key.weight[v] = channel->mKeys[k].mWeights[v];
                }
            }
        }
    }
}
```

### Update Animation for Morphing

```cpp
void updateAnimation(GLTFContext& glTF, AnimationState& anim, float dt)
{
    // ...
    glTF.morphStates.resize(glTF.meshesStorage.size());
    if (glTF.enableMorphing) {
        if (!activeAnim.morphChannels.empty()) {
            for (size_t i = 0; i < activeAnim.morphChannels.size(); ++i) {
                const MorphingChannel& channel = activeAnim.morphChannels[i];
                const uint32_t meshId = glTF.meshesRemap[channel.name];
                const MorphTarget& morphTarget = glTF.morphTargets[meshId];
                if (morphTarget.meshId != ~0u)
                    glTF.morphStates[morphTarget.meshId] =
                        morphTransform(morphTarget, channel, anim.currentTime);
            }
            glTF.morphing = true;
        }
    }
}
```

### morphTransform Helper

```cpp
MorphState morphTransform(
    const MorphTarget& target,
    const MorphingChannel& channel,
    float time)
{
    MorphState ms;
    ms.meshId = target.meshId;
    float mix = 0.0f;
    int start = 0;
    int end = 0;
    
    if (channel.key.size() > 0) {
        start = getTimeIndex(channel.key, time);
        end = start + 1;
        mix = interpolationVal(
            channel.key[start].time,
            channel.key[end].time, time);
    }
    
    for (uint32_t i = 0;
        i < std::min((uint32_t)target.offset.size(), (uint32_t)MAX_MORPH_WEIGHTS);
        ++i)
    {
        ms.morphTarget[i] = target.offset[channel.key[start].mesh[i]];
        ms.weights[i] = glm::mix(
            channel.key[start].weight[i],
            channel.key[end].weight[i], mix);
    }
    return ms;
}
```

### Compute Shader Morphing

```glsl
void main() {
    uint index = gl_GlobalInvocationID.x;
    VertexSkinningData inVtx = pc.inBufferId.vertices[index];
    vec4 inPos = vec4(inVtx.pos.xyz, 1.0);
    vec4 inNorm = vec4(inVtx.norm.xyz, 0.0);
    
    if (inVtx.meshId < pc.numMorphStates) {
        MorphState ms = pc.morphStates.morphStates[inVtx.meshId];
        if (ms.meshId != ~0) {
            for (int m = 0; m != MAX_WEIGHTS; m++) {
                if (ms.weights[m] > 0) {
                    VertexData mVtx = pc.morphTargets.vertices[
                        ms.morphTarget[m] + index];
                    inPos.xyz += mVtx.pos * ms.weights[m];
                    inNorm.xyz += mVtx.norm * ms.weights[m];
                }
            }
        }
    }
    // ...
}
```

### Demo Application

```cpp
int main() {
    VulkanApp app({
        .initialCameraPos = vec3(2.13f, 2.44f, -3.5f),
        .initialCameraTarget = vec3(0, 0, 2),
        .showGLTFInspector = true });
    
    GLTFContext gltf(app);
    loadGLTF(gltf,
        "deps/src/glTF-Sample-Assets/Models/AnimatedMorphCube/glTF/AnimatedMorphCube.gltf",
        "deps/src/glTF-Sample-Assets/Models/AnimatedMorphCube/glTF/");
    
    const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));
    
    AnimationState anim = {
        .animId = 0,
        .currentTime = 0.0f,
        .playOnce = false,
        .active = true };
    
    gltf.inspector = { .activeAnim = { 0 }, .showAnimations = true };
    
    app.run([&](uint32_t width, uint32_t height,
        float aspectRatio, float deltaSeconds)
    {
        const mat4 m = t * glm::rotate(mat4(1.0f), glm::radians(180.0f), vec3(0, 1, 0));
        const mat4 v = app.camera_.getViewMatrix();
        const mat4 p = glm::perspective(45, aspectRatio, 0.01, 100);
        
        animateGLTF(gltf, anim, deltaSeconds);
        renderGLTF(gltf, m, v, p);
        
        if (gltf.inspector.activeAnim[0] != anim.animId) {
            anim = {
                .animId = gltf.inspector.activeAnim[0],
                .currentTime = 0.0f,
                .playOnce = false,
                .active = true };
        }
    });
    return 0;
}
```

### Notes on Assimp Limitations

At the time this book was written, Assimp used string names to bind animation channels to meshes, which does not fully align with the glTF specification. Additionally, it lacks comprehensive support for loading morph targets from glTF files.

**Recommended reading:** *Computer Animation: Algorithms and Techniques* by Rick Parent - a comprehensive guide covering the principles and methodologies of this dynamic field.

---

## Animation Blending

In the recipes Implementing the glTF animation player and Doing skeletal animations in compute shaders, we implemented an animation player with the capability to switch between skeletal animations. However, when switching between animations, the transition happens rather abruptly and unnaturally.

We can improve this by interpolating between animations using a floating-point transition factor. This technique requires not only the interpolation of per-channel transformations but also a gradual blending between different animations.

### Getting Ready

Make sure to revisit the recipes Implementing the glTF animation player and Doing skeletal animations in compute shaders.

**Source code:** `Chapter09/04_AnimationBlending`

### Basic Concept

We have 2 different animations and a floating-point blending factor (0...1) between them to calculate the resulting animation. If we can successfully blend 2 animations, this technique can be trivially extended to blend any number of animations.

### Demo Application

```cpp
int main() {
    VulkanApp app({
        .initialCameraPos = vec3(2.13f, 2.44f, -3.5f),
        .initialCameraTarget = vec3(0, 0, 2),
        .showGLTFInspector = true });
    
    GLTFContext gltf(app);
    loadGLTF(gltf, "deps/src/glTF-Sample-Assets/Models/Fox/glTF/Fox.gltf",
        "deps/src/glTF-Sample-Assets/Models/Fox/glTF/");
    
    const mat4 t = glm::translate(mat4(1.0f), vec3(0.0f, 1.1f, 0.0f));
    const mat4 s = glm::scale(mat4(1.0f), vec3(0.01f, 0.01f, 0.01f));
    
    AnimationState anim1 = {
        .animId = 1,
        .currentTime = 0.0f,
        .playOnce = false,
        .active = true };
    AnimationState anim2 = {
        .animId = 2,
        .currentTime = 0.0f,
        .playOnce = false,
        .active = true };
    
    gltf.skinning = true;
    gltf.inspector = {
        .activeAnim = {1, 2},
        .blend = 0.5f,
        .showAnimations = true,
        .showAnimationBlend = true };
    
    app.run([&](uint32_t width, uint32_t height,
        float aspectRatio, float deltaSeconds)
    {
        const mat4 m = t * glm::rotate(mat4(1.0f),
            glm::radians(90.0f), vec3(0, 1, 0)) * s;
        const mat4 v = app.camera_.getViewMatrix();
        const mat4 p = glm::perspective(45, aspectRatio, 0.01, 100);
        
        animateBlendingGLTF(gltf, anim1, anim2,
            gltf.inspector.blend, deltaSeconds);
        renderGLTF(gltf, m, v, p);
        
        if (gltf.inspector.activeAnim[0] != anim1.animId ||
            gltf.inspector.activeAnim[1] != anim2.animId) {
            anim1 = {
                .animId = gltf.inspector.activeAnim[0],
                .currentTime = 0.0f,
                .playOnce = false,
                .active = gltf.inspector.activeAnim[0] != ~0u };
            anim2 = {
                .animId = gltf.inspector.activeAnim[1],
                .currentTime = 0.0f,
                .playOnce = false,
                .active = gltf.inspector.activeAnim[1] != ~0u };
        }
    });
    return 0;
}
```

### animateBlendingGLTF Function

```cpp
void animateBlendingGLTF(GLTFContext& gltf,
    AnimationState& anim1,
    AnimationState& anim2,
    float weight, float dt)
{
    if (gltf.transforms.empty()) return;
    if (gltf.pipelineComputeAnimations.empty())
        gltf.pipelineComputeAnimations =
            gltf.app.ctx_->createComputePipeline({ .smComp = gltf.animation });
    
    anim1.active = anim1.animId != ~0;
    anim2.active = anim2.animId != ~0;
    gltf.animated = anim1.active || anim2.active;
    
    if (anim1.active && anim2.active) {
        updateAnimationBlending(gltf, anim1, anim2, weight, dt);
    } else if (anim1.active) {
        updateAnimation(gltf, anim1, dt);
    } else if (anim2.active) {
        updateAnimation(gltf, anim2, dt);
    }
}
```

### updateAnimationBlending Function

```cpp
void updateAnimationBlending(GLTFContext& glTF,
    AnimationState& anim1,
    AnimationState& anim2,
    float weight, float dt)
{
    if (anim1.active && anim2.active) {
        const Animation& activeAnim1 = glTF.animations[anim1.animId];
        anim1.currentTime += activeAnim1.ticksPerSecond * dt;
        if (anim1.playOnce && anim1.currentTime > activeAnim1.duration) {
            anim1.currentTime = activeAnim1.duration;
            anim1.active = false;
        } else {
            anim1.currentTime = fmodf(anim1.currentTime, activeAnim1.duration);
        }
        
        const Animation& activeAnim2 = glTF.animations[anim2.animId];
        anim2.currentTime += activeAnim2.ticksPerSecond * dt;
        if (anim2.playOnce && anim2.currentTime > activeAnim2.duration) {
            anim2.currentTime = activeAnim2.duration;
            anim2.active = false;
        } else {
            anim2.currentTime = fmodf(anim2.currentTime, activeAnim2.duration);
        }
        
        std::function<void(GLTFNodeRef gltfNode, const mat4& parentTransform)>
        traverseTree = [&](GLTFNodeRef gltfNode, const mat4& parentTransform)
        {
            const GLTFBone& bone = glTF.bonesByName[
                glTF.nodesStorage[gltfNode].name];
            const uint32_t boneId = bone.boneId;
            
            if (boneId != ~0u) {
                auto channel1 = activeAnim1.channels.find(boneId);
                auto channel2 = activeAnim2.channels.find(boneId);
                
                if (channel1 != activeAnim1.channels.end() &&
                    channel2 != activeAnim2.channels.end())
                {
                    glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                        parentTransform *
                        animationTransformBlending(
                            channel1->second, anim1.currentTime,
                            channel2->second, anim2.currentTime, weight);
                } else if (channel1 != activeAnim1.channels.end()) {
                    glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                        parentTransform *
                        animationTransform(channel1->second, anim1.currentTime);
                } else if (channel2 != activeAnim2.channels.end()) {
                    glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                        parentTransform *
                        animationTransform(channel2->second, anim2.currentTime);
                } else {
                    glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId] =
                        parentTransform * glTF.nodesStorage[gltfNode].transform;
                }
                glTF.skinning = true;
            }
            
            for (uint32_t i = 0;
                i < glTF.nodesStorage[gltfNode].children.size(); i++)
            {
                const uint32_t child = glTF.nodesStorage[gltfNode].children[i];
                traverseTree(child,
                    glTF.matrices[glTF.nodesStorage[gltfNode].modelMtxId]);
            }
        };
        
        traverseTree(glTF.root, mat4(1.0f));
        for (const std::pair<std::string, GLTFBone>& b : glTF.bonesByName) {
            if (b.second.boneId != ~0u)
                glTF.matrices[b.second.boneId] =
                    glTF.matrices[b.second.boneId] * b.second.transform;
        }
    } else {
        glTF.morphing = false;
        glTF.skinning = false;
    }
}
```

### animationTransformBlending Helper

```cpp
mat4 animationTransformBlending(
    const AnimationChannel& channel1, float time1,
    const AnimationChannel& channel2, float time2,
    float weight)
{
    mat4 trans1 = glm::translate(mat4(1.0f),
        interpolatePosition(channel1, time1));
    mat4 trans2 = glm::translate(mat4(1.0f),
        interpolatePosition(channel2, time2));
    mat4 translation = glm::mix(trans1, trans2, weight);
    
    quat rot1 = interpolateRotation(channel1, time1);
    quat rot2 = interpolateRotation(channel2, time2);
    mat4 rotation = glm::toMat4(
        glm::normalize(glm::slerp(rot1, rot2, weight)));
    
    vec3 scl1 = interpolateScaling(channel1, time1);
    vec3 scl2 = interpolateScaling(channel2, time2);
    mat4 scale = glm::scale(mat4(1.0f), glm::mix(scl1, scl2, weight));
    
    return translation * rotation * scale;
}
```

### Usage Tips

Experiment with the demo: try selecting both the Survey and Run animations, then adjust the blending factor so that the fox slowly turns its head while running. Moving the Blend slider slowly from 0 to 1 will give you a smooth transition between surveying and running.

The main opportunity here is not just manual adjustment but the ability to automatically create smooth transitions between different animations. In a 3D game, you could have a state machine (commonly called a **blend tree**) to switch animations based on high-level game logic. This state machine would rely on the animation blending mechanism to ensure seamless, automatic transitions between animations.

---

## Additional Examples

Two more code examples are included:

1. **Chapter09/05_ImportLights** - Demonstrates how to import lights from a glTF file
2. **Chapter09/06_ImportCameras** - Shows how to import cameras and switch between different camera definitions within the app

---

## Summary

With this chapter, we conclude our exploration of the glTF file format and its capabilities. The key topics covered include:

- Node-based animations and the glTF animation specification
- Skeletal animations with vertex skinning
- Loading skeleton and animation data using Assimp
- Implementing an animation player with compute shader-based skinning
- Morph targets for mesh deformation
- Animation blending for smooth transitions

The next chapter shifts focus back to rendering with image-based rendering techniques and post-processing effects.
