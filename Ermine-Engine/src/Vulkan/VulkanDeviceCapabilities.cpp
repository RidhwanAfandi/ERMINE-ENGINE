/* Start Header ************************************************************************/
/*!
\file       VulkanDeviceCapabilities.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      GPU capability query and adaptive buffer sizing implementation.
            See VulkanDeviceCapabilities.h for the full design rationale.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanDeviceCapabilities.h"

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <cstring>

// Shadow map resolution tiers keyed to AdapterTier
static constexpr uint32_t SHADOW_RES_TABLE[] = { 1024, 1024, 2048, 4096 };
// Shadow layer counts keyed to AdapterTier
static constexpr uint32_t SHADOW_LAYER_TABLE[] = { 16, 32, 64, 128 };

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// Query
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::Query(VkPhysicalDevice physDevice, VkSurfaceKHR surface)
{
    m_PhysDevice = physDevice;

    // Core properties
    vkGetPhysicalDeviceProperties(physDevice, &m_Properties);

    // Core features (Vulkan 1.0)
    vkGetPhysicalDeviceFeatures(physDevice, &m_DeviceFeatures);

    QueryExtensions(physDevice);
    QueryFeatures(physDevice);
    QueryMemory(physDevice);
    QuerySwapchainSupport(physDevice, surface);
    QueryQueueFamilies(physDevice, surface);
    DetermineAdapterTier();
}

// ─────────────────────────────────────────────────────────────────────────────
// QueryExtensions
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::QueryExtensions(VkPhysicalDevice dev)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());

    m_Extensions.reserve(count);
    for (const auto& e : exts)
        m_Extensions.emplace_back(e.extensionName);
}

// ─────────────────────────────────────────────────────────────────────────────
// QueryFeatures
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::QueryFeatures(VkPhysicalDevice dev)
{
    // Populate SupportedFeatures from core Vulkan 1.0 device features
    m_Features.multiDrawIndirect         = m_DeviceFeatures.multiDrawIndirect        == VK_TRUE;
    m_Features.drawIndirectFirstInstance = m_DeviceFeatures.drawIndirectFirstInstance == VK_TRUE;
    m_Features.fragmentStoresAndAtomics  = m_DeviceFeatures.fragmentStoresAndAtomics == VK_TRUE;
    m_Features.samplerAnisotropy         = m_DeviceFeatures.samplerAnisotropy        == VK_TRUE;
    m_Features.depthClamp                = m_DeviceFeatures.depthClamp               == VK_TRUE;
    m_Features.geometryShader            = m_DeviceFeatures.geometryShader           == VK_TRUE;
    m_Features.independentBlend          = m_DeviceFeatures.independentBlend         == VK_TRUE;
    m_Features.fillModeNonSolid          = m_DeviceFeatures.fillModeNonSolid         == VK_TRUE;
    m_Features.wideLines                 = m_DeviceFeatures.wideLines                == VK_TRUE;
    m_Features.sparseBinding             = m_DeviceFeatures.sparseBinding            == VK_TRUE;
    m_Features.multiViewport             = m_DeviceFeatures.multiViewport            == VK_TRUE;
    m_Features.pipelineStatisticsQuery   = m_DeviceFeatures.pipelineStatisticsQuery  == VK_TRUE;
    m_Features.shaderFloat16             = m_DeviceFeatures.shaderFloat64             == VK_TRUE; // Note: VK1.0 uses shaderFloat64

    // Vulkan 1.2 features (descriptor indexing, timeline semaphores, etc.)
    uint32_t major = VK_VERSION_MAJOR(m_Properties.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(m_Properties.apiVersion);
    if (major > 1 || (major == 1 && minor >= 2))
    {
        // Chain the Vulkan 1.2 features struct
        memset(&m_Vulkan12Features, 0, sizeof(m_Vulkan12Features));
        m_Vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &m_Vulkan12Features;
        vkGetPhysicalDeviceFeatures2(dev, &features2);

        m_Features.descriptorIndexing  = m_Vulkan12Features.descriptorIndexing           == VK_TRUE
                                      && m_Vulkan12Features.shaderSampledImageArrayNonUniformIndexing == VK_TRUE
                                      && m_Vulkan12Features.runtimeDescriptorArray        == VK_TRUE;

        m_Features.timelineSemaphores  = m_Vulkan12Features.timelineSemaphore            == VK_TRUE;
        m_Features.bufferDeviceAddress = m_Vulkan12Features.bufferDeviceAddress          == VK_TRUE;
        m_Features.shaderFloat16       = m_Vulkan12Features.shaderFloat16                == VK_TRUE;
    }

    // Dynamic rendering (Vulkan 1.3 core or VK_KHR_dynamic_rendering extension)
    m_Features.dynamicRendering = HasExtension("VK_KHR_dynamic_rendering");
    if (major > 1 || (major == 1 && minor >= 3))
        m_Features.dynamicRendering = true;  // Core in 1.3

    // Mesh shaders
    m_Features.meshShaders = HasExtension("VK_EXT_mesh_shader") ||
                              HasExtension("VK_NV_mesh_shader");

    // Ray tracing
    m_Features.rayTracing = HasExtension("VK_KHR_ray_tracing_pipeline") &&
                            HasExtension("VK_KHR_acceleration_structure");

    // Shader atomic float (GI voxelization)
    m_Features.shaderAtomicFloat = HasExtension("VK_EXT_shader_atomic_float");

    // Calibrated timestamps (GPU profiler)
    m_Features.calibratedTimestamps = HasExtension("VK_EXT_calibrated_timestamps") ||
                                      HasExtension("VK_KHR_calibrated_timestamps");
}

// ─────────────────────────────────────────────────────────────────────────────
// QueryMemory
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::QueryMemory(VkPhysicalDevice dev)
{
    vkGetPhysicalDeviceMemoryProperties(dev, &m_MemProperties);

    for (uint32_t i = 0; i < m_MemProperties.memoryHeapCount; ++i)
    {
        const auto& heap = m_MemProperties.memoryHeaps[i];
        m_MemInfo.totalBytes += heap.size;
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            m_MemInfo.deviceLocalBytes += heap.size;
            // BAR memory: device-local AND host-visible (AMD RADV / NV resizable BAR)
            for (uint32_t j = 0; j < m_MemProperties.memoryTypeCount; ++j)
            {
                if (m_MemProperties.memoryTypes[j].heapIndex == i)
                {
                    constexpr VkMemoryPropertyFlags kBarFlags =
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                    if ((m_MemProperties.memoryTypes[j].propertyFlags & kBarFlags) == kBarFlags)
                        m_MemInfo.hasBarMemory = true;
                }
            }
        }
        else
        {
            m_MemInfo.hostVisibleBytes += heap.size;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// QuerySwapchainSupport
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::QuerySwapchainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    if (surface == VK_NULL_HANDLE) return;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &m_SwapchainSupport.capabilities);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount, nullptr);
    if (fmtCount > 0)
    {
        m_SwapchainSupport.formats.resize(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &fmtCount,
                                              m_SwapchainSupport.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, nullptr);
    if (modeCount > 0)
    {
        m_SwapchainSupport.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount,
                                                   m_SwapchainSupport.presentModes.data());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// QueryQueueFamilies
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::QueryQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i)
    {
        const auto& q = families[i];

        if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            m_QueueFamilies.graphicsFamily = i;

        if (q.queueFlags & VK_QUEUE_COMPUTE_BIT)
            m_QueueFamilies.computeFamily = i;

        // Dedicated transfer (no graphics/compute flags)
        if ((q.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(q.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(q.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            m_QueueFamilies.transferFamily = i;
        }

        if (surface != VK_NULL_HANDLE)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (presentSupport)
                m_QueueFamilies.presentFamily = i;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DetermineAdapterTier
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::DetermineAdapterTier()
{
    // Use device-local VRAM as the primary heuristic
    uint64_t vramMB = m_MemInfo.deviceLocalBytes / (1024ull * 1024ull);

    if (m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU || vramMB < 1024)
        m_Tier = AdapterTier::Integrated;
    else if (vramMB < 4096)
        m_Tier = AdapterTier::LowEnd;
    else if (vramMB < 8192)
        m_Tier = AdapterTier::MidRange;
    else
        m_Tier = AdapterTier::HighEnd;
}

// ─────────────────────────────────────────────────────────────────────────────
// ValidateRequired
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDeviceCapabilities::ValidateRequired() const
{
    std::string errors;

    auto check = [&](bool ok, const char* msg) {
        if (!ok) { errors += "  - "; errors += msg; errors += '\n'; }
    };

    check(m_Features.multiDrawIndirect,         "multiDrawIndirect not supported");
    check(m_Features.drawIndirectFirstInstance, "drawIndirectFirstInstance not supported");
    check(m_Features.fragmentStoresAndAtomics,  "fragmentStoresAndAtomics not supported");
    check(m_Features.samplerAnisotropy,         "samplerAnisotropy not supported");
    check(m_Features.depthClamp,                "depthClamp not supported");
    check(m_Features.independentBlend,          "independentBlend not supported (G-buffer needs 4 MRTs)");
    check(HasExtension("VK_KHR_swapchain"),     "VK_KHR_swapchain extension not available");
    check(m_QueueFamilies.IsComplete(),         "Required queue families (graphics/present/compute) not available");
    check(m_SwapchainSupport.IsAdequate(),      "Swapchain support inadequate (no formats or present modes)");

    // API version
    uint32_t major = VK_VERSION_MAJOR(m_Properties.apiVersion);
    uint32_t minor = VK_VERSION_MINOR(m_Properties.apiVersion);
    check(major > 1 || (major == 1 && minor >= 2), "Vulkan 1.2+ required");

    // Limits
    const auto& lim = m_Properties.limits;
    check(lim.maxBoundDescriptorSets     >= 4,        "maxBoundDescriptorSets < 4");
    check(lim.maxColorAttachments        >= 4,        "maxColorAttachments < 4 (G-buffer needs 4 MRTs)");
    check(lim.maxUniformBufferRange      >= 65536,    "maxUniformBufferRange < 64 KB");
    check(lim.maxStorageBufferRange      >= 134217728,"maxStorageBufferRange < 128 MB");
    check(lim.maxPushConstantsSize       >= 64,       "maxPushConstantsSize < 64 bytes");
    check(lim.maxImageDimension2D        >= 4096,     "maxImageDimension2D < 4096");
    check(lim.maxImageArrayLayers        >= 64,       "maxImageArrayLayers < 64 (shadow map array)");

    if (!errors.empty())
        throw std::runtime_error("VulkanDeviceCapabilities: Required features missing:\n" + errors);
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildAdaptiveConfig
// ─────────────────────────────────────────────────────────────────────────────

AdaptiveConfig VulkanDeviceCapabilities::BuildAdaptiveConfig() const
{
    AdaptiveConfig cfg;
    cfg.tier = m_Tier;
    cfg.useMailboxPresent = m_SwapchainSupport.SupportsMailbox();
    cfg.useBindlessTextures = m_Features.descriptorIndexing;
    cfg.maxAnisotropy = std::min(m_DeviceFeatures.samplerAnisotropy == VK_TRUE
                                     ? m_Properties.limits.maxSamplerAnisotropy
                                     : 1.0f,
                                 16.0f);

    uint64_t vramMB = m_MemInfo.deviceLocalBytes / (1024ull * 1024ull);

    // ── Shadow maps ───────────────────────────────────────────────────────────
    auto tierIdx = static_cast<size_t>(m_Tier);
    cfg.shadowMapResolution = SHADOW_RES_TABLE[tierIdx];
    cfg.maxShadowLayers     = std::min(
        SHADOW_LAYER_TABLE[tierIdx],
        m_Properties.limits.maxImageArrayLayers
    );

    // ── UBOs ─────────────────────────────────────────────────────────────────
    // Camera UBO: one per frame in flight
    cfg.cameraUBOSize = AlignUp(sizeof(CameraUBO) * FRAMES_IN_FLIGHT,
                                 m_Properties.limits.minUniformBufferOffsetAlignment);
    cfg.cameraUBOSize = ClampToDeviceLimit(cfg.cameraUBOSize,
                                            m_Properties.limits.maxUniformBufferRange, "cameraUBO");

    // Lights UBO: scale with tier
    static constexpr VkDeviceSize LIGHTS_SIZES[] = { KB(16), KB(32), KB(64), KB(128) };
    cfg.lightsUBOSize = ClampToDeviceLimit(LIGHTS_SIZES[tierIdx],
                                            m_Properties.limits.maxUniformBufferRange, "lightsUBO");

    // Probes UBO
    static constexpr VkDeviceSize PROBES_SIZES[] = { KB(8), KB(16), KB(32), KB(64) };
    cfg.probesUBOSize = ClampToDeviceLimit(PROBES_SIZES[tierIdx],
                                            m_Properties.limits.maxUniformBufferRange, "probesUBO");

    // ── SSBOs ─────────────────────────────────────────────────────────────────
    static constexpr VkDeviceSize MATERIAL_SIZES[] = { MB(4), MB(8),  MB(16), MB(32)  };
    static constexpr VkDeviceSize SKELETAL_SIZES[] = { MB(8), MB(16), MB(32), MB(64)  };
    static constexpr VkDeviceSize DRAW_CMD_SIZES[] = { MB(4), MB(8),  MB(16), MB(32)  };
    static constexpr VkDeviceSize DRAW_INF_SIZES[] = { MB(8), MB(16), MB(32), MB(64)  };
    static constexpr VkDeviceSize SHADOW_V_SIZES[] = { KB(64), KB(128), KB(256), KB(512) };
    static constexpr VkDeviceSize TEXTURE_H_SIZES[] = { KB(64), KB(128), KB(256), KB(512) };
    static constexpr VkDeviceSize VERTEX_S_SIZES[] = { MB(64), MB(128), MB(256), MB(512) };
    static constexpr VkDeviceSize INDEX_S_SIZES[]  = { MB(32), MB(64), MB(128), MB(256) };

    auto clamp = [&](VkDeviceSize requested, const char* label) {
        return ClampToDeviceLimit(requested,
                                   m_Properties.limits.maxStorageBufferRange, label);
    };

    cfg.materialSSBOSize   = clamp(MATERIAL_SIZES[tierIdx], "materialSSBO");
    cfg.skeletalSSBOSize   = clamp(SKELETAL_SIZES[tierIdx], "skeletalSSBO");
    cfg.drawCommandsSize   = clamp(DRAW_CMD_SIZES[tierIdx], "drawCommandsSSBO");
    cfg.drawInfoSize       = clamp(DRAW_INF_SIZES[tierIdx], "drawInfoSSBO");
    cfg.shadowViewsSize    = ClampToDeviceLimit(SHADOW_V_SIZES[tierIdx],
                                                 m_Properties.limits.maxStorageBufferRange, "shadowViewsSSBO");
    cfg.textureHandlesSize = ClampToDeviceLimit(TEXTURE_H_SIZES[tierIdx],
                                                 m_Properties.limits.maxStorageBufferRange, "textureHandlesSSBO");
    cfg.vertexSSBOSize     = clamp(VERTEX_S_SIZES[tierIdx], "vertexSSBO");
    cfg.indexSSBOSize      = clamp(INDEX_S_SIZES[tierIdx],  "indexSSBO");

    // ── Staging buffer ────────────────────────────────────────────────────────
    static constexpr VkDeviceSize STAGING_SIZES[] = { MB(64), MB(128), MB(256), MB(512) };
    cfg.stagingBufferSize = STAGING_SIZES[tierIdx];
    // Limit staging to 25% of host-visible memory
    if (m_MemInfo.hostVisibleBytes > 0)
    {
        VkDeviceSize maxStaging = m_MemInfo.hostVisibleBytes / 4;
        cfg.stagingBufferSize = std::min(cfg.stagingBufferSize, maxStaging);
    }

    // ── Descriptor pools ──────────────────────────────────────────────────────
    static constexpr uint32_t POOL_SIZES[] = { 256, 512, 1024, 2048 };
    cfg.descriptorPoolMaxSets = POOL_SIZES[tierIdx];

    // Bindless texture array size
    static constexpr uint32_t BINDLESS_SIZES[] = { 64, 128, 512, 1024 };
    cfg.maxBindlessTextures = std::min(
        BINDLESS_SIZES[tierIdx],
        m_Properties.limits.maxPerStageDescriptorSampledImages
    );

    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindMemoryType
// ─────────────────────────────────────────────────────────────────────────────

uint32_t VulkanDeviceCapabilities::FindMemoryType(
    uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    for (uint32_t i = 0; i < m_MemProperties.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) &&
            (m_MemProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

// ─────────────────────────────────────────────────────────────────────────────
// FindDepthFormat
// ─────────────────────────────────────────────────────────────────────────────

VkFormat VulkanDeviceCapabilities::FindDepthFormat() const
{
    // Prefer pure D32 (no stencil waste), fall back to D24_S8, then D16
    static constexpr VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM,
    };
    for (auto fmt : candidates)
    {
        if (SupportsFormat(fmt, VK_IMAGE_TILING_OPTIMAL,
                           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
            return fmt;
    }
    throw std::runtime_error("VulkanDeviceCapabilities: No supported depth format found");
}

// ─────────────────────────────────────────────────────────────────────────────
// SupportsFormat
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanDeviceCapabilities::SupportsFormat(
    VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_PhysDevice, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR)
        return (props.linearTilingFeatures & features) == features;

    return (props.optimalTilingFeatures & features) == features;
}

// ─────────────────────────────────────────────────────────────────────────────
// HasExtension
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanDeviceCapabilities::HasExtension(std::string_view name) const
{
    for (const auto& e : m_Extensions)
        if (e == name) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// ClampToDeviceLimit
// ─────────────────────────────────────────────────────────────────────────────

VkDeviceSize VulkanDeviceCapabilities::ClampToDeviceLimit(
    VkDeviceSize requested, VkDeviceSize deviceMax, const char* label) const
{
    if (requested > deviceMax)
    {
        // EE_CORE_WARN would be used here in a full engine context
        return deviceMax;
    }
    return requested;
}

// ─────────────────────────────────────────────────────────────────────────────
// GetDriverVersion / GetAPIVersion
// ─────────────────────────────────────────────────────────────────────────────

std::string VulkanDeviceCapabilities::GetDriverVersion() const
{
    uint32_t v = m_Properties.driverVersion;
    // NVIDIA encodes differently
    if (m_Properties.vendorID == 0x10DE)
        return std::to_string((v >> 22) & 0x3FF) + "." +
               std::to_string((v >> 14) & 0xFF)  + "." +
               std::to_string((v >> 6)  & 0xFF);
    return std::to_string(VK_VERSION_MAJOR(v)) + "." +
           std::to_string(VK_VERSION_MINOR(v)) + "." +
           std::to_string(VK_VERSION_PATCH(v));
}

std::string VulkanDeviceCapabilities::GetAPIVersion() const
{
    uint32_t v = m_Properties.apiVersion;
    return std::to_string(VK_VERSION_MAJOR(v)) + "." +
           std::to_string(VK_VERSION_MINOR(v)) + "." +
           std::to_string(VK_VERSION_PATCH(v));
}

// ─────────────────────────────────────────────────────────────────────────────
// Dump
// ─────────────────────────────────────────────────────────────────────────────

std::string VulkanDeviceCapabilities::Dump() const
{
    const auto& lim = m_Properties.limits;
    uint64_t vramMB = m_MemInfo.deviceLocalBytes / (1024ull * 1024ull);

    static constexpr const char* tierNames[] = {
        "Integrated", "LowEnd", "MidRange", "HighEnd"
    };

    std::ostringstream ss;
    ss << "=== Vulkan Device Capabilities ===\n";
    ss << "  Device   : " << m_Properties.deviceName        << "\n";
    ss << "  Tier     : " << tierNames[static_cast<int>(m_Tier)] << "\n";
    ss << "  Vulkan   : " << GetAPIVersion()                << "\n";
    ss << "  Driver   : " << GetDriverVersion()             << "\n";
    ss << "  VRAM     : " << vramMB                         << " MB\n";
    ss << "  BAR mem  : " << (m_MemInfo.hasBarMemory ? "yes" : "no") << "\n";
    ss << "\n";
    ss << "  Limits:\n";
    ss << "    maxBoundDescriptorSets        : " << lim.maxBoundDescriptorSets             << "\n";
    ss << "    maxColorAttachments           : " << lim.maxColorAttachments                << "\n";
    ss << "    maxUniformBufferRange         : " << lim.maxUniformBufferRange / 1024       << " KB\n";
    ss << "    maxStorageBufferRange         : " << lim.maxStorageBufferRange / (1024*1024)<< " MB\n";
    ss << "    maxPushConstantsSize          : " << lim.maxPushConstantsSize               << " bytes\n";
    ss << "    maxImageDimension2D           : " << lim.maxImageDimension2D                << "\n";
    ss << "    maxImageArrayLayers           : " << lim.maxImageArrayLayers                << "\n";
    ss << "    maxDrawIndirectCount          : " << lim.maxDrawIndirectCount               << "\n";
    ss << "    maxPerStageDescSampledImages  : " << lim.maxPerStageDescriptorSampledImages << "\n";
    ss << "\n";
    ss << "  Features:\n";
    auto feat = [&](const char* n, bool v) {
        ss << "    " << std::setw(36) << std::left << n
           << ": " << (v ? "YES" : "no") << "\n";
    };
    feat("multiDrawIndirect",         m_Features.multiDrawIndirect);
    feat("descriptorIndexing",        m_Features.descriptorIndexing);
    feat("dynamicRendering",          m_Features.dynamicRendering);
    feat("bufferDeviceAddress",       m_Features.bufferDeviceAddress);
    feat("timelineSemaphores",        m_Features.timelineSemaphores);
    feat("meshShaders",               m_Features.meshShaders);
    feat("rayTracing",                m_Features.rayTracing);
    feat("shaderAtomicFloat",         m_Features.shaderAtomicFloat);
    feat("geometryShader",            m_Features.geometryShader);
    feat("samplerAnisotropy",         m_Features.samplerAnisotropy);
    feat("pipelineStatisticsQuery",   m_Features.pipelineStatisticsQuery);
    feat("mailbox present mode",      m_SwapchainSupport.SupportsMailbox());
    ss << "===================================\n";

    return ss.str();
}

} // namespace Ermine::Vulkan
