/* Start Header ************************************************************************/
/*!
\file       forward_opaque_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Forward opaque pass fragment shader.
            Writes a single HDR colour target using the same G-buffer-compatible
            PBR model so results blend with deferred objects seamlessly.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// ── Inputs ─────────────────────────────────────────────────────────────────────
layout(location = 0)  in vec3  vWorldPos;
layout(location = 1)  in vec3  vViewPos;
layout(location = 2)  in vec2  vTexCoord;
layout(location = 3)  in vec3  vViewNormal;
layout(location = 4)  in vec3  vViewTangent;
layout(location = 5)  in vec3  vViewBitangent;
layout(location = 6)  flat in uint  vMaterialIndex;
layout(location = 7)  flat in vec4  vAlbedo;
layout(location = 8)  flat in vec4  vRoughnessAoEtc;
layout(location = 9)  flat in vec3  vEmissive;
layout(location = 10) flat in uint  vTextureFlags;
layout(location = 11) flat in ivec4 vTexIndices0;
layout(location = 12) flat in ivec2 vTexIndices1;
layout(location = 13) in vec2  vTransformedUV;

layout(location = 0) out vec4 outColor;

// ── Set 0: Camera + Lights ────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view; mat4 projection; mat4 viewProjection;
    mat4 invView; mat4 invProjection; mat4 prevViewProjection;
    vec4 cameraPosition; vec4 cameraDirection;
    vec2 resolution; vec2 invResolution;
    float elapsedTime; float deltaTime; uint frameIndex; float _pad;
} camera;

const int MAX_LIGHTS   = 32;
const int NUM_CASCADES = 4;
struct Light {
    vec4 position_type;
    vec4 color_intensity;
    vec4 direction_range;
    vec4 spot_angles_castshadows_startOffset;
    mat4 lightSpaceMatrix[NUM_CASCADES];
    mat4 pointLightMatrices[6];
    vec4 splitDepths[1];
};
layout(set = 0, binding = 1) uniform LightsUBO { vec4 lightCount; Light lights[MAX_LIGHTS]; } lights_ubo;

// ── Set 1: Shadow map array ────────────────────────────────────────────────────
layout(set = 1, binding = 5) uniform sampler2DArray shadowMapArray;

// ── Set 2: Bindless textures ───────────────────────────────────────────────────
layout(set = 2, binding = 1) uniform sampler2D textures[];

// ── PBR helpers ───────────────────────────────────────────────────────────────
const float PI = 3.14159265359;
float distGGX(vec3 N, vec3 H, float r)   { float a=r*r,a2=a*a,nh=max(dot(N,H),0.0),d=nh*nh*(a2-1.0)+1.0; return a2/(PI*d*d); }
float geomSchlick(float nv, float r)     { float k=(r+1.0)*(r+1.0)/8.0; return nv/(nv*(1.0-k)+k); }
float geomSmith(vec3 N,vec3 V,vec3 L,float r){ return geomSchlick(max(dot(N,V),0.0),r)*geomSchlick(max(dot(N,L),0.0),r); }
vec3  fresnel(float cosT,vec3 F0)        { return F0+(1.0-F0)*pow(clamp(1.0-cosT,0.0,1.0),5.0); }

// ── main ──────────────────────────────────────────────────────────────────────
void main()
{
    const uint MAT_ALBEDO = 1u, MAT_NORMAL = 2u, MAT_ROUGH = 4u, MAT_METAL = 8u,
               MAT_AO    = 16u, MAT_EMISS = 32u;

    vec3  albedo    = vAlbedo.rgb;
    float metallic  = vAlbedo.a;
    float roughness = vRoughnessAoEtc.r;
    float ao        = vRoughnessAoEtc.g;
    float emissiveI = vRoughnessAoEtc.b;
    float normStr   = vRoughnessAoEtc.a;

    // Textures
    if ((vTextureFlags & MAT_ALBEDO) != 0u && vTexIndices0.x >= 0)
        albedo   *= texture(textures[nonuniformEXT(vTexIndices0.x)], vTransformedUV).rgb;
    if ((vTextureFlags & MAT_ROUGH)  != 0u && vTexIndices0.z >= 0)
        roughness *= texture(textures[nonuniformEXT(vTexIndices0.z)], vTransformedUV).r;
    if ((vTextureFlags & MAT_METAL)  != 0u && vTexIndices0.w >= 0)
        metallic  *= texture(textures[nonuniformEXT(vTexIndices0.w)], vTransformedUV).r;
    if ((vTextureFlags & MAT_AO)     != 0u && vTexIndices1.x >= 0)
        ao        *= texture(textures[nonuniformEXT(vTexIndices1.x)], vTransformedUV).r;

    vec3 N = normalize(vViewNormal);
    if ((vTextureFlags & MAT_NORMAL) != 0u && vTexIndices0.y >= 0)
    {
        vec3 tn = fma(texture(textures[nonuniformEXT(vTexIndices0.y)], vTransformedUV).rgb,
                      vec3(2.0), vec3(-1.0));
        tn.xy *= normStr;
        N = normalize(vViewTangent * tn.x + vViewBitangent * tn.y + vViewNormal * tn.z);
    }

    vec3 V   = normalize(-vViewPos);
    vec3 F0  = mix(vec3(0.04), albedo, metallic);
    vec3 res = vec3(0.04) * albedo * ao; // Ambient

    int numLights = int(lights_ubo.lightCount.x);
    for (int i = 0; i < numLights; ++i)
    {
        int   ltype = int(lights_ubo.lights[i].position_type.w);
        vec3  lightDir;
        float att;
        const int DIR_LIGHT = 1, SPOT_LIGHT = 2;

        if (ltype == DIR_LIGHT) {
            lightDir = normalize(mat3(camera.view) * lights_ubo.lights[i].direction_range.xyz);
            att = 1.0;
        } else {
            vec3 lv = (camera.view * vec4(lights_ubo.lights[i].position_type.xyz,1.0)).xyz;
            lightDir = normalize(lv - vViewPos);
            float d = max(length(lv - vViewPos), 0.01);
            float r = lights_ubo.lights[i].direction_range.w;
            att = 1.0 / (1.0 + 0.045*d + 0.0075*d*d);
            if (d > r) att = 0.0; else att *= smoothstep(r, r*0.6, d);
            if (ltype == SPOT_LIGHT) {
                vec3 sd = normalize(mat3(camera.view) * lights_ubo.lights[i].direction_range.xyz);
                float ca = dot(-lightDir, sd);
                float inner = lights_ubo.lights[i].spot_angles_castshadows_startOffset.x;
                float outer = lights_ubo.lights[i].spot_angles_castshadows_startOffset.y;
                att *= clamp((ca-outer)/(inner-outer),0.0,1.0);
            }
        }
        if (att <= 0.0) continue;

        vec3  H    = normalize(V + lightDir);
        roughness  = clamp(roughness, 0.05, 1.0);
        float NDF  = distGGX(N, H, roughness);
        float G    = geomSmith(N, V, lightDir, roughness);
        vec3  F    = fresnel(max(dot(H,V),0.0), F0);
        vec3  kD   = (vec3(1.0) - F) * (1.0 - metallic);
        vec3  spec = NDF * G * F / (4.0 * max(dot(N,V),0.0) * max(dot(N,lightDir),0.0) + 0.0001);
        vec3  rad  = lights_ubo.lights[i].color_intensity.xyz * lights_ubo.lights[i].color_intensity.w * att;
        res += (kD * albedo / PI + spec) * rad * max(dot(N, lightDir), 0.0);
    }

    vec3 emissive = vEmissive;
    if ((vTextureFlags & MAT_EMISS) != 0u && vTexIndices1.y >= 0)
        emissive *= texture(textures[nonuniformEXT(vTexIndices1.y)], vTransformedUV).rgb;
    if (emissiveI > 0.001) res += emissive * emissiveI;

    outColor = vec4(res, 1.0);
}
