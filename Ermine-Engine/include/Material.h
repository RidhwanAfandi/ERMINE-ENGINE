/* Start Header ************************************************************************/
/*!
\file       Material.h
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\co-author  Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\date       Sep 9, 2025
\brief      Material system for graphics rendering with SSBO support

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include "PreCompile.h"
#include "Shader.h"
#include "Texture.h"
#include "Cubemap.h"
#include "MathVector.h"
#include <algorithm>
#include <cmath>

namespace Ermine::graphics
{
    /*!***********************************************************************
    \brief
        Material texture flag bits for packed bitfield
    *************************************************************************/
    enum MaterialTextureFlags : uint32_t
    {
        MAT_FLAG_ALBEDO_MAP    = 1 << 0,  // bit 0: hasAlbedoMap
        MAT_FLAG_NORMAL_MAP    = 1 << 1,  // bit 1: hasNormalMap
        MAT_FLAG_ROUGHNESS_MAP = 1 << 2,  // bit 2: hasRoughnessMap
        MAT_FLAG_METALLIC_MAP  = 1 << 3,  // bit 3: hasMetallicMap
        MAT_FLAG_AO_MAP        = 1 << 4,  // bit 4: hasAoMap
        MAT_FLAG_EMISSIVE_MAP  = 1 << 5   // bit 5: hasEmissiveMap
    };

    /*!***********************************************************************
    \brief
        Material parameter types for type safety
    *************************************************************************/
    enum class MaterialParamType
    {
        FLOAT,
        VEC2,
        VEC3,
        VEC4,
        INT,
        BOOL,
        TEXTURE_2D
    };

    /*!***********************************************************************
    \brief
        Material parameter wrapper
    *************************************************************************/
    struct MaterialParam
    {
        MaterialParamType type;
        std::vector<float> floatValues;
        int intValue = 0;
        bool boolValue = false;
        std::shared_ptr<Texture> texture = nullptr;
        std::shared_ptr<Cubemap> cubemap = nullptr;

        // Default constructor
        MaterialParam() : type(MaterialParamType::FLOAT), intValue(0), boolValue(false) {}

        // Constructors for different types
        MaterialParam(float value) : type(MaterialParamType::FLOAT), floatValues{ value } {}
        MaterialParam(const Vec2& value) : type(MaterialParamType::VEC2), floatValues{ value.x, value.y } {}
        MaterialParam(const Vec3& value) : type(MaterialParamType::VEC3), floatValues{ value.x, value.y, value.z } {}
        MaterialParam(const Vec4& value) : type(MaterialParamType::VEC4), floatValues{ value.x, value.y, value.z, value.w } {}
        MaterialParam(int value) : type(MaterialParamType::INT), intValue(value) {}
        MaterialParam(bool value) : type(MaterialParamType::BOOL), boolValue(value) {}
        MaterialParam(std::shared_ptr<Texture> tex) : type(MaterialParamType::TEXTURE_2D), texture(std::move(tex)) {}
    };

    /*!***********************************************************************
    \brief
        GPU-compatible material structure for SSBO
        Uses std430 layout rules (no padding required)
    *************************************************************************/
    struct MaterialSSBO
    {
        Vec4 albedo{ 0.8f, 0.8f, 0.8f, 1.0f };  // 16 bytes (0-15)

        float metallic{ 0.0f };                  // 4 bytes (16-19)
        float roughness{ 0.5f };                 // 4 bytes (20-23)
        float ao{ 1.0f };                        // 4 bytes (24-27)
        float normalStrength{ 1.0f };            // 4 bytes (28-31)

        Vec3 emissive{ 0.0f, 0.0f, 0.0f };      // 12 bytes (32-43)
        float emissiveIntensity{ 0.0f };         // 4 bytes (44-47)

        int shadingModel{ 0 };                   // 4 bytes (48-51)
        uint32_t textureFlags{ 0 };              // 4 bytes (52-55) - Packed bitfield for all texture flags
        int castsShadows{ 1 };                   // 4 bytes (56-59) - Whether this material casts shadows (1=true, 0=false)
        float fillAmount{ 1.0f };                // 4 bytes (60-63) - Mesh fill amount [0,1]

        // UV Scale and Offset
        Vec2 uvScale{ 1.0f, 1.0f };             // 8 bytes (64-71)
        Vec2 uvOffset{ 0.0f, 0.0f };            // 8 bytes (72-79)

        // Texture Array Indices (indices into bindless texture array)
        int albedoMapIndex{ -1 };               // 4 bytes (80-83)
        int normalMapIndex{ -1 };               // 4 bytes (84-87)
        int roughnessMapIndex{ -1 };            // 4 bytes (88-91)
        int metallicMapIndex{ -1 };             // 4 bytes (92-95)

        int aoMapIndex{ -1 };                   // 4 bytes (96-99)
        int emissiveMapIndex{ -1 };             // 4 bytes (100-103)
        float fillDirOctX{ 0.0f };              // 4 bytes (104-107) - Normalized UV fill axis X
        float fillDirOctY{ 1.0f };              // 4 bytes (108-111) - Normalized UV fill axis Y
        // Total: 112 bytes (down from 128 bytes) - 12.5% reduction
    };
    static_assert(sizeof(MaterialSSBO) == 112, "MaterialSSBO must remain 112 bytes");

    // Forward declaration
    class Material;

    // Predefined material templates
    class MaterialTemplates
    {
    public:
		// Returns a parameter map for a red PBR material.
        static std::map<std::string, MaterialParam> PBR_RED()
        {
            return {
                {"materialAlbedo", Vec4(1.0f, 0.0f, 0.0f, 1.0f)},
                {"materialMetallic", 0.0f},
                {"materialRoughness", 0.3f},
                {"materialAo", 1.0f},
                {"materialEmissive", Vec3(0.0f, 0.0f, 0.0f)},
                {"materialEmissiveIntensity", 0.0f},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }
        // Returns a parameter map for a metallic PBR material.
        static std::map<std::string, MaterialParam> PBR_METAL()
        {
            return {
                {"materialAlbedo", Vec4(0.7f, 0.7f, 0.8f, 1.0f)},
                {"materialMetallic", 1.0f},  // Full metallic
                {"materialRoughness", 0.15f}, // Slightly rough for visible reflections
                {"materialAo", 1.0f},
                {"materialEmissive", Vec3(0.0f, 0.0f, 0.0f)},
                {"materialEmissiveIntensity", 0.0f},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }

         // Returns a parameter map for a white PBR material.
        static std::map<std::string, MaterialParam> PBR_WHITE()
        {
            return {
                {"materialAlbedo", Vec4(0.8f, 0.8f, 0.8f, 1.0f)},
                {"materialMetallic", 0.0f},
                {"materialRoughness", 0.3f},
                {"materialAo", 1.0f},
                {"materialEmissive", Vec3(0.0f, 0.0f, 0.0f)},
                {"materialEmissiveIntensity", 0.0f},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }
		// Emissive material
        static std::map<std::string, MaterialParam> EMISSIVE(const Vec3& color, float intensity)
        {
            return {
                {"materialAlbedo", Vec4(0.0f, 0.0f, 0.0f, 1.0f)},
                {"materialMetallic", 0.0f},
                {"materialRoughness", 1.0f},
                {"materialAo", 1.0f},
                {"materialEmissive", color},
                {"materialEmissiveIntensity", intensity},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }

        // Glass material (transparent)
        static std::map<std::string, MaterialParam> PBR_GLASS(float transparency = 0.9f)
        {
            return {
                {"materialAlbedo", Vec4(0.95f, 0.95f, 0.95f, 1.0f - transparency)},
                {"materialMetallic", 0.0f},
                {"materialRoughness", 0.05f},
                {"materialAo", 1.0f},
                {"materialEmissive", Vec3(0.0f, 0.0f, 0.0f)},
                {"materialEmissiveIntensity", 0.0f},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }

        // Water material (transparent)
        static std::map<std::string, MaterialParam> PBR_WATER(float transparency = 0.7f)
        {
            return {
                {"materialAlbedo", Vec4(0.1f, 0.3f, 0.6f, 1.0f - transparency)},
                {"materialMetallic", 0.0f},
                {"materialRoughness", 0.1f},
                {"materialAo", 1.0f},
                {"materialEmissive", Vec3(0.0f, 0.0f, 0.0f)},
                {"materialEmissiveIntensity", 0.0f},
                {"materialNormalStrength", 1.0f},
                {"materialShadingModel", 0},
                {"materialHasAlbedoMap", false},
                {"materialHasNormalMap", false},
                {"materialHasRoughnessMap", false},
                {"materialHasMetallicMap", false},
                {"materialHasAoMap", false},
                {"materialHasEmissiveMap", false},
                {"materialCastsShadows", true},
                {"materialFillAmount", 1.0f},
                {"materialFillDirection", Vec3(0.0f, 1.0f, 0.0f)},
                {"materialFillUVAxis", Vec2(0.0f, 1.0f)}
            };
        }

    };

    /*!***********************************************************************
    \brief
        Main Material class with SSBO support
    *************************************************************************/
    class Material
    {
    private:
        static float Clamp01(const float value)
        {
            return std::max(0.0f, std::min(1.0f, value));
        }

        std::map<std::string, MaterialParam> m_parameters;
        std::shared_ptr<Shader> m_shader;

        // Texture slots management
        std::map<std::string, int> m_textureSlots;
        int m_nextTextureSlot = 0;
        std::unordered_map<std::string, std::shared_ptr<Cubemap>> cubemaps;

        // SSBO management
        mutable MaterialSSBO m_materialData;
        mutable bool m_ssboDirty = true;

        // Material indexing for SSBO upload
        int m_materialIndex = -1;  // Index in the global material buffer

        // Texture array indices (for bindless texture array)
        std::map<std::string, int> m_textureArrayIndices;

        /**
         * @brief Gets the SSBO data for this material.
         * @return Reference to MaterialSSBO.
         */
        void UpdateSSBOData() const
        {
            if (!m_ssboDirty) return;

            // Update material data from parameters
            if (auto param = GetParameter("materialAlbedo"))
            {
                if (param->type == MaterialParamType::VEC4 && param->floatValues.size() >= 4)
                {
                    m_materialData.albedo = Vec4(param->floatValues[0],
                        param->floatValues[1],
                        param->floatValues[2],
                        param->floatValues[3]);
                }
                else if (param->type == MaterialParamType::VEC3 && param->floatValues.size() >= 3)
                {
                    m_materialData.albedo = Vec4(param->floatValues[0],
                        param->floatValues[1],
                        param->floatValues[2],
                        1.0f);
                }
            }

            if (auto param = GetParameter("materialMetallic"))
                m_materialData.metallic = param->floatValues[0];

            if (auto param = GetParameter("materialRoughness"))
                m_materialData.roughness = param->floatValues[0];

            if (auto param = GetParameter("materialAo"))
                m_materialData.ao = param->floatValues[0];

            if (auto param = GetParameter("materialNormalStrength"))
                m_materialData.normalStrength = param->floatValues[0];

            if (auto param = GetParameter("materialEmissive"))
            {
                if (param->floatValues.size() >= 3)
                {
                    m_materialData.emissive = Vec3(param->floatValues[0],
                        param->floatValues[1],
                        param->floatValues[2]);
                }
            }

            if (auto param = GetParameter("materialEmissiveIntensity"))
                m_materialData.emissiveIntensity = param->floatValues[0];

            if (auto param = GetParameter("materialShadingModel"))
                m_materialData.shadingModel = param->intValue;

            if (auto param = GetParameter("materialCastsShadows"))
                m_materialData.castsShadows = param->boolValue ? 1 : 0;

            // Mesh fill controls
            float fillAmount = 1.0f;
            if (auto param = GetParameter("materialFillAmount"))
            {
                if (!param->floatValues.empty()) {
                    fillAmount = param->floatValues[0];
                }
            }
            m_materialData.fillAmount = Clamp01(fillAmount);

            Vec2 fillAxisUV(0.0f, 1.0f);
            if (auto param = GetParameter("materialFillUVAxis"))
            {
                if (param->floatValues.size() >= 2)
                {
                    fillAxisUV = Vec2(param->floatValues[0], param->floatValues[1]);
                }
            }
            else if (auto param = GetParameter("materialFillDirection"))
            {
                if (param->floatValues.size() >= 2)
                {
                    fillAxisUV = Vec2(param->floatValues[0], param->floatValues[1]);
                }
            }

            const float fillAxisLenSq = fillAxisUV.x * fillAxisUV.x + fillAxisUV.y * fillAxisUV.y;
            if (fillAxisLenSq < 1e-12f) {
                fillAxisUV = Vec2(0.0f, 1.0f);
            }
            else {
                const float invLen = 1.0f / std::sqrt(fillAxisLenSq);
                fillAxisUV.x *= invLen;
                fillAxisUV.y *= invLen;
            }

            m_materialData.fillDirOctX = fillAxisUV.x;
            m_materialData.fillDirOctY = fillAxisUV.y;

            // Update texture flags (packed into bitfield for efficiency)
            m_materialData.textureFlags = 0;
            if (GetParameter("materialHasAlbedoMap") && GetParameter("materialHasAlbedoMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_ALBEDO_MAP;
            if (GetParameter("materialHasNormalMap") && GetParameter("materialHasNormalMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_NORMAL_MAP;
            if (GetParameter("materialHasRoughnessMap") && GetParameter("materialHasRoughnessMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_ROUGHNESS_MAP;
            if (GetParameter("materialHasMetallicMap") && GetParameter("materialHasMetallicMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_METALLIC_MAP;
            if (GetParameter("materialHasAoMap") && GetParameter("materialHasAoMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_AO_MAP;
            if (GetParameter("materialHasEmissiveMap") && GetParameter("materialHasEmissiveMap")->boolValue)
                m_materialData.textureFlags |= MAT_FLAG_EMISSIVE_MAP;

            // Update texture array indices
            m_materialData.albedoMapIndex = GetTextureArrayIndex("materialAlbedoMap");
            m_materialData.normalMapIndex = GetTextureArrayIndex("materialNormalMap");
            m_materialData.roughnessMapIndex = GetTextureArrayIndex("materialRoughnessMap");
            m_materialData.metallicMapIndex = GetTextureArrayIndex("materialMetallicMap");
            m_materialData.aoMapIndex = GetTextureArrayIndex("materialAoMap");
            m_materialData.emissiveMapIndex = GetTextureArrayIndex("materialEmissiveMap");

            m_ssboDirty = false;
        }

    public:
        /**
         * @brief Default constructor.
         */
        Material() = default;
        /**
         * @brief Constructs a material with a shader and optional parameters.
         * @param shader Shared pointer to Shader.
         * @param params Map of parameter names to MaterialParam.
         */
        Material(std::shared_ptr<Shader> shader, const std::map<std::string, MaterialParam>& params = {})
            : m_shader(std::move(shader)), m_parameters(params) {
        }

        /**
         * @brief Sets the shader for this material.
         * @param shader Shared pointer to Shader.
         */
        void SetShader(std::shared_ptr<Shader> shader) { m_shader = std::move(shader); }

        /**
         * @brief Gets the shader associated with this material.
         * @return Shared pointer to Shader.
         */
        std::shared_ptr<Shader> GetShader() const { return m_shader; }

        /**
         * @brief Sets a material parameter.
         * @param name Parameter name.
         * @param param MaterialParam value.
         */
        void SetParameter(const std::string& name, const MaterialParam& param)
        {
            m_parameters[name] = param;
            m_ssboDirty = true;

            if (param.type == MaterialParamType::TEXTURE_2D)
            {
                if (m_textureSlots.find(name) == m_textureSlots.end())
                {
                    m_textureSlots[name] = m_nextTextureSlot++;
                }
            }
        }
        /**
         * @brief Sets a float parameter.
         * @param name Parameter name.
         * @param value Float value.
         */
        void SetFloat(const std::string& name, float value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets a Vec2 parameter.
         * @param name Parameter name.
         * @param value Vec2 value.
         */
        void SetVec2(const std::string& name, const Vec2& value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets a Vec3 parameter.
         * @param name Parameter name.
         * @param value Vec3 value.
         */
        void SetVec3(const std::string& name, const Vec3& value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets a Vec4 parameter.
         * @param name Parameter name.
         * @param value Vec4 value.
         */
        void SetVec4(const std::string& name, const Vec4& value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets an int parameter.
         * @param name Parameter name.
         * @param value Integer value.
         */
        void SetInt(const std::string& name, int value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets a bool parameter.
         * @param name Parameter name.
         * @param value Boolean value.
         */
        void SetBool(const std::string& name, bool value) {
            SetParameter(name, MaterialParam(value));
        }
        /**
         * @brief Sets a texture parameter.
         * @param name Parameter name.
         * @param texture Shared pointer to Texture.
         */
        void SetTexture(const std::string& name, std::shared_ptr<Texture> texture)
        {
            SetParameter(name, MaterialParam(std::move(texture)));
        }

        /**
         * @brief Gets a texture parameter.
         * @param name Parameter name.
         * @return Shared pointer to Texture, or nullptr if not found.
         */
        std::shared_ptr<Texture> GetTexture(const std::string& name)
        {
            if (auto param = GetParameter(name))
            {
                if (param->type == MaterialParamType::TEXTURE_2D)
                {
                    return param->texture;
                }
            }
            return nullptr;
        }
        /**
         * @brief Checks if a parameter exists.
         * @param name Parameter name.
         * @return true if parameter exists, false otherwise.
         */
        bool HasParameter(const std::string& name) const
        {
            return m_parameters.find(name) != m_parameters.end();
        }
        /**
         * @brief Get all parameters (read-only).
         * @return Reference to the internal parameter map.
         */
        const std::map<std::string, MaterialParam>& GetParameters() const
        {
            return m_parameters;
        }
        /**
         * @brief Gets a parameter by name.
         * @param name Parameter name.
         * @return Pointer to MaterialParam, or nullptr if not found.
         */
        const MaterialParam* GetParameter(const std::string& name) const
        {
            auto it = m_parameters.find(name);
            return it != m_parameters.end() ? &it->second : nullptr;
        }
        /**
         * @brief Gets the SSBO data for this material.
         * @return Reference to MaterialSSBO.
         */
        const MaterialSSBO& GetSSBOData() const
        {
            UpdateSSBOData();
            return m_materialData;
        }

        /**
         * @brief Checks if the material SSBO data is dirty and needs GPU update.
         * @return True if material has been modified since last GPU upload.
         */
        bool IsDirty() const
        {
            return m_ssboDirty;
        }
        /**
         * @brief Binds all textures associated with this material.
         */
        void BindTextures() const
        {
            if (!m_shader || !m_shader->IsValid()) return;

            for (const auto& [name, param] : m_parameters)
            {
                if (param.type == MaterialParamType::TEXTURE_2D &&
                    param.texture && param.texture->IsValid())
                {
                    auto slotIt = m_textureSlots.find(name);
                    if (slotIt != m_textureSlots.end())
                    {
                        param.texture->Bind(slotIt->second);
                        m_shader->SetUniform1i(name, slotIt->second);
                    }
                }
            }
        }
        /**
         * @brief Binds the material's shader and textures.
         */
        void Bind() const
        {
            if (!m_shader || !m_shader->IsValid()) return;
            m_shader->Bind();
            BindTextures();
        }
        /**
         * @brief Unbinds all textures and the shader.
         */
        void Unbind() const
        {
            for (const auto& [name, param] : m_parameters)
            {
                if (param.type == MaterialParamType::TEXTURE_2D &&
                    param.texture && param.texture->IsValid())
                {
                    param.texture->Unbind();
                }
            }

            if (m_shader) m_shader->Unbind();
        }
        /**
         * @brief Loads a material template into this material.
         * @param templateParams Map of parameter names to MaterialParam.
         */
        void LoadTemplate(const std::map<std::string, MaterialParam>& templateParams)
        {
            for (const auto& [name, param] : templateParams)
            {
                SetParameter(name, param);
            }
        }
        /**
         * @brief Copy constructor.
         * @param other Material to copy from.
         */
        Material(const Material& other)
            : m_parameters(other.m_parameters)
            , m_shader(other.m_shader)
            , m_textureSlots(other.m_textureSlots)
            , m_nextTextureSlot(other.m_nextTextureSlot)
            , m_materialData(other.m_materialData)
            , m_ssboDirty(true)
        {
        }
        /**
         * @brief Copy assignment operator.
         * @param other Material to copy from.
         * @return Reference to this material.
         */
        Material& operator=(const Material& other)
        {
            if (this != &other)
            {
                m_parameters = other.m_parameters;
                m_shader = other.m_shader;
                m_textureSlots = other.m_textureSlots;
                m_nextTextureSlot = other.m_nextTextureSlot;
                m_materialData = other.m_materialData;
                m_ssboDirty = true;
            }
            return *this;
        }
        /**
         * @brief Sets the UV scale for texture sampling.
         * @param scale Vec2 representing the UV scale factor.
         */
        void SetUVScale(const Vec2& scale)
        {
            m_materialData.uvScale = scale;
            m_ssboDirty = true;
        }
        
        /**
         * @brief Sets the UV offset for texture sampling.
         * @param offset Vec2 representing the UV offset.
         */
        void SetUVOffset(const Vec2& offset)
        {
            m_materialData.uvOffset = offset;
            m_ssboDirty = true;
        }
        
        /**
         * @brief Gets the UV scale.
         * @return Vec2 UV scale.
         */
        Vec2 GetUVScale() const { return m_materialData.uvScale; }
        
        /**
         * @brief Gets the UV offset.
         * @return Vec2 UV offset.
         */
        Vec2 GetUVOffset() const { return m_materialData.uvOffset; }
        
        /**
         * @brief Sets the material index in the global buffer.
         * @param index The material index.
         */
        void SetMaterialIndex(int index) { m_materialIndex = index; }
        
        /**
         * @brief Gets the material index in the global buffer.
         * @return The material index, or -1 if not assigned.
         */
        int GetMaterialIndex() const { return m_materialIndex; }

        /**
         * @brief Sets a texture array index for a specific texture type.
         * @param textureName The name of the texture parameter (e.g., "materialAlbedoMap").
         * @param index The index in the global texture array.
         */
        void SetTextureArrayIndex(const std::string& textureName, int index)
        {
            m_textureArrayIndices[textureName] = index;
            m_ssboDirty = true;
        }

        /**
         * @brief Gets the texture array index for a specific texture type.
         * @param textureName The name of the texture parameter.
         * @return The texture array index, or -1 if not found.
         */
        int GetTextureArrayIndex(const std::string& textureName) const
        {
            auto it = m_textureArrayIndices.find(textureName);
            return it != m_textureArrayIndices.end() ? it->second : -1;
        }

        /**
         * @brief Gets all texture array indices.
         * @return Map of texture names to array indices.
         */
        const std::map<std::string, int>& GetTextureArrayIndices() const
        {
            return m_textureArrayIndices;
        }

        /**
         * @brief Gets the cubemap textures associated with this material.
         * @return Unordered map of cubemap names to shared pointers.
         */
        const std::unordered_map<std::string, std::shared_ptr<Cubemap>>& GetCubemaps() const {
            return cubemaps;
        }
    };

    /*!***********************************************************************
    \brief
        Material factory for common materials
    *************************************************************************/
    class MaterialFactory
    {
    public:
        /**
         * @brief Creates a PBR material with specified properties.
         * @param shader Shared pointer to Shader.
         * @param albedo Albedo color.
         * @param metallic Metallic value.
         * @param roughness Roughness value.
         * @return Unique pointer to Material.
         */
        static std::unique_ptr<Material> CreatePBRMaterial(std::shared_ptr<Shader> shader, const Vec3& albedo,
            float metallic, float roughness)
        {
            auto material = std::make_unique<Material>(shader);
            material->SetVec3("materialAlbedo", albedo);
            material->SetFloat("materialMetallic", metallic);
            material->SetFloat("materialRoughness", roughness);
            material->SetFloat("materialAo", 1.0f);
            material->SetVec3("materialEmissive", Vec3(0.0f, 0.0f, 0.0f));
            material->SetFloat("materialEmissiveIntensity", 0.0f);
            material->SetInt("materialShadingModel", 0);
            return material;
        }

        /**
         * @brief Creates an emissive material.
         * @param shader Shared pointer to Shader.
         * @param color Emissive color.
         * @param intensity Emissive intensity.
         * @return Unique pointer to Material.
         */
        static std::unique_ptr<Material> CreateEmissiveMaterial(std::shared_ptr<Shader> shader,
            const Vec3& color, float intensity)
        {
            auto material = std::make_unique<Material>(shader);
            material->LoadTemplate(MaterialTemplates::EMISSIVE(color, intensity));
            return material;
        }
    };
}
