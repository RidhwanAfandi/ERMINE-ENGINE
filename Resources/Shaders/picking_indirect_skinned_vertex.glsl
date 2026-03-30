#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;      // Not used for picking, but needed for attribute layout
layout(location = 2) in vec2 aTexCoord;    // Not used for picking, but needed for attribute layout
layout(location = 3) in vec4 aTangent;     // Not used for picking, but needed for attribute layout

// Skinning attributes
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4 aWeights;

// Draw info structure matching CPU-side DrawInfo (std430 layout)
// Total size: 160 bytes (must match C++ DrawInfo in DrawCommands.h)
struct DrawInfo {
    mat4 modelMatrix;           // 64 bytes (offset 0-63) - Model transformation matrix
    vec3 aabbMin;               // 12 bytes (offset 64-75) - AABB minimum bounds
    uint materialIndex;         // 4 bytes (offset 76-79) - Index into material SSBO
    vec3 aabbMax;               // 12 bytes (offset 80-91) - AABB maximum bounds
    uint entityID;              // 4 bytes (offset 92-95) - Entity ID
    uint flags;                 // 4 bytes (offset 96-99) - Flags (bit 0: useSkinning)
    uint boneTransformOffset;   // 4 bytes (offset 100-103) - Starting index in skeletal SSBO
    uint _pad0;                 // 4 bytes (offset 104-107) - Padding
    uint _pad1;                 // 4 bytes (offset 108-111) - Padding to 16-byte boundary
    vec4 normalMatrixCol0;      // 16 bytes (offset 112-127) - Normal matrix column 0 (xyz used)
    vec4 normalMatrixCol1;      // 16 bytes (offset 128-143) - Normal matrix column 1 (xyz used)
    vec4 normalMatrixCol2;      // 16 bytes (offset 144-159) - Normal matrix column 2 (xyz used)
    
};

// SSBO binding for indirect rendering DrawInfo
layout(std430, binding = 1) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

// Skeletal animation bone transforms SSBO (Binding 2)
layout(std430, binding = 2) restrict readonly buffer BoneTransformBuffer {
    mat4 boneTransforms[]; // All bone transforms for all entities
};

// Base draw ID offset for multi-batch indirect rendering
uniform uint baseDrawID;

// View-projection matrix
uniform mat4 u_ViewProjection;

// Pass entity ID to fragment shader
flat out uint v_EntityID;

void main()
{
    // Get draw info from SSBO using gl_DrawID (indirect rendering)
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];
    mat4 modelMatrix = drawInfo.modelMatrix;

    // DrawInfo flag bits (must match C++ DrawCommands.h)
    const uint FLAG_SKINNING = 1u << 0u;
    const uint FLAG_CAMERA_ATTACHED = 1u << 1u;

    bool useSkinning = (drawInfo.flags & FLAG_SKINNING) != 0u;

    // Calculate skinned position
    vec4 skinnedPos = vec4(aPosition, 1.0);

    if (useSkinning) {
        // Get bone offset for this entity from DrawInfo
        uint boneOffset = drawInfo.boneTransformOffset;

        // Calculate final bone transform using weighted blend
        mat4 boneTransform =
            boneTransforms[boneOffset + aBoneIDs[0]] * aWeights[0] +
            boneTransforms[boneOffset + aBoneIDs[1]] * aWeights[1] +
            boneTransforms[boneOffset + aBoneIDs[2]] * aWeights[2] +
            boneTransforms[boneOffset + aBoneIDs[3]] * aWeights[3];

        skinnedPos = boneTransform * vec4(aPosition, 1.0);
    }

    // Transform vertex to clip space
    gl_Position = u_ViewProjection * modelMatrix * skinnedPos;

    // Pass entity ID to fragment shader (encoded as +1 so 0 means "no hit")
    v_EntityID = drawInfo.entityID + 1u;
}
