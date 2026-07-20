#include "VkInit.hpp"
#include "Platform.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
); // _AI  (signature fixed by PFN_vkDebugUtilsMessengerCallbackEXT)
std::vector<const char*> instanceExtensions_AI(const std::vector<const char*>& requiredExtensions_AI, bool validationEnabled_AI);
Instance createInstance(Proof<const GlfwInitialization>, InstanceConfig config);

WindowSurface createWindowSurface(Proof<const Instance> instance, Proof<const GlfwWindow> window);

bool familiesComplete(const PhysicalDevice::QueueFamilyIndices& q) noexcept;
bool hasDedicatedTransfer(const PhysicalDevice::QueueFamilyIndices& q) noexcept;
std::set<uint32_t> uniqueFamilies(const PhysicalDevice::QueueFamilyIndices& q);
PhysicalDevice::QueueFamilyIndices findQueueFamilies_AI(vk::PhysicalDevice device_AI, vk::SurfaceKHR surface_AI);
bool supportsRequiredExtensions_AI(vk::PhysicalDevice candidate_AI, const std::vector<const char*>& requiredExtensions_AI);
bool supportsRequiredFeatures_AI(vk::PhysicalDevice candidate_AI, const DeviceConfig::FeatureChain& requiredFeatureChain_AI);
bool supportsSurface_AI(vk::PhysicalDevice candidate_AI, vk::SurfaceKHR surface_AI);
size_t deviceRank_AI(vk::PhysicalDeviceType deviceType_AI, const std::vector<vk::PhysicalDeviceType>& preferredTypes_AI);
PhysicalDevice pickPhysicalDevice_AI(Proof<const Instance> instance_AI, vk::SurfaceKHR surface_AI, const DeviceConfig& config_AI);
vk::UniqueDevice createLogicalDevice_AI(const PhysicalDevice& physicalDevice_AI, const DeviceConfig& config_AI);
std::unique_ptr<Fact<Device>> createDevice(Proof<const Instance> instance, Proof<const WindowSurface> surface, DeviceConfig config);

CommandPool createCommandPool(Proof<const Device> device, CommandPoolConfig_AI config_AI);

