/* Start Header ************************************************************************/
/*!
\file       Systems.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      To maintain a record of registered systems and their signatures.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include <PreCompile.h>
#include "Entity.h"      // Entity, Signature

namespace Ermine
{
	/**
	 * @brief A system is a collection of entities that share the same signature.
	 *		This allows you to iterate over all entities that have the same components inside your systems class via inheritance.
	 */
	class System
	{
	public:
		std::set<EntityID> m_Entities;
	};

	class SystemManager
	{
		std::unordered_map<const char*, SignatureID> m_Signatures{};

		std::unordered_map<const char*, std::shared_ptr<System>> m_Systems{};

	public:
		/**
		 * @brief Register a system
		 * @tparam T The system to register
		 * @return shared pointer the registered system
		 */
		template <typename T>
		void RegisterSystem();

		/**
		 * @brief Get a system
		 * @tparam T The system to get
		 * @return shared pointer to the system
		 */
		template <typename T>
		std::shared_ptr<T> GetSystem();

		/**
		 * @brief Set the signature of a system
		 * @tparam T The system to set the signature of
		 * @param signature The signature to set
		 */
		template <typename T>
		void SetSystemSignature(SignatureID signature);

		/**
		 * @brief Notify each system that an entity was destroyed
		 * @param entity The entity that was destroyed
		 */
		void EntityDestroyed(EntityID entity) const;

		/**
		 * @brief Notify each system that an entity's signature changed
		 * @param entity The entity whose signature changed
		 * @param entitySignature The new signature of the entity
		 */
		void EntitySignatureChanged(EntityID entity, SignatureID entitySignature);
	};
}
#include "Systems.tpp"
// 0x4B45414E
