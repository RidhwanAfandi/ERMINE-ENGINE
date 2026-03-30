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
    public class AudioComponent : Component
    {
        // Properties to control audio playback
        public bool shouldPlay
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public bool shouldStop
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public bool isPlaying
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float volume
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public string soundName
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public bool playOnStart
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public bool is3D
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float minDistance
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float maxDistance
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public bool followTransform
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        // Reverb properties
        public bool useReverb
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float reverbWetLevel
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float reverbDryLevel
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public float reverbDecayTime
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        // Convenience methods
        public void Play()
        {
            shouldPlay = true;
        }

        public void Stop()
        {
            shouldStop = true;
        }

        /// <summary>
        /// Enable reverb with custom settings
        /// </summary>
        /// <param name="wetLevel">Reverb wet level in dB (-80 to 20)</param>
        /// <param name="dryLevel">Reverb dry level in dB (-80 to 20)</param>
        /// <param name="decayTime">Reverb decay time (0.1 to 20)</param>
        public void SetReverb(float wetLevel = -12.0f, float dryLevel = 0.0f, float decayTime = 1.0f)
        {
            useReverb = true;
            reverbWetLevel = wetLevel;
            reverbDryLevel = dryLevel;
            reverbDecayTime = decayTime;
        }

        /// <summary>
        /// Disable reverb effect
        /// </summary>
        public void ClearReverb()
        {
            useReverb = false;
        }
    }
}