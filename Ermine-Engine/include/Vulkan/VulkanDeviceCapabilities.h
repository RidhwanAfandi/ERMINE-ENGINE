/* Start Header ************************************************************************/
/*!
\file       VulkanDeviceCapabilities.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      GPU capability query and adaptive buffer sizing for the Vulkan renderer.

            This module queries the physical device's actual hardware limits and
            computes conservative-but-optimal buffer sizes.  Every allocation in
            the Vulkan backend goes through AdaptiveConfig so the renderer degrades
            gracefully on integrated GPUs and scales up on high-end hardware.

            Usage:
              VulkanDeviceCapabilities caps;
              caps.Query(physicalDevice);
              auto cfg = caps.BuildAdaptiveConfig();
              // Pass cfg to VulkanBuffer, VulkanDescriptorManager, etc.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Ermine::Vulkan
{

// ─── Feature support flags ─────────────────────────────────────────────────────
struct SupportedFeatures
{
    // Required (absence = renderer cannot initialize)
    bool multiDrawIndirect          = false;
    bool drawIndirectFirstInstance  = false;
    bool fragmentStoresAndAtomics   = false;
    bool samplerAnisotropy          = false;
    bool depthClamp                 = false;
    bool geometryShader             = false;
    bool independentBlend           = false;   // G-buffer needs per-MRT blend

    // Preferred (absence = reduced quality / fallback path)
    bool descriptorIndexing         = false;   // Bindless textures
    bool dynamicRendering           = false;   // VK_KHR_dynamic_rendering
    bool bufferDeviceAddress        = false;   // GPU-driven indirect + pointers
    bool shaderAtomicFloat          = false;   // GI voxelization
    bool meshShaders                = false;   // Future: mesh shader path
    bool rayTracing                 = false;   // Future: RT shadows
    bool timelineSemaphores         = false;   // Fine-grained CPU-GPU sync
    bool calibratedTimestamps       = false;   // Accurate GPU profiler
    bool fillModeNonSolid           = false;   // Debug wireframe
    bool wideLines                  = false;   // Debug line rendering
    bool pipelineStatisticsQuery    = false;   // GPU profiler counters
    bool shaderFloat16              = false;   // Half-precision math
    bool sparseBinding              = false;   // Virtual texturing
    bool multiViewport              = false;   // Editor split view
};

// ─── Adapter tier (determines preset quality level) ────────────────────────────
enum class AdapterTier : uint8_t
{
    Integrated  = 0,    // < 2 GB VRAM, shared memory, battery-constrained
    LowEnd      = 1,    // 2-4 GB dedicated VRAM
    MidRange    = 2,    // 4-8 GB dedicated VRAM
    HighEnd     = 3,    // > 8 GB dedicated VRAM
};

// ─── Adaptive buffer configuration ────────────────────────────────────────────
// All sizes are pre-validated against device limits.
struct AdaptiveConfig
{
    AdapterTier tier = AdapterTier::MidRange;

    // ── UBOs ──────────────────────────────────────────────────────────────────
    VkDeviceSize cameraUBOSize      = 0;   // sizeof(CameraUBO) * FRAMES_IN_FLIGHT
    VkDeviceSize lightsUBOSize      = 0;   // Lights array
    VkDeviceSize probesUBOSize      = 0;   // Light probe array

    // ── SSBOs ─────────────────────────────────────────────────────────────────
    VkDeviceSize materialSSBOSize   = 0;   // All compiled materials
    VkDeviceSize skeletalSSBOSize   = 0;   // Per-entity bone matrices
    VkDeviceSize drawCommandsSize   = 0;   // VkDrawIndexedIndirectCommand[]
    VkDeviceSize drawInfoSize       = 0;   // DrawInfo[] (model matrix, mat index)
    VkDeviceSize shadowViewsSize    = 0;   // ShadowViewGPU[]
    VkDeviceSize textureHandlesSize = 0;   // Bindless texture handle array
    VkDeviceSize vertexSSBOSize     = 0;   // All mesh vertices (GPU-driven)
    VkDeviceSize indexSSBOSize      = 0;   // All mesh indices (GPU-driven)

    // ── Staging ───────────────────────────────────────────────────────────────
    VkDeviceSize stagingBufferSize  = 0;   // CPU→GPU upload ring buffer

    // ── Descriptor pools ─────────────────────────────────────────────────────
    uint32_t descriptorPoolMaxSets  = 0;
    uint32_t maxBindlessTextures    = 0;   // Size of texture array descriptor

    // ── Shadow maps ───────────────────────────────────────────────────────────
    uint32_t maxShadowLayers        = 0;   // Array layers in shadow texture
    uint32_t shadowMapResolution    = 0;   // Per-cascade resolution

    // ── Misc ──────────────────────────────────────────────────────────────────
    float    maxAnisotropy          = 1.0f;
    bool     useMailboxPresent      = false;  // Triple buffering present mode
    bool     useBindlessTextures    = false;  // Descriptor indexing path
};

// ─── Memory heap info ──────────────────────────────────────────────────────────
struct MemoryInfo
{
    uint64_t deviceLocalBytes   = 0;   // GPU-only VRAM
    uint64_t hostVisibleBytes   = 0;   // CPU-visible (may be shared VRAM on iGPU)
    uint64_t totalBytes         = 0;
    bool     hasBarMemory       = false; // AMD RADV BAR (CPU + GPU fast path)
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanDeviceCapabilities
// ─────────────────────────────────────────────────────────────────────────────
class VulkanDeviceCapabilities
{
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    VulkanDeviceCapabilities() = default;

    /**
     * @brief Query all capabilities from the physical device.
     *        Must be called once after physical device selection.
     * @param physDevice  The selected VkPhysicalDevice.
     * @param surface     The presentation surface (for swapchain query).
     */
    void Query(VkPhysicalDevice physDevice, VkSurfaceKHR surface);

    /**
     * @brief Validate that all REQUIRED features are present.
     *        Throws std::runtime_error with a descriptive message if any
     *        required feature is missing.
     */
    void ValidateRequired() const;

    /**
     * @brief Build an AdaptiveConfig derived from actual device limits.
     *        All sizes are clamped to the device's reported maxima.
     *        Shadow map resolution and layer counts scale with VRAM tier.
     */
    AdaptiveConfig BuildAdaptiveConfig() const;

    // ── Accessors ─────────────────────────────────────────────────────────────

    const VkPhysicalDeviceProperties&       GetProperties()       const { return m_Properties;      }
    const VkPhysicalDeviceLimits&           GetLimits()           const { return m_Properties.limits; }
    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemProperties;   }
    const SupportedFeatures&                GetFeatures()         const { return m_Features;         }
    const MemoryInfo&                       GetMemoryInfo()       const { return m_MemInfo;          }
    const SwapchainSupportDetails&          GetSwapchainSupport() const { return m_SwapchainSupport; }
    const QueueFamilyIndices&               GetQueueFamilies()    const { return m_QueueFamilies;    }
    AdapterTier                             GetTier()             const { return m_Tier;             }

    std::string GetDeviceName()     const { return m_Properties.deviceName; }
    std::string GetDriverVersion()  const;
    std::string GetAPIVersion()     const;
    bool        IsDiscrete()        const { return m_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU; }

    /**
     * @brief Find the best memory type for the given requirements.
     * @param typeFilter    Bitmask of acceptable memory type indices.
     * @param properties    Required memory property flags.
     * @return Memory type index, or UINT32_MAX if not found.
     */
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    /**
     * @brief Find the best depth format supported by the device.
     *        Returns D32_SFLOAT if available, falling back to D24_UNORM_S8_UINT.
     */
    VkFormat FindDepthFormat() const;

    /**
     * @brief Check if a format supports the requested tiling and features.
     */
    bool SupportsFormat(VkFormat format, VkImageTiling tiling,
                        VkFormatFeatureFlags features) const;

    /**
     * @brief Dump capabilities to a human-readable log string.
     */
    std::string Dump() const;

    // ── Extension lists ───────────────────────────────────────────────────────

    const std::vector<std::string>& GetAvailableExtensions() const { return m_Extensions; }

    bool HasExtension(std::string_view name) const;

