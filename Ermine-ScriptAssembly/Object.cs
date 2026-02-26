/* Start Header ************************************************************************/
/*!
\file       Object.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       03/09/2025
\brief      This file contains the base class for all objects in the Ermine Engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public class Object
    {
        #region Properties

        public string name
        {
            [MethodImpl(MethodImplOptions.InternalCall)]
            get;
            [MethodImpl(MethodImplOptions.InternalCall)]
            set;
        }

        private long EntityID = 0; // Gets updates from Native side (ScriptInstance::InjectEntityIfAvailable)
        #endregion

        #region Public Methods
        public long GetInstanceID() => EntityID;
        public override string ToString() => name ?? base.ToString();

        public override bool Equals(object obj)
        {
            if (ReferenceEquals(this, obj)) return true;
            if (obj is null || obj.GetType() != GetType()) return false;
            return ((Object)obj).EntityID == EntityID;
        }

        public override int GetHashCode() => EntityID.GetHashCode();

        public static bool operator ==(Object lhs, Object rhs)
        {
            if (ReferenceEquals(lhs, rhs)) return true;
            if (lhs is null || rhs is null) return false;
            return lhs.EntityID == rhs.EntityID;
        }

        public static bool operator !=(Object lhs, Object rhs) => !(lhs == rhs);
        #endregion
    }
}
