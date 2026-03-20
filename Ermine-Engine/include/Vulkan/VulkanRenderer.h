/* Start Header ************************************************************************/
/*!
\file       VulkanRenderer.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Main Vulkan renderer — triple-buffered, deferred + forward pipeline.

            Architecture:
            ┌────────────────────────────────────────────────────────────────┐
            │ VulkanRenderer                                                 │
            │   owns: VulkanContext   (instance, device, queues)             │
            │         VulkanSwapchain (triple buffering, sync objects)       │
            │         GBufferAttachments (4 MRTs + depth)                   │
            │         VulkanRenderPassManager (all passes + framebuffers)    │
            │         VulkanDescriptorManager (set layouts + pools + sets)   │
            │         VulkanPipelineManager   (all graphics pipelines)       │
            │         VulkanRingBuffers       (per-frame streaming SSBOs)    │
            │         GPU SSBOs              (static: mesh, material, etc.)  │
            │         Samplers               (linear, shadow, etc.)          │
            └────────────────────────────────────────────────────────────────┘

            Frame flow (triple buffering):
            1. AcquireNextImage  — waits for frame fence, acquires swapchain image
            2. UpdatePerFrameUBOs — camera, lights, probes → persistently mapped UBOs
            3. DepthPrePass       — depth-only; written to shared depth target
            4. ShadowPass         — per-light cascaded/spot/point depth writes
            5. GeometryPass       — G-buffer fill (indirect draw, 4 MRTs)
            6. LightingPass       — deferred lighting fullscreen quad
            7. ForwardPass        — opaque custom shaders + transparent sorted
            8. PostProcessPass    — bloom + FXAA + tone map → swapchain image
            9. SubmitAndPresent   — submit cmd buf, wait renderFinished, present

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanSwapchain.h"
#include "Vulkan/VulkanBuffer.h"
#include "Vulkan/VulkanImage.h"
#include "Vulkan/VulkanRenderPass.h"
#include "Vulkan/VulkanDescriptorManager.h"
#include "Vulkan/VulkanPipeline.h"
#include "Vulkan/VulkanShader.h"

// Engine types (from existing codebase)
#include "Components.h"
#include "shadow_config.h"
#include "DrawCommands.h"
#include "Material.h"

#include <string>
#include <vector>
#include <memory>

struct GLFWwindow;

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanRenderer
// ─────────────────────────────────────────────────────────────────────────────

class VulkanRenderer
{
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() { Shutdown(); }

    VulkanRenderer(const VulkanRenderer&)            = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Initialize the Vulkan renderer.
     *
     * @param window          GLFW window handle (for surface + initial size).
     * @param screenWidth     Initial framebuffer width.
     * @param screenHeight    Initial framebuffer height.
     * @param appName         Embedded in VkApplicationInfo.
     * @param enableValidation Enable Vulkan validation layers (debug builds).
     * @param shaderDir       Path to compiled SPIR-V directory.
     */
    void Init(GLFWwindow* window,
              int screenWidth, int screenHeight,
              const std::string& appName  = "ERMINE-ENGINE",
              bool enableValidation       = false,
              const std::string& shaderDir = "Resources/Shaders/Vulkan/SPIRV");

    /**
     * @brief Destroy all Vulkan resources.
     *        Safe to call even if Init was not called.
     */
    void Shutdown();

    // ── Per-frame interface ───────────────────────────────────────────────────

    /**
     * @brief Begin a new frame: acquire swapchain image, reset command buffer.
     *        Returns false if the swapchain needs recreation (window resize).
     */
    bool BeginFrame();

    /**
     * @brief Upload per-frame CPU data: camera UBO, lights UBO, probes UBO,
     *        shadow view SSBO, and draw commands/infos.
     */
    void UpdatePerFrameData(const glm::mat4& view, const glm::mat4& projection,
                             const glm::vec3& cameraPosition,
                             float nearPlane, float farPlane,
                             float elapsedTime, float deltaTime);

    /**
     * @brief Record all render passes into the current frame's command buffer.
     *        Call between BeginFrame() and EndFrame().
     */
    void RecordFrame(const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief Submit and present the frame. Advances to the next frame slot.
     *        Returns false if swapchain needs recreation.
     */
    bool EndFrame();

    /**
     * @brief Handle window resize: recreate swapchain, G-buffer, framebuffers.
     */
    void OnWindowResize(int width, int height);

    // ── Draw data management (mirrors existing Renderer interface) ────────────

    /**
     * @brief Mark all draw data for rebuild next frame.
     *        Called when entities, materials, or meshes change.
     */
    void MarkDrawDataForRebuild()   { m_DrawDataDirty = true; }
    void MarkMaterialsDirty()       { m_MaterialsDirty = true; m_DrawDataDirty = true; }

    // ── Material management ───────────────────────────────────────────────────

    /**
     * @brief Upload all compiled materials to the material SSBO.
     *        Called once after scene load, or when materials change.
     */
    void UploadMaterials(const std::vector<graphics::MaterialSSBO>& materials);

    /**
     * @brief Update a single material entry in the SSBO.
     */
    void UpdateMaterial(const graphics::MaterialSSBO& data, uint32_t index);

    // ── Texture registration ──────────────────────────────────────────────────

    /**
     * @brief Register a texture in the bindless array.
     *        Returns the array index for use in material SSBO.
     */
    uint32_t RegisterTexture(VkImageView view, VkSampler sampler);

    // ── Debug rendering ───────────────────────────────────────────────────────

    void SubmitDebugLine    (const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);
    void SubmitDebugTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color);

    // ── Accessors ─────────────────────────────────────────────────────────────

    const VulkanContext&          GetContext()      const { return m_Ctx;      }
    const VulkanSwapchain&        GetSwapchain()    const { return m_Swapchain;}
    const AdaptiveConfig&         GetAdaptive()     const { return m_Ctx.GetAdaptiveConfig(); }

    VkExtent2D GetExtent() const { return m_Swapchain.GetExtent(); }
    bool       IsValid()   const { return m_Ctx.IsValid();         }

    // ── Post-processing toggles (mirror existing Renderer public members) ─────
    bool  m_FXAAEnabled            = true;
    bool  m_BloomEnabled           = true;
    bool  m_ToneMappingEnabled     = true;
    bool  m_GammaCorrectionEnabled = true;
    bool  m_SSAOEnabled            = false;
    bool  m_VignetteEnabled        = false;
    bool  m_MotionBlurEnabled      = false;
    bool  m_FilmGrainEnabled       = false;
    bool  m_ShowSkybox             = true;
    bool  m_FogEnabled             = false;

    float m_Exposure         = 1.0f;
    float m_Contrast         = 1.0f;
    float m_Saturation       = 1.0f;
    float m_Gamma            = 2.2f;
    float m_BloomThreshold   = 1.0f;
    float m_BloomStrength    = 0.04f;

    glm::vec3 m_AmbientColor     = glm::vec3(1.0f);
    float     m_AmbientIntensity = 0.08f;

