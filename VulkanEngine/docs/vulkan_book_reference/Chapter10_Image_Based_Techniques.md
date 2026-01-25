# Chapter 10: Image-Based Techniques

This chapter presents a series of recipes for enhancing rendering realism using various image-based techniques and post-processing effects.

## Recipes in This Chapter

1. Implementing offscreen rendering in Vulkan
2. Implementing full-screen triangle rendering
3. Implementing shadow maps
4. Implementing MSAA in Vulkan
5. Implementing screen space ambient occlusion (SSAO)
6. Implementing HDR rendering and tone mapping
7. Implementing HDR light adaptation

## Technical Requirements

- Computer with video card and drivers supporting Vulkan 1.3
- Build environment from Chapter 1
- Scene and geometry loading code from Chapter 8

---

## Recipe 1: Implementing Offscreen Rendering in Vulkan

### Overview
Learn how to render directly into specific mip levels of an image and access each mip level individually. This foundational technique is used throughout for various rendering and post-processing effects.

### Demo Location
`Chapter10/01_OffscreenRendering`

### Prerequisites
- Review "Using texture data in Vulkan" from Chapter 3

### Key Concepts

#### Creating Icosahedron Geometry
```cpp
struct VertexData { float pos[3]; };
const float t = (1.0f + sqrtf(5.0f)) / 2.0f;  // Golden ratio
const VertexData vertices[] = {
    {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, { 1, -t, 0},
    { 0,-1, t}, {0, 1, t}, { 0, -1,-t}, { 0, 1, -t},
    { t, 0,-1}, {t, 0, 1}, {-t, 0,-1}, {-t, 0, 1}
};
const uint16_t indices[] = { 0, 11, 5, 0, 5, 1, 0, 1,
    7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10,
    2, 10, 7, 6, 7, 1, 8, 3, 9, 4, 3, 4, 2, 3, 2, 6,
    3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10,
    8, 6, 7, 9, 8, 1 };
```

#### Creating Texture with Mipmap Pyramid
```cpp
constexpr uint8_t numMipLevels = lvk::calcNumMipLevels(512, 512);
Holder<TextureHandle> texture = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_UN8,
    .dimensions = {512, 512},
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
    .numMipLevels = numMipLevels
});
```

#### Creating Texture Views for Individual Mip Levels
```cpp
Holder<TextureHandle> mipViews[numMipLevels];
for (uint32_t mip = 0; mip != numMipLevels; mip++)
    mipViews[mip] = ctx->createTextureView(texture, { .mipLevel = mip });
```

The `createTextureView()` function creates a new `VkImageView` while keeping the original `VkImage` from the initial texture.

#### Rendering to Individual Mip Levels
```cpp
ICommandBuffer& buf = ctx->acquireCommandBuffer();
for (uint8_t i = 0; i != numMipLevels; i++) {
    buf.cmdBeginRendering(RenderPass{
        .color = { {.loadOp = lvk::LoadOp_Clear,
                    .level = i,
                    .clearColor = {colors[i].r, colors[i].g, colors[i].b, 1}}}
    }, lvk::Framebuffer{.color = { { .texture = texture }} });
    buf.cmdEndRendering();
}
ctx->submit(buf);
```

### GLSL Shaders

#### Vertex Shader (Cylindrical UV Mapping)
```glsl
layout (location = 0) in vec3 pos;
layout (location = 0) out vec2 uv;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
};

#define PI 3.1415926
float atan2(float y, float x) {
    return x == 0.0 ? sign(y) * PI/2 : atan(y, x);
}

void main() {
    gl_Position = mvp * vec4(pos, 1.0);
    uv = vec2(atan2(pos.y, pos.x), acos(pos.z / length(pos)));
}
```

#### Fragment Shader
```glsl
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 out_FragColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint textureId;
};

void main() {
    out_FragColor = textureBindless2D(textureId, uv);
}
```

### How It Works

