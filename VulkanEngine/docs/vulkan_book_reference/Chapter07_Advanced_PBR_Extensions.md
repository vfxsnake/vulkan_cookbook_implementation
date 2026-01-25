# Chapter 7: Advanced PBR Extensions

This chapter covers advanced glTF PBR extensions that build upon the base metallic-roughness model. While the base metallic-roughness model provides a starting point, it falls short of capturing the full spectrum of real-life materials. glTF incorporates additional material layers, each with specific parameters that define their unique behaviors.

## Recipes in This Chapter

1. Introduction to glTF PBR extensions
2. Implementing the KHR_materials_clearcoat extension
3. Implementing the KHR_materials_sheen extension
4. Implementing the KHR_materials_transmission extension
5. Implementing the KHR_materials_volume extension
6. Implementing the KHR_materials_ior extension
7. Implementing the KHR_materials_specular extension
8. Implementing the KHR_materials_emissive_strength extension
9. Extending analytical lights support with KHR_lights_punctual

> **Note:** GLSL shader code is based on the official Khronos Sample Viewer and serves as an example implementation of these extensions.

---

## 1. Introduction to glTF PBR Extensions

### How the glTF 2.0 PBR Model is Designed

Khronos introduced a **layered approach** (like an onion) rather than simply extending the Metallic-Roughness model. This method lets you gradually add complexity to PBR materials, similar to how layers are built in **Adobe Standard Surface**.

### Layering Concepts

- **Base Layer**: Should be either fully opaque (metallic surfaces) or completely transparent (glass, skin)
- **Dielectric Slabs**: Additional layers added on top one by one
- **Light Interaction**: When light hits layer boundaries, it can reflect or continue through the material stack
- **Absorption**: Light passing through lower layers may be absorbed by the material

### Mixing Operation

- Statistically weighted blend of two different materials
- When done as linear interpolation, it follows energy conservation
- Not all combinations are physically realistic (e.g., mixing oil and water)

### Reference Resources

- Khronos ratified extensions: https://github.com/KhronosGroup/glTF/blob/main/extensions/README.md
- Adobe Standard Surface: https://github.com/Autodesk/standard-surface

---

## 2. KHR_materials_clearcoat Extension

### Overview

The clearcoat extension adds a thin, clear layer on top of the base material. Common real-world applications include:
- Car paint with clear lacquer
- Coated wood floors
- Carbon fiber with protective coating

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `clearcoatFactor` | float [0-1] | Strength of clearcoat layer |
| `clearcoatTexture` | texture | Modulates clearcoat factor |
| `clearcoatRoughnessFactor` | float [0-1] | Roughness of clearcoat layer |
| `clearcoatRoughnessTexture` | texture | Modulates clearcoat roughness |
| `clearcoatNormalTexture` | texture | Normal map for clearcoat layer |

### Implementation Notes

#### C++ Material Structure (shared/UtilsGLTF.h)

```cpp
struct GLTFMaterialDataGPU {
    // ...
    vec4 clearcoatTransmissionThickness = vec4(0.0f);  // x=clearcoat, y=clearcoatRoughness
    uint32_t clearcoatTexture = 0;
    uint32_t clearcoatTextureSampler = 0;
    uint32_t clearcoatTextureUV = 0;
    uint32_t clearcoatRoughnessTexture = 0;
    uint32_t clearcoatRoughnessTextureSampler = 0;
    uint32_t clearcoatRoughnessTextureUV = 0;
    uint32_t clearcoatNormalTexture = 0;
    uint32_t clearcoatNormalTextureSampler = 0;
    uint32_t clearcoatNormalTextureUV = 0;
    // ...
};
```

#### GLSL Implementation

Add to `PBRInfo` structure:
```glsl
struct PBRInfo {
    // ... existing fields ...
    float clearcoatFactor;
    float clearcoatRoughness;
    vec3 clearcoatNormal;
    vec3 clearcoatF0;
    vec3 clearcoatF90;
};
```

Clearcoat contribution calculation:
```glsl
vec3 clearCoatContrib = vec3(0);
if (isClearCoat) {
    clearCoatContrib = getIBLRadianceContributionGGX(
        pbrInputs.clearcoatNormal, pbrInputs.v,
        pbrInputs.clearcoatRoughness,
        pbrInputs.clearcoatF0, 1.0, envMap);
}

// Fresnel calculation for clearcoat
vec3 clearcoatFresnel = vec3(0);
if (isClearCoat) {
    clearcoatFresnel = F_Schlick(
        pbrInputs.clearcoatF0,
        pbrInputs.clearcoatF90,
        clampedDot(pbrInputs.clearcoatNormal, pbrInputs.v));
}

// Final color with clearcoat on top of all layers
vec3 color = specularColor + diffuseColor + emissiveColor + sheenColor;
color = color * (1.0 - pbrInputs.clearcoatFactor * clearcoatFresnel) + clearCoatContrib;
```