struct SwapChainSupportDetails;
struct ImageSharing_AI;
SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
vk::SurfaceFormatKHR preferredSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, vk::Format preferredFormat, vk::ColorSpaceKHR preferredColorSpace);
vk::PresentModeKHR preferredPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, vk::PresentModeKHR preferredMode);
vk::Extent2D resolveSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D framebufferExtent);
uint32_t clampImageCount(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t preferredImageCount);
ImageSharing_AI deriveImageSharing_AI(const PhysicalDevice::QueueFamilyIndices& families_AI);
std::vector<vk::UniqueImageView> makeImageViews_AI(vk::Device device_AI, const std::vector<vk::Image>& images_AI, vk::Format format_AI);
Swapchain createSwapchain(Proof<const Device> device, Proof<const WindowSurface> surface, vk::Extent2D framebufferExtent, const SwapchainConfig& config);

}  // namespace

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(  // _AI  (signature fixed by PFN_vkDebugUtilsMessengerCallbackEXT)
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    (void)messageType;
    (void)pUserData;
    std::string_view message(pCallbackData->pMessage);
    if (message.find("Device Extension:") != std::string_view::npos) return VK_FALSE;

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("Validation layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("Validation layer: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        spdlog::debug("Validation layer: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

std::vector<const char*> instanceExtensions_AI(const std::vector<const char*>& requiredExtensions_AI, bool validationEnabled_AI) {
    uint32_t glfwExtensionCount_AI = 0;
    const char** glfwExtensions_AI = glfwGetRequiredInstanceExtensions(&glfwExtensionCount_AI);
    if (glfwExtensions_AI == nullptr) {
        throw std::runtime_error("Failed to get GLFW Vulkan instance extensions!");
    }
    std::vector<const char*> extensions_AI(glfwExtensions_AI, glfwExtensions_AI + glfwExtensionCount_AI);
    extensions_AI.insert(extensions_AI.end(),
                         requiredExtensions_AI.begin(), requiredExtensions_AI.end());
    if (validationEnabled_AI) {
        extensions_AI.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions_AI;
}

Instance createInstance(Proof<const GlfwInitialization>, InstanceConfig config) {
    static vk::DynamicLoader dynamicLoader_AI;
    const auto vkGetInstanceProcAddr = dynamicLoader_AI.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    Instance self;
    self.config = std::move(config);
    const bool validationEnabled = !self.config.validationLayers.empty();

    const auto appInfo = vk::ApplicationInfo()
        .setPApplicationName(self.config.applicationName.c_str())
        .setApplicationVersion(self.config.applicationVersion)
        .setPEngineName(self.config.engineName.c_str())
        .setEngineVersion(self.config.engineVersion)
        .setApiVersion(self.config.apiVersion);

    const std::vector<const char*> extensions = instanceExtensions_AI(self.config.requiredExtensions, validationEnabled);

    auto createInfo = vk::InstanceCreateInfo()
        .setPApplicationInfo(&appInfo)
        .setPEnabledExtensionNames(extensions);

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    vk::ValidationFeaturesEXT validationFeatures;
    if (validationEnabled) {
        createInfo.setPEnabledLayerNames(self.config.validationLayers);
        validationFeatures.setEnabledValidationFeatures(self.config.validationFeatures);
        debugCreateInfo
            .setMessageSeverity(self.config.debugSeverity)
            .setMessageType(self.config.debugTypes)
            .setPfnUserCallback(debugCallback)
            .setPNext(&validationFeatures);
        createInfo.setPNext(&debugCreateInfo);
    }

    self.instance = vk::createInstanceUnique(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*self.instance);

    if (validationEnabled) {
        debugCreateInfo.pNext = nullptr;
        self.debugMessenger = self.instance->createDebugUtilsMessengerEXTUnique(debugCreateInfo);
    }

    return self;
}

WindowSurface createWindowSurface(Proof<const Instance> instance, Proof<const GlfwWindow> window) {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(VkInstance(*instance->instance), window->windowHandle, nullptr, &rawSurface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface!");
    auto surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(rawSurface), *instance->instance);
    return WindowSurface{
        std::move(instance),
        std::move(window),
        std::move(surface),
    };
}

bool familiesComplete(const PhysicalDevice::QueueFamilyIndices& q) noexcept {
    return q.graphicsFamily.has_value() && q.presentFamily.has_value() && q.transferFamily.has_value();
}

bool hasDedicatedTransfer(const PhysicalDevice::QueueFamilyIndices& q) noexcept {
    return q.transferFamily.has_value() && q.graphicsFamily.has_value()
        && q.transferFamily.value() != q.graphicsFamily.value();
}

std::set<uint32_t> uniqueFamilies(const PhysicalDevice::QueueFamilyIndices& q) {
    std::set<uint32_t> families;
    if (q.graphicsFamily.has_value()) families.insert(q.graphicsFamily.value());
    if (q.presentFamily.has_value())  families.insert(q.presentFamily.value());
    if (q.transferFamily.has_value()) families.insert(q.transferFamily.value());
    return families;
}

PhysicalDevice::QueueFamilyIndices findQueueFamilies_AI(vk::PhysicalDevice device_AI,
                                                        vk::SurfaceKHR surface_AI) {
    PhysicalDevice::QueueFamilyIndices indices_AI;
    const auto queueFamilies_AI = device_AI.getQueueFamilyProperties();
    bool unifiedGraphicsPresentFound_AI = false;
    bool transferOnlyFound_AI = false;

    for (uint32_t queueFamilyIndex_AI = 0; queueFamilyIndex_AI < queueFamilies_AI.size();
         ++queueFamilyIndex_AI) {
        const auto& queueFamily_AI = queueFamilies_AI[queueFamilyIndex_AI];
        const bool supportsGraphics_AI =
            static_cast<bool>(queueFamily_AI.queueFlags & vk::QueueFlagBits::eGraphics);
        const bool supportsCompute_AI =
            static_cast<bool>(queueFamily_AI.queueFlags & vk::QueueFlagBits::eCompute);
        const bool supportsTransfer_AI =
            static_cast<bool>(queueFamily_AI.queueFlags & vk::QueueFlagBits::eTransfer);
        const bool supportsPresent_AI = device_AI.getSurfaceSupportKHR(queueFamilyIndex_AI, surface_AI);

        if (supportsGraphics_AI && supportsPresent_AI && !unifiedGraphicsPresentFound_AI) {
            indices_AI.graphicsFamily = queueFamilyIndex_AI;
            indices_AI.presentFamily = queueFamilyIndex_AI;
            unifiedGraphicsPresentFound_AI = true;
        } else if (!unifiedGraphicsPresentFound_AI) {
            if (supportsGraphics_AI && !indices_AI.graphicsFamily) {
                indices_AI.graphicsFamily = queueFamilyIndex_AI;
            }
            if (supportsPresent_AI && !indices_AI.presentFamily) {
                indices_AI.presentFamily = queueFamilyIndex_AI;
            }
        }

        const bool transferOnly_AI = supportsTransfer_AI && !supportsGraphics_AI && !supportsCompute_AI;
        if (supportsTransfer_AI && !supportsGraphics_AI
            && (!indices_AI.transferFamily || (transferOnly_AI && !transferOnlyFound_AI))) {
            indices_AI.transferFamily = queueFamilyIndex_AI;
            transferOnlyFound_AI = transferOnly_AI;
        }
    }

    if (!indices_AI.transferFamily) {
        indices_AI.transferFamily = indices_AI.graphicsFamily;
    }

    return indices_AI;
}

bool supportsRequiredExtensions_AI(vk::PhysicalDevice candidate_AI, const std::vector<const char*>& requiredExtensions_AI) {
    const auto availableExtensions_AI = candidate_AI.enumerateDeviceExtensionProperties();
    std::set<std::string_view> missingExtensions_AI(requiredExtensions_AI.begin(),
                                                    requiredExtensions_AI.end());
    for (const auto& extension_AI : availableExtensions_AI) {
        missingExtensions_AI.erase(extension_AI.extensionName.data());
    }

    if (!missingExtensions_AI.empty()) {
        spdlog::debug("Missing device extensions:");
        for (const auto extension_AI : missingExtensions_AI) {
            spdlog::debug("  - {}", extension_AI);
        }
    }
    return missingExtensions_AI.empty();
}

bool supportsSurface_AI(vk::PhysicalDevice candidate_AI, vk::SurfaceKHR surface_AI) {
    const auto formats_AI = candidate_AI.getSurfaceFormatsKHR(surface_AI);
    const auto presentModes_AI = candidate_AI.getSurfacePresentModesKHR(surface_AI);
    return !formats_AI.empty() && !presentModes_AI.empty();
}

bool supportsRequiredFeatures_AI(vk::PhysicalDevice candidate_AI, const DeviceConfig::FeatureChain& requiredFeatureChain_AI) {
    DeviceConfig::FeatureChain supportedFeatureChain_AI;
    candidate_AI.getFeatures2(&supportedFeatureChain_AI.get<vk::PhysicalDeviceFeatures2>());

    const auto& requiredFeatures_AI = requiredFeatureChain_AI.get<vk::PhysicalDeviceFeatures2>().features;
    const auto& supportedFeatures_AI = supportedFeatureChain_AI.get<vk::PhysicalDeviceFeatures2>().features;
    const auto* required_AI = reinterpret_cast<const VkBool32*>(&requiredFeatures_AI);
    const auto* supported_AI = reinterpret_cast<const VkBool32*>(&supportedFeatures_AI);
    constexpr size_t featureCount_AI = sizeof(vk::PhysicalDeviceFeatures) / sizeof(VkBool32);
    for (size_t featureIndex_AI = 0; featureIndex_AI < featureCount_AI; ++featureIndex_AI) {
        if (required_AI[featureIndex_AI] && !supported_AI[featureIndex_AI]) {
            return false;
        }
    }

    const auto supportsFeature_AI = [&]<typename Feature_AI>(VkBool32 Feature_AI::* feature_AI) {
        return !(requiredFeatureChain_AI.get<Feature_AI>().*feature_AI)
            || supportedFeatureChain_AI.get<Feature_AI>().*feature_AI;
    };

    return supportsFeature_AI(&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters)
        && supportsFeature_AI(&vk::PhysicalDeviceVulkan12Features::timelineSemaphore)
        && supportsFeature_AI(&vk::PhysicalDeviceVulkan12Features::bufferDeviceAddress)
        && supportsFeature_AI(&vk::PhysicalDeviceSynchronization2Features::synchronization2)
        && supportsFeature_AI(&vk::PhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
        && supportsFeature_AI(&vk::PhysicalDeviceShaderObjectFeaturesEXT::shaderObject);
}

size_t deviceRank_AI(vk::PhysicalDeviceType deviceType_AI, const std::vector<vk::PhysicalDeviceType>& preferredTypes_AI) {
    const auto found_AI = std::find(preferredTypes_AI.begin(), preferredTypes_AI.end(), deviceType_AI);
    return static_cast<size_t>(found_AI - preferredTypes_AI.begin());
}

PhysicalDevice pickPhysicalDevice_AI(Proof<const Instance> instance_AI, vk::SurfaceKHR surface_AI,
                                     const DeviceConfig& config_AI) {
    const auto devices_AI = instance_AI->instance->enumeratePhysicalDevices();
    if (devices_AI.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    const vk::PhysicalDevice* selectedDevice_AI = nullptr;
    PhysicalDevice::QueueFamilyIndices selectedQueueFamilies_AI;

    for (const auto& candidate_AI : devices_AI) {
        const auto queueFamilies_AI = findQueueFamilies_AI(candidate_AI, surface_AI);
        const bool suitable_AI =
               familiesComplete(queueFamilies_AI)
            && supportsRequiredExtensions_AI(candidate_AI, config_AI.requiredExtensions)
            && supportsRequiredFeatures_AI(candidate_AI, config_AI.featureChain)
            && supportsSurface_AI(candidate_AI, surface_AI);
        if (!suitable_AI) {
            continue;
        }

        if (selectedDevice_AI == nullptr
            || deviceRank_AI(candidate_AI.getProperties().deviceType, config_AI.preferredDeviceTypes_AI)
                 < deviceRank_AI(selectedDevice_AI->getProperties().deviceType, config_AI.preferredDeviceTypes_AI)) {
            selectedDevice_AI = &candidate_AI;
            selectedQueueFamilies_AI = queueFamilies_AI;
        }
    }

    if (selectedDevice_AI == nullptr) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    const auto properties_AI = selectedDevice_AI->getProperties();
    spdlog::info("Selected GPU: {} (Type: {})", properties_AI.deviceName.data(),
                 vk::to_string(properties_AI.deviceType));

    if (properties_AI.deviceType == vk::PhysicalDeviceType::eCpu) {
        spdlog::warn("Rendering on a CPU software rasterizer ({}) — no hardware GPU met the "
                     "engine's requirements, so performance will be drastically lower than on a "
                     "real GPU. This usually means the GPU driver lacks a required extension or "
                     "feature; updating the GPU driver typically fixes it.",
                     properties_AI.deviceName.data());
    }

    return PhysicalDevice{
        std::move(instance_AI),
        *selectedDevice_AI,
        selectedQueueFamilies_AI,
    };
}

vk::UniqueDevice createLogicalDevice_AI(const PhysicalDevice& physicalDevice_AI, const DeviceConfig& config_AI) {
    const auto& queueFamilies_AI = physicalDevice_AI.queueFamilies;
    const auto families_AI = uniqueFamilies(queueFamilies_AI);
    const float queuePriority_AI = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos_AI;
    queueCreateInfos_AI.reserve(families_AI.size());
    for (const uint32_t queueFamily_AI : families_AI) {
        queueCreateInfos_AI.push_back(vk::DeviceQueueCreateInfo()
                                          .setQueueFamilyIndex(queueFamily_AI)
                                          .setQueueCount(1)
                                          .setPQueuePriorities(&queuePriority_AI));
    }

    const auto createInfo_AI = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueCreateInfos_AI)
        .setPEnabledExtensionNames(config_AI.requiredExtensions)
        .setPNext(&config_AI.featureChain.get<vk::PhysicalDeviceFeatures2>());

    auto device_AI = physicalDevice_AI.handle_AI.createDeviceUnique(createInfo_AI);

    return device_AI;
}

std::unique_ptr<Fact<Device>> createDevice(Proof<const Instance> instance,
                                           Proof<const WindowSurface> surface,
                                           DeviceConfig config) {
    PhysicalDevice physicalDevice = pickPhysicalDevice_AI(std::move(instance), *surface->surface, config);
    vk::UniqueDevice device = createLogicalDevice_AI(physicalDevice, config);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);

    const auto& queueFamilies = physicalDevice.queueFamilies;
    const vk::Queue graphicsQueue = device->getQueue(queueFamilies.graphicsFamily.value(), 0);
    const vk::Queue presentQueue  = device->getQueue(queueFamilies.presentFamily.value(), 0);
    const vk::Queue transferQueue = device->getQueue(queueFamilies.transferFamily.value(), 0);

    return std::make_unique<Fact<Device>>(
        [physicalDevice = std::move(physicalDevice), device = std::move(device),
         graphicsQueue, presentQueue, transferQueue]() mutable {
            return Device{
                .physicalDevice   = std::move(physicalDevice),
                .device           = std::move(device),
                .graphicsQueue_AI = graphicsQueue,
                .presentQueue_AI  = presentQueue,
                .transferQueue_AI = transferQueue,
            };
        });
}

CommandPool createCommandPool(Proof<const Device> device, CommandPoolConfig_AI config_AI) {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags            = config_AI.flags_AI;
    poolInfo.queueFamilyIndex = device->physicalDevice.queueFamilies.graphicsFamily.value();
    auto pool = device->device->createCommandPoolUnique(poolInfo);
    return CommandPool{
        std::move(device),
        std::move(pool)};
}

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR        capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR>   presentModes;
};

struct ImageSharing_AI {
    vk::SharingMode       mode_AI;
    std::vector<uint32_t> queueFamilyIndices_AI;
};

SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats      = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return details;
}

vk::SurfaceFormatKHR preferredSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats,
                                            vk::Format preferredFormat, vk::ColorSpaceKHR preferredColorSpace) {
    for (const auto& availableFormat : availableFormats)
        if (availableFormat.format == preferredFormat && availableFormat.colorSpace == preferredColorSpace)
            return availableFormat;
    for (const auto& availableFormat : availableFormats)
        if (availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) return availableFormat;
    spdlog::warn("No sRGB format available, using first available format");
    return availableFormats[0];
}

vk::PresentModeKHR preferredPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, vk::PresentModeKHR preferredMode) {
    for (const auto& mode : availablePresentModes)
        if (mode == preferredMode) return mode;
    if (preferredMode != vk::PresentModeKHR::eMailbox)
        for (const auto& mode : availablePresentModes)
            if (mode == vk::PresentModeKHR::eMailbox) return mode;
    return vk::PresentModeKHR::eFifo;  // guaranteed to be available
}

