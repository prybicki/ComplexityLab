#pragma once

#include "VkConfig.hpp"
#include "Fact.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct GlfwInitialization;
struct GlfwWindow;

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

struct PhysicalDevice {
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> transferFamily;
    };

    Proof<const Instance> instance;
    vk::PhysicalDevice    handle_AI;
    QueueFamilyIndices    queueFamilies;
};

struct Device {
    PhysicalDevice   physicalDevice;
    vk::UniqueDevice device;

    vk::Queue             graphicsQueue_AI;
    vk::Queue             presentQueue_AI;
    vk::Queue             transferQueue_AI;

    static std::unique_ptr<Fact<Device>> init(Proof<const Instance> instance,
                                              Proof<const WindowSurface> surface,
                                              DeviceConfig config = {});
    ~Device();
};

struct CommandPool {
    Proof<const Device>   device;
    vk::UniqueCommandPool pool;

    static CommandPool init(Proof<const Device> device, CommandPoolConfig_AI config_AI = {});
};


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
