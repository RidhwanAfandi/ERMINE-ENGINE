/* Start Header ************************************************************************/
/*!
\file       ECS.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      This file contains the definition of the member function of ECS.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "ECS.h"

#include "GuidRegistry.h"
#include "Components.h"
#include "HierarchySystem.h"
#include "Physics.h"

namespace Ermine
{

	/**
	 * @brief Initialize the ECS
	 */
	void ECS::Init()
	{
		m_ComponentManager = std::make_unique<ComponentManager>();
		m_EntityManager = std::make_unique<EntityManager>();
		m_SystemManager = std::make_unique<SystemManager>();
		m_GuidRegistry = std::make_unique<GuidRegistry>();
	}

	/**
	 * @brief Shutdown the ECS, and release all resources via their destructors
	 */
	void ECS::Shutdown()
	{
		EE_CORE_TRACE("Shutting down ECS...");
		m_SystemManager.reset();
		m_ComponentManager.reset();
		m_EntityManager.reset();
		m_GuidRegistry.reset();
	}

	/**
	 * @brief Create an entity
	 * @return The ID of the created entity
	 */
	EntityID ECS::CreateEntity() const
	{
		EntityID e = m_EntityManager->CreateEntity();
		auto g = Guid::New();
		m_ComponentManager->AddComponent<IDComponent>(e, IDComponent{ g });
		m_GuidRegistry->Register(e, g);
		auto sig = m_EntityManager->GetSignature(e);
		sig.set(m_ComponentManager->GetComponentType<IDComponent>(), true);
		m_EntityManager->SetSignature(e, sig);
		m_SystemManager->EntitySignatureChanged(e, sig);
		return e;
	}

	/**
	 * @brief Destroy an entity
	 * @param entity The entity to destroy
	 */
	void ECS::DestroyEntity(EntityID entity) const
	{
		if (m_ComponentManager->HasComponent<IDComponent>(entity))
			m_GuidRegistry->Unregister(entity);

		m_EntityManager->DestroyEntity(entity);
		m_ComponentManager->EntityDestroyed(entity);
		m_SystemManager->EntityDestroyed(entity);
	}

	/**
	 * @brief Destroy all entities and reclaim IDs
	 * Destroys all entities that have components (notifying components/systems),
	 * then resets the EntityManager to reclaim IDs for entities without components.
	 */
	void ECS::ClearAllEntities()
	{
		for (EntityID e = 0; e < MAX_ENTITIES; ++e)
		{
			if (m_EntityManager->IsEntityAlive(e))
				DestroyEntity(e);
		}

		// Reclaim IDs for entities without components
		ReloadEntityManager();
	}

	EntityID ECS::CloneEntity(EntityID entity) const
	{
		EntityID newEntity = CreateEntity();

		m_ComponentManager->CloneAllComponents(entity, newEntity); // This functionality handles all component cloning, by calling their clone functions

		SignatureID originalSignature = m_EntityManager->GetSignature(entity);
		m_EntityManager->SetSignature(newEntity, originalSignature);

		return newEntity;
		//// 1. Create new entity (without components yet)
		//EntityID newEntity = m_EntityManager->CreateEntity();

		//// 2. CAPTURE world transform BEFORE cloning components
		//bool hasTransform = m_ComponentManager->HasComponent<Transform>(entity);
		//Vec3 worldPos, worldScale;
		//Quaternion worldRot;
		//
		//if (hasTransform)
		//{
		//	auto hierarchySystem = GetSystem<HierarchySystem>();
		//	if (hierarchySystem && m_ComponentManager->HasComponent<HierarchyComponent>(entity))
		//	{
		//		// Use hierarchy system for accurate world transform
		//		worldPos = hierarchySystem->GetWorldPosition(entity);
		//		worldRot = hierarchySystem->GetWorldRotation(entity);
		//		worldScale = hierarchySystem->GetWorldScale(entity);
		//	}
		//	else
		//	{
		//		// Fallback: entity has no hierarchy, use local as world
		//		auto& transform = m_ComponentManager->GetComponent<Transform>(entity);
		//		worldPos = transform.position;
		//		worldRot = transform.rotation;
		//		worldScale = transform.scale;
		//	}
		//}

		//// 3. Clone all components (includes IDComponent with new GUID via custom clone)
		//m_ComponentManager->CloneAllComponents(entity, newEntity);

		//// 4. Unity behavior: Make clone a SIBLING of original (same parent, not child)
		//if (m_ComponentManager->HasComponent<HierarchyComponent>(entity))
		//{
		//	auto& originalHierarchy = m_ComponentManager->GetComponent<HierarchyComponent>(entity);
		//	auto& newHierarchy = m_ComponentManager->GetComponent<HierarchyComponent>(newEntity);
		//	
		//	// Clear any children from cloned hierarchy (Unity doesn't clone children)
		//	newHierarchy.children.clear();
		//	
		//	auto hierarchySystem = GetSystem<HierarchySystem>();
		//	if (hierarchySystem)
		//	{
		//		// Reparent to original's parent (making it a sibling)
		//		if (originalHierarchy.parent != 0)
		//		{
		//			// Set same parent as original, preserving world transform
		//			hierarchySystem->SetParent(newEntity, originalHierarchy.parent, true);
		//		}
		//		else
		//		{
		//			// Original is root, make clone a root too
		//			hierarchySystem->UnsetParent(newEntity);
		//		}
		//		
		//		// Ensure GlobalTransform exists and is initialized
		//		hierarchySystem->EnsureGlobalTransform(newEntity);
		//		hierarchySystem->InitializeEntity(newEntity);
		//	}
		//}

		//// 5. RESTORE world transform after reparenting
		//if (hasTransform)
		//{
		//	auto hierarchySystem = GetSystem<HierarchySystem>();
		//	if (hierarchySystem && m_ComponentManager->HasComponent<HierarchyComponent>(newEntity))
		//	{
		//		// Use hierarchy system to set world transform (auto-calculates local)
		//		hierarchySystem->SetWorldPosition(newEntity, worldPos);
		//		hierarchySystem->SetWorldRotation(newEntity, worldRot);
		//		hierarchySystem->SetWorldScale(newEntity, worldScale);
		//		hierarchySystem->MarkDirty(newEntity);
		//	}
		//	else
		//	{
		//		// No hierarchy, just set local transform
		//		auto& transform = m_ComponentManager->GetComponent<Transform>(newEntity);
		//		transform.position = worldPos;
		//		transform.rotation = worldRot;
		//		transform.scale = worldScale;
		//		transform.isDirty = true;
		//	}
		//}

		//// 6. Handle physics component duplication
		//if (m_ComponentManager->HasComponent<PhysicComponent>(newEntity))
		//{
		//	auto physicsSystem = GetSystem<Physics>();
		//	if (physicsSystem)
		//	{
		//		// Force physics system to rebuild body list to create new body for clone
		//		physicsSystem->UpdatePhysicList();
		//	}
		//}

		//// 7. Update signature
		//SignatureID originalSignature = m_EntityManager->GetSignature(entity);
		//m_EntityManager->SetSignature(newEntity, originalSignature);
		//m_SystemManager->EntitySignatureChanged(newEntity, originalSignature);

		//return newEntity;
	}

	/**
	 * @brief Reload the entity manager
	 */
	void ECS::ReloadEntityManager()
	{
		m_EntityManager.reset(new EntityManager());
		m_GuidRegistry->Clear();
	}

	unsigned long int ECS::GetLivingEntityCount() const
	{
		return m_EntityManager->GetLivingEntityCount();
	}
}
// 0x4B45414E