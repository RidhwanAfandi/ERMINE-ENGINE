/* Start Header ************************************************************************/
/*!
\file       Physics.h
\author     Tan Si Han, t.sihan, 2301264, t.sihan\@digipen.edu
\date       Sept 02, 2025
\brief      This file contains the declaration of the Physics structure.

			The Physics class serves as a wrapper around the Jolt Physics
			engine, managing initialization, simulation updates, body creation,
			ECS synchronization, and debug visualization. It integrates with
			the ECS system to keep physics bodies and entities consistent.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>  // ConvexHullShape
#include <Jolt/Physics/Collision/Shape/CompoundShape.h>    // CompoundShape
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>

#include "ECS.h"
#include "Components.h"
#include "GeometryFactory.h"
#include "AssetManager.h"
#include "PhysicDebugRenderer.h"
#include <tuple>
#include <queue>

using namespace JPH;

namespace Ermine
{
	// ------------------------------- Debug Settings -------------------------------
	/*!***********************************************************************
		\brief
			Settings for how bodies are visualized in the physics debug renderer.
	*************************************************************************/
	struct BodyDrawSettings
	{
		bool mDrawShapeWireframe = true;   // toggle wireframe drawing
		bool mDrawInactiveBodies = true;   // draw inactive bodies
		bool mDrawCenterOfMass = false;
		bool mDrawBodyAxes = false;
	};

	struct RaycastHit
	{
		Vec3 point{};
		Vec3 normal{};
		float distance{};
		EntityID entityID{};
	};

	// ------------------------------- Physics Class -------------------------------
	class Physics : public System
	{
	public:
		/*!***********************************************************************
		  \brief
			Constructor. Initializes memory allocator, job system, and prepares
			listeners/filters for the physics system.
		*************************************************************************/
		Physics();

		/*!***********************************************************************
		  \brief
			Destructor. Shuts down the physics system and cleans up resources.
		*************************************************************************/
		~Physics();

		/*!***********************************************************************
		  \brief
			Initializes the Jolt physics system, registers types, sets up gravity,
			listeners, and optimizes the broadphase.
		*************************************************************************/
		void Init();

		/*!***********************************************************************
		  \brief
			Shuts down the physics system and clears entity-to-body mappings.
		*************************************************************************/
		void Shutdown();

		/*!***********************************************************************
		  \brief
			Updates the physics simulation for a single frame and synchronizes ECS
			transforms with physics bodies.
		  \param[in] deltaTime
			Time elapsed since the last frame, in seconds.
		*************************************************************************/
		void Update(float deltaTime);

		/*!***********************************************************************
		  \brief
			Rebuilds the list of physics bodies from ECS entities. Old bodies are
			removed and new shapes are created and registered in the physics world.
		*************************************************************************/
		void UpdatePhysicList();

		/*!***********************************************************************
		  \brief
			Retrieves the physics body ID associated with a given ECS entity.
		  \param[in] objectID
			The ECS entity ID to query.
		  \return
			A valid BodyID if the entity has a physics body, otherwise an invalid BodyID.
		*************************************************************************/
		JPH::BodyID GetBodyID(EntityID objectID);

		/*!***********************************************************************
		  \brief
			Finds the ECS EntityID linked to a physics body.
		  \param[in] bodyID
			The JPH::BodyID to look up.
		*************************************************************************/
		EntityID GetEntityID(JPH::BodyID bodyID);

		/*!***********************************************************************
		  \brief
			Provides access to the Jolt BodyInterface for manual body operations.
		  \return
			Reference to the BodyInterface instance.
		*************************************************************************/
		BodyInterface& GetBodyInterface() { return mPhysicsSystem.GetBodyInterface(); }

		/*!***********************************************************************
		  \brief
			Provides access to the Jolt BodyLockInterface for safe body access.
		  \return
			Reference to the const BodyLockInterface instance.
		*************************************************************************/
		const BodyLockInterface& GetBodyLockInterface() { return mPhysicsSystem.GetBodyLockInterface(); }

		/*!***********************************************************************
		  \brief
			BodyManager DrawSettings setup for draw debug
		*************************************************************************/
		void DrawDebug();

		/*!***********************************************************************
		  \brief
			Draws wireframe
		*************************************************************************/
		void DrawDebugPhysics();

		/*!***********************************************************************
		  \brief
			Connects a debug renderer for visualizing physics.
		  \param[in] 
			renderer Shared pointer to a custom debug renderer.
		*************************************************************************/
		void AttachDebugRenderer(std::shared_ptr<MyDebugRenderer> renderer);

		/*!***********************************************************************
		  \brief
			Enumeration representing collision event type.
		*************************************************************************/
		enum class CollisionEventType : char { Begin, Stay, End };

		/*!***********************************************************************
		  \brief 
			Handles a collision event between two bodies.
		  \param[in] 
			a First body.
		  \param[in] 
			b Second body.
		  \param[in] 
			type Type of collision event (Begin, Stay, or End).
		*************************************************************************/
		void HandleCollisionEvent(const Body& a, const Body& b, CollisionEventType type);

		/*!***********************************************************************
		  \brief
			Simplified version using BodyIDs instead of references.
		*************************************************************************/
		void HandleCollisionEvent(JPH::BodyID a, JPH::BodyID b, CollisionEventType type);
		
		/*!***********************************************************************
		  \brief 
			Performs a single raycast and returns the first intersection.
		  \param[in] 
			origin Start of the ray.
		  \param[in] 
			direction Normalized direction vector.
		  \param[in] 
			maxDistance Maximum distance of the ray.
		  \param[out] 
			outResult Populated with hit data if an intersection occurs.
		  \return 
			True if the ray hit a body, false otherwise.
		*************************************************************************/
		bool Raycast(const JPH::Vec3& origin, const JPH::Vec3& direction, float maxDistance, JPH::RayCastResult& outResult);

		/*!***********************************************************************
		  \brief 
			Performs a raycast and collects all intersections along the ray.
		  \param[in] 
			origin Start of the ray.
		  \param[in] 
			direction Normalized direction vector.
		  \param[in] 
			maxDistance Maximum distance of the ray.
		  \return 
			A vector of all RayCastResults encountered.
		*************************************************************************/
		std::vector<JPH::RayCastResult> RaycastAll(const JPH::Vec3& origin, const JPH::Vec3& direction, float maxDistance);

		/*!***********************************************************************
		  \brief
			Removes all physics bodies currently registered.
		*************************************************************************/
		void ClearPhysicBody();

		/*!***********************************************************************
		  \brief
			Sets the world position of a physics body by entity ID
		*************************************************************************/
		void SetPosition(EntityID ID, Ermine::Vec3 position);

		/*!***********************************************************************
		  \brief
			Sets the world rotation using Euler angles.
		*************************************************************************/
		void SetRotation(EntityID ID, Ermine::Vec3 rotation);

		/*!***********************************************************************
		  \brief
			Sets the world rotation using a quaternion.
		*************************************************************************/
		void SetRotation(EntityID ID, Ermine::Quaternion rotation);

		/*!***********************************************************************
		  \brief
			Gets the world position of a physics body by entity ID
		*************************************************************************/
		Ermine::Vec3 GetPosition(EntityID id);

		/*!***********************************************************************
		  \brief
			Gets the world rotation of a physics body by entity ID
		*************************************************************************/
		Ermine::Quaternion GetRotation(EntityID id);

		/*!***********************************************************************
		  \brief
			Moves the body using (position + rotation in Euler).
		*************************************************************************/
		void Move(EntityID ID, Ermine::Vec3 position, Ermine::Vec3 rotation);

		/*!***********************************************************************
		  \brief
			Moves the body using (position + rotation in Quaternion).
		*************************************************************************/
		void Move(EntityID ID, Ermine::Vec3 position, Ermine::Quaternion rotation);

		void Jump(EntityID ID, float jumpStrength);

		void RemovePhysic(EntityID ID);

		bool HasPhysicComp(EntityID ID);

		void ForceUpdate();

		int GetMotionType(EntityID ID);
		
		// Shared pointer to the debug renderer used for visualizing physics.
		std::shared_ptr<MyDebugRenderer> mDebugRenderer;
		
		// Whether to draw wireframe physics bodies.
		bool wireframe;
	private:

		/*!***********************************************************************
		  \brief
			Converts pending BodyID collision pairs into ECS-level collision events.
		*************************************************************************/
		void FlushPendingPairsToEntityEvents();

		// --- Important: allocator first, job system second, physics system third ---
		JPH::TempAllocatorImpl       mTempAllocator;
		JPH::JobSystemThreadPool     mJobSystem;
		JPH::PhysicsSystem           mPhysicsSystem;

		class BPLayerInterfaceImpl* mBroadPhaseLayerInterface = nullptr;
		class ObjectVsBroadPhaseLayerFilterImpl* mObjectVsBroadPhaseLayerFilter = nullptr;
		class ObjectLayerPairFilterImpl* mObjectLayerPairFilter = nullptr;

		class MyBodyActivationListener* mBodyActivationListener = nullptr;
		class MyContactListener* mContactListener = nullptr;

		//Store entity and bodyID
		std::unordered_map<EntityID, JPH::BodyID> mEntityToBody;

		//Debug Rendering
		JPH::BodyManager::DrawSettings mBodyDrawSettings{};

		//Collision event Queue
		std::queue<std::tuple<CollisionEventType, EntityID, EntityID, bool>> mCollisionEvent;

		struct PendingPair
		{
			CollisionEventType type;
			JPH::BodyID a;
			JPH::BodyID b;
		};
		std::mutex mPendingMutex;
		std::vector<PendingPair> mPendingPairs;
	};
}