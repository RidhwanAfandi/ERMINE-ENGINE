/* Start Header ************************************************************************/
/*!
\file       VulkanShader.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      SPIR-V shader module loading and pipeline shader stage creation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include <string>
#include <vector>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanShader  (single SPIR-V module)
// ─────────────────────────────────────────────────────────────────────────────

class VulkanShader
{
public:
    VulkanShader() = default;
    ~VulkanShader() { Destroy(); }

    VulkanShader(const VulkanShader&)            = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    /**
     * @brief Load a SPIR-V .spv file from disk and create the shader module.
     * @param device    Logical device.
     * @param spirvPath Path to the .spv file (from Resources/Shaders/Vulkan/SPIRV/).
     * @param stage     Pipeline stage (VERTEX, FRAGMENT, COMPUTE, etc.)
     */
    void Load(VkDevice device, const std::string& spirvPath, VkShaderStageFlagBits stage);

    /**
     * @brief Create from raw SPIR-V bytes (for embedded/hot-compiled shaders).
     */
    void CreateFromBytes(VkDevice device, const std::vector<uint32_t>& spirv,
                         VkShaderStageFlagBits stage);

    void Destroy();

    /**
     * @brief Get the VkPipelineShaderStageCreateInfo for use in pipeline creation.
     *        Entry point is always "main".
     */
    VkPipelineShaderStageCreateInfo GetStageInfo() const;

    VkShaderModule       GetModule() const { return m_Module; }
    VkShaderStageFlagBits GetStage() const { return m_Stage;  }
    bool IsValid() const { return m_Module != VK_NULL_HANDLE; }

    const std::string& GetPath() const { return m_Path; }

private:
    VkDevice              m_Device = VK_NULL_HANDLE;
    VkShaderModule        m_Module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_Stage  = VK_SHADER_STAGE_VERTEX_BIT;
    std::string           m_Path;
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanShaderProgram  (vertex + fragment pair, or compute)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Convenience wrapper pairing a vertex and fragment shader (or compute only).
 *
 *        Loads from the SPIR-V directory at a standard naming convention:
 *        Resources/Shaders/Vulkan/SPIRV/<name>_vertex_vertex.glsl.spv
 *        Resources/Shaders/Vulkan/SPIRV/<name>_fragment_fragment.glsl.spv
 */
class VulkanShaderProgram
{
public:
    /**
     * @brief Load vertex + fragment shaders by base name.
     * @param device    Logical device.
     * @param baseName  E.g. "gbuffer" loads "gbuffer_vertex.glsl.spv" + "gbuffer_fragment.glsl.spv"
     * @param shaderDir Directory containing SPIR-V files.
     */
    void LoadGraphics(VkDevice device, const std::string& baseName,
                      const std::string& shaderDir);

    /**
     * @brief Load a compute shader.
     */
    void LoadCompute(VkDevice device, const std::string& baseName,
                     const std::string& shaderDir);

    void Destroy();

    /**
     * @brief Get all stage create infos for vkCreateGraphicsPipeline.
     */
    std::vector<VkPipelineShaderStageCreateInfo> GetStages() const;

    bool IsGraphics() const { return m_Vertex.IsValid() && m_Fragment.IsValid(); }
    bool IsCompute()  const { return m_Compute.IsValid(); }

    const VulkanShader& GetVertex()   const { return m_Vertex;   }
    const VulkanShader& GetFragment() const { return m_Fragment; }
    const VulkanShader& GetCompute()  const { return m_Compute;  }

private:
    VulkanShader m_Vertex;
    VulkanShader m_Fragment;
    VulkanShader m_Compute;
};

} // namespace Ermine::Vulkan
