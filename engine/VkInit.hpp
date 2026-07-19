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
//   auto instance = Instance::init(glfw.proof());            // instance + debug
//   auto surface  = createWindowSurface(instance, window);   // GLFW → VkSurface
//   auto device   = makeDevice(rawHandle(instance), *surface);
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

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

template <typename T> struct Proof;
struct GlfwInitialization;
struct GlfwWindow;

// The Vulkan instance and its debug messenger. Move-only; create with init.
struct Instance {
    vk::UniqueInstance               instance;
    vk::UniqueDebugUtilsMessengerEXT debugMessenger;

    static Instance init(Proof<const GlfwInitialization>);
};

// ── device ──

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;  // dedicated transfer queue, or == graphicsFamily
};

struct DeviceConfig {
    using FeatureChain = vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceSynchronization2Features,
        vk::PhysicalDeviceVulkan12Features,
        vk::PhysicalDeviceDynamicRenderingFeatures,
        vk::PhysicalDeviceShaderObjectFeaturesEXT
    >;

    // Extensions the runtime requires. Callers may append scene-specific
    // extensions after the baseline.
    std::vector<const char*> requiredExtensions = {
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
    };

    FeatureChain featureChain{
        vk::PhysicalDeviceFeatures2{
            vk::PhysicalDeviceFeatures{
                VkPhysicalDeviceFeatures{
                    .multiDrawIndirect = VK_TRUE,
                    .wideLines         = VK_TRUE,
                    .largePoints       = VK_TRUE,
                    .samplerAnisotropy = VK_TRUE,
                    .shaderInt64       = VK_TRUE,
                }
            }
        },
        vk::PhysicalDeviceVulkan11Features{
            VkPhysicalDeviceVulkan11Features{
                .sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                .shaderDrawParameters = VK_TRUE,
            }
        },
        vk::PhysicalDeviceSynchronization2Features{
            VkPhysicalDeviceSynchronization2Features{
                .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
                .synchronization2 = VK_TRUE,
            }
        },
        vk::PhysicalDeviceVulkan12Features{
            VkPhysicalDeviceVulkan12Features{
                .sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .timelineSemaphore   = VK_TRUE,
                .bufferDeviceAddress = VK_TRUE,
            }
        },
        vk::PhysicalDeviceDynamicRenderingFeatures{
            VkPhysicalDeviceDynamicRenderingFeatures{
                .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
                .dynamicRendering = VK_TRUE,
            }
        },
        vk::PhysicalDeviceShaderObjectFeaturesEXT{
            VkPhysicalDeviceShaderObjectFeaturesEXT{
                .sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
                .shaderObject = VK_TRUE,
            }
        },
    };
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

// The GLFW → Vulkan surface call, kept inside the module so no caller reaches for
// glfwCreateWindowSurface directly. The returned surface must outlive the Device
// and Swapchain built against it.
[[nodiscard]] vk::UniqueSurfaceKHR createWindowSurface(const Instance& instance,
                                                       const GlfwWindow& window);

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
