#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct InstanceConfig {
    std::string applicationName    = "ComplexityLab";
    uint32_t applicationVersion    = VK_MAKE_VERSION(1, 0, 0);
    std::string engineName         = "No Engine";
    uint32_t engineVersion         = VK_MAKE_VERSION(1, 0, 0);
    uint32_t apiVersion            = VK_API_VERSION_1_3;

#ifndef NDEBUG
    std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };
    std::vector<vk::ValidationFeatureEnableEXT> validationFeatures = {
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
    };
#else
    std::vector<const char*> validationLayers;
    std::vector<vk::ValidationFeatureEnableEXT> validationFeatures;
#endif

    vk::DebugUtilsMessageSeverityFlagsEXT debugSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

    vk::DebugUtilsMessageTypeFlagsEXT debugTypes =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

    std::vector<const char*> requiredExtensions;
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

    std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
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

struct SwapchainConfig {
    vk::Format         preferredFormat      = vk::Format::eB8G8R8A8Srgb;
    vk::ColorSpaceKHR  preferredColorSpace  = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::PresentModeKHR preferredPresentMode = vk::PresentModeKHR::eMailbox;
    uint32_t           preferredImageCount  = 3;   // triple buffering if possible
    vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment |
                                     vk::ImageUsageFlagBits::eTransferDst |
                                     vk::ImageUsageFlagBits::eSampled;
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    bool clipped = VK_TRUE;
};
