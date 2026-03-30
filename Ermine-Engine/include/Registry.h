/* Start Header ************************************************************************/
/*!
\file       Registry.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      A registry where all components are stored.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include <unordered_map>

#include "Entity.h"

namespace Ermine
{
	class IComponentArray
	{
	public:
		virtual ~IComponentArray() = default;
		virtual void EntityDestroyed(EntityID entity) = 0;
	};

	// The component array class that stores the components of type T in a contiguous array.
	template <typename T>
	class ComponentArray : public IComponentArray
	{
	public:
		/**
		 * @brief Insert data into the component array
		 * @param entity The entity to insert the data into
		 * @param component The component data to insert
		 */
		void InsertData(EntityID entity, T component);

		/**
		 * @brief Remove data from the component array
		 * @param entity The entity to remove the data from
		 */
		void RemoveData(EntityID entity);

		/**
		 * @brief Get the data of an entity
		 * @param entity The entity to get the data from
		 * @return The data of the entity
		 */
		T& GetData(EntityID entity);

		/**
		 * @brief Entity destroyed
		 * @param entity The entity that was destroyed
		 */
		void EntityDestroyed(EntityID entity) override;
		
		/**
		 * @brief Check if an entity has a component
		 * @param entity The entity to check
		 * @return True if the entity has the component, false otherwise
		 */
		bool HasEntity(EntityID entity) const {
			return m_EntityToIndexMap.contains(entity);
		}

	private:
		// The packed array of components (of generic type T),
		// set to a specified maximum amount, matching the maximum number
		// of entities allowed to exist simultaneously, so that each entity
		// has a unique spot.
		std::array<T, MAX_ENTITIES> m_ComponentArray;

		// Map from an entity ID to an array index.
		std::unordered_map<EntityID, size_t> m_EntityToIndexMap;

		// Map from an array index to an entity ID.
		std::unordered_map<size_t, EntityID> m_IndexToEntityMap;

		// Total size of valid entries in the array.
		size_t m_Size = 0;
	};
}
#include "Registry.tpp"
// 0x4B45414E
