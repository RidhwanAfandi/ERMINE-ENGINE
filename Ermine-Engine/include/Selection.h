/* Start Header ************************************************************************/
/*!
\file       Selection.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       20/11/2025
\brief      This files contains the declaration for Selection utilities
			Provides utility functions for getting and setting EntityID on managed objects,

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include <unordered_set>
#include "Entity.h"
#include "Scene.h"
#include "HierarchySystem.h"

namespace Ermine
{
	class Scene;
	class HierarchySystem;
}

namespace Ermine::editor
{
	class Selection
	{
		static inline std::unordered_set<EntityID> s_Selected{};
		static inline std::vector<EntityID> s_TopLevel{};
		static inline EntityID s_Primary = 0;
	public:
		static bool IsSelected(EntityID id) { return s_Selected.contains(id); }
		static const std::unordered_set<EntityID>& All() { return s_Selected; }
		static const std::vector<EntityID>& TopLevel() { return s_TopLevel; }
		static EntityID Primary() { return s_Primary; }
		static bool Multiple() { return s_Selected.size() > 1; }

		static void RebuildTopLevel(const Scene* scene)
		{
			s_TopLevel.clear();
			if (!scene) return;
			auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
			if (!hs) return;

			for (auto id : s_Selected)
			{
				EntityID parent = hs->GetParent(id);
				if (parent == 0 || !s_Selected.contains(parent))
					s_TopLevel.push_back(id);
			}
		}

		static void Clear(Scene* scene)
		{
			s_Selected.clear();
			s_TopLevel.clear();
			s_Primary = 0;
			if (scene) scene->ClearSelection();
		}

		static void SelectSingle(Scene* scene, EntityID id)
		{
			s_Selected.clear();
			if (id != 0) s_Selected.insert(id);
			s_Primary = id;
			RebuildTopLevel(scene);
			if (scene) scene->SetSelectedEntity(id);
		}

		static void Set(Scene* scene, const std::unordered_set<EntityID>& ids)
		{
			s_Selected = ids;
			RebuildTopLevel(scene);
			s_Primary = s_Selected.contains(s_Primary) 
			? s_Primary 
			: s_Selected.empty() ? 0 : *s_Selected.begin();
			if (scene) scene->SetSelectedEntity(s_Primary);
		}

		static void SetSingle(Scene* scene, EntityID id)
		{
			s_Selected.clear();
			if (id != 0) s_Selected.insert(id);
			s_Primary = id;
			RebuildTopLevel(scene);
			if (scene) scene->SetSelectedEntity(id);
		}

		static void Add(Scene* scene, EntityID id)
		{
			if (id == 0) return;
			s_Selected.insert(id);
			s_Primary = id;
			RebuildTopLevel(scene);
			if (scene) scene->SetSelectedEntity(s_Primary);
		}

		static void Toggle(Scene* scene, EntityID id)
		{
			if (id == 0) return;
			auto it = s_Selected.find(id);
			if (it == s_Selected.end())
			{
				s_Selected.insert(id);
				s_Primary = id;
			}
			else
			{
				s_Selected.erase(it);
				if (s_Primary == id)
					s_Primary = s_Selected.empty() ? 0 : *s_Selected.begin();
			}
			RebuildTopLevel(scene);
			if (scene) scene->SetSelectedEntity(s_Primary);
		}
	};
}
