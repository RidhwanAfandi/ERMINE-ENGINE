#version 460 core

layout(location = 0) in vec3 aPos;

// Draw info structure must match C++ DrawInfo (same as other indirect shaders)
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

uniform mat4 view;
uniform mat4 projection;
uniform uint baseDrawID;

void main()
{
    DrawInfo drawInfo = drawInfos[baseDrawID + gl_DrawID];
    vec4 worldPos = drawInfo.modelMatrix * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
}