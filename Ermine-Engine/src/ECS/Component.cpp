/* Start Header ************************************************************************/
/*!
\file       Component.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      This file contains the definition of the member function of ComponentManager.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Component.h"

namespace Ermine
{
	/**
	 * @brief Register a component
	 * @param entity The entity to add the component to
	 */
	void ComponentManager::EntityDestroyed(EntityID entity) const
	{
		for (auto const& pair : m_ComponentArrays) // Iterate over all component arrays
		{
			auto const& component = pair.second;

			component->EntityDestroyed(entity);
		}
	}

	bool ComponentManager::HasComponent(EntityID entity, std::string_view name) const
	{
		auto it = m_Descriptors.find(std::string(name));
		if (it == m_Descriptors.end())
			return false;
		return it->second.has(entity);
	}

	std::vector<std::string> ComponentManager::GetComponentNames(EntityID entity) const
	{
		std::vector<std::string> out;
		out.reserve(m_Descriptors.size());
		for (auto const& [name,desc] : m_Descriptors)
		{
			if (desc.has(entity))
				out.emplace_back(name);
		}
		return out;
	}

	void ComponentManager::CloneAllComponents(EntityID src, EntityID dst)
	{
		for (auto& desc : m_Descriptors | std::views::values)
		{
			if (desc.has(src))
				desc.clone(src, dst); // RegisterComponent will have set up the clone function | If there's a custom clone, it will declare in the engine.cpp during registration
		}
	}


	const ComponentDescriptor* ComponentManager::GetDescriptor(std::string_view name) const
	{
		auto it = m_Descriptors.find(std::string(name));
		return it == m_Descriptors.end() ? nullptr : &it->second;
	}
}
// 0x4B45414E