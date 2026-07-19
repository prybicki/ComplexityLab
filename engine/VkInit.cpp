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
#include <string>
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

bool hasGraphics(const QueueFamilyIndices& q) noexcept;
bool familiesComplete(const QueueFamilyIndices& q) noexcept;
bool hasDedicatedTransfer(const QueueFamilyIndices& q) noexcept;
std::set<uint32_t> uniqueFamilies(const QueueFamilyIndices& q);
QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface);

// ───────────────────────── device build ────────────────────────────

bool checkDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& requiredExtensions);
bool isDeviceSuitable(vk::PhysicalDevice candidate, std::optional<vk::SurfaceKHR> surface,
                      const DeviceConfig& config);
void pickPhysicalDevice(Device& self, std::optional<vk::SurfaceKHR> surface,
                        const DeviceConfig& config);
void createLogicalDevice(Device& self, const DeviceConfig& config);
void createQueueObjects(Device& self);

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

Device::Device(Proof<const Instance> instance)
    : instance(std::move(instance)) {}

std::unique_ptr<Fact<Device>> makeDevice(Proof<const Instance> instance,
                                         std::optional<Proof<const WindowSurface>> surface,
                                         DeviceConfig config) {
    std::optional<vk::SurfaceKHR> surfaceHandle;
    if (surface.has_value()) surfaceHandle = rawHandle(**surface);

    auto self = std::make_unique<Fact<Device>>(
        [instance = std::move(instance)]() mutable {
            return Device(std::move(instance));
        });

    pickPhysicalDevice(**self, surfaceHandle, config);
    createLogicalDevice(**self, config);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*(*self)->device);
    createQueueObjects(**self);
    return self;
}

Device::~Device() {
    // Drain the GPU before vkDestroyDevice (the spec requires every queue idle at
    // device destruction). Raw waitIdle: the mutex-locking drain that guarded
    // concurrent submits belongs to the runtime queue module, and at init teardown
    // no work is in flight.
    if (device) device->waitIdle();
}

std::unique_ptr<CommandPool> makeCommandPool(Proof<const Device> device) {
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = device->queueFamilies.graphicsFamily.value();
    auto pool = rawHandle(*device).createCommandPoolUnique(poolInfo);
    return std::make_unique<CommandPool>(
        std::move(device),
        std::move(pool));
}

std::unique_ptr<Swapchain> makeSwapchain(Proof<const Device> device,
                                         Proof<const WindowSurface> surface,
                                         vk::Extent2D framebufferExtent, const SwapchainConfig& config) {
    assert(framebufferExtent.width > 0 && framebufferExtent.height > 0 &&
           "makeSwapchain: framebuffer extent must be non-zero");
    auto self = std::make_unique<Swapchain>(
        std::move(device),
        std::move(surface));

    createSwapchainHandle(*self, config, framebufferExtent);
    createImageViews(*self);
    spdlog::info("Swapchain: {}x{}, {} images", self->extent.width, self->extent.height, self->images.size());
    return self;
}

vk::Instance rawHandle(const Instance& instance) { return *instance.instance; }
vk::SurfaceKHR rawHandle(const WindowSurface& surface) { return *surface.surface; }
vk::Device   rawHandle(const Device& device)     { return *device.device; }

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

bool hasGraphics(const QueueFamilyIndices& q) noexcept { return q.graphicsFamily.has_value(); }

bool familiesComplete(const QueueFamilyIndices& q) noexcept {
    return q.graphicsFamily.has_value() && q.presentFamily.has_value() && q.transferFamily.has_value();
}

bool hasDedicatedTransfer(const QueueFamilyIndices& q) noexcept {
    return q.transferFamily.has_value() && q.graphicsFamily.has_value()
        && q.transferFamily.value() != q.graphicsFamily.value();
}

std::set<uint32_t> uniqueFamilies(const QueueFamilyIndices& q) {
    std::set<uint32_t> families;
    if (q.graphicsFamily.has_value()) families.insert(q.graphicsFamily.value());
    if (q.presentFamily.has_value())  families.insert(q.presentFamily.value());
    if (q.transferFamily.has_value()) families.insert(q.transferFamily.value());
    return families;
}

QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface) {
    QueueFamilyIndices indices;
    auto queueFamilies = device.getQueueFamilyProperties();

    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) indices.graphicsFamily = i;

        // Present family: PREFER the graphics family (a unified graphics+present
        // queue lets the swapchain stay EXCLUSIVE instead of CONCURRENT), else
        // take the first present-capable family.
        if (surface.has_value() && device.getSurfaceSupportKHR(i, *surface) &&
            (!indices.presentFamily.has_value() ||
             (indices.graphicsFamily.has_value() && i == indices.graphicsFamily.value()))) {
            indices.presentFamily = i;
        }

        // Prefer a dedicated transfer queue (transfer-capable, no graphics/compute).
        if ((queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
            !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) &&
            !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
            !indices.transferFamily.has_value()) {
            indices.transferFamily = i;
        }
        i++;
    }

    // Fallback: use graphics queue for transfers.
    if (!indices.transferFamily.has_value() && indices.graphicsFamily.has_value())
        indices.transferFamily = indices.graphicsFamily;

    return indices;
}

// ───────────────────────── device build ────────────────────────────

bool checkDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& requiredExtensions) {
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensionsSet(requiredExtensions.begin(), requiredExtensions.end());
    for (const auto& extension : availableExtensions) requiredExtensionsSet.erase(extension.extensionName.data());

    if (!requiredExtensionsSet.empty()) {
        spdlog::debug("Missing device extensions:");
        for (const auto& ext : requiredExtensionsSet) spdlog::debug("  - {}", ext);
    }
    return requiredExtensionsSet.empty();
}

bool isDeviceSuitable(vk::PhysicalDevice candidate, std::optional<vk::SurfaceKHR> surface,
                      const DeviceConfig& config) {
    auto indices = findQueueFamilies(candidate, surface);
    bool extensionsSupported = checkDeviceExtensionSupport(candidate, config.requiredExtensions);

    bool swapChainAdequate = true;
    if (surface.has_value() && extensionsSupported) {
        auto formats = candidate.getSurfaceFormatsKHR(*surface);
        auto presentModes = candidate.getSurfacePresentModesKHR(*surface);
        swapChainAdequate = !formats.empty() && !presentModes.empty();
    }

    DeviceConfig::FeatureChain supportedFeatureChain;
    candidate.getFeatures2(&supportedFeatureChain.get<vk::PhysicalDeviceFeatures2>());

    const auto& requiredFeatures = config.featureChain.get<vk::PhysicalDeviceFeatures2>().features;
    const auto& supportedFeatures = supportedFeatureChain.get<vk::PhysicalDeviceFeatures2>().features;
    const auto* required  = reinterpret_cast<const VkBool32*>(&requiredFeatures);
    const auto* supported = reinterpret_cast<const VkBool32*>(&supportedFeatures);
    constexpr size_t featureCount = sizeof(vk::PhysicalDeviceFeatures) / sizeof(VkBool32);
    bool featuresSupported = true;
    for (size_t i = 0; i < featureCount; ++i) {
        if (required[i] && !supported[i]) { featuresSupported = false; break; }
    }

    featuresSupported = featuresSupported
        && (!config.featureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters
            || supportedFeatureChain.get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters)
        && (!config.featureChain.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore
            || supportedFeatureChain.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore)
        && (!config.featureChain.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress
            || supportedFeatureChain.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress)
        && (!config.featureChain.get<vk::PhysicalDeviceSynchronization2Features>().synchronization2
            || supportedFeatureChain.get<vk::PhysicalDeviceSynchronization2Features>().synchronization2)
        && (!config.featureChain.get<vk::PhysicalDeviceDynamicRenderingFeatures>().dynamicRendering
            || supportedFeatureChain.get<vk::PhysicalDeviceDynamicRenderingFeatures>().dynamicRendering)
        && (!config.featureChain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>().shaderObject
            || supportedFeatureChain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>().shaderObject);

    bool queuesOk = surface.has_value() ? familiesComplete(indices) : hasGraphics(indices);

    return queuesOk && extensionsSupported && swapChainAdequate && featuresSupported;
}

