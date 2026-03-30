/* Start Header ************************************************************************/
/*!
\file       PostEffects.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       25/02/2026
\brief      This file contains APIs related to post-processing effects in the Ermine Engine.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class PostEffects
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetExposure(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetContrast(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetSaturation(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGamma(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetBloomStrength(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGrainIntensity(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetGrainSize(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetChromaticAberrationIntensity(float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetVignetteEnabled();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteEnabled(bool enabled);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetFlimGrainEnabled();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetFlimGrainEnabled(bool enabled);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetChromaticAberrationEnabled();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetChromaticAberrationEnabled(bool enabled);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetBloomEnabled();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetBloomEnabled(bool enabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetVignetteIntensity();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteIntensity(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetVignetteRadius();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteRadius(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetVignetteCoverage();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteCoverage(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetVignetteFalloff();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteFalloff(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetVignetteMapStrength();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetVignetteMapStrength(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetVignetteMapPath();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVignetteMapPath(string path);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Vector3 Internal_GetVignetteMapRGBModifier();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetVignetteMapRGBModifier(Vector3 value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool GetRadialBlurEnabled();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetRadialBlurEnabled(bool enabled);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern float GetRadialBlurStrength();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetRadialBlurStrength(float value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int GetRadialBlurSamples();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void SetRadialBlurSamples(int value);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Vector2 Internal_GetRadialBlurCenter();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetRadialBlurCenter(Vector2 value);

        public static bool EnableVignette
        {
            get => GetVignetteEnabled();
            set => SetVignetteEnabled(value);
        }

        public static bool EnableFlimGrain
        {
            get => GetFlimGrainEnabled();
            set => SetFlimGrainEnabled(value);
        }

        public static bool EnableChromaticAberration
        {
            get => GetChromaticAberrationEnabled();
            set => SetChromaticAberrationEnabled(value);
        }

        public static bool EnableBloom
        {
            get => GetBloomEnabled();
            set => SetBloomEnabled(value);
        }

        public static float Exposure
        {
            set => SetExposure(value);
        }

        public static float Contrast
        {
            set => SetContrast(value);
        }

        public static float Saturation
        {
            set => SetSaturation(value);
        }

        public static float Gamma
        {
            set => SetGamma(value);
        }

        public static float VignetteIntensity
        {
            get => GetVignetteIntensity();
            set => SetVignetteIntensity(value);
        }

        public static float VignetteRadius
        {
            get => GetVignetteRadius();
            set => SetVignetteRadius(value);
        }

        public static float VignetteCoverage
        {
            get => GetVignetteCoverage();
            set => SetVignetteCoverage(value);
        }

        public static float VignetteFalloff
        {
            get => GetVignetteFalloff();
            set => SetVignetteFalloff(value);
        }

        public static float VignetteMapStrength
        {
            get => GetVignetteMapStrength();
            set => SetVignetteMapStrength(value);
        }

        public static string VignetteMapPath
        {
            get => Internal_GetVignetteMapPath();
            set => Internal_SetVignetteMapPath(value);
        }

        public static Vector3 VignetteMapRGBModifier
        {
            get => Internal_GetVignetteMapRGBModifier();
            set => Internal_SetVignetteMapRGBModifier(value);
        }

        public static bool EnableRadialBlur
        {
            get => GetRadialBlurEnabled();
            set => SetRadialBlurEnabled(value);
        }

        public static float RadialBlurStrength
        {
            get => GetRadialBlurStrength();
            set => SetRadialBlurStrength(value);
        }

        public static int RadialBlurSamples
        {
            get => GetRadialBlurSamples();
            set => SetRadialBlurSamples(value);
        }

        public static Vector2 RadialBlurCenter
        {
            get => Internal_GetRadialBlurCenter();
            set => Internal_SetRadialBlurCenter(value);
        }

        public static float BloomStrength
        {
            set => SetBloomStrength(value);
        }

        public static float GrainIntensity
        {
            set => SetGrainIntensity(value);
        }

        public static float GrainSize
        {
            set => SetGrainSize(value);
        }

        public static float ChromaticAberrationIntensity
        {
            set => SetChromaticAberrationIntensity(value);
        }

        public static void SetVignetteMapTexture(string path)
        {
            VignetteMapPath = path;
        }

        public static void ClearVignetteMapTexture()
        {
            VignetteMapPath = string.Empty;
        }

        public static void SetVignetteMapRGBModifier(Vector3 value)
        {
            VignetteMapRGBModifier = value;
        }
    }
}