The `createTextureView()` implementation in LightweightVK:
```cpp
Holder<TextureHandle> VulkanContext::createTextureView(
    TextureHandle handle, const TextureViewCreateDesc& desc)
{
    const lvk::VulkanTexture& parentTex = *texturesPool_.get(handle);
    const VkImageViewCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = parentTex.vkImage_,
        .viewType = textureTypeToVkImageViewType(parentTex.type_),
        .format = parentTex.vkImageFormat_,
        .subresourceRange = {
            .aspectMask = parentTex.vkImageAspect_,
            .baseMipLevel = desc.mipLevel,
            .levelCount = desc.numMipLevels > 0 ? desc.numMipLevels : 
                          parentTex.numMipLevels_ - desc.mipLevel,
            .baseArrayLayer = 0,
            .layerCount = parentTex.numLayers_
        }
    };
    // Creates new VkImageView with subset of mip levels
}
```

---

## Recipe 2: Implementing Full-Screen Triangle Rendering

### Overview
Render a full-screen triangle without vertex buffers - a common technique for post-processing effects. More efficient than two triangles (avoids diagonal overdraw).

### Demo Location
`Chapter10/02_FullScreenTriangle`

### Key Concepts

#### Full-Screen Triangle Vertex Shader
```glsl
layout (location = 0) out vec2 uv;

void main() {
    // Generate full-screen triangle vertices procedurally
    // Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
    uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
```

#### Fragment Shader
```glsl
layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 out_FragColor;

layout(push_constant) uniform PushConstants {
    uint textureId;
    uint samplerId;
};

void main() {
    out_FragColor = textureBindless2D(textureId, samplerId, uv);
}
```

#### Pipeline Configuration
```cpp
Holder<lvk::RenderPipelineHandle> pipelineFullScreenQuad =
    ctx->createRenderPipeline({
        .smVert = vertFullScreenQuad,
        .smFrag = fragFullScreenQuad,
        .color = { { .format = ctx->getSwapchainFormat() } },
    });
```

#### Drawing Without Vertex Buffer
```cpp
buf.cmdBindRenderPipeline(pipelineFullScreenQuad);
buf.cmdPushConstants(pc);
buf.cmdDraw(3);  // 3 vertices, no vertex buffer
```

### Why a Triangle Instead of Quad?
- Single triangle covers entire screen
- Avoids potential texture sampling issues at quad diagonal
- Fewer vertices to process
- More cache-friendly rasterization

---

## Recipe 3: Implementing Shadow Maps

### Overview
Implement shadow mapping for spotlights using two-pass rendering: first render scene depth from light's perspective, then use depth map to determine shadows.

### Demo Location
`Chapter10/03_ShadowMapping`

### Prerequisites
- Review "Implementing offscreen rendering in Vulkan"

### Key Concepts

#### Shadow Map Texture Setup
```cpp
const uint32_t kShadowMapWidth = 1024;
const uint32_t kShadowMapHeight = 1024;

Holder<TextureHandle> textureShadowMap = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_Z_UN16,  // 16-bit depth format
    .dimensions = {kShadowMapWidth, kShadowMapHeight},
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
    .numMipLevels = 1
});
```

#### Shadow Map Sampler with Comparison
```cpp
Holder<SamplerHandle> samplerShadow = ctx->createSampler({
    .wrapU = lvk::SamplerWrap_Clamp,
    .wrapV = lvk::SamplerWrap_Clamp,
    .depthCompareOp = lvk::CompareOp_LessOrEqual,
    .depthCompareEnabled = true
});
```

#### Light Space Matrix Calculation
```cpp
const mat4 lightView = glm::lookAt(lightPos, lightTarget, vec3(0, 1, 0));
const mat4 lightProj = glm::perspective(
    glm::radians(45.0f),
    float(kShadowMapWidth) / float(kShadowMapHeight),
    0.1f, 100.0f
);
const mat4 lightSpaceMatrix = lightProj * lightView;
```

