/* Start Header ************************************************************************/
/*!
\file       postprocess_vertex.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Post-process pass — fullscreen triangle vertex shader.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) out vec2 vTexCoord;

void main()
{
    vTexCoord   = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vTexCoord * 2.0 - 1.0, 0.0, 1.0);
}
