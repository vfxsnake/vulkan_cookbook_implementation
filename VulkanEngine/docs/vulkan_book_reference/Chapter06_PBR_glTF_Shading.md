# Chapter 6: Physically Based Rendering Using the glTF 2.0 Shading Model

## Overview

This chapter covers integrating physically based rendering (PBR) into graphics applications using the glTF 2.0 shading model. PBR is a set of concepts using measured surface values and realistic shading models to accurately represent real-world materials.

## Technical Requirements

- glTF 2.0 specification: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
- Source code: https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition/

## Recipes Covered

1. An introduction to glTF 2.0 physically based shading model
2. Rendering unlit glTF 2.0 materials
3. Precomputing BRDF look-up tables
4. Precomputing irradiance maps and diffuse convolution
5. Implementing the glTF 2.0 core metallic-roughness shading model
6. Implementing the glTF 2.0 core specular-glossiness shading model

---

## Recipe 1: Introduction to glTF 2.0 PBR

### Key Concepts

**Light-Object Interactions:**
- **Specular reflection**: Mirror-like reflection from smooth surfaces
- **Diffuse light/Photon diffusion/Subsurface scattering**: Light that penetrates and scatters within material
- **Albedo**: Color defined by fractions of light wavelengths scattered back (diffuse color)

**Energy Conservation:**
- Total incoming light = reflected + transmitted + absorbed light
- Highly reflective objects have minimal diffuse light
- Materials with strong diffusion cannot be highly reflective

### Bidirectional Scattering Distribution Functions (BSDFs)

**BRDF (Bidirectional Reflectance Distribution Function):**
- Describes how light is reflected from a surface
- Components:
  - **Fresnel term (F)**: Determines reflection amount at given angle
  - **Normal Distribution Function (NDF/D)**: Microfacet distribution
  - **Geometry term (G)**: Accounts for microfacet shadowing

**GGX BRDF Formula:**
```
f(l,v) = F(v,h) * D(h) * G(l,v,h) / (4 * (n·l) * (n·v))
```

Where:
- D is GGX NDF: `D(h) = α² / (π * ((n·h)²(α²-1)+1)²)`
- G is geometric shadowing factor
- F is Fresnel term: `F(v,h,F0) = F0 + (1-F0)(1-(v·h))⁵`

### Surface Types

| Surface Type | Description |
|-------------|-------------|
| Conductor/Metal | High specular reflection, no diffuse |
| Dielectric/Insulator | Low specular reflection, high diffuse |
| Semiconductor | Mixed properties |

### glTF 2.0 Material Models

1. **Metallic-Roughness** (primary model)
2. **Specular-Glossiness** (via KHR_materials_pbrSpecularGlossiness extension)
3. **Unlit** (KHR_materials_unlit extension)

---

## Recipe 2: Rendering Unlit glTF 2.0 Materials

### Material Type Detection

```cpp
enum MaterialType : uint32_t {
    MaterialType_Invalid = 0,
    MaterialType_Unlit = 0xF,
    MaterialType_MetallicRoughness = 0x1,
    MaterialType_SpecularGlossiness = 0x2,
};
```

### Loading with Assimp

```cpp
aiString baseColorTexturePath;
if (mtlDescriptor->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, 
    &baseColorTexturePath) != AI_SUCCESS) {
    mtlDescriptor->GetTexture(aiTextureType_DIFFUSE, 0, &baseColorTexturePath);
}
```

### Vertex Shader

```glsl
layout(std430, buffer_reference) readonly buffer Vertices {
    Vertex in_Vertices[];
};

struct PerVertex {
    vec2 uv;
    vec4 color;
};

void main() {
    gl_Position = pc.proj * pc.view * pc.model * vec4(getPosition(gl_VertexIndex), 1.0);
    vtx.uv = getTexCoord(gl_VertexIndex);
    vtx.color = pc.vertexColor;
}
```

### Fragment Shader

```glsl
layout (location = 0) in vec2 uv;
layout (location = 1) in vec4 vertexColor;
layout (location = 0) out vec4 out_FragColor;

void main() {
    vec4 baseColorTexture = textureBindless2D(textureId, 0, uv);
    out_FragColor = baseColorTexture * vertexColor;
}
```

