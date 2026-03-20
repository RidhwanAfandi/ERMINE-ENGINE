/* Start Header ************************************************************************/
/*!
\file       VulkanRenderPass.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Render pass and framebuffer creation/management.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanRenderPass.h"
#include <array>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::Init(
    VulkanContext& ctx,
    const VulkanSwapchain& swapchain,
    const GBufferAttachments& gbuffer,
    VkFormat hdrFormat,
    VkFormat shadowFormat)
{
    m_Ctx    = &ctx;
    m_Extent = swapchain.GetExtent();

    VkFormat depthFmt = ctx.GetCapabilities().FindDepthFormat();

    CreateDepthPrePass   (depthFmt);
    CreateShadowPass     (shadowFormat);
    CreateGeometryPass   (depthFmt);
    CreateLightingPass   (hdrFormat);
    CreateForwardPass    (hdrFormat, depthFmt);
    CreatePostProcessPass(swapchain.GetImageFormat());
}

void VulkanRenderPassManager::Destroy()
{
    if (!m_Ctx) return;
    VkDevice dev = m_Ctx->GetDevice();

    DestroyFramebuffers();

    auto destroy = [&](VkRenderPass& rp) {
        if (rp) { vkDestroyRenderPass(dev, rp, nullptr); rp = VK_NULL_HANDLE; }
    };
    destroy(m_DepthPrePass);
    destroy(m_ShadowPass);
    destroy(m_GeometryPass);
    destroy(m_LightingPass);
    destroy(m_ForwardPass);
    destroy(m_PostProcessPass);
    m_Ctx = nullptr;
}

void VulkanRenderPassManager::DestroyFramebuffers()
{
    VkDevice dev = m_Ctx->GetDevice();
    auto destroyFB = [&](VkFramebuffer& fb) {
        if (fb) { vkDestroyFramebuffer(dev, fb, nullptr); fb = VK_NULL_HANDLE; }
    };
    destroyFB(m_DepthPrePassFB);
    destroyFB(m_GeometryPassFB);
    destroyFB(m_LightingPassFB);
    destroyFB(m_ForwardPassFB);
    for (auto& fb : m_PostProcessFBs) if (fb) vkDestroyFramebuffer(dev, fb, nullptr);
    for (auto& fb : m_ShadowPassFBs)  if (fb) vkDestroyFramebuffer(dev, fb, nullptr);
    m_PostProcessFBs.clear();
    m_ShadowPassFBs.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// RecreateFramebuffers
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::RecreateFramebuffers(
    const VulkanSwapchain& swapchain,
    const GBufferAttachments& gbuffer,
    VkImageView hdrColorView,
    uint32_t width, uint32_t height)
{
    vkDeviceWaitIdle(m_Ctx->GetDevice());
    DestroyFramebuffers();
    m_Extent = { width, height };

    CreateDepthPrePassFB (gbuffer.depth.GetView(), width, height);
    CreateGeometryPassFB (gbuffer, width, height);
    CreateLightingPassFB (hdrColorView, width, height);
    CreateForwardPassFB  (hdrColorView, gbuffer.depth.GetView(), width, height);
    CreatePostProcessFBs (swapchain);
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateDepthPrePass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreateDepthPrePass(VkFormat depthFmt)
{
    VkAttachmentDescription depth{};
    depth.format         = depthFmt;
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &depth;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 2;
    ci.pDependencies   = deps;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_DepthPrePass));
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateShadowPass
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreateShadowPass(VkFormat shadowFmt)
{
    VkAttachmentDescription depth{};
    depth.format        = shadowFmt;
    depth.samples       = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &depth;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_ShadowPass));
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateGeometryPass  (G-buffer fill: 4 color MRTs + depth)
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreateGeometryPass(VkFormat depthFmt)
{
    // 4 color MRT attachments + depth
    std::array<VkAttachmentDescription, 5> attachments{};
    VkFormat colorFormats[] = {
        VK_FORMAT_R32G32B32A32_SFLOAT, // packed0: position
        VK_FORMAT_R16G16B16A16_SFLOAT, // packed1: normal
        VK_FORMAT_R8G8B8A8_UNORM,      // packed2: albedo/material
        VK_FORMAT_R8G8B8A8_UNORM,      // packed3: emissive/flags
    };
    for (int i = 0; i < 4; ++i)
    {
        attachments[i].format         = colorFormats[i];
        attachments[i].samples        = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[i].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[i].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[i].finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    // Depth attachment (reuse from pre-pass with LOAD)
    attachments[4].format         = depthFmt;
    attachments[4].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachments[4].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;   // Keep pre-pass depth
    attachments[4].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[4].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[4].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    std::array<VkAttachmentReference, 4> colorRefs{};
    for (int i = 0; i < 4; ++i) colorRefs[i] = { static_cast<uint32_t>(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 4;
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments    = attachments.data();
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_GeometryPass));
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateLightingPass  (fullscreen quad → HDR color buffer)
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreateLightingPass(VkFormat hdrFmt)
{
    VkAttachmentDescription color{};
    color.format        = hdrFmt;
    color.samples       = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass    = 0;
    deps[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass    = 0;
    deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &color;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 2;
    ci.pDependencies   = deps;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_LightingPass));
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateForwardPass  (HDR color + depth, for opaque/transparent custom shaders)
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreateForwardPass(VkFormat hdrFmt, VkFormat depthFmt)
{
    std::array<VkAttachmentDescription, 2> attachments{};
    attachments[0].format        = hdrFmt;
    attachments[0].samples       = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;  // Keep lighting pass output
    attachments[0].storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[0].finalLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    attachments[1].format        = depthFmt;
    attachments[1].samples       = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp        = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].storeOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    attachments[1].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 2;
    ci.pAttachments    = attachments.data();
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_ForwardPass));
}

// ─────────────────────────────────────────────────────────────────────────────
// CreatePostProcessPass  (HDR → swapchain)
// ─────────────────────────────────────────────────────────────────────────────

void VulkanRenderPassManager::CreatePostProcessPass(VkFormat swapchainFmt)
{
    VkAttachmentDescription color{};
    color.format        = swapchainFmt;
    color.samples       = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &color;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
    VK_CHECK(vkCreateRenderPass(m_Ctx->GetDevice(), &ci, nullptr, &m_PostProcessPass));
}

// ─────────────────────────────────────────────────────────────────────────────
// Framebuffer creation
// ─────────────────────────────────────────────────────────────────────────────

static VkFramebuffer MakeFB(VkDevice dev, VkRenderPass rp,
                              const std::vector<VkImageView>& views,
                              uint32_t w, uint32_t h, uint32_t layers = 1)
{
    VkFramebufferCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass      = rp;
    ci.attachmentCount = static_cast<uint32_t>(views.size());
    ci.pAttachments    = views.data();
    ci.width           = w;
    ci.height          = h;
    ci.layers          = layers;
    VkFramebuffer fb;
    VK_CHECK(vkCreateFramebuffer(dev, &ci, nullptr, &fb));
    return fb;
}

void VulkanRenderPassManager::CreateDepthPrePassFB(VkImageView depthView, uint32_t w, uint32_t h)
{
    m_DepthPrePassFB = MakeFB(m_Ctx->GetDevice(), m_DepthPrePass, { depthView }, w, h);
}

void VulkanRenderPassManager::CreateGeometryPassFB(const GBufferAttachments& gb, uint32_t w, uint32_t h)
{
    m_GeometryPassFB = MakeFB(m_Ctx->GetDevice(), m_GeometryPass,
        { gb.packed0.GetView(), gb.packed1.GetView(),
          gb.packed2.GetView(), gb.packed3.GetView(),
          gb.depth.GetView()   }, w, h);
}

void VulkanRenderPassManager::CreateLightingPassFB(VkImageView hdrView, uint32_t w, uint32_t h)
{
    m_LightingPassFB = MakeFB(m_Ctx->GetDevice(), m_LightingPass, { hdrView }, w, h);
}

void VulkanRenderPassManager::CreateForwardPassFB(VkImageView hdrView,
                                                   VkImageView depthView,
                                                   uint32_t w, uint32_t h)
{
    m_ForwardPassFB = MakeFB(m_Ctx->GetDevice(), m_ForwardPass, { hdrView, depthView }, w, h);
}

void VulkanRenderPassManager::CreatePostProcessFBs(const VulkanSwapchain& sc)
{
    m_PostProcessFBs.resize(sc.GetImageCount());
    uint32_t w = sc.GetExtent().width, h = sc.GetExtent().height;
    for (uint32_t i = 0; i < sc.GetImageCount(); ++i)
        m_PostProcessFBs[i] = MakeFB(m_Ctx->GetDevice(), m_PostProcessPass,
                                      { sc.GetImageView(i) }, w, h);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPass helpers
// ─────────────────────────────────────────────────────────────────────────────

static void BeginPass(VkCommandBuffer cmd, VkRenderPass rp, VkFramebuffer fb,
                       VkExtent2D extent, const std::vector<VkClearValue>& clears)
{
    VkRenderPassBeginInfo bi{};
    bi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    bi.renderPass        = rp;
    bi.framebuffer       = fb;
    bi.renderArea.offset = {};
    bi.renderArea.extent = extent;
    bi.clearValueCount   = static_cast<uint32_t>(clears.size());
    bi.pClearValues      = clears.data();
    vkCmdBeginRenderPass(cmd, &bi, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderPassManager::BeginDepthPrePass(VkCommandBuffer cmd) const
{
    VkClearValue depth{};
    depth.depthStencil = { 1.0f, 0 };
    BeginPass(cmd, m_DepthPrePass, m_DepthPrePassFB, m_Extent, { depth });
}

void VulkanRenderPassManager::BeginGeometryPass(VkCommandBuffer cmd) const
{
    std::vector<VkClearValue> clears(5);
    for (int i = 0; i < 4; ++i) clears[i].color = { 0.f, 0.f, 0.f, 0.f };
    clears[4].depthStencil = { 1.0f, 0 };
    BeginPass(cmd, m_GeometryPass, m_GeometryPassFB, m_Extent, clears);
}

void VulkanRenderPassManager::BeginLightingPass(VkCommandBuffer cmd) const
{
    VkClearValue clear{};
    clear.color = { 0.f, 0.f, 0.f, 1.f };
    BeginPass(cmd, m_LightingPass, m_LightingPassFB, m_Extent, { clear });
}

void VulkanRenderPassManager::BeginForwardPass(VkCommandBuffer cmd) const
{
    BeginPass(cmd, m_ForwardPass, m_ForwardPassFB, m_Extent, {});
}

void VulkanRenderPassManager::BeginPostProcessPass(VkCommandBuffer cmd, uint32_t imageIndex) const
{
    VkClearValue clear{};
    clear.color = { 0.f, 0.f, 0.f, 1.f };
    BeginPass(cmd, m_PostProcessPass, m_PostProcessFBs[imageIndex], m_Extent, { clear });
}

void VulkanRenderPassManager::BeginShadowPass(VkCommandBuffer cmd,
                                               uint32_t layer, uint32_t shadowRes) const
{
    VkClearValue depth{};
    depth.depthStencil = { 1.0f, 0 };
    VkExtent2D ext{ shadowRes, shadowRes };
    BeginPass(cmd, m_ShadowPass, m_ShadowPassFBs[layer], ext, { depth });
}

VkFramebuffer VulkanRenderPassManager::GetShadowPassFB(uint32_t layer) const
{
    if (layer < m_ShadowPassFBs.size())
        return m_ShadowPassFBs[layer];
    return VK_NULL_HANDLE;
}

} // namespace Ermine::Vulkan
