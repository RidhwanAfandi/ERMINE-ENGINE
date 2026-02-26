/* Start Header ************************************************************************/
/*!
\file       GISystem.h
\co-author  Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.
\date       02/01/2026
\brief      Defines GISystem for managing light probe entities.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "Systems.h"
#include "ECS.h"
#include "Components.h"

namespace Ermine
{
	class GISystem : public System
	{
	public:
		/**
		 * @brief Updates GI system state.
		 */
		void Update();

		/**
		 * @brief Rebuilds all light probe volumes (regenerates probe entities).
		 */
		void RebuildAllProbeVolumes();

		/**
		 * @brief Returns a stable view of the current probe entities set.
		 */
		const std::set<EntityID>& GetProbeEntities() const { return m_Entities; }

	};
}
