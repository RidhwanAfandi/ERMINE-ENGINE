/* Start Header ************************************************************************/
/*!
\file       AnimationManager.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\co-author  Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\date       28/02/2026
\brief      This file contains the definition of the animation manager.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AnimationManager.h"
#include "Components.h"
#include "Renderer.h"

namespace Ermine::graphics
{
	/**
	 * @brief Update all entities with AnimationComponent.
	 *
	 * Multiple instances of the same model can have independent skeletal animation states:
	 * - Each entity has its own AnimationComponent with its own Animator instance
	 * - Each entity gets its own bone transform offset in the SkeletalSSBO
	 * - Bone transforms are updated independently per entity
	 *
	 * @param deltaTime Frame time step in seconds
	 */
	void AnimationManager::Update(double deltaTime)
	{
		auto& ecs = ECS::GetInstance();

		for (auto& entity : m_Entities)
		{
			if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
			{
				const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
				if (!meta.selfActive)
					continue;
			}
			// Check if animation and model components exist
			if (!ecs.HasComponent<AnimationComponent>(entity)
				|| !ecs.HasComponent<ModelComponent>(entity))
				continue;

			auto& animComp = ecs.GetComponent<AnimationComponent>(entity);
			auto& modelComp = ecs.GetComponent<ModelComponent>(entity);

			// Check if model exists and is valid
			if (!modelComp.m_model || !modelComp.m_model->GetAssimpScene())
				continue;

			// Check if animator exists
			if (!animComp.m_animator)
				continue;

			// Initialize animation graph on first update
			if (!animComp.initialized)
			{
				auto& graph = animComp.m_animationGraph;

				if (graph && !graph->states.empty())
				{
					// Find start state
					auto it = std::find_if(
						graph->states.begin(),
						graph->states.end(),
						[](const auto& s) { return s->isStartState && s->isAttached; }
					);

					if (it != graph->states.end())
					{
						graph->current = *it;
						graph->playing = true;

						animComp.m_animator->PlayAnimation(graph->current->clipName, graph->current->loop);

						EE_CORE_INFO("Animation started at state '{}'", graph->current->name);
					}
				}

				animComp.initialized = true;
			}

			// Update animator if animation is playing
			if (animComp.m_animator->GetCurrentClip())
			{
				animComp.m_animator->Update(deltaTime, entity);
			}

			const auto& finalBones = animComp.m_animator->GetFinalBoneMatrices();
			if (!finalBones.empty() && m_SkeletalSSBO)
			{
				// Allocate bone space if not already allocated
				if (animComp.boneTransformOffset == -1)
				{
					animComp.boneTransformOffset = m_SkeletalSSBO->AllocateBoneSpace(finalBones.size());
					if (animComp.boneTransformOffset >= 0) {
						if (auto renderer = ecs.GetSystem<Renderer>()) {
							renderer->MarkDrawDataForRebuild();
						}
					}
				}

				// Update bone transforms using persistent mapped buffer (direct memcpy, zero-copy)
				if (animComp.boneTransformOffset >= 0)
				{
					m_SkeletalSSBO->UpdateBoneTransforms(animComp.boneTransformOffset, finalBones);
				}
			}
		}
	}
}