#### Shadow Depth Pass
```cpp
// Render scene from light's perspective (depth only)
buf.cmdBeginRendering(
    lvk::RenderPass{
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
    },
    lvk::Framebuffer{ .depthStencil = { .texture = textureShadowMap } }
);
// Draw scene geometry with depth-only shader
buf.cmdEndRendering();
```

#### Shadow Fragment Shader
```glsl
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
    mat4 lightSpaceMatrix;
    uint texShadowMap;
    uint smplShadow;
    vec4 lightPos;
};

float shadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    // PCF (Percentage Closer Filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureBindlessSize2D(texShadowMap);
    for(int x = -1; x <= 1; x++) {
        for(int y = -1; y <= 1; y++) {
            shadow += textureBindless2DShadow(
                texShadowMap, smplShadow,
                vec3(projCoords.xy + vec2(x, y) * texelSize, projCoords.z)
            );
        }
    }
    return shadow / 9.0;
}
```

### Shadow Acne and Solutions

**Problem:** Self-shadowing artifacts due to depth precision limits.

**Solutions:**
1. **Depth Bias:** Add small offset to depth values
2. **PCF:** Percentage Closer Filtering - sample multiple depth values
3. **Normal Offset:** Offset sampling position along surface normal

---

## Recipe 4: Implementing MSAA in Vulkan

### Overview
Implement Multisample Anti-Aliasing (MSAA) to reduce jagged edges. Requires multisampled render targets and resolve operations.

### Demo Location
`Chapter10/04_MSAA`

### Prerequisites
- Review "Implementing offscreen rendering in Vulkan"

### Key Concepts

#### Creating Multisampled Textures
```cpp
const uint32_t kNumSamples = 8;  // 8x MSAA

// Multisampled color attachment
Holder<TextureHandle> textureMSAA = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = ctx->getSwapchainFormat(),
    .dimensions = {width, height},
    .usage = lvk::TextureUsageBits_Attachment,
    .numSamples = kNumSamples
});

// Multisampled depth attachment
Holder<TextureHandle> textureDepthMSAA = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = app.getDepthFormat(),
    .dimensions = {width, height},
    .usage = lvk::TextureUsageBits_Attachment,
    .numSamples = kNumSamples
});
```

#### Pipeline Configuration for MSAA
```cpp
Holder<lvk::RenderPipelineHandle> pipelineMSAA =
    ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert = vert,
        .smFrag = frag,
        .color = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .samplesCount = kNumSamples
    });
```

#### Render Pass with MSAA Resolve
```cpp
buf.cmdBeginRendering(
    lvk::RenderPass{
        .color = {{
            .loadOp = lvk::LoadOp_Clear,
            .storeOp = lvk::StoreOp_MsaaResolve,  // Resolve to single-sample
            .clearColor = {0.1f, 0.1f, 0.1f, 1.0f}
        }},
        .depth = {
            .loadOp = lvk::LoadOp_Clear,
            .clearDepth = 1.0f
        }
    },
    lvk::Framebuffer{
        .color = {{ .texture = textureMSAA }},
        .colorResolve = {{ .texture = ctx->getCurrentSwapchainTexture() }},
        .depthStencil = { .texture = textureDepthMSAA }
    }
);
```

### How MSAA Works

1. **Multiple Samples:** Each pixel stores multiple color/depth samples
2. **Edge Detection:** Hardware determines which samples are covered by triangle edges
3. **Sample Shading:** Fragment shader runs once per pixel (not per sample)
4. **Resolve:** Average samples to produce final pixel color

### Performance Considerations

| MSAA Level | Memory Multiplier | Quality |
|------------|-------------------|---------|
| 2x         | 2x                | Low     |
| 4x         | 4x                | Medium  |
| 8x         | 8x                | High    |
| 16x        | 16x               | Maximum |

---

## Recipe 5: Implementing Screen Space Ambient Occlusion (SSAO)

### Overview
SSAO approximates ambient occlusion using only screen-space depth information. Creates subtle shadows in crevices and corners.