vk::Extent2D resolveSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D framebufferExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;
    vk::Extent2D actualExtent = framebufferExtent;
    actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

uint32_t clampImageCount(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t preferredImageCount) {
    uint32_t imageCount = std::max(preferredImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);
    return imageCount;
}

ImageSharing_AI deriveImageSharing_AI(const PhysicalDevice::QueueFamilyIndices& families_AI) {
    const uint32_t graphicsFamily_AI = families_AI.graphicsFamily.value();
    const uint32_t presentFamily_AI  = families_AI.presentFamily.value();
    if (graphicsFamily_AI == presentFamily_AI) {
        return {vk::SharingMode::eExclusive, {}};
    }
    return {vk::SharingMode::eConcurrent, {graphicsFamily_AI, presentFamily_AI}};
}

std::vector<vk::UniqueImageView> makeImageViews_AI(vk::Device device_AI,
                                                   const std::vector<vk::Image>& images_AI,
                                                   vk::Format format_AI) {
    std::vector<vk::UniqueImageView> imageViews_AI;
    imageViews_AI.reserve(images_AI.size());
    for (const vk::Image image_AI : images_AI) {
        const auto createInfo_AI = vk::ImageViewCreateInfo()
            .setImage(image_AI)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format_AI)
            .setComponents(vk::ComponentMapping())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0).setLevelCount(1).setBaseArrayLayer(0).setLayerCount(1));
        imageViews_AI.push_back(device_AI.createImageViewUnique(createInfo_AI));
    }
    return imageViews_AI;
}

