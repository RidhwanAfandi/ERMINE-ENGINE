/* Start Header ************************************************************************/
/*!
\file       ImguiUIWindow.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (85%)
\co-author Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (15%)
\date       31/01/2026
\brief      This file contains declarations for interfaces of ImguiUIWindow.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

namespace Ermine {

	class ImGUIWindow {
	public:
		/*!***********************************************************************
		\brief
		 Constructor for ImGUIWindow, initializes the window with a name, and sets its open state in the static map.
		\param[in] _name
		 Optional name for the ImGui window. Defaults to an empty string.
		*************************************************************************/
		ImGUIWindow(std::string _name = "") : m_name{ _name } {
			if (!s_windowOpenStates.contains(m_name)) s_windowOpenStates.emplace(m_name, true);
		}
		/*!***********************************************************************
		\brief
		 Virtual destructor for ImGUIWindow.
		*************************************************************************/
		virtual ~ImGUIWindow() {};
		/*!***********************************************************************
		\brief
		 Update logic for the ImGui window.
		 Intended to be overridden by derived classes.
		*************************************************************************/
		virtual void Update() {}
		/*!***********************************************************************
		\brief
		 Pure virtual function to render the ImGui window.
		 Must be implemented by derived classes.
		*************************************************************************/
		virtual void Render() = 0;
		/*!***********************************************************************
		\brief
		 Getter for the window name.
		\return
		 The name of the ImGui window as a std::string.
		*************************************************************************/
		std::string Name() { return m_name; }
		/*!***********************************************************************
		\brief
		 Setter for the window name.
		\param[in] _name
		 New name to assign to the ImGui window.
		*************************************************************************/
		void Name(std::string _name) {
			m_name = _name;
			if (!s_windowOpenStates.contains(m_name)) s_windowOpenStates.emplace(m_name, true);
		}
		/*!***********************************************************************
		\brief
		 Check if the window is open.
		\return
		 True if the window is open, false otherwise.
		*************************************************************************/
		bool IsOpen() const {
			auto it = s_windowOpenStates.find(m_name);
			return it != s_windowOpenStates.end() ? it->second : true;
		}
		/*!***********************************************************************
		\brief
		 Set the open state of the window.
		\param[in] open
		 True to open the window, false to close it.
		*************************************************************************/
		void SetOpen(bool open) { s_windowOpenStates[m_name] = open; }
		/*!***********************************************************************
		\brief
		 Get a pointer to the open state of the window.
		\return
		 Pointer to the boolean representing the open state of the window.
		*************************************************************************/
		bool* GetOpenPtr() { return &s_windowOpenStates[m_name]; }
		/*!***********************************************************************
		\brief
		 Static method to get the open state of a window by name.
		\return
		 Static method to get the map of all window open states.
		*************************************************************************/
		static const std::unordered_map<std::string, bool>& GetAllWindowStates() { return s_windowOpenStates; }
		/*!***********************************************************************
		\brief
		 Static method to set the open states of all windows.
		\param[in] states
		 Map of window names to their desired open states.
		*************************************************************************/
		static void SetAllWindowStates(const std::unordered_map<std::string, bool>& states) { s_windowOpenStates = states; }

	protected:
		// Name of the ImGui window
		std::string m_name;

	private:
		// Static map to track open states of windows by name
		static inline std::unordered_map<std::string, bool> s_windowOpenStates{};
	};
}