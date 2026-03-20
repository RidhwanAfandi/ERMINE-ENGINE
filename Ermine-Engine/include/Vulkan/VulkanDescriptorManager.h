/* Start Header ************************************************************************/
/*!
\file       VulkanDescriptorManager.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Descriptor set layouts, pools, and set management for the deferred pipeline.

            Descriptor set layout (4 sets, see VulkanTypes.h):
              Set 0 – Per-frame : camera UBO, lights UBO, probes UBO, shadow views SSBO
              Set 1 – Per-pass  : G-buffer textures (post-process read), shadow map array
              Set 2 – Per-draw  : material SSBO, bindless texture array, skeletal SSBO
              Set 3 – Global    : draw commands SSBO, draw info SSBO (indirect rendering)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include <array>
#include <vector>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanDescriptorManager
// ─────────────────────────────────────────────────────────────────────────────

class VulkanDescriptorManager
{
public:
    VulkanDescriptorManager() = default;
    ~VulkanDescriptorManager() { Destroy(); }

    VulkanDescriptorManager(const VulkanDescriptorManager&)            = delete;
    VulkanDescriptorManager& operator=(const VulkanDescriptorManager&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Create all descriptor set layouts and allocate pools.
     * @param ctx           Active VulkanContext.
     * @param bindlessCount Number of slots in the bindless texture array (from AdaptiveConfig).
     */
    void Init(VulkanContext& ctx, uint32_t bindlessCount);
    void Destroy();

    // ── Set 0: Per-frame ──────────────────────────────────────────────────────

    /**
     * @brief Allocate and write descriptor set 0 for a given frame.
     *        Called once per frame during Init (triple-buffered).
     */
    void WritePerFrameSet(uint32_t frameIndex,
                           VkBuffer cameraUBO,   VkDeviceSize cameraRange,
                           VkBuffer lightsUBO,   VkDeviceSize lightsRange,
                           VkBuffer probesUBO,   VkDeviceSize probesRange,
                           VkBuffer shadowViewSSBO, VkDeviceSize shadowViewRange);

    /**
     * @brief Update only the camera UBO for the current frame (called every frame).
     */
    void UpdateCameraUBO(uint32_t frameIndex, VkBuffer buffer, VkDeviceSize offset,
                         VkDeviceSize range);

    // ── Set 1: Per-pass (G-buffer read) ───────────────────────────────────────

    void WriteGBufferReadSet(VkImageView pos, VkImageView normal,
                              VkImageView material, VkImageView extra,
                              VkImageView depth, VkSampler sampler,
                              VkImageView shadowMapArray, VkSampler shadowSampler);

    // ── Set 2: Per-draw ───────────────────────────────────────────────────────

    void WriteMaterialSet(VkBuffer materialSSBO, VkDeviceSize materialRange,
                           VkBuffer skeletalSSBO, VkDeviceSize skeletalRange);

    /**
     * @brief Update bindless texture array entry.
     *        Call this when a new texture is registered.
     */
    void UpdateBindlessTexture(uint32_t index, VkImageView view, VkSampler sampler);

    // ── Set 3: Global indirect ────────────────────────────────────────────────

    void WriteGlobalSet(VkBuffer drawCommandSSBO, VkDeviceSize drawCmdRange,
                         VkBuffer drawInfoSSBO,    VkDeviceSize drawInfoRange,
                         VkBuffer vertexSSBO,      VkDeviceSize vertexRange,
                         VkBuffer indexSSBO,       VkDeviceSize indexRange);

    // ── Descriptor set handles ─────────────────────────────────────────────────

    VkDescriptorSet GetPerFrameSet(uint32_t frameIndex) const
    { return m_PerFrameSets[frameIndex]; }

    VkDescriptorSet GetGBufferReadSet()  const { return m_GBufferReadSet;  }
    VkDescriptorSet GetMaterialSet()     const { return m_MaterialSet;     }
    VkDescriptorSet GetGlobalSet()       const { return m_GlobalSet;       }

    // ── Layouts (for pipeline creation) ───────────────────────────────────────

    VkDescriptorSetLayout GetPerFrameLayout()   const { return m_PerFrameLayout;   }
    VkDescriptorSetLayout GetPerPassLayout()    const { return m_PerPassLayout;    }
    VkDescriptorSetLayout GetPerDrawLayout()    const { return m_PerDrawLayout;    }
    VkDescriptorSetLayout GetGlobalLayout()     const { return m_GlobalLayout;     }

    /**
     * @brief Array of all 4 layouts in order [0..3] for pipeline layout creation.
     */
    std::array<VkDescriptorSetLayout, 4> GetAllLayouts() const
    {
        return { m_PerFrameLayout, m_PerPassLayout, m_PerDrawLayout, m_GlobalLayout };
    }

private:
    void CreateLayouts(uint32_t bindlessCount);
    void CreatePool(uint32_t maxSets);
    void AllocateSets();

    VulkanContext* m_Ctx = nullptr;

    VkDescriptorPool      m_Pool           = VK_NULL_HANDLE;

    // Layouts
    VkDescriptorSetLayout m_PerFrameLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_PerPassLayout  = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_PerDrawLayout  = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_GlobalLayout   = VK_NULL_HANDLE;

    // Sets
    std::array<VkDescriptorSet, FRAMES_IN_FLIGHT> m_PerFrameSets{};
    VkDescriptorSet m_GBufferReadSet = VK_NULL_HANDLE;
    VkDescriptorSet m_MaterialSet    = VK_NULL_HANDLE;
    VkDescriptorSet m_GlobalSet      = VK_NULL_HANDLE;

    uint32_t m_BindlessCount = 0;
};

} // namespace Ermine::Vulkan