Swapchain createSwapchain(Proof<const Device> device, Proof<const WindowSurface> surface,
                          vk::Extent2D framebufferExtent, const SwapchainConfig& config) {
    assert(framebufferExtent.width > 0 && framebufferExtent.height > 0 &&
           "createSwapchain: framebuffer extent must be non-zero");
    const vk::PhysicalDevice physicalDevice = device->physicalDevice.handle_AI;
    const vk::SurfaceKHR surfaceHandle = *surface->surface;

    const SwapChainSupportDetails support = querySwapchainSupport(physicalDevice, surfaceHandle);
    if (support.formats.empty() || support.presentModes.empty())
        throw std::runtime_error("Swapchain support is inadequate");

    const vk::SurfaceFormatKHR surfaceFormat = preferredSurfaceFormat(support.formats, config.preferredFormat, config.preferredColorSpace);
    const vk::PresentModeKHR   presentMode   = preferredPresentMode(support.presentModes, config.preferredPresentMode);
    const vk::Extent2D         extent        = resolveSwapExtent(support.capabilities, framebufferExtent);
    const uint32_t             imageCount    = clampImageCount(support.capabilities, config.preferredImageCount);
    const ImageSharing_AI      sharing_AI    = deriveImageSharing_AI(device->physicalDevice.queueFamilies);

    const auto createInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(surfaceHandle)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(config.imageUsage)
        .setImageSharingMode(sharing_AI.mode_AI)
        .setQueueFamilyIndices(sharing_AI.queueFamilyIndices_AI)
        .setPreTransform(support.capabilities.currentTransform)
        .setCompositeAlpha(config.compositeAlpha)
        .setPresentMode(presentMode)
        .setClipped(config.clipped);

    const vk::Device vkDevice = *device->device;
    vk::UniqueSwapchainKHR swapchain = vkDevice.createSwapchainKHRUnique(createInfo);
    std::vector<vk::Image> images = vkDevice.getSwapchainImagesKHR(*swapchain);
    std::vector<vk::UniqueImageView> imageViews = makeImageViews_AI(vkDevice, images, surfaceFormat.format);

    return Swapchain{
        .device      = std::move(device),
        .surface     = std::move(surface),
        .swapchain   = std::move(swapchain),
        .images      = std::move(images),
        .imageViews  = std::move(imageViews),
        .imageFormat = surfaceFormat.format,
        .extent      = extent,
    };
}

}  // namespace