### Source Code Location

- Demo: `Chapter07/01_Clearcoat/`
- Shared utilities: `shared/UtilsGLTF.cpp`
- Shaders: `data/shaders/gltf/`

---

## 3. KHR_materials_sheen Extension

### Overview

Simulates the sheen effect found on fabrics like satin or brushed metals. Creates more realistic and visually appealing sheen highlights.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `sheenColorFactor` | vec3 | Base intensity of sheen effect (0 disables) |
| `sheenColorTexture` | texture | sRGB texture multiplied by factor |
| `sheenRoughnessFactor` | float | Controls fiber alignment |
| `sheenRoughnessTexture` | texture | Alpha channel modulates roughness |

### Sheen BRDF

- Based on exponentiated sinusoidal distribution (Conty & Kulla, 2017)
- Models light scattering off tiny fibers perpendicular to surface
- **Lower roughness**: Aligned fibers → sharper sheen highlight
- **Higher roughness**: Scattered fibers → softer sheen highlight

### Implementation Notes

#### C++ Material Structure

```cpp
struct GLTFMaterialDataGPU {
    // ...
    vec4 sheenFactors = vec4(1.0f, 1.0f, 1.0f, 1.0f);  // xyz=color, w=roughness
    uint32_t sheenColorTexture = 0;
    uint32_t sheenColorTextureSampler = 0;
    uint32_t sheenColorTextureUV = 0;
    uint32_t sheenRoughnessTexture = 0;
    uint32_t sheenRoughnessTextureSampler = 0;
    uint32_t sheenRoughnessTextureUV = 0;
    // ...
};
```

#### GLSL Implementation

IBL Radiance for Charlie distribution:
```glsl
vec3 getIBLRadianceCharlie(PBRInfo pbrInputs, EnvironmentMapDataGPU envMap) {
    float sheenRoughness = pbrInputs.sheenRoughnessFactor;
    vec3 sheenColor = pbrInputs.sheenColorFactor;
    float mipCount = float(sampleEnvMapQueryLevels(envMap));
    float lod = sheenRoughness * float(mipCount - 1);
    vec3 reflection = normalize(reflect(-pbrInputs.v, pbrInputs.n));
    vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, sheenRoughness), 
                                  vec2(0.0, 0.0), vec2(1.0, 1.0));
    float brdf = sampleBRDF_LUT(brdfSamplePoint, envMap).b;  // Blue channel for sheen
    vec3 sheenSample = sampleCharlieEnvMapLod(reflection.xyz, lod, envMap).rgb;
    return sheenSample * sheenColor * brdf;
}
```

### Source Code Location

- Demo: `Chapter07/02_Sheen/`
- Sample model: `deps/src/glTF-Sample-Assets/Models/SheenChair/glTF/SheenChair.gltf`

### Reference

- Conty & Kulla, 2017: https://blog.selfshadow.com/publications/s2017-shading-course/#course_content

---

## 4. KHR_materials_transmission Extension

### Overview

Enables realistic rendering of thin transparent materials like glass or plastic. Overcomes limitations of alpha-as-coverage which only decides if a surface exists (0/1) rather than simulating complex light interactions.

### Key Differences from Alpha-as-Coverage

| Aspect | Alpha-as-Coverage | Transmission |
|--------|-------------------|--------------|
| Light handling | Binary (exists/not) | Reflection, refraction, absorption |
| Reflections | Weaker when transparent | Strong even when see-through |
| Use case | Gauze, burlap | Glass, plastic |

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `transmissionFactor` | float [0-1] | 0=opaque, 1=fully transparent |
| `transmissionTexture` | texture | Modulates transmission factor |
| `transmissionFilter` | vec3 | Alters color of transmitted light |

### Transmission BTDF

- Uses **Bidirectional Transmission Distribution Function** based on microfacet model
- Same Trowbridge-Reitz distribution as specular BRDF
- Samples along view vector instead of reflection direction
- Models two back-to-back surfaces representing thin material

### Implementation Notes