### Demo Location
`Chapter10/04_SSAO`

### Prerequisites
- Review previous recipes in this chapter
- Review "Implementing MSAA in Vulkan"

### Key Concepts

#### SSAO Pipeline Overview
1. **G-Buffer Pass:** Render depth and normals
2. **SSAO Pass:** Compute occlusion from depth samples
3. **Blur Pass:** Bilateral blur to smooth results
4. **Composite Pass:** Apply SSAO to final image

#### G-Buffer Setup
```cpp
// Depth texture
Holder<TextureHandle> texDepth = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_Z_UN24,
    .dimensions = {width, height},
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled
});

// View-space normals texture
Holder<TextureHandle> texNormals = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_F16,
    .dimensions = {width, height},
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled
});
```

#### SSAO Kernel Generation
```cpp
const int kKernelSize = 64;
vec4 ssaoKernel[kKernelSize];

for (int i = 0; i < kKernelSize; i++) {
    // Random hemisphere sample
    vec3 sample = vec3(
        randomFloat(-1.0f, 1.0f),
        randomFloat(-1.0f, 1.0f),
        randomFloat(0.0f, 1.0f)  // Hemisphere facing +Z
    );
    sample = glm::normalize(sample);
    
    // Scale samples to cluster closer to origin
    float scale = float(i) / float(kKernelSize);
    scale = glm::mix(0.1f, 1.0f, scale * scale);
    ssaoKernel[i] = vec4(sample * scale, 0.0f);
}
```

#### Random Rotation Texture (4x4)
```cpp
const int kNoiseSize = 4;
vec4 ssaoNoise[kNoiseSize * kNoiseSize];

for (int i = 0; i < kNoiseSize * kNoiseSize; i++) {
    ssaoNoise[i] = vec4(
        randomFloat(-1.0f, 1.0f),
        randomFloat(-1.0f, 1.0f),
        0.0f, 0.0f
    );
}
```

#### SSAO Compute Shader
```glsl
layout (local_size_x = 16, local_size_y = 16) in;

layout(push_constant) uniform PushConstants {
    mat4 proj;
    uint texDepth;
    uint texNormals;
    uint texNoise;
    uint texSSAO;
    float radius;
    float bias;
};

const int kKernelSize = 64;
layout(std430, binding = 0) readonly buffer SSAOKernel {
    vec4 samples[kKernelSize];
};

void main() {
    vec2 uv = (gl_GlobalInvocationID.xy + 0.5) / imageSize(texSSAO);
    
    // Get view-space position from depth
    float depth = textureBindless2D(texDepth, uv).r;
    vec3 fragPos = viewPosFromDepth(uv, depth, proj);
    
    // Get view-space normal
    vec3 normal = textureBindless2D(texNormals, uv).xyz;
    
    // Random rotation from noise texture
    vec2 noiseScale = imageSize(texSSAO) / 4.0;
    vec3 randomVec = textureBindless2D(texNoise, uv * noiseScale).xyz;
    
    // Create TBN matrix for hemisphere orientation
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // Sample kernel and accumulate occlusion
    float occlusion = 0.0;
    for (int i = 0; i < kKernelSize; i++) {
        vec3 samplePos = fragPos + TBN * samples[i].xyz * radius;
        
        // Project sample to screen space
        vec4 offset = proj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // Compare depths
        float sampleDepth = textureBindless2D(texDepth, offset.xy).r;
        float rangeCheck = smoothstep(0.0, 1.0, 
            radius / abs(fragPos.z - viewPosFromDepth(offset.xy, sampleDepth, proj).z));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }
    
    imageStore(texSSAO, ivec2(gl_GlobalInvocationID.xy), 
               vec4(1.0 - occlusion / kKernelSize));
}
```

