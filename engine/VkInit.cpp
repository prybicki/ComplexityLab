//==============================================================================
// VkInit — definitions. See VkInit.hpp for the interface and the bring-up arc.
// Ported from ComplexityLabVibes/engine/render/VkBackend.{h,cpp}, trimmed to the
// instance / device / command-pool / swapchain bring-up path.
//==============================================================================

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

// Storage for the dynamic dispatcher — file scope, outside any namespace,
// because the macro opens namespace vk {} itself. Exactly one definition per
// program; it lives with instance creation.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {

// ───────────────────────── instance build ──────────────────────────

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
); // _AI  (signature fixed by PFN_vkDebugUtilsMessengerCallbackEXT)

// ───────────────────────── queue families ──────────────────────────

bool familiesComplete(const PhysicalDevice::QueueFamilyIndices& q) noexcept;
bool hasDedicatedTransfer(const PhysicalDevice::QueueFamilyIndices& q) noexcept;
std::set<uint32_t> uniqueFamilies(const PhysicalDevice::QueueFamilyIndices& q);
PhysicalDevice::QueueFamilyIndices findQueueFamilies_AI(vk::PhysicalDevice device_AI,
                                                        vk::SurfaceKHR surface_AI);

// ───────────────────────── device build ────────────────────────────

bool checkDeviceExtensionSupport_AI(vk::PhysicalDevice device_AI,
                                    const std::vector<const char*>& requiredExtensions_AI);
bool supportsRequiredFeatures_AI(vk::PhysicalDevice candidate_AI, const DeviceConfig& config_AI);
std::optional<PhysicalDevice::QueueFamilyIndices> findSuitableQueueFamilies_AI(
    vk::PhysicalDevice candidate_AI, vk::SurfaceKHR surface_AI, const DeviceConfig& config_AI);
PhysicalDevice pickPhysicalDevice_AI(Proof<const Instance> instance_AI, vk::SurfaceKHR surface_AI,
                                     const DeviceConfig& config_AI);
void createLogicalDevice_AI(Device& self_AI, const DeviceConfig& config_AI);
void createQueueObjects_AI(Device& self_AI);

// ───────────────────────── swapchain build ─────────────────────────

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR        capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR>   presentModes;
};

SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface);
bool isAdequate(const SwapChainSupportDetails& d) noexcept;
vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, const SwapchainConfig& config);
vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, const SwapchainConfig& config);
vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D framebufferExtent);
uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities, const SwapchainConfig& config);
void createSwapchainHandle(Swapchain& self, const SwapchainConfig& config, vk::Extent2D framebufferExtent);
void createImageViews(Swapchain& self);

}  // namespace

//══════════════════════════ public: bring-up ══════════════════════════