This is the most complex extension, requiring significant C++ changes for transparency rendering.

#### Rendering Order

1. Render opaque objects first
2. Render transparent objects using off-screen framebuffer as transmission source
3. Sample background through the material based on roughness

#### GLSL Key Functions

```glsl
// Sample transmitted color with roughness-based blur
vec3 getTransmissionSample(vec2 fragCoord, float roughness, float ior) {
    float framebufferLod = log2(float(textureSize)) * applyIorToRoughness(roughness, ior);
    vec3 transmittedLight = textureLod(transmissionFramebuffer, fragCoord, framebufferLod).rgb;
    return transmittedLight;
}

// Apply IOR to roughness for refraction simulation
float applyIorToRoughness(float roughness, float ior) {
    return roughness * clamp(ior * 2.0 - 2.0, 0.0, 1.0);
}
```

### Source Code Location

- Demo: `Chapter07/03_Transmission/`
- Sample model: `deps/src/glTF-Sample-Assets/Models/TransmissionRoughnessTest/glTF/`

---

## 5. KHR_materials_volume Extension

### Overview

Works with the transmission extension to simulate light behavior inside thick, transparent materials. Adds volume absorption and attenuation effects.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `thicknessFactor` | float | Material thickness in scene units |
| `thicknessTexture` | texture | Per-pixel thickness variation |
| `attenuationDistance` | float | Distance where light reduces to attenuationColor |
| `attenuationColor` | vec3 | Color remaining after traveling attenuationDistance |

### Beer-Lambert Law

Volume absorption follows Beer-Lambert law:
```glsl
vec3 applyVolumeAttenuation(vec3 radiance, float transmissionDistance, 
                             vec3 attenuationColor, float attenuationDistance) {
    if (attenuationDistance == 0.0) return radiance;
    
    vec3 attenuationCoefficient = -log(attenuationColor) / attenuationDistance;
    vec3 transmittance = exp(-attenuationCoefficient * transmissionDistance);
    return radiance * transmittance;
}
```

### Source Code Location

- Demo: `Chapter07/04_Volume/`

---

## 6. KHR_materials_ior Extension

### Overview

Allows explicit control over the **Index of Refraction** (IOR) for dielectric materials. Default IOR in glTF is 1.5 (typical for plastics/glass).

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `ior` | float | 1.5 | Index of refraction |

### Common IOR Values

| Material | IOR |
|----------|-----|
| Air | 1.0 |
| Water | 1.33 |
| Glass | 1.5 |
| Diamond | 2.42 |
| Amber | 1.55 |

### Implementation

The IOR affects F0 (specular reflectance at normal incidence):

```glsl
// Calculate F0 from IOR
vec3 f0 = isSpecularGlossiness ?
    getSpecularFactor(mat) * mrSample.rgb :
    vec3(pow((pbrInputs.ior - 1.0) / (pbrInputs.ior + 1.0), 2.0));

// For clearcoat
if (isClearCoat) {
    pbrInputs.clearcoatF0 = vec3(
        pow((pbrInputs.ior - 1.0) / (pbrInputs.ior + 1.0), 2.0));
}
```

### Source Code Location

- Demo: `Chapter07/05_IOR/`

---

## 7. KHR_materials_specular Extension

### Overview

Provides precise control over specular reflections, replacing the deprecated KHR_materials_pbrSpecularGlossiness extension. Compatible with most other glTF PBR extensions.

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `specularFactor` | float | Scales overall specular reflection intensity |
| `specularTexture` | texture | Alpha channel modulates specularFactor |
| `specularColorFactor` | vec3 | Modifies specular reflection color |
| `specularColorTexture` | texture | RGB modulates specularColorFactor |

### Implementation Notes

#### C++ Material Loading

```cpp
ai_real specularFactor;
if (mtlDescriptor->Get(AI_MATKEY_SPECULAR_FACTOR, specularFactor) == AI_SUCCESS) {
    res.specularFactors.w = specularFactor;
    useSpecular = true;
}
aiColor4D specularColorFactor;
if (mtlDescriptor->Get(AI_MATKEY_COLOR_SPECULAR, specularColorFactor) == AI_SUCCESS) {
    res.specularFactors = vec4(specularColorFactor.r, specularColorFactor.g, 
                                specularColorFactor.b, res.specularFactors.w);
    useSpecular = true;
}
```

#### GLSL Specular Contribution

