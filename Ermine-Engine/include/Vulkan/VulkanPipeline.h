/* Start Header ************************************************************************/
/*!
\file       VulkanPipeline.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Graphics and compute pipeline state objects.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include "VulkanShader.h"
#include "VulkanDescriptorManager.h"
#include <vector>
#include <functional>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// PipelineBuilder  (fluent builder for graphics pipelines)
// ─────────────────────────────────────────────────────────────────────────────

class PipelineBuilder
{
public:
    // ── Defaults ──────────────────────────────────────────────────────────────
    PipelineBuilder();

    // ── Setters ───────────────────────────────────────────────────────────────
    PipelineBuilder& SetShaders      (const VulkanShaderProgram& program);
    PipelineBuilder& SetTopology     (VkPrimitiveTopology t);
    PipelineBuilder& SetDepthTest    (bool enable, bool write = true,
                                      VkCompareOp op = VK_COMPARE_OP_LESS);
    PipelineBuilder& SetBlending     (bool enable, uint32_t attachmentIndex = 0);
    PipelineBuilder& SetAdditiveBlend(uint32_t attachmentIndex = 0);
    PipelineBuilder& SetCullMode     (VkCullModeFlags mode,
                                      VkFrontFace front = VK_FRONT_FACE_COUNTER_CLOCKWISE);
    PipelineBuilder& SetPolygonMode  (VkPolygonMode mode);
    PipelineBuilder& SetMultisample  (VkSampleCountFlagBits samples);
    PipelineBuilder& SetColorBlendAttachmentCount(uint32_t n);
    PipelineBuilder& SetDynamicViewport(bool enable = true);
    PipelineBuilder& SetDynamicScissor (bool enable = true);

    /**
     * @brief Add a vertex binding + attribute descriptions.
     *        Called once for the mesh vertex layout.
     */
    PipelineBuilder& AddVertexBinding(uint32_t binding, uint32_t stride,
                                       VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX);
    PipelineBuilder& AddVertexAttribute(uint32_t location, uint32_t binding,
                                         VkFormat format, uint32_t offset);

    /**
     * @brief Build the pipeline layout (shared across similar pipelines).
     */
    PipelineBuilder& SetPipelineLayout(VkPipelineLayout layout);

    /**
     * @brief Build the VkPipeline for the given render pass and subpass.
     */
    VkPipeline Build(VkDevice device, VkRenderPass renderPass, uint32_t subpass = 0);

private:
    std::vector<VkPipelineShaderStageCreateInfo>       m_Stages;
    std::vector<VkVertexInputBindingDescription>        m_BindingDescs;
    std::vector<VkVertexInputAttributeDescription>      m_AttrDescs;
    std::vector<VkPipelineColorBlendAttachmentState>   m_BlendAttachments;
    std::vector<VkDynamicState>                         m_DynamicStates;

    VkPipelineInputAssemblyStateCreateInfo m_InputAssembly{};
    VkPipelineRasterizationStateCreateInfo m_Rasterization{};
    VkPipelineMultisampleStateCreateInfo   m_Multisample{};
    VkPipelineDepthStencilStateCreateInfo  m_DepthStencil{};
    VkPipelineLayout                       m_Layout = VK_NULL_HANDLE;
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanPipelineManager  (owns all named pipelines used by the renderer)
// ─────────────────────────────────────────────────────────────────────────────

class VulkanPipelineManager
{
public:
    VulkanPipelineManager() = default;
    ~VulkanPipelineManager() { Destroy(); }

    VulkanPipelineManager(const VulkanPipelineManager&)            = delete;
    VulkanPipelineManager& operator=(const VulkanPipelineManager&) = delete;

    /**
     * @brief Create all named pipelines and the shared pipeline layout.
     *
     * @param ctx          Active VulkanContext.
     * @param descMgr      Descriptor manager (for layout array).
     * @param renderPasses Render pass handles.
     * @param shaderDir    Directory containing compiled SPIR-V files.
     * @param gbufferPass  Geometry render pass.
     * @param lightingPass Lighting render pass.
     * @param forwardPass  Forward render pass.
     * @param ppPass       Post-process render pass.
     * @param shadowPass   Shadow render pass.
     * @param depthPass    Depth pre-pass.
     */
    void Init(VulkanContext& ctx,
              const VulkanDescriptorManager& descMgr,
              const std::string& shaderDir,
              VkRenderPass gbufferPass,
              VkRenderPass lightingPass,
              VkRenderPass forwardPass,
              VkRenderPass ppPass,
              VkRenderPass shadowPass,
              VkRenderPass depthPass);

    void Destroy();

    // ── Named pipelines ───────────────────────────────────────────────────────

    VkPipeline GetDepthPrePass()     const { return m_DepthPrePass;     }
    VkPipeline GetGBuffer()          const { return m_GBuffer;          }
    VkPipeline GetGBufferSkinned()   const { return m_GBufferSkinned;   }
    VkPipeline GetLighting()         const { return m_Lighting;         }
    VkPipeline GetShadowMap()        const { return m_ShadowMap;        }
    VkPipeline GetShadowMapSkinned() const { return m_ShadowMapSkinned; }
    VkPipeline GetForwardOpaque()    const { return m_ForwardOpaque;    }
    VkPipeline GetForwardTransp()    const { return m_ForwardTransp;    }
    VkPipeline GetPostProcess()      const { return m_PostProcess;      }
    VkPipeline GetFXAA()             const { return m_FXAA;             }
    VkPipeline GetBloom()            const { return m_Bloom;            }
    VkPipeline GetOutlineMask()      const { return m_OutlineMask;      }
    VkPipeline GetDebugLine()        const { return m_DebugLine;        }

    VkPipelineLayout GetLayout()     const { return m_PipelineLayout;   }

private:
    void CreatePipelineLayout(const VulkanDescriptorManager& descMgr);

    VulkanContext*   m_Ctx    = nullptr;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    // All pipeline handles
    VkPipeline m_DepthPrePass     = VK_NULL_HANDLE;
    VkPipeline m_GBuffer          = VK_NULL_HANDLE;
    VkPipeline m_GBufferSkinned   = VK_NULL_HANDLE;
    VkPipeline m_Lighting         = VK_NULL_HANDLE;
    VkPipeline m_ShadowMap        = VK_NULL_HANDLE;
    VkPipeline m_ShadowMapSkinned = VK_NULL_HANDLE;
    VkPipeline m_ForwardOpaque    = VK_NULL_HANDLE;
    VkPipeline m_ForwardTransp    = VK_NULL_HANDLE;
    VkPipeline m_PostProcess      = VK_NULL_HANDLE;
    VkPipeline m_FXAA             = VK_NULL_HANDLE;
    VkPipeline m_Bloom            = VK_NULL_HANDLE;
    VkPipeline m_OutlineMask      = VK_NULL_HANDLE;
    VkPipeline m_DebugLine        = VK_NULL_HANDLE;
};

} // namespace Ermine::Vulkan