Instance Instance::init(Proof<const GlfwInitialization>, InstanceConfig config)
{
    static vk::DynamicLoader dynamicLoader_AI;
    const auto vkGetInstanceProcAddr = dynamicLoader_AI.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    Instance self;
    self.config = std::move(config);

    const auto appInfo = vk::ApplicationInfo()
        .setPApplicationName(self.config.applicationName.c_str())
        .setApplicationVersion(self.config.applicationVersion)
        .setPEngineName(self.config.engineName.c_str())
        .setEngineVersion(self.config.engineVersion)
        .setApiVersion(self.config.apiVersion);

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        throw std::runtime_error("Failed to get GLFW Vulkan instance extensions!");
    }
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.insert(extensions.end(), self.config.requiredExtensions.begin(), self.config.requiredExtensions.end());

    auto createInfo = vk::InstanceCreateInfo()
        .setPApplicationInfo(&appInfo)
        .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
        .setPpEnabledExtensionNames(extensions.data());

    const bool validationEnabled = !self.config.validationLayers.empty();
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    vk::ValidationFeaturesEXT validationFeatures;

    if (validationEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        createInfo
            .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
            .setPpEnabledExtensionNames(extensions.data())
            .setEnabledLayerCount(static_cast<uint32_t>(self.config.validationLayers.size()))
            .setPpEnabledLayerNames(self.config.validationLayers.data());

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

WindowSurface WindowSurface::init(Proof<const Instance> instance, Proof<const GlfwWindow> window) {
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

std::unique_ptr<Fact<Device>> Device::init(Proof<const Instance> instance,
                                           Proof<const WindowSurface> surface,
                                           DeviceConfig config) {
    const vk::SurfaceKHR surfaceHandle = *surface->surface;
    auto physicalDevice_AI = pickPhysicalDevice_AI(std::move(instance), surfaceHandle, config);

    auto self = std::make_unique<Fact<Device>>(
        [physicalDevice_AI = std::move(physicalDevice_AI)]() mutable {
            return Device{.physicalDevice = std::move(physicalDevice_AI)};
        });

    createLogicalDevice_AI(**self, config);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*(*self)->device);
    createQueueObjects_AI(**self);
    return self;
}

Device::~Device()
{
    if (device) {
        device->waitIdle();
    }
}

CommandPool CommandPool::init(Proof<const Device> device) {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = device->physicalDevice.queueFamilies.graphicsFamily.value();
    auto pool = device->device->createCommandPoolUnique(poolInfo);
    return CommandPool{
        std::move(device),
        std::move(pool)};
}

Swapchain Swapchain::init(Proof<const Device> device,
                          Proof<const WindowSurface> surface,
                          vk::Extent2D framebufferExtent, const SwapchainConfig& config) {
    assert(framebufferExtent.width > 0 && framebufferExtent.height > 0 &&
           "Swapchain::init: framebuffer extent must be non-zero");
    Swapchain self{
        std::move(device),
        std::move(surface)};

    createSwapchainHandle(self, config, framebufferExtent);
    createImageViews(self);
    spdlog::info("Swapchain: {}x{}, {} images", self.extent.width, self.extent.height, self.images.size());
    return self;
}

//══════════════════════ internal: helper definitions ══════════════════════

namespace {

// ───────────────────────── instance build ──────────────────────────

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

// ───────────────────────── queue families ──────────────────────────

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

// ───────────────────────── device build ────────────────────────────

bool checkDeviceExtensionSupport_AI(vk::PhysicalDevice device_AI,
                                    const std::vector<const char*>& requiredExtensions_AI) {
    const auto availableExtensions_AI = device_AI.enumerateDeviceExtensionProperties();
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

bool supportsRequiredFeatures_AI(vk::PhysicalDevice candidate_AI, const DeviceConfig& config_AI) {
    DeviceConfig::FeatureChain supportedFeatureChain_AI;
    candidate_AI.getFeatures2(&supportedFeatureChain_AI.get<vk::PhysicalDeviceFeatures2>());

    const auto& requiredFeatures_AI = config_AI.featureChain.get<vk::PhysicalDeviceFeatures2>().features;
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
        return !(config_AI.featureChain.get<Feature_AI>().*feature_AI)
            || supportedFeatureChain_AI.get<Feature_AI>().*feature_AI;
    };

    return supportsFeature_AI(&vk::PhysicalDeviceVulkan11Features::shaderDrawParameters)
        && supportsFeature_AI(&vk::PhysicalDeviceVulkan12Features::timelineSemaphore)
        && supportsFeature_AI(&vk::PhysicalDeviceVulkan12Features::bufferDeviceAddress)
        && supportsFeature_AI(&vk::PhysicalDeviceSynchronization2Features::synchronization2)
        && supportsFeature_AI(&vk::PhysicalDeviceDynamicRenderingFeatures::dynamicRendering)
        && supportsFeature_AI(&vk::PhysicalDeviceShaderObjectFeaturesEXT::shaderObject);
}

std::optional<PhysicalDevice::QueueFamilyIndices> findSuitableQueueFamilies_AI(
    vk::PhysicalDevice candidate_AI, vk::SurfaceKHR surface_AI, const DeviceConfig& config_AI) {
    auto queueFamilies_AI = findQueueFamilies_AI(candidate_AI, surface_AI);
    if (!familiesComplete(queueFamilies_AI)) {
        return std::nullopt;
    }
    if (!checkDeviceExtensionSupport_AI(candidate_AI, config_AI.requiredExtensions)) {
        return std::nullopt;
    }

    const auto formats_AI = candidate_AI.getSurfaceFormatsKHR(surface_AI);
    const auto presentModes_AI = candidate_AI.getSurfacePresentModesKHR(surface_AI);
    if (formats_AI.empty() || presentModes_AI.empty()) {
        return std::nullopt;
    }
    if (!supportsRequiredFeatures_AI(candidate_AI, config_AI)) {
        return std::nullopt;
    }

    return queueFamilies_AI;
}

PhysicalDevice pickPhysicalDevice_AI(Proof<const Instance> instance_AI, vk::SurfaceKHR surface_AI,
                                     const DeviceConfig& config_AI) {
    const auto devices_AI = instance_AI->instance->enumeratePhysicalDevices();
    if (devices_AI.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    vk::PhysicalDevice selectedDevice_AI;
    PhysicalDevice::QueueFamilyIndices selectedQueueFamilies_AI;
    vk::PhysicalDevice fallbackDevice_AI;
    PhysicalDevice::QueueFamilyIndices fallbackQueueFamilies_AI;

    for (const auto& candidate_AI : devices_AI) {
        auto queueFamilies_AI = findSuitableQueueFamilies_AI(candidate_AI, surface_AI, config_AI);
        if (!queueFamilies_AI) {
            continue;
        }

        const auto properties_AI = candidate_AI.getProperties();
        if (properties_AI.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            selectedDevice_AI = candidate_AI;
            selectedQueueFamilies_AI = std::move(*queueFamilies_AI);
            break;
        }
        if (!fallbackDevice_AI) {
            fallbackDevice_AI = candidate_AI;
            fallbackQueueFamilies_AI = std::move(*queueFamilies_AI);
        }
    }

    if (!selectedDevice_AI) {
        selectedDevice_AI = fallbackDevice_AI;
        selectedQueueFamilies_AI = std::move(fallbackQueueFamilies_AI);
    }
    if (!selectedDevice_AI) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    const auto properties_AI = selectedDevice_AI.getProperties();
    spdlog::info("Selected GPU: {} (Type: {})", properties_AI.deviceName.data(),
                 vk::to_string(properties_AI.deviceType));

    // A CPU-type device is a software rasterizer (e.g. llvmpipe): reaching it
    // means NO hardware GPU satisfied the engine's requirements, so we fell
    // through to software rendering — orders of magnitude slower. Warn loudly.
    if (properties_AI.deviceType == vk::PhysicalDeviceType::eCpu) {
        spdlog::warn("Rendering on a CPU software rasterizer ({}) — no hardware GPU met the "
                     "engine's requirements, so performance will be drastically lower than on a "
                     "real GPU. This usually means the GPU driver lacks a required extension or "
                     "feature; updating the GPU driver typically fixes it.",
                     properties_AI.deviceName.data());
    }

    return PhysicalDevice{
        std::move(instance_AI),
        selectedDevice_AI,
        std::move(selectedQueueFamilies_AI),
    };
}

void createLogicalDevice_AI(Device& self_AI, const DeviceConfig& config_AI) {
    const auto& queueFamilies_AI = self_AI.physicalDevice.queueFamilies;
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

    self_AI.device = self_AI.physicalDevice.handle_AI.createDeviceUnique(createInfo_AI);

    spdlog::info("Queue families: graphics={}, present={}, transfer={} (dedicated={})",
                 queueFamilies_AI.graphicsFamily.value(),
                 queueFamilies_AI.presentFamily.value(),
                 queueFamilies_AI.transferFamily.value(),
                 hasDedicatedTransfer(queueFamilies_AI));
}

// Obtain the raw queue handles the device just created. getQueue is idempotent;
// the submit / timeline / command-ring machinery that OPERATES these queues is a
// separate runtime module. present aliases graphics when they share a family.
void createQueueObjects_AI(Device& self_AI) {
    const auto& queueFamilies_AI = self_AI.physicalDevice.queueFamilies;
    self_AI.graphicsQueue_AI = self_AI.device->getQueue(queueFamilies_AI.graphicsFamily.value(), 0);
    self_AI.transferQueue_AI = self_AI.device->getQueue(queueFamilies_AI.transferFamily.value(), 0);
    self_AI.presentQueue_AI = self_AI.device->getQueue(queueFamilies_AI.presentFamily.value(), 0);
}

// ───────────────────────── swapchain build ─────────────────────────

SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;
    details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    details.formats      = physicalDevice.getSurfaceFormatsKHR(surface);
    details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    return details;
}

bool isAdequate(const SwapChainSupportDetails& d) noexcept { return !d.formats.empty() && !d.presentModes.empty(); }

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats, const SwapchainConfig& config) {
    for (const auto& availableFormat : availableFormats)
        if (availableFormat.format == config.preferredFormat && availableFormat.colorSpace == config.preferredColorSpace)
            return availableFormat;
    for (const auto& availableFormat : availableFormats)
        if (availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) return availableFormat;
    spdlog::warn("No sRGB format available, using first available format");
    return availableFormats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, const SwapchainConfig& config) {
    for (const auto& mode : availablePresentModes)
        if (mode == config.preferredPresentMode) return mode;
    if (config.preferredPresentMode != vk::PresentModeKHR::eMailbox)
        for (const auto& mode : availablePresentModes)
            if (mode == vk::PresentModeKHR::eMailbox) return mode;
    return vk::PresentModeKHR::eFifo;  // guaranteed to be available
}

vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D framebufferExtent) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) return capabilities.currentExtent;
    vk::Extent2D actualExtent = framebufferExtent;
    actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