private:
    // ── Internal render pass recording ───────────────────────────────────────

    void RecordDepthPrePass   (VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj);
    void RecordShadowPasses   (VkCommandBuffer cmd);
    void RecordGeometryPass   (VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj);
    void RecordLightingPass   (VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj);
    void RecordForwardPass    (VkCommandBuffer cmd, const glm::mat4& view, const glm::mat4& proj);
    void RecordPostProcessPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex);

    // ── Draw data compilation ─────────────────────────────────────────────────
    void CompileDrawData();
    void RebuildDrawData();
    void UploadDrawDataToGPU();

    // ── Resource creation helpers ─────────────────────────────────────────────
    void CreateHDRBuffer();
    void CreateShadowMapArray();
    void CreateStreamingBuffers();
    void CreateStaticBuffers();
    void CreateSamplers();
    void InitDescriptors();
    void BuildPipelines();

    void DestroyHDRBuffer();
    void DestroyShadowMapArray();

    // ── Viewport helper ───────────────────────────────────────────────────────
    void SetViewportScissor(VkCommandBuffer cmd) const;

    // ── State ─────────────────────────────────────────────────────────────────

    VulkanContext              m_Ctx;
    VulkanSwapchain            m_Swapchain;
    GBufferAttachments         m_GBuffer;
    VulkanRenderPassManager    m_RenderPasses;
    VulkanDescriptorManager    m_Descriptors;
    VulkanPipelineManager      m_Pipelines;

    // HDR intermediate render target
    VulkanImage                m_HDRBuffer;
    static constexpr VkFormat  k_HDRFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    // Shadow map texture array
    VulkanImage                m_ShadowMapArray;
    uint32_t                   m_ShadowMapLayers = 0;
    uint32_t                   m_ShadowResolution = 0;
    static constexpr VkFormat  k_ShadowFormat = VK_FORMAT_D32_SFLOAT;

    // ── Streaming ring buffers (triple-buffered per-frame writes) ─────────────
    VulkanRingBuffer           m_CameraUBORing;
    VulkanRingBuffer           m_LightsUBORing;
    VulkanRingBuffer           m_ProbesUBORing;
    VulkanRingBuffer           m_ShadowViewsRing;
    VulkanRingBuffer           m_DrawCommandsRing;
    VulkanRingBuffer           m_DrawInfoRing;

    // ── Static device-local SSBOs (uploaded once) ──────────────────────────────
    VulkanBuffer               m_MaterialSSBO;
    VulkanBuffer               m_SkeletalSSBO;
    VulkanBuffer               m_VertexSSBO;
    VulkanBuffer               m_IndexSSBO;

    // ── Samplers ──────────────────────────────────────────────────────────────
    VulkanSampler              m_LinearRepeatSampler;
    VulkanSampler              m_LinearClampSampler;
    VulkanSampler              m_NearestClampSampler;
    VulkanSampler              m_ShadowSampler;

    // ── Draw data ─────────────────────────────────────────────────────────────
    std::vector<VkDrawIndexedIndirectCommand> m_DrawCommands;
    std::vector<DrawInfo>                     m_DrawInfos;
    bool m_DrawDataDirty   = true;
    bool m_MaterialsDirty  = true;

    // ── Debug draw vertices ───────────────────────────────────────────────────
    struct DebugVertex { glm::vec3 pos; glm::vec3 color; };
    std::vector<DebugVertex> m_DebugLines;
    std::vector<DebugVertex> m_DebugTriangles;

    // ── Bindless texture registry ─────────────────────────────────────────────
    uint32_t m_NextTextureIndex = 0;

    // ── Frame tracking ────────────────────────────────────────────────────────
    uint32_t m_AcquiredImageIndex = 0;
    uint64_t m_FrameNumber        = 0;

    std::string m_ShaderDir;
    int m_ViewportWidth  = 0;
    int m_ViewportHeight = 0;
};

} // namespace Ermine::Vulkan
