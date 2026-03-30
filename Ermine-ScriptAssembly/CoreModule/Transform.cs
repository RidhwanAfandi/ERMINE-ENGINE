/* Start Header ************************************************************************/
/*!
\file       Transform.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong@digipen.edu
\date       02/09/2025
\brief      Managed Transform component API (Unity‑style) exposing position, rotation (Euler degrees), and scale.
            Backed by native ECS Transform (position, rotation, scale, matrix).
            Internal calls (get_/set_) must be registered on the C++ side (ScriptEngine::RegisterInternalCalls).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class Transform : Component
    {
        #region InternalCalls
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vector3 Internal_GetWorldForward();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vector3 Internal_GetWorldRight();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vector3 Internal_GetWorldUp();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Transform Internal_GetParentTransform();
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Transform Internal_GetChildTransformByName(string n);
        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Transform Internal_GetChildTransformByIndex(int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_AddChild(Transform child);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern void Internal_RemoveChild(Transform child);

        #endregion 

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vector3 Internal_GetWorldPosition();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Quaternion Internal_GetWorldRotation();

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern Vector3 Internal_GetWorldScale();

        public Vector3 worldPosition => Internal_GetWorldPosition();
        public Quaternion worldRotation => Internal_GetWorldRotation();
        public Vector3 worldScale => Internal_GetWorldScale();

        // World position in Vector3
        public Vector3 position
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        // World rotation in Quaternion
        public Quaternion rotation
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        // World scale in Vector3
        public Vector3 scale
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        public int childCount
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
        }

        public Vector3 eulerAngles => rotation.eulerAngles;

        public Transform parent
        {
            get
            {
                Transform result = Internal_GetParentTransform();
                if(result == null)
                    Debug.LogWarning($"Transform.parent returned null for entity {gameObject?.name ?? "unknown"}");
                return result;
            }

        }

        public Vector3 forward => Internal_GetWorldForward();
        public Vector3 right => Internal_GetWorldRight();
        public Vector3 up => Internal_GetWorldUp();

        public void Translate(Vector3 delta) => position += delta;

        public void Rotate(Vector3 deltaEuler)
        {
            Quaternion deltaRotation = Quaternion.Euler(deltaEuler.x, deltaEuler.y, deltaEuler.z);
            rotation *= deltaRotation;
        }

        public void LookAt(Vector3 target)
        {
            Vector3 dir = (target - position).normalized;
            if (dir.SqrMagnitude < 1e-8f) return;

            float yaw = (float)System.Math.Atan2(dir.x, dir.z) / Mathf.Deg2Rad;
            float pitch = (float)System.Math.Asin(-dir.y) / Mathf.Deg2Rad;

            // Persist the roll (z) component of the current rotation
            Quaternion q = rotation;
            Vector3 currentEuler = q.eulerAngles;
            q.eulerAngles = new Vector3(pitch, yaw, currentEuler.z);
            rotation = q.normalized;
        }

        public Transform Find(string n)
        {
            if (string.IsNullOrEmpty(n))
            {
                Debug.LogWarning("Transform.Find called with null or empty name");
                return null;
            }

            Transform result = Internal_GetChildTransformByName(n);
            if(result == null)
                Debug.LogWarning($"Transform.Find: Child '{n}' not found!");
            return result;
        }

        public Transform GetChild(int index)
        {
            if (index < 0 || index >= childCount)
            {
                Debug.LogError($"Transform.GetChild: Index {index} out of range [0, {childCount})");
                return null;
            }

            Transform result = Internal_GetChildTransformByIndex(index);
            if(result == null)
                Debug.LogError($"Transform.GetChild: Native call returned null!");
            return result;
        }

        public void AddChild(Transform child)
        {
            if (child == null)
            {
                Debug.LogWarning("Transform.AddChild called with null child");
                return;
            }

            if (child == this)
            {
                Debug.LogWarning("Transform.AddChild: Cannot add transform as its own child");
                return;
            }

            Internal_AddChild(child);
        }
        public void RemoveChild(Transform child)
        {
            if (child == null)
            {
                Debug.LogWarning("Transform.RemoveChild called with null child");
                return;
            }

            if (child == this)
            {
                Debug.LogWarning("Transform.RemoveChild: Cannot remove self as child");
                return;
            }

            Internal_RemoveChild(child);
        }

    }
}
