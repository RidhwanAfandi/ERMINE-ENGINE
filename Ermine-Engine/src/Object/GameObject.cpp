/* Start Header ************************************************************************/
/*!
\file       GameObject.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       10/04/2025
\brief      This file contains the declaration of the GameObject class. This class
			represents a game object in the ECS (Entity-Component-System) architecture.
			It provides methods to add, remove, and access components associated with the
			game object. The GameObject class is used to manage the lifecycle and
			behavior of game objects in the engine. Treat it like a wrapper class

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "GameObject.h"

#include "Components.h"

using namespace Ermine::object;

/**
 * @brief Construct a new game object with a default transform component
 */
GameObject::GameObject() : m_Name("GameObject"), m_Tag("Untagged"), m_EntityID(ECS::GetInstance().CreateEntity())
{
	ECS::GetInstance().AddComponent(m_EntityID, Transform(Vec3()));
}

/**
 * @brief Destroy the game object and its components
 */
GameObject::~GameObject()
{
	ECS::GetInstance().DestroyEntity(m_EntityID);
}

/**
 * @brief Construct a new Game Object
 * @param name The name of the game object
 * @param tag The tag of the game object
 */
GameObject::GameObject(const std::string& name, const std::string& tag)
{
	m_Name = name;
	m_Tag = tag;
	m_EntityID = ECS::GetInstance().CreateEntity();
	ECS::GetInstance().AddComponent(m_EntityID, Transform(Vec3()));
}

/**
 * @brief Copy assignment operator
 * @param other The other game object to copy from
 * @return A reference to this game object
 */
//GameObject& GameObject::operator=(const GameObject& other)
//{
//	if (this != &other)
//	{
//		m_Name = other.m_Name;
//		m_Tag = other.m_Tag;
//		m_EntityID = other.m_EntityID;
//		m_IsActive = other.m_IsActive;
//	}
//	return *this;
//}
