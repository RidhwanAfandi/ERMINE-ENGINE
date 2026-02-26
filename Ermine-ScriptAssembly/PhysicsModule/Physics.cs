/* Start Header ************************************************************************/
/*!
\file       Physics.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       04/11/2025
\brief      This file contains the Physics class which serves as a container for physics-related functionalities.
            Global physics properties and helper functions are defined here.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace ErmineEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RaycastHit
    {
        public Vector3 point;      // The impact point in world space where the ray hit the collider.
        public Vector3 normal;     // The normal of the surface the ray hit.
        public float distance;     // The distance from the ray's origin to the impact point.
        //public Collider collider;  // The collider that was hit.
        internal ulong entityID;

        public Transform transform => Physics.Internal_GetTransform(entityID);
        public Rigidbody rigidbody => Physics.Internal_GetRigidbody(entityID);
    }

    public class Physics
    {
        // TODO: Internal call in with internal access modifier
        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool Internal_Raycast(Vector3 origin, Vector3 direction, out RaycastHit hitInfo, float maxDistance);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RemovePhysic(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool HasPhysicComp(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern int CheckMotionType(ulong entityID);

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

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void ForceUpdate();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Transform Internal_GetTransform(ulong id);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Rigidbody Internal_GetRigidbody(ulong id);

        public static bool Raycast(Vector3 origin, Vector3 direction, out RaycastHit hitInfo, float maxDistance)
        {
            hitInfo = new RaycastHit();
            return Internal_Raycast(origin, direction, out hitInfo, maxDistance);
        }
    }
}
