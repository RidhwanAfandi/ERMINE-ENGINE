/* Start Header ************************************************************************/
/*!
\file       VulkanBuffer.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan buffer implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanBuffer.h"
#include "Vulkan/VulkanDeviceCapabilities.h"

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t FindMemoryTypeLocal(const VkPhysicalDeviceMemoryProperties& memProps,
                                     uint32_t typeFilter, VkMemoryPropertyFlags props)
{
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("VulkanBuffer: No suitable memory type found");
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanBuffer::Create
// ─────────────────────────────────────────────────────────────────────────────

void VulkanBuffer::Create(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties& memProps,
                           VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags memoryProps,
                           VkDeviceSize size)
{
    m_Device = device;
    m_Size   = size;
    m_Usage  = usage;

    VkBufferCreateInfo ci{};
    ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size        = size;
    ci.usage       = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &ci, nullptr, &m_Buffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_Buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryTypeLocal(memProps, memReqs.memoryTypeBits, memoryProps);
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_Memory));
    VK_CHECK(vkBindBufferMemory(device, m_Buffer, m_Memory, 0));

    // Persistently map host-visible memory
    if (memoryProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        VK_CHECK(vkMapMemory(device, m_Memory, 0, size, 0, &m_Mapped));
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanBuffer::Destroy
// ─────────────────────────────────────────────────────────────────────────────

void VulkanBuffer::Destroy()
{
    if (m_Device == VK_NULL_HANDLE) return;
    if (m_Mapped)  { vkUnmapMemory  (m_Device, m_Memory);           m_Mapped = nullptr; }
    if (m_Buffer)  { vkDestroyBuffer(m_Device, m_Buffer, nullptr);  m_Buffer = VK_NULL_HANDLE; }
    if (m_Memory)  { vkFreeMemory   (m_Device, m_Memory, nullptr);  m_Memory = VK_NULL_HANDLE; }
    m_Device = VK_NULL_HANDLE;
    m_Size   = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateGPU
// ─────────────────────────────────────────────────────────────────────────────

VulkanBuffer VulkanBuffer::CreateGPU(VulkanContext& ctx, VkBufferUsageFlags usage,
                                      const void* data, VkDeviceSize size)
{
    VulkanBuffer buf;
    buf.Create(ctx.GetDevice(),
               ctx.GetCapabilities().GetMemoryProperties(),
               usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               size);

    if (data)
    {
        // Upload via staging
        VulkanBuffer staging = CreateStaging(ctx, size);
        memcpy(staging.m_Mapped, data, static_cast<size_t>(size));

        buf.CopyFrom(ctx, staging, 0, 0, size);
    }
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateHostVisible
// ─────────────────────────────────────────────────────────────────────────────

VulkanBuffer VulkanBuffer::CreateHostVisible(VulkanContext& ctx, VkBufferUsageFlags usage,
                                              VkDeviceSize size)
{
    VulkanBuffer buf;
    buf.Create(ctx.GetDevice(),
               ctx.GetCapabilities().GetMemoryProperties(),
               usage,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               size);
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory: CreateStaging
// ─────────────────────────────────────────────────────────────────────────────

VulkanBuffer VulkanBuffer::CreateStaging(VulkanContext& ctx, VkDeviceSize size)
{
    VulkanBuffer buf;
    buf.Create(ctx.GetDevice(),
               ctx.GetCapabilities().GetMemoryProperties(),
               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               size);
    return buf;
}

// ─────────────────────────────────────────────────────────────────────────────
// CopyFrom
// ─────────────────────────────────────────────────────────────────────────────

void VulkanBuffer::CopyFrom(VulkanContext& ctx, const VulkanBuffer& staging,
                             VkDeviceSize srcOffset, VkDeviceSize dstOffset,
                             VkDeviceSize size)
{
    VkDeviceSize copySize = (size == VK_WHOLE_SIZE) ? m_Size : size;

    ctx.ImmediateTransferSubmit([&](VkCommandBuffer cmd)
    {
        VkBufferCopy region{};
        region.srcOffset = srcOffset;
        region.dstOffset = dstOffset;
        region.size      = copySize;
        vkCmdCopyBuffer(cmd, staging.GetBuffer(), m_Buffer, 1, &region);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanRingBuffer
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRingBuffer::Create(VulkanContext& ctx, VkDeviceSize partitionSize,
                               VkBufferUsageFlags usage)
{
    // Align partition size to the device's minimum buffer alignment
    const auto& lim = ctx.GetCapabilities().GetLimits();
    VkDeviceSize alignment = lim.minStorageBufferOffsetAlignment;
    if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        alignment = std::max(alignment, lim.minUniformBufferOffsetAlignment);

    m_PartitionSize = AlignUp(partitionSize, alignment);
    VkDeviceSize totalSize = m_PartitionSize * FRAMES_IN_FLIGHT;

    m_Buffer = VulkanBuffer::CreateHostVisible(ctx, usage, totalSize);
    m_Mapped = m_Buffer.GetMapped();
}

void VulkanRingBuffer::Destroy()
{
    m_Buffer.Destroy();
    m_Mapped = nullptr;
    m_PartitionSize = 0;
}

} // namespace Ermine::Vulkan
