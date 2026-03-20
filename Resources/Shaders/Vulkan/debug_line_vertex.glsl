/* Start Header ************************************************************************/
/*!
\file       debug_line_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Debug line vertex shader. Uses interleaved pos+colour vertex buffer.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view; mat4 projection; mat4 viewProjection;
    mat4 invView; mat4 invProjection; mat4 prevViewProjection;
    vec4 cameraPosition; vec4 cameraDirection;
    vec2 resolution; vec2 invResolution;
    float elapsedTime; float deltaTime; uint frameIndex; float _pad;
} camera;

layout(location = 0) out vec3 vColor;

void main()
{
    vColor      = aColor;
    gl_Position = camera.viewProjection * vec4(aPos, 1.0);
}
