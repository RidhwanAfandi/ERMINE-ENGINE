/* Start Header ************************************************************************/
/*!
\file       Component.tpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Sep 15, 2024
\brief      To manage all the the different components that needed to be added or removed.

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include <cassert>
#include <ranges>
#include <utility>

#include "Component.h"

namespace Ermine {
	template <typename, typename=void> struct has_ser  : std::false_type {};
	template <typename T>
	struct has_ser<T, std::void_t<
	  decltype(std::declval<const T&>().Serialize(
		  std::declval<rapidjson::Value&>(),
		  std::declval<rapidjson::Document::AllocatorType&>()))>> : std::true_type {};

	template <typename, typename=void> struct has_deser : std::false_type {};
	template <typename T>
	struct has_deser<T, std::void_t<
	  decltype(std::declval<T&>().Deserialize(std::declval<const rapidjson::Value&>()))>> : std::true_type {};

	/**
	 * @brief Get the component array of type T
	 */
	template <typename T>
	std::shared_ptr<ComponentArray<T>> ComponentManager::GetComponentArray()
	{
		auto typeIdx = std::type_index(typeid(T));
		// auto itName = m_TypeIndexToName.find(typeIdx);
		auto itId = m_TypeIndexToID.find(typeIdx);
		assert(itId != m_TypeIndexToID.end() && "Component type not registered before use!");
		ComponentTypeID id = itId->second;
		auto ptr = m_ArraysByTypeID[id];
		assert(ptr && "Component array not found for registered component type.");
		return std::static_pointer_cast<ComponentArray<T>>(ptr);
	}

	/**
	 * @brief Register a component
	 */
	template <typename T>
	void ComponentManager::RegisterComponent()
	{
		RegisterComponent<T>(typeid(T).name());
	}

	template <typename T>
	void ComponentManager::RegisterComponent(std::string_view customName)
	{
		std::string nameStr{customName};
		auto typeIdx = std::type_index(typeid(T));

		assert(!m_ComponentTypes.contains(nameStr)&& "Registering component name more than once!");
		assert(!m_TypeIndexToName.contains(typeIdx)&& "Registering component type more than once!");
		assert(m_NextComponentType < MAX_COMPONENTS && "Exceeded maximum number of component types!");

		const ComponentTypeID id = m_NextComponentType++;

		auto arr = std::make_shared<ComponentArray<T>>();

		m_ComponentTypes.insert({nameStr,id});
		m_ComponentArrays.insert({nameStr, arr});
		m_TypeIndexToName.insert({typeIdx,nameStr});
		m_TypeIndexToID.insert({typeIdx,id});
		m_ArraysByTypeID[id] = arr;

		auto& d   = m_Descriptors[nameStr];
		d.name    = nameStr;
		d.typeID  = id;
		d.size    = sizeof(T);
		d.has     = [this](EntityID e){ return this->HasComponent<T>(e); };
		d.clone   = [this](EntityID src, EntityID dst){
			if (!this->HasComponent<T>(src)) return;
			const T& s = this->GetComponent<T>(src);
			this->AddComponent<T>(dst, s);
		};

		// Optional hooks auto-wired if T provides them
		if constexpr (has_ser<T>::value) {
			d.serialize = [this](EntityID e, rapidjson::Value& out, rapidjson::Document::AllocatorType& a){
				const T& c = this->GetComponent<T>(e);
				c.Serialize(out, a);
			};
		}
		if constexpr (has_deser<T>::value) {
			d.deserialize = [this](EntityID e, const rapidjson::Value& in){
				if (!this->HasComponent<T>(e)) this->AddComponent<T>(e, T{});
				auto& c = this->GetComponent<T>(e);
				c.Deserialize(in);
			};
		}
	}

	template <typename T, typename CloneFn>
	void ComponentManager::RegisterComponent(std::string_view customName, CloneFn customClone)
	{
		std::string nameStr(customName);
		auto typeIdx = std::type_index(typeid(T));

		assert(m_ComponentTypes.find(nameStr) == m_ComponentTypes.end() && "Registering component name more than once.");
		assert(m_TypeIndexToName.find(typeIdx) == m_TypeIndexToName.end() && "Registering component type more than once.");
		assert(m_NextComponentType < MAX_COMPONENTS && "Exceeded MAX_COMPONENTS");

		const ComponentTypeID id = m_NextComponentType++;

	    auto arr = std::make_shared<ComponentArray<T>>();

	    m_ComponentTypes.insert({nameStr, id});
	    m_ComponentArrays.insert({nameStr, arr});
	    m_TypeIndexToName.insert({typeIdx, nameStr});
	    m_TypeIndexToID.insert({typeIdx, id});
	    m_ArraysByTypeID[id] = arr;

		ComponentDescriptor desc{
		.name = nameStr,
		.typeID = id,
		.size = sizeof(T),
		.has = [this](EntityID e) { return this->HasComponent<T>(e);},
		.clone = [this, clone = std::move(customClone)](EntityID s, EntityID d) { clone(*this,s,d); }
		};
		m_Descriptors.emplace(nameStr,std::move(desc));
	}

	/**
	 * @brief Get the component type ID of a component
	 */
	template <typename T>
	ComponentTypeID ComponentManager::GetComponentType()
	{
		auto typeIdx = std::type_index(typeid(T));
		auto itName = m_TypeIndexToName.find(typeIdx);
		assert(itName != m_TypeIndexToName.end() && "Component not registered before use!");
		return m_TypeIndexToID[typeIdx];
	}

	/**
	 * @brief Add a component to an entity
	 * @param entity The entity to add the component to
	 * @param component The component to add
	 */
	template <typename T>
	void ComponentManager::AddComponent(EntityID entity, T component)
	{
		GetComponentArray<T>()->InsertData(entity, std::move(component));
	}

	/**
	 * @brief Remove a component from an entity
	 * @param entity The entity to add the component to
	 */
	template <typename T>
	void ComponentManager::RemoveComponent(EntityID entity)
	{
		GetComponentArray<T>()->RemoveData(entity);
	}

	/**
	 * @brief Get a reference to a component of type T for an entity
	 * @param entity The entity to add the component to
	 */
	template <typename T>
	T& ComponentManager::GetComponent(EntityID entity)
	{
		return GetComponentArray<T>()->GetData(entity);
	}

	/**
	 * @brief Try to get a pointer to a component of type T for an entity
	 * @param entity The entity to get the component from
	 * @return Pointer to the component if it exists, nullptr otherwise
	 */
	template <typename T>
	T* ComponentManager::TryGetComponent(EntityID entity)
	{
		auto componentArray = GetComponentArray<T>();
		if (componentArray && componentArray->HasEntity(entity)) {
			return &componentArray->GetData(entity);
		}
		return nullptr;
	}

	template <typename T>
	void ComponentManager::ForEachComponentType(T&& fn) const
	{
		for (const auto& desc : m_Descriptors | std::views::values)
			fn(desc);
	}

}