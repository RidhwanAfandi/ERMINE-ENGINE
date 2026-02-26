/* Start Header ************************************************************************/
/*!
\file       GameObjectFactory.h
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

#pragma once
#include "PreCompile.h"
#include "GameObject.h"

namespace Ermine::object
{
	class GameObjectFactory
	{
	public:
		static std::list<std::unique_ptr<GameObject>> m_GameObjects; // List of all game objects

		/**
		 * @brief Create a new game object
		 * @param name The name of the game object
		 * @param tag The tag of the game object
		 * @return A pointer to the created game object
		 */
		static GameObject* CreateGameObject(const std::string& name = "GameObject", const std::string& tag = "Untagged");

		/**
		 * @brief Clone a game object
		 * @param game_object The game object to clone
		 * @return A pointer to the cloned game object
		 */
		static GameObject* CloneGameObject(GameObject* game_object);

		/**
		 * @brief Destroy a game object
		 * @param gameObject The game object to destroy
		 */
		static void DestroyGameObject(GameObject* gameObject);
	};
}
