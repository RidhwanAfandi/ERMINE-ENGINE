/* Start Header ************************************************************************/
/*!
\file       depth_prepass_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Depth pre-pass vertex shader.
            Writes only depth; no colour outputs.
            Uses gl_DrawID to fetch per-draw model matrix from DrawInfo SSBO (set 3, binding 1).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

// ── Vertex inputs (position only – depth pass) ─────────────────────────────────
layout(location = 0) in vec3 aPos;

// ── Set 0: Per-frame data ──────────────────────────────────────────────────────
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    mat4 prevViewProjection;
    vec4 cameraPosition;   // xyz=pos, w=nearPlane
    vec4 cameraDirection;  // xyz=dir, w=farPlane
    vec2 resolution;
    vec2 invResolution;
    float elapsedTime;
    float deltaTime;
    uint frameIndex;
    float _pad;
} camera;

// ── Set 3: DrawInfo SSBO (model matrices per draw call) ────────────────────────
struct DrawInfo {
    mat4 modelMatrix;           // offset  0
    vec3 aabbMin;               // offset 64
    uint materialIndex;         // offset 76
    vec3 aabbMax;               // offset 80
    uint entityID;              // offset 92
    uint flags;                 // offset 96
    uint boneTransformOffset;   // offset 100
    uint _pad0;
    uint _pad1;
    vec4 normalMatrixCol0;      // offset 112
    vec4 normalMatrixCol1;      // offset 128
    vec4 normalMatrixCol2;      // offset 144
};

layout(set = 3, binding = 1, std430) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

void main()
{
    mat4 model = drawInfos[gl_DrawID].modelMatrix;
    gl_Position = camera.viewProjection * model * vec4(aPos, 1.0);
}
