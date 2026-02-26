/* Start Header ************************************************************************/
/*!
\file       AssetRegistry.cpp
\author     Wee Hong RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       8/10/2025
\brief      This file contains the definition of the AssetRegistry class.
			Used for mapping between GUIDs and File Paths.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "AssetRegistry.h"

void Ermine::AssetRegistry::Register(EntityID e, Guid g)
{
	if (auto it = m_EntityToGuid.find(e); it != m_EntityToGuid.end())
		m_GuidToEntity.erase(it->second);
	if (auto it = m_GuidToEntity.find(g); it != m_GuidToEntity.end())
		m_EntityToGuid.erase(it->second);

	m_EntityToGuid[e] = g;
	m_GuidToEntity[g] = e;
}

void Ermine::AssetRegistry::Unregister(EntityID e)
{
	if (auto it = m_EntityToGuid.find(e); it != m_EntityToGuid.end()) {
		m_GuidToEntity.erase(it->second);
		m_EntityToGuid.erase(it);
	}
}

Ermine::EntityID Ermine::AssetRegistry::FindEntity(Guid g) const
{
	auto it = m_GuidToEntity.find(g);
	return it == m_GuidToEntity.end() ? 0 : it->second;
}

Ermine::Guid Ermine::AssetRegistry::FindGuid(EntityID e) const
{
	auto it = m_EntityToGuid.find(e);
	return it == m_EntityToGuid.end() ? Guid{} : it->second;
}

void Ermine::AssetRegistry::Clear()
{
	m_GuidToEntity.clear();
	m_EntityToGuid.clear();
}
