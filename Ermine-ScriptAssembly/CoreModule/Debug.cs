/* Start Header ************************************************************************/
/*!
\file       Debug.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       03/09/2025
\brief      This file contains the Debug class which provides logging functionalities for the Ermine Engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class Debug
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogInternal(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogWarningInternal(string message);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void LogErrorInternal(string message);


        public static void Log(object message) => LogInternal(message?.ToString());
        public static void Log(string message) => LogInternal(message);
        public static void LogFormat(string format, params object[] args) =>
            LogInternal(string.Format(format, args));

        public static void LogWarning(object message) => LogWarningInternal(message?.ToString());
        public static void LogWarning(string message) => LogWarningInternal(message);
        public static void LogWarningFormat(string format, params object[] args) =>
            LogWarningInternal(string.Format(format, args));

        public static void LogError(object message) => LogErrorInternal(message?.ToString());
        public static void LogError(string message) => LogErrorInternal(message);
        public static void LogErrorFormat(string format, params object[] args) =>
            LogErrorInternal(string.Format(format, args));
    }
}