Device::~Device()
{
    if (device) {
        device->waitIdle();
    }
}

VkAll VkAll::init(Proof<const GlfwInitialization> glfw,
                  Proof<const GlfwWindow> window,
                  InstanceConfig instanceConfig,
                  DeviceConfig deviceConfig,
                  CommandPoolConfig_AI commandPoolConfig,
                  SwapchainConfig swapchainConfig) {
    auto instance = std::make_unique<Fact<Instance>>(
        createInstance(std::move(glfw), std::move(instanceConfig)));
    auto surface = std::make_unique<Fact<WindowSurface>>(
        createWindowSurface(instance->proof(), window));
    auto device = createDevice(instance->proof(), surface->proof(), std::move(deviceConfig));
    auto commandPool = std::make_unique<Fact<CommandPool>>(
        createCommandPool(device->proof(), std::move(commandPoolConfig)));

    int framebufferWidth_AI = 0;
    int framebufferHeight_AI = 0;
    glfwGetFramebufferSize(window->windowHandle, &framebufferWidth_AI, &framebufferHeight_AI);
    const vk::Extent2D framebufferExtent_AI{static_cast<uint32_t>(framebufferWidth_AI),
                                            static_cast<uint32_t>(framebufferHeight_AI)};

    auto swapchain = std::make_unique<Fact<Swapchain>>(
        createSwapchain(device->proof(), surface->proof(), framebufferExtent_AI, swapchainConfig));

    return VkAll{
        .instance    = std::move(instance),
        .surface     = std::move(surface),
        .device      = std::move(device),
        .commandPool = std::move(commandPool),
        .swapchain   = std::move(swapchain),
    };
}
