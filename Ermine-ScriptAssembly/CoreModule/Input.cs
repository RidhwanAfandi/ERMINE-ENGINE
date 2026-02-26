/* Start Header ************************************************************************/
/*!
\file       Input.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       04/09/2025
\brief      Interface for the input system inside Ermine Engine

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class Input
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetKey(KeyCode key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetKeyDown(KeyCode key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetKeyUp(KeyCode key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetMouseButton(KeyCode key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetMouseButtonDown(KeyCode key);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool InternalGetMouseButtonUp(KeyCode key);

        public static bool GetKey(KeyCode key) => InternalGetKey(key);

        public static bool GetKeyDown(KeyCode key) => InternalGetKeyDown(key);

        public static bool GetKeyUp(KeyCode key) => InternalGetKeyUp(key);

        public static bool GetMouseButton(KeyCode key) => InternalGetMouseButton(key);

        public static bool GetMouseButton(int key) => InternalGetMouseButton((KeyCode)key);

        public static bool GetMouseButtonDown(KeyCode key) => InternalGetMouseButtonDown(key);

        public static bool GetMouseButtonDown(int key) => InternalGetMouseButtonDown((KeyCode)key);

        public static bool GetMouseButtonUp(KeyCode key) => InternalGetMouseButtonUp(key);

        public static Vector3 mousePosition
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
        }

        public static Vector3 mousePositionDelta
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
        }
    }

    // Key code based on GLFW Key codes
    public enum KeyCode : int
    {
        LeftMouseButton = 0,
        RightMouseButton = 1,
        Tab = 258,
        RightArrow = 262,
        LeftArrow = 263,
        DownArrow = 264,
        UpArrow = 265,
        PageUp = 266,
        PageDown = 267,
        Home = 268,
        End = 269,
        Insert = 260,
        Delete = 261,
        Backspace = 259,
        Space = 32,
        Enter = 257,
        Escape = 256,
        Quote = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Semicolon = 59,
        Equal = 61,
        LeftBracket = 91,
        Backslash = 92,
        RightBracket = 93,
        GraveAccent = 96,
        CapsLock = 280,
        ScrollLock = 281,
        NumLock = 282,
        PrintScreen = 283,
        Pause = 284,
        Keypad0 = 320,
        Keypad1 = 321,
        Keypad2 = 322,
        Keypad3 = 323,
        Keypad4 = 324,
        Keypad5 = 325,
        Keypad6 = 326,
        Keypad7 = 327,
        Keypad8 = 328,
        Keypad9 = 329,
        KeypadPeriod = 330,
        KeypadDivide = 331,
        KeypadMultiply = 332,
        KeypadMinus = 333,
        KeypadPlus = 334,
        KeypadEnter = 335,
        KeypadEqual = 336,
        LeftShift = 340,
        LeftControl = 341,
        LeftAlt = 342,
        RightShift = 344,
        RightControl = 345,
        RightAlt = 346,
        Menu = 348,
        Alpha0 = 48,
        Alpha1 = 49,
        Alpha2 = 50,
        Alpha3 = 51,
        Alpha4 = 52,
        Alpha5 = 53,
        Alpha6 = 54,
        Alpha7 = 55,
        Alpha8 = 56,
        Alpha9 = 57,
        A = 65,
        B = 66,
        C = 67,
        D = 68,
        E = 69,
        F = 70,
        G = 71,
        H = 72,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        M = 77,
        N = 78,
        O = 79,
        P = 80,
        Q = 81,
        R = 82,
        S = 83,
        T = 84,
        U = 85,
        V = 86,
        W = 87,
        X = 88,
        Y = 89,
        Z = 90,
        F1 = 290,
        F2 = 291,
        F3 = 292,
        F4 = 293,
        F5 = 294,
        F6 = 295,
        F7 = 296,
        F8 = 297,
        F9 = 298,
        F10 = 299,
        F11 = 300,
        F12 = 301,
    }
}