---

## Recipe 3: Precomputing BRDF Look-up Tables

### Purpose

BRDF LUT stores precomputed values to avoid expensive per-pixel BRDF calculations:
- X-axis: dot(N, V) - angle between normal and view direction
- Y-axis: Roughness (0...1)
- Channels: R,G for GGX BRDF scale/bias, B for Sheen (Charlie) BRDF

### C++ Implementation

```cpp
const uint32_t kBrdfW = 256;
const uint32_t kBrdfH = 256;
const uint32_t kNumSamples = 1024;

void calculateLUT(const std::unique_ptr<lvk::IContext>& ctx,
                  void* output, uint32_t size)
{
    Holder<ShaderModuleHandle> comp = loadShaderModule(
        ctx, "Chapter06/02_BRDF_LUT/src/main.comp");
    
    Holder<ComputePipelineHandle> computePipelineHandle =
        ctx->createComputePipeline({
            .smComp = comp,
            .specInfo = {
                .entries = {{ .constantId = 0, .size = sizeof(kNumSamples) }},
                .data = &kNumSamples,
                .dataSize = sizeof(kNumSamples),
            },
        });
    
    Holder<BufferHandle> dstBuffer = ctx->createBuffer({
        .usage = lvk::BufferUsageBits_Storage,
        .storage = lvk::StorageType_HostVisible,
        .size = size,
    });
    
    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    buf.cmdBindComputePipeline(computePipelineHandle);
    buf.cmdPushConstants(pc);
    buf.cmdDispatchThreadGroups({ kBrdfW / 16, kBrdfH / 16, 1 });
    
    ctx->wait(ctx->submit(buf));
    memcpy(output, ctx->getMappedPtr(dstBuffer), size);
}
```

### GLSL Compute Shader

```glsl
layout (local_size_x=16, local_size_y=16, local_size_z=1) in;
layout (constant_id = 0) const uint NUM_SAMPLES = 1024u;

// Hammersley point generation for quasi-random sampling
vec2 hammersley2d(uint i, uint N) {
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u)<<1u)|((bits & 0xAAAAAAAAu)>>1u);
    bits = ((bits & 0x33333333u)<<2u)|((bits & 0xCCCCCCCCu)>>2u);
    bits = ((bits & 0x0F0F0F0Fu)<<4u)|((bits & 0xF0F0F0F0u)>>4u);
    bits = ((bits & 0x00FF00FFu)<<8u)|((bits & 0xFF00FF00u)>>8u);
    float rdi = float(bits) * 2.3283064365386963e-10;
    return vec2(float(i) / float(N), rdi);
}

// GGX importance sampling
vec3 importanceSample_GGX(vec2 Xi, float roughness, vec3 normal) {
    float alpha = roughness * roughness;
    float phi = 2.0 * PI * Xi.x + random(normal.xz) * 0.1;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (alpha*alpha - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
    
    vec3 up = abs(normal.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 tangentX = normalize(cross(up, normal));
    vec3 tangentY = normalize(cross(normal, tangentX));
    
    return normalize(tangentX * H.x + tangentY * H.y + normal * H.z);
}

// GGX geometric shadowing factor
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float k = (roughness * roughness) / 2.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

vec3 BRDF(float NoV, float roughness) {
    vec3 N = vec3(0.0, 0.0, 1.0);
    vec3 V = vec3(sqrt(1.0 - NoV * NoV), 0.0, NoV);
    
    vec2 r = vec2(0.0);
    float r2 = 0.0;
    
    for (uint i = 0; i < NUM_SAMPLES; i++) {
        vec2 Xi = hammersley2d(i, NUM_SAMPLES);
        
        // GGX sampling
        vec3 H = importanceSample_GGX(Xi, roughness, N);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = clamp(L.z, 0.0, 1.0);
        float NdotH = clamp(H.z, 0.0, 1.0);
        float VdotH = clamp(dot(V, H), 0.0, 1.0);
        
        if (NdotL > 0.0) {
            float G = G_SchlicksmithGGX(NdotL, NoV, roughness);
            float GVis = (G * VdotH) / (NdotH * NoV);
            float Fc = pow(1.0 - VdotH, 5.0);
            r += vec2((1.0 - Fc) * GVis, Fc * GVis);
        }
        
        // Charlie (Sheen) sampling - similar process
    }
    
    return vec3(r / float(NUM_SAMPLES), r2 / float(NUM_SAMPLES));
}
```

