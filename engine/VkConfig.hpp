#pragma once

#ifndef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#endif

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <vector>

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
