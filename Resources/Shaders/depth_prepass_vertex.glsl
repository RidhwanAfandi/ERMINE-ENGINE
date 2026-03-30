#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec4 aTangent;

// Skinning attributes
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4 aBoneWeights;

// Draw info structure (std430 layout)
// Total size: 160 bytes (must match C++ DrawInfo in DrawCommands.h)
struct DrawInfo {
    mat4 modelMatrix;           // 64 bytes (offset 0-63)
    vec3 aabbMin;               // 12 bytes (offset 64-75)
    uint materialIndex;         // 4 bytes (offset 76-79)
    vec3 aabbMax;               // 12 bytes (offset 80-91)
    uint entityID;              // 4 bytes (offset 92-95)
    uint flags;                 // 4 bytes (offset 96-99) - bit 0: useSkinning
    uint boneTransformOffset;   // 4 bytes (offset 100-103)
    uint _pad0;                 // 4 bytes (offset 104-107)
    uint _pad1;                 // 4 bytes (offset 108-111) - Padding to 16-byte boundary
    vec4 normalMatrixCol0;      // 16 bytes (offset 112-127) - Normal matrix column 0 (xyz used)
    vec4 normalMatrixCol1;      // 16 bytes (offset 128-143) - Normal matrix column 1 (xyz used)
    vec4 normalMatrixCol2;      // 16 bytes (offset 144-159) - Normal matrix column 2 (xyz used)
    
};

// DrawInfo SSBO (Binding 1)
layout(std430, binding = 1) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

// Skeletal animation SSBO (Binding 2)
layout(std430, binding = 2) restrict readonly buffer BoneTransformBuffer {
    mat4 boneTransforms[];
};

// IMPORTANT: Use same matrix uniforms as geometry pass for identical depth computation
uniform mat4 view;
uniform mat4 projection;
uniform uint baseDrawID;

void main()
{
    // Get draw info from SSBO using gl_DrawID (indirect rendering)
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];
    mat4 model = drawInfo.modelMatrix;

    // DrawInfo flag bits (must match C++ DrawCommands.h)
    const uint FLAG_SKINNING = 1u << 0u;
    const uint FLAG_CAMERA_ATTACHED = 1u << 1u;

    bool useSkinning = (drawInfo.flags & FLAG_SKINNING) != 0u;

    // Initialize skinned position (default to non-skinned)
    vec4 skinnedPos = vec4(aPos, 1.0);

    // Apply skinning if needed (optimized: skip zero-weight bones)
    if (useSkinning) {
        // Get bone offset for this entity from DrawInfo
        uint boneOffset = drawInfo.boneTransformOffset;

        // Optimized conditional bone skinning - depth prepass only needs positions
        vec4 skinnedPosition = vec4(0.0);

        // Bone 0 - skip if zero weight
        if (aBoneWeights[0] > 0.0) {
            skinnedPosition += boneTransforms[boneOffset + aBoneIDs[0]] * vec4(aPos, 1.0) * aBoneWeights[0];
        }

        // Bone 1
        if (aBoneWeights[1] > 0.0) {
            skinnedPosition += boneTransforms[boneOffset + aBoneIDs[1]] * vec4(aPos, 1.0) * aBoneWeights[1];
        }

        // Bone 2
        if (aBoneWeights[2] > 0.0) {
            skinnedPosition += boneTransforms[boneOffset + aBoneIDs[2]] * vec4(aPos, 1.0) * aBoneWeights[2];
        }

        // Bone 3
        if (aBoneWeights[3] > 0.0) {
            skinnedPosition += boneTransforms[boneOffset + aBoneIDs[3]] * vec4(aPos, 1.0) * aBoneWeights[3];
        }

        skinnedPos = skinnedPosition;
    }

    // CRITICAL: Use IDENTICAL matrix multiplication order as geometry pass
    // to ensure bit-exact depth values (prevents precision artifacts)
    vec4 worldPos = model * skinnedPos;
    vec4 viewPos = view * worldPos;
    gl_Position = projection * viewPos;
}
