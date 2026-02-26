#version 460 core

layout(location = 0) in vec3 aPos;
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

layout(std430, binding = 1) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

layout(std430, binding = 2) restrict readonly buffer BoneTransformBuffer {
    mat4 boneTransforms[];
};

uniform mat4 view;
uniform mat4 projection;
uniform uint baseDrawID;

void main()
{
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];

    // Must match the flag meaning used elsewhere
    const uint FLAG_SKINNING = 1u << 0u;
    bool useSkinning = (drawInfo.flags & FLAG_SKINNING) != 0u;

    vec4 skinnedPos = vec4(aPos, 1.0);

    if (useSkinning)
    {
        uint boneOffset = drawInfo.boneTransformOffset;

        // Weighted blend (same style as your shadowmap_instanced shader)
        mat4 boneTransform =
            boneTransforms[boneOffset + aBoneIDs[0]] * aBoneWeights[0] +
            boneTransforms[boneOffset + aBoneIDs[1]] * aBoneWeights[1] +
            boneTransforms[boneOffset + aBoneIDs[2]] * aBoneWeights[2] +
            boneTransforms[boneOffset + aBoneIDs[3]] * aBoneWeights[3];

        skinnedPos = boneTransform * vec4(aPos, 1.0);
    }

    vec4 worldPos = drawInfo.modelMatrix * skinnedPos;
    gl_Position = projection * view * worldPos;
}