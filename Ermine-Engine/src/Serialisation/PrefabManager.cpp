/* Start Header ************************************************************************/
/*!
\file       PrefabManager.cpp
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Sep 10, 2025
\brief      Prrefab management including open, save

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "PrefabManager.h"

namespace Ermine
{
	PrefabManager& PrefabManager::GetInstance()
	{
		static PrefabManager instance;
		return instance;
	}

	void PrefabManager::SavePrefab(EntityID root, const std::filesystem::path & path)
	{
		SavePrefabToFile(ECS::GetInstance(), root, path);
	}

	EntityID PrefabManager::LoadPrefab(const std::filesystem::path& path)
	{
		return LoadPrefabFromFile(ECS::GetInstance(), path);
	}
}
