/* Start Header ************************************************************************/
/*!
\file       ViewPortGUI.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       31/01/2026
\brief      This file contains the responsibility for rendering the viewport window

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "imgui.h"
#include "ImguiUIWindow.h"
#include "ImGuizmo.h"
#include "Renderer.h"

namespace Ermine
{
	class ViewPortGUI : public ImGUIWindow
	{
	public:
		ViewPortGUI();

		void Update() override;
		void Render() override;
		/**
		 * @brief Display the simulation control buttons (play, pause, stop) in the top bar
		 * @param iconSize Size of the icons
		 */
		void TopBarSimulationControl(ImVec2 iconSize);
		/**
		 * @brief Handle the framebuffer for the viewport
		 * @param viewport_size Size of the viewport
		 */
		void FrameBufferHandler(const ImVec2& viewport_size);
		/**
		 * @brief Handle the ImGuizmo operations
		 * @param imgMin Minimum position of the image
		 * @param gOperation Current operation mode (translate, rotate, scale)
		 * @param gMode Current mode (local, world)
		 */
		void OverlayGizmoOperation(const ImVec2& imgMin, const ImGuizmo::OPERATION& gOperation, const ImGuizmo::MODE& gMode);
		/**
		 * @brief Focus the camera on the selected entity
		 * @param selectedEntity Currently selected entity
		 * @param viewportHovered Whether the viewport is hovered
		 * @param viewportFocused Whether the viewport is focused
		 */
		void FocusOnSelected(const Ermine::EntityID& selectedEntity, const bool& viewportHovered, const bool& viewportFocused);
		/**
		 * @brief Handle camera controls (orbit, pan, zoom)
		 * @param overViewCube Whether the mouse is over the view cube
		 * @param selectedEntity Currently selected entity
		 * @param viewportHovered Whether the viewport is hovered
		 * @param s_orbiting Whether the camera is currently orbiting
		 */
		void CameraControls(const bool& overViewCube, const Ermine::EntityID& selectedEntity, const bool& viewportHovered, bool& s_orbiting);
		/**
		 * @brief Handle object picking in the viewport
		 * @param offscreen_buffer Offscreen framebuffer for rendering
		 * @param imgMin Minimum position of the image
		 * @param imgSize Size of the image
		 * @param overViewCube Whether the mouse is over the view cube
		 * @param s_orbiting Whether the camera is currently orbiting
		 */
		void ObjectPicking(const std::shared_ptr<Ermine::graphics::Renderer::OffscreenBuffer>& offscreen_buffer, const ImVec2& imgMin,
		                   const ImVec2& imgSize, const bool& overViewCube, bool s_orbiting);
		/**
		 * @brief Render and handle the ImGuizmo overlay for object manipulation
		 * @param imgMin Minimum position of the image
		 * @param imgSize Size of the image
		 * @param vmSize Size of the view cube
		 * @param vmPos Position of the view cube
		 * @param selectedEntity Currently selected entity
		 * @param gOperation Current operation mode (translate, rotate, scale)
		 * @param gMode Current mode (local, world)
		 */
		void GizmoOverlay(const ImVec2& imgMin, const ImVec2& imgSize, const ImVec2& vmSize, const ImVec2& vmPos, const Ermine::EntityID&
		                  selectedEntity,
		                  ImGuizmo::OPERATION& gOperation, ImGuizmo::MODE& gMode);
	};
}
