/* Start Header ************************************************************************/
/*!
\file       Collider.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       21/09/2025
\brief      This file contains the Collider class which serves as a base class for all collider components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace ErmineEngine
{
    public class Collider : Component
    {
        public bool IsTrigger
        {
            [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.InternalCall)]
            get;
            [System.Runtime.CompilerServices.MethodImpl(System.Runtime.CompilerServices.MethodImplOptions.InternalCall)]
            set;
        }
    }

    public class Collision
    {
        public readonly Collider collider; // The collider we hit
        public readonly GameObject gameObject; // The gameObject whose collider you are colliding with.
        public readonly Rigidbody rigidbody; // The Rigidbody we hit. This is null if the object we hit is a collider with no rigidbody attached.
        public readonly Transform transform; // The Transform of the object we hit.
    }
}