void pickPhysicalDevice(Device& self, std::optional<vk::SurfaceKHR> surface,
                        const DeviceConfig& config) {
    auto devices = rawHandle(*self.instance).enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

    vk::PhysicalDevice selectedDevice;
    vk::PhysicalDevice fallbackDevice;

    for (const auto& candidate : devices) {
        if (!isDeviceSuitable(candidate, surface, config)) continue;
        auto properties = candidate.getProperties();
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) { selectedDevice = candidate; break; }
        if (!fallbackDevice) fallbackDevice = candidate;
    }

    self.physicalDevice = selectedDevice ? selectedDevice : fallbackDevice;
    if (!self.physicalDevice) throw std::runtime_error("Failed to find a suitable GPU!");

    self.queueFamilies = findQueueFamilies(self.physicalDevice, surface);

    auto properties = self.physicalDevice.getProperties();
    spdlog::info("Selected GPU: {} (Type: {})", properties.deviceName.data(), vk::to_string(properties.deviceType));

    // A CPU-type device is a software rasterizer (e.g. llvmpipe): reaching it
    // means NO hardware GPU satisfied the engine's requirements, so we fell
    // through to software rendering — orders of magnitude slower. Warn loudly.
    if (properties.deviceType == vk::PhysicalDeviceType::eCpu) {
        spdlog::warn("Rendering on a CPU software rasterizer ({}) — no hardware GPU met the "
                     "engine's requirements, so performance will be drastically lower than on a "
                     "real GPU. This usually means the GPU driver lacks a required extension or "
                     "feature; updating the GPU driver typically fixes it.",
                     properties.deviceName.data());
    }
}

void createLogicalDevice(Device& self, const DeviceConfig& config) {
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    auto families = uniqueFamilies(self.queueFamilies);

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : families) {
        queueCreateInfos.push_back(
            vk::DeviceQueueCreateInfo().setQueueFamilyIndex(queueFamily).setQueueCount(1).setPQueuePriorities(&queuePriority));
    }

    auto createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
        .setPQueueCreateInfos(queueCreateInfos.data())
        .setPEnabledFeatures(nullptr)
        .setEnabledExtensionCount(static_cast<uint32_t>(config.requiredExtensions.size()))
        .setPpEnabledExtensionNames(config.requiredExtensions.data())
        .setPNext(&config.featureChain.get<vk::PhysicalDeviceFeatures2>());

    self.device = self.physicalDevice.createDeviceUnique(createInfo);

    spdlog::info("Queue families: graphics={}, present={}, transfer={} (dedicated={})",
                 self.queueFamilies.graphicsFamily.value(),
                 self.queueFamilies.presentFamily.has_value() ? static_cast<int>(self.queueFamilies.presentFamily.value()) : -1,
                 self.queueFamilies.transferFamily.value(),
                 hasDedicatedTransfer(self.queueFamilies));
}

// Obtain the raw queue handles the device just created. getQueue is idempotent;
// the submit / timeline / command-ring machinery that OPERATES these queues is a
// separate runtime module. present aliases graphics when there is no present
// family.
void createQueueObjects(Device& self) {
    const vk::Queue graphicsHandle = self.device->getQueue(self.queueFamilies.graphicsFamily.value(), 0);
    const vk::Queue transferHandle = self.device->getQueue(self.queueFamilies.transferFamily.value(), 0);
    const vk::Queue presentHandle  = self.queueFamilies.presentFamily.has_value()
        ? self.device->getQueue(self.queueFamilies.presentFamily.value(), 0)
        : graphicsHandle;

    self.graphicsQueue_AI = graphicsHandle;
    self.transferQueue_AI = transferHandle;
    self.presentQueue_AI  = presentHandle;
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
    auto physicalDevice = self.device->physicalDevice;
    SwapChainSupportDetails support = querySwapchainSupport(physicalDevice, rawHandle(*self.surface));
    if (!isAdequate(support)) throw std::runtime_error("Swapchain support is inadequate");

    auto surfaceFormat = chooseSwapSurfaceFormat(support.formats, config);
    auto presentMode   = chooseSwapPresentMode(support.presentModes, config);
    self.extent        = chooseSwapExtent(support.capabilities, framebufferExtent);
    uint32_t imageCount = chooseImageCount(support.capabilities, config);

    auto createInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(rawHandle(*self.surface))
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(self.extent)
        .setImageArrayLayers(1)
        .setImageUsage(config.imageUsage);

    const auto& families = self.device->queueFamilies;
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

    auto vkDevice = rawHandle(*self.device);
    self.swapchain   = vkDevice.createSwapchainKHRUnique(createInfo);
    self.images      = vkDevice.getSwapchainImagesKHR(*self.swapchain);
    self.imageFormat = surfaceFormat.format;
}

void createImageViews(Swapchain& self) {
    self.imageViews.resize(self.images.size());
    auto vkDevice = rawHandle(*self.device);
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
