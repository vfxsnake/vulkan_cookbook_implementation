# Chapter 2: Getting Started with Vulkan

## Overview
This chapter covers fundamental Vulkan components: instance, device, swapchain, command buffers, and pipelines.

## LightweightVK (lvk) Introduction

The book uses LightweightVK as a thin Vulkan wrapper. Key abstractions:

```cpp
// Core types
lvk::IContext          // Main Vulkan context (instance, device, swapchain)
lvk::ICommandBuffer    // Command buffer wrapper
lvk::TextureHandle     // Opaque handle for textures
lvk::BufferHandle      // Opaque handle for buffers
lvk::RenderPipelineHandle  // Graphics pipeline handle
lvk::Holder<T>         // RAII wrapper that auto-destroys resources
```

## Creating Vulkan Context with Swapchain

```cpp
#include "lvk/LVK.h"

int main() {
    // Initialize logging
    minilog::initialize(nullptr, { .threadNames = false });
    
    int width = 960, height = 540;
    
    // Create GLFW window (no OpenGL context)
    GLFWwindow* window = lvk::initWindow("Vulkan App", width, height);
    
    // Create Vulkan context with swapchain
    std::unique_ptr<lvk::IContext> ctx = 
        lvk::createVulkanContextWithSwapchain(window, width, height, {});
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glfwGetFramebufferSize(window, &width, &height);
        if (!width || !height) continue;
        
        // Acquire command buffer, submit, present
        lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
        ctx->submit(buf, ctx->getCurrentSwapchainTexture());
    }
    
    // Cleanup (order matters!)
    ctx.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

## Vulkan Instance Creation (Inside LightweightVK)

Key steps in `VulkanContext::createInstance()`:

```cpp
// 1. Check validation layers
const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

// 2. Required extensions
std::vector<const char*> instanceExtensionNames = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#if defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,  // or WAYLAND
#endif
};

// 3. Application info (request Vulkan 1.3)
const VkApplicationInfo appInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "LVK/Vulkan",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_3,
};

// 4. Create instance
VkInstanceCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = validationEnabled ? 1 : 0,
    .ppEnabledLayerNames = validationEnabled ? kDefaultValidationLayers : nullptr,
    .enabledExtensionCount = (uint32_t)instanceExtensionNames.size(),
    .ppEnabledExtensionNames = instanceExtensionNames.data(),
};
vkCreateInstance(&ci, nullptr, &vkInstance_);
```

## Physical Device Selection

```cpp
// Enumerate physical devices
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr);
std::vector<VkPhysicalDevice> devices(deviceCount);
vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, devices.data());

// Prefer discrete GPU, fallback to integrated
for (auto& device : devices) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);
    
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        vkPhysicalDevice_ = device;
        break;
    }
}
```

## Swapchain Creation

Key parameters for swapchain:

```cpp
VkSwapchainCreateInfoKHR ci = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = vkSurface_,
    .minImageCount = 3,  // Triple buffering
    .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
    .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    .imageExtent = { width, height },
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = VK_PRESENT_MODE_FIFO_KHR,  // V-Sync
    .clipped = VK_TRUE,
};
vkCreateSwapchainKHR(vkDevice_, &ci, nullptr, &vkSwapchain_);
```

## Command Buffer Usage

```cpp
// Acquire a command buffer (LightweightVK manages pools)
lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();

// Begin rendering to swapchain
buf.cmdBeginRendering(
    lvk::RenderPass{
        .color = {{ 
            .loadOp = lvk::LoadOp_Clear,
            .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}
        }},
        .depth = {
            .loadOp = lvk::LoadOp_Clear,
            .clearDepth = 1.0f
        }
    },
    lvk::Framebuffer{
        .color = {{ .texture = ctx->getCurrentSwapchainTexture() }},
        .depthStencil = { .texture = depthTexture }
    }
);

// Draw commands here...
buf.cmdBindRenderPipeline(pipeline);
buf.cmdDraw(3);  // Draw 3 vertices (triangle)

buf.cmdEndRendering();

// Submit and present
ctx->submit(buf, ctx->getCurrentSwapchainTexture());
```

## Creating a Render Pipeline (Hello Triangle)

```cpp
// Load shaders
lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(ctx, "main.vert");
lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(ctx, "main.frag");

// Create pipeline
lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
    .smVert = vert,
    .smFrag = frag,
    .color = {{ .format = ctx->getSwapchainFormat() }},
    .depthFormat = lvk::Format_Z_F32,
});
```

## Hello Triangle Shaders

### Vertex Shader (main.vert)
```glsl
#version 460

// Hardcoded triangle vertices
const vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),
    vec2( 0.5,  0.5),
    vec2(-0.5,  0.5)
);

const vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
```

### Fragment Shader (main.frag)
```glsl
#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

## Using GLM for Transforms

```cpp
#include <glm/glm.hpp>
#include <glm/ext.hpp>

using namespace glm;

// Create MVP matrices
mat4 model = rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0, 1, 0));
mat4 view = lookAt(vec3(0, 0, -3), vec3(0, 0, 0), vec3(0, 1, 0));
mat4 proj = perspective(radians(45.0f), aspectRatio, 0.1f, 100.0f);
mat4 mvp = proj * view * model;
```

## Push Constants

Pass small data directly to shaders:

```cpp
// In C++
struct PushConstants {
    mat4 mvp;
    uint32_t textureId;
};

PushConstants pc = {
    .mvp = proj * view * model,
    .textureId = texture.index(),
};
buf.cmdPushConstants(pc);

// In GLSL
layout(push_constant) uniform PushConstants {
    mat4 mvp;
    uint textureId;
};
```

## Validation Layer Debugging

LightweightVK sets up debug callbacks:

```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printf("Validation: %s\n", data->pMessage);
    }
    return VK_FALSE;
}
```

## Demo Applications

| Demo | Description | Output |
|------|-------------|--------|
| 01_Swapchain | Empty window with swapchain | Black window |
| 02_HelloTriangle | Colored triangle | RGB triangle |
| 03_GLM | Rotating cube with transforms | 3D animated cube |

## Key Vulkan 1.3 Features Used

1. **Dynamic Rendering** - No render pass objects needed
2. **Synchronization2** - Simplified barriers
3. **Maintenance4** - Various improvements
4. **Extended Dynamic State** - Reduce pipeline permutations

## Common Pitfalls

1. **Forgetting to call `glfwPollEvents()`** - Window becomes unresponsive
2. **Wrong image layout transitions** - Validation errors
3. **Missing synchronization** - Rendering artifacts
4. **Not handling window resize** - Crash on resize
