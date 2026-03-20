/* Start Header ************************************************************************/
/*!
\file       VulkanImage.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan image and sampler abstraction for textures, attachments,
            and G-buffer targets.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanImage
// ─────────────────────────────────────────────────────────────────────────────

class VulkanImage
{
public:
    VulkanImage() = default;
    ~VulkanImage() { Destroy(); }

    VulkanImage(const VulkanImage&)            = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    VulkanImage(VulkanImage&& o) noexcept
        : m_Device(o.m_Device), m_Image(o.m_Image), m_Memory(o.m_Memory),
          m_View(o.m_View), m_Format(o.m_Format), m_Extent(o.m_Extent),
          m_MipLevels(o.m_MipLevels), m_ArrayLayers(o.m_ArrayLayers),
          m_CurrentLayout(o.m_CurrentLayout)
    {
        o.m_Device = VK_NULL_HANDLE;
        o.m_Image  = VK_NULL_HANDLE;
        o.m_Memory = VK_NULL_HANDLE;
        o.m_View   = VK_NULL_HANDLE;
    }

    // ── Factory helpers ───────────────────────────────────────────────────────

    /**
     * @brief Create a 2D color attachment (G-buffer MRT target, post-process buffer).
     */
    static VulkanImage CreateColorAttachment(VulkanContext& ctx,
                                              uint32_t width, uint32_t height,
                                              VkFormat format);

    /**
     * @brief Create a 2D depth/stencil attachment.
     */
    static VulkanImage CreateDepthAttachment(VulkanContext& ctx,
                                              uint32_t width, uint32_t height,
                                              VkFormat format = VK_FORMAT_UNDEFINED);

    /**
     * @brief Create a 2D texture from raw RGBA8 data (CPU upload via staging).
     */
    static VulkanImage CreateTexture2D(VulkanContext& ctx,
                                        uint32_t width, uint32_t height,
                                        VkFormat format,
                                        const void* pixelData,
                                        VkDeviceSize dataSize,
                                        uint32_t mipLevels = 1);

    /**
     * @brief Create a 2D texture array (for shadow maps, atlas layers, etc.)
     */
    static VulkanImage CreateTextureArray(VulkanContext& ctx,
                                           uint32_t width, uint32_t height,
                                           uint32_t layers,
                                           VkFormat format,
                                           VkImageUsageFlags extraUsage = 0);

    /**
     * @brief Create a cubemap image (for skybox, light probe capture).
     */
    static VulkanImage CreateCubemap(VulkanContext& ctx,
                                      uint32_t size,
                                      VkFormat format,
                                      uint32_t mipLevels = 1);

    // ── Core creation ─────────────────────────────────────────────────────────

    struct CreateInfo
    {
        uint32_t         width       = 1;
        uint32_t         height      = 1;
        uint32_t         depth       = 1;
        uint32_t         mipLevels   = 1;
        uint32_t         arrayLayers = 1;
        VkFormat         format      = VK_FORMAT_R8G8B8A8_UNORM;
        VkImageTiling    tiling      = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage      = VK_IMAGE_USAGE_SAMPLED_BIT;
        VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkImageCreateFlags createFlags = 0;
        VkImageType        type        = VK_IMAGE_TYPE_2D;
        VkImageViewType    viewType    = VK_IMAGE_VIEW_TYPE_2D;
        VkImageAspectFlags aspect      = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSampleCountFlagBits samples  = VK_SAMPLE_COUNT_1_BIT;
    };

    void Create(VulkanContext& ctx, const CreateInfo& info);
    void Destroy();

    // ── Layout transitions ────────────────────────────────────────────────────

    /**
     * @brief Issue a pipeline barrier to transition this image's layout.
     *        Immediately submitted on the graphics queue.
     */
    void TransitionLayout(VulkanContext& ctx,
                          VkImageLayout newLayout,
                          VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    /**
     * @brief Record a layout transition into an existing command buffer.
     */
    void RecordLayoutTransition(VkCommandBuffer cmd,
                                VkImageLayout oldLayout,
                                VkImageLayout newLayout,
                                VkPipelineStageFlags srcStage,
                                VkPipelineStageFlags dstStage,
                                VkAccessFlags srcAccess,
                                VkAccessFlags dstAccess,
                                VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                                uint32_t baseMip    = 0,
                                uint32_t mipCount   = VK_REMAINING_MIP_LEVELS,
                                uint32_t baseLayer  = 0,
                                uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);

    // ── Accessors ─────────────────────────────────────────────────────────────

    VkImage       GetImage()   const { return m_Image;  }
    VkImageView   GetView()    const { return m_View;   }
    VkFormat      GetFormat()  const { return m_Format; }
    VkExtent2D    GetExtent2D()const { return { m_Extent.width, m_Extent.height }; }
    VkExtent3D    GetExtent()  const { return m_Extent; }
    uint32_t      GetMipLevels()   const { return m_MipLevels;   }
    uint32_t      GetArrayLayers() const { return m_ArrayLayers; }
    VkImageLayout GetCurrentLayout() const { return m_CurrentLayout; }

    bool IsValid() const { return m_Image != VK_NULL_HANDLE; }

    VkDescriptorImageInfo GetDescriptorInfo(VkSampler sampler,
                                             VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const
    {
        return { sampler, m_View, layout };
    }

private:
    VkDevice       m_Device      = VK_NULL_HANDLE;
    VkImage        m_Image       = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory      = VK_NULL_HANDLE;
    VkImageView    m_View        = VK_NULL_HANDLE;
    VkFormat       m_Format      = VK_FORMAT_UNDEFINED;
    VkExtent3D     m_Extent      = {};
    uint32_t       m_MipLevels   = 1;
    uint32_t       m_ArrayLayers = 1;
    VkImageLayout  m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanSampler
// ─────────────────────────────────────────────────────────────────────────────

class VulkanSampler
{
public:
    VulkanSampler() = default;
    ~VulkanSampler() { Destroy(); }

    VulkanSampler(const VulkanSampler&)            = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;

    struct Desc
    {
        VkFilter             minFilter     = VK_FILTER_LINEAR;
        VkFilter             magFilter     = VK_FILTER_LINEAR;
        VkSamplerMipmapMode  mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode addressMode   = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        float                maxAnisotropy = 1.0f;  // Set from AdaptiveConfig
        float                maxLod        = VK_LOD_CLAMP_NONE;
        float                mipLodBias    = 0.0f;
        bool                 unnormalizedCoords = false;
        VkCompareOp          compareOp     = VK_COMPARE_OP_NEVER;
        bool                 enableCompare = false;  // For shadow samplers
        VkBorderColor        borderColor   = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    };

    void Create(VkDevice device, const Desc& desc);
    void Destroy();

    VkSampler Get() const { return m_Sampler; }
    bool IsValid() const { return m_Sampler != VK_NULL_HANDLE; }

    // Pre-built sampler factories
    static VulkanSampler CreateLinearRepeat(VkDevice device, float maxAnisotropy);
    static VulkanSampler CreateLinearClamp (VkDevice device);
    static VulkanSampler CreateNearestClamp(VkDevice device);
    static VulkanSampler CreateShadowSampler(VkDevice device);  // PCF compare sampler

private:
    VkDevice  m_Device  = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
};

// ─────────────────────────────────────────────────────────────────────────────
// GBufferAttachments  (convenience wrapper for the G-buffer MRTs)
// ─────────────────────────────────────────────────────────────────────────────

struct GBufferAttachments
{
    VulkanImage packed0;   // Position  (RGBA32F)
    VulkanImage packed1;   // Normal    (RGBA16F)
    VulkanImage packed2;   // Material  (RGBA8)
    VulkanImage packed3;   // Extra     (RGBA8)
    VulkanImage depth;     // Depth     (D32F or D24S8)

    void Create(VulkanContext& ctx, uint32_t width, uint32_t height);
    void Destroy();
    void Resize(VulkanContext& ctx, uint32_t width, uint32_t height);
};

} // namespace Ermine::Vulkan
