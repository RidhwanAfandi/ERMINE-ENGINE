/* Start Header ************************************************************************/
/*!
\file       Component.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       03/09/2025
\brief      Unity‑style base class for all attachable units (data/logic). Provides
            access to owning GameObject, its Transform and tag (backed by native side).
            The owning GameObject reference is injected by GameObject when components
            are created or fetched.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public abstract class Component : Object
    {
        #region Properties
        public GameObject gameObject { get; internal set; }

        public Transform transform => gameObject != null ? gameObject.transform : null;

        public string tag
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }
        #endregion

        #region Public Methods
        // Gets a reference to a component of type T on the same GameObject as the component specified
        public T GetComponent<T>() where T : Component
        {
            return gameObject.GetComponent<T>();
        }
        #endregion
    }
}
