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
};

struct WindowSurface {
    Proof<const Instance>   instance;
    Proof<const GlfwWindow> window;
    vk::UniqueSurfaceKHR    surface;
};

// Obtained through Device
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

    ~Device();
};

struct CommandPool {
    Proof<const Device>   device;
    vk::UniqueCommandPool pool;
};


struct Swapchain {
    Proof<const Device>             device;
    Proof<const WindowSurface>      surface;
    vk::UniqueSwapchainKHR           swapchain;
    std::vector<vk::Image>           images;      // NOT owned (the swapchain owns them)
    std::vector<vk::UniqueImageView> imageViews;  // owned
    vk::Format                       imageFormat;
    vk::Extent2D                     extent;
};

struct VkAll {
    std::unique_ptr<Fact<Instance>>      instance;
    std::unique_ptr<Fact<WindowSurface>> surface;
    std::unique_ptr<Fact<Device>>        device;
    std::unique_ptr<Fact<CommandPool>>   commandPool;
    std::unique_ptr<Fact<Swapchain>>     swapchain;

    static VkAll init(Proof<const GlfwInitialization> glfw,
                      Proof<const GlfwWindow> window,
                      InstanceConfig instanceConfig = {},
                      DeviceConfig deviceConfig = {},
                      CommandPoolConfig_AI commandPoolConfig = {},
                      SwapchainConfig swapchainConfig = {});
};
