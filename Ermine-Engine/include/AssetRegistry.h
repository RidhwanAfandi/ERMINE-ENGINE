/* Start Header ************************************************************************/
/*!
\file       AssetRegistry.cpp
\author     Wee Hong RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       8/10/2025
\brief      This file contains the declaration of the AssetRegistry class.
			Used for mapping between GUIDs and File Paths.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "Guid.h"
#include "Entity.h"

namespace Ermine
{
	class AssetRegistry
	{
		std::unordered_map<Guid, EntityID> m_GuidToEntity;
		std::unordered_map<EntityID, Guid> m_EntityToGuid;
	public:
		void Register(EntityID e, Guid g);
		void Unregister(EntityID e);
		EntityID FindEntity(Guid g) const;
		Guid FindGuid(EntityID e) const;
		void Clear();
	};
}