#### Bilateral Blur
```glsl
// Blur while preserving edges using depth-aware weights
void main() {
    float centerDepth = textureBindless2D(texDepth, uv).r;
    vec3 result = vec3(0.0);
    float totalWeight = 0.0;
    
    for (int i = -kBlurRadius; i <= kBlurRadius; i++) {
        vec2 offset = kIsHorizontal ? vec2(i, 0) : vec2(0, i);
        vec2 sampleUV = uv + offset * texelSize;
        
        float sampleDepth = textureBindless2D(texDepth, sampleUV).r;
        float depthDiff = abs(centerDepth - sampleDepth);
        float weight = gaussianWeight[abs(i)] * (1.0 / (1.0 + depthDiff * 100.0));
        
        result += textureBindless2D(texSSAO, sampleUV).rgb * weight;
        totalWeight += weight;
    }
    
    out_FragColor = vec4(result / totalWeight, 1.0);
}
```

### SSAO Parameters

| Parameter | Effect | Typical Range |
|-----------|--------|---------------|
| `radius`  | Sample spread | 0.1 - 2.0 |
| `bias`    | Acne prevention | 0.01 - 0.05 |
| `kernelSize` | Quality vs performance | 16 - 64 |

---

## Recipe 6: Implementing HDR Rendering and Tone Mapping

### Overview
Render to high dynamic range (HDR) textures and apply tone mapping to display on standard monitors.

### Demo Location
`Chapter10/05_HDR`

### Prerequisites
- Review previous recipes in this chapter

### Key Concepts

#### HDR Render Target
```cpp
Holder<TextureHandle> texHDRScene = ctx->createTexture({
    .type = lvk::TextureType_2D,
    .format = lvk::Format_RGBA_F16,  // 16-bit float per channel
    .dimensions = {width, height},
    .usage = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled
});
```

#### HDR Pipeline Overview
1. **Scene Rendering:** Render to 16-bit float HDR target
2. **Bright Pass:** Extract bright areas for bloom
3. **Bloom:** Gaussian blur of bright areas
4. **Luminance:** Calculate scene luminance with mip pyramid
5. **Tone Mapping:** Convert HDR to LDR for display

#### Bright Pass Compute Shader
```glsl
layout (local_size_x = 16, local_size_y = 16) in;

layout(push_constant) uniform PushConstants {
    uint texColor;       // HDR scene (rgba16f)
    uint texOut;         // Bright areas (rgba16f)
    uint texLuminance;   // Luminance (r16f)
    float exposure;
};

void main() {
    vec2 uv = (gl_GlobalInvocationID.xy + 0.5) / textureSize(texOut);
    
    // 3x3 box filter for smoothing
    vec4 color = vec4(0);
    vec2 texelSize = 1.0 / textureSize(texOut);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            color += textureBindless2D(texColor, uv + vec2(x-1, y-1) * texelSize);
        }
    }
    color /= 9.0;
    
    // Convert to luminance
    float luminance = exposure * dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    
    // Only keep bright areas
    vec3 rgb = luminance > 1.0 ? color.rgb : vec3(0);
    
    imageStore(texOut, ivec2(gl_GlobalInvocationID.xy), vec4(rgb, 1.0));
    imageStore(texLuminance, ivec2(gl_GlobalInvocationID.xy), vec4(luminance));
}
```

#### Tone Mapping Functions

##### Reinhard Extended
```glsl
vec3 reinhard2(vec3 v, float maxWhite) {
    float l_old = dot(v, vec3(0.2126, 0.7152, 0.0722));
    float l_new = l_old * (1.0 + (l_old / (maxWhite * maxWhite))) / (1.0 + l_old);
    return v * (l_new / l_old);
}
```

##### Uchimura (Gran Turismo)
```glsl
vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    vec3 w0 = 1.0 - smoothstep(vec3(0.0), vec3(m), x);
    vec3 w2 = step(vec3(m + l0), x);
    vec3 w1 = 1.0 - w0 - w2;
    
    vec3 T = m * pow(x / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (x - S0));
    vec3 L = m + a * (x - m);
    
    return T * w0 + L * w1 + S * w2;
}
```

