/* Start Header ************************************************************************/
/*!
\file       Registry.tpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      A registry where all components are stored.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include <cassert>
#include <utility>

namespace Ermine
{
	/**
	 * @brief Insert data into the component array
	 * @param entity The entity to insert the data into
	 * @param component The component data to insert
	 */
	template <typename T>
	void ComponentArray<T>::InsertData(EntityID entity, T component)
	{
		assert(m_EntityToIndexMap.find(entity) == m_EntityToIndexMap.end() && "Component added to same entity more than once.");

		// Put new entry at end and update the maps
		size_t newIndex = m_Size;
		m_EntityToIndexMap[entity] = newIndex;
		m_IndexToEntityMap[newIndex] = entity;
		m_ComponentArray[newIndex] = std::move(component);
		++m_Size;
	}

	/**
	 * @brief Remove data from the component array
	 * @param entity The entity to remove the data from
	 */
	template <typename T>
	void ComponentArray<T>::RemoveData(EntityID entity)
	{
		assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() && "Removing non-existent component.");

		size_t indexOfRemovedEntity = m_EntityToIndexMap[entity]; // Copy element at end into deleted element's place to maintain density
		size_t indexOfLastElement = m_Size - 1;
		m_ComponentArray[indexOfRemovedEntity] = m_ComponentArray[indexOfLastElement];

		EntityID entityOfLastElement = m_IndexToEntityMap[indexOfLastElement]; // Update map to point to moved spot
		m_EntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
		m_IndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

		m_EntityToIndexMap.erase(entity);
		m_IndexToEntityMap.erase(indexOfLastElement);

		--m_Size;
	}

	/**
	 * @brief Get the data of an entity
	 * @param entity The entity to get the data from
	 * @return The data of the entity
	 */
	template <typename T>
	T& ComponentArray<T>::GetData(EntityID entity)
	{
		auto it = m_EntityToIndexMap.find(entity);
		assert(it != m_EntityToIndexMap.end() && "Retrieving non-existent component.");

		return m_ComponentArray[it->second]; // Return a reference to the entity's component
	}

	/**
	 * @brief Entity destroyed
	 * @param entity The entity that was destroyed
	 */
	template <typename T>
	void ComponentArray<T>::EntityDestroyed(EntityID entity)
	{
		if (m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end())
			RemoveData(entity); // Remove the entity's component if it existed
	}
}