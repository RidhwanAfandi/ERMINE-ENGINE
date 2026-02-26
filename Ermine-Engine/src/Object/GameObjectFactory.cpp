/* Start Header ************************************************************************/
/*!
\file       GameObjectFactory.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       16/04/2025
\brief      This file contains the declaration of the GameObjectFactory class. This class
			is responsible for creating and managing game objects in the engine. It provides
			methods to create, destroy, and manage game objects and their components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "GameObjectFactory.h"

using namespace Ermine::object;

std::list<std::unique_ptr<GameObject>> GameObjectFactory::m_GameObjects; // List of all game objects

/**
 * @brief Create a new game object
 * @param name The name of the game object
 * @param tag The tag of the game object
 * @return A pointer to the created game object
 */
GameObject* GameObjectFactory::CreateGameObject(const std::string& name, const std::string& tag)
{
	auto gameObject = std::make_unique<GameObject>(name, tag);
	GameObject* rawPointer = gameObject.get();
	m_GameObjects.push_back(std::move(gameObject));
	return rawPointer;
}

//GameObject* GameObjectFactory::CloneGameObject(GameObject* game_object)
//{
//
//}

/**
 * @brief Destroy a game object
 * @param gameObject The game object to destroy
 */
void GameObjectFactory::DestroyGameObject(GameObject* gameObject)
{
	auto it = std::find_if(m_GameObjects.begin(), m_GameObjects.end(),
		[gameObject](const std::unique_ptr<GameObject>& obj) { return obj.get() == gameObject; });

	if (it != m_GameObjects.end())
		m_GameObjects.erase(it);
	else
		EE_CORE_ERROR("GameObject not found in the list");
}