---

## Recipe 4: Precomputing Irradiance Maps and Diffuse Convolution

### Types of Prefiltered Maps

1. **Lambertian** - Diffuse irradiance
2. **GGX** - Specular radiance (roughness-based mip levels)
3. **Charlie** - Sheen radiance

### C++ Prefiltering Function

```cpp
void prefilterCubemap(
    const std::unique_ptr<lvk::IContext>& ctx,
    Holder<TextureHandle> envMapCube,
    Holder<TextureHandle>& prefilteredMapCube,
    lvk::Holder<ShaderModuleHandle>& vert,
    lvk::Holder<ShaderModuleHandle>& frag,
    uint32_t envMapDim,
    Distribution distribution,
    uint32_t sampleCount)
{
    auto cube = ctx->queryTextureFormatDesc(prefilteredMapCube);
    
    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    for (uint32_t mip = 0; mip < cube->numLevels; mip++) {
        for (uint32_t face = 0; face < 6; face++) {
            buf.cmdBeginRendering(
                { .color = {{ .loadOp = lvk::LoadOp_Clear,
                              .layer = (uint8_t)face,
                              .level = (uint8_t)mip,
                              .clearColor = {1,1,1,1} }} },
                { .color = {{ .texture = prefilteredMapCube }} });
            
            struct PerFrameData {
                uint32_t face;
                float roughness;
                uint32_t sampleCount;
                uint32_t width;
                uint32_t envMap;
                uint32_t distribution;
                uint32_t sampler;
            } perFrameData = {
                .face = face,
                .roughness = (float)(mip) / (cube->numLevels - 1),
                .sampleCount = sampleCount,
                .width = cube->baseWidth,
                .envMap = envMapCube.index(),
                .distribution = uint32_t(distribution),
            };
            
            buf.cmdPushConstants(perFrameData);
            buf.cmdDraw(3);  // Full-screen triangle
            buf.cmdEndRendering();
        }
    }
    ctx->submit(buf);
}
```

### Fragment Shader - Filtering

```glsl
vec3 filterColor(vec3 N) {
    vec3 color = vec3(0.f);
    float weight = 0.0f;
    
    for (uint i = 0; i < perFrameData.sampleCount; i++) {
        vec4 importanceSample = getImportanceSample(i, N, perFrameData.roughness);
        vec3 H = vec3(importanceSample.xyz);
        float pdf = importanceSample.w;
        float lod = computeLod(pdf);
        
        if (perFrameData.distribution == cLambertian) {
            vec3 lambertian = textureBindlessCubeLod(
                perFrameData.envMap, perFrameData.samplerIdx, H, lod).xyz;
            color += lambertian;
        } else if (perFrameData.distribution == cGGX) {
            vec3 V = N;
            vec3 L = normalize(reflect(-V, H));
            float NdotL = dot(N, L);
            if (NdotL > 0.0) {
                vec3 sampleColor = textureBindlessCubeLod(
                    perFrameData.envMap, perFrameData.samplerIdx, L, lod).xyz;
                color += sampleColor * NdotL;
                weight += NdotL;
            }
        }
    }
    
    color /= (weight != 0.0f) ? weight : float(perFrameData.sampleCount);
    return color.rgb;
}

// LOD calculation for importance sampling
float computeLod(float pdf) {
    float w = float(perFrameData.width);
    float h = float(perFrameData.height);
    float sampleCount = float(perFrameData.sampleCount);
    return 0.5 * log2(6.0 * w * h / (sampleCount * pdf));
}
```

---

## Recipe 5: Metallic-Roughness Shading Model

### Helper Structures

