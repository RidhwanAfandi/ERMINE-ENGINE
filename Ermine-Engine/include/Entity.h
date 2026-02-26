/* Start Header ************************************************************************/
/*!
\file       Entity.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      The ECS that in charge of distributing entity IDs and keeping record of which IDs are in use and which are not.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "PreCompile.h"

namespace Ermine
{
	using EntityID = unsigned long long int;       // A type alias representing Entity type (Their ID)
	using ComponentTypeID = unsigned char; // A type alias representing Component type (Their ID)
	constexpr EntityID MAX_ENTITIES = 2501;   // Maximum number of entities that can be registered TODO: Here's to increase entities count
	constexpr ComponentTypeID MAX_COMPONENTS = 255; // Maximum number of components that can be registered TODO: Here's to increase components amount, unsigned char is 8-bit indexing up to 255 components, if needed change componentID to uint16_t for up to 256+
	using SignatureID = std::bitset<MAX_COMPONENTS>; // a type alias representing components bit signature (0x111 to represent 3 components)

	// Manage the Entities
	class EntityManager
	{
		std::queue<EntityID> m_AvailableEntities{};           // Queue of unused entity IDs
		std::array<SignatureID, MAX_ENTITIES> m_Signatures{}; // Array of signatures where the index corresponds to the entity ID

		unsigned long int m_ulLivingEntityCount{};

	public:
		EntityManager()
		{
			for (std::size_t entity = 1; entity < MAX_ENTITIES; ++entity) // Reserve 0 as invalid/null. Valid IDs are [1...MAX_ENTITIES-1]
				m_AvailableEntities.push(entity);
		}

		/**
		 * @brief Create an entity
		 * @return The ID of the created entity
		 */
		EntityID CreateEntity();
		
		/**
		 * @brief Destroy an entity
		 * @param entity The entity to destroy
		 */
		void DestroyEntity(EntityID entity);
		
		/**
		 * @brief Set the signature of an entity
		 * @param entity The entity to set the signature of
		 * @param signature The signature to set
		 */
		void SetSignature(EntityID entity, SignatureID signature);
		
		/**
		 * @brief Get the signature of an entity
		 * @param entity The entity to get the signature of
		 * @return The signature of the entity
		 */
		SignatureID GetSignature(EntityID entity) const;
		
		/**
		 * @brief Get the number of living entities
		 * @return The number of living entities
		 */
		unsigned long int GetLivingEntityCount() const;

		/**
		 * @brief Check if an entity is currently alive/valid
		 * @param entity The entity to check
		 * @return True if the entity is alive, false otherwise
		 */
		bool IsEntityAlive(EntityID entity) const;
	};
};
// 0x4B45414E
