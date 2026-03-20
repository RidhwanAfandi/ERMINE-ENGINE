/* Start Header ************************************************************************/
/*!
\file       debug_line_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Debug line fragment shader.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

layout(location = 0) in  vec3 vColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(vColor, 1.0);
}
