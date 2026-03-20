/* Start Header ************************************************************************/
/*!
\file       VulkanRenderer.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Main Vulkan renderer implementation.
            Triple-buffered, deferred + forward pipeline with adaptive
            buffer sizing and hardened feature testing at startup.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanRenderer.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::Init(GLFWwindow* window,
                           int screenWidth, int screenHeight,
                           const std::string& appName,
                           bool enableValidation,
                           const std::string& shaderDir)
{
    m_ShaderDir      = shaderDir;
    m_ViewportWidth  = screenWidth;
    m_ViewportHeight = screenHeight;

    // ── 1. Initialize core Vulkan context ─────────────────────────────────────
    m_Ctx.Init(appName, window, enableValidation);

    // ── 2. Create triple-buffered swapchain ───────────────────────────────────
    m_Swapchain.Init(m_Ctx,
                     static_cast<uint32_t>(screenWidth),
                     static_cast<uint32_t>(screenHeight));

    // ── 3. Create G-buffer attachments ────────────────────────────────────────
    m_GBuffer.Create(m_Ctx,
                     static_cast<uint32_t>(screenWidth),
                     static_cast<uint32_t>(screenHeight));

    // ── 4. Create HDR intermediate buffer ────────────────────────────────────
    CreateHDRBuffer();

    // ── 5. Create shadow map array ────────────────────────────────────────────
    CreateShadowMapArray();

    // ── 6. Create per-frame streaming ring buffers ────────────────────────────
    CreateStreamingBuffers();

    // ── 7. Create static device-local SSBOs ──────────────────────────────────
    CreateStaticBuffers();

    // ── 8. Create samplers ────────────────────────────────────────────────────
    CreateSamplers();

    // ── 9. Create render passes ───────────────────────────────────────────────
    m_RenderPasses.Init(m_Ctx, m_Swapchain, m_GBuffer,
                         k_HDRFormat, k_ShadowFormat);

    // ── 10. Create framebuffers ───────────────────────────────────────────────
    m_RenderPasses.RecreateFramebuffers(m_Swapchain, m_GBuffer,
                                         m_HDRBuffer.GetView(),
                                         static_cast<uint32_t>(screenWidth),
                                         static_cast<uint32_t>(screenHeight));

    // ── 11. Initialize descriptors ────────────────────────────────────────────
    InitDescriptors();

    // ── 12. Build pipelines ───────────────────────────────────────────────────
    BuildPipelines();
}

// ─────────────────────────────────────────────────────────────────────────────
// Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::Shutdown()
{
    if (!m_Ctx.IsValid()) return;

    vkDeviceWaitIdle(m_Ctx.GetDevice());

    // Destroy in reverse init order
    m_Pipelines.Destroy();
    m_Descriptors.Destroy();
    m_RenderPasses.Destroy();

    m_LinearRepeatSampler.Destroy();
    m_LinearClampSampler.Destroy();
    m_NearestClampSampler.Destroy();
    m_ShadowSampler.Destroy();

    m_MaterialSSBO.Destroy();
    m_SkeletalSSBO.Destroy();
    m_VertexSSBO.Destroy();
    m_IndexSSBO.Destroy();

    m_CameraUBORing.Destroy();
    m_LightsUBORing.Destroy();
    m_ProbesUBORing.Destroy();
    m_ShadowViewsRing.Destroy();
    m_DrawCommandsRing.Destroy();
    m_DrawInfoRing.Destroy();

    DestroyShadowMapArray();
    DestroyHDRBuffer();
    m_GBuffer.Destroy();
    m_Swapchain.Shutdown();
    m_Ctx.Shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginFrame
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanRenderer::BeginFrame()
{
    if (!m_Swapchain.AcquireNextImage(m_AcquiredImageIndex))
    {
        // Swapchain out of date — caller must call OnWindowResize
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdatePerFrameData
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::UpdatePerFrameData(
    const glm::mat4& view, const glm::mat4& projection,
    const glm::vec3& cameraPosition,
    float nearPlane, float farPlane,
    float elapsedTime, float deltaTime)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    // ── Camera UBO ────────────────────────────────────────────────────────────
    CameraUBO cam{};
    cam.view               = view;
    cam.projection         = projection;
    cam.viewProjection     = projection * view;
    cam.invView            = glm::inverse(view);
    cam.invProjection      = glm::inverse(projection);
    cam.cameraPosition     = glm::vec4(cameraPosition, nearPlane);
    cam.cameraDirection    = glm::vec4(glm::vec3(-view[0][2], -view[1][2], -view[2][2]), farPlane);
    cam.resolution         = glm::vec2(static_cast<float>(m_ViewportWidth),
                                        static_cast<float>(m_ViewportHeight));
    cam.invResolution      = 1.0f / cam.resolution;
    cam.elapsedTime        = elapsedTime;
    cam.deltaTime          = deltaTime;
    cam.frameIndex         = static_cast<uint32_t>(m_FrameNumber);

    memcpy(m_CameraUBORing.GetFramePtr(fi), &cam, sizeof(cam));

    // Lights UBO, Probes UBO, and ShadowViews SSBO would be populated by
    // the ECS systems (LightSystem, etc.) calling UpdateLightsUBO-equivalent
    // functions — handled externally before RecordFrame().
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordFrame
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordFrame(const glm::mat4& view, const glm::mat4& projection)
{
    VkCommandBuffer cmd = m_Swapchain.GetCurrentFrame().commandBuffer;

    if (m_DrawDataDirty)
    {
        RebuildDrawData();
        UploadDrawDataToGPU();
        m_DrawDataDirty = false;
    }

    // ── Set dynamic viewport + scissor ────────────────────────────────────────
    // (set once; all passes share the same viewport unless shadow/picking)
    SetViewportScissor(cmd);

    // ── Pass 1: Depth pre-pass ────────────────────────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "DepthPrePass", 0.8f, 0.8f, 0.2f);
    RecordDepthPrePass(cmd, view, projection);
    m_Ctx.EndDebugLabel(cmd);

    // ── Pass 2: Shadow pass ───────────────────────────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "ShadowPass", 0.2f, 0.2f, 0.8f);
    RecordShadowPasses(cmd);
    m_Ctx.EndDebugLabel(cmd);

    // ── Pass 3: G-buffer (geometry) pass ──────────────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "GeometryPass", 0.2f, 0.8f, 0.2f);
    RecordGeometryPass(cmd, view, projection);
    m_Ctx.EndDebugLabel(cmd);

    // ── Pass 4: Deferred lighting pass ────────────────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "LightingPass", 1.0f, 0.8f, 0.0f);
    RecordLightingPass(cmd, view, projection);
    m_Ctx.EndDebugLabel(cmd);

    // ── Pass 5: Forward pass (custom + transparent) ────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "ForwardPass", 0.0f, 0.8f, 1.0f);
    RecordForwardPass(cmd, view, projection);
    m_Ctx.EndDebugLabel(cmd);

    // ── Pass 6: Post-process + present ────────────────────────────────────────
    m_Ctx.BeginDebugLabel(cmd, "PostProcess", 1.0f, 0.4f, 0.0f);
    RecordPostProcessPass(cmd, m_AcquiredImageIndex);
    m_Ctx.EndDebugLabel(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// EndFrame
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanRenderer::EndFrame()
{
    bool ok = m_Swapchain.SubmitAndPresent(m_AcquiredImageIndex);
    m_Swapchain.AdvanceFrame();
    ++m_FrameNumber;
    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
// OnWindowResize
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::OnWindowResize(int width, int height)
{
    if (width == 0 || height == 0) return;

    m_ViewportWidth  = width;
    m_ViewportHeight = height;

    vkDeviceWaitIdle(m_Ctx.GetDevice());

    m_Swapchain.Recreate(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    m_GBuffer.Resize(m_Ctx, static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    DestroyHDRBuffer();
    CreateHDRBuffer();

    m_RenderPasses.RecreateFramebuffers(m_Swapchain, m_GBuffer,
                                         m_HDRBuffer.GetView(),
                                         static_cast<uint32_t>(width),
                                         static_cast<uint32_t>(height));

    // Re-write G-buffer read descriptors (image views changed)
    m_Descriptors.WriteGBufferReadSet(
        m_GBuffer.packed0.GetView(), m_GBuffer.packed1.GetView(),
        m_GBuffer.packed2.GetView(), m_GBuffer.packed3.GetView(),
        m_GBuffer.depth.GetView(),
        m_LinearClampSampler.Get(),
        m_ShadowMapArray.GetView(),
        m_ShadowSampler.Get()
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordDepthPrePass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordDepthPrePass(VkCommandBuffer cmd,
                                         const glm::mat4& view, const glm::mat4& projection)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    m_RenderPasses.BeginDepthPrePass(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetDepthPrePass());

    // Depth pre-pass needs: set 0 (camera VP) and set 3 (draw info for model matrices).
    // Sets 1 and 2 are bound to null-compatible handles via the full layout; the
    // shader only accesses set 0 and set 3.
    VkDescriptorSet sets[] = {
        m_Descriptors.GetPerFrameSet(fi),   // set 0 – camera UBO
        m_Descriptors.GetGBufferReadSet(),   // set 1 – bound but not accessed
        m_Descriptors.GetMaterialSet(),      // set 2 – bound but not accessed
        m_Descriptors.GetGlobalSet()         // set 3 – draw info (model matrices)
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_Pipelines.GetLayout(), 0, 4, sets, 0, nullptr);

    if (!m_DrawCommands.empty())
    {
        // Indirect draw from the GPU ring buffer
        VkBuffer indirectBuf = m_DrawCommandsRing.GetBuffer();
        VkDeviceSize offset  = static_cast<VkDeviceSize>(fi) * m_DrawCommandsRing.GetPartitionSize();
        vkCmdDrawIndexedIndirect(cmd, indirectBuf, offset,
                                  static_cast<uint32_t>(m_DrawCommands.size()),
                                  sizeof(VkDrawIndexedIndirectCommand));
    }

    m_RenderPasses.EndPass(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordShadowPasses
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordShadowPasses(VkCommandBuffer cmd)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetShadowMap());

    // Shadow pass needs: set 0 (shadow views SSBO at binding 3) and set 3 (draw info).
    // pc.drawIndex = layer index for selecting the light-space VP matrix.
    {
        VkDescriptorSet shadowSets[] = {
            m_Descriptors.GetPerFrameSet(fi),   // set 0 – shadow views SSBO
            m_Descriptors.GetGBufferReadSet(),   // set 1 – bound but not accessed
            m_Descriptors.GetMaterialSet(),      // set 2 – bound but not accessed
            m_Descriptors.GetGlobalSet()         // set 3 – draw info (model matrices)
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 m_Pipelines.GetLayout(), 0, 4, shadowSets, 0, nullptr);
    }

    for (uint32_t layer = 0; layer < m_ShadowMapLayers; ++layer)
    {
        // Set shadow map viewport (resolution may differ from main viewport)
        VkViewport shadowVP{};
        shadowVP.width    = static_cast<float>(m_ShadowResolution);
        shadowVP.height   = static_cast<float>(m_ShadowResolution);
        shadowVP.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &shadowVP);

        VkRect2D shadowSC{{ 0, 0 }, { m_ShadowResolution, m_ShadowResolution }};
        vkCmdSetScissor(cmd, 0, 1, &shadowSC);

        m_RenderPasses.BeginShadowPass(cmd, layer, m_ShadowResolution);

        PushConstants pc{};
        pc.drawIndex = layer;
        vkCmdPushConstants(cmd, m_Pipelines.GetLayout(),
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                            0, sizeof(pc), &pc);

        if (!m_DrawCommands.empty())
        {
            VkBuffer indirectBuf = m_DrawCommandsRing.GetBuffer();
            VkDeviceSize offset  = static_cast<VkDeviceSize>(fi) * m_DrawCommandsRing.GetPartitionSize();
            vkCmdDrawIndexedIndirect(cmd, indirectBuf, offset,
                                      static_cast<uint32_t>(m_DrawCommands.size()),
                                      sizeof(VkDrawIndexedIndirectCommand));
        }

        m_RenderPasses.EndPass(cmd);
    }

    // Restore main viewport
    SetViewportScissor(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordGeometryPass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordGeometryPass(VkCommandBuffer cmd,
                                         const glm::mat4& view, const glm::mat4& projection)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    m_RenderPasses.BeginGeometryPass(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetGBuffer());

    VkDescriptorSet sets[] = {
        m_Descriptors.GetPerFrameSet(fi),
        m_Descriptors.GetGBufferReadSet(),
        m_Descriptors.GetMaterialSet(),
        m_Descriptors.GetGlobalSet()
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_Pipelines.GetLayout(), 0, 4, sets, 0, nullptr);

    if (!m_DrawCommands.empty())
    {
        VkBuffer indirectBuf = m_DrawCommandsRing.GetBuffer();
        VkDeviceSize offset  = static_cast<VkDeviceSize>(fi) * m_DrawCommandsRing.GetPartitionSize();
        vkCmdDrawIndexedIndirect(cmd, indirectBuf, offset,
                                  static_cast<uint32_t>(m_DrawCommands.size()),
                                  sizeof(VkDrawIndexedIndirectCommand));
    }

    m_RenderPasses.EndPass(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordLightingPass  (fullscreen deferred lighting)
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordLightingPass(VkCommandBuffer cmd,
                                         const glm::mat4& view, const glm::mat4& projection)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    m_RenderPasses.BeginLightingPass(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetLighting());

    VkDescriptorSet sets[] = {
        m_Descriptors.GetPerFrameSet(fi),
        m_Descriptors.GetGBufferReadSet()
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_Pipelines.GetLayout(), 0, 2, sets, 0, nullptr);

    // Fullscreen triangle (no vertex buffer needed — generated in vertex shader)
    vkCmdDraw(cmd, 3, 1, 0, 0);

    m_RenderPasses.EndPass(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordForwardPass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordForwardPass(VkCommandBuffer cmd,
                                        const glm::mat4& view, const glm::mat4& projection)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    m_RenderPasses.BeginForwardPass(cmd);

    VkDescriptorSet sets[] = {
        m_Descriptors.GetPerFrameSet(fi),
        m_Descriptors.GetGBufferReadSet(),
        m_Descriptors.GetMaterialSet(),
        m_Descriptors.GetGlobalSet()
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_Pipelines.GetLayout(), 0, 4, sets, 0, nullptr);

    // Opaque custom shader pass
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetForwardOpaque());
    // (draw calls would be dispatched here for entities with custom shaders)

    // Transparent pass
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetForwardTransp());
    // (sorted transparent draw calls here)

    // Debug lines
    if (!m_DebugLines.empty())
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetDebugLine());
        // (debug line draw calls)
    }

    m_RenderPasses.EndPass(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// RecordPostProcessPass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::RecordPostProcessPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex)
{
    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    m_RenderPasses.BeginPostProcessPass(cmd, swapchainImageIndex);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines.GetPostProcess());

    VkDescriptorSet sets[] = {
        m_Descriptors.GetPerFrameSet(fi),
        m_Descriptors.GetGBufferReadSet()
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             m_Pipelines.GetLayout(), 0, 2, sets, 0, nullptr);

    // Push post-process parameters
    struct PostProcessParams {
        float exposure;
        float contrast;
        float saturation;
        float gamma;
        float bloomStrength;
        float bloomThreshold;
        uint32_t flags; // bit0=FXAA, bit1=bloom, bit2=tonemap, bit3=gamma, bit4=vignette
        float _pad;
    } params{};
    params.exposure       = m_Exposure;
    params.contrast       = m_Contrast;
    params.saturation     = m_Saturation;
    params.gamma          = m_Gamma;
    params.bloomStrength  = m_BloomStrength;
    params.bloomThreshold = m_BloomThreshold;
    params.flags = (m_FXAAEnabled        ? 0x01 : 0)
                 | (m_BloomEnabled       ? 0x02 : 0)
                 | (m_ToneMappingEnabled ? 0x04 : 0)
                 | (m_GammaCorrectionEnabled ? 0x08 : 0)
                 | (m_VignetteEnabled    ? 0x10 : 0);

    vkCmdPushConstants(cmd, m_Pipelines.GetLayout(),
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(params), &params);

    // Fullscreen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);

    m_RenderPasses.EndPass(cmd);
}

// ─────────────────────────────────────────────────────────────────────────────
// CompileDrawData / RebuildDrawData / UploadDrawDataToGPU
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::CompileDrawData()
{
    if (m_DrawDataDirty)
    {
        RebuildDrawData();
        UploadDrawDataToGPU();
        m_DrawDataDirty = false;
    }
}

void VulkanRenderer::RebuildDrawData()
{
    // This function would traverse the ECS to rebuild m_DrawCommands / m_DrawInfos
    // identically to the OpenGL Renderer::RebuildDrawData(), but writing
    // VkDrawIndexedIndirectCommand instead of GL's DrawElementsIndirectCommand.
    // The logic is delegated to the calling ECS system; here we just clear.
    // (The real implementation is in the engine integration layer.)
}

void VulkanRenderer::UploadDrawDataToGPU()
{
    if (m_DrawCommands.empty()) return;

    uint32_t fi = m_Swapchain.GetCurrentFrameIndex();

    // Write draw commands to the current frame's ring partition
    void* cmdPtr  = m_DrawCommandsRing.GetFramePtr(fi);
    void* infoPtr = m_DrawInfoRing.GetFramePtr(fi);

    memcpy(cmdPtr,  m_DrawCommands.data(),
           m_DrawCommands.size() * sizeof(VkDrawIndexedIndirectCommand));
    memcpy(infoPtr, m_DrawInfos.data(),
           m_DrawInfos.size()    * sizeof(DrawInfo));
}

// ─────────────────────────────────────────────────────────────────────────────
// UploadMaterials / UpdateMaterial
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::UploadMaterials(const std::vector<graphics::MaterialSSBO>& materials)
{
    if (materials.empty()) return;

    VkDeviceSize size = materials.size() * sizeof(graphics::MaterialSSBO);
    auto staging = VulkanBuffer::CreateStaging(m_Ctx, size);
    memcpy(staging.GetMapped(), materials.data(), static_cast<size_t>(size));
    m_MaterialSSBO.CopyFrom(m_Ctx, staging, 0, 0, size);
    m_MaterialsDirty = false;
}

void VulkanRenderer::UpdateMaterial(const graphics::MaterialSSBO& data, uint32_t index)
{
    VkDeviceSize offset = static_cast<VkDeviceSize>(index) * sizeof(graphics::MaterialSSBO);
    VkDeviceSize size   = sizeof(graphics::MaterialSSBO);
    auto staging = VulkanBuffer::CreateStaging(m_Ctx, size);
    memcpy(staging.GetMapped(), &data, static_cast<size_t>(size));
    m_MaterialSSBO.CopyFrom(m_Ctx, staging, 0, offset, size);
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterTexture
// ─────────────────────────────────────────────────────────────────────────────

uint32_t VulkanRenderer::RegisterTexture(VkImageView view, VkSampler sampler)
{
    uint32_t idx = m_NextTextureIndex++;
    m_Descriptors.UpdateBindlessTexture(idx, view, sampler);
    return idx;
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug draw
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::SubmitDebugLine(const glm::vec3& from, const glm::vec3& to,
                                      const glm::vec3& color)
{
    m_DebugLines.push_back({ from, color });
    m_DebugLines.push_back({ to,   color });
}

void VulkanRenderer::SubmitDebugTriangle(const glm::vec3& a, const glm::vec3& b,
                                          const glm::vec3& c, const glm::vec3& color)
{
    m_DebugTriangles.push_back({ a, color });
    m_DebugTriangles.push_back({ b, color });
    m_DebugTriangles.push_back({ c, color });
}

// ─────────────────────────────────────────────────────────────────────────────
// Resource creation helpers
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderer::CreateHDRBuffer()
{
    m_HDRBuffer = VulkanImage::CreateColorAttachment(
        m_Ctx,
        static_cast<uint32_t>(m_ViewportWidth),
        static_cast<uint32_t>(m_ViewportHeight),
        k_HDRFormat
    );
}

void VulkanRenderer::DestroyHDRBuffer()
{
    m_HDRBuffer.Destroy();
}

void VulkanRenderer::CreateShadowMapArray()
{
    const auto& cfg = m_Ctx.GetAdaptiveConfig();
    m_ShadowResolution = cfg.shadowMapResolution;
    m_ShadowMapLayers  = std::min(cfg.maxShadowLayers,
                                   static_cast<uint32_t>(SHADOW_MAX_LAYERS_DESIRED));

    m_ShadowMapArray = VulkanImage::CreateTextureArray(
        m_Ctx,
        m_ShadowResolution,
        m_ShadowResolution,
        m_ShadowMapLayers,
        k_ShadowFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void VulkanRenderer::DestroyShadowMapArray()
{
    m_ShadowMapArray.Destroy();
}

void VulkanRenderer::CreateStreamingBuffers()
{
    const auto& cfg = m_Ctx.GetAdaptiveConfig();

    m_CameraUBORing.Create  (m_Ctx, cfg.cameraUBOSize,   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_LightsUBORing.Create  (m_Ctx, cfg.lightsUBOSize,   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ProbesUBORing.Create  (m_Ctx, cfg.probesUBOSize,   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    m_ShadowViewsRing.Create(m_Ctx, cfg.shadowViewsSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    m_DrawCommandsRing.Create(m_Ctx, cfg.drawCommandsSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
    m_DrawInfoRing.Create   (m_Ctx, cfg.drawInfoSize,    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
}

void VulkanRenderer::CreateStaticBuffers()
{
    const auto& cfg = m_Ctx.GetAdaptiveConfig();

    m_MaterialSSBO = VulkanBuffer::CreateGPU(m_Ctx,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              nullptr, cfg.materialSSBOSize);
    m_SkeletalSSBO = VulkanBuffer::CreateGPU(m_Ctx,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              nullptr, cfg.skeletalSSBOSize);
    m_VertexSSBO   = VulkanBuffer::CreateGPU(m_Ctx,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                              nullptr, cfg.vertexSSBOSize);
    m_IndexSSBO    = VulkanBuffer::CreateGPU(m_Ctx,
                                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                              nullptr, cfg.indexSSBOSize);
}

void VulkanRenderer::CreateSamplers()
{
    float aniso = m_Ctx.GetAdaptiveConfig().maxAnisotropy;
    m_LinearRepeatSampler = VulkanSampler::CreateLinearRepeat(m_Ctx.GetDevice(), aniso);
    m_LinearClampSampler  = VulkanSampler::CreateLinearClamp (m_Ctx.GetDevice());
    m_NearestClampSampler = VulkanSampler::CreateNearestClamp(m_Ctx.GetDevice());
    m_ShadowSampler       = VulkanSampler::CreateShadowSampler(m_Ctx.GetDevice());
}

void VulkanRenderer::InitDescriptors()
{
    const auto& cfg = m_Ctx.GetAdaptiveConfig();

    m_Descriptors.Init(m_Ctx, cfg.maxBindlessTextures);

    // Write per-frame sets for all 3 frames
    for (uint32_t fi = 0; fi < FRAMES_IN_FLIGHT; ++fi)
    {
        auto camDesc    = m_CameraUBORing.GetFrameDescriptor(fi);
        auto lightDesc  = m_LightsUBORing.GetFrameDescriptor(fi);
        auto probeDesc  = m_ProbesUBORing.GetFrameDescriptor(fi);
        auto shadowDesc = m_ShadowViewsRing.GetFrameDescriptor(fi);

        m_Descriptors.WritePerFrameSet(
            fi,
            camDesc.buffer,   camDesc.range,
            lightDesc.buffer, lightDesc.range,
            probeDesc.buffer, probeDesc.range,
            shadowDesc.buffer,shadowDesc.range
        );
    }

    // Write G-buffer read set (uses the G-buffer image views)
    m_Descriptors.WriteGBufferReadSet(
        m_GBuffer.packed0.GetView(), m_GBuffer.packed1.GetView(),
        m_GBuffer.packed2.GetView(), m_GBuffer.packed3.GetView(),
        m_GBuffer.depth.GetView(),
        m_LinearClampSampler.Get(),
        m_ShadowMapArray.GetView(),
        m_ShadowSampler.Get()
    );

    // Write material + skeletal set
    m_Descriptors.WriteMaterialSet(
        m_MaterialSSBO.GetBuffer(), m_MaterialSSBO.GetSize(),
        m_SkeletalSSBO.GetBuffer(), m_SkeletalSSBO.GetSize()
    );

    // Write global indirect set
    auto cmdDesc  = m_DrawCommandsRing.GetFrameDescriptor(0);
    auto infoDesc = m_DrawInfoRing.GetFrameDescriptor(0);
    m_Descriptors.WriteGlobalSet(
        cmdDesc.buffer,             m_DrawCommandsRing.GetTotalSize(),
        infoDesc.buffer,            m_DrawInfoRing.GetTotalSize(),
        m_VertexSSBO.GetBuffer(),   m_VertexSSBO.GetSize(),
        m_IndexSSBO.GetBuffer(),    m_IndexSSBO.GetSize()
    );
}

void VulkanRenderer::BuildPipelines()
{
    m_Pipelines.Init(
        m_Ctx, m_Descriptors, m_ShaderDir,
        m_RenderPasses.GetGeometryPass(),
        m_RenderPasses.GetLightingPass(),
        m_RenderPasses.GetForwardPass(),
        m_RenderPasses.GetPostProcessPass(),
        m_RenderPasses.GetShadowPass(),
        m_RenderPasses.GetDepthPrePass()
    );
}

void VulkanRenderer::SetViewportScissor(VkCommandBuffer cmd) const
{
    VkViewport vp{};
    // Vulkan: flip Y by using negative height + offset at bottom
    // This matches OpenGL's coordinate system convention
    vp.x        = 0.0f;
    vp.y        = static_cast<float>(m_ViewportHeight);
    vp.width    = static_cast<float>(m_ViewportWidth);
    vp.height   = -static_cast<float>(m_ViewportHeight);  // Negative for Y-flip
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.extent = { static_cast<uint32_t>(m_ViewportWidth),
                       static_cast<uint32_t>(m_ViewportHeight) };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

} // namespace Ermine::Vulkan
