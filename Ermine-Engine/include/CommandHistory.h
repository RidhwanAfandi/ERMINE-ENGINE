/* Start Header ************************************************************************/
/*!
\file       CommandHistory.h
\author     Wong Jun Yu, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Jan 16, 2026
\brief      This files contains the declaration of CommandHistory class which managed the history of editor
			commands for undo/redo functionality.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "PreCompile.h"

#include "EditorCommand.h"

namespace Ermine::editor
{
	class CommandHistory
	{
		std::vector<std::unique_ptr<EditorCommand>> m_commands;
		size_t m_currentIndex = 0; // Points to the next command to execute
		size_t m_maxHistorySize = 100; // Maximum number of commands to keep in history
		std::function<void()> m_onHistoryChanged;

		CommandHistory() = default;

		void NotifyHistoryChanged() const
		{
			if (m_onHistoryChanged)
				m_onHistoryChanged();
		}
	public:
		static CommandHistory& GetInstance()
		{
			static CommandHistory instance;
			return instance;
		}

		void Execute(std::unique_ptr<EditorCommand> command)
		{
			if (!command) return;

			// Clear command redo stack
			if (m_currentIndex < m_commands.size())
				m_commands.erase(m_commands.begin() + m_currentIndex, m_commands.end());

			// Try to merge with the last command if possible
			if (!m_commands.empty() && m_commands.back()->CanMergeWith(command.get()))
			{
				m_commands.back()->MergeWith(command.get());

				m_commands.back()->Execute();

				NotifyHistoryChanged();
				return;
			}
			
			command->Execute();
			m_commands.push_back(std::move(command));
			++m_currentIndex;

			// Limit history size
			if (m_commands.size() > m_maxHistorySize)
			{
				m_commands.erase(m_commands.begin());
				--m_currentIndex;
			}
			
			NotifyHistoryChanged();
		}

		bool Undo()
		{
			if (!CanUndo()) return false;

			--m_currentIndex;
			m_commands[m_currentIndex]->Undo();
			NotifyHistoryChanged();
			return true;
		}

		bool Redo()
		{
			if (!CanRedo()) return false;

			m_commands[m_currentIndex]->Redo();
			++m_currentIndex;
			NotifyHistoryChanged();
			return true;
		}

		bool CanUndo() const { return m_currentIndex > 0; }
		bool CanRedo() const { return m_currentIndex < m_commands.size(); }

		void Clear()
		{
			m_commands.clear();
			m_currentIndex = 0;
			NotifyHistoryChanged();
		}
		
		std::vector<std::string> GetUndoStack() const
		{
			std::vector<std::string> stack;
			for (size_t i = 0; i < m_currentIndex; ++i)
				stack.emplace_back(m_commands[i]->GetDescription());
			return stack;
		}

		std::vector<std::string> GetRedoStack() const
		{
			std::vector<std::string> stack;
			for (const auto& m_command : m_commands)
				stack.emplace_back(m_command->GetDescription());
			return stack;
		}

		void SetHistoryChangedCallback(std::function<void()> callback)
		{
			m_onHistoryChanged = std::move(callback);
		}

		void SetMaxHistorySize(size_t size) { m_maxHistorySize = size; }
	};
}
