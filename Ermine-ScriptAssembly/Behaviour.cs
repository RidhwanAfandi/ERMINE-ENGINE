/* Start Header ************************************************************************/
/*!
\file       Behaviour.cs
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       22/08/2025
\brief      Behaviours are Components that can be enabled or disabled.
            They are used to define the behavior of an Entity in the game world.
            Behaviours can be attached to Entities and can be enabled or disabled at runtime.
            When enabled, they will receive update calls from the Script System.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

namespace ErmineEngine
{
    public abstract class Behaviour : Component
    {
        public bool Enabled { get; private set; } = true;

        internal void InternalSetEnabled(bool value)
        {
            if (Enabled == value) return;
            Enabled = value;
            if(value) OnEnable();
            else OnDisable();
        }

        protected virtual void OnEnable()
        {
        }

        protected virtual void OnDisable()
        {
        }
    }
}