uint32_t chooseImageCount(const vk::SurfaceCapabilitiesKHR& capabilities, const SwapchainConfig& config) {
    uint32_t imageCount = std::max(config.preferredImageCount, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);
    return imageCount;
}

void createSwapchainHandle(Swapchain& self, const SwapchainConfig& config, vk::Extent2D framebufferExtent) {
    auto physicalDevice = self.device->physicalDevice.handle_AI;
    SwapChainSupportDetails support = querySwapchainSupport(physicalDevice, *self.surface->surface);
    if (!isAdequate(support)) throw std::runtime_error("Swapchain support is inadequate");

    auto surfaceFormat = chooseSwapSurfaceFormat(support.formats, config);
    auto presentMode   = chooseSwapPresentMode(support.presentModes, config);
    self.extent        = chooseSwapExtent(support.capabilities, framebufferExtent);
    uint32_t imageCount = chooseImageCount(support.capabilities, config);

    auto createInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(*self.surface->surface)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(self.extent)
        .setImageArrayLayers(1)
        .setImageUsage(config.imageUsage);

    const auto& families = self.device->physicalDevice.queueFamilies;
    uint32_t graphicsFamily = families.graphicsFamily.value();
    uint32_t presentFamily  = families.presentFamily.value();
    uint32_t queueFamilyIndices[] = {graphicsFamily, presentFamily};
    if (graphicsFamily != presentFamily) {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                  .setQueueFamilyIndexCount(2)
                  .setPQueueFamilyIndices(queueFamilyIndices);
    } else {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(support.capabilities.currentTransform)
              .setCompositeAlpha(config.compositeAlpha)
              .setPresentMode(presentMode)
              .setClipped(config.clipped);

    auto vkDevice = *self.device->device;
    self.swapchain   = vkDevice.createSwapchainKHRUnique(createInfo);
    self.images      = vkDevice.getSwapchainImagesKHR(*self.swapchain);
    self.imageFormat = surfaceFormat.format;
}

void createImageViews(Swapchain& self) {
    self.imageViews.resize(self.images.size());
    auto vkDevice = *self.device->device;
    for (size_t i = 0; i < self.images.size(); i++) {
        auto createInfo = vk::ImageViewCreateInfo()
            .setImage(self.images[i])
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(self.imageFormat)
            .setComponents(vk::ComponentMapping())
            .setSubresourceRange(vk::ImageSubresourceRange()
                .setAspectMask(vk::ImageAspectFlagBits::eColor)
                .setBaseMipLevel(0).setLevelCount(1).setBaseArrayLayer(0).setLayerCount(1));
        self.imageViews[i] = vkDevice.createImageViewUnique(createInfo);
    }
}

}  // namespace
