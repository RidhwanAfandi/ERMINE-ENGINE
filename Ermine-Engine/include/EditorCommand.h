/* Start Header ************************************************************************/
/*!
\file       EditorCommand.h
\author     Wong Jun Yu, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Jan 16, 2026
\brief      Base class for editor commands implementing undo/redo functionality.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"
#include "ECS.h"
#include "HierarchySystem.h"

namespace Ermine
{
	class HierarchySystem;
	struct Transform;
}

namespace Ermine::editor
{
	// Base class for editor commands
	// Each command should have its own class inheriting from this command
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() = default;

		virtual void Execute() = 0;

		virtual void Undo() = 0;

		virtual void Redo() { Execute(); }

		virtual const std::string GetDescription() const = 0;

		virtual bool CanMergeWith(const EditorCommand* other) const { return false; }
		virtual void MergeWith(const EditorCommand* other) {}
	};

	// Command to change an entity's transform
	class TransformCommand : public EditorCommand
	{
		EntityID m_entity;
		Transform m_oldTransform;
		Transform m_newTransform;

		void MarkTransformDirty() const
		{
			if (auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>())
			{
				hierarchySystem->MarkDirty(m_entity);
				hierarchySystem->OnTransformChanged(m_entity);
			}
		}
	public:
		TransformCommand(EntityID entity, const Transform& oldTransform, const Transform& newTransform)
			: m_entity(entity), m_oldTransform(oldTransform), m_newTransform(newTransform) 
		{
		}

		void Execute() override
		{
			auto& ecs = ECS::GetInstance();
			if (ecs.IsEntityValid(m_entity) && ecs.HasComponent<Transform>(m_entity))
			{
				ecs.GetComponent<Transform>(m_entity) = m_newTransform;
				MarkTransformDirty();
			}
		}

		void Undo() override
		{
			auto& ecs = ECS::GetInstance();
			if (ecs.IsEntityValid(m_entity) && ecs.HasComponent<Transform>(m_entity))
			{
				ecs.GetComponent<Transform>(m_entity) = m_oldTransform;
				MarkTransformDirty();
			}
		}

		const std::string GetDescription() const override
		{
			return "Transform Entity";
		}

		/// <summary>
		/// If this command's entity is the same as the other command's entity, we can merge them
		/// </summary>
		/// <param name="other">The other command pointer</param>
		/// <returns>True if entity are same, otherwise false</returns>
		bool CanMergeWith(const EditorCommand* other) const override
		{
			(void)other;
			return false;
			/*const auto* otherCmd = dynamic_cast<const TransformCommand*>(other);
			return otherCmd && otherCmd->m_entity == m_entity;*/
		}

		void MergeWith(const EditorCommand* other) override
		{
			//const auto* otherCmd = dynamic_cast<const TransformCommand*>(other);
			//m_newTransform = otherCmd->m_newTransform;
			(void)other;
		}
	};
}
