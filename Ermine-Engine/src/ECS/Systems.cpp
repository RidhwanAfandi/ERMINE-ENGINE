/* Start Header ************************************************************************/
/*!
\file       Systems.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 16, 2024
\brief      This file contains the definition of the member function of SystemManager.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "systems.h"

namespace Ermine
{
	/**
	 * @brief Destroy an entity
	 * @param entity The entity to destroy
	 */
	void SystemManager::EntityDestroyed(EntityID entity) const
	{
		for (auto const& pair : m_Systems)
		{
			auto const& system = pair.second;

			system->m_Entities.erase(entity);
		}
	}

	/**
	 * @brief Notify each system that an entity's signature changed
	 * @param entity The entity whose signature changed
	 * @param entitySignature The new signature of the entity
	 */
	void SystemManager::EntitySignatureChanged(EntityID entity, SignatureID entitySignature)
	{
		// Notify each system that an entity's signature changed
		for (auto const& pair : m_Systems)
		{
			auto const& type = pair.first;
			auto const& system = pair.second;
			auto const& systemSignature = m_Signatures[type];

			// Entity signature matches system signature - insert into set
			if ((entitySignature & systemSignature) == systemSignature)
			{
				system->m_Entities.insert(entity);
			}
			else // Entity signature does not match system signature - erase from set
			{
				system->m_Entities.erase(entity);
			}
		}
	}
}