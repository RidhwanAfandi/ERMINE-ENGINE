/* Start Header ************************************************************************/
/*!
\file       GuidRegistry.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       8/10/2025
\brief      This file contains the declaration of the GuidRegistry class.
			Used for mapping between GUIDs and Entity IDs.

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
	class GuidRegistry
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
