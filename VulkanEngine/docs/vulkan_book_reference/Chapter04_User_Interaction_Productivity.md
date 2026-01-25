# Chapter 4: Adding User Interaction and Productivity Tools

## Overview

This chapter covers implementing basic helpers to simplify debugging of graphical applications. All demos use Vulkan with material from the previous three chapters. Focus is on creating extensible and adaptable Vulkan rendering code starting with 2D user-interface rendering.

## Technical Requirements

- GPU with recent drivers supporting Vulkan 1.3
- Source code: https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition/

## Recipes Covered

1. Rendering ImGui user interfaces
2. Integrating Tracy into C++ applications
3. Using Tracy GPU profiling
4. Adding a frames-per-second counter
5. Using cube map textures in Vulkan
6. Working with a 3D camera and basic user interaction
7. Adding camera animations and motion
8. Implementing an immediate-mode 3D drawing canvas
9. Rendering on-screen graphs with ImGui and ImPlot
10. Putting it all together into a Vulkan application

---

## Recipe 1: Rendering ImGui User Interfaces

ImGui is a popular bloat-free graphical user interface library for C++ essential for interactive debugging.

### Key Implementation Steps

1. **Create ImGuiRenderer object**:
```cpp
unique_ptr<lvk::ImGuiRenderer> imgui =
    std::make_unique<lvk::ImGuiRenderer>(
        *ctx, "data/OpenSans-Light.ttf", 30.0f);
```

2. **Set up GLFW callbacks for mouse input**:
```cpp
glfwSetCursorPosCallback(window,
    [](auto* window, double x, double y) {
        ImGui::GetIO().MousePos = ImVec2(x, y);
    });
glfwSetMouseButtonCallback(window, /* mouse button handling */);
```

3. **Render loop**:
```cpp
ICommandBuffer& buf = ctx->acquireCommandBuffer();
const lvk::Framebuffer framebuffer = {
    .color = {{ .texture = ctx->getCurrentSwapchainTexture() }}};
buf.cmdBeginRendering({ .color = { {
    .loadOp = lvk::LoadOp_Clear,
    .clearColor = {1.0f, 1.0f, 1.0f, 1.0f} } } },
    framebuffer);
imgui->beginFrame(framebuffer);
// ImGui commands here
imgui->endFrame(buf);
buf.cmdEndRendering();
ctx->submit(buf, ctx->getCurrentSwapchainTexture());
```

### Key Classes

- `lvk::ImGuiRenderer` - Helper class in `lvk/HelpersImGui.h`
- Methods: `beginFrame()`, `endFrame()`, `setDisplayScale()`, `updateFont()`

### Important Notes

- Uses bindless rendering scheme with texture indices passed as `ImTextureID`
- Multiple buffers used for stall-free operation (vertex and index buffers per frame)
- Host-visible memory needs flushing via `ctx->flushMappedMemory()`

---

## Recipe 2: Integrating Tracy into C++ Applications

Tracy profiler integration for runtime performance profiling.

### CMake Configuration

```cmake
option(LVK_WITH_TRACY "Enable Tracy profiler" ON)
if(LVK_WITH_TRACY)
    add_definitions("-DTRACY_ENABLE=1")
    add_subdirectory(third-party/deps/src/tracy)
endif()
```

### Key Macros

```cpp
#define LVK_PROFILER_COLOR_WAIT 0xff0000
#define LVK_PROFILER_COLOR_SUBMIT 0x0000ff
#define LVK_PROFILER_COLOR_PRESENT 0x00ff00
#define LVK_PROFILER_COLOR_CREATE 0xff6600
#define LVK_PROFILER_COLOR_DESTROY 0xffa500

#define LVK_PROFILER_FUNCTION() ZoneScoped
#define LVK_PROFILER_FUNCTION_COLOR(color) ZoneScopedC(color)
#define LVK_PROFILER_ZONE(name, color) { ZoneScopedC(color); ZoneName(name, strlen(name))
#define LVK_PROFILER_ZONE_END() }
#define LVK_PROFILER_THREAD(name) tracy::SetThreadName(name)
#define LVK_PROFILER_FRAME(name) FrameMarkNamed(name)
```

