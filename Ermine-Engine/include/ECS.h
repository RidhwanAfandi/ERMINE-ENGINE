/* Start Header ************************************************************************/
/*!
\file       ECS.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      The ECS will coordinator between the Component, Entity and System managers.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "Component.h"
#include "Systems.h"
#include "GuidRegistry.h"

namespace Ermine
{
	class ECS
	{
		std::unique_ptr<ComponentManager> m_ComponentManager;
		std::unique_ptr<EntityManager> m_EntityManager;
		std::unique_ptr<SystemManager> m_SystemManager;
		std::unique_ptr<GuidRegistry> m_GuidRegistry;

		ECS() = default;
		~ECS() = default;
	public:
		static ECS& GetInstance()
		{
			static ECS instance;
			return instance;
		}
		
		ECS(const ECS&) = delete;
		ECS& operator=(const ECS&) = delete;
		ECS(ECS&&) = delete;
		ECS& operator=(ECS&&) = delete;
		
		/**
		 * @brief Initialize the ECS
		 */
		void Init();

		/**
		 * @brief Shutdown the ECS, and release all resources via their destructors
		 */
		void Shutdown();

		/**
		 * @brief Create an entity
		 * @return The ID of the created entity
		 */
		EntityID CreateEntity() const;

		/**
		 * @brief Reload the entity manager
		 */
		void ReloadEntityManager();

		/**
		 * @brief Destroy an entity
		 * @param entity The entity to destroy
		 */
		void DestroyEntity(EntityID entity) const;

		/**
		 * @brief Destroy all entities and reclaim IDs
		 * Destroys all entities that have components (notifying components/systems),
		 * then resets the EntityManager to reclaim IDs for entities without components.
		 */
		void ClearAllEntities();

		/**
		 * @brief Clone an entity
		 * @param entity The entity to clone
		 * @return The ID of the cloned entity
		 */
		EntityID CloneEntity(EntityID entity) const;

		/**
		 * @brief Get the number of living entities in the ECS
		 * @return The number of living entities
		 */
		unsigned long int GetLivingEntityCount() const;

		/**
		 * @brief Register a component type with the ECS
		 * @tparam T The component type to register
		 */
		template<typename T>
		void RegisterComponent() const;

		/**
		 * @brief Register a component type with a custom name
		 * @tparam T The component type to register
		 * @param customName The custom name to register the component with
		 */
		template<typename T>
		void RegisterComponent(std::string_view customName) const;

		/**
		 * @brief Register a component type with a custom name and a custom clone function
		 * @tparam T The component type to register
		 * @tparam CloneFn The type of the custom clone function
		 * @param customName The custom name to register the component with
		 * @param customClone The custom clone function to use when cloning the component
		 */
		template<typename T, typename CloneFn>
		void RegisterComponent(std::string_view customName, CloneFn customClone) const;

		/**
		 * @brief Add a component to an entity.
		 * @tparam T The component type to add
		 * @param entity The entity to add the component to
		 * @param component The component to add
		 */
		template<typename T>
		void AddComponent(EntityID entity, T component);

		/**
		 * @brief Remove a component from an entity.
		 * @tparam T The component type to remove
		 * @param entity The entity to remove the component from
		 */
		template<typename T>
		void RemoveComponent(EntityID entity) const;

		/**
		 * @brief Get a reference to a component of type T for an entity.
		 * @tparam T The component type to get
		 * @param entity The entity to get the component from
		 * @return A reference to the component
		 */
		template<typename T>
		T& GetComponent(EntityID entity) const;

		/**
		 * @brief Get a component type ID
		 * @tparam T The component type to get the ID of
		 * @return The component type ID
		 */
		template<typename T>
		ComponentTypeID GetComponentType() const;
		
		/**
		 * @brief Register a system with the ECS
		 * @tparam T The system type to register
		 */
		template<typename T>
		void RegisterSystem() const;

		/**
		 * @brief Get a system from the ECS
		 * @tparam T The system type to get
		 * @return A shared pointer to the system
		 */
		template<typename T>
		std::shared_ptr<T> GetSystem() const;

		/**
		 * @brief Set the signature of a system
		 * @tparam T The system type to set the signature of
		 * @param signature The signature to set
		 */
		template<typename T>
		void SetSystemSignature(SignatureID signature) const;
		
		/**
		 * @brief Check if an entity has a component
		 * Manage and verify the relationship between entity and their components within ECS
		 * @tparam T The component type to check for
		 * @param entity The entity to check
		 * @return True if the entity has the component, false otherwise
		 */
		template<typename T>
		bool HasComponent(EntityID entity) const
		{
			return m_ComponentManager->HasComponent<T>(entity);
		}

		/**
		 * @brief Check if an entity has a component by name
		 * Manage and verify the relationship between entity and their components within ECS
		 * @tparam T The component type to check for
		 * @param entity The entity to check
		 * @param name The name of the component to check for
		 * @return True if the entity has the component, false otherwise
		 */
		bool HasComponent(EntityID entity, std::string_view name) const
		{
			return m_ComponentManager->HasComponent(entity, name);
		}

		/**
		 * @brief Get the names of all components attached to an entity
		 * @param entity The entity to get the component names from
		 * @return A vector of component names
		 */
		std::vector<std::string> GetComponentNames(EntityID entity) const
		{
			return m_ComponentManager->GetComponentNames(entity);
		}

		/**
		 * @brief Check if an entity ID is valid
		 * @param entity The entity ID to check
		 * @return True if the entity is valid, false otherwise
		 */
		bool IsEntityValid(EntityID entity) const
		{
			return m_EntityManager->IsEntityAlive(entity);
		}

		GuidRegistry& GetGuidRegistry() const { return *m_GuidRegistry; }

		template<typename Fn>
		void ForEachComponentType(Fn&& fn) const
		{
			m_ComponentManager->ForEachComponentType(std::forward<Fn>(fn));
		}

		const ComponentDescriptor* GetDescriptor(std::string_view name) const
		{
			return m_ComponentManager->GetDescriptor(name);
		}

		void ResyncAllSignaturesFromStorage();
	};
}
#include "ECS.tpp"
// 0x4B45414E
