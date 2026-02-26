/* Start Header ************************************************************************/
/*!
\file       NavAgent.cs
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       03/11/2025
\brief      Provides an interface for C# scripts to command nav mesh agents. It exposes a
            function that sets an agent’s movement destination.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

using System;
using System.Runtime.CompilerServices;

namespace ErmineEngine
{
    public static class NavAgent
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void SetDestination(ulong entityID, Vector3 destination);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void StartJump(ulong agentEntityID, ulong linkEntityID);
    }
}