### Usage Example

```cpp
{
    LVK_PROFILER_ZONE("Initialization", LVK_PROFILER_COLOR_CREATE);
    // initialization code
    LVK_PROFILER_ZONE_END();
}
```

---

## Recipe 3: Tracy GPU Profiling

Uses Vulkan timestamps for GPU command timing.

### Key Structures

```cpp
struct VulkanContextImpl final {
#if defined(LVK_WITH_TRACY_GPU)
    TracyVkCtx tracyVkCtx_ = nullptr;
    VkCommandPool tracyCommandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer tracyCommandBuffer_ = VK_NULL_HANDLE;
#endif
};
```

### Initialization

- Checks for `VK_EXT_calibrated_timestamps` extension
- Uses `TracyVkContextHostCalibrated` when host querying is supported
- Creates dedicated command pool/buffer when not supported

---

## Recipe 4: FPS Counter

### FPSCounter Class

```cpp
class FPSCounter {
public:
    explicit FPSCounter(float avgInterval = 0.5f);
    bool tick(float deltaSeconds, bool frameRendered = true);
    float getFPS() const { return currentFPS_; }
private:
    float avgInterval_ = 0.5f;
    unsigned numFrames_ = 0;
    double accumulatedTime_ = 0;
    float currentFPS_ = 0.0f;
};
```

### ImGui Rendering

```cpp
if (const ImGuiViewport* v = ImGui::GetMainViewport()) {
    ImGui::SetNextWindowPos({
        v->WorkPos.x + v->WorkSize.x - 15.0f,
        v->WorkPos.y + 15.0f },
        ImGuiCond_Always, { 1.0f, 0.0f });
}
ImGui::SetNextWindowBgAlpha(0.30f);
ImGui::Text("FPS : %i", (int)fpsCounter.getFPS());
```

---

## Recipe 5: Cube Map Textures in Vulkan

### Key Concepts

- Cube maps contain 6 individual 2D textures forming sides of a cube
- Sampled using direction vectors
- Useful for storing diffuse part of PBR lighting (irradiance cube maps)
- Common formats: equirectangular projections, vertical/horizontal crosses

### Bitmap Helper Class

```cpp
class Bitmap {
public:
    int w_ = 0, h_ = 0, d_ = 1, comp_ = 3;
    eBitmapFormat fmt_ = eBitmapFormat_UnsignedByte;
    eBitmapType type_ = eBitmapType_2D;
    std::vector<uint8_t> data_;
    
    static int getBytesPerComponent(eBitmapFormat fmt);
    void setPixel(int x, int y, const glm::vec4& c);
    glm::vec4 getPixel(int x, int y) const;
};
```

### Conversion Functions

```cpp
Bitmap convertEquirectangularMapToVerticalCross(const Bitmap& b);
Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap& out);
```

### Loading HDR Cube Map

```cpp
int w, h;
const float* img = stbi_loadf("data/piazza_bologni_1k.hdr", &w, &h, nullptr, 4);
Bitmap in(w, h, 4, eBitmapFormat_Float, img);
Bitmap out = convertEquirectangularMapToVerticalCross(in);
Bitmap cubemap = convertVerticalCrossToCubeMapFaces(out);

Holder<TextureHandle> cubemapTex = ctx->createTexture({
    .type = lvk::TextureType_Cube,
    .format = lvk::Format_RGBA_F32,
    .dimensions = {(uint32_t)cubemap.w_, (uint32_t)cubemap.h_},
    .usage = lvk::TextureUsageBits_Sampled,
    .data = cubemap.data_.data(),
});
```

---

## Recipe 6: 3D Camera and User Interaction

