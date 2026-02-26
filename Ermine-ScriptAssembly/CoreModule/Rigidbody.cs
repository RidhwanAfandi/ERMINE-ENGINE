/* Start Header ************************************************************************/
/*!
\file       Rigidbody.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       16/09/2025
\brief      Managed Rigidbody component API (Unity‑style) exposing position.
            Backed by native ECS Rigidbody (position, velocity, mass, etc).
            Internal calls (get_/set_) must be registered on the C++ side (ScriptEngine::RegisterInternalCalls).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class Rigidbody : Component
    {
        #region Internal Calls Registration
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetPosition(ulong entityID, Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetRotationEuler(ulong entityID, Vector3 eulerDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetRotationQuat(ulong entityID, Quaternion rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void MoveEuler(ulong entityID, Vector3 position, Vector3 eulerDeg);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void MoveQuat(ulong entityID, Vector3 position, Quaternion rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Jump(ulong entityID, float jump);
        #endregion

        /// <summary>
        /// The position of the rigidbody
        /// </summary>
        public Vector3 position
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        /// <summary>
        /// The rotation of the rigidbody
        /// </summary>
        public Quaternion rotation
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        /// <summary>
        /// The linear velocity vector of the rigidbody. It represents the rate of change of Rigidbody position.
        /// </summary>
        public Vector3 linearVelocity
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }
    }
}
