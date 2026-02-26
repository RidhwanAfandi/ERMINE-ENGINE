/* Start Header ************************************************************************/
/*!
\file       Application.cs
\author     Claude Code
\date       11/2025
\brief      Application class for application-level control from C# scripts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class Application
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void QuitInternal();

        /// <summary>
        /// Quits the application.
        /// Note: This does not work in the editor, only in builds.
        /// </summary>
        public static void Quit()
        {
            Debug.Log("Application.Quit() called");
            QuitInternal();
        }
    }
}
