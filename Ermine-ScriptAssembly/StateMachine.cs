/* Start Header ************************************************************************/
/*!
\file       StateMachine.cs
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       21/10/2025
\brief      Provides an interface for C# scripts to trigger state transitions for an
            entity’s Finite State Machine

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class StateMachine
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RequestNextState(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RequestPreviousState(ulong entityID);
    }
}

