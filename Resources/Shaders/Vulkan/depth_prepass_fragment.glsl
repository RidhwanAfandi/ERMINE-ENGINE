/* Start Header ************************************************************************/
/*!
\file       depth_prepass_fragment.glsl
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Depth pre-pass fragment shader. No colour output — only writes depth.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#version 450

// No colour outputs — depth-only pass.
// The depth attachment is written implicitly via gl_FragDepth (hardware).

void main()
{
    // Intentionally empty.
}
