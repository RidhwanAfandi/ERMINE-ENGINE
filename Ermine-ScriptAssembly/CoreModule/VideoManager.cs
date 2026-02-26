/* Start Header ************************************************************************/
/*!
\file       VideoManager.cs
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       01/30/2026
\brief      Managed VideoManager wrapper exposing video playback control to C# scripts.
            Backed by native VideoManager system.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public enum VideoFitMode
    {
        AspectFit = 0,
        StretchToFill = 1
    }

    public static class VideoManager
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_LoadVideo(string name, string filepath, bool loop);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetCurrentVideo(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern string Internal_GetCurrentVideo();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Play();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Pause();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Stop();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsPlaying();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_IsDonePlaying(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_VideoExists(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_FreeVideo(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_CleanupAllVideos();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetFitMode(int mode);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern int Internal_GetFitMode();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetRenderEnabled(bool enabled);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetRenderEnabled();

        public static bool Load(string name, string filepath, bool loop = false)
        {
            if (string.IsNullOrEmpty(name) || string.IsNullOrEmpty(filepath))
            {
                Debug.LogError("VideoManager.Load: name or filepath is null/empty.");
                return false;
            }
            return Internal_LoadVideo(name, filepath, loop);
        }

        public static void SetCurrent(string name)
        {
            if (string.IsNullOrEmpty(name))
            {
                Debug.LogError("VideoManager.SetCurrent: name is null/empty.");
                return;
            }
            Internal_SetCurrentVideo(name);
        }

        public static string GetCurrent()
        {
            return Internal_GetCurrentVideo();
        }

        public static void Play()
        {
            Internal_Play();
        }

        public static void Pause()
        {
            Internal_Pause();
        }

        public static void Stop()
        {
            Internal_Stop();
        }

        public static bool IsPlaying()
        {
            return Internal_IsPlaying();
        }

        public static bool IsDonePlaying(string name)
        {
            if (string.IsNullOrEmpty(name))
                return false;
            return Internal_IsDonePlaying(name);
        }

        public static bool Exists(string name)
        {
            if (string.IsNullOrEmpty(name))
                return false;
            return Internal_VideoExists(name);
        }

        public static void Free(string name)
        {
            if (string.IsNullOrEmpty(name))
                return;
            Internal_FreeVideo(name);
        }

        public static void CleanupAll()
        {
            Internal_CleanupAllVideos();
        }

        public static VideoFitMode GetFitMode()
        {
            return (VideoFitMode)Internal_GetFitMode();
        }

        public static void SetFitMode(VideoFitMode mode)
        {
            Internal_SetFitMode((int)mode);
        }

        public static bool IsRenderEnabled()
        {
            return Internal_GetRenderEnabled();
        }

        public static void SetRenderEnabled(bool enabled)
        {
            Internal_SetRenderEnabled(enabled);
        }
    }
}
