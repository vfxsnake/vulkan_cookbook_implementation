# Session 2.1 — LVK Internals Study Notes

## Topic 1: Instance & Device Creation

### Vulkan Instance vs Device

- **VkInstance** — Your application's connection to the Vulkan runtime. The entry point to discover available hardware.
  - Stores: API version, enabled instance extensions (debug utils, surface), validation layers
  - Created first, before anything else
  - Used to enumerate physical devices and create window surfaces

- **VkPhysicalDevice** — Represents actual GPU hardware. Read-only handle for querying capabilities (memory, limits, supported features, queue families). You don't create this — you discover it via the instance.

- **VkDevice** (Logical Device) — Your application's "session" with a chosen physical device. All resource creation (buffers, textures, pipelines, command buffers) goes through this.

**Flow**: Instance → discover Physical Devices → pick one → create Logical Device

### Volk (Dynamic Function Loading)

- `volkInitialize()` dynamically loads the Vulkan shared library (`vulkan-1.dll` / `libvulkan.so`) at runtime using OS calls (`LoadLibrary` / `dlopen`)
- Fetches `vkGetInstanceProcAddr` — the one function that can look up all others
- **Why not static linking?**
  1. **Portability** — Static linking crashes on launch if Vulkan drivers aren't installed. Dynamic loading can detect and show an error gracefully.
  2. **Function dispatch** — Volk loads direct function pointers at each stage, bypassing the Vulkan loader's dispatch table (slightly faster)

- **Three levels of Vulkan functions**:
  - **Global** (e.g., `vkCreateInstance`) — loaded via `vkGetInstanceProcAddr(NULL, ...)`
  - **Instance-level** (e.g., `vkEnumeratePhysicalDevices`) — loaded after instance creation via `volkLoadInstance()`
  - **Device-level** (e.g., `vkCreateBuffer`, `vkCmdDraw`) — loaded after device creation via `volkLoadDevice()`

> **Why device-level is loaded separately (comprehension check)**:
> Two reasons — not just one:
> 1. **Handle didn't exist yet** — you need a `VkDevice` handle before you can load pointers for it.
> 2. **Direct per-device dispatch** — `volkLoadDevice()` loads function pointers that go *directly*
>    to that device's driver code, bypassing the Vulkan loader's generic dispatch table. This
>    eliminates one indirection per call (important for high-frequency calls like `vkCmdDraw`).
>    With multiple GPUs, each device gets its own dispatch table so calls never cross to the wrong driver.

### Queue Families

- **Queue** = A submission channel to the GPU. You submit command buffers to a queue for execution.
- **Queue Family** = A group of queues sharing the same capabilities.

| Family Type | Capabilities |
|---|---|
| **Graphics** | Drawing + compute + transfer (can do everything) |
| **Compute** | Compute shaders + transfer (no drawing) |
| **Transfer** | Memory copies only (CPU→GPU, GPU→GPU) |

- Different GPUs expose different numbers of families (often 3+)
- Each family can have multiple queues (e.g., "graphics family with 16 queues")
- **Why separate compute queue?** A heavy compute job on the graphics queue blocks rendering. A dedicated compute queue runs **in parallel** on different GPU hardware units.
- LVK tries to find compute queue from a **different family** than graphics; falls back to shared if unavailable.

> **Same-family handling (comprehension check)**:
> If compute and graphics end up with the same `queueFamilyIndex`, LVK collapses to a single
> `VkDeviceQueueCreateInfo` (sets `numQueues = 1`). This is required — the Vulkan spec forbids
> duplicate `queueFamilyIndex` values in the `pQueueCreateInfos` array (validation error if violated).
> The `queueCreateInfoCount` variable in LVK's device creation is not hardcoded to 2 for this reason.

### VMA (Vulkan Memory Allocator)

**Problem**: Raw `vkAllocateMemory` is impractical because:

1. **Allocation limit** — Hard cap (~4096 allocations on desktop GPUs). One per resource hits this fast.
2. **Memory type selection** — GPUs expose multiple memory types (device-local VRAM, host-visible RAM, host-coherent). Picking the right one is complex.
3. **Fragmentation** — Create/destroy cycles leave gaps. Needs smart management.
4. **Alignment** — Different resources need different memory alignment.

**Solution**: VMA sub-allocates from large Vulkan memory blocks. Like `malloc` for the GPU:
- Allocates big blocks from Vulkan, carves them up for individual resources
- One Vulkan allocation can hold hundreds of buffers/textures
- Handles memory type selection, alignment, and fragmentation automatically

> **Correction (comprehension check)**: The ~4096 allocation cap is **not an OS limit** — it is a
> **Vulkan implementation limit** (`VkPhysicalDeviceLimits::maxMemoryAllocationCount`) reported by
> the GPU driver. It reflects how many separate entries the **GPU's MMU page table** can track
> simultaneously. The OS has its own virtual memory limits which are much larger and separate.

### Full Init Flow

```
volkInitialize()
  → create Instance
    → create Surface (window)
      → pick Physical Device
        → create Logical Device (with queue families)
          → create VMA Allocator
            → create Pipeline Cache
```

---

## Topic 2: Swapchain Management

### Why Multiple Images

The swapchain manages a set of images for presenting to the display. Multiple images are needed so the GPU can render into one image while the display is scanning out another — no tearing, no half-rendered frames shown. These are **color-only presentation images**; depth buffers, normal buffers, and G-buffers are separate allocations that never go to the screen.

### Surface Capabilities

Before creating the swapchain, LVK calls `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` to query `VkSurfaceCapabilitiesKHR`. Two key constraints read from it:

1. **Image count** (`minImageCount` / `maxImageCount`) — the swapchain must request an image count within this range (e.g., minimum 2 for double buffering)
2. **Image extent** (`currentExtent`) — resolution must match the surface's current size, clamped to min/max extent

Also queried: `supportedUsageFlags` — what operations are allowed on swapchain images (e.g., `TRANSFER_DST` to allow screenshot copies).

### Present Modes

| Mode | Behavior | Tradeoff |
|---|---|---|
| **FIFO** | Strict queue; GPU stalls when full, waits for VSync | Tear-free, power-efficient, slightly higher latency |
| **Mailbox** | Single waiting slot; newer frames replace waiting ones | Tear-free, lower latency, burns extra GPU cycles |

Both are tear-free (both wait for VSync before presenting). FIFO is guaranteed available on all Vulkan implementations; Mailbox must be checked for support first.

### Frame Loop Synchronization

Swapchain images are owned by the presentation engine — you cannot pick an arbitrary index to render into.

**`vkAcquireNextImageKHR`** returns:
- The image **index** to render into
- An **acquireSemaphore** — signaled by the presentation engine when it finishes reading (scanning out) that image. Your render commands must **wait** on this before writing to the image.

**`vkQueuePresentKHR`** takes a `VkPresentInfoKHR` containing:
- A **renderCompleteSemaphore** in `pWaitSemaphores` — signaled by your render submission when it finishes. The presentation engine **waits** on this before scanning out.

Full synchronized loop:
```
acquire image  →  GPU signals: acquireSemaphore (image free to write)
render cmds    →  wait: acquireSemaphore | signal: renderCompleteSemaphore
present        →  wait: renderCompleteSemaphore (presentation waits before scan-out)
```

---

## Topic 3: Command Buffers & Synchronization

*(To be filled after study)*

---

## Topic 4: Shader Modules & Bindless Descriptors

*(To be filled after study)*

---

## Topic 5: Pipeline Creation

*(To be filled after study)*
