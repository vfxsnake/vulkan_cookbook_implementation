# Chapter 2: Getting Started with Vulkan

In this chapter, we'll take our first steps with Vulkan, focusing on swapchains, shaders, and pipelines. The recipes in this chapter will guide you through getting your first triangle on the screen using Vulkan. The Vulkan implementation we'll use is based on the open-source library LightweightVK (https://github.com/corporateshark/lightweightvk), which we'll explore throughout the book.

## Recipes Covered

- Initializing Vulkan instance and graphical device
- Initializing Vulkan swapchain
- Setting up Vulkan debugging capabilities
- Using Vulkan command buffers
- Initializing Vulkan shader modules
- Initializing Vulkan pipelines

## Technical Requirements

To run the recipes from this chapter, you have to use a Windows or Linux computer with a video card and drivers supporting Vulkan 1.3. Read Chapter 1 if you want to learn how to configure it properly.

---

## Initializing Vulkan Instance and Graphical Device

As some readers may recall from the first edition of our book, the Vulkan API is significantly more verbose than OpenGL. To make things more manageable, we've broken down the process of creating our first graphical demo apps into a series of smaller, focused recipes. In this recipe, we'll cover how to create a Vulkan instance, enumerate all physical devices in the system capable of 3D graphics rendering, and initialize one of these devices to create a window with an attached surface.

### Getting Ready

We recommend starting with beginner-friendly Vulkan books, such as *The Modern Vulkan Cookbook* by Preetish Kakkar and Mauricio Maurer (published by Packt) or *Vulkan Programming Guide: The Official Guide to Learning Vulkan* by Graham Sellers and John Kessenich (Addison-Wesley Professional).

The most challenging aspect of transitioning from OpenGL to Vulkan—or to any similar modern graphics API—is the extensive amount of explicit code required to set up the rendering process, which, fortunately, only needs to be done once. It's also helpful to familiarize yourself with Vulkan's object model. A great starting point is Adam Sawicki's article, *Understanding Vulkan Objects* (https://gpuopen.com/understanding-vulkan-objects).

In the recipes that follow, our goal is to start rendering 3D scenes with the minimal setup needed, demonstrating how modern bindless Vulkan can be wrapped into a more user-friendly API.

All our Vulkan recipes rely on the LightweightVK library, which can be downloaded from https://github.com/corporateshark/lightweightvk using the provided Bootstrap snippet:

```json
{
  "name": "lightweightvk",
  "source": {
    "type": "git",
    "url": "https://github.com/corporateshark/lightweightvk.git",
    "revision": "v1.3"
  }
}
```

The complete Vulkan example for this recipe can be found in `Chapter02/01_Swapchain`.

### How to Do It

Before diving into the actual implementation, let's take a look at some scaffolding code that makes debugging Vulkan backends a bit easier. We will begin with error-checking facilities.

#### 1. VK_ASSERT Macro

Any function call from a complex API can fail. To handle failures, or at least provide the developer with the exact location of the failure, LightweightVK wraps most Vulkan calls in the `VK_ASSERT()` and `VK_ASSERT_RETURN()` macros, which check the results of Vulkan operations:

```cpp
#define VK_ASSERT(func) { \
  const VkResult vk_assert_result = func; \
  if (vk_assert_result != VK_SUCCESS) { \
    LLOGW("Vulkan API call failed: %s:%i\n %s\n %s\n", \
      __FILE__, __LINE__, #func, \
      ivkGetVulkanResultString(vk_assert_result)); \
    assert(false); \
  } \
}
```

#### 2. VK_ASSERT_RETURN Macro

The `VK_ASSERT_RETURN()` macro is very similar and returns the control to the calling code:

```cpp
#define VK_ASSERT_RETURN(func) { \
  const VkResult vk_assert_result = func; \
  if (vk_assert_result != VK_SUCCESS) { \
    LLOGW("Vulkan API call failed: %s:%i\n %s\n %s\n", \
      __FILE__, __LINE__, #func, \
      ivkGetVulkanResultString(vk_assert_result)); \
    assert(false); \
    return getResultFromVkResult(vk_assert_result); \
  } \
}
```

### Creating the First Vulkan Application

Let's explore what is going on in the sample application `Chapter02/01_Swapchain` which creates a window, a Vulkan instance and device together with a Vulkan swapchain:

```cpp
int main(void) {
  minilog::initialize(nullptr, { .threadNames = false });
  int width = 960;
  int height = 540;
  GLFWwindow* window = lvk::initWindow("Simple example", width, height);
  std::unique_ptr<lvk::IContext> ctx =
    lvk::createVulkanContextWithSwapchain(window, width, height, {});
```

The application's main loop handles updates to the framebuffer size if the window is resized, acquires a command buffer, submits it, and presents the current swapchain image:

```cpp
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    glfwGetFramebufferSize(window, &width, &height);
    if (!width || !height) continue;
    lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
    ctx->submit(buf, ctx->getCurrentSwapchainTexture());
  }
```

The shutdown process is straightforward. The IDevice object should be destroyed before the GLFW window:

```cpp
  ctx.reset();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
```

### createVulkanContextWithSwapchain Implementation

This helper function calls LightweightVK to create a `VulkanContext` object, taking the provided GLFW window and display properties for our operating system into account:

```cpp
std::unique_ptr<lvk::IContext> createVulkanContextWithSwapchain(
  GLFWwindow* window, uint32_t width, uint32_t height,
  const lvk::vulkan::VulkanContextConfig& cfg,
  lvk::HWDeviceType preferredDeviceType)
{
  std::unique_ptr<vulkan::VulkanContext> ctx;
#if defined(_WIN32)
  ctx = std::make_unique<VulkanContext>(cfg, (void*)glfwGetWin32Window(window));
#elif defined(__linux__)
  #if defined(LVK_WITH_WAYLAND)
    wl_surface* waylandWindow = glfwGetWaylandWindow(window);
    if (!waylandWindow) {
      LVK_ASSERT_MSG(false, "Wayland window not found");
      return nullptr;
    }
    ctx = std::make_unique<VulkanContext>(cfg,
      (void*)waylandWindow, (void*)glfwGetWaylandDisplay());
  #else
    ctx = std::make_unique<VulkanContext>(cfg,
      (void*)glfwGetX11Window(window), (void*)glfwGetX11Display());
  #endif
#else
# error Unsupported OS
#endif
```

Next, we enumerate Vulkan physical devices and attempt to select the most preferred one:

```cpp
  HWDeviceDesc device;
  uint32_t numDevices = ctx->queryDevices(preferredDeviceType, &device, 1);
  if (!numDevices) {
    if (preferredDeviceType == HWDeviceType_Discrete) {
      numDevices = ctx->queryDevices(HWDeviceType_Integrated, &device);
    } else if (preferredDeviceType == HWDeviceType_Integrated) {
      numDevices = ctx->queryDevices(HWDeviceType_Discrete, &device);
    }
  }
```

Once a physical device is selected, we call `VulkanContext::initContext()`:

```cpp
  if (!numDevices) return nullptr;
  Result res = ctx->initContext(device);
  if (!res.isOk()) return nullptr;

  if (width > 0 && height > 0) {
    res = ctx->initSwapchain(width, height);
    if (!res.isOk()) return nullptr;
  }
  return std::move(ctx);
}
```

### How It Works

There are several helper functions involved in getting Vulkan up and running. It all begins with the creation of a Vulkan instance in `VulkanContext::createInstance()`.

#### Creating the Vulkan Instance

First, check if the required Vulkan Validation Layers are available:

```cpp
const char* kDefaultValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

void VulkanContext::createInstance() {
  vkInstance_ = VK_NULL_HANDLE;
  uint32_t numLayerProperties = 0;
  vkEnumerateInstanceLayerProperties(&numLayerProperties, nullptr);
  std::vector<VkLayerProperties> layerProperties(numLayerProperties);
  vkEnumerateInstanceLayerProperties(&numLayerProperties, layerProperties.data());
```

#### Instance Extensions

Specify the names of all Vulkan instance extensions required:

```cpp
  std::vector<const char*> instanceExtensionNames = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#if defined(_WIN32)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
  #if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
  #else
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
  #endif
#endif
  };
```

#### Application Info

Specify mandatory information about our application requesting Vulkan 1.3:

```cpp
  const VkApplicationInfo appInfo = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "LVK/Vulkan",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "LVK/Vulkan",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_3,
  };
```

#### Creating the Instance

```cpp
  const VkInstanceCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = &layerSettingsCreateInfo,
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = config_.enableValidation ?
      (uint32_t)LVK_ARRAY_NUM_ELEMENTS(kDefaultValidationLayers) : 0u,
    .ppEnabledLayerNames = config_.enableValidation ?
      kDefaultValidationLayers : nullptr,
    .enabledExtensionCount = (uint32_t)instanceExtensionNames.size(),
    .ppEnabledExtensionNames = instanceExtensionNames.data(),
  };
  VK_ASSERT(vkCreateInstance(&ci, nullptr, &vkInstance_));
  volkLoadInstance(vkInstance_);
```

> **Note:** Volk is a meta-loader for Vulkan. It allows you to dynamically load entry points required to use Vulkan without linking to vulkan-1.dll or statically linking the Vulkan loader. https://github.com/zeux/volk

#### Enumerating Physical Devices

```cpp
uint32_t lvk::VulkanContext::queryDevices(
  HWDeviceType deviceType,
  HWDeviceDesc* outDevices,
  uint32_t maxOutDevices)
{
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, nullptr);
  std::vector<VkPhysicalDevice> vkDevices(deviceCount);
  vkEnumeratePhysicalDevices(vkInstance_, &deviceCount, vkDevices.data());
  
  const HWDeviceType desiredDeviceType = deviceType;
  uint32_t numCompatibleDevices = 0;
  for (uint32_t i = 0; i < deviceCount; ++i) {
    VkPhysicalDevice physicalDevice = vkDevices[i];
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    const HWDeviceType deviceType =
      convertVulkanDeviceTypeToLVK(deviceProperties.deviceType);
    if (desiredDeviceType != HWDeviceType_Software &&
        desiredDeviceType != deviceType) continue;
    if (outDevices && numCompatibleDevices < maxOutDevices) {
      outDevices[numCompatibleDevices] =
        {.guid = (uintptr_t)vkDevices[i], .type = deviceType};
      strncpy(outDevices[numCompatibleDevices].name,
        deviceProperties.deviceName,
        strlen(deviceProperties.deviceName));
      numCompatibleDevices++;
    }
  }
  return numCompatibleDevices;
}
```

#### Device Types

```cpp
enum HWDeviceType {
  HWDeviceType_Discrete = 1,
  HWDeviceType_External = 2,
  HWDeviceType_Integrated = 3,
  HWDeviceType_Software = 4,
};
```

### Initializing the Context

The function `VulkanContext::initContext()` creates device queues:

```cpp
lvk::Result VulkanContext::initContext(const HWDeviceDesc& desc)
{
  vkPhysicalDevice_ = (VkPhysicalDevice)desc.guid;
  
  // Get all supported extensions
  std::vector<VkExtensionProperties> allDeviceExtensions;
  getDeviceExtensionProps(vkPhysicalDevice_, allDeviceExtensions);
  
  // Retrieve features and properties
  vkGetPhysicalDeviceFeatures2(vkPhysicalDevice_, &vkFeatures10_);
  vkGetPhysicalDeviceProperties2(vkPhysicalDevice_, &vkPhysicalDeviceProperties2_);
```

#### Finding Queue Families

```cpp
  deviceQueues_.graphicsQueueFamilyIndex =
    lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_GRAPHICS_BIT);
  deviceQueues_.computeQueueFamilyIndex =
    lvk::findQueueFamilyIndex(vkPhysicalDevice_, VK_QUEUE_COMPUTE_BIT);
  
  const float queuePriority = 1.0f;
  const VkDeviceQueueCreateInfo ciQueue[2] = {
    { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = deviceQueues_.graphicsQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority, },
    { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = deviceQueues_.computeQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority, },
  };
```

> **Note:** In Vulkan, `queueFamilyIndex` is the index of the queue family to which the queue belongs. A queue family is a collection of Vulkan queues with similar properties and functionality.

#### DeviceQueues Structure

```cpp
struct DeviceQueues {
  const static uint32_t INVALID = 0xFFFFFFFF;
  uint32_t graphicsQueueFamilyIndex = INVALID;
  uint32_t computeQueueFamilyIndex = INVALID;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue computeQueue = VK_NULL_HANDLE;
};
```

#### Requesting Vulkan Features

Request all necessary Vulkan 1.0–1.3 features:

```cpp
  VkPhysicalDeviceVulkan12Features deviceFeatures12 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &deviceFeatures11,
    .drawIndirectCount = vkFeatures12_.drawIndirectCount,
    .descriptorIndexing = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .descriptorBindingVariableDescriptorCount = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    // ...
  };
  
  VkPhysicalDeviceVulkan13Features deviceFeatures13 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &deviceFeatures12,
    .subgroupSizeControl = VK_TRUE,
    .synchronization2 = VK_TRUE,
    .dynamicRendering = VK_TRUE,
    .maintenance4 = VK_TRUE,
  };
```

> **Note:** 
> - **Descriptor indexing** is a set of Vulkan 1.2 features that enable applications to access all of their resources and select among them using integer indices in shaders.
> - **Dynamic rendering** is a Vulkan 1.3 feature that allows applications to render directly into images without the need to create render pass objects or framebuffers.

#### Creating the Device

```cpp
  const VkDeviceCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = createInfoNext,
    .queueCreateInfoCount = numQueues,
    .pQueueCreateInfos = ciQueue,
    .enabledExtensionCount = deviceExtensionNames.size(),
    .ppEnabledExtensionNames = deviceExtensionNames.data(),
    .pEnabledFeatures = &deviceFeatures10,
  };
  vkCreateDevice(vkPhysicalDevice_, &ci, nullptr, &vkDevice_);
  volkLoadDevice(vkDevice_);
  vkGetDeviceQueue(vkDevice_,
    deviceQueues_.graphicsQueueFamilyIndex, 0,
    &deviceQueues_.graphicsQueue);
  vkGetDeviceQueue(vkDevice_,
    deviceQueues_.computeQueueFamilyIndex, 0,
    &deviceQueues_.computeQueue);
}
```

---

## Initializing Vulkan Swapchain

Normally, each frame is rendered into an offscreen image. After the rendering process is finished, the offscreen image should be made visible or "presented." A swapchain is an object that holds a collection of available offscreen images, or more specifically, a queue of rendered images waiting to be presented to the screen.

In OpenGL, presenting an offscreen buffer to the visible area of a window is done using system-dependent functions, such as `wglSwapBuffers()` on Windows, `eglSwapBuffers()` on OpenGL ES, `glXSwapBuffers()` on Linux, or automatically on macOS. Vulkan, however, gives us much more fine-grained control. We need to select a presentation mode for swapchain images and specify various flags.

### Getting Ready

Revisit the previous recipe *Initializing Vulkan instance and graphical device*, which covers the initial steps necessary to initialize Vulkan. The source code discussed in this recipe is implemented in the class `lvk::VulkanSwapchain`.

### How to Do It

#### Querying Surface Capabilities

```cpp
void lvk::VulkanContext::querySurfaceCapabilities() {
  const VkFormat depthFormats[] = {
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT, 
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D16_UNORM 
  };
  for (const auto& depthFormat : depthFormats) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(
      vkPhysicalDevice_, depthFormat, &formatProps);
    if (formatProps.optimalTilingFeatures)
      deviceDepthFormats_.push_back(depthFormat);
  }
  if (vkSurface_ == VK_NULL_HANDLE) return;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    vkPhysicalDevice_, vkSurface_, &deviceSurfaceCaps_);
  
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    vkPhysicalDevice_, vkSurface_, &formatCount, nullptr);
  if (formatCount) {
    deviceSurfaceFormats_.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      vkPhysicalDevice_, vkSurface_,
      &formatCount, deviceSurfaceFormats_.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    vkPhysicalDevice_, vkSurface_, &presentModeCount, nullptr);
  if (presentModeCount) {
    devicePresentModes_.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      vkPhysicalDevice_, vkSurface_,
      &presentModeCount, devicePresentModes_.data());
  }
}
```

#### Choosing Surface Format

```cpp
VkSurfaceFormatKHR chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR>& formats,
  lvk::ColorSpace colorSpace)
{
  const VkSurfaceFormatKHR preferred =
    colorSpaceToVkSurfaceFormat(colorSpace, isNativeSwapChainBGR(formats));
  for (const VkSurfaceFormatKHR& fmt : formats) {
    if (fmt.format == preferred.format &&
        fmt.colorSpace == preferred.colorSpace) return fmt;
  }
  for (const VkSurfaceFormatKHR& fmt : formats) {
    if (fmt.format == preferred.format) return fmt;
  }
  return formats[0];
}
```

#### Choosing Presentation Mode

```cpp
auto chooseSwapPresentMode = [](
  const std::vector<VkPresentModeKHR>& modes) -> VkPresentModeKHR
{
#if defined(__linux__) || defined(_M_ARM64)
  if (std::find(modes.cbegin(), modes.cend(),
      VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }
#endif
  if (std::find(modes.cbegin(), modes.cend(),
      VK_PRESENT_MODE_MAILBOX_KHR) != modes.cend()) {
    return VK_PRESENT_MODE_MAILBOX_KHR;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
};
```

#### Creating the Swapchain

```cpp
const VkSwapchainCreateInfoKHR ci = {
  .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
  .surface = ctx.vkSurface_,
  .minImageCount = chooseSwapImageCount(ctx.deviceSurfaceCaps_),
  .imageFormat = surfaceFormat_.format,
  .imageColorSpace = surfaceFormat_.colorSpace,
  .imageExtent = {.width = width, .height = height},
  .imageArrayLayers = 1,
  .imageUsage = usageFlags,
  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
  .queueFamilyIndexCount = 1,
  .pQueueFamilyIndices = &ctx.deviceQueues_.graphicsQueueFamilyIndex,
  .preTransform = ctx.deviceSurfaceCaps_.currentTransform,
  .compositeAlpha = isCompositeAlphaOpaqueSupported ?
    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
  .presentMode = chooseSwapPresentMode(ctx.devicePresentModes_),
  .clipped = VK_TRUE,
  .oldSwapchain = VK_NULL_HANDLE,
};
vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_);
```

#### Retrieving Swapchain Images

```cpp
VkImage swapchainImages[LVK_MAX_SWAPCHAIN_IMAGES];
vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, nullptr);
if (numSwapchainImages_ > LVK_MAX_SWAPCHAIN_IMAGES) {
  numSwapchainImages_ = LVK_MAX_SWAPCHAIN_IMAGES;
}
vkGetSwapchainImagesKHR(device_, swapchain_, &numSwapchainImages_, swapchainImages);
```

---

## Setting Up Vulkan Debugging Capabilities

After creating a Vulkan instance, we can start monitoring all potential errors and warnings generated by the validation layers. This is done by using the `VK_EXT_debug_utils` extension to create a callback function and register it with the Vulkan instance.

### How to Do It

#### Creating Debug Messenger

```cpp
vkCreateInstance(&ci, nullptr, &vkInstance_);
volkLoadInstance(vkInstance_);

const VkDebugUtilsMessengerCreateInfoEXT ci = {
  .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
  .messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
  .messageType = 
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
  .pfnUserCallback = &vulkanDebugCallback,
  .pUserData = this,
};
vkCreateDebugUtilsMessengerEXT(vkInstance_, &ci, nullptr, &vkDebugUtilsMessenger_);
```

#### Debug Callback Implementation

```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
  VkDebugUtilsMessageTypeFlagsEXT msgType,
  const VkDebugUtilsMessengerCallbackDataEXT* cbData,
  void* userData)
{
  if (msgSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    return VK_FALSE;
  const bool isError = (msgSeverity &
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
  const bool isWarning = (msgSeverity &
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;
  lvk::VulkanContext* ctx = static_cast<lvk::VulkanContext*>(userData);
  minilog::eLogLevel level = minilog::Log;
  if (isError) {
    level = ctx->config_.terminateOnValidationError ?
      minilog::FatalError : minilog::Warning;
  }
  MINILOG_LOG_PROC(level, "%sValidation layer:\n%s\n", 
    isError ? "\nERROR:\n" : "", cbData->pMessage);
  if (isError) {
    if (ctx->config_.terminateOnValidationError) {
      std::terminate();
    }
  }
  return VK_FALSE;
}
```

#### Setting Debug Object Names

```cpp
lvk::setDebugObjectName(vkDevice_, VK_OBJECT_TYPE_DEVICE,
  (uint64_t)vkDevice_, "Device: VulkanContext::vkDevice_");
```

---

## Using Vulkan Command Buffers

Vulkan command buffers are used to record Vulkan commands which can then be submitted to a device queue for execution. Command buffers are allocated from pools which allow the Vulkan implementation to amortize the cost of resource creation across multiple command buffers. Command pools should be externally synchronized, meaning one command pool should not be used between multiple threads.

### Getting Ready

We are going to explore the command buffers management code from the LightweightVK library. Take a look at the class `VulkanImmediateCommands` from `lvk/vulkan/VulkanClasses.h`.

The main loop of the application looks as follows:

```cpp
while (!glfwWindowShouldClose(window)) {
  glfwPollEvents();
  glfwGetFramebufferSize(window, &width, &height);
  if (!width || !height) continue;
  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
  ctx->submit(buf, ctx->getCurrentSwapchainTexture());
}
```

### How to Do It

#### SubmitHandle Structure

```cpp
struct SubmitHandle {
  uint32_t bufferIndex_ = 0;
  uint32_t submitId_ = 0;
  SubmitHandle() = default;
  explicit SubmitHandle(uint64_t handle) :
    bufferIndex_(uint32_t(handle & 0xffffffff)),
    submitId_(uint32_t(handle >> 32)) {}
  bool empty() const { return submitId_ == 0; }
  uint64_t handle() const { return (uint64_t(submitId_) << 32) + bufferIndex_; }
};
```

#### CommandBufferWrapper Structure

```cpp
struct CommandBufferWrapper {
  VkCommandBuffer cmdBuf_ = VK_NULL_HANDLE;
  VkCommandBuffer cmdBufAllocated_ = VK_NULL_HANDLE;
  SubmitHandle handle_ = {};
  VkFence fence_ = VK_NULL_HANDLE;
  VkSemaphore semaphore_ = VK_NULL_HANDLE;
  bool isEncoding_ = false;
};
```

#### VulkanImmediateCommands Class

```cpp
class VulkanImmediateCommands final {
public:
  static constexpr uint32_t kMaxCommandBuffers = 64;
  VulkanImmediateCommands(VkDevice device,
    uint32_t queueFamilyIdx, const char* debugName);
  ~VulkanImmediateCommands();

  const CommandBufferWrapper& acquire();
  SubmitHandle submit(const CommandBufferWrapper& wrapper);

  void waitSemaphore(VkSemaphore semaphore);
  void signalSemaphore(VkSemaphore semaphore, uint64_t signalValue);
  VkSemaphore acquireLastSubmitSemaphore();

  SubmitHandle getLastSubmitHandle() const;
  bool isReady(SubmitHandle handle) const;
  void wait(SubmitHandle handle);
  void waitAll();

private:
  void purge();
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex_ = 0;
  const char* debugName_ = "";
  CommandBufferWrapper buffers_[kMaxCommandBuffers];
  // ...
};
```

#### Constructor Implementation

```cpp
lvk::VulkanImmediateCommands::VulkanImmediateCommands(
  VkDevice device,
  uint32_t queueFamilyIndex, const char* debugName) :
  device_(device), queueFamilyIndex_(queueFamilyIndex),
  debugName_(debugName)
{
  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue_);
  const VkCommandPoolCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
             VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
    .queueFamilyIndex = queueFamilyIndex,
  };
  vkCreateCommandPool(device, &ci, nullptr, &commandPool_);

  const VkCommandBufferAllocateInfo ai = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = commandPool_,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  for (uint32_t i = 0; i != kMaxCommandBuffers; i++) {
    CommandBufferWrapper& buf = buffers_[i];
    buf.semaphore_ = lvk::createSemaphore(device, semaphoreName);
    buf.fence_ = lvk::createFence(device, fenceName);
    vkAllocateCommandBuffers(device, &ai, &buf.cmdBufAllocated_);
    buffers_[i].handle_.bufferIndex_ = i;
  }
}
```

#### acquire() Implementation

```cpp
const lvk::VulkanImmediateCommands::CommandBufferWrapper&
  lvk::VulkanImmediateCommands::acquire()
{
  while (!numAvailableCommandBuffers_) purge();

  VulkanImmediateCommands::CommandBufferWrapper* current = nullptr;
  for (CommandBufferWrapper& buf : buffers_) {
    if (buf.cmdBuf_ == VK_NULL_HANDLE) {
      current = &buf;
      break;
    }
  }

  current->handle_.submitId_ = submitCounter_;
  numAvailableCommandBuffers_--;
  current->cmdBuf_ = current->cmdBufAllocated_;
  current->isEncoding_ = true;

  const VkCommandBufferBeginInfo bi = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  VK_ASSERT(vkBeginCommandBuffer(current->cmdBuf_, &bi));
  nextSubmitHandle_ = current->handle_;
  return *current;
}
```

#### submit() Implementation

```cpp
SubmitHandle lvk::VulkanImmediateCommands::submit(
  const CommandBufferWrapper& wrapper) 
{
  vkEndCommandBuffer(wrapper.cmdBuf_);

  VkSemaphoreSubmitInfo waitSemaphores[] = {{}, {}};
  uint32_t numWaitSemaphores = 0;
  if (waitSemaphore_.semaphore)
    waitSemaphores[numWaitSemaphores++] = waitSemaphore_;
  if (lastSubmitSemaphore_.semaphore)
    waitSemaphores[numWaitSemaphores++] = lastSubmitSemaphore_;

  VkSemaphoreSubmitInfo signalSemaphores[] = {
    VkSemaphoreSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .semaphore = wrapper.semaphore_,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT},
    {},
  };
  uint32_t numSignalSemaphores = 1;
  if (signalSemaphore_.semaphore) {
    signalSemaphores[numSignalSemaphores++] = signalSemaphore_;
  }

  const VkCommandBufferSubmitInfo bufferSI = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = wrapper.cmdBuf_,
  };
  const VkSubmitInfo2 si = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = numWaitSemaphores,
    .pWaitSemaphoreInfos = waitSemaphores,
    .commandBufferInfoCount = 1u,
    .pCommandBufferInfos = &bufferSI,
    .signalSemaphoreInfoCount = numSignalSemaphores,
    .pSignalSemaphoreInfos = signalSemaphores,
  };
  vkQueueSubmit2(queue_, 1u, &si, wrapper.fence_);
  lastSubmitSemaphore_.semaphore = wrapper.semaphore_;
  lastSubmitHandle_ = wrapper.handle_;

  waitSemaphore_.semaphore = VK_NULL_HANDLE;
  signalSemaphore_.semaphore = VK_NULL_HANDLE;
  const_cast<CommandBufferWrapper&>(wrapper).isEncoding_ = false;
  submitCounter_++;
  if (!submitCounter_) submitCounter_++;
  return lastSubmitHandle_;
}
```

---

## Initializing Vulkan Shader Modules

The Vulkan API consumes shaders in the form of compiled SPIR-V binaries. We already learned how to compile shaders from GLSL source code to SPIR-V using the open-source glslang compiler from Khronos.

### How to Do It

#### loadShaderModule Helper

```cpp
Holder<ShaderModuleHandle> loadShaderModule(
  const std::unique_ptr<lvk::IContext>& ctx,
  const char* fileName)
{
  const std::string code = readShaderFile(fileName);
  const lvk::ShaderStage stage = lvkShaderStageFromFileName(fileName);
  Holder<ShaderModuleHandle> handle =
    ctx->createShaderModule({ code.c_str(), stage,
      (std::string("Shader Module: ") + fileName).c_str() });
  return handle;
}
```

#### Creating Shader Modules

```cpp
Holder<ShaderModuleHandle> vert = loadShaderModule(
  ctx, "Chapter02/02_HelloTriangle/src/main.vert");
Holder<ShaderModuleHandle> frag = loadShaderModule(
  ctx, "Chapter02/02_HelloTriangle/src/main.frag");
```

#### ShaderModuleDesc Structure

```cpp
struct ShaderModuleDesc {
  ShaderStage stage = Stage_Frag;
  const char* data = nullptr;
  size_t dataSize = 0;
  const char* debugName = "";
  ShaderModuleDesc(const char* source, lvk::ShaderStage stage,
    const char* debugName) : stage(stage), data(source),
    debugName(debugName) {}
  ShaderModuleDesc(const void* data, size_t dataLength,
    lvk::ShaderStage stage, const char* debugName) :
    stage(stage), data(static_cast<const char*>(data)),
    dataSize(dataLength), debugName(debugName) {}
};
```

#### Creating VkShaderModule from SPIR-V

```cpp
ShaderModuleState VulkanContext::createShaderModuleFromSPIRV(
  const void* spirv,
  size_t numBytes,
  const char* debugName,
  Result* outResult) const
{
  VkShaderModule vkShaderModule = VK_NULL_HANDLE;
  const VkShaderModuleCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = numBytes,
    .pCode = (const uint32_t*)spirv,
  };
  vkCreateShaderModule(vkDevice_, &ci, nullptr, &vkShaderModule);

  // Use SPIRV-Reflect to get push constants size
  SpvReflectShaderModule mdl;
  SpvReflectResult result =
    spvReflectCreateShaderModule(numBytes, spirv, &mdl);
  
  uint32_t pushConstantsSize = 0;
  for (uint32_t i = 0; i < mdl.push_constant_block_count; ++i) {
    const SpvReflectBlockVariable& block = mdl.push_constant_blocks[i];
    pushConstantsSize = std::max(pushConstantsSize, block.offset + block.size);
  }
  
  return {
    .sm = vkShaderModule,
    .pushConstantsSize = pushConstantsSize,
  };
}
```

---

## Initializing Vulkan Pipelines

A Vulkan pipeline is an implementation of an abstract graphics pipeline, which is a sequence of operations that transform vertices and rasterize the resulting image. Essentially, it's like a single snapshot of a "frozen" OpenGL state. Vulkan pipelines are mostly immutable, meaning multiple Vulkan pipelines should be created to allow different data paths through the graphics pipeline.

### Getting Ready

For additional information on descriptor set layouts, check out the Vulkan Tutorial: https://vulkan-tutorial.com/Uniform_buffers/Descriptor_layout_and_buffer

### How to Do It

#### Hello Triangle Application

```cpp
GLFWwindow* window = lvk::initWindow("Simple example", width, height);
std::unique_ptr<lvk::IContext> ctx =
  lvk::createVulkanContextWithSwapchain(window, width, height, {});

Holder<lvk::ShaderModuleHandle> vert = loadShaderModule(
  ctx, "Chapter02/02_HelloTriangle/src/main.vert");
Holder<lvk::ShaderModuleHandle> frag = loadShaderModule(
  ctx, "Chapter02/02_HelloTriangle/src/main.frag");
Holder<lvk::RenderPipelineHandle> rpTriangle =
  ctx->createRenderPipeline({
    .smVert = vert,
    .smFrag = frag,
    .color = { { .format = ctx->getSwapchainFormat() } } });
```

#### Main Rendering Loop

```cpp
while (!glfwWindowShouldClose(window)) {
  glfwPollEvents();
  glfwGetFramebufferSize(window, &width, &height);
  if (!width || !height) continue;
  lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
  
  buf.cmdBeginRendering(
    {.color = {{ .loadOp = LoadOp_Clear, .clearColor = {1,1,1,1} }}},
    {.color = {{ .texture = ctx->getCurrentSwapchainTexture() }}});

  buf.cmdBindRenderPipeline(rpTriangle);
  buf.cmdPushDebugGroupLabel("Render Triangle", 0xff0000ff);
  buf.cmdDraw(3);
  buf.cmdPopDebugGroupLabel();
  buf.cmdEndRendering();
  ctx->submit(buf, ctx->getCurrentSwapchainTexture());
}
```

### GLSL Shaders

#### Vertex Shader (main.vert)

```glsl
#version 460
layout (location=0) out vec3 color;

const vec2 pos[3] = vec2[3](
  vec2(-0.6, -0.4), vec2(0.6, -0.4), vec2(0.0, 0.6) );
const vec3 col[3] = vec3[3](
  vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );

void main() {
  gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
  color = col[gl_VertexIndex];
}
```

#### Fragment Shader (main.frag)

```glsl
#version 460
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;

void main() {
  out_FragColor = vec4(color, 1.0);
}
```

### RenderPipelineDesc Structure

```cpp
struct RenderPipelineDesc final {
  Topology topology = Topology_Triangle;
  VertexInput vertexInput;
  ShaderModuleHandle smVert;
  ShaderModuleHandle smTesc;
  ShaderModuleHandle smTese;
  ShaderModuleHandle smGeom;
  ShaderModuleHandle smTask;
  ShaderModuleHandle smMesh;
  ShaderModuleHandle smFrag;
  SpecializationConstantDesc specInfo = {};
  const char* entryPointVert = "main";
  const char* entryPointTesc = "main";
  const char* entryPointTese = "main";
  const char* entryPointGeom = "main";
  const char* entryPointTask = "main";
  const char* entryPointMesh = "main";
  const char* entryPointFrag = "main";
  ColorAttachment color[LVK_MAX_COLOR_ATTACHMENTS] = {};
  uint32_t getNumColorAttachments() const {
    uint32_t n = 0;
    while (n < LVK_MAX_COLOR_ATTACHMENTS &&
           color[n].format != Format_Invalid) n++;
    return n;
  }
  Format depthFormat = Format_Invalid;
  Format stencilFormat = Format_Invalid;
  CullMode cullMode = lvk::CullMode_None;
  WindingMode frontFaceWinding = lvk::WindingMode_CCW;
  PolygonMode polygonMode = lvk::PolygonMode_Fill;
  StencilState backFaceStencil = {};
  StencilState frontFaceStencil = {};
  uint32_t samplesCount = 1u;
  uint32_t patchControlPoints = 0;
  float minSampleShading = 0.0f;
  const char* debugName = "";
};
```

### Creating the VkPipeline

The `VulkanPipelineBuilder` provides a fluent interface:

```cpp
lvk::vulkan::VulkanPipelineBuilder()
  .dynamicState(VK_DYNAMIC_STATE_VIEWPORT)
  .dynamicState(VK_DYNAMIC_STATE_SCISSOR)
  .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS)
  .dynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
  .dynamicState(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE)
  .dynamicState(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)
  .dynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)
  .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE)
  .primitiveTopology(topologyToVkPrimitiveTopology(desc.topology))
  .rasterizationSamples(getVulkanSampleCountFlags(desc.samplesCount,
    getFramebufferMSAABitMask()), desc.minSampleShading)
  .polygonMode(polygonModeToVkPolygonMode(desc_.polygonMode))
  .shaderStage(lvk::getPipelineShaderStageCreateInfo(
    VK_SHADER_STAGE_VERTEX_BIT, vertModule->sm, desc.entryPointVert, &si))
  .shaderStage(lvk::getPipelineShaderStageCreateInfo(
    VK_SHADER_STAGE_FRAGMENT_BIT, fragModule->sm, desc.entryPointFrag, &si))
  .cullMode(cullModeToVkCullMode(desc_.cullMode))
  .frontFace(windingModeToVkFrontFace(desc_.frontFaceWinding))
  .vertexInputState(vertexInputStateCreateInfo_)
  .colorAttachments(colorBlendAttachmentStates,
    colorAttachmentFormats, numColorAttachments)
  .depthAttachmentFormat(formatToVkFormat(desc.depthFormat))
  .stencilAttachmentFormat(formatToVkFormat(desc.stencilFormat))
  .patchControlPoints(desc.patchControlPoints)
  .build(vkDevice_, pipelineCache_, layout, &pipeline, desc.debugName);
```

#### Final Pipeline Creation

```cpp
const VkGraphicsPipelineCreateInfo ci = {
  .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
  .pNext = &renderingInfo,
  .flags = 0,
  .stageCount = numShaderStages_,
  .pStages = shaderStages_,
  .pVertexInputState = &vertexInputState_,
  .pInputAssemblyState = &inputAssembly_,
  .pTessellationState = &tessellationState_,
  .pViewportState = &viewportState,
  .pRasterizationState = &rasterizationState_,
  .pMultisampleState = &multisampleState_,
  .pDepthStencilState = &depthStencilState_,
  .pColorBlendState = &colorBlendState,
  .pDynamicState = &dynamicState,
  .layout = pipelineLayout,
  .renderPass = VK_NULL_HANDLE,
  .subpass = 0,
  .basePipelineHandle = VK_NULL_HANDLE,
  .basePipelineIndex = -1,
};
vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, outPipeline);
```

### There's More

If you are familiar with older versions of Vulkan, you might have noticed that in this recipe we completely left out any references to render passes. They are also not mentioned in any of the data structures. The reason is that we use Vulkan 1.3 dynamic rendering functionality, which allows `VkPipeline` objects to operate without needing a render pass.

In case you want to implement a similar wrapper for older versions of Vulkan and without using the `VK_KHR_dynamic_rendering` extension, you can maintain a "global" collection of render passes in an array inside `VulkanContext` and add an integer index of a corresponding render pass as a data member to `RenderPipelineDynamicState`.

If you want to explore an actual working implementation of this approach, take a look at Meta's IGL library: https://github.com/facebook/igl/blob/main/src/igl/vulkan/RenderPipelineState.h