```glsl
vec3 getIBLRadianceContributionGGX(PBRInfo pbrInputs, float specularWeight, 
                                    EnvironmentMapDataGPU envMap) {
    vec3 n = pbrInputs.n;
    vec3 v = pbrInputs.v;
    vec3 reflection = normalize(reflect(-v, n));
    float mipCount = float(sampleEnvMapQueryLevels(envMap));
    float lod = pbrInputs.perceptualRoughness * (mipCount - 1);
    vec2 brdfSamplePoint = clamp(vec2(pbrInputs.NdotV, pbrInputs.perceptualRoughness),
                                  vec2(0.0, 0.0), vec2(1.0, 1.0));
    vec3 brdf = sampleBRDF_LUT(brdfSamplePoint, envMap).rgb;
    vec3 specularLight = sampleEnvMapLod(reflection.xyz, lod, envMap).rgb;
    vec3 Fr = max(vec3(1.0 - pbrInputs.perceptualRoughness), pbrInputs.reflectance0) 
              - pbrInputs.reflectance0;
    vec3 k_S = pbrInputs.reflectance0 + Fr * pow(1.0 - pbrInputs.NdotV, 5.0);
    vec3 FssEss = k_S * brdf.x + brdf.y;
    return specularWeight * specularLight * FssEss;
}
```

### Source Code Location

- Demo: `Chapter07/06_Specular/`
- Reference: https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_specular/README.md

---

## 8. KHR_materials_emissive_strength Extension

### Overview

Provides precise control over the intensity of a material's emitted light. Before this extension, controlling emission intensity was difficult.

### Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `emissiveStrength` | float | 1.0 | Multiplier for emissive color |

### Implementation

Simply multiply emissive factors by strength:

```cpp
ai_real emissiveStrength = 1.0f;
if (mtlDescriptor->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissiveStrength) == AI_SUCCESS) {
    res.emissiveFactorAlphaCutoff *= vec4(emissiveStrength, emissiveStrength, 
                                           emissiveStrength, 1.0);
}
```

### Source Code Location

- Demo: `Chapter07/07_EmissiveStrength/`
- Sample model: `deps/src/glTF-Sample-Assets/Models/EmissiveStrengthTest/glTF/`

---

## 9. KHR_lights_punctual Extension

### Overview

Adds support for analytical (punctual) light sources to glTF assets. These terms are used interchangeably:
- **Analytical Light**: Light defined by mathematical equations
- **Punctual Light**: Infinitely small point emitting light in specific directions

### Light Types

```cpp
enum LightType : uint32_t {
    LightType_Directional = 0,
    LightType_Point = 1,
    LightType_Spot = 2,
};
```

### Light Data Structure

```cpp
struct LightDataGPU {
    vec3 direction = vec3(0, 0, 1);
    float range = 10000.0;
    vec3 color = vec3(1, 1, 1);
    float intensity = 1.0;
    vec3 position = vec3(0, 0, -5);
    float innerConeCos = 0.0;      // For spot lights
    float outerConeCos = 0.78;     // For spot lights
    LightType type = LightType_Directional;
    int padding[2];
};
```

### IBL vs Punctual Lights

| Aspect | Image-Based Lighting (IBL) | Punctual Lights |
|--------|---------------------------|-----------------|
| Source | Pre-computed environment maps | Specific light sources |
| Integration | Hemisphere sampling | Per-light calculation |
| Shadows | Not inherent | Must check visibility |
| Performance | Generally faster | More expensive per light |

### GLSL Implementation

#### Light Intensity Calculation

```glsl
vec3 getLightIntensity(Light light, vec3 pointToLight) {
    float rangeAttenuation = 1.0;
    float spotAttenuation = 1.0;
    
    if (light.type != LightType_Directional) {
        rangeAttenuation = getRangeAttenuation(light.range, length(pointToLight));
    }
    if (light.type == LightType_Spot) {
        spotAttenuation = getSpotAttenuation(pointToLight, light.direction, 
                                              light.outerConeCos, light.innerConeCos);
    }
    return rangeAttenuation * spotAttenuation * light.intensity * light.color;
}
```

#### Light Loop in Fragment Shader

