/* Start Header ************************************************************************/
/*!
\file       VulkanBuffer.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Unified Vulkan buffer abstraction covering vertex, index, uniform,
            storage, staging, and indirect draw buffers.

            All allocations check the adaptive config produced by
            VulkanDeviceCapabilities::BuildAdaptiveConfig() to stay within
            the device's reported limits.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include <span>
#include <cstring>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanBuffer
// ─────────────────────────────────────────────────────────────────────────────

class VulkanBuffer
{
public:
    VulkanBuffer() = default;
    ~VulkanBuffer() { Destroy(); }

    // Non-copyable, movable
    VulkanBuffer(const VulkanBuffer&)            = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& o) noexcept
        : m_Device(o.m_Device), m_Buffer(o.m_Buffer), m_Memory(o.m_Memory),
          m_Size(o.m_Size), m_Usage(o.m_Usage), m_Mapped(o.m_Mapped)
    {
        o.m_Device = VK_NULL_HANDLE;
        o.m_Buffer = VK_NULL_HANDLE;
        o.m_Memory = VK_NULL_HANDLE;
        o.m_Mapped = nullptr;
    }

    // ── Factory helpers ───────────────────────────────────────────────────────

    /**
     * @brief Create a device-local GPU buffer and upload initial data via staging.
     *        Used for vertex buffers, index buffers, and static SSBOs.
     *
     * @param ctx     Active VulkanContext.
     * @param usage   VkBufferUsageFlags (VERTEX | TRANSFER_DST, etc.)
     * @param data    Pointer to data to upload. May be nullptr for empty allocation.
     * @param size    Size in bytes.
     */
    static VulkanBuffer CreateGPU(VulkanContext& ctx, VkBufferUsageFlags usage,
                                   const void* data, VkDeviceSize size);

    /**
     * @brief Create a persistently mapped host-visible buffer (for UBOs/small SSBOs
     *        that need per-frame CPU writes).
     */
    static VulkanBuffer CreateHostVisible(VulkanContext& ctx, VkBufferUsageFlags usage,
                                           VkDeviceSize size);

    /**
     * @brief Create a staging buffer for CPU→GPU uploads.
     *        HOST_VISIBLE | HOST_COHERENT, TRANSFER_SRC.
     */
    static VulkanBuffer CreateStaging(VulkanContext& ctx, VkDeviceSize size);

    // ── Core creation ─────────────────────────────────────────────────────────

    /**
     * @brief Allocate the buffer with the given usage and memory properties.
     *        Low-level; prefer the factory helpers above.
     */
    void Create(VkDevice device,
                const VkPhysicalDeviceMemoryProperties& memProps,
                VkBufferUsageFlags usage,
                VkMemoryPropertyFlags memoryProps,
                VkDeviceSize size);

    void Destroy();

    // ── Upload ────────────────────────────────────────────────────────────────

    /**
     * @brief Write to the persistently mapped buffer (host-visible only).
     *        No flushing needed when HOST_COHERENT.
     */
    void Write(const void* src, VkDeviceSize offset, VkDeviceSize size)
    {
        assert(m_Mapped && "Buffer is not host-visible / not mapped");
        memcpy(static_cast<uint8_t*>(m_Mapped) + offset, src, size);
    }

    template<typename T>
    void Write(const T& value, VkDeviceSize offset = 0)
    {
        Write(&value, offset, sizeof(T));
    }

    /**
     * @brief Copy data from a staging buffer into this (device-local) buffer
     *        via a one-shot command buffer on the transfer queue.
     */
    void CopyFrom(VulkanContext& ctx, const VulkanBuffer& staging,
                  VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0,
                  VkDeviceSize size = VK_WHOLE_SIZE);

    // ── Accessors ─────────────────────────────────────────────────────────────

    VkBuffer     GetBuffer() const { return m_Buffer; }
    VkDeviceSize GetSize()   const { return m_Size;   }
    bool         IsValid()   const { return m_Buffer != VK_NULL_HANDLE; }
    bool         IsMapped()  const { return m_Mapped != nullptr; }
    void*        GetMapped() const { return m_Mapped; }

    VkDescriptorBufferInfo GetDescriptorInfo(VkDeviceSize offset = 0,
                                              VkDeviceSize range  = VK_WHOLE_SIZE) const
    {
        return { m_Buffer, offset, range };
    }

private:
    VkDevice         m_Device = VK_NULL_HANDLE;
    VkBuffer         m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory   m_Memory = VK_NULL_HANDLE;
    VkDeviceSize     m_Size   = 0;
    VkBufferUsageFlags m_Usage = 0;
    void*            m_Mapped = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanRingBuffer  (streaming SSBO / per-frame data with triple buffering)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Triple-partition ring buffer for per-frame streaming data.
 *
 *        The total size is (partitionSize * FRAMES_IN_FLIGHT).
 *        Each frame writes to its own partition, avoiding GPU-CPU sync stalls.
 *
 *        Typical use: per-frame draw commands, draw info SSBOs, camera UBOs.
 */
class VulkanRingBuffer
{
public:
    /**
     * @brief Create the ring buffer.
     * @param ctx             Active VulkanContext.
     * @param partitionSize   Per-frame data budget (validated against device limits).
     * @param usage           VkBufferUsageFlags (STORAGE_BUFFER, UNIFORM_BUFFER, etc.)
     */
    void Create(VulkanContext& ctx, VkDeviceSize partitionSize, VkBufferUsageFlags usage);
    void Destroy();

    /**
     * @brief Get the write pointer for the current frame's partition.
     */
    void* GetFramePtr(uint32_t frameIndex) const
    {
        assert(m_Mapped);
        return static_cast<uint8_t*>(m_Mapped) + frameIndex * m_PartitionSize;
    }

    /**
     * @brief Get a descriptor info pointing to the current frame's partition.
     */
    VkDescriptorBufferInfo GetFrameDescriptor(uint32_t frameIndex) const
    {
        return { m_Buffer.GetBuffer(),
                 static_cast<VkDeviceSize>(frameIndex) * m_PartitionSize,
                 m_PartitionSize };
    }

    VkBuffer     GetBuffer()        const { return m_Buffer.GetBuffer();       }
    VkDeviceSize GetPartitionSize() const { return m_PartitionSize;            }
    VkDeviceSize GetTotalSize()     const { return m_PartitionSize * FRAMES_IN_FLIGHT; }

private:
    VulkanBuffer m_Buffer;
    VkDeviceSize m_PartitionSize = 0;
    void*        m_Mapped        = nullptr;
};

} // namespace Ermine::Vulkan
