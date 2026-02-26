/* Start Header ************************************************************************/
/*!
\file       Cursor.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       29/12/2025
\brief      Cursor API for setting the cursor state in Ermine Engine

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class Cursor
    {
        public enum CursorLockState : int
        {
            None = 0,
            Locked = 1,
            Confined = 2
        }

        public static CursorLockState lockState
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
        }

        public static bool visible
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
        }
    }
}
