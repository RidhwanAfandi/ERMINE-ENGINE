/* Start Header ************************************************************************/
/*!
\file       gBuffer_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      G-buffer fill fragment shader.
            Writes 4 MRTs:
              RT0 (RGBA32F)  – albedo (rgb) — packed0
              RT1 (RGBA16F)  – view-space normal (xyz) — packed1
              RT2 (RGBA8)    – emissive rgb + intensity — packed2
              RT3 (RGBA8)    – roughness, metallic, ao, motionBlurFlag — packed3

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// ── Inputs from vertex shader ──────────────────────────────────────────────────
layout(location =  0) in vec2  vTexCoord;
layout(location =  1) in vec3  vWorldPos;
layout(location =  2) in vec3  vViewPos;
layout(location =  3) in vec3  vViewNormal;
layout(location =  4) in vec3  vViewTangent;
layout(location =  5) in vec3  vViewBitangent;
layout(location =  6) flat in uint  vMaterialIndex;
layout(location =  7) flat in vec4  vAlbedo;            // rgb=albedo, a=metallic
layout(location =  8) flat in vec4  vRoughnessAoEtc;   // r=roughness, g=ao, b=emissiveIntensity, a=normalStrength
layout(location =  9) flat in vec3  vEmissive;
layout(location = 10) flat in uint  vTextureFlags;
layout(location = 11) flat in ivec4 vTexIndices0;       // albedo, normal, roughness, metallic
layout(location = 12) flat in ivec2 vTexIndices1;       // ao, emissive
layout(location = 13) in vec2  vTransformedUV;
layout(location = 14) flat in float vMotionBlurFlag;

// ── G-buffer MRT outputs ───────────────────────────────────────────────────────
layout(location = 0) out vec4 gBuffer0;  // Albedo
layout(location = 1) out vec4 gBuffer1;  // View-space normal
layout(location = 2) out vec4 gBuffer2;  // Emissive rgb + intensity
layout(location = 3) out vec4 gBuffer3;  // Roughness, metallic, ao, motionBlurFlag

// ── Set 2: Bindless texture array ─────────────────────────────────────────────
layout(set = 2, binding = 1) uniform sampler2D textures[];

// ── Material texture flag bits ─────────────────────────────────────────────────
const uint MAT_FLAG_ALBEDO_MAP    = 1u << 0u;
const uint MAT_FLAG_NORMAL_MAP    = 1u << 1u;
const uint MAT_FLAG_ROUGHNESS_MAP = 1u << 2u;
const uint MAT_FLAG_METALLIC_MAP  = 1u << 3u;
const uint MAT_FLAG_AO_MAP        = 1u << 4u;
const uint MAT_FLAG_EMISSIVE_MAP  = 1u << 5u;

void main()
{
    // ── Unpack material data from varyings ─────────────────────────────────────
    vec3  albedo           = vAlbedo.rgb;
    float metallic         = vAlbedo.a;
    float roughness        = vRoughnessAoEtc.r;
    float ao               = vRoughnessAoEtc.g;
    float emissiveIntensity= vRoughnessAoEtc.b;
    float normalStrength   = vRoughnessAoEtc.a;

    // ── Fast path: no textures ─────────────────────────────────────────────────
    if (vTextureFlags == 0u)
    {
        gBuffer0 = vec4(clamp(albedo, 0.0, 65504.0), 1.0);
        gBuffer1 = vec4(normalize(vViewNormal) * 0.5 + 0.5, 1.0);
        gBuffer2 = (emissiveIntensity > 0.001)
                 ? vec4(clamp(vEmissive, 0.0, 1.0), clamp(emissiveIntensity, 0.0, 255.0) / 255.0)
                 : vec4(0.0);
        gBuffer3 = vec4(clamp(roughness, 0.0, 1.0),
                        clamp(metallic,  0.0, 1.0),
                        clamp(ao,        0.0, 1.0),
                        vMotionBlurFlag);
        return;
    }

    // ── Sample textures ────────────────────────────────────────────────────────
    vec3  albedoSample    = ((vTextureFlags & MAT_FLAG_ALBEDO_MAP)    != 0u && vTexIndices0.x >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices0.x)], vTransformedUV).rgb
                          : vec3(1.0);

    vec3  normalSample    = ((vTextureFlags & MAT_FLAG_NORMAL_MAP)    != 0u && vTexIndices0.y >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices0.y)], vTransformedUV).rgb
                          : vec3(0.5, 0.5, 1.0);

    float roughnessSample = ((vTextureFlags & MAT_FLAG_ROUGHNESS_MAP) != 0u && vTexIndices0.z >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices0.z)], vTransformedUV).r
                          : 1.0;

    float metallicSample  = ((vTextureFlags & MAT_FLAG_METALLIC_MAP)  != 0u && vTexIndices0.w >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices0.w)], vTransformedUV).r
                          : 1.0;

    float aoSample        = ((vTextureFlags & MAT_FLAG_AO_MAP)        != 0u && vTexIndices1.x >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices1.x)], vTransformedUV).r
                          : 1.0;

    vec3  emissiveSample  = ((vTextureFlags & MAT_FLAG_EMISSIVE_MAP)  != 0u && vTexIndices1.y >= 0)
                          ? texture(textures[nonuniformEXT(vTexIndices1.y)], vTransformedUV).rgb
                          : vec3(1.0);

    // ── Compose material properties ────────────────────────────────────────────
    vec3 finalAlbedo     = albedo * albedoSample;
    float finalRoughness = roughness * roughnessSample;
    float finalMetallic  = metallic  * metallicSample;
    float finalAO        = ao        * aoSample;
    vec3 finalEmissive   = vEmissive  * emissiveSample;

    // ── Normal mapping ─────────────────────────────────────────────────────────
    vec3 finalNormal = normalize(vViewNormal);
    if ((vTextureFlags & MAT_FLAG_NORMAL_MAP) != 0u && vTexIndices0.y >= 0)
    {
        vec3 tN = fma(normalSample, vec3(2.0), vec3(-1.0));
        tN.xy   *= normalStrength;
        if (abs(normalStrength - 1.0) > 0.01) tN = normalize(tN);
        finalNormal = normalize(vViewTangent   * tN.x +
                                vViewBitangent * tN.y +
                                vViewNormal    * tN.z);
    }

    // ── Write G-buffer ─────────────────────────────────────────────────────────
    gBuffer0 = vec4(clamp(finalAlbedo, 0.0, 65504.0), 1.0);
    gBuffer1 = vec4(finalNormal * 0.5 + 0.5, 1.0);
    gBuffer2 = (emissiveIntensity > 0.001)
             ? vec4(clamp(finalEmissive, 0.0, 1.0), clamp(emissiveIntensity, 0.0, 255.0) / 255.0)
             : vec4(0.0);
    gBuffer3 = vec4(clamp(finalRoughness, 0.0, 1.0),
                    clamp(finalMetallic,  0.0, 1.0),
                    clamp(finalAO,        0.0, 1.0),
                    vMotionBlurFlag);
}