```glsl
vec3 lights_diffuse = vec3(0);
vec3 lights_specular = vec3(0);
vec3 lights_sheen = vec3(0);
vec3 lights_clearcoat = vec3(0);
vec3 lights_transmission = vec3(0);

for (int i = 0; i < getLightsCount(); ++i) {
    Light light = getLight(i);
    vec3 l = normalize(pointToLight);
    vec3 h = normalize(l + v);
    
    float NdotL = clampedDot(n, l);
    float NdotV = clampedDot(n, v);
    float NdotH = clampedDot(n, h);
    float LdotH = clampedDot(l, h);
    float VdotH = clampedDot(v, h);
    
    if (NdotL > 0.0 || NdotV > 0.0) {
        vec3 intensity = getLightIntensity(light, pointToLight);
        
        lights_diffuse += intensity * NdotL *
            getBRDFLambertian(pbrInputs.reflectance0, pbrInputs.reflectance90,
                              pbrInputs.diffuseColor, pbrInputs.specularWeight, VdotH);
        
        lights_specular += intensity * NdotL *
            getBRDFSpecularGGX(pbrInputs.reflectance0, pbrInputs.reflectance90,
                               pbrInputs.alphaRoughness, pbrInputs.specularWeight,
                               VdotH, NdotL, NdotV, NdotH);
        
        if (isSheen) {
            lights_sheen += intensity * getPunctualRadianceSheen(...);
        }
        
        if (isClearCoat) {
            lights_clearcoat += intensity * getPunctualRadianceClearCoat(...);
        }
    }
}
```

### Performance Note

> Evaluating all lights for every object can be costly. Alternatives like **clustered shading** or **deferred shading** can help improve performance.

### Source Code Location

- Demo: `Chapter07/08_AnalyticalLight/`
- Sample model: `deps/src/glTF-Sample-Assets/Models/LightsPunctualLamp/glTF/`

---

## Key Shader Files

| File | Purpose |
|------|---------|
| `data/shaders/gltf/main.frag` | Main fragment shader with all extensions |
| `data/shaders/gltf/PBR.sp` | PBR utility functions (BRDF, IBL, etc.) |
| `data/shaders/gltf/inputs.frag` | Material input accessor functions |
| `shared/UtilsGLTF.h` | C++ material structures |
| `shared/UtilsGLTF.cpp` | Material loading via Assimp |

---

## Material Type Flags

```cpp
enum MaterialType : uint32_t {
    MaterialType_Invalid = 0,
    MaterialType_Unlit = 1,
    MaterialType_MetallicRoughness = 2,
    MaterialType_SpecularGlossiness = 4,
    MaterialType_Sheen = 8,
    MaterialType_ClearCoat = 16,
    MaterialType_Specular = 32,
    MaterialType_Transmission = 64,
    MaterialType_Volume = 128,
    MaterialType_Ior = 256,
};
```

---

## Session Targets for Implementation

### Session 1: Clearcoat Extension (2 hours)
- [ ] Add clearcoat material parameters to `GLTFMaterialDataGPU`
- [ ] Implement clearcoat loading via Assimp
- [ ] Add GLSL clearcoat contribution calculation
- [ ] Test with clearcoat sample model

### Session 2: Sheen Extension (2 hours)
- [ ] Add sheen material parameters
- [ ] Implement Charlie distribution IBL sampling
- [ ] Use pre-computed Charlie BRDF LUT (blue channel)
- [ ] Test with SheenChair model

### Session 3: Transmission Extension (3 hours)
- [ ] Understand BTDF vs BRDF
- [ ] Implement off-screen framebuffer for transmission source
- [ ] Add transmission sampling with roughness-based blur
- [ ] Implement proper render ordering (opaque → transparent)

### Session 4: Volume Extension (2 hours)
- [ ] Add volume parameters (thickness, attenuation)
- [ ] Implement Beer-Lambert absorption
- [ ] Combine with transmission for thick materials

### Session 5: IOR & Specular Extensions (2 hours)
- [ ] Add IOR parameter and F0 calculation
- [ ] Implement specular factor/color parameters
- [ ] Test specular weight in IBL contributions

### Session 6: Emissive Strength & Punctual Lights (2 hours)
- [ ] Add emissive strength multiplier
- [ ] Implement light data structures
- [ ] Add light loop in fragment shader
- [ ] Test with LightsPunctualLamp model

---

## References

- Khronos glTF Extensions: https://github.com/KhronosGroup/glTF/blob/main/extensions/README.md
- Khronos glTF Sample Viewer: https://github.com/KhronosGroup/glTF-Sample-Viewer
- Conty & Kulla 2017 (Sheen): https://blog.selfshadow.com/publications/s2017-shading-course/
- Don McCurdy on Specular-Glossiness: https://www.donmccurdy.com/2022/11/28/converting-gltf-pbr-materials-from-specgloss-to-metalrough