##### Khronos PBR Neutral
```glsl
vec3 PBRNeutralToneMapping(vec3 color, float startCompression, float desaturation) {
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;
    
    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}
```

#### Tone Mapping Fragment Shader
```glsl
layout(push_constant) uniform PushConstants {
    uint texColor;
    uint texLuminance;
    uint texBloom;
    int drawMode;
    float exposure;
    float bloomStrength;
    float maxWhite;
    // Uchimura params
    float P, a, m, l, c, b;
    // Khronos params
    float startCompression, desaturation;
};

void main() {
    vec3 hdrColor = textureBindless2D(texColor, uv).rgb;
    vec3 bloom = textureBindless2D(texBloom, uv).rgb;
    
    // Apply exposure and bloom
    hdrColor *= exposure;
    hdrColor += bloom * bloomStrength;
    
    vec3 ldrColor;
    switch (drawMode) {
        case ToneMappingMode_None:
            ldrColor = hdrColor;
            break;
        case ToneMappingMode_Reinhard:
            ldrColor = reinhard2(hdrColor, maxWhite);
            break;
        case ToneMappingMode_Uchimura:
            ldrColor = uchimura(hdrColor, P, a, m, l, c, b);
            break;
        case ToneMappingMode_KhronosPBR:
            ldrColor = PBRNeutralToneMapping(hdrColor, startCompression, desaturation);
            break;
    }
    
    // Gamma correction
    out_FragColor = vec4(pow(ldrColor, vec3(1.0/2.2)), 1.0);
}
```

### Luminance Mip Pyramid

Calculate average scene luminance by generating mip chain:
```cpp
// Create luminance texture with full mip pyramid
uint32_t numMipLevels = lvk::calcNumMipLevels(512, 512);
Holder<TextureHandle> texLuminance = ctx->createTexture({
    .format = lvk::Format_R_F16,
    .dimensions = {512, 512},
    .numMipLevels = numMipLevels
});

// Generate mips with compute shader (average 4 texels per output)
for (uint32_t mip = 1; mip < numMipLevels; mip++) {
    buf.cmdBindComputePipeline(pipelineDownsample);
    buf.cmdPushConstants({
        .texIn = texLumViews[mip - 1].index(),
        .texOut = texLumViews[mip].index()
    });
    buf.cmdDispatchThreadGroups({
        (width >> mip + 15) / 16,
        (height >> mip + 15) / 16
    });
}
```

---

## Recipe 7: Implementing HDR Light Adaptation

### Overview
Simulate human eye adaptation to changing light conditions using temporal smoothing of scene luminance.

### Demo Location
`Chapter10/06_HDR_Adaptation`

### Key Concepts

#### Adapted Luminance Textures (Ping-Pong)
```cpp
Holder<TextureHandle> texAdaptedLum[2] = {
    ctx->createTexture({
        .format = lvk::Format_R_F16,
        .dimensions = {1, 1}
    }),
    ctx->createTexture({
        .format = lvk::Format_R_F16,
        .dimensions = {1, 1}
    })
};
```

#### Adaptation Compute Shader
```glsl
layout (local_size_x = 1, local_size_y = 1) in;

layout(push_constant) uniform PushConstants {
    uint texCurrSceneLuminance;
    uint texPrevAdaptedLuminance;
    uint texAdaptedOut;
    float adaptationSpeed;  // deltaTime * speedFactor
};

void main() {
    float lumCurr = imageLoad(texCurrSceneLuminance, ivec2(0, 0)).x;
    float lumPrev = imageLoad(texPrevAdaptedLuminance, ivec2(0, 0)).x;
    
    // Filament adaptation equation
    float factor = 1.0 - exp(-adaptationSpeed);
    float newAdaptation = lumPrev + (lumCurr - lumPrev) * factor;
    
    imageStore(texAdaptedOut, ivec2(0, 0), vec4(newAdaptation));
}
```

