/* Start Header ************************************************************************/
/*!
\file       lighting_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Deferred lighting pass — fullscreen triangle vertex shader.
            No vertex buffer required; positions are generated from gl_VertexIndex.
            Draw with vkCmdDraw(cmd, 3, 1, 0, 0).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) out vec2 vTexCoord;

void main()
{
    // Generate a clip-space triangle that covers the entire screen.
    // Vertex 0: (-1, -1, 0) → uv (0, 0)
    // Vertex 1: ( 3, -1, 0) → uv (2, 0)
    // Vertex 2: (-1,  3, 0) → uv (0, 2)
    vTexCoord   = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vTexCoord * 2.0 - 1.0, 0.0, 1.0);
}
