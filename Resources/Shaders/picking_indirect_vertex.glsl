#version 460 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;      // Not used for picking, but needed for attribute layout
layout(location = 2) in vec2 aTexCoord;    // Not used for picking, but needed for attribute layout
layout(location = 3) in vec3 aTangent;     // Not used for picking, but needed for attribute layout

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

    // Transform vertex to clip space
    gl_Position = u_ViewProjection * drawInfo.modelMatrix * vec4(aPosition, 1.0);

    // Pass entity ID to fragment shader (encoded as +1 so 0 means "no hit")
    v_EntityID = drawInfo.entityID + 1u;
}
