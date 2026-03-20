/* Start Header ************************************************************************/
/*!
\file       shadowmap_instanced_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Shadow map vertex shader.
            pc.drawIndex = shadow layer index.
            Fetches the light-space view-projection from the ShadowViews SSBO (set 0, binding 3)
            and the per-object model matrix from the DrawInfo SSBO (set 3, binding 1).
            Front-face culling is set at pipeline level to reduce Peter-panning.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) in vec3 aPos;

// ── Push constants ─────────────────────────────────────────────────────────────
layout(push_constant) uniform PC {
    uint drawIndex;      // Shadow layer index
    uint materialIndex;
    uint skeletalIndex;
    uint flags;
    mat4 modelMatrix;    // Unused for indirect draws
} pc;

// ── Set 0: Per-frame — shadow view-projection matrices ─────────────────────────
struct ShadowView {
    mat4 viewProj;
};

layout(set = 0, binding = 3, std430) restrict readonly buffer ShadowViewsSSBO {
    ShadowView shadowViews[];
};

// ── Set 3: DrawInfo SSBO (model matrices per draw call) ────────────────────────
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

layout(set = 3, binding = 1, std430) restrict readonly buffer DrawInfoBuffer {
    DrawInfo drawInfos[];
};

void main()
{
    mat4 model    = drawInfos[gl_DrawID].modelMatrix;
    mat4 lightVP  = shadowViews[pc.drawIndex].viewProj;
    gl_Position   = lightVP * model * vec4(aPos, 1.0);
}
