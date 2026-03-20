/* Start Header ************************************************************************/
/*!
\file       VulkanRenderPass.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan render pass definitions for the deferred + forward pipeline.

            Passes and their framebuffers:
            ┌──────────────────┬──────────────────────────────────────────────┐
            │ DepthPrePass     │ Depth-only write (early-Z optimization)      │
            │ ShadowMap        │ Per-light depth array write                  │
            │ Geometry         │ G-buffer write (4 MRTs + depth)              │
            │ Lighting         │ Fullscreen deferred lighting → HDR color buf  │
            │ ForwardOpaque    │ Custom shader opaque objects                  │
            │ ForwardTransp    │ Transparent objects (back-to-front sorted)    │
            │ PostProcess      │ Bloom, FXAA, tone map → swapchain             │
            └──────────────────┴──────────────────────────────────────────────┘

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include "VulkanImage.h"
#include "VulkanSwapchain.h"
#include <array>
#include <vector>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanRenderPassManager
// ─────────────────────────────────────────────────────────────────────────────

class VulkanRenderPassManager
{
public:
    VulkanRenderPassManager() = default;
    ~VulkanRenderPassManager() { Destroy(); }

    VulkanRenderPassManager(const VulkanRenderPassManager&)            = delete;
    VulkanRenderPassManager& operator=(const VulkanRenderPassManager&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Create all render passes.
     *        Must be called after VulkanContext and VulkanSwapchain are initialized.
     *
     * @param ctx          Active VulkanContext.
     * @param swapchain    Triple-buffered swapchain (for present format).
     * @param gbuffer      G-buffer attachments (created before this call).
     * @param hdrFormat    Format of the HDR intermediate render target.
     * @param shadowFormat Depth format used for shadow map array.
     */
    void Init(VulkanContext& ctx,
              const VulkanSwapchain& swapchain,
              const GBufferAttachments& gbuffer,
              VkFormat hdrFormat,
              VkFormat shadowFormat);

    void Destroy();

    /**
     * @brief Recreate framebuffers after window resize (render passes themselves survive).
     */
    void RecreateFramebuffers(const VulkanSwapchain& swapchain,
                               const GBufferAttachments& gbuffer,
                               VkImageView hdrColorView,
                               uint32_t width, uint32_t height);

    // ── Render pass handles ───────────────────────────────────────────────────

    VkRenderPass GetDepthPrePass()   const { return m_DepthPrePass;  }
    VkRenderPass GetShadowPass()     const { return m_ShadowPass;    }
    VkRenderPass GetGeometryPass()   const { return m_GeometryPass;  }
    VkRenderPass GetLightingPass()   const { return m_LightingPass;  }
    VkRenderPass GetForwardPass()    const { return m_ForwardPass;   }
    VkRenderPass GetPostProcessPass()const { return m_PostProcessPass;}

    // ── Framebuffer handles ────────────────────────────────────────────────────

    VkFramebuffer GetDepthPrePassFB()    const { return m_DepthPrePassFB;   }
    VkFramebuffer GetGeometryPassFB()    const { return m_GeometryPassFB;   }
    VkFramebuffer GetLightingPassFB()    const { return m_LightingPassFB;   }
    VkFramebuffer GetForwardPassFB()     const { return m_ForwardPassFB;    }
    VkFramebuffer GetShadowPassFB(uint32_t layer) const;

    /**
     * @brief Get the present framebuffer for the given swapchain image index.
     */
    VkFramebuffer GetPostProcessFB(uint32_t imageIndex) const
    { return m_PostProcessFBs[imageIndex]; }

    // ── Viewport/extent ────────────────────────────────────────────────────────
    VkExtent2D GetExtent() const { return m_Extent; }

    // ── Begin/end pass helpers ────────────────────────────────────────────────

    /**
     * @brief Record a vkCmdBeginRenderPass with standard clear values.
     */
    void BeginDepthPrePass   (VkCommandBuffer cmd) const;
    void BeginGeometryPass   (VkCommandBuffer cmd) const;
    void BeginLightingPass   (VkCommandBuffer cmd) const;
    void BeginForwardPass    (VkCommandBuffer cmd) const;
    void BeginPostProcessPass(VkCommandBuffer cmd, uint32_t imageIndex) const;
    void BeginShadowPass     (VkCommandBuffer cmd, uint32_t layer,
                               uint32_t shadowRes) const;

    void EndPass(VkCommandBuffer cmd) const { vkCmdEndRenderPass(cmd); }

private:
    // ── Pass creation ──────────────────────────────────────────────────────────
    void CreateDepthPrePass(VkFormat depthFmt);
    void CreateShadowPass  (VkFormat shadowFmt);
    void CreateGeometryPass(VkFormat depthFmt);
    void CreateLightingPass(VkFormat hdrFmt);
    void CreateForwardPass (VkFormat hdrFmt, VkFormat depthFmt);
    void CreatePostProcessPass(VkFormat swapchainFmt);

    // ── Framebuffer creation ──────────────────────────────────────────────────
    void CreateDepthPrePassFB(VkImageView depthView, uint32_t w, uint32_t h);
    void CreateGeometryPassFB(const GBufferAttachments& gb, uint32_t w, uint32_t h);
    void CreateLightingPassFB(VkImageView hdrView, uint32_t w, uint32_t h);
    void CreateForwardPassFB (VkImageView hdrView, VkImageView depthView,
                               uint32_t w, uint32_t h);
    void CreatePostProcessFBs(const VulkanSwapchain& sc);
    void CreateShadowPassFBs (uint32_t layers, uint32_t res, VkImageView arrayView);

    void DestroyFramebuffers();

    // ── State ──────────────────────────────────────────────────────────────────
    VulkanContext* m_Ctx = nullptr;
    VkExtent2D     m_Extent{};

    // Render pass objects
    VkRenderPass m_DepthPrePass   = VK_NULL_HANDLE;
    VkRenderPass m_ShadowPass     = VK_NULL_HANDLE;
    VkRenderPass m_GeometryPass   = VK_NULL_HANDLE;
    VkRenderPass m_LightingPass   = VK_NULL_HANDLE;
    VkRenderPass m_ForwardPass    = VK_NULL_HANDLE;
    VkRenderPass m_PostProcessPass= VK_NULL_HANDLE;

    // Framebuffer objects
    VkFramebuffer              m_DepthPrePassFB = VK_NULL_HANDLE;
    VkFramebuffer              m_GeometryPassFB = VK_NULL_HANDLE;
    VkFramebuffer              m_LightingPassFB = VK_NULL_HANDLE;
    VkFramebuffer              m_ForwardPassFB  = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_PostProcessFBs;  // One per swapchain image
    std::vector<VkFramebuffer> m_ShadowPassFBs;   // One per shadow map layer
};

} // namespace Ermine::Vulkan
