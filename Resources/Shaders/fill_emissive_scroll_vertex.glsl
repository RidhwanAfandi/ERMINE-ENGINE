#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4 aBoneWeights;

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

struct MaterialData {
    vec4 albedo;
    float metallic;
    float roughness;
    float ao;
    float normalStrength;

    vec3 emissive;
    float emissiveIntensity;

    int shadingModel;
    uint textureFlags;
    int castsShadows; // Kept for CPU/GPU layout parity
    float fillAmount;

    vec2 uvScale;
    vec2 uvOffset;

    int albedoMapIndex;
    int normalMapIndex;
    int roughnessMapIndex;
    int metallicMapIndex;

    int aoMapIndex;
    int emissiveMapIndex;
    float fillDirOctX;
    float fillDirOctY;
};

layout(std430, binding = 1) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

layout(std430, binding = 2) restrict readonly buffer BoneTransformBuffer {
    mat4 boneTransforms[];
};

layout(std430, binding = 3) restrict readonly buffer MaterialBlock {
    MaterialData materials[];
};

uniform mat4 view;
uniform mat4 projection;
uniform uint baseDrawID;

out vec2 vBaseUV;
flat out vec4 vAlbedo;
flat out vec3 vEmissive;
flat out float vEmissiveIntensity;
flat out uint vTextureFlags;
flat out int vAlbedoMapIndex;
flat out float vFillAmount;
out float vFillCoord;
out vec2 vFillScrollDirUV;

void main()
{
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];
    MaterialData material = materials[drawInfo.materialIndex];

    const uint FLAG_SKINNING = 1u << 0u;
    bool useSkinning = (drawInfo.flags & FLAG_SKINNING) != 0u;

    vec4 skinnedPos = vec4(aPos, 1.0);
    vec3 skinnedNormal = aNormal;
    vec3 skinnedTangent = aTangent.xyz;

    if (useSkinning) {
        uint boneOffset = drawInfo.boneTransformOffset;
        vec4 skinnedPosition = vec4(0.0);
        vec3 skinnedNormalVec = vec3(0.0);
        vec3 skinnedTangentVec = vec3(0.0);

        if (aBoneWeights[0] > 0.0) {
            mat4 b = boneTransforms[boneOffset + aBoneIDs[0]];
            skinnedPosition += b * vec4(aPos, 1.0) * aBoneWeights[0];
            skinnedNormalVec += mat3(b) * aNormal * aBoneWeights[0];
            skinnedTangentVec += mat3(b) * aTangent.xyz * aBoneWeights[0];
        }
        if (aBoneWeights[1] > 0.0) {
            mat4 b = boneTransforms[boneOffset + aBoneIDs[1]];
            skinnedPosition += b * vec4(aPos, 1.0) * aBoneWeights[1];
            skinnedNormalVec += mat3(b) * aNormal * aBoneWeights[1];
            skinnedTangentVec += mat3(b) * aTangent.xyz * aBoneWeights[1];
        }
        if (aBoneWeights[2] > 0.0) {
            mat4 b = boneTransforms[boneOffset + aBoneIDs[2]];
            skinnedPosition += b * vec4(aPos, 1.0) * aBoneWeights[2];
            skinnedNormalVec += mat3(b) * aNormal * aBoneWeights[2];
            skinnedTangentVec += mat3(b) * aTangent.xyz * aBoneWeights[2];
        }
        if (aBoneWeights[3] > 0.0) {
            mat4 b = boneTransforms[boneOffset + aBoneIDs[3]];
            skinnedPosition += b * vec4(aPos, 1.0) * aBoneWeights[3];
            skinnedNormalVec += mat3(b) * aNormal * aBoneWeights[3];
            skinnedTangentVec += mat3(b) * aTangent.xyz * aBoneWeights[3];
        }

        skinnedPos = skinnedPosition;
        skinnedNormal = skinnedNormalVec;
        skinnedTangent = skinnedTangentVec;
    }

    vec4 worldPos = drawInfo.modelMatrix * skinnedPos;
    gl_Position = projection * view * worldPos;

    vAlbedo = material.albedo;
    vEmissive = material.emissive;
    vEmissiveIntensity = material.emissiveIntensity;
    vTextureFlags = material.textureFlags;
    vAlbedoMapIndex = material.albedoMapIndex;
    vFillAmount = clamp(material.fillAmount, 0.0, 1.0);
    vBaseUV = fma(aTexCoord, material.uvScale, material.uvOffset);

    const float EPS = 1e-6;
    vec2 fillAxisUV = vec2(material.fillDirOctX, material.fillDirOctY);
    float fillAxisLenSq = dot(fillAxisUV, fillAxisUV);
    if (fillAxisLenSq <= EPS) {
        fillAxisUV = vec2(0.0, 1.0);
    } else {
        fillAxisUV *= inversesqrt(fillAxisLenSq);
    }

    float p = dot(aTexCoord, fillAxisUV);
    float minP = min(min(0.0, fillAxisUV.x), min(fillAxisUV.y, fillAxisUV.x + fillAxisUV.y));
    float maxP = max(max(0.0, fillAxisUV.x), max(fillAxisUV.y, fillAxisUV.x + fillAxisUV.y));
    float range = maxP - minP;
    if (range <= EPS) {
        vFillCoord = 0.0;
    } else {
        vFillCoord = clamp((p - minP) / range, 0.0, 1.0);
    }

    vec2 scrollDir = fillAxisUV;
    float scrollLen = length(scrollDir);
    vFillScrollDirUV = (scrollLen > EPS) ? (scrollDir / scrollLen) : vec2(0.0, 1.0);
}
