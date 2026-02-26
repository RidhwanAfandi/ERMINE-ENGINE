/* Start Header ************************************************************************/
/*!
\file       ScriptSystem.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/08/2025
\brief      This files contains the declaration for the Script System. 
			The Script System is responsible for managing the Mono script engine,
			loading scripts, and executing them. It provides an interface for the ECS
			to interact with the scripts, allowing for dynamic behavior in the game.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "Systems.h"
#include "ScriptEngine.h"

namespace Ermine::scripting
{
	class ScriptSystem : public System
	{
		mutable std::vector<std::pair<EntityID, std::string>> m_RestoreList;
	public:
		std::unique_ptr<ScriptEngine> m_ScriptEngine;
		ScriptSystem();
		void Update() const;
		void FixedUpdate() const;

		void PrepareForHotReload() const;
		void FinishHotReload(bool success) const;
		
		// New method: Cleanup all script instances before scene transition
		void CleanupAllScripts() const;
	};
}