/* Start Header ************************************************************************/
/*!
\file       NavMesh.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       03/11/2025
\brief      This file declares the NavMeshSystem class, which manages navigation mesh
            data and pathfinding functionality for AI.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Entity.h"
#include "ECS.h"
#include "Components.h"

class rcContext; struct rcHeightfield; struct rcCompactHeightfield;
struct rcContourSet; struct rcPolyMesh; struct rcPolyMeshDetail;
struct duDebugDraw; class dtNavMesh; class dtNavMeshQuery;

namespace Ermine {

    class NavMeshSystem : public System
    {
    public:
        /*!***********************************************************************
        \brief
         Initializes the NavMesh system and prepares debug drawing utilities.
         Registers custom memory allocation hooks for Recast and Detour.
        \return
         None.
        *************************************************************************/
        void Init();
        /*!***********************************************************************
        \brief
         Shuts down the NavMesh system and releases all allocated navigation data.
         Frees memory for both build-time and runtime data for all active entities.
        \return
         None.
        *************************************************************************/
        void Shutdown();

        void FreeAllNavMeshes();
        /*!***********************************************************************
        \brief
         Bakes a simple navigation mesh on top of the given entity, typically used
         for testing or debugging navigation on simple geometry (e.g., cubes).
        \param[in] e
         The entity ID for which to build the navigation mesh.
        \return
         True if the bake was successful, false otherwise.
        *************************************************************************/
        bool BakeNavMesh(EntityID e);
        /*!***********************************************************************
        \brief
         Draws debug visualizations of all existing navigation meshes using OpenGL.
        \return
         None.
        *************************************************************************/
        void DebugDraw();
        /*!***********************************************************************
        \brief
         Highlights specific navigation geometry or paths for debugging purposes.
        \return
         None.
        *************************************************************************/
        void DebugHighLight();
        /*!***********************************************************************
        \brief
         Computes a straight-line path between two world positions on the
         navigation mesh of a given entity.
        \param[in] navEntity
         The entity ID that owns the navigation mesh to query.
        \param[in] start
         The starting position of the path in world space.
        \param[in] end
         The target position of the path in world space.
        \param[out] outPath
         A vector that will be populated with the resulting path points.
        \return
         True if a valid path was successfully computed, false otherwise.
        *************************************************************************/
        bool ComputeStraightPath(EntityID navEntity, const Vec3& start, const Vec3& end, const float extents[3], std::vector<Vec3>& outPath);

        bool ClampToNavMesh(EntityID navEntity, const Vec3& inPos, const float extents[3], Vec3& outPos);
        /*!***********************************************************************
        \brief
         Removes all navigation data associated with the specified entity,
         including build-time and runtime data.
        \param[in] e
         The entity ID whose navigation data should be removed.
        \return
         None.
        *************************************************************************/
        void RemoveEntity(EntityID e);
        /*!***********************************************************************
        \brief
         Destroys and frees all build-time navigation data (heightfield, mesh, etc.)
         for the specified NavMesh component.
        \param[in,out] c
         Reference to the NavMeshComponent whose build data should be destroyed.
        \return
         None.
        *************************************************************************/
        void DestroyBuild(NavMeshComponent& c);
        /*!***********************************************************************
        \brief
         Destroys and frees all runtime navigation data (Detour mesh and query)
         for the specified NavMesh component.
        \param[in,out] c
         Reference to the NavMeshComponent whose runtime data should be destroyed.
        \return
         None.
        *************************************************************************/
        void DestroyRuntime(NavMeshComponent& c);
        // BRIAN COMMENT
        void WarmStartBakedNavMeshes();
        // BRIAN STUPID
        bool CreateRuntimeFromBaked(NavMeshComponent& nm);

    private:
        /*!***********************************************************************
        \brief
         Builds a navigation mesh from a list of triangles and vertices.
         Converts geometry into a usable navigation mesh for AI pathfinding.
        \param[in,out] c
         Reference to the NavMeshComponent being built.
        \param[in] verts
         Pointer to an array of vertex positions (x, y, z) used to construct the mesh.
        \param[in] nverts
         Number of vertices in the mesh.
        \param[in] tris
         Pointer to an array of triangle indices defining the mesh topology.
        \param[in] ntris
         Number of triangles in the mesh.
        \return
         True if the build succeeded, false otherwise.
        *************************************************************************/
        bool BuildFromTriangles(NavMeshComponent& c, const float* verts, int nverts, const int* tris, int ntris);
        duDebugDraw* m_dd = nullptr;
    };
}