#### C++ Integration
```cpp
float adaptationSpeed = 3.0f;

// In render loop:
const struct AdaptationPC {
    uint32_t texCurrSceneLuminance;
    uint32_t texPrevAdaptedLuminance;
    uint32_t texNewAdaptedLuminance;
    float adaptationSpeed;
} pcAdaptation = {
    .texCurrSceneLuminance = texLumViews[numMipLevels - 1].index(),
    .texPrevAdaptedLuminance = texAdaptedLum[0].index(),
    .texNewAdaptedLuminance = texAdaptedLum[1].index(),
    .adaptationSpeed = deltaSeconds * adaptationSpeed
};

buf.cmdBindComputePipeline(pipelineAdaptation);
buf.cmdPushConstants(pcAdaptation);
buf.cmdDispatchThreadGroups({1, 1});

// After submit, swap ping-pong buffers
std::swap(texAdaptedLum[0], texAdaptedLum[1]);
```

### Adaptation Equation

From Filament documentation:
```
L_adapted = L_prev + (L_curr - L_prev) * (1 - e^(-dt * speed))
```

Where:
- `L_adapted`: New adapted luminance
- `L_prev`: Previous frame's adapted luminance
- `L_curr`: Current frame's scene luminance
- `dt`: Delta time
- `speed`: Adaptation rate parameter

---

## Summary: Complete HDR Pipeline

```
┌─────────────────┐
│  Scene Render   │ (HDR 16-bit float)
└────────┬────────┘
         │
    ┌────▼────┐
    │ Bright  │ Extract luminance > 1.0
    │  Pass   │
    └────┬────┘
         │
    ┌────▼────┐
    │  Bloom  │ Gaussian blur (separable)
    │  Pass   │
    └────┬────┘
         │
    ┌────▼────┐
    │Luminance│ Generate mip pyramid
    │Pyramid  │ (average down to 1x1)
    └────┬────┘
         │
    ┌────▼────┐
    │Adaptation│ Temporal smoothing
    │  Pass    │
    └────┬────┘
         │
    ┌────▼────┐
    │  Tone   │ HDR → LDR conversion
    │ Mapping │ (Reinhard/Uchimura/Khronos)
    └────┬────┘
         │
    ┌────▼────┐
    │ Display │ (8-bit LDR)
    └─────────┘
```

---

## Further Reading

- **HDR Lighting:** "Uncharted 2: HDR Lighting" by John Hable (GDC 2010)
- **Post Processing:** "Next Generation Post Processing in Call of Duty: Advanced Warfare" by Jorge Jimenez (SIGGRAPH 2014)
- **Filament:** https://google.github.io/filament/Filament.md.html
- **Tone Mapping:** https://64.github.io/tonemapping
- **Uchimura:** https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp

---

## Session Planning (for Vulkan Learning Project)

### Suggested 2-Hour Sessions

| Session | Topic | Implementation Goals |
|---------|-------|---------------------|
| 10.1 | Offscreen Rendering | Texture views, mip level rendering, render-to-texture |
| 10.2 | Full-Screen Effects | Full-screen triangle, post-process foundation |
| 10.3 | Shadow Mapping | Depth-only pass, light matrices, PCF filtering |
| 10.4 | MSAA | Multisampled attachments, resolve operations |
| 10.5 | SSAO Part 1 | G-buffer setup, kernel generation |
| 10.6 | SSAO Part 2 | SSAO compute shader, bilateral blur |
| 10.7 | HDR Part 1 | HDR targets, bright pass, bloom |
| 10.8 | HDR Part 2 | Tone mapping operators, luminance pyramid |
| 10.9 | Light Adaptation | Temporal adaptation, ping-pong buffers |
| 10.10 | Integration | Combine all effects into unified pipeline |

### Git Milestones
- `ch10-offscreen-rendering`
- `ch10-fullscreen-triangle`
- `ch10-shadow-mapping`
- `ch10-msaa`
- `ch10-ssao`
- `ch10-hdr-bloom`
- `ch10-tone-mapping`
- `ch10-light-adaptation`
