/* Start Header ************************************************************************/
/*!
\file       VulkanDescriptorManager.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Descriptor manager implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanDescriptorManager.h"
#include <array>

namespace Ermine::Vulkan
{

void VulkanDescriptorManager::Init(VulkanContext& ctx, uint32_t bindlessCount)
{
    m_Ctx          = &ctx;
    m_BindlessCount = bindlessCount;

    CreateLayouts(bindlessCount);
    // Pool needs enough sets for: FRAMES_IN_FLIGHT per-frame + 1 gbuffer-read + 1 material + 1 global
    CreatePool(FRAMES_IN_FLIGHT + 3 + 64);
    AllocateSets();
}

void VulkanDescriptorManager::Destroy()
{
    if (!m_Ctx) return;
    VkDevice dev = m_Ctx->GetDevice();

    if (m_Pool)           { vkDestroyDescriptorPool      (dev, m_Pool,           nullptr); m_Pool           = VK_NULL_HANDLE; }
    if (m_PerFrameLayout) { vkDestroyDescriptorSetLayout (dev, m_PerFrameLayout, nullptr); m_PerFrameLayout = VK_NULL_HANDLE; }
    if (m_PerPassLayout)  { vkDestroyDescriptorSetLayout (dev, m_PerPassLayout,  nullptr); m_PerPassLayout  = VK_NULL_HANDLE; }
    if (m_PerDrawLayout)  { vkDestroyDescriptorSetLayout (dev, m_PerDrawLayout,  nullptr); m_PerDrawLayout  = VK_NULL_HANDLE; }
    if (m_GlobalLayout)   { vkDestroyDescriptorSetLayout (dev, m_GlobalLayout,   nullptr); m_GlobalLayout   = VK_NULL_HANDLE; }
    m_Ctx = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateLayouts
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDescriptorManager::CreateLayouts(uint32_t bindlessCount)
{
    VkDevice dev = m_Ctx->GetDevice();

    // ── Set 0: Per-frame ──────────────────────────────────────────────────────
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        // Camera UBO
        bindings[0].binding         = Bindings::CAMERA_UBO;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        // Lights UBO
        bindings[1].binding         = Bindings::LIGHTS_UBO;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Probes UBO
        bindings[2].binding         = Bindings::PROBES_UBO;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Shadow views SSBO
        bindings[3].binding         = Bindings::SHADOW_VIEWS_SSBO;
        bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &ci, nullptr, &m_PerFrameLayout));
    }

    // ── Set 1: Per-pass (G-buffer read) ──────────────────────────────────────
    {
        std::array<VkDescriptorSetLayoutBinding, 7> bindings{};
        auto sampled = [](uint32_t binding) {
            VkDescriptorSetLayoutBinding b{};
            b.binding         = binding;
            b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.descriptorCount = 1;
            b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
            return b;
        };
        bindings[0] = sampled(Bindings::GBUFFER_POS);
        bindings[1] = sampled(Bindings::GBUFFER_NORMAL);
        bindings[2] = sampled(Bindings::GBUFFER_MATERIAL);
        bindings[3] = sampled(Bindings::GBUFFER_EXTRA);
        bindings[4] = sampled(Bindings::GBUFFER_DEPTH);
        bindings[5] = sampled(Bindings::SHADOW_MAP_ARRAY);
        // Postprocess input (binding 6)
        bindings[6] = sampled(6);

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &ci, nullptr, &m_PerPassLayout));
    }

    // ── Set 2: Per-draw ───────────────────────────────────────────────────────
    {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        // Material SSBO
        bindings[0].binding         = Bindings::MATERIAL_SSBO;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Bindless texture array
        bindings[1].binding         = Bindings::TEXTURE_SSBO;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = bindlessCount;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Skeletal SSBO
        bindings[2].binding         = Bindings::SKELETAL_SSBO;
        bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        // Descriptor indexing flags for bindless array
        std::array<VkDescriptorBindingFlags, 3> bindingFlags{};
        bindingFlags[1] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                          VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsCI{};
        flagsCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsCI.bindingCount  = static_cast<uint32_t>(bindingFlags.size());
        flagsCI.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.pNext        = m_Ctx->GetCapabilities().GetFeatures().descriptorIndexing ? &flagsCI : nullptr;
        ci.flags        = m_Ctx->GetCapabilities().GetFeatures().descriptorIndexing
                              ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &ci, nullptr, &m_PerDrawLayout));
    }

    // ── Set 3: Global indirect ────────────────────────────────────────────────
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        auto storage = [](uint32_t binding, VkShaderStageFlags stages) {
            VkDescriptorSetLayoutBinding b{};
            b.binding         = binding;
            b.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            b.descriptorCount = 1;
            b.stageFlags      = stages;
            return b;
        };
        bindings[0] = storage(Bindings::DRAW_COMMANDS, VK_SHADER_STAGE_VERTEX_BIT);
        bindings[1] = storage(Bindings::DRAW_INFO,     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        bindings[2] = storage(Bindings::VERTEX_SSBO,   VK_SHADER_STAGE_VERTEX_BIT);
        bindings[3] = storage(Bindings::INDEX_SSBO,    VK_SHADER_STAGE_VERTEX_BIT);

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.data();
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &ci, nullptr, &m_GlobalLayout));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CreatePool
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDescriptorManager::CreatePool(uint32_t maxSets)
{
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         maxSets * 4 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         maxSets * 8 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * (m_BindlessCount + 8) };
    poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          maxSets };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    ci.maxSets       = maxSets;
    ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    ci.pPoolSizes    = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(m_Ctx->GetDevice(), &ci, nullptr, &m_Pool));
}

// ─────────────────────────────────────────────────────────────────────────────
// AllocateSets
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDescriptorManager::AllocateSets()
{
    VkDevice dev = m_Ctx->GetDevice();

    // Per-frame sets (one per frame-in-flight)
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = m_Pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &m_PerFrameLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &m_PerFrameSets[i]));
    }

    // G-buffer read set
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = m_Pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &m_PerPassLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &m_GBufferReadSet));
    }

    // Material set (with variable descriptor count for bindless)
    {
        VkDescriptorSetVariableDescriptorCountAllocateInfo varCount{};
        varCount.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        varCount.descriptorSetCount = 1;
        varCount.pDescriptorCounts  = &m_BindlessCount;

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.pNext              = m_Ctx->GetCapabilities().GetFeatures().descriptorIndexing ? &varCount : nullptr;
        ai.descriptorPool     = m_Pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &m_PerDrawLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &m_MaterialSet));
    }

    // Global set
    {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = m_Pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &m_GlobalLayout;
        VK_CHECK(vkAllocateDescriptorSets(dev, &ai, &m_GlobalSet));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Write helpers
// ─────────────────────────────────────────────────────────────────────────────

void VulkanDescriptorManager::WritePerFrameSet(
    uint32_t frameIndex,
    VkBuffer cameraUBO,   VkDeviceSize cameraRange,
    VkBuffer lightsUBO,   VkDeviceSize lightsRange,
    VkBuffer probesUBO,   VkDeviceSize probesRange,
    VkBuffer shadowViewSSBO, VkDeviceSize shadowViewRange)
{
    VkDescriptorBufferInfo camBuf   = { cameraUBO,       0, cameraRange       };
    VkDescriptorBufferInfo lightBuf = { lightsUBO,       0, lightsRange       };
    VkDescriptorBufferInfo probeBuf = { probesUBO,       0, probesRange       };
    VkDescriptorBufferInfo shadowBuf= { shadowViewSSBO,  0, shadowViewRange   };

    std::array<VkWriteDescriptorSet, 4> writes{};
    auto makeWrite = [&](uint32_t binding, VkDescriptorType type,
                         const VkDescriptorBufferInfo* bufInfo) {
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = m_PerFrameSets[frameIndex];
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = type;
        w.pBufferInfo     = bufInfo;
        return w;
    };

    writes[0] = makeWrite(Bindings::CAMERA_UBO,        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &camBuf);
    writes[1] = makeWrite(Bindings::LIGHTS_UBO,        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightBuf);
    writes[2] = makeWrite(Bindings::PROBES_UBO,        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &probeBuf);
    writes[3] = makeWrite(Bindings::SHADOW_VIEWS_SSBO, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &shadowBuf);

    vkUpdateDescriptorSets(m_Ctx->GetDevice(),
                            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanDescriptorManager::WriteGBufferReadSet(
    VkImageView pos, VkImageView normal, VkImageView material,
    VkImageView extra, VkImageView depth,
    VkSampler sampler,
    VkImageView shadowMapArray, VkSampler shadowSampler)
{
    auto makeImgInfo = [](VkImageView view, VkSampler samp, VkImageLayout layout) {
        return VkDescriptorImageInfo{ samp, view, layout };
    };

    VkDescriptorImageInfo posInfo    = makeImgInfo(pos,    sampler,       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo normInfo   = makeImgInfo(normal, sampler,       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo matInfo    = makeImgInfo(material, sampler,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo extraInfo  = makeImgInfo(extra,  sampler,       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo depthInfo  = makeImgInfo(depth,  sampler,       VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    VkDescriptorImageInfo shadowInfo = makeImgInfo(shadowMapArray, shadowSampler, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    std::array<VkWriteDescriptorSet, 6> writes{};
    auto makeWrite = [&](uint32_t binding, const VkDescriptorImageInfo* info) {
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = m_GBufferReadSet;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = info;
        return w;
    };
    writes[0] = makeWrite(Bindings::GBUFFER_POS,      &posInfo);
    writes[1] = makeWrite(Bindings::GBUFFER_NORMAL,   &normInfo);
    writes[2] = makeWrite(Bindings::GBUFFER_MATERIAL, &matInfo);
    writes[3] = makeWrite(Bindings::GBUFFER_EXTRA,    &extraInfo);
    writes[4] = makeWrite(Bindings::GBUFFER_DEPTH,    &depthInfo);
    writes[5] = makeWrite(Bindings::SHADOW_MAP_ARRAY, &shadowInfo);

    vkUpdateDescriptorSets(m_Ctx->GetDevice(),
                            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanDescriptorManager::WriteMaterialSet(
    VkBuffer materialSSBO, VkDeviceSize materialRange,
    VkBuffer skeletalSSBO, VkDeviceSize skeletalRange)
{
    VkDescriptorBufferInfo matBuf = { materialSSBO, 0, materialRange };
    VkDescriptorBufferInfo skeBuf = { skeletalSSBO, 0, skeletalRange };

    std::array<VkWriteDescriptorSet, 2> writes{};

    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_MaterialSet;
    writes[0].dstBinding      = Bindings::MATERIAL_SSBO;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo     = &matBuf;

    writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet          = m_MaterialSet;
    writes[1].dstBinding      = Bindings::SKELETAL_SSBO;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo     = &skeBuf;

    vkUpdateDescriptorSets(m_Ctx->GetDevice(),
                            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanDescriptorManager::UpdateBindlessTexture(uint32_t index,
                                                     VkImageView view, VkSampler sampler)
{
    VkDescriptorImageInfo imgInfo{ sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = m_MaterialSet;
    w.dstBinding      = Bindings::TEXTURE_SSBO;
    w.dstArrayElement = index;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo      = &imgInfo;

    vkUpdateDescriptorSets(m_Ctx->GetDevice(), 1, &w, 0, nullptr);
}

void VulkanDescriptorManager::WriteGlobalSet(
    VkBuffer drawCommandSSBO, VkDeviceSize drawCmdRange,
    VkBuffer drawInfoSSBO,    VkDeviceSize drawInfoRange,
    VkBuffer vertexSSBO,      VkDeviceSize vertexRange,
    VkBuffer indexSSBO,       VkDeviceSize indexRange)
{
    VkDescriptorBufferInfo cmdBuf  = { drawCommandSSBO, 0, drawCmdRange  };
    VkDescriptorBufferInfo infoBuf = { drawInfoSSBO,    0, drawInfoRange };
    VkDescriptorBufferInfo vtxBuf  = { vertexSSBO,      0, vertexRange   };
    VkDescriptorBufferInfo idxBuf  = { indexSSBO,       0, indexRange    };

    std::array<VkWriteDescriptorSet, 4> writes{};
    auto makeWrite = [&](uint32_t binding, const VkDescriptorBufferInfo* info) {
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = m_GlobalSet;
        w.dstBinding      = binding;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo     = info;
        return w;
    };
    writes[0] = makeWrite(Bindings::DRAW_COMMANDS, &cmdBuf);
    writes[1] = makeWrite(Bindings::DRAW_INFO,     &infoBuf);
    writes[2] = makeWrite(Bindings::VERTEX_SSBO,   &vtxBuf);
    writes[3] = makeWrite(Bindings::INDEX_SSBO,    &idxBuf);

    vkUpdateDescriptorSets(m_Ctx->GetDevice(),
                            static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanDescriptorManager::UpdateCameraUBO(uint32_t frameIndex,
                                               VkBuffer buffer,
                                               VkDeviceSize offset,
                                               VkDeviceSize range)
{
    VkDescriptorBufferInfo bufInfo{ buffer, offset, range };
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = m_PerFrameSets[frameIndex];
    w.dstBinding      = Bindings::CAMERA_UBO;
    w.descriptorCount = 1;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.pBufferInfo     = &bufInfo;
    vkUpdateDescriptorSets(m_Ctx->GetDevice(), 1, &w, 0, nullptr);
}

} // namespace Ermine::Vulkan
