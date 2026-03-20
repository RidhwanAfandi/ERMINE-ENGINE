/* Start Header ************************************************************************/
/*!
\file       outline_mask_indirect_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Outline mask vertex shader — depth-only pass that marks selected objects.
            No colour output; the depth result is read by the post-process shader to
            generate a screen-space outline edge.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) in vec3 aPos;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view; mat4 projection; mat4 viewProjection;
    mat4 invView; mat4 invProjection; mat4 prevViewProjection;
    vec4 cameraPosition; vec4 cameraDirection;
    vec2 resolution; vec2 invResolution;
    float elapsedTime; float deltaTime; uint frameIndex; float _pad;
} camera;

struct DrawInfo {
    mat4 modelMatrix;
    vec3 aabbMin; uint materialIndex;
    vec3 aabbMax; uint entityID;
    uint flags; uint boneTransformOffset; uint _pad0; uint _pad1;
    vec4 normalMatrixCol0; vec4 normalMatrixCol1; vec4 normalMatrixCol2;
};
layout(set = 3, binding = 1, std430) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

void main()
{
    gl_Position = camera.viewProjection * drawInfos[gl_DrawID].modelMatrix * vec4(aPos, 1.0);
}
