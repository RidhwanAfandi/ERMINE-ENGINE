/* Start Header ************************************************************************/
/*!
\file       NavMeshAgentSystem.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       03/11/2025
\brief      This file declares the NavMeshAgentSystem class, which manages
            navigation agents that move across baked navigation meshes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "ECS.h"
#include "Components.h"
#include "NavMesh.h"
#include "Logger.h"
#include "Renderer.h"

#include <glm/glm.hpp>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>

#include <vector>
#include <cfloat>

namespace Ermine
{
    /*!***********************************************************************
    \brief
     Public convenience function for requesting a navigation path for a given
     agent entity toward a specified destination.

     This function retrieves the NavMeshAgent and Transform components of the
     entity, computes a navigation path using the NavMeshAgentSystem and
     NavMeshSystem, and assigns the resulting path back to the agent.
    \param[in] agentEntity
     The entity ID of the agent requesting a path.
    \param[in] destination
     The target world-space position that the agent should navigate toward.
    \return
     True if a valid path was successfully computed and assigned;
     false otherwise.
    *************************************************************************/
    bool RequestPathForAgent(EntityID agentEntity, const Ermine::Vec3& destination);

    class NavMeshAgentSystem : public System
    {
    public:
        void Init();
        /*!***********************************************************************
        \brief
         Updates all entities with NavMeshAgent components each frame.

         The system moves agents along their computed paths, transitions between
         path corners, and updates their Transform components. It also performs
         debug rendering of paths in the editor when enabled.
        \param[in] dt
         The time elapsed since the previous frame, used for smooth movement
         interpolation.
        \return
         None.
        *************************************************************************/
        void Update(float dt);
        /*!***********************************************************************
        \brief
         Computes a navigation path for the specified agent between a start and end
         position by querying the nearest NavMesh entity’s navigation data.

         Delegates actual path computation to the NavMeshSystem, which uses Detour
         for navigation queries.
        \param[in] agentEntity
         The entity requesting the path.
        \param[in] startPos
         The starting position of the path in world space.
        \param[in] endPos
         The target destination position in world space.
        \param[out] outPath
         A vector that will be filled with a list of world-space waypoints forming
         the computed path.
        \return
         True if a valid path was found; false otherwise.
        *************************************************************************/
        bool FindPath(EntityID agentEntity, const Ermine::Vec3& startPos, const Ermine::Vec3& endPos, std::vector<Ermine::Vec3>& outPath);

        /*!***********************************************************************
        \brief
         Finds the nearest entity in the ECS that has a NavMeshComponent relative
         to a given world-space position.

         This is used to determine which navigation mesh an agent should query when
         performing pathfinding.
        \param[in] pos
         The world-space position to compare against all entities with NavMeshComponents.
        \return
         The entity ID of the nearest entity that has a NavMeshComponent,
         or 0 if none are found.
        *************************************************************************/
        EntityID FindNearestNavMeshEntity(const Ermine::Vec3& pos);
        /*!***********************************************************************
        \brief
         Same Function as before now with 2 more parameters. Use to find nearest Nav
         Mesh for AI to jump.

         This is used to determine which navigation mesh an agent should query when
         performing pathfinding.
        \param[in] pos
         The world-space position to compare against all entities with NavMeshComponents.
        \param[in] exclude
         Current entity that the AI is standing on.
        \param[in] excludePrev
         The previous entity that the AI jumped from.
        \return
         The entity ID of the nearest entity that has a NavMeshComponent,
         or 0 if none are found.
        *************************************************************************/
        EntityID FindNearestNavMeshEntityExcluding(const Ermine::Vec3& pos, EntityID exclude, EntityID excludePrev) const;
    private:
    };
}