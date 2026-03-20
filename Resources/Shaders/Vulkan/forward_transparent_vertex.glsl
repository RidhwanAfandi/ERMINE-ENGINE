/* Start Header ************************************************************************/
/*!
\file       forward_transparent_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Forward-opaque pass vertex shader (custom-shader opaque objects).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;

// ── Set 0: Camera ─────────────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    mat4 prevViewProjection;
    vec4 cameraPosition;
    vec4 cameraDirection;
    vec2 resolution;
    vec2 invResolution;
    float elapsedTime;
    float deltaTime;
    uint frameIndex;
    float _pad;
} camera;

// ── Set 2: Material SSBO ──────────────────────────────────────────────────────
struct MaterialData {
    vec4  albedo;
    float metallic;
    float roughness;
    float ao;
    float normalStrength;
    vec3  emissive;
    float emissiveIntensity;
    int   shadingModel;
    uint  textureFlags;
    int   castsShadows;
    float _pad0;
    vec2  uvScale;
    vec2  uvOffset;
    int   albedoMapIndex;
    int   normalMapIndex;
    int   roughnessMapIndex;
    int   metallicMapIndex;
    int   aoMapIndex;
    int   emissiveMapIndex;
};
layout(set = 2, binding = 0, std430) restrict readonly buffer MaterialBuffer {
    MaterialData materials[];
};

// ── Set 3: DrawInfo ────────────────────────────────────────────────────────────
struct DrawInfo {
    mat4 modelMatrix;
    vec3 aabbMin;
    uint materialIndex;
    vec3 aabbMax;
    uint entityID;
    uint flags;
    uint boneTransformOffset;
    uint _pad0;
    uint _pad1;
    vec4 normalMatrixCol0;
    vec4 normalMatrixCol1;
    vec4 normalMatrixCol2;
};
layout(set = 3, binding = 1, std430) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

// ── Outputs ────────────────────────────────────────────────────────────────────
layout(location = 0) out vec3  vWorldPos;
layout(location = 1) out vec3  vViewPos;
layout(location = 2) out vec2  vTexCoord;
layout(location = 3) out vec3  vViewNormal;
layout(location = 4) out vec3  vViewTangent;
layout(location = 5) out vec3  vViewBitangent;
layout(location = 6) flat out uint  vMaterialIndex;
layout(location = 7) flat out vec4  vAlbedo;
layout(location = 8) flat out vec4  vRoughnessAoEtc;
layout(location = 9) flat out vec3  vEmissive;
layout(location = 10) flat out uint vTextureFlags;
layout(location = 11) flat out ivec4 vTexIndices0;
layout(location = 12) flat out ivec2 vTexIndices1;
layout(location = 13) out vec2 vTransformedUV;

void main()
{
    DrawInfo di = drawInfos[gl_DrawID];
    mat3 normalMatrix = mat3(di.normalMatrixCol0.xyz,
                             di.normalMatrixCol1.xyz,
                             di.normalMatrixCol2.xyz);

    vec4 worldPos4 = di.modelMatrix * vec4(aPos, 1.0);
    vec4 viewPos4  = camera.view * worldPos4;
    vWorldPos      = worldPos4.xyz;
    vViewPos       = viewPos4.xyz;
    gl_Position    = camera.projection * viewPos4;
    vTexCoord      = aTexCoord;

    mat3 mvn       = mat3(camera.view) * normalMatrix;
    vViewNormal    = normalize(mvn * aNormal);
    vViewTangent   = normalize(mvn * aTangent.xyz);
    vViewTangent   = normalize(vViewTangent - dot(vViewTangent, vViewNormal) * vViewNormal);
    vViewBitangent = cross(vViewNormal, vViewTangent) * aTangent.w;

    vMaterialIndex = di.materialIndex;
    MaterialData mat = materials[di.materialIndex];
    vAlbedo          = vec4(mat.albedo.rgb, mat.metallic);
    vRoughnessAoEtc  = vec4(mat.roughness, mat.ao, mat.emissiveIntensity, mat.normalStrength);
    vEmissive        = mat.emissive;
    vTextureFlags    = mat.textureFlags;
    vTexIndices0     = ivec4(mat.albedoMapIndex, mat.normalMapIndex,
                             mat.roughnessMapIndex, mat.metallicMapIndex);
    vTexIndices1     = ivec2(mat.aoMapIndex, mat.emissiveMapIndex);
    vTransformedUV   = fma(aTexCoord, mat.uvScale, mat.uvOffset);
}
