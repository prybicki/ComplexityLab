#pragma once
//==============================================================================
// VkInit — brings Vulkan up to a draw-ready state and stops there.
//
// This module owns Vulkan BRING-UP only: the instance (+ validation/debug
// messenger), the window surface, the physical/logical device and its queue
// handles, the graphics command pool, and the swapchain (images + views). After
// makeSwapchain() the GPU stack is ready to record and draw into; nothing here
// records, submits, presents, or allocates GPU memory.
//
// -- The arc of bring-up (windowed) -------------------------------------------
//   auto instance = makeInstance();                          // instance + debug
//   auto surface  = createWindowSurface(*instance, window);  // GLFW → VkSurface
//   auto device   = makeDevice(rawHandle(*instance), *surface);
//   auto pool     = makeCommandPool(*device);
//   auto swap     = makeSwapchain(*device, *surface, drawableExtent);
//   // ... hand these to the render / present / memory modules ...
//
//   Headless: pass std::nullopt for the surface to makeDevice and skip the
//   surface + swapchain steps.
//
// -- Ownership & teardown -----------------------------------------------------
//   Each make* verb returns a move-only owner (unique_ptr / UniqueHandle). The
//   pieces borrow downward — Device borrows the instance handle, Swapchain and
//   CommandPool borrow the Device — so the COMPOSER (Engine) must declare them
//   in bring-up order (instance, surface, device, pool, swapchain) and let
//   reverse-order destruction unwind them. Bright data is read directly
//   (device->queueFamilies, swap->images); there are no getters.
//   ~Device drains the GPU (vkDeviceWaitIdle) before vkDestroyDevice, the one
//   teardown step declaration order cannot express.
//
// -- Deliberately NOT here (operating the stack, not booting it) --------------
//   * Queue OPERATION: timeline semaphores, command-buffer rings, submit /
//     present. VkInit only obtains the raw vk::Queue handles (below).
//   * GPU memory: VMA, buffers, images, upload/staging/readback rings.
//   * Shaders / pipelines, and the swapchain present path (acquire / present /
//     recreate + the per-image render-finished semaphores).
//   These belong to the runtime modules that consume a Device built here.
//==============================================================================

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

// ── instance ──

struct InstanceConfig {
    std::string              applicationName        = "ComplexityLab";
    uint32_t                 applicationVersion     = VK_MAKE_VERSION(1, 0, 0);
    bool                     enableValidationLayers = true;
    bool                     headless               = false;   // no surface extensions
    std::vector<const char*> validationLayers       = {"VK_LAYER_KHRONOS_validation"};
    std::vector<const char*> requiredExtensions     = {};
};

// The Vulkan instance + its debug messenger, plus the validation-error counter
// test fixtures assert against. PINNED: the debug messenger holds a pointer back
// to errorCount (written from the driver thread), so the address must stay
// stable — heap-owned via makeInstance, copy/move deleted.
struct Instance {
    InstanceConfig                      config;
    vk::UniqueInstance               instance;
    vk::UniqueDebugUtilsMessengerEXT debugMessenger;
    // Monotonic count of Error-severity validation messages since creation.
    std::atomic<uint64_t>               errorCount{0};

    Instance() = default;
    Instance(const Instance&)            = delete;
    Instance& operator=(const Instance&) = delete;
    Instance(Instance&&)                 = delete;
    Instance& operator=(Instance&&)      = delete;
};

// ── device ──

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;  // dedicated transfer queue, or == graphicsFamily
};

struct DeviceConfig {
    // Extensions the runtime requires. Baseline is populated by the default
    // constructor; callers may append scene-specific extensions after.
    std::vector<const char*> requiredExtensions;

    // Vulkan 1.0 features. Caller sets additional flags on top of the empty
    // baseline.
    vk::PhysicalDeviceFeatures features = {};

    // Canonical runtime feature chain. Baseline flags are set by the default
    // constructor (shaderDrawParameters, bufferDeviceAddress, timelineSemaphore,
    // synchronization2, dynamicRendering). Callers may enable additional flags on
    // these structs. Do NOT set pNext; chain wiring is managed internally by
    // createLogicalDevice, rebuilt from scratch so Config stays safe to copy,
    // move, and default-construct.
    vk::PhysicalDeviceVulkan11Features         vulkan11         = {};
    vk::PhysicalDeviceVulkan12Features         vulkan12         = {};
    vk::PhysicalDeviceSynchronization2Features sync2            = {};
    vk::PhysicalDeviceDynamicRenderingFeatures dynamicRendering = {};

