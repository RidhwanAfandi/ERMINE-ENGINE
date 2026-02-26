/* Start Header ************************************************************************/
/*!
\file       PrefabManager.h
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Sep 10, 2025
\brief      Prefab management including open, save

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "ECS.h"
#include "Serialisation.h"

namespace Ermine
{
	class PrefabManager
	{
	public:
		/*!
		\brief Singleton for PrefabManager
		*/
		static PrefabManager& GetInstance();
		/*!
		\brief Save prefab to file
		*/
		void SavePrefab(EntityID root, const std::filesystem::path& path);
		/*!
		\brief Load prefab from file
		*/
		EntityID LoadPrefab(const std::filesystem::path& path);
	private:
		PrefabManager() = default;
	};
} // namespace Ermine

