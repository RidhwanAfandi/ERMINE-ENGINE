/* Start Header ************************************************************************/
/*!
\file       GameObject.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       10/04/2025
\brief      This file contains the declaration of the GameObject class. This class
			represents a game object in the ECS (Entity-Component-System) architecture.
			It provides methods to add, remove, and access components associated with the
			game object. The GameObject class is used to manage the lifecycle and
			behavior of game objects in the engine. Treat it like a wrapper class

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "ECS.h"

namespace Ermine::object
{
	class GameObject
	{
		std::string m_Name;
		std::string m_Tag;

		EntityID m_EntityID = 0;

		bool m_IsActive = true;
	public:
		/**
		 * @brief Construct a new game object with a default transform component
		 */
		GameObject();
		/**
		 * @brief Destroy the game object and its components
		 */
		~GameObject();
		/**
		 * @brief Construct a new Game Object
		 * @param name The name of the game object
		 * @param tag The tag of the game object
		 */
		GameObject(const std::string& name, const std::string& tag);

		GameObject(const GameObject&) = delete;
		GameObject(GameObject&&) = delete;

		GameObject& operator=(const GameObject&) = delete;
		GameObject& operator=(GameObject&&) = delete;

	public:
		/**
		 * @brief Add a component to the game object
		 * @tparam T The component type to add
		 * @param component The component to add
		 */
		template<typename T>
		void AddComponent(T component) { if (m_EntityID == 0) return; ECS::GetInstance().AddComponent<T>(m_EntityID, std::move(component)); }

		/**
		 * @brief Get a component from the game object
		 * @tparam T The component type to get
		 * @return A reference to the component
		 */
		template<typename T>
		T& GetComponent() { if (m_EntityID == 0) return T(); return ECS::GetInstance().GetComponent<T>(m_EntityID); }

		/**
		 * @brief Remove a component from the game object
		 * @tparam T The component type to remove
		 */
		template<typename T>
		void RemoveComponent() const { if (m_EntityID == 0) return; ECS::GetInstance().RemoveComponent<T>(m_EntityID); }

		/**
		 * @brief Check if the game object has a component of type T
		 * @tparam T The component type to check
		 * @return True if the game object has the component, false otherwise
		 */
		template<typename T>
		[[nodiscard]] bool HasComponent() const { if (m_EntityID == 0) return false; return ECS::GetInstance().HasComponent<T>(m_EntityID); }

	public:
		/**
		 * @brief Set the name of the game object
		 * @param name The name to set
		 * @return The name of the game object
		 */
		void SetName(const std::string& name) { m_Name = name; }
		/**
		 * @brief Get the name of the game object
		 * @return The name of the game object
		 */
		[[nodiscard]] const std::string& GetName() const { return m_Name; }
		/**
		 * @brief Set the tag of the game object
		 * @param tag The tag to set
		 * @return The tag of the game object
		 */
		void SetTag(const std::string& tag) { m_Tag = tag; }
		/**
		 * @brief Get the tag of the game object
		 * @return The tag of the game object
		 */
		[[nodiscard]] const std::string& GetTag() const { return m_Tag; }
		/**
		 * @brief Get the entity ID of the game object
		 * @return The entity ID of the game object
		 */
		[[nodiscard]] EntityID GetEntityID() const { return m_EntityID; }

		/**
		 * @brief Check if the game object is active
		 * @return True if the game object is active, false otherwise
		 */
		[[nodiscard]] bool IsActive() const { return m_IsActive; }

		void SetActive(bool isActive) { m_IsActive = isActive; }
	};
}
