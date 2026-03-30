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

        /// <summary>
        /// Play a sound effect with optional reverb
        /// </summary>
        /// <param name="name">Name of the SFX</param>
        /// <param name="useReverb">Enable reverb effect</param>
        /// <param name="wetLevel">Reverb wet level in dB (-80 to 20)</param>
        /// <param name="dryLevel">Reverb dry level in dB (-80 to 20)</param>
        /// <param name="decayTime">Reverb decay time (0.1 to 20)</param>
        /// <param name="earlyDelay">Early reflection delay (0.001 to 0.3), default 0.001 (minimal)</param>
        /// <param name="lateDelay">Late reflection delay (0.001 to 0.1), default 0.001 (minimal)</param>
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void PlaySFXWithReverb(string name, bool useReverb = false, float wetLevel = -12.0f, float dryLevel = 0.0f, float decayTime = 1.0f, float earlyDelay = 0.001f, float lateDelay = 0.001f);

        /// <summary>
        /// Play a sound effect with reverb (convenience method)
        /// </summary>
        /// <param name="name">Name of the SFX</param>
        /// <param name="wetLevel">Reverb wet level in dB (-80 to 20), default -12dB</param>
        /// <param name="decayTime">Reverb decay time (0.1 to 20), default 1.0s</param>
        /// <param name="earlyDelay">Early reflection delay (0.001 to 0.3), default 0.001 (minimal)</param>
        /// <param name="lateDelay">Late reflection delay (0.001 to 0.1), default 0.001 (minimal)</param>
        public static void PlaySFXWithReverbSimple(string name, float wetLevel = -12.0f, float decayTime = 1.0f, float earlyDelay = 0.001f, float lateDelay = 0.001f)
        {
            PlaySFXWithReverb(name, useReverb: true, wetLevel: wetLevel, dryLevel: 0.0f, decayTime: decayTime, earlyDelay: earlyDelay, lateDelay: lateDelay);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void StopSFX(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void PlayMusic(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetMusicVolume(float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetSFXVolume(float volume);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void PlayVoice(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void StopVoice();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetVoiceVolume(float volume);
    }
}