```cpp
struct GLTFGlobalSamplers {
    Holder<SamplerHandle> clamp;
    Holder<SamplerHandle> wrap;
    Holder<SamplerHandle> mirror;
};

struct EnvironmentMapTextures {
    Holder<TextureHandle> texBRDF_LUT;
    Holder<TextureHandle> envMapTexture;          // GGX prefiltered
    Holder<TextureHandle> envMapTextureCharlie;   // Charlie prefiltered
    Holder<TextureHandle> envMapTextureIrradiance; // Lambertian
};

struct GLTFMaterialTextures {
    Holder<TextureHandle> baseColorTexture;
    Holder<TextureHandle> surfacePropertiesTexture; // Metallic-Roughness
    Holder<TextureHandle> normalTexture;
    Holder<TextureHandle> occlusionTexture;
    Holder<TextureHandle> emissiveTexture;
    // Additional textures for extensions...
};
```

### GPU Material Data Structure

```cpp
struct MetallicRoughnessDataGPU {
    vec4 baseColorFactor = vec4(1.0f);
    // Packed: metallicFactor, roughnessFactor, normalScale, occlusionStrength
    vec4 metallicRoughnessNormalOcclusion = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    // Packed: emissiveFactor.rgb, alphaCutoff
    vec4 emissiveFactorAlphaCutoff = vec4(0.0f, 0.0f, 0.0f, 0.5f);
    
    uint32_t baseColorTexture;
    uint32_t baseColorTextureSampler;
    uint32_t baseColorTextureUV;
    // ... more texture references
};
```

### PBRInfo Structure (GLSL)

```glsl
struct PBRInfo {
    float NdotL;  // cos angle between normal and light direction
    float NdotV;  // cos angle between normal and view direction
    float NdotH;  // cos angle between normal and half vector
    float LdotH;  // cos angle between light dir and half vector
    float VdotH;  // cos angle between view dir and half vector
    vec3 n;       // normal at surface point
    vec3 v;       // vector from surface point to camera
    
    float perceptualRoughness;
    vec3 reflectance0;    // full reflectance color (F0)
    vec3 reflectance90;   // reflectance at grazing angle
    float alphaRoughness; // remapped linear roughness
    vec3 diffuseColor;
    vec3 specularColor;
};
```

### Fragment Shader Main

```glsl
void main() {
    MetallicRoughnessDataGPU mat = getMaterial(getMaterialId());
    
    // Sample textures
    vec4 Kao = sampleAO(tc, mat);
    vec4 Ke = sampleEmissive(tc, mat);
    vec4 Kd = sampleAlbedo(tc, mat) * color;
    vec4 mrSample = sampleMetallicRoughness(tc, mat);
    
    // Calculate normal with normal mapping
    vec3 n = normalize(normal);
    vec3 normalSample = sampleNormal(tc, getMaterialId()).xyz;
    n = perturbNormal(n, worldPos, normalSample, getNormalUV(tc, mat));
    if (!gl_FrontFacing) n *= -1.0f;
    
    // Calculate PBR inputs
    PBRInfo pbrInputs = calculatePBRInputsMetallicRoughness(
        Kd, n, perFrame.drawable.cameraPos.xyz, worldPos, mrSample);
    
    // IBL contributions
    vec3 specular_color = getIBLRadianceContributionGGX(pbrInputs, 1.0);
    vec3 diffuse_color = getIBLRadianceLambertian(
        pbrInputs.NdotV, n, pbrInputs.perceptualRoughness,
        pbrInputs.diffuseColor, pbrInputs.reflectance0, 1.0);
    vec3 color = specular_color + diffuse_color;
    
    // Directional light contribution
    vec3 lightPos = vec3(0, 0, -5);
    color += calculatePBRLightContribution(
        pbrInputs, normalize(lightPos - worldPos), vec3(1.0));
    
    // Apply AO and emissive
    color = color * (Kao.r < 0.01 ? 1.0 : Kao.r);
    color = pow(Ke.rgb + color, vec3(1.0/2.2)); // Gamma correction
    
    out_FragColor = vec4(color, 1.0);
}
```

### IBL Functions

