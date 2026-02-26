/* Start Header ************************************************************************/
/*!
\file       AudioListener.cs
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       11/25/2025
\brief      Static AudioListener class for managing 3D audio listener position/orientation.
            
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    /// Controls the 3D audio listener position and orientation for spatial audio.
    public static class AudioListener
    {
        /// Sets the listener's position in world space.
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetPosition(Vector3 position);

        /// Sets the listener's orientation (forward and up vectors).
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetOrientation(Vector3 forward, Vector3 up);

        /// Sets all listener attributes at once (position, velocity, forward, up).
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetAttributes(Vector3 position, Vector3 velocity, Vector3 forward, Vector3 up);
    }
}