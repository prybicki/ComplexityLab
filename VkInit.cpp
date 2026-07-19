//==============================================================================
// VkInit — definitions. See VkInit.hpp for the interface and the bring-up arc.
// Ported from ComplexityLabVibes/engine/render/VkBackend.{h,cpp}, trimmed to the
// instance / device / command-pool / swapchain bring-up path.
//==============================================================================

#include "engine/VkInit.hpp"
#include "engine/Platform.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cassert>
#include <cstring>
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

#ifndef NDEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
); // _AI  (signature fixed by PFN_vkDebugUtilsMessengerCallbackEXT)
#endif

// ───────────────────────── queue families ──────────────────────────

bool hasGraphics(const QueueFamilyIndices& q) noexcept;
bool familiesComplete(const QueueFamilyIndices& q) noexcept;
bool hasDedicatedTransfer(const QueueFamilyIndices& q) noexcept;
std::set<uint32_t> uniqueFamilies(const QueueFamilyIndices& q);
QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface);

// ───────────────────────── device build ────────────────────────────

bool checkDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*>& requiredExtensions);
bool isDeviceSuitable(const Device& self, vk::PhysicalDevice candidate);
void pickPhysicalDevice(Device& self);
void createLogicalDevice(Device& self);
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

Instance Instance::init(Proof<const GlfwInitialization>)
{
    static vk::DynamicLoader dynamicLoader_AI;
    const auto vkGetInstanceProcAddr = dynamicLoader_AI.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    const auto appInfo = vk::ApplicationInfo().setApiVersion(VK_MAKE_API_VERSION(0, 1, 4, 0));

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        throw std::runtime_error("Failed to get GLFW Vulkan instance extensions!");
    }
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    auto createInfo = vk::InstanceCreateInfo()
        .setPApplicationInfo(&appInfo);

    Instance self;