```glsl
// sRGB to Linear conversion
vec4 SRGBtoLINEAR(vec4 srgbIn) {
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut, srgbIn.a);
}

// Schlick Fresnel approximation
vec3 fresnelSchlick(vec3 f0, vec3 f90, float VdotH) {
    return f0 + (f90 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// IBL Lambertian (diffuse)
vec3 getIBLRadianceLambertian(float NdotV, vec3 n, float roughness,
                               vec3 diffuseColor, vec3 F0, float specularWeight)
{
    vec2 brdfSamplePoint = clamp(vec2(NdotV, roughness), vec2(0), vec2(1));
    vec2 f_ab = textureBindless2D(brdfLUT, samplerIdx, brdfSamplePoint).rg;
    
    vec3 irradiance = textureBindlessCube(irradianceMap, samplerIdx, n).rgb;
    
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 k_S = F0 + Fr * pow(1.0 - NdotV, 5.0);
    vec3 FssEss = specularWeight * k_S * f_ab.x + f_ab.y;
    
    float Ems = (1.0 - (f_ab.x + f_ab.y));
    vec3 F_avg = specularWeight * (F0 + (1.0 - F0) / 21.0);
    vec3 FmsEms = Ems * FssEss * F_avg / (1.0 - F_avg * Ems);
    vec3 k_D = diffuseColor * (1.0 - FssEss + FmsEms);
    
    return (FmsEms + k_D) * irradiance;
}

// IBL GGX (specular)
vec3 getIBLRadianceContributionGGX(PBRInfo pbrInputs, float specularWeight) {
    float lod = pbrInputs.perceptualRoughness * float(envMapMipLevels - 1);
    vec3 reflection = normalize(reflect(-pbrInputs.v, pbrInputs.n));
    
    vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, pbrInputs.perceptualRoughness),
                                  vec2(0), vec2(1));
    vec2 f_ab = textureBindless2D(brdfLUT, samplerIdx, brdfSamplePoint).rg;
    vec3 specularLight = textureBindlessCubeLod(envMap, samplerIdx, reflection, lod).rgb;
    
    vec3 Fr = max(vec3(1.0 - pbrInputs.perceptualRoughness), pbrInputs.reflectance0) 
              - pbrInputs.reflectance0;
    vec3 k_S = pbrInputs.reflectance0 + Fr * pow(1.0 - pbrInputs.NdotV, 5.0);
    vec3 FssEss = k_S * f_ab.x + f_ab.y;
    
    return specularWeight * specularLight * FssEss;
}
```

---

## Recipe 6: Specular-Glossiness Shading Model

Alternative to metallic-roughness using:
- **Diffuse color** (instead of base color)
- **Specular color** (instead of metallic)
- **Glossiness** (instead of roughness, inverse relationship)

### Key Differences

| Metallic-Roughness | Specular-Glossiness |
|-------------------|---------------------|
| baseColorFactor | diffuseFactor |
| metallicFactor (0-1) | specularFactor (RGB) |
| roughnessFactor (0-1) | glossinessFactor (0-1) |

---

## Source Code Locations

| Recipe | Demo Location |
|--------|---------------|
| Unlit Materials | Chapter06/01_Unlit |
| BRDF LUT | Chapter06/02_BRDF_LUT |
| Environment Filtering | Chapter06/03_FilterEnvmap |
| Metallic-Roughness | Chapter06/04_MetallicRoughness |
| Specular-Glossiness | Chapter06/05_SpecularGlossiness |

## Key Shared Files

- `shared/UtilsGLTF.h` - glTF helper structures and functions
- `data/shaders/UtilsPBR.sp` - PBR helper functions
- `Chapter06/04_MetallicRoughness/src/PBR.sp` - Main PBR implementation

## Required Precomputed Data

1. `data/brdfLUT.ktx` - BRDF look-up table
2. `data/piazza_bologni_1k_prefilter.ktx` - GGX prefiltered environment
3. `data/piazza_bologni_1k_irradiance.ktx` - Lambertian irradiance map
4. `data/piazza_bologni_1k_charlie.ktx` - Charlie (sheen) prefiltered map
