/* Start Header ************************************************************************/
/*!
\file       VulkanPipeline.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Graphics pipeline creation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanPipeline.h"
#include <array>
#include <stdexcept>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// PipelineBuilder defaults
// ─────────────────────────────────────────────────────────────────────────────

PipelineBuilder::PipelineBuilder()
{
    // Default input assembly: triangle list
    m_InputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    m_InputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    m_InputAssembly.primitiveRestartEnable = VK_FALSE;

    // Default rasterization
    m_Rasterization.sType                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    m_Rasterization.polygonMode            = VK_POLYGON_MODE_FILL;
    m_Rasterization.cullMode               = VK_CULL_MODE_BACK_BIT;
    m_Rasterization.frontFace              = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    m_Rasterization.lineWidth              = 1.0f;
    m_Rasterization.depthBiasEnable        = VK_FALSE;

    // Default multisample: 1x
    m_Multisample.sType                    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    m_Multisample.rasterizationSamples     = VK_SAMPLE_COUNT_1_BIT;
    m_Multisample.sampleShadingEnable      = VK_FALSE;

    // Default depth: test=true, write=true, LESS
    m_DepthStencil.sType                   = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    m_DepthStencil.depthTestEnable         = VK_TRUE;
    m_DepthStencil.depthWriteEnable        = VK_TRUE;
    m_DepthStencil.depthCompareOp          = VK_COMPARE_OP_LESS;
    m_DepthStencil.depthBoundsTestEnable   = VK_FALSE;
    m_DepthStencil.stencilTestEnable       = VK_FALSE;

    // Default blend attachment: off
    VkPipelineColorBlendAttachmentState blendOff{};
    blendOff.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendOff.blendEnable = VK_FALSE;
    m_BlendAttachments.push_back(blendOff);

    // Dynamic viewport + scissor by default
    m_DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
}

PipelineBuilder& PipelineBuilder::SetShaders(const VulkanShaderProgram& program)
{
    m_Stages = program.GetStages();
    return *this;
}

PipelineBuilder& PipelineBuilder::SetTopology(VkPrimitiveTopology t)
{
    m_InputAssembly.topology = t;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthTest(bool enable, bool write, VkCompareOp op)
{
    m_DepthStencil.depthTestEnable  = enable ? VK_TRUE : VK_FALSE;
    m_DepthStencil.depthWriteEnable = write  ? VK_TRUE : VK_FALSE;
    m_DepthStencil.depthCompareOp   = op;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetBlending(bool enable, uint32_t idx)
{
    if (idx >= m_BlendAttachments.size())
        m_BlendAttachments.resize(idx + 1, m_BlendAttachments[0]);

    auto& att = m_BlendAttachments[idx];
    if (enable)
    {
        att.blendEnable         = VK_TRUE;
        att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        att.colorBlendOp        = VK_BLEND_OP_ADD;
        att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        att.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    else
    {
        att.blendEnable = VK_FALSE;
    }
    return *this;
}

PipelineBuilder& PipelineBuilder::SetAdditiveBlend(uint32_t idx)
{
    if (idx >= m_BlendAttachments.size())
        m_BlendAttachments.resize(idx + 1, m_BlendAttachments[0]);

    auto& att = m_BlendAttachments[idx];
    att.blendEnable         = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.colorBlendOp        = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    att.alphaBlendOp        = VK_BLEND_OP_ADD;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetCullMode(VkCullModeFlags mode, VkFrontFace front)
{
    m_Rasterization.cullMode  = mode;
    m_Rasterization.frontFace = front;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    m_Rasterization.polygonMode = mode;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetColorBlendAttachmentCount(uint32_t n)
{
    VkPipelineColorBlendAttachmentState blendOff{};
    blendOff.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendOff.blendEnable = VK_FALSE;
    m_BlendAttachments.assign(n, blendOff);
    return *this;
}

PipelineBuilder& PipelineBuilder::AddVertexBinding(uint32_t binding, uint32_t stride,
                                                     VkVertexInputRate rate)
{
    m_BindingDescs.push_back({ binding, stride, rate });
    return *this;
}

PipelineBuilder& PipelineBuilder::AddVertexAttribute(uint32_t location, uint32_t binding,
                                                       VkFormat format, uint32_t offset)
{
    m_AttrDescs.push_back({ location, binding, format, offset });
    return *this;
}

PipelineBuilder& PipelineBuilder::SetPipelineLayout(VkPipelineLayout layout)
{
    m_Layout = layout;
    return *this;
}

VkPipeline PipelineBuilder::Build(VkDevice device, VkRenderPass renderPass, uint32_t subpass)
{
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = static_cast<uint32_t>(m_BindingDescs.size());
    vertexInput.pVertexBindingDescriptions      = m_BindingDescs.empty() ? nullptr : m_BindingDescs.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_AttrDescs.size());
    vertexInput.pVertexAttributeDescriptions    = m_AttrDescs.empty() ? nullptr : m_AttrDescs.data();

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = static_cast<uint32_t>(m_BlendAttachments.size());
    colorBlend.pAttachments    = m_BlendAttachments.data();

    VkPipelineDynamicStateCreateInfo dynState{};
    dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynState.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
    dynState.pDynamicStates    = m_DynamicStates.empty() ? nullptr : m_DynamicStates.data();

    // Viewport and scissor (dynamic; actual values set at draw time)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount          = static_cast<uint32_t>(m_Stages.size());
    ci.pStages             = m_Stages.data();
    ci.pVertexInputState   = &vertexInput;
    ci.pInputAssemblyState = &m_InputAssembly;
    ci.pViewportState      = &viewportState;
    ci.pRasterizationState = &m_Rasterization;
    ci.pMultisampleState   = &m_Multisample;
    ci.pDepthStencilState  = &m_DepthStencil;
    ci.pColorBlendState    = &colorBlend;
    ci.pDynamicState       = m_DynamicStates.empty() ? nullptr : &dynState;
    ci.layout              = m_Layout;
    ci.renderPass          = renderPass;
    ci.subpass             = subpass;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
    return pipeline;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanPipelineManager
// ─────────────────────────────────────────────────────────────────────────────

// Mesh vertex layout (mirrors OpenGL VAO layout)
struct MeshVertex {
    float position[3];
    float normal  [3];
    float texcoord[2];
    float tangent [4]; // xyz + w (bitangent sign)
};

// Skinned mesh vertex (with bone weights)
struct SkinnedVertex {
    float    position [3];
    float    normal   [3];
    float    texcoord [2];
    float    tangent  [4];
    uint32_t boneIds  [4];
    float    boneWeights[4];
};

void VulkanPipelineManager::CreatePipelineLayout(const VulkanDescriptorManager& descMgr)
{
    auto layouts = descMgr.GetAllLayouts();

    // Push constants: PushConstants struct (see VulkanTypes.h)
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset     = 0;
    pushRange.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount         = static_cast<uint32_t>(layouts.size());
    ci.pSetLayouts            = layouts.data();
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(m_Ctx->GetDevice(), &ci, nullptr, &m_PipelineLayout));
}

void VulkanPipelineManager::Init(
    VulkanContext& ctx,
    const VulkanDescriptorManager& descMgr,
    const std::string& shaderDir,
    VkRenderPass gbufferPass,
    VkRenderPass lightingPass,
    VkRenderPass forwardPass,
    VkRenderPass ppPass,
    VkRenderPass shadowPass,
    VkRenderPass depthPass)
{
    m_Ctx = &ctx;
    CreatePipelineLayout(descMgr);

    VkDevice dev = ctx.GetDevice();

    // ── Depth Pre-Pass ────────────────────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "depth_prepass", shaderDir);

        m_DepthPrePass = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0)
            .SetDepthTest(true, true)
            .SetColorBlendAttachmentCount(0)
            .Build(dev, depthPass);
        sh.Destroy();
    }

    // ── G-Buffer (opaque, deferred) ───────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "gBuffer", shaderDir);

        m_GBuffer = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT,  offsetof(MeshVertex, position))
            .AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT,  offsetof(MeshVertex, normal))
            .AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT,     offsetof(MeshVertex, texcoord))
            .AddVertexAttribute(3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(MeshVertex, tangent))
            .SetDepthTest(true, false, VK_COMPARE_OP_EQUAL) // Read pre-pass depth
            .SetColorBlendAttachmentCount(4) // 4 MRTs
            .Build(dev, gbufferPass);
        sh.Destroy();
    }

    // ── Lighting (fullscreen deferred) ────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "lighting", shaderDir);

        m_Lighting = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .SetDepthTest(false, false)
            .SetCullMode(VK_CULL_MODE_NONE)
            .Build(dev, lightingPass);
        sh.Destroy();
    }

    // ── Shadow Map ────────────────────────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "shadowmap_instanced", shaderDir);

        m_ShadowMap = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position))
            .SetDepthTest(true, true)
            .SetColorBlendAttachmentCount(0)
            .SetCullMode(VK_CULL_MODE_FRONT_BIT) // Peter panning reduction
            .Build(dev, shadowPass);
        sh.Destroy();
    }

    // ── Forward Opaque (custom shaders) ──────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "forward_opaque", shaderDir);

        m_ForwardOpaque = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT,   offsetof(MeshVertex, position))
            .AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT,   offsetof(MeshVertex, normal))
            .AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT,      offsetof(MeshVertex, texcoord))
            .AddVertexAttribute(3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(MeshVertex, tangent))
            .SetDepthTest(true, true)
            .Build(dev, forwardPass);
        sh.Destroy();
    }

    // ── Forward Transparent ───────────────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "forward_transparent", shaderDir);

        m_ForwardTransp = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT,   offsetof(MeshVertex, position))
            .AddVertexAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT,   offsetof(MeshVertex, normal))
            .AddVertexAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT,      offsetof(MeshVertex, texcoord))
            .SetDepthTest(true, false) // No depth write (transparent)
            .SetBlending(true)
            .SetCullMode(VK_CULL_MODE_NONE)
            .Build(dev, forwardPass);
        sh.Destroy();
    }

    // ── Post-Process (bloom + FXAA + tone map) ────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "postprocess", shaderDir);

        m_PostProcess = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .SetDepthTest(false, false)
            .SetCullMode(VK_CULL_MODE_NONE)
            .Build(dev, ppPass);
        sh.Destroy();
    }

    // ── Debug Line ────────────────────────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "debug_line", shaderDir);

        m_DebugLine = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .SetTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .SetDepthTest(true, false)
            .Build(dev, forwardPass);
        sh.Destroy();
    }

    // ── Outline Mask ──────────────────────────────────────────────────────────
    {
        VulkanShaderProgram sh;
        sh.LoadGraphics(dev, "outline_mask_indirect", shaderDir);

        m_OutlineMask = PipelineBuilder{}
            .SetShaders(sh)
            .SetPipelineLayout(m_PipelineLayout)
            .AddVertexBinding(0, sizeof(MeshVertex))
            .AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position))
            .SetDepthTest(true, true)
            .SetColorBlendAttachmentCount(0)
            .Build(dev, depthPass);
        sh.Destroy();
    }
}

void VulkanPipelineManager::Destroy()
{
    if (!m_Ctx) return;
    VkDevice dev = m_Ctx->GetDevice();

    auto destroy = [&](VkPipeline& p) {
        if (p) { vkDestroyPipeline(dev, p, nullptr); p = VK_NULL_HANDLE; }
    };
    destroy(m_DepthPrePass);
    destroy(m_GBuffer);
    destroy(m_GBufferSkinned);
    destroy(m_Lighting);
    destroy(m_ShadowMap);
    destroy(m_ShadowMapSkinned);
    destroy(m_ForwardOpaque);
    destroy(m_ForwardTransp);
    destroy(m_PostProcess);
    destroy(m_FXAA);
    destroy(m_Bloom);
    destroy(m_OutlineMask);
    destroy(m_DebugLine);

    if (m_PipelineLayout) { vkDestroyPipelineLayout(dev, m_PipelineLayout, nullptr); m_PipelineLayout = VK_NULL_HANDLE; }
    m_Ctx = nullptr;
}

} // namespace Ermine::Vulkan