    // Scene-specific feature structs to layer on top of the canonical chain. Each
    // pointee's pNext is overwritten during device creation; callers set only the
    // feature flags. Pointees must outlive the makeDevice call.
    std::vector<vk::BaseOutStructure*> extras;

    // If provided, called during device selection to verify a physical device
    // supports scene-specific features beyond the baseline chain. Return true if
    // the device is acceptable.
    std::function<bool(vk::PhysicalDevice)> checkFeatureSupport = nullptr;

    // Establishes the canonical runtime baseline. Some flags (bufferDeviceAddress,
    // timelineSemaphore, the memory-budget extension) primarily serve the runtime
    // memory / submit modules, but device features are creation-time-only, so they
    // are front-loaded here to keep the device usable by those later modules.
    DeviceConfig();
};

// The logical device: the physical-device pick, the logical device, the queue
// family indices, and the raw queue handles obtained at creation. PINNED +
// non-movable: Swapchain / CommandPool borrow a stable Device address.
//
// The queue HANDLES are bright data; the submit / timeline / command-ring
// machinery that OPERATES them lives in a separate runtime module. presentQueue
// aliases graphicsQueue when there is no dedicated present family (or when the
// device was built headless, so no present family was scored).
struct Device {
    DeviceConfig                     config;
    vk::Instance                  instance;       // borrowed
    std::optional<vk::SurfaceKHR> surface;        // borrowed; nullopt ⇒ headless
    vk::PhysicalDevice            physicalDevice;
    vk::UniqueDevice              device;
    QueueFamilyIndices               queueFamilies;

    vk::Queue                     graphicsQueue_AI;
    vk::Queue                     presentQueue_AI;
    vk::Queue                     transferQueue_AI;

    Device() = default;
    ~Device();                                       // drains the GPU before vkDestroyDevice
    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&)                 = delete;
    Device& operator=(Device&&)      = delete;
};

// ── command pool ──

struct CommandPool {
    vk::Device            device;   // borrowed handle; the owning Device outlives this
    vk::UniqueCommandPool pool;
};

// ── swapchain ──

struct SwapchainConfig {
    vk::Format         preferredFormat      = vk::Format::eB8G8R8A8Srgb;
    vk::ColorSpaceKHR  preferredColorSpace  = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::PresentModeKHR preferredPresentMode = vk::PresentModeKHR::eMailbox;
    uint32_t              preferredImageCount  = 3;   // triple buffering if possible
    vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                        vk::ImageUsageFlagBits::eTransferDst |
                                        vk::ImageUsageFlagBits::eSampled;
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    bool                  clipped = VK_TRUE;
};

// The presentable image set for one surface configuration: the swapchain, its
// images (owned by the swapchain) and matching views. The present path
// (acquire / present / recreate + render-finished semaphores) is NOT here.
struct Swapchain {
    Device*                             device;   // non-owning, must outlive this
    vk::SurfaceKHR                   surface;
    vk::UniqueSwapchainKHR           swapchain;
    std::vector<vk::Image>           images;      // NOT owned (the swapchain owns them)
    std::vector<vk::UniqueImageView> imageViews;  // owned
    vk::Format                       imageFormat;
    vk::Extent2D                     extent;
};

// ── bring-up verbs ──

[[nodiscard]] std::unique_ptr<Instance> makeInstance(InstanceConfig config = {});

// The GLFW → Vulkan surface call, kept inside the module so no caller reaches for
// glfwCreateWindowSurface directly. The returned surface must outlive the Device
// and Swapchain built against it.
[[nodiscard]] vk::UniqueSurfaceKHR createWindowSurface(const Instance& instance, GLFWwindow* window);

// A null surface builds headless (graphics only, no present family, no swapchain);
// a surface requires present support and auto-injects VK_KHR_swapchain.
[[nodiscard]] std::unique_ptr<Device> makeDevice(vk::Instance instance,
                                                 std::optional<vk::SurfaceKHR> surface,
                                                 DeviceConfig config = {});

[[nodiscard]] std::unique_ptr<CommandPool> makeCommandPool(const Device& device);

// framebufferExtent is the desired drawable size in pixels (asserted non-zero).
[[nodiscard]] std::unique_ptr<Swapchain> makeSwapchain(Device& device, vk::SurfaceKHR surface,
                                                       vk::Extent2D framebufferExtent,
                                                       const SwapchainConfig& config = {});

// Raw handles for vulkan-hpp interop / third-party backends. The owning
// UniqueInstance / UniqueDevice stay sealed; callers get the value.
[[nodiscard]] vk::Instance rawHandle(const Instance& instance);
[[nodiscard]] vk::Device   rawHandle(const Device& device);