private:
    // ── Internal helpers ──────────────────────────────────────────────────────

    void QueryExtensions(VkPhysicalDevice dev);
    void QueryFeatures(VkPhysicalDevice dev);
    void QueryMemory(VkPhysicalDevice dev);
    void QuerySwapchainSupport(VkPhysicalDevice dev, VkSurfaceKHR surface);
    void QueryQueueFamilies(VkPhysicalDevice dev, VkSurfaceKHR surface);
    void DetermineAdapterTier();

    /**
     * @brief Clamp a requested size to the device's reported maximum.
     *        Logs a warning if clamping occurred.
     */
    VkDeviceSize ClampToDeviceLimit(VkDeviceSize requested, VkDeviceSize deviceMax,
                                     const char* label) const;

    // ── Device data ───────────────────────────────────────────────────────────

    VkPhysicalDevice                    m_PhysDevice    = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties          m_Properties    = {};
    VkPhysicalDeviceFeatures            m_DeviceFeatures = {};
    VkPhysicalDeviceMemoryProperties    m_MemProperties = {};
    SupportedFeatures                   m_Features      = {};
    MemoryInfo                          m_MemInfo       = {};
    SwapchainSupportDetails             m_SwapchainSupport = {};
    QueueFamilyIndices                  m_QueueFamilies = {};
    AdapterTier                         m_Tier          = AdapterTier::MidRange;

    std::vector<std::string>            m_Extensions;

    // Vulkan 1.1+ extended feature structs (chained via pNext)
    VkPhysicalDeviceDescriptorIndexingFeatures  m_DescIndexingFeatures  = {};
    VkPhysicalDeviceVulkan12Features            m_Vulkan12Features      = {};
};

} // namespace Ermine::Vulkan
