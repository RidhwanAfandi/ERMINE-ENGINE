/* Start Header ************************************************************************/
/*!
\file       GameObject.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       02/09/2025
\brief      This file contains the GameObject class which serves as a fundamental entity in the Ermine Engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class GameObject : Object
    {
        Transform _transform;

        #region Constructors

        public GameObject(string name = "GameObject")
        {
            Internal_CreateGameObject(this,name);
        }

        internal GameObject(ulong entityID)
        {
            if (!Internal_BindExisting(entityID))
                throw new System.ArgumentException("Invalid or dead entityID.", nameof(entityID));
        }

        internal static GameObject FromEntityID(ulong entityID) => Internal_WrapExisting(entityID);

        //public GameObject()
        //{
        //    Internal_CreateGameObject(this,"GameObject");
        //}

        //public GameObject(string name)
        //{
        //    Internal_CreateGameObject(this, name ?? "GameObject");
        //}
        #endregion

        #region Core Properties

        public Transform transform
        {
            get
            {
                if (_transform == null)
                {
                    _transform = Internal_GetTransform(this);
                    if (_transform != null)
                        _transform.gameObject = this; // Ensure the Transform's gameObject reference is set
                }
                return _transform;
            }
        }

        public bool activeSelf => Internal_GetActive(this);
        #endregion

        #region Activation

        public void SetActive(bool value) => Internal_SetActive(this, value);
        #endregion

        #region Component API

        public T AddComponent<T>() where T : Component => (T)AddComponent(typeof(T));

        public Component AddComponent(Type type)
        {
            if (type == null) throw new ArgumentNullException(nameof(type));
            if (!typeof(Component).IsAssignableFrom(type))
                throw new ArgumentException("Type must derive from Component", nameof(type));
            return Internal_AddComponent(this, type);
        }

        public T GetComponent<T>() where T : Component => (T)GetComponent(typeof(T));

        public Component GetComponent(Type type)
        {
            if (type == null)
            {
                Debug.LogError("Null exception on get component!");
                throw new ArgumentNullException(nameof(type));
            }
            return Internal_GetComponent(this, type);
        }

        public bool HasComponent<T>() where T : Component =>
            HasComponent(typeof(T));

        public bool HasComponent(Type type)
        {
            if (type == null) throw new ArgumentNullException(nameof(type));
            return Internal_HasComponent(this, type);
        }

        public void RemoveComponent<T>() where T : Component =>
            RemoveComponent(typeof(T));

        public void RemoveComponent(Type type)
        {
            if (type == null) throw new ArgumentNullException(nameof(type));
            Internal_RemoveComponent(this, type);
            if (type == typeof(Transform))
                _transform = null; 
        }
        #endregion

        #region Find / Instantiate / Destroy
        public static GameObject Find(string name) => Internal_FindByName(name);
        public static GameObject FindWithTag(string tag) => Internal_FindWithTag(tag);
        public static GameObject[] FindGameObjectsWithTag(string tag) => Internal_FindGameObjectsWithTag(tag);

        public static GameObject Instantiate(GameObject original)
        {
            if (original == null) throw new ArgumentNullException(nameof(original));
            return Internal_Instantiate(original);
        }

        public static void Destroy(GameObject target)
        {
            if (target == null) return;
            Internal_Destroy(target);
        }
        #endregion

        #region Internal Calls

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern GameObject Internal_WrapExisting(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal extern bool Internal_BindExisting(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_CreateGameObject(GameObject self, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Transform Internal_GetTransform(GameObject self);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_GetActive(GameObject self);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_SetActive(GameObject self, bool value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component Internal_AddComponent(GameObject self, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern Component Internal_GetComponent(GameObject self, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern bool Internal_HasComponent(GameObject self, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_RemoveComponent(GameObject self, Type componentType);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject Internal_FindByName(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject Internal_FindWithTag(string tag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject[] Internal_FindGameObjectsWithTag(string tag);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern GameObject Internal_Instantiate(GameObject original);

        [MethodImpl(MethodImplOptions.InternalCall)]
        private static extern void Internal_Destroy(GameObject target);

        #endregion
    }
}
