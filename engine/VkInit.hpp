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
//   Fact<Instance> instance{[&] { return Instance::init(glfw.proof()); }};
//   Fact<WindowSurface> surface{[&] {
//       return createWindowSurface(instance.proof(), window.proof());
//   }};
//   auto device = makeDevice(instance.proof(), surface.proof());
//   auto pool   = makeCommandPool(device->proof());
//   auto swap   = makeSwapchain(device->proof(), surface.proof(), drawableExtent);
//   // ... hand these to the render / present / memory modules ...
//
// -- Ownership & teardown -----------------------------------------------------
//   Fact/Proof bindings encode dependencies between the instance, surface,
//   device, command pool and swapchain. The COMPOSER (Engine) still declares
//   them in bring-up order so reverse-order destruction unwinds naturally.
//   Bright data is read directly ((*device)->queueFamilies, swap->images);
//   there are no getters.
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

#include "VkConfig.hpp"
#include "Fact.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct GlfwInitialization;
struct GlfwWindow;

// The Vulkan instance and its debug messenger. Move-only; create with init.
struct Instance {
    InstanceConfig                    config;
    vk::UniqueInstance               instance;
    vk::UniqueDebugUtilsMessengerEXT debugMessenger;

    static Instance init(Proof<const GlfwInitialization>, InstanceConfig config = {});
};

struct WindowSurface {
    Proof<const Instance>   instance;
    Proof<const GlfwWindow> window;
    vk::UniqueSurfaceKHR    surface;

    static WindowSurface init(Proof<const Instance> instance, Proof<const GlfwWindow> window);
};

// ── device ──

// The logical device: the physical-device pick, the logical device, the queue
// family indices, and the raw queue handles obtained at creation. PINNED +
// non-movable: Swapchain / CommandPool borrow a stable Device address.
//
// The queue HANDLES are bright data; the submit / timeline / command-ring
// machinery that OPERATES them lives in a separate runtime module. presentQueue
// aliases graphicsQueue when there is no dedicated present family.
struct Device {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;  // dedicated transfer queue, or == graphicsFamily
    };

    Proof<const Instance>         instance;
    vk::PhysicalDevice            physicalDevice;
    vk::UniqueDevice              device;
    QueueFamilyIndices               queueFamilies;

    vk::Queue                     graphicsQueue_AI;
    vk::Queue                     presentQueue_AI;
    vk::Queue                     transferQueue_AI;

    static std::unique_ptr<Fact<Device>> init(Proof<const Instance> instance,
                                              Proof<const WindowSurface> surface,
                                              DeviceConfig config = {});
    ~Device();                                       // drains the GPU before vkDestroyDevice
};

// ── command pool ──

struct CommandPool {
    Proof<const Device>   device;
    vk::UniqueCommandPool pool;

    static CommandPool init(Proof<const Device> device);
};

// ── swapchain ──

// The presentable image set for one surface configuration: the swapchain, its
// images (owned by the swapchain) and matching views. The present path
// (acquire / present / recreate + render-finished semaphores) is NOT here.
struct Swapchain {
    Proof<const Device>             device;
    Proof<const WindowSurface>      surface;
    vk::UniqueSwapchainKHR           swapchain;
    std::vector<vk::Image>           images;      // NOT owned (the swapchain owns them)
    std::vector<vk::UniqueImageView> imageViews;  // owned
    vk::Format                       imageFormat;
    vk::Extent2D                     extent;

    static Swapchain init(Proof<const Device> device,
                          Proof<const WindowSurface> surface,
                          vk::Extent2D framebufferExtent,
                          const SwapchainConfig& config = {});
};

// ── bring-up verbs ──

// The GLFW → Vulkan surface call, kept inside the module so no caller reaches for
// glfwCreateWindowSurface directly. The returned wrapper retains its instance
// and window dependencies.

// framebufferExtent is the desired drawable size in pixels (asserted non-zero).

// Raw handles for vulkan-hpp interop / third-party backends. The owning
// UniqueInstance / UniqueDevice stay sealed; callers get the value.
[[nodiscard]] vk::Instance rawHandle(const Instance& instance);
[[nodiscard]] vk::SurfaceKHR rawHandle(const WindowSurface& surface);
[[nodiscard]] vk::Device   rawHandle(const Device& device);
