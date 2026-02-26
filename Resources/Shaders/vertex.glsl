#version 460 core

// Vertex attributes from VBO (via VAO)
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in ivec4 aBoneIDs;      // Only present in SkinnedVAO
layout(location = 5) in vec4 aBoneWeights;    // Only present in SkinnedVAO

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

// SSBO bindings
layout(std430, binding = 1) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

// Skeletal animation bone transforms SSBO (Binding 7)
layout(std430, binding = 2) restrict readonly buffer BoneTransformBuffer {
    mat4 boneTransforms[];
};

// Transformation matrices
uniform mat4 view;
uniform mat4 projection;

// Base draw ID offset for multi-batch rendering
uniform uint baseDrawID;

// Outputs to fragment shader
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec3 ViewPos;
out vec3 Tangent;
out vec3 Bitangent;
flat out uint vMaterialIndex;
flat out vec3 vModelCenter;  // Model center in world space (for volumetrics)
flat out vec3 vCameraPos;    // Camera position in world space

void main()
{
    // Get draw info for this draw call (offset by baseDrawID for multi-batch rendering)
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];
    mat4 model = drawInfo.modelMatrix;
    mat3 normalMatrix = mat3(drawInfo.normalMatrixCol0, drawInfo.normalMatrixCol1, drawInfo.normalMatrixCol2);

    // DrawInfo flag bits (must match C++ DrawCommands.h)
    const uint FLAG_SKINNING = 1u << 0u;
    const uint FLAG_CAMERA_ATTACHED = 1u << 1u;

    // Extract useSkinning flag from bit 0 of flags
    bool useSkinning = (drawInfo.flags & FLAG_SKINNING) != 0u;

    vec4 skinnedPos = vec4(aPos, 1.0);
    vec3 skinnedNormal  = aNormal;
    vec3 skinnedTangent = aTangent;

    // Apply skeletal animation if enabled
    if (useSkinning) {
        // Get bone offset for this entity from DrawInfo
        uint boneOffset = drawInfo.boneTransformOffset;

        // Optimized conditional bone skinning - skip zero-weight bones
        // Processes positions, normals, and tangents for forward pass
        vec4 skinnedPosition = vec4(0.0);
        vec3 skinnedNormalVec = vec3(0.0);
        vec3 skinnedTangentVec = vec3(0.0);

        // Bone 0 - skip if zero weight
        if (aBoneWeights[0] > 0.0) {
            mat4 boneTransform = boneTransforms[boneOffset + aBoneIDs[0]];
            skinnedPosition += boneTransform * vec4(aPos, 1.0) * aBoneWeights[0];
            skinnedNormalVec += mat3(boneTransform) * aNormal * aBoneWeights[0];
            skinnedTangentVec += mat3(boneTransform) * aTangent * aBoneWeights[0];
        }

        // Bone 1
        if (aBoneWeights[1] > 0.0) {
            mat4 boneTransform = boneTransforms[boneOffset + aBoneIDs[1]];
            skinnedPosition += boneTransform * vec4(aPos, 1.0) * aBoneWeights[1];
            skinnedNormalVec += mat3(boneTransform) * aNormal * aBoneWeights[1];
            skinnedTangentVec += mat3(boneTransform) * aTangent * aBoneWeights[1];
        }

        // Bone 2
        if (aBoneWeights[2] > 0.0) {
            mat4 boneTransform = boneTransforms[boneOffset + aBoneIDs[2]];
            skinnedPosition += boneTransform * vec4(aPos, 1.0) * aBoneWeights[2];
            skinnedNormalVec += mat3(boneTransform) * aNormal * aBoneWeights[2];
            skinnedTangentVec += mat3(boneTransform) * aTangent * aBoneWeights[2];
        }

        // Bone 3
        if (aBoneWeights[3] > 0.0) {
            mat4 boneTransform = boneTransforms[boneOffset + aBoneIDs[3]];
            skinnedPosition += boneTransform * vec4(aPos, 1.0) * aBoneWeights[3];
            skinnedNormalVec += mat3(boneTransform) * aNormal * aBoneWeights[3];
            skinnedTangentVec += mat3(boneTransform) * aTangent * aBoneWeights[3];
        }

        skinnedPos = skinnedPosition;
        skinnedNormal = skinnedNormalVec;
        skinnedTangent = skinnedTangentVec;
    }

    // World & View
    vec4 worldPos = model * skinnedPos;
    vec4 viewPos = view * worldPos;

    gl_Position = projection * viewPos;
    TexCoord = aTexCoord;

    // Calculate fragment position in world space
    FragPos = worldPos.xyz;

    // Calculate view space position
    ViewPos = viewPos.xyz;

    // Transform normal using pre-calculated normal matrix (avoids expensive inverse())
    Normal = normalize(normalMatrix * skinnedNormal);

    // Calculate tangent and bitangent for normal mapping
    Tangent = normalize(normalMatrix * skinnedTangent);

    // Calculate bitangent using cross product
    Bitangent = cross(Normal, Tangent);

    // Gram-Schmidt process to re-orthogonalize TBN vectors
    // Only tangent needs correction; bitangent is recalculated from corrected tangent
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
    Bitangent = cross(Normal, Tangent);

    // Pass material index to fragment shader
    vMaterialIndex = drawInfo.materialIndex;

    // Extract model center from model matrix (translation component) for volumetric shaders
    vModelCenter = vec3(model[3][0], model[3][1], model[3][2]);

    // Extract camera position from view matrix for volumetric shaders
    mat3 viewRot = mat3(view);
    vCameraPos = -transpose(viewRot) * view[3].xyz;
}