#ifndef NDEBUG
    const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    vk::ValidationFeaturesEXT validationFeatures;
    vk::ValidationFeatureEnableEXT enabledFeatures[] = {
        vk::ValidationFeatureEnableEXT::eSynchronizationValidation,
    };

    createInfo
        .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
        .setPpEnabledExtensionNames(extensions.data())
        .setEnabledLayerCount(static_cast<uint32_t>(validationLayers.size()))
        .setPpEnabledLayerNames(validationLayers.data());

    validationFeatures.setEnabledValidationFeatures(enabledFeatures);
    debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT()
        .setMessageSeverity(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
        .setMessageType(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
        .setPfnUserCallback(debugCallback);
    debugCreateInfo.setPNext(&validationFeatures);
    createInfo.setPNext(&debugCreateInfo);
    self.instance = vk::createInstanceUnique(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*self.instance);

    self.debugMessenger = self.instance->createDebugUtilsMessengerEXTUnique(
        vk::DebugUtilsMessengerCreateInfoEXT()
            .setMessageSeverity(
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
            .setMessageType(
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            .setPfnUserCallback(debugCallback));
#else
    createInfo
        .setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
        .setPpEnabledExtensionNames(extensions.data());
    self.instance = vk::createInstanceUnique(createInfo);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*self.instance);
#endif

    return self;
}

DeviceConfig::DeviceConfig() {
    requiredExtensions = {
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
    };
    vulkan11.shaderDrawParameters     = VK_TRUE;
    vulkan12.bufferDeviceAddress      = VK_TRUE;
    vulkan12.timelineSemaphore        = VK_TRUE;
    sync2.synchronization2            = VK_TRUE;
    dynamicRendering.dynamicRendering = VK_TRUE;
}


vk::UniqueSurfaceKHR createWindowSurface(const Instance& instance, const GlfwWindow& window) {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(VkInstance(*instance.instance), window.windowHandle, nullptr, &rawSurface) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface!");
    return vk::UniqueSurfaceKHR(vk::SurfaceKHR(rawSurface), *instance.instance);
}

std::unique_ptr<Device> makeDevice(vk::Instance instance, std::optional<vk::SurfaceKHR> surface, DeviceConfig config) {
    auto self = std::make_unique<Device>();
    self->config   = std::move(config);
    self->instance = instance;
    self->surface  = surface;

    if (surface.has_value()) {
        // Inject swapchain extension if not already present.
        auto& exts = self->config.requiredExtensions;
        bool hasSwapchain = std::any_of(exts.begin(), exts.end(), [](const char* e) {
            return std::strcmp(e, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
        });
        if (!hasSwapchain) exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    pickPhysicalDevice(*self);
    createLogicalDevice(*self);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*self->device);
    createQueueObjects(*self);
    return self;
}

Device::~Device() {
    // Drain the GPU before vkDestroyDevice (the spec requires every queue idle at
    // device destruction). Raw waitIdle: the mutex-locking drain that guarded
    // concurrent submits belongs to the runtime queue module, and at init teardown
    // no work is in flight.
    if (device) device->waitIdle();
}

std::unique_ptr<CommandPool> makeCommandPool(const Device& device) {
    auto self = std::make_unique<CommandPool>();
    self->device = rawHandle(device);

    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = device.queueFamilies.graphicsFamily.value();
    self->pool = self->device.createCommandPoolUnique(poolInfo);
    return self;
}

std::unique_ptr<Swapchain> makeSwapchain(Device& device, vk::SurfaceKHR surface,
                                         vk::Extent2D framebufferExtent, const SwapchainConfig& config) {
    assert(framebufferExtent.width > 0 && framebufferExtent.height > 0 &&
           "makeSwapchain: framebuffer extent must be non-zero");
    auto self = std::make_unique<Swapchain>();
    self->device  = &device;
    self->surface = surface;

    createSwapchainHandle(*self, config, framebufferExtent);
    createImageViews(*self);
    spdlog::info("Swapchain: {}x{}, {} images", self->extent.width, self->extent.height, self->images.size());
    return self;
}

vk::Instance rawHandle(const Instance& instance) { return *instance.instance; }
vk::Device   rawHandle(const Device& device)     { return *device.device; }

//══════════════════════ internal: helper definitions ══════════════════════

namespace {

// ───────────────────────── instance build ──────────────────────────

#ifndef NDEBUG
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

#endif

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

bool isDeviceSuitable(const Device& self, vk::PhysicalDevice candidate) {
    auto indices = findQueueFamilies(candidate, self.surface);
    bool extensionsSupported = checkDeviceExtensionSupport(candidate, self.config.requiredExtensions);

    bool swapChainAdequate = true;
    if (self.surface.has_value() && extensionsSupported) {
        auto formats = candidate.getSurfaceFormatsKHR(*self.surface);
        auto presentModes = candidate.getSurfacePresentModesKHR(*self.surface);
        swapChainAdequate = !formats.empty() && !presentModes.empty();
    }

    // ASSUMED, not checked here: the canonical baseline feature CHAIN
    // (timelineSemaphore / bufferDeviceAddress / sync2 / dynamicRendering, set by
    // DeviceConfig()) — a GPU missing one is deemed "suitable" and then throws at
    // createLogicalDevice rather than falling through to the next candidate. The
    // baseline is a hard requirement, so a missing baseline feature is fatal, not
    // a skip. Only the Vulkan 1.0 features below and the checkFeatureSupport hook
    // gate candidate selection.
    auto supportedFeatures = candidate.getFeatures();
    const auto* required  = reinterpret_cast<const VkBool32*>(&self.config.features);
    const auto* supported = reinterpret_cast<const VkBool32*>(&supportedFeatures);
    constexpr size_t featureCount = sizeof(vk::PhysicalDeviceFeatures) / sizeof(VkBool32);
    bool featuresSupported = true;
    for (size_t i = 0; i < featureCount; ++i) {
        if (required[i] && !supported[i]) { featuresSupported = false; break; }
    }

    // Caller-provided check for pNext-chain features (Vulkan 1.1+, extensions).
    if (featuresSupported && self.config.checkFeatureSupport)
        featuresSupported = self.config.checkFeatureSupport(candidate);

    // Headless: only need graphics. With surface: need graphics + present.
    bool queuesOk = self.surface.has_value() ? familiesComplete(indices) : hasGraphics(indices);

    return queuesOk && extensionsSupported && swapChainAdequate && featuresSupported;
}

void pickPhysicalDevice(Device& self) {
    auto devices = self.instance.enumeratePhysicalDevices();
    if (devices.empty()) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

    vk::PhysicalDevice selectedDevice;
    vk::PhysicalDevice fallbackDevice;

    for (const auto& candidate : devices) {
        if (!isDeviceSuitable(self, candidate)) continue;
        auto properties = candidate.getProperties();
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) { selectedDevice = candidate; break; }
        if (!fallbackDevice) fallbackDevice = candidate;
    }

    self.physicalDevice = selectedDevice ? selectedDevice : fallbackDevice;
    if (!self.physicalDevice) throw std::runtime_error("Failed to find a suitable GPU!");

    self.queueFamilies = findQueueFamilies(self.physicalDevice, self.surface);

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

void createLogicalDevice(Device& self) {
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    auto families = uniqueFamilies(self.queueFamilies);

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : families) {
        queueCreateInfos.push_back(
            vk::DeviceQueueCreateInfo().setQueueFamilyIndex(queueFamily).setQueueCount(1).setPQueuePriorities(&queuePriority));
    }

    // Build the pNext graph from config's own struct instances, so pointers
    // always refer to this config's current memory — safe across copies/moves.
    self.config.vulkan11.pNext         = &self.config.sync2;
    self.config.sync2.pNext            = &self.config.vulkan12;
    self.config.vulkan12.pNext         = &self.config.dynamicRendering;
    self.config.dynamicRendering.pNext = nullptr;
    auto* tail = reinterpret_cast<vk::BaseOutStructure*>(&self.config.dynamicRendering);
    for (auto* extra : self.config.extras) { tail->pNext = extra; tail = extra; }
    tail->pNext = nullptr;

    auto createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()))
        .setPQueueCreateInfos(queueCreateInfos.data())
        .setPEnabledFeatures(&self.config.features)
        .setEnabledExtensionCount(static_cast<uint32_t>(self.config.requiredExtensions.size()))
        .setPpEnabledExtensionNames(self.config.requiredExtensions.data())
        .setPNext(&self.config.vulkan11);

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
// family (headless / surfaceless build).
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
    SwapChainSupportDetails support = querySwapchainSupport(physicalDevice, self.surface);
    if (!isAdequate(support)) throw std::runtime_error("Swapchain support is inadequate");

    auto surfaceFormat = chooseSwapSurfaceFormat(support.formats, config);
    auto presentMode   = chooseSwapPresentMode(support.presentModes, config);
    self.extent        = chooseSwapExtent(support.capabilities, framebufferExtent);
    uint32_t imageCount = chooseImageCount(support.capabilities, config);

    auto createInfo = vk::SwapchainCreateInfoKHR()
        .setSurface(self.surface)
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
