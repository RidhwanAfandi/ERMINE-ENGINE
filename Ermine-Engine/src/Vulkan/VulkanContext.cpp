/* Start Header ************************************************************************/
/*!
\file       VulkanContext.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan context implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanContext.h"

// GLFW must be included AFTER vulkan.h (already included via VulkanTypes.h)
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdexcept>
#include <set>
#include <algorithm>
#include <cstring>

namespace Ermine::Vulkan
{

// ─── Validation layer ──────────────────────────────────────────────────────────
static constexpr const char* k_ValidationLayer = "VK_LAYER_KHRONOS_validation";

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::Init(const std::string& appName, GLFWwindow* window, bool enableValidation)
{
    m_ValidationEnabled = enableValidation;

    CreateInstance(appName, enableValidation);
    if (enableValidation)
        SetupDebugMessenger();
    CreateSurface(window);
    SelectPhysicalDevice();
    CreateLogicalDevice();
    CreateCommandPools();
    LoadExtensionFunctions();

    m_AdaptiveConfig = m_Caps.BuildAdaptiveConfig();

    // Log capabilities in debug builds
#ifdef EE_DEBUG
    EE_CORE_INFO("{}", m_Caps.Dump());
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::Shutdown()
{
    if (m_Device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(m_Device);

    if (m_TransferCmdPool) { vkDestroyCommandPool(m_Device, m_TransferCmdPool, nullptr); m_TransferCmdPool = VK_NULL_HANDLE; }
    if (m_ComputeCmdPool)  { vkDestroyCommandPool(m_Device, m_ComputeCmdPool,  nullptr); m_ComputeCmdPool  = VK_NULL_HANDLE; }
    if (m_GraphicsCmdPool) { vkDestroyCommandPool(m_Device, m_GraphicsCmdPool, nullptr); m_GraphicsCmdPool = VK_NULL_HANDLE; }

    vkDestroyDevice(m_Device, nullptr);
    m_Device = VK_NULL_HANDLE;

    if (m_Surface)
    {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    if (m_DebugMessenger && m_pfnDestroyDebugMsg)
    {
        m_pfnDestroyDebugMsg(m_Instance, m_DebugMessenger, nullptr);
        m_DebugMessenger = VK_NULL_HANDLE;
    }

    if (m_Instance)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateInstance
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::CreateInstance(const std::string& appName, bool enableValidation)
{
    if (enableValidation && !CheckValidationLayerSupport())
        throw std::runtime_error("VulkanContext: Validation layers requested but not available");

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "ERMINE-ENGINE";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_2;

    auto extensions = GetRequiredInstanceExtensions(enableValidation);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCI{};
    if (enableValidation)
    {
        createInfo.enabledLayerCount   = 1;
        createInfo.ppEnabledLayerNames = &k_ValidationLayer;

        debugCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCI.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCI.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCI.pfnUserCallback = DebugCallback;
        createInfo.pNext = &debugCI;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
}

// ─────────────────────────────────────────────────────────────────────────────
// SetupDebugMessenger
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::SetupDebugMessenger()
{
    m_pfnCreateDebugMsg = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
    m_pfnDestroyDebugMsg = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (!m_pfnCreateDebugMsg) return;

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = DebugCallback;
    ci.pUserData       = nullptr;

    m_pfnCreateDebugMsg(m_Instance, &ci, nullptr, &m_DebugMessenger);
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateSurface
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::CreateSurface(GLFWwindow* window)
{
    // Use GLFW's cross-platform surface creation
    VK_CHECK(glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface));
}

// ─────────────────────────────────────────────────────────────────────────────
// SelectPhysicalDevice
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::SelectPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
    if (count == 0)
        throw std::runtime_error("VulkanContext: No Vulkan-capable GPUs found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

    int bestScore = -1;
    for (auto dev : devices)
    {
        if (!IsDeviceSuitable(dev)) continue;
        int score = ScorePhysicalDevice(dev);
        if (score > bestScore)
        {
            bestScore  = score;
            m_PhysDevice = dev;
        }
    }

    if (m_PhysDevice == VK_NULL_HANDLE)
        throw std::runtime_error("VulkanContext: No suitable Vulkan GPU found");

    // Query full capabilities
    m_Caps.Query(m_PhysDevice, m_Surface);
    m_Caps.ValidateRequired();
}

// ─────────────────────────────────────────────────────────────────────────────
// ScorePhysicalDevice
// ─────────────────────────────────────────────────────────────────────────────

int VulkanContext::ScorePhysicalDevice(VkPhysicalDevice dev) const
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(dev, &memProps);

    int score = 0;

    // Discrete GPU strongly preferred
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)       score += 10000;
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) score += 1000;

    // More VRAM = better (in MB)
    uint64_t vramMB = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            vramMB += memProps.memoryHeaps[i].size / (1024ull * 1024ull);
    score += static_cast<int>(std::min(vramMB, uint64_t(32768)));  // cap at 32 GB contribution

    // Higher max image dimension = better
    score += static_cast<int>(props.limits.maxImageDimension2D / 256);

    // Prefer hardware with geometry shaders (needed for debug rendering)
    VkPhysicalDeviceFeatures feats;
    vkGetPhysicalDeviceFeatures(dev, &feats);
    if (feats.geometryShader)            score += 100;
    if (feats.samplerAnisotropy)         score += 50;
    if (feats.multiDrawIndirect)         score += 200;  // Core to our render architecture

    return score;
}

// ─────────────────────────────────────────────────────────────────────────────
// IsDeviceSuitable
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice dev) const
{
    // Must have swapchain extension
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

    for (const auto* required : k_RequiredDeviceExtensions)
    {
        bool found = false;
        for (const auto& e : exts)
            if (strcmp(e.extensionName, required) == 0) { found = true; break; }
        if (!found) return false;
    }

    // Must have graphics + present queues
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, queues.data());

    bool hasGraphics = false, hasPresent = false;
    for (uint32_t i = 0; i < qCount; ++i)
    {
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphics = true;
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_Surface, &presentSupport);
        if (presentSupport) hasPresent = true;
    }
    if (!hasGraphics || !hasPresent) return false;

    // Must have at least one surface format and present mode
    uint32_t fmtCount = 0, modeCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_Surface, &fmtCount, nullptr);
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_Surface, &modeCount, nullptr);

    return fmtCount > 0 && modeCount > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateLogicalDevice
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::CreateLogicalDevice()
{
    const auto& qf = m_Caps.GetQueueFamilies();

    // Collect unique queue family indices
    std::set<uint32_t> uniqueFamilies;
    uniqueFamilies.insert(qf.graphicsFamily.value());
    uniqueFamilies.insert(qf.presentFamily.value());
    uniqueFamilies.insert(qf.computeFamily.value());
    if (qf.HasDedicatedTransfer())
        uniqueFamilies.insert(qf.transferFamily.value());

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    for (uint32_t family : uniqueFamilies)
    {
        VkDeviceQueueCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci.queueFamilyIndex = family;
        ci.queueCount       = 1;
        ci.pQueuePriorities = &priority;
        queueCIs.push_back(ci);
    }

    // ── Enable features ───────────────────────────────────────────────────────
    const auto& caps = m_Caps;

    VkPhysicalDeviceFeatures features{};
    features.multiDrawIndirect          = caps.GetFeatures().multiDrawIndirect          ? VK_TRUE : VK_FALSE;
    features.drawIndirectFirstInstance  = caps.GetFeatures().drawIndirectFirstInstance  ? VK_TRUE : VK_FALSE;
    features.fragmentStoresAndAtomics   = caps.GetFeatures().fragmentStoresAndAtomics   ? VK_TRUE : VK_FALSE;
    features.samplerAnisotropy          = caps.GetFeatures().samplerAnisotropy          ? VK_TRUE : VK_FALSE;
    features.depthClamp                 = caps.GetFeatures().depthClamp                 ? VK_TRUE : VK_FALSE;
    features.geometryShader             = caps.GetFeatures().geometryShader             ? VK_TRUE : VK_FALSE;
    features.independentBlend           = caps.GetFeatures().independentBlend           ? VK_TRUE : VK_FALSE;
    features.fillModeNonSolid           = caps.GetFeatures().fillModeNonSolid           ? VK_TRUE : VK_FALSE;
    features.wideLines                  = caps.GetFeatures().wideLines                  ? VK_TRUE : VK_FALSE;
    features.multiViewport              = caps.GetFeatures().multiViewport              ? VK_TRUE : VK_FALSE;
    features.pipelineStatisticsQuery    = caps.GetFeatures().pipelineStatisticsQuery    ? VK_TRUE : VK_FALSE;
    features.shaderStorageImageWriteWithoutFormat = VK_TRUE;

    // ── Vulkan 1.2 features chain ─────────────────────────────────────────────
    VkPhysicalDeviceVulkan12Features vk12Features{};
    vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    if (caps.GetFeatures().descriptorIndexing)
    {
        vk12Features.descriptorIndexing                           = VK_TRUE;
        vk12Features.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
        vk12Features.shaderStorageBufferArrayNonUniformIndexing   = VK_TRUE;
        vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vk12Features.descriptorBindingVariableDescriptorCount     = VK_TRUE;
        vk12Features.runtimeDescriptorArray                       = VK_TRUE;
    }
    if (caps.GetFeatures().timelineSemaphores)
        vk12Features.timelineSemaphore = VK_TRUE;
    if (caps.GetFeatures().bufferDeviceAddress)
        vk12Features.bufferDeviceAddress = VK_TRUE;

    // ── Collect required device extensions ────────────────────────────────────
    std::vector<const char*> enabledExts(
        std::begin(k_RequiredDeviceExtensions),
        std::end(k_RequiredDeviceExtensions)
    );

    // Optional but valuable extensions
    auto addIfPresent = [&](const char* name) {
        if (caps.HasExtension(name))
            enabledExts.push_back(name);
    };
    addIfPresent("VK_KHR_dynamic_rendering");
    addIfPresent("VK_EXT_descriptor_indexing");
    addIfPresent("VK_KHR_buffer_device_address");
    addIfPresent("VK_EXT_memory_budget");
    addIfPresent("VK_KHR_shader_float16_int8");
    addIfPresent("VK_EXT_calibrated_timestamps");
    addIfPresent("VK_KHR_calibrated_timestamps");
    addIfPresent("VK_EXT_mesh_shader");
    addIfPresent("VK_KHR_multiview");

    // Debug utils for object naming
    addIfPresent("VK_EXT_debug_utils");

    // ── Create the logical device ─────────────────────────────────────────────
    VkDeviceCreateInfo deviceCI{};
    deviceCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext                   = &vk12Features;
    deviceCI.queueCreateInfoCount    = static_cast<uint32_t>(queueCIs.size());
    deviceCI.pQueueCreateInfos       = queueCIs.data();
    deviceCI.pEnabledFeatures        = &features;
    deviceCI.enabledExtensionCount   = static_cast<uint32_t>(enabledExts.size());
    deviceCI.ppEnabledExtensionNames = enabledExts.data();

    if (m_ValidationEnabled)
    {
        deviceCI.enabledLayerCount   = 1;
        deviceCI.ppEnabledLayerNames = &k_ValidationLayer;
    }

    VK_CHECK(vkCreateDevice(m_PhysDevice, &deviceCI, nullptr, &m_Device));

    // Retrieve queue handles
    vkGetDeviceQueue(m_Device, qf.graphicsFamily.value(), 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, qf.presentFamily.value(),  0, &m_PresentQueue);
    vkGetDeviceQueue(m_Device, qf.computeFamily.value(),  0, &m_ComputeQueue);

    if (qf.HasDedicatedTransfer())
        vkGetDeviceQueue(m_Device, qf.transferFamily.value(), 0, &m_TransferQueue);
    else
        m_TransferQueue = m_GraphicsQueue;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateCommandPools
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::CreateCommandPools()
{
    const auto& qf = m_Caps.GetQueueFamilies();

    auto createPool = [&](uint32_t family, VkCommandPoolCreateFlags flags) -> VkCommandPool
    {
        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = family;
        ci.flags            = flags;
        VkCommandPool pool;
        VK_CHECK(vkCreateCommandPool(m_Device, &ci, nullptr, &pool));
        return pool;
    };

    // Graphics pool: allow per-buffer reset (for per-frame rerecording)
    m_GraphicsCmdPool = createPool(
        qf.graphicsFamily.value(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );

    // Compute pool
    m_ComputeCmdPool = createPool(
        qf.computeFamily.value(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    );

    // Transfer pool: transient flag for short-lived upload buffers
    uint32_t transferFamily = qf.HasDedicatedTransfer()
                                ? qf.transferFamily.value()
                                : qf.graphicsFamily.value();
    m_TransferCmdPool = createPool(
        transferFamily,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadExtensionFunctions
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::LoadExtensionFunctions()
{
#define LOAD_INSTANCE(name) \
    m_pfn##name = reinterpret_cast<PFN_vk##name>( \
        vkGetInstanceProcAddr(m_Instance, "vk" #name))

    LOAD_INSTANCE(SetDebugUtilsObjectNameEXT);
    LOAD_INSTANCE(CmdBeginDebugUtilsLabelEXT);
    LOAD_INSTANCE(CmdEndDebugUtilsLabelEXT);
    LOAD_INSTANCE(CmdInsertDebugUtilsLabelEXT);

#undef LOAD_INSTANCE
}

// ─────────────────────────────────────────────────────────────────────────────
// ImmediateSubmit
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::ImmediateSubmit(
    VkCommandPool pool, VkQueue queue,
    std::function<void(VkCommandBuffer)> fn) const
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    fn(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    // Create a fence for synchronization
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    VK_CHECK(vkCreateFence(m_Device, &fenceCI, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(m_Device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_Device, fence, nullptr);
    vkFreeCommandBuffers(m_Device, pool, 1, &cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug utilities
// ─────────────────────────────────────────────────────────────────────────────

void VulkanContext::BeginDebugLabel(VkCommandBuffer cmd, const char* label,
                                    float r, float g, float b) const
{
#ifdef EE_DEBUG
    if (!m_pfnBeginLabel) return;
    VkDebugUtilsLabelEXT info{};
    info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = label;
    info.color[0]   = r; info.color[1] = g; info.color[2] = b; info.color[3] = 1.f;
    m_pfnBeginLabel(cmd, &info);
#endif
}

void VulkanContext::EndDebugLabel(VkCommandBuffer cmd) const
{
#ifdef EE_DEBUG
    if (m_pfnEndLabel) m_pfnEndLabel(cmd);
#endif
}

void VulkanContext::InsertDebugLabel(VkCommandBuffer cmd, const char* label) const
{
#ifdef EE_DEBUG
    if (!m_pfnInsertLabel) return;
    VkDebugUtilsLabelEXT info{};
    info.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = label;
    m_pfnInsertLabel(cmd, &info);
#endif
}

void VulkanContext::SetDebugName(VkObjectType type, uint64_t handle, const char* name) const
{
#ifdef EE_DEBUG
    if (!m_pfnSetDebugName) return;
    VkDebugUtilsObjectNameInfoEXT info{};
    info.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType   = type;
    info.objectHandle = handle;
    info.pObjectName  = name;
    m_pfnSetDebugName(m_Device, &info);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanContext::CheckValidationLayerSupport()
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const auto& layer : layers)
        if (strcmp(layer.layerName, k_ValidationLayer) == 0)
            return true;
    return false;
}

std::vector<const char*> VulkanContext::GetRequiredInstanceExtensions(bool enableValidation)
{
    // Get GLFW's required extensions
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    if (enableValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    // Always request the portability enumeration extension if available
    // (required on newer Vulkan SDK to avoid VK_ERROR_INCOMPATIBLE_DRIVER on macOS)
    uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, available.data());

    for (const auto& ext : available)
        if (strcmp(ext.extensionName, "VK_KHR_get_physical_device_properties2") == 0)
        {
            extensions.push_back("VK_KHR_get_physical_device_properties2");
            break;
        }

    return extensions;
}

// ─────────────────────────────────────────────────────────────────────────────
// DebugCallback
// ─────────────────────────────────────────────────────────────────────────────

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
    VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*                                       /*pUserData*/)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        // In engine context, would call EE_CORE_ERROR
        // For portability, using stderr
        fprintf(stderr, "[Vulkan ERROR] %s\n", pCallbackData->pMessage);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        fprintf(stderr, "[Vulkan WARN]  %s\n", pCallbackData->pMessage);
    }

    return VK_FALSE; // Do not abort the Vulkan call
}

} // namespace Ermine::Vulkan