### CameraPositioner Interface

```cpp
class CameraPositionerInterface {
public:
    virtual glm::mat4 getViewMatrix() const = 0;
    virtual glm::vec3 getPosition() const = 0;
};
```

### First-Person Camera Positioner

```cpp
class CameraPositioner_FirstPerson final : public CameraPositionerInterface {
public:
    struct Movement {
        bool forward_ = false;
        bool backward_ = false;
        bool left_ = false;
        bool right_ = false;
        bool up_ = false;
        bool down_ = false;
        bool fastSpeed_ = false;
    } movement_;

    void update(double deltaSeconds, const glm::vec2& mousePos, bool mousePressed);
    void setSpeed(const glm::vec3& speed);
    void lookAt(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up);
    
private:
    glm::vec2 mousePos_ = glm::vec2(0);
    glm::vec3 cameraPosition_ = glm::vec3(0.0f);
    glm::quat cameraOrientation_ = glm::quat(glm::vec3(0));
    glm::vec3 moveSpeed_ = glm::vec3(0.0f);
    float mouseSpeed_ = 4.0f;
    float acceleration_ = 150.0f;
    float damping_ = 0.2f;
    float maxSpeed_ = 10.0f;
    float fastCoef_ = 10.0f;
};
```

### Camera Class

```cpp
class Camera final {
public:
    explicit Camera(CameraPositionerInterface& positioner);
    glm::mat4 getViewMatrix() const;
    glm::vec3 getPosition() const;
private:
    const CameraPositionerInterface& positioner_;
};
```

### GLFW Input Setup

```cpp
glfwSetKeyCallback(window,
    [](GLFWwindow* window, int key, int, int action, int mods) {
        const bool press = action != GLFW_RELEASE;
        if (key == GLFW_KEY_W) positioner.movement_.forward_ = press;
        if (key == GLFW_KEY_S) positioner.movement_.backward_ = press;
        if (key == GLFW_KEY_A) positioner.movement_.left_ = press;
        if (key == GLFW_KEY_D) positioner.movement_.right_ = press;
        if (key == GLFW_KEY_1) positioner.movement_.up_ = press;
        if (key == GLFW_KEY_2) positioner.movement_.down_ = press;
        if (mods & GLFW_MOD_SHIFT) positioner.movement_.fastSpeed_ = press;
    });
```

---

## Recipe 7: Camera Animations and Motion

### CameraPositioner_MoveTo

```cpp
class CameraPositioner_MoveTo final : public CameraPositionerInterface {
public:
    float dampingLinear_ = 10.0f;
    vec3 dampingEulerAngles_ = vec3(5.0f, 5.0f, 5.0f);
    
    void update(float deltaSeconds, const vec2& mousePos, bool mousePressed);
    void setDesiredPosition(const vec3& pos);
    void setDesiredAngles(const vec3& angles);
    
private:
    vec3 positionCurrent_, positionDesired_;
    vec3 anglesCurrent_, anglesDesired_; // pitch, pan, roll
    mat4 currentTransform_;
};
```

### Movement Implementation

```cpp
void update(float deltaSeconds, const vec2& mousePos, bool mousePressed) {
    positionCurrent_ += dampingLinear_ * deltaSeconds * 
        (positionDesired_ - positionCurrent_);
    
    anglesCurrent_ = clipAngles(anglesCurrent_);
    anglesDesired_ = clipAngles(anglesDesired_);
    anglesCurrent_ -= angleDelta(anglesCurrent_, anglesDesired_) * 
        dampingEulerAngles_ * deltaSeconds;
    
    const vec3 ang = glm::radians(anglesCurrent_);
    currentTransform_ = glm::translate(
        glm::yawPitchRoll(ang.y, ang.x, ang.z), -positionCurrent_);
}
```

---

## Recipe 8: Immediate-Mode 3D Drawing Canvas

### LineCanvas3D Class

