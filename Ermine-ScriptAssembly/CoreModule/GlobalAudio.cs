/* Start Header ************************************************************************/
/*!
\file       AudioComponent.cs
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       11/11/2025
\brief      Managed AudioComponent wrapper exposing audio playback control to C# scripts.
            Backed by native ECS AudioComponent.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class GlobalAudio
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void PlaySFX(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void PlayMusic(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetMusicVolume(float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetSFXVolume(float volume);
    }
}