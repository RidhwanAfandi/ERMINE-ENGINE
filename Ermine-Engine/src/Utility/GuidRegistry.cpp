/* Start Header ************************************************************************/
/*!
\file       GuidRegistry.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       8/10/2025
\brief      This file contains the declaration of the GuidRegistry class.
			Used for mapping between GUIDs and Entity IDs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "GuidRegistry.h"

void Ermine::GuidRegistry::Register(EntityID e, Guid g)
{
	if (auto it = m_EntityToGuid.find(e); it != m_EntityToGuid.end())
		m_GuidToEntity.erase(it->second);
	if (auto it = m_GuidToEntity.find(g); it != m_GuidToEntity.end())
		m_EntityToGuid.erase(it->second);

	m_EntityToGuid[e] = g;
	m_GuidToEntity[g] = e;
}

void Ermine::GuidRegistry::Unregister(EntityID e)
{
	if (auto it = m_EntityToGuid.find(e); it != m_EntityToGuid.end()) {
		m_GuidToEntity.erase(it->second);
		m_EntityToGuid.erase(it);
	}
}

Ermine::EntityID Ermine::GuidRegistry::FindEntity(Guid g) const
{
	auto it = m_GuidToEntity.find(g);
	return it == m_GuidToEntity.end() ? 0 : it->second;
}

Ermine::Guid Ermine::GuidRegistry::FindGuid(EntityID e) const
{
	auto it = m_EntityToGuid.find(e);
	return it == m_EntityToGuid.end() ? Guid{} : it->second;
}

void Ermine::GuidRegistry::Clear()
{
	m_GuidToEntity.clear();
	m_EntityToGuid.clear();
}