```cpp
class LineCanvas3D {
public:
    void clear();
    void setMatrix(const glm::mat4& mvp);
    
    void line(const glm::vec3& p1, const glm::vec3& p2, const glm::vec4& c);
    void plane(const vec3& orig, const vec3& v1, const vec3& v2, 
               int n1, int n2, float s1, float s2,
               const vec4& color, const vec4& outlineColor);
    void box(const mat4& m, const BoundingBox& box, const vec4& c);
    void frustum(const glm::mat4& camView, const glm::mat4& camProj, const glm::vec4& color);
    
    void render(lvk::IContext& ctx, const lvk::Framebuffer& desc,
                lvk::ICommandBuffer& buf, uint32_t numSamples = 1);
};
```

### Line Data Structure

```cpp
struct LineData {
    vec4 p1, p2;
    vec4 color;
};
```

### GLSL Vertex Shader (Programmable Vertex Pulling)

```glsl
layout (location = 0) out vec4 out_color;
layout(std430, buffer_reference) readonly buffer VertexBuffer {
    Vertex vertices[];
};
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    VertexBuffer vb;
};
void main() {
    Vertex v = vb.vertices[gl_VertexIndex];
    out_color = v.rgba;
    gl_Position = mvp * v.pos;
}
```

---

## Recipe 9: On-Screen Graphs with ImPlot

### LinearGraph Class

```cpp
class LinearGraph {
    const char* name_;
    const size_t maxPoints_;
    std::deque<float> graph_;
public:
    explicit LinearGraph(const char* name, size_t maxGraphPoints = 256);
    void addPoint(float value);
    void renderGraph(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                     const vec4& color = vec4(1.0)) const;
};
```

### LineCanvas2D Class

```cpp
class LineCanvas2D {
public:
    void clear() { lines_.clear(); }
    void line(const vec2& p1, const vec2& p2, const vec4& c);
    void render(const char* name, uint32_t width, uint32_t height);
private:
    struct LineData { vec2 p1, p2; vec4 color; };
    std::vector<LineData> lines_;
};
```

---

## Recipe 10: Complete Vulkan Application

### VulkanApp Helper Class

Located in `shared/VulkanApp.h` and `shared/VulkanApp.cpp`. Wraps:
- LightweightVK context creation
- GLFW window lifetime management
- First-person camera positioner
- FPS counter
- ImGuiRenderer

### Demo Application Structure

```cpp
int main() {
    VulkanApp app({...});
    // Load textures, create buffers, pipelines
    
    app.run([&](uint32_t width, uint32_t height,
                float aspectRatio, float deltaSeconds) {
        // Update camera, matrices
        // Begin rendering
        // Draw skybox, mesh
        // Draw ImGui
        // Draw graphs and canvas
        // End rendering, submit
    });
    return 0;
}
```

### Key Features Demonstrated

- Skybox rendering with cube maps
- Mesh rendering with depth testing
- ImGui windows with keyboard hints
- FPS counter display
- 2D/3D graph rendering
- Frustum visualization

---

## Source Code Locations

| Recipe | Demo Location |
|--------|---------------|
| ImGui | Chapter04/01_ImGui |
| Tracy Profiler | Chapter04/02_TracyProfiler |
| FPS Counter | Chapter04/03_FPSCounter |
| Cube Map | Chapter04/04_CubeMap |
| Camera | Chapter04/05_Camera |
| Demo App | Chapter04/06_DemoApp |

## Key Shared Files

- `shared/VulkanApp.h` / `shared/VulkanApp.cpp` - Application helper class
- `shared/Camera.h` - Camera positioner classes
- `shared/Bitmap.h` - Bitmap helper class
- `shared/UtilsCubemap.cpp` - Cube map conversion functions
- `shared/Graph.h` - LinearGraph class
- `lvk/HelpersImGui.h` / `lvk/HelpersImGui.cpp` - ImGui renderer
