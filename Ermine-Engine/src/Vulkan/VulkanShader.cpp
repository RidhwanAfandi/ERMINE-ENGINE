/* Start Header ************************************************************************/
/*!
\file       VulkanShader.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      SPIR-V shader module loading implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanShader.h"

#include <fstream>
#include <stdexcept>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// SPIR-V loading helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<uint32_t> LoadSPIRV(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("VulkanShader: Cannot open SPIR-V file: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize % 4 != 0)
        throw std::runtime_error("VulkanShader: SPIR-V file size not 4-byte aligned: " + path);

    std::vector<uint32_t> code(fileSize / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(fileSize));

    // Validate SPIR-V magic number
    static constexpr uint32_t SPIRV_MAGIC = 0x07230203;
    if (code.empty() || (code[0] != SPIRV_MAGIC && code[0] != 0x03022307))
        throw std::runtime_error("VulkanShader: Invalid SPIR-V magic in: " + path);

    return code;
}

static VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size() * sizeof(uint32_t);
    ci.pCode    = code.data();

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module));
    return module;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanShader
// ─────────────────────────────────────────────────────────────────────────────

void VulkanShader::Load(VkDevice device, const std::string& spirvPath,
                         VkShaderStageFlagBits stage)
{
    m_Device = device;
    m_Stage  = stage;
    m_Path   = spirvPath;

    auto code = LoadSPIRV(spirvPath);
    m_Module  = CreateShaderModule(device, code);
}

void VulkanShader::CreateFromBytes(VkDevice device, const std::vector<uint32_t>& spirv,
                                    VkShaderStageFlagBits stage)
{
    m_Device = device;
    m_Stage  = stage;
    m_Module = CreateShaderModule(device, spirv);
}

void VulkanShader::Destroy()
{
    if (m_Module)
    {
        vkDestroyShaderModule(m_Device, m_Module, nullptr);
        m_Module = VK_NULL_HANDLE;
    }
    m_Device = VK_NULL_HANDLE;
}

VkPipelineShaderStageCreateInfo VulkanShader::GetStageInfo() const
{
    VkPipelineShaderStageCreateInfo info{};
    info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage  = m_Stage;
    info.module = m_Module;
    info.pName  = "main";
    return info;
}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanShaderProgram
// ─────────────────────────────────────────────────────────────────────────────

void VulkanShaderProgram::LoadGraphics(VkDevice device, const std::string& baseName,
                                        const std::string& shaderDir)
{
    auto join = [&](const std::string& suffix) {
        return shaderDir + "/" + baseName + suffix;
    };

    m_Vertex.Load  (device, join("_vertex.glsl.spv"),   VK_SHADER_STAGE_VERTEX_BIT);
    m_Fragment.Load(device, join("_fragment.glsl.spv"), VK_SHADER_STAGE_FRAGMENT_BIT);
}

void VulkanShaderProgram::LoadCompute(VkDevice device, const std::string& baseName,
                                       const std::string& shaderDir)
{
    m_Compute.Load(device, shaderDir + "/" + baseName + "_compute.glsl.spv",
                   VK_SHADER_STAGE_COMPUTE_BIT);
}

void VulkanShaderProgram::Destroy()
{
    m_Vertex.Destroy();
    m_Fragment.Destroy();
    m_Compute.Destroy();
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanShaderProgram::GetStages() const
{
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    if (m_Vertex.IsValid())   stages.push_back(m_Vertex.GetStageInfo());
    if (m_Fragment.IsValid()) stages.push_back(m_Fragment.GetStageInfo());
    if (m_Compute.IsValid())  stages.push_back(m_Compute.GetStageInfo());
    return stages;
}

} // namespace Ermine::Vulkan
