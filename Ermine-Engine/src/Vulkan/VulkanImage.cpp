/* Start Header ************************************************************************/
/*!
\file       VulkanImage.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan image and sampler implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanImage.h"

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanImage::Create
// ─────────────────────────────────────────────────────────────────────────────

void VulkanImage::Create(VulkanContext& ctx, const CreateInfo& info)
{
    m_Device      = ctx.GetDevice();
    m_Format      = info.format;
    m_Extent      = { info.width, info.height, info.depth };
    m_MipLevels   = info.mipLevels;
    m_ArrayLayers = info.arrayLayers;

    // ── Create VkImage ───────────────────────────────────────────────────────
    VkImageCreateInfo imgCI{};
    imgCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCI.flags         = info.createFlags;
    imgCI.imageType     = info.type;
    imgCI.format        = info.format;
    imgCI.extent        = m_Extent;
    imgCI.mipLevels     = info.mipLevels;
    imgCI.arrayLayers   = info.arrayLayers;
    imgCI.samples       = info.samples;
    imgCI.tiling        = info.tiling;
    imgCI.usage         = info.usage;
    imgCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(m_Device, &imgCI, nullptr, &m_Image));

    // ── Allocate memory ──────────────────────────────────────────────────────
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_Device, m_Image, &memReqs);

    uint32_t memType = ctx.GetCapabilities().FindMemoryType(
        memReqs.memoryTypeBits, info.memProps);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;
    VK_CHECK(vkAllocateMemory(m_Device, &allocInfo, nullptr, &m_Memory));
    VK_CHECK(vkBindImageMemory(m_Device, m_Image, m_Memory, 0));

    // ── Create image view ────────────────────────────────────────────────────
    VkImageViewCreateInfo viewCI{};
    viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image                           = m_Image;
    viewCI.viewType                        = info.viewType;
    viewCI.format                          = info.format;
    viewCI.subresourceRange.aspectMask     = info.aspect;
    viewCI.subresourceRange.baseMipLevel   = 0;
    viewCI.subresourceRange.levelCount     = info.mipLevels;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount     = info.arrayLayers;
    VK_CHECK(vkCreateImageView(m_Device, &viewCI, nullptr, &m_View));

    m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanImage::Destroy
// ─────────────────────────────────────────────────────────────────────────────

void VulkanImage::Destroy()
{
    if (m_Device == VK_NULL_HANDLE) return;
    if (m_View)   { vkDestroyImageView(m_Device, m_View,   nullptr); m_View   = VK_NULL_HANDLE; }
    if (m_Image)  { vkDestroyImage    (m_Device, m_Image,  nullptr); m_Image  = VK_NULL_HANDLE; }
    if (m_Memory) { vkFreeMemory      (m_Device, m_Memory, nullptr); m_Memory = VK_NULL_HANDLE; }
    m_Device = VK_NULL_HANDLE;
    m_CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanImage::TransitionLayout
// ─────────────────────────────────────────────────────────────────────────────

void VulkanImage::TransitionLayout(VulkanContext& ctx, VkImageLayout newLayout,
                                    VkImageAspectFlags aspect)
{
    VkImageLayout oldLayout = m_CurrentLayout;

    // Choose appropriate access masks and pipeline stages
    VkAccessFlags      srcAccess = 0, dstAccess = 0;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccess = VK_ACCESS_SHADER_READ_BIT;
        srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstStage  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstAccess = VK_ACCESS_SHADER_READ_BIT;
        srcStage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    ctx.ImmediateGraphicsSubmit([&](VkCommandBuffer cmd) {
        RecordLayoutTransition(cmd, oldLayout, newLayout, srcStage, dstStage,
                               srcAccess, dstAccess, aspect);
    });

    m_CurrentLayout = newLayout;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanImage::RecordLayoutTransition
// ─────────────────────────────────────────────────────────────────────────────

void VulkanImage::RecordLayoutTransition(VkCommandBuffer cmd,
                                          VkImageLayout oldLayout, VkImageLayout newLayout,
                                          VkPipelineStageFlags srcStage,
                                          VkPipelineStageFlags dstStage,
                                          VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                          VkImageAspectFlags aspect,
                                          uint32_t baseMip, uint32_t mipCount,
                                          uint32_t baseLayer, uint32_t layerCount)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = m_Image;
    barrier.subresourceRange.aspectMask     = aspect;
    barrier.subresourceRange.baseMipLevel   = baseMip;
    barrier.subresourceRange.levelCount     = mipCount;
    barrier.subresourceRange.baseArrayLayer = baseLayer;
    barrier.subresourceRange.layerCount     = layerCount;
    barrier.srcAccessMask                   = srcAccess;
    barrier.dstAccessMask                   = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                          0, nullptr, 0, nullptr, 1, &barrier);

    m_CurrentLayout = newLayout;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateColorAttachment
// ─────────────────────────────────────────────────────────────────────────────

VulkanImage VulkanImage::CreateColorAttachment(VulkanContext& ctx,
                                                uint32_t width, uint32_t height,
                                                VkFormat format)
{
    CreateInfo info{};
    info.width  = width;
    info.height = height;
    info.format = format;
    info.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT          |
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    VulkanImage img;
    img.Create(ctx, info);
    img.TransitionLayout(ctx, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateDepthAttachment
// ─────────────────────────────────────────────────────────────────────────────

VulkanImage VulkanImage::CreateDepthAttachment(VulkanContext& ctx,
                                                uint32_t width, uint32_t height,
                                                VkFormat format)
{
    if (format == VK_FORMAT_UNDEFINED)
        format = ctx.GetCapabilities().FindDepthFormat();

    CreateInfo info{};
    info.width  = width;
    info.height = height;
    info.format = format;
    info.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_SAMPLED_BIT;
    info.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
        info.aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VulkanImage img;
    img.Create(ctx, info);
    img.TransitionLayout(ctx, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, info.aspect);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateTexture2D
// ─────────────────────────────────────────────────────────────────────────────

VulkanImage VulkanImage::CreateTexture2D(VulkanContext& ctx,
                                          uint32_t width, uint32_t height,
                                          VkFormat format,
                                          const void* pixelData,
                                          VkDeviceSize dataSize,
                                          uint32_t mipLevels)
{
    CreateInfo info{};
    info.width     = width;
    info.height    = height;
    info.format    = format;
    info.mipLevels = mipLevels;
    info.usage     = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT;

    VulkanImage img;
    img.Create(ctx, info);
    img.TransitionLayout(ctx, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Upload via staging
    if (pixelData && dataSize > 0)
    {
        auto staging = VulkanBuffer::CreateStaging(ctx, dataSize);
        memcpy(staging.GetMapped(), pixelData, static_cast<size_t>(dataSize));

        ctx.ImmediateTransferSubmit([&](VkCommandBuffer cmd) {
            VkBufferImageCopy region{};
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;
            region.imageExtent                     = { width, height, 1 };
            vkCmdCopyBufferToImage(cmd, staging.GetBuffer(), img.m_Image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        });
    }

    img.TransitionLayout(ctx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateTextureArray
// ─────────────────────────────────────────────────────────────────────────────

VulkanImage VulkanImage::CreateTextureArray(VulkanContext& ctx,
                                             uint32_t width, uint32_t height,
                                             uint32_t layers, VkFormat format,
                                             VkImageUsageFlags extraUsage)
{
    CreateInfo info{};
    info.width       = width;
    info.height      = height;
    info.arrayLayers = layers;
    info.format      = format;
    info.viewType    = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    info.usage       = VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | extraUsage;

    // Detect if this is a depth format
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D16_UNORM)
    {
        info.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VulkanImage img;
    img.Create(ctx, info);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateCubemap
// ─────────────────────────────────────────────────────────────────────────────

VulkanImage VulkanImage::CreateCubemap(VulkanContext& ctx, uint32_t size,
                                        VkFormat format, uint32_t mipLevels)
{
    CreateInfo info{};
    info.width        = size;
    info.height       = size;
    info.arrayLayers  = 6;
    info.mipLevels    = mipLevels;
    info.format       = format;
    info.viewType     = VK_IMAGE_VIEW_TYPE_CUBE;
    info.createFlags  = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.usage        = VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VulkanImage img;
    img.Create(ctx, info);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanSampler
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSampler::Create(VkDevice device, const Desc& desc)
{
    m_Device = device;

    VkSamplerCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter               = desc.magFilter;
    ci.minFilter               = desc.minFilter;
    ci.mipmapMode              = desc.mipmapMode;
    ci.addressModeU            = desc.addressMode;
    ci.addressModeV            = desc.addressMode;
    ci.addressModeW            = desc.addressMode;
    ci.mipLodBias              = desc.mipLodBias;
    ci.anisotropyEnable        = (desc.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
    ci.maxAnisotropy           = desc.maxAnisotropy;
    ci.compareEnable           = desc.enableCompare ? VK_TRUE : VK_FALSE;
    ci.compareOp               = desc.compareOp;
    ci.minLod                  = 0.0f;
    ci.maxLod                  = desc.maxLod;
    ci.borderColor             = desc.borderColor;
    ci.unnormalizedCoordinates = desc.unnormalizedCoords ? VK_TRUE : VK_FALSE;
    VK_CHECK(vkCreateSampler(device, &ci, nullptr, &m_Sampler));
}

void VulkanSampler::Destroy()
{
    if (m_Sampler) { vkDestroySampler(m_Device, m_Sampler, nullptr); m_Sampler = VK_NULL_HANDLE; }
    m_Device = VK_NULL_HANDLE;
}

VulkanSampler VulkanSampler::CreateLinearRepeat(VkDevice device, float maxAnisotropy)
{
    VulkanSampler s;
    Desc d;
    d.maxAnisotropy = maxAnisotropy;
    s.Create(device, d);
    return s;
}

VulkanSampler VulkanSampler::CreateLinearClamp(VkDevice device)
{
    VulkanSampler s;
    Desc d;
    d.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.Create(device, d);
    return s;
}

VulkanSampler VulkanSampler::CreateNearestClamp(VkDevice device)
{
    VulkanSampler s;
    Desc d;
    d.minFilter   = VK_FILTER_NEAREST;
    d.magFilter   = VK_FILTER_NEAREST;
    d.mipmapMode  = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    d.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.Create(device, d);
    return s;
}

VulkanSampler VulkanSampler::CreateShadowSampler(VkDevice device)
{
    VulkanSampler s;
    Desc d;
    d.minFilter    = VK_FILTER_LINEAR;
    d.magFilter    = VK_FILTER_LINEAR;
    d.addressMode  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    d.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    d.enableCompare = true;
    d.compareOp    = VK_COMPARE_OP_LESS_OR_EQUAL;
    s.Create(device, d);
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
// GBufferAttachments
// ─────────────────────────────────────────────────────────────────────────────

void GBufferAttachments::Create(VulkanContext& ctx, uint32_t width, uint32_t height)
{
    packed0 = VulkanImage::CreateColorAttachment(ctx, width, height, VK_FORMAT_R32G32B32A32_SFLOAT); // Position
    packed1 = VulkanImage::CreateColorAttachment(ctx, width, height, VK_FORMAT_R16G16B16A16_SFLOAT); // Normal
    packed2 = VulkanImage::CreateColorAttachment(ctx, width, height, VK_FORMAT_R8G8B8A8_UNORM);      // Albedo/Metal/Rough/AO
    packed3 = VulkanImage::CreateColorAttachment(ctx, width, height, VK_FORMAT_R8G8B8A8_UNORM);      // Emissive/Flags
    depth   = VulkanImage::CreateDepthAttachment(ctx, width, height);
}

void GBufferAttachments::Destroy()
{
    packed0.Destroy();
    packed1.Destroy();
    packed2.Destroy();
    packed3.Destroy();
    depth.Destroy();
}

void GBufferAttachments::Resize(VulkanContext& ctx, uint32_t width, uint32_t height)
{
    Destroy();
    Create(ctx, width, height);
}

} // namespace Ermine::Vulkan
