	/* Start Header ************************************************************************/
/*!
\file       ViewPortGUI.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       31/01/2026
\brief      This file contains the responsibility for rendering the viewport window

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "ViewPortGUI.h"

#include "ECS.h"
#include "FrameController.h"
#include "Input.h"
#include "Renderer.h"
#include "GLFW/glfw3.h"
#include "EditorGUI.h"
#include "HierarchySystem.h"
#include "UIButtonSystem.h"

#include <ImGuizmo.h>
#include <glm/gtx/matrix_decompose.hpp>

#include "AssetManager.h"
#include "CommandHistory.h"
#include "EditorCommand.h"
#include "HierarchyPanel.h"
#include "imgui_internal.h"
#include "InspectorGUI.h"

#include "Scene.h"
#include "SceneManager.h"
#include "PrefabManager.h"
#include "Physics.h"
#include "TransformMode.h"
#include "Selection.h"
#include "MultiSelectionManipulator.h"
#include "Window.h"

using namespace Ermine::editor;

EditorGUI::SimState EditorGUI::s_state = SimState::stopped;

namespace Ermine::editor
{
	// NEW: Global transform mode state
	static TransformMode s_transformMode = TransformMode::Pivot;
}

namespace
{
	ImTextureID gIconPlay = 0;
	ImTextureID gIconStop = 0;
	bool gIconsLoaded = false;

	struct ViewportRect
	{
		ImVec2 min{};
		ImVec2 max{};
	};

	std::optional<ViewportRect> ProjectAABBToViewportRect(
		const glm::mat4& viewProj,
		const ImVec2& imgMin,
		const ImVec2& imgMax,
		const glm::vec3& aabbMin,
		const glm::vec3& aabbMax)
	{
		glm::vec3 corners[8] =
		{
			{ aabbMin.x, aabbMin.y, aabbMin.z },
			{ aabbMax.x, aabbMin.y, aabbMin.z },
			{ aabbMin.x, aabbMax.y, aabbMin.z },
			{ aabbMax.x, aabbMax.y, aabbMin.z },
			{ aabbMin.x, aabbMin.y, aabbMax.z },
			{ aabbMax.x, aabbMin.y, aabbMax.z },
			{ aabbMin.x, aabbMax.y, aabbMax.z },
			{ aabbMax.x, aabbMax.y, aabbMax.z },
		};

		bool anyVisible = false;

		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();

		const float vpW = imgMax.x - imgMin.x;
		const float vpH = imgMax.y - imgMin.y;
		if (vpW <= 1.0f || vpH <= 1.0f) // Invalid viewport size
			return std::nullopt;

		for (const glm::vec3& c : corners)
		{
			glm::vec4 clip = viewProj * glm::vec4(c, 1.0f);

			if (clip.w <= 0.00001f) // Object's aabb is behind camera
				continue;

			const glm::vec3 ndc = glm::vec3(clip) / clip.w;

			if (ndc.z >= -1.0f && ndc.z <= 1.0f)
				anyVisible = true;

			// Convert NDC to viewport Coords
			const float sx = imgMin.x + (ndc.x * 0.5f + 0.5f) * vpW;
			const float sy = imgMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpH; // 1.0f for flip in ImGUI

			minX = std::min(minX, sx);
			minY = std::min(minY, sy);
			maxX = std::max(maxX, sx);
			maxY = std::max(maxY, sy);
		}

		if (!anyVisible || !std::isfinite(minX) || !std::isfinite(minY) || !std::isfinite(maxX) || !std::isfinite(maxY))
			return std::nullopt;

		ViewportRect r{};
		r.min = ImVec2(ImClamp(minX, imgMin.x, imgMax.x), ImClamp(minY, imgMin.y, imgMax.y));
		r.max = ImVec2(ImClamp(maxX, imgMin.x, imgMax.x), ImClamp(maxY, imgMin.y, imgMax.y));

		if ((r.max.x - r.min.x) < 2.0f || (r.max.y - r.min.y) < 2.0f)
			return std::nullopt;

		return r;
	}

	void DrawSelectionHighlightRect(
		const Ermine::EntityID selectedEntity,
		const ImVec2& imgMin,
		const ImVec2& imgMax)
	{
		if (EditorGUI::isPlaying)
			return;

		auto& ecs = Ermine::ECS::GetInstance();
		if (!ecs.IsEntityValid(selectedEntity) || !ecs.HasComponent<Ermine::Transform>(selectedEntity))
			return;

		// Build a conservative AABB.
		// If you later have mesh/model bounds in components, replace this with real bounds.
		const Ermine::Transform& tr = ecs.GetComponent<Ermine::Transform>(selectedEntity);

		const glm::vec3 center(tr.position.x, tr.position.y, tr.position.z);
		const glm::vec3 halfExtents(
			std::max(0.25f, std::abs(tr.scale.x) * 0.5f),
			std::max(0.25f, std::abs(tr.scale.y) * 0.5f),
			std::max(0.25f, std::abs(tr.scale.z) * 0.5f));

		const glm::vec3 aabbMin = center - halfExtents;
		const glm::vec3 aabbMax = center + halfExtents;

		const Ermine::Mtx44& v = Ermine::editor::EditorCamera::GetInstance().GetViewMatrix();
		const Ermine::Mtx44& p = Ermine::editor::EditorCamera::GetInstance().GetProjectionMatrix();

		const glm::mat4 view = glm::mat4(
			v.m00, v.m01, v.m02, v.m03,
			v.m10, v.m11, v.m12, v.m13,
			v.m20, v.m21, v.m22, v.m23,
			v.m30, v.m31, v.m32, v.m33
		);

		const glm::mat4 proj = glm::mat4(
			p.m00, p.m01, p.m02, p.m03,
			p.m10, p.m11, p.m12, p.m13,
			p.m20, p.m21, p.m22, p.m23,
			p.m30, p.m31, p.m32, p.m33
		);

		const glm::mat4 viewProj = proj * view;

		const auto rectOpt = ProjectAABBToViewportRect(viewProj, imgMin, imgMax, aabbMin, aabbMax);
		if (!rectOpt.has_value())
			return;

		const ViewportRect r = rectOpt.value();

		ImDrawList* dl = ImGui::GetWindowDrawList();

		const ImU32 outlineCol = IM_COL32(64, 160, 255, 235);
		const ImU32 fillCol = IM_COL32(64, 160, 255, 35);

		// Soft fill + double outline
		dl->AddRectFilled(r.min, r.max, fillCol, 2.0f);
		dl->AddRect(r.min, r.max, outlineCol, 2.0f, 0, 2.0f);
		dl->AddRect(ImVec2(r.min.x - 1.0f, r.min.y - 1.0f), ImVec2(r.max.x + 1.0f, r.max.y + 1.0f), IM_COL32(0, 0, 0, 120), 2.0f, 0, 2.0f);
	}

	void SubmitReferenceGridY0(const float y,
		const float spacing,
		const int halfLineCount,
		const int majorEvery,
		const glm::vec3& minorColor,
		const glm::vec3& majorColor,
		const glm::vec3& axisXColor,
		const glm::vec3& axisZColor)
	{
		auto renderer = Ermine::ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>();
		if (!renderer)
			return;

		const Ermine::Vector3D camPosEE = Ermine::editor::EditorCamera::GetInstance().GetPosition();
		const glm::vec2 camXZ(camPosEE.x, camPosEE.z);

		// Snap the grid to spacing so it doesn't "swim" when moving.
		const float snappedX = std::floor(camXZ.x / spacing) * spacing;
		const float snappedZ = std::floor(camXZ.y / spacing) * spacing;

		const float extent = static_cast<float>(halfLineCount) * spacing;

		for (int i = -halfLineCount; i <= halfLineCount; ++i)
		{
			const float offset = static_cast<float>(i) * spacing;

			const bool isMajor = (majorEvery > 0) && (i % majorEvery == 0);
			const glm::vec3 col = isMajor ? majorColor : minorColor;

			// Lines parallel to Z (vary X)
			{
				const float x = snappedX + offset;
				const glm::vec3 a(x, y, snappedZ - extent);
				const glm::vec3 b(x, y, snappedZ + extent);
				renderer->SubmitDebugLine(a, b, col);
			}

			// Lines parallel to X (vary Z)
			{
				const float z = snappedZ + offset;
				const glm::vec3 a(snappedX - extent, y, z);
				const glm::vec3 b(snappedX + extent, y, z);
				renderer->SubmitDebugLine(a, b, col);
			}
		}

		// Axis emphasis at world origin (only if origin is inside the drawn tile)
		// X axis (Z=0), Z axis (X=0)
		{
			const glm::vec3 xA(-extent, y, 0.0f);
			const glm::vec3 xB(extent, y, 0.0f);
			renderer->SubmitDebugLine(xA, xB, axisXColor);

			const glm::vec3 zA(0.0f, y, -extent);
			const glm::vec3 zB(0.0f, y, extent);
			renderer->SubmitDebugLine(zA, zB, axisZColor);
		}
	}

	void LoadToolbarIcons()
	{
		if (gIconsLoaded) return;

		auto loadTex = [](const char* path) -> ImTextureID
			{
				auto tex = Ermine::AssetManager::GetInstance().LoadTexture(path);
				if (tex && tex->IsValid())
					return static_cast<ImTextureID>(static_cast<intptr_t>(tex->GetRendererID()));
				return 0;
			};

		gIconPlay = loadTex("../Resources/Textures/Icons/play.png");
		if (!gIconPlay) EE_CORE_WARN("Cannot find play button!");

		gIconStop = loadTex("../Resources/Textures/Icons/stop.png");
		if (!gIconStop) EE_CORE_WARN("Cannot find stop button!");

		gIconsLoaded = true;
	}

	bool DrawIconOrTextButton(ImTextureID icon, const char* text, const ImVec2& size)
	{
		if (icon)
			return ImGui::ImageButton(text, icon, size, ImVec2(0, 1), ImVec2(1, 0));
		return ImGui::Button(text, size);
	}
}

Ermine::ViewPortGUI::ViewPortGUI() : ImGUIWindow("Viewport")
{
}

void Ermine::ViewPortGUI::Render()
{
}

void Ermine::ViewPortGUI::TopBarSimulationControl(const ImVec2 iconSize)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 6.f));
	ImGui::BeginGroup();
	{
		const bool playing = (EditorGUI::s_state == EditorGUI::SimState::playing);
		//const bool paused = (EditorGUI::s_state == EditorGUI::SimState::paused);
		const bool stopped = (EditorGUI::s_state == EditorGUI::SimState::stopped);

		// Play
		// TODO: We need to handle the cursor mode properly instead of directly setting it here. It is also for scripting purposes.
		ImGui::BeginDisabled(playing);
		if (DrawIconOrTextButton(gIconPlay, "Play", iconSize))
		{
			EditorGUI::s_state = EditorGUI::SimState::playing;
			SceneManager::GetInstance().SaveTemp();
			EE_CORE_INFO("Simulation: Play");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Play (Ctrl+P)");
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Pause/Resume
		//ImGui::BeginDisabled(stopped);
		//const char* pauseText = paused ? "Resume" : "Pause";
		//ImTextureID pauseIcon = paused ? gIconPlay : gIconPause; // show Play icon for Resume
		//if (DrawIconOrTextButton(pauseIcon, pauseText, iconSize))
		//{
		//	EditorGUI::s_state = paused ? EditorGUI::SimState::playing : EditorGUI::SimState::paused;
		//	EE_CORE_INFO("Simulation: {0}", paused ? "Resume" : "Pause");
		//}
		//if (ImGui::IsItemHovered())
		//	ImGui::SetTooltip("Pause/Resume (Ctrl+P)");
		//ImGui::EndDisabled();

		//ImGui::SameLine();

		// Stop
		ImGui::BeginDisabled(stopped);
		if (DrawIconOrTextButton(gIconStop, "Stop", iconSize))
		{
			EditorGUI::s_state = EditorGUI::SimState::stopped;
			SceneManager::GetInstance().LoadTemp();

			CommandHistory::GetInstance().Clear();
			EE_CORE_INFO("Simulation: Stop");

			// *** ALWAYS reset cursor state when stopping play mode ***
			GLFWwindow* window = glfwGetCurrentContext();
			if (window)
			{
				Window::SetCursorLockState(Window::CursorLockState::None);
				EE_CORE_INFO("Cursor unlocked - play mode stopped");
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Stop (Ctrl+Shift+P)");
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();

		// Preview UI toggle button
		ImGui::BeginDisabled(playing);
		if (ImGui::Checkbox("Preview UI", &EditorGUI::isPreviewingUI))
		{
			EE_CORE_INFO("UI Preview: {}", EditorGUI::isPreviewingUI ? "Enabled" : "Disabled");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Toggle UI preview in viewport (for Main Menu & Cutscenes)");
		ImGui::EndDisabled();
	}
	ImGui::EndGroup();

	float rightOffset = ImGui::GetContentRegionAvail().x - 150.0f;
	ImGui::SameLine(ImGui::GetCursorPosX() + rightOffset);

	ImGui::Checkbox("Physics Wireframe", &ECS::GetInstance().GetSystem<Physics>()->wireframe);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Requires Physics Component");

	ImGui::PopStyleVar();
}

void Ermine::ViewPortGUI::FrameBufferHandler(const ImVec2& viewport_size)
{
	static bool first_time = true;
	auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
	if (first_time)
	{
		renderer->CreateOffscreenBuffer(static_cast<int>(viewport_size.x), static_cast<int>(viewport_size.y));
		renderer->ResizeGBuffer(static_cast<int>(viewport_size.x), static_cast<int>(viewport_size.y));
		first_time = false;
	}

	if (const auto offscreen_buffer = renderer->GetOffscreenBuffer())
	{
		// Resize the offscreen buffer when viewport size changes
		if (offscreen_buffer->width != static_cast<int>(viewport_size.x) ||
			offscreen_buffer->height != static_cast<int>(viewport_size.y))
		{
			renderer->ResizeOffscreenBuffer(static_cast<int>(viewport_size.x), static_cast<int>(viewport_size.y));
			renderer->ResizeGBuffer(static_cast<int>(viewport_size.x), static_cast<int>(viewport_size.y));
		}
	}
}

void Ermine::ViewPortGUI::OverlayGizmoOperation(const ImVec2& imgMin, const ImGuizmo::OPERATION& gOperation, const ImGuizmo::MODE& gMode)
{
	// Overlay current gizmo operation/mode

	auto OpToString = [](ImGuizmo::OPERATION op) -> const char*
		{
			switch (op)
			{
			case ImGuizmo::TRANSLATE: return "Translate";
			case ImGuizmo::ROTATE: return "Rotate";
			case ImGuizmo::SCALE: return "Scale";
			default: return "Unknown";
			}
		};

	auto ModeToString = [](ImGuizmo::MODE m) -> const char*
		{
			return (m == ImGuizmo::LOCAL) ? "Local" : "World";
		};

	const char* opText = OpToString(gOperation);
	const char* modeText = ModeToString(gMode);

	// Access transform mode from namespace
	const char* transformModeText = (editor::s_transformMode == TransformMode::Pivot) ? "Pivot" : "Center";

	char label[256];
	(void)snprintf(label, sizeof(label), "Op: %s | Mode: %s | Transform: %s (Y to toggle)", 
		opText, modeText, transformModeText);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 padPx(8.f, 6.f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	const ImVec2 boxPos = ImVec2(imgMin.x + 8.f, imgMin.y + 8.f);
	const ImVec2 boxMax = ImVec2(boxPos.x + textSize.x + padPx.x * 2.f,
		boxPos.y + textSize.y + padPx.y * 2.f);

	dl->AddRectFilled(boxPos, boxMax, IM_COL32(0, 0, 0, 180), 4.0f);
	dl->AddText(ImVec2(boxPos.x + padPx.x, boxPos.y + padPx.y), IM_COL32(255, 255, 255, 255), label);
}

void Ermine::ViewPortGUI::FocusOnSelected(const Ermine::EntityID& selectedEntity, const bool& viewportHovered, const bool& viewportFocused)
{
	// Focus camera on selected entity (F key)
	if (viewportFocused && viewportHovered && !EditorGUI::isPlaying && Input::IsKeyPressedEditor(GLFW_KEY_F))
	{
		if (ECS::GetInstance().IsEntityValid(selectedEntity) && ECS::GetInstance().HasComponent<Transform>(selectedEntity))
		{
			auto& ecs = ECS::GetInstance();
			auto& tr = ecs.GetComponent<Transform>(selectedEntity);
			EditorCamera::GetInstance().Focus(tr.position, 2.5f); // TODO: Lerp for smoother transition
		}
	}
}

void Ermine::ViewPortGUI::CameraControls(const bool& overViewCube, const Ermine::EntityID& selectedEntity, const bool& viewportHovered, bool& s_orbiting)
{
	static ImVec2 s_lastMouse = ImVec2(0, 0);
	static Vector3D s_pivot = Vector3D(0.0f, 0.0f, 0.0f);
	static float s_distance = 5.0f;

	const bool altDown = Input::IsKeyDownEditor(GLFW_KEY_LEFT_ALT);
	const bool MouseDown = Input::IsMouseButtonDownEditor(GLFW_MOUSE_BUTTON_LEFT);

	// Orbit controls
	if (viewportHovered && !EditorGUI::isPlaying && altDown && MouseDown && !overViewCube)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (!s_orbiting)
		{
			// Initialize orbit
			if (ECS::GetInstance().IsEntityValid(selectedEntity) && ECS::GetInstance().HasComponent<Transform>(selectedEntity))
			{
				auto& ecs = ECS::GetInstance();
				auto& tr = ecs.GetComponent<Transform>(selectedEntity);
				s_pivot = tr.position;
			}
			else
			{
				//s_pivot = EditorCamera::GetInstance().GetPosition() + EditorCamera::GetInstance().() * s_distance;
			}

			const Vector3D camPos = EditorCamera::GetInstance().GetPosition();
			Vector3D diff = camPos - s_pivot;
			s_distance = Vec3Length(diff);
			if (s_distance < 0.001f) s_distance = 5.0f;

			s_lastMouse = io.MousePos;
			s_orbiting = true;
		}
		else
		{
			const float dx = io.MousePos.x - s_lastMouse.x;
			const float dy = io.MousePos.y - s_lastMouse.y;
			s_lastMouse = io.MousePos;

			EditorCamera::GetInstance().OrbitAround(s_pivot, dx, dy, s_distance);
		}
	}
	else
	{
		s_orbiting = false;
	}

	// Camera controls
	if (viewportHovered && ImGui::IsWindowFocused() && !EditorGUI::isPlaying && !s_orbiting)
	{
		if (!ImGuizmo::IsUsing())
		{
			EditorCamera::GetInstance().ProcessMouseMovement();
			EditorCamera::GetInstance().ProcessKeyboardInput(FrameController::GetDeltaTime());
			EditorCamera::GetInstance().ProcessScrollWheel(Input::GetMouseScrollOffsetEditor());
		}
	}
}

void Ermine::ViewPortGUI::ObjectPicking(const std::shared_ptr<Ermine::graphics::Renderer::OffscreenBuffer>& offscreen_buffer, const ImVec2& imgMin, const ImVec2
	& imgSize, const bool& overViewCube, bool s_orbiting)
{
	// Left-click within the image, perform picking
	if (!EditorGUI::isPlaying && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (!s_orbiting && !overViewCube && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
		{
			ImGuiIO io = ImGui::GetIO();
			const float localX = io.MousePos.x - imgMin.x;
			const float localY = io.MousePos.y - imgMin.y;

			if (localX >= 0.0f && localY >= 0.0f && localX <= imgSize.x && localY <= imgSize.y)
			{
				// Convert to framebuffer coordinates (y is flipped)
				const float u = imgSize.x > 0.0f ? localX / imgSize.x : 0.0f;
				const float v = imgSize.y > 0.0f ? localY / imgSize.y : 0.0f;

				const int px = static_cast<int>(u * offscreen_buffer->width);
				const int py = static_cast<int>((1.0f - v) * offscreen_buffer->height);
				auto [hit, entity] = ECS::GetInstance().GetSystem<graphics::Renderer>()->PickEntityAt(std::clamp(px, 0, offscreen_buffer->width - 1),
					std::clamp(py, 0, offscreen_buffer->height - 1),
					EditorCamera::GetInstance().GetViewMatrix(),
					EditorCamera::GetInstance().GetProjectionMatrix());

				if (hit)
				{
					if (EditorGUI::GetHierarchyPanel() && EditorGUI::GetHierarchyPanel()->GetScene())
					{
						if (ImGui::GetIO().KeyCtrl)
							Selection::Toggle(EditorGUI::GetHierarchyPanel()->GetScene(), entity);
						else
							Selection::SelectSingle(EditorGUI::GetHierarchyPanel()->GetScene(), entity);
					}

					// Set selection in Inspector
					//EditorGUI::GetHierarchyPanel()->

					SceneManager::GetInstance().GetActiveScene()->SetSelectedEntity(entity);
				}
				else
					SceneManager::GetInstance().GetActiveScene()->SetSelectedEntity(0);
			}
		}
	}
}

void Ermine::ViewPortGUI::GizmoOverlay(const ImVec2& imgMin, const ImVec2& imgSize, const ImVec2& vmSize, const ImVec2& vmPos, const Ermine::EntityID&
	selectedEntity, ImGuizmo::OPERATION& gOperation, ImGuizmo::MODE& gMode)
{
	const Mtx44& v = EditorCamera::GetInstance().GetViewMatrix();
	const Mtx44& p = EditorCamera::GetInstance().GetProjectionMatrix();

	glm::mat4 view = glm::mat4(
		v.m00, v.m01, v.m02, v.m03,
		v.m10, v.m11, v.m12, v.m13,
		v.m20, v.m21, v.m22, v.m23,
		v.m30, v.m31, v.m32, v.m33
	);
	glm::mat4 proj = glm::mat4(
		p.m00, p.m01, p.m02, p.m03,
		p.m10, p.m11, p.m12, p.m13,
		p.m20, p.m21, p.m22, p.m23,
		p.m30, p.m31, p.m32, p.m33
	);

	// ImGuizmo setup for this image rect
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	ImGuizmo::SetRect(imgMin.x, imgMin.y, imgSize.x, imgSize.y);

	// OBJECT Gizmo overlay
	bool multi = Selection::Multiple();
	if (!EditorGUI::isPlaying && 
		((multi && !Selection::TopLevel().empty()) ||
			(!multi && ECS::GetInstance().IsEntityValid(selectedEntity) && ECS::GetInstance().HasComponent<Transform>(selectedEntity))))
	{
		auto& ecs = ECS::GetInstance();
		//auto& tr = ecs.GetComponent<Transform>(selectedEntity);
		Transform* singleTr = nullptr;
		if (!multi)
			singleTr = &ecs.GetComponent<Transform>(selectedEntity);

		// Toggle transform mode with Y key
		//if (Input::IsKeyPressedEditor(GLFW_KEY_Y)) {
		//	editor::s_transformMode = (editor::s_transformMode == TransformMode::Pivot)
		//		? TransformMode::Center
		//		: TransformMode::Pivot;
		//	//EE_CORE_INFO("Transform mode: {}",
		//	//	(editor::s_transformMode == TransformMode::Pivot) ? "Pivot" : "Center");
		//}

		// Get the position where gizmo should appear
		//Vec3 gizmoPosition = TransformModeHelper::GetManipulationPosition(selectedEntity, editor::s_transformMode);
		Vec3 gizmoPosition{};
		Quaternion gizmoRotation{ 0,0,0,1 };
		Vec3 gizmoScale{ 1,1,1 };

		if (multi)
		{
			auto gt = MultiSelectionManipulator::BuildGroup();
			gizmoPosition = gt.pivotWorld;
			gizmoScale = gt.averageScale;
		}
		else
		{
			gizmoPosition = TransformModeHelper::GetManipulationPosition(selectedEntity, s_transformMode);
			gizmoScale = singleTr->scale;
			gizmoRotation = singleTr->rotation;
		}

		// Build model matrix using gizmo position instead of transform.position
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(gizmoPosition.x, gizmoPosition.y, gizmoPosition.z));
		//glm::quat rotQuat(tr.rotation.w, tr.rotation.x, tr.rotation.y, tr.rotation.z);
		glm::quat rotQuat(gizmoRotation.w, gizmoRotation.x, gizmoRotation.y, gizmoRotation.z);
		rotQuat = glm::normalize(rotQuat);
		model *= glm::mat4_cast(rotQuat);
		//model = glm::scale(model, glm::vec3(tr.scale.x, tr.scale.y, tr.scale.z));
		model = glm::scale(model, glm::vec3(gizmoScale.x, gizmoScale.y, gizmoScale.z));

		// Snapping
		const bool useSnap = Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) || Input::IsKeyDownEditor(GLFW_KEY_RIGHT_CONTROL);
		float snap[3] = { 0.5f, 0.5f, 0.5f };
		if (gOperation == ImGuizmo::ROTATE) { snap[0] = snap[1] = snap[2] = 5.0f; }
		if (gOperation == ImGuizmo::SCALE) { snap[0] = snap[1] = snap[2] = 0.1f; }

		// Manipulate
		LightProbeVolumeComponent* volume = nullptr;
		if (!multi && ecs.HasComponent<LightProbeVolumeComponent>(selectedEntity)) {
			auto& vol = ecs.GetComponent<LightProbeVolumeComponent>(selectedEntity);
			if (vol.showGizmos) {
				volume = &vol;
			}
		}

		if (volume && gOperation == ImGuizmo::SCALE)
		{
			auto hs = ecs.GetSystem<HierarchySystem>();
			Vec3 worldPos = singleTr ? singleTr->position : Vec3{};
			Quaternion worldRot = singleTr ? singleTr->rotation : Quaternion{ 0,0,0,1 };
			Vec3 worldScale = singleTr ? singleTr->scale : Vec3{ 1,1,1 };

			if (hs) {
				worldPos = hs->GetWorldPosition(selectedEntity);
				worldRot = hs->GetWorldRotation(selectedEntity);
				worldScale = hs->GetWorldScale(selectedEntity);
			}

			glm::mat4 entityWorld = glm::mat4(1.0f);
			entityWorld = glm::translate(entityWorld, glm::vec3(worldPos.x, worldPos.y, worldPos.z));
			glm::quat worldQuat(worldRot.w, worldRot.x, worldRot.y, worldRot.z);
			worldQuat = glm::normalize(worldQuat);
			entityWorld *= glm::mat4_cast(worldQuat);
			entityWorld = glm::scale(entityWorld, glm::vec3(worldScale.x, worldScale.y, worldScale.z));

			glm::vec3 localMin = glm::vec3(volume->boundsMin.x, volume->boundsMin.y, volume->boundsMin.z);
			glm::vec3 localMax = glm::vec3(volume->boundsMax.x, volume->boundsMax.y, volume->boundsMax.z);
			glm::vec3 localCenter = (localMin + localMax) * 0.5f;
			glm::vec3 localSize = (localMax - localMin);

			glm::mat4 localBox = glm::mat4(1.0f);
			localBox = glm::translate(localBox, localCenter);
			localBox = glm::scale(localBox, localSize);

			glm::mat4 boxWorld = entityWorld * localBox;

			ImGuizmo::Manipulate(
				glm::value_ptr(view),
				glm::value_ptr(proj),
				ImGuizmo::SCALE,
				ImGuizmo::LOCAL,
				glm::value_ptr(boxWorld),
				nullptr,
				useSnap ? snap : nullptr
			);

			if (ImGuizmo::IsUsing())
			{
				glm::mat4 local = glm::inverse(entityWorld) * boxWorld;
				glm::vec3 skew, translation, scale;
				glm::vec4 perspective;
				glm::quat rotation;
				if (glm::decompose(local, scale, rotation, translation, skew, perspective))
				{
					scale = glm::abs(scale);
					glm::vec3 newCenter = translation;
					glm::vec3 halfSize = scale * 0.5f;
					volume->boundsMin = glm::vec3(newCenter.x - halfSize.x, newCenter.y - halfSize.y, newCenter.z - halfSize.z);
					volume->boundsMax = glm::vec3(newCenter.x + halfSize.x, newCenter.y + halfSize.y, newCenter.z + halfSize.z);
				}
			}
			return;
		}

		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
			gOperation,
			gMode,
			glm::value_ptr(model),
			nullptr,
			useSnap ? snap : nullptr
		);

		static Transform s_startTransform;
		static bool s_commandStarted = false;

		// Apply result back into Transform
		if (ImGuizmo::IsUsing())
		{
			glm::vec3 skew, translation, scale;
			glm::vec4 perspective;
			glm::quat rotation;
			if (glm::decompose(model, scale, rotation, translation, skew, perspective))
			{
				rotation = glm::normalize(rotation);

				if (!s_commandStarted && singleTr)
				{
					s_startTransform = *singleTr;
					s_commandStarted = true;
				}

				if (multi)
				{
					// Derive deltas
					Vec3 newPivot(translation.x, translation.y, translation.z);
					Vec3 deltaT = newPivot - gizmoPosition;

					// For rotation delta
					glm::quat prevRot = rotQuat;
					glm::quat deltaRot = rotation * glm::inverse(prevRot);
					Quaternion deltaQ(deltaRot.x, deltaRot.y, deltaRot.z, deltaRot.w);

					float scaleFactor = (gizmoScale.x != 0.0f) ? (scale.x / gizmoScale.x) : 1.0f;
					if (gOperation == ImGuizmo::TRANSLATE)
						MultiSelectionManipulator::ApplyTranslation(deltaT);
					else if (gOperation == ImGuizmo::ROTATE)
						MultiSelectionManipulator::ApplyRotation(deltaQ, gizmoPosition);
					else if (gOperation == ImGuizmo::SCALE)
						MultiSelectionManipulator::ApplyUniformScale(scaleFactor, gizmoPosition);
				}
				else
				{
					auto& thisTR = *singleTr;
					// Calculate offset from geometric center to pivot
					Vec3 centerOffset = gizmoPosition - thisTR.position;

					// The gizmo manipulated the center point, so we need to adjust for pivot
					if (s_transformMode == TransformMode::Center && gOperation == ImGuizmo::ROTATE)
					{
						// When rotating around center, we need to orbit the pivot around that center
						glm::vec3 centerPos = glm::vec3(gizmoPosition.x, gizmoPosition.y, gizmoPosition.z);
						glm::vec3 pivotOffset = glm::vec3(centerOffset.x, centerOffset.y, centerOffset.z);

						// Rotate the offset vector by the rotation difference
						glm::quat oldRot(thisTR.rotation.w, thisTR.rotation.x, thisTR.rotation.y, thisTR.rotation.z);
						glm::quat deltaRot = rotation * glm::inverse(oldRot);
						glm::vec3 rotatedOffset = deltaRot * pivotOffset;

						// New pivot position = center position - rotated offset
						thisTR.position = Vector3D(
							translation.x - rotatedOffset.x,
							translation.y - rotatedOffset.y,
							translation.z - rotatedOffset.z
						);
					}
					else
					{
						// For translation and pivot mode, just use the manipulated position directly
						thisTR.position = Vector3D(translation.x, translation.y, translation.z);
					}

					thisTR.scale = Vector3D(scale.x, scale.y, scale.z);
					thisTR.rotation = Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);

					// Mark transform as dirty to trigger hierarchy update
					ecs.GetSystem<HierarchySystem>()->MarkDirty(selectedEntity);
					ecs.GetSystem<HierarchySystem>()->OnTransformChanged(selectedEntity);
				}
			}
		}

		if (s_commandStarted && !ImGuizmo::IsUsing())
		{
			auto& finalTransform = ecs.GetComponent<Transform>(selectedEntity);

			auto cmd = std::make_unique<TransformCommand>(
				selectedEntity,
				s_startTransform,
				finalTransform
			);

			CommandHistory::GetInstance().Execute(std::move(cmd));
			s_commandStarted = false;
		}
	}

	// VIEW gizmo (camera nav cube)
	{
		glm::mat4 viewBefore = view;
		glm::mat4 viewEdit = view;

		const Vector3D camPos = EditorCamera::GetInstance().GetPosition();
		float viewLength = Vec3Length(camPos);
		if (viewLength < 0.001f) viewLength = 5.0f;

		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

		ImGuizmo::ViewManipulate(
			glm::value_ptr(viewEdit),
			viewLength,
			vmPos,
			vmSize,
			0x10101010
		);

		auto matChanged = [](const glm::mat4& a, const glm::mat4& b) {
			const float eps = 1e-5f;
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
					if (!glm::epsilonEqual(a[c][r], b[c][r], eps))
						return true;
			return false;
			};

		if (matChanged(viewBefore, viewEdit))
		{
			glm::mat4 inv = glm::inverse(viewEdit);
			glm::vec3 pos = glm::vec3(inv[3]);
			glm::vec3 fwd = glm::normalize(-glm::vec3(inv[2])); // -Z axis

			auto rad2deg = [](float r) { return r * 57.29577951308232f; };
			float yaw = rad2deg(std::atan2(fwd.z, fwd.x));
			float pitch = rad2deg(std::asin(std::clamp(fwd.y, -1.0f, 1.0f)));

			EditorCamera::GetInstance().SetPosition(Vector3D(pos.x, pos.y, pos.z));
			EditorCamera::GetInstance().SetYawPitch(yaw, pitch);
		}
	}
}

void Ermine::ViewPortGUI::Update()
{
	// Return if window is closed
	if (!IsOpen()) return;

	if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
	{
		ImGui::End();
		return;
	}

	LoadToolbarIcons();

	constexpr ImVec2 iconSize = ImVec2(28.f, 28.f);
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	constexpr int buttonCount = 3;
	const float totalWidth = buttonCount * iconSize.x + (buttonCount - 1) * spacing;

	// Center horizontally
	float availWidth = ImGui::GetContentRegionAvail().x;
	float startOffsetX = (availWidth > totalWidth) ? (availWidth - totalWidth) * 0.5f : 0.0f;
	float oldX = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX(oldX + startOffsetX);

	TopBarSimulationControl(iconSize);

	ImGui::Separator();

	EditorGUI::isPlaying = EditorGUI::s_state == EditorGUI::SimState::playing;

	// Begin ImGuizmo frame
	ImGuizmo::BeginFrame();
	ImGuizmo::Enable(true);

	// Obtain available context region in the window (viewport size)
	ImVec2 viewport_size = ImGui::GetContentRegionAvail();

	// Ensure the viewport size is within an acceptable range
	constexpr int minSize = 1;
	int max_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

	if (!EditorGUI::isPlaying)
	{
		constexpr float lineSpacing = 1.0f;
		constexpr int majorEvery = 10;
		constexpr int halfLineCount = 75;

		SubmitReferenceGridY0(
			0.0f,
			lineSpacing,
			halfLineCount,
			majorEvery,
			glm::vec3(0.25f, 0.25f, 0.25f),
			glm::vec3(0.40f, 0.40f, 0.40f),
			glm::vec3(0.80f, 0.20f, 0.20f),
			glm::vec3(0.20f, 0.80f, 0.20f));
	}

	viewport_size.x = std::clamp(viewport_size.x, static_cast<float>(minSize), static_cast<float>(max_size));
	viewport_size.y = std::clamp(viewport_size.y, static_cast<float>(minSize), static_cast<float>(max_size));

	FrameBufferHandler(viewport_size);
	const auto offscreen_buffer = ECS::GetInstance().GetSystem<graphics::Renderer>()->GetOffscreenBuffer();

	// Set editor's camera viewport size
	EditorCamera::GetInstance().SetViewportSize(viewport_size.x, viewport_size.y);

	// Child region that ignores all ImGui inputs
	ImGuiWindowFlags vpChildFlags =
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::BeginChild("SceneViewportRegion", ImVec2(0, 0), false, vpChildFlags);

	// Draw the rendered scene
	if (offscreen_buffer)
	{
		ImGui::Image(
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W) || defined(IMGUI_IMPL_OPENGL_ES2) || defined(IMGUI_IMPL_OPENGL_ES3) || defined(IMGUI_IMPL_OPENGL_LOADER_GLEW) || defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
			(ImTextureID)(intptr_t)offscreen_buffer->ColorTexture,
#else
			offscreen_buffer->ColorTexture,
#endif
			ImGui::GetContentRegionAvail(),
			ImVec2(0, 1), ImVec2(1, 0)
		);
	}

	// Capture the image rect for mouse->pixel conversion
	const ImVec2 imgMin = ImGui::GetItemRectMin();
	const ImVec2 imgMax = ImGui::GetItemRectMax();
	const ImVec2 imgSize = ImGui::GetItemRectSize();

	// UPDATE UI BUTTON SYSTEM WITH VIEWPORT INFO FOR EDITOR MODE
#ifdef EE_EDITOR
	auto uiButtonSystem = ECS::GetInstance().GetSystem<UIButtonSystem>();
	if (uiButtonSystem)
	{
		uiButtonSystem->SetViewportInfo(imgMin, imgSize);
	}
#endif

	// View cube (top-right corner)
	const float pad = 10.f;
	const ImVec2 vmSize = ImVec2(100.f, 100.f);
	const ImVec2 vmPos = ImVec2(imgMax.x - vmSize.x - pad, imgMin.y + pad);
	const ImVec2 vmPosBR = ImVec2(vmPos.x + vmSize.x, vmPos.y + vmSize.y);

	const bool overViewCube = ImGui::IsMouseHoveringRect(vmPos, vmPosBR, false);

	EntityID selectedEntity{};
	//selectedEntity = ref_Inspector->GetEntity();
	selectedEntity = SceneManager::GetInstance().GetActiveScene()->GetSelectedEntity();

	// Keyboard shortcuts for gizmo
	static ImGuizmo::OPERATION gOperation = ImGuizmo::TRANSLATE;
	static ImGuizmo::MODE gMode = ImGuizmo::LOCAL;

	const ImGuiHoveredFlags hovFlags =
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenOverlappedByWindow |
		ImGuiHoveredFlags_AllowWhenOverlappedByItem;

	const bool viewportHovered = ImGui::IsItemHovered(hovFlags);
	const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_None);

	if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		ImGui::SetWindowFocus();
	if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		ImGui::SetWindowFocus();

	//if (!EditorGUI::isPlaying)
	//{
	//	const bool rmbDown = Input::IsMouseButtonDownEditor(GLFW_MOUSE_BUTTON_RIGHT);
	//	const bool shouldCapture = viewportFocused && viewportHovered && rmbDown;

	//	if (shouldCapture)
	//	{
	//		Window::SetVisibleCursor(false);
	//		Window::SetCursorLockState(Window::CursorLockState::Locked);
	//	}
	//	else
	//	{
	//		Window::SetCursorLockState(Window::CursorLockState::None);
	//		Window::SetVisibleCursor(true);
	//	}
	//}

	// Set manipulation mode based on keyboard shortcuts
	if (viewportFocused && viewportHovered && !EditorGUI::isPlaying && !Input::IsMouseButtonDownEditor(GLFW_MOUSE_BUTTON_RIGHT))
	{
		if (Input::IsKeyPressedEditor(GLFW_KEY_W)) gOperation = ImGuizmo::TRANSLATE;
		if (Input::IsKeyPressedEditor(GLFW_KEY_E)) gOperation = ImGuizmo::ROTATE;
		if (Input::IsKeyPressedEditor(GLFW_KEY_R)) gOperation = ImGuizmo::SCALE;
		if (Input::IsKeyPressedEditor(GLFW_KEY_Q)) gMode = gMode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
	}

	OverlayGizmoOperation(imgMin, gOperation, gMode);

	FocusOnSelected(selectedEntity, viewportHovered, viewportFocused);

	static bool s_orbiting = false;
	CameraControls(overViewCube, selectedEntity, viewportHovered, s_orbiting);

	//DrawSelectionHighlightRect(selectedEntity, imgMin, imgMax);

	GizmoOverlay(imgMin, imgSize, vmSize, vmPos, selectedEntity, gOperation, gMode);

	// Marquee mutli-selection state
	static bool s_dragSelecting = false;
	static ImVec2 s_dragStart = {};
	static ImVec2 s_dragEnd = {};
	const bool ctrlDown = Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) || Input::IsKeyDownEditor(GLFW_KEY_RIGHT_CONTROL);
	const float dragThreshold = 3.0f;

	const bool altDown = Input::IsKeyDownEditor(GLFW_KEY_LEFT_ALT);

	// Drag selection
	if (viewportHovered && !EditorGUI::isPlaying && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
		&& !overViewCube && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !altDown)
	{
		s_dragSelecting = true;
		s_dragStart = ImGui::GetMousePos();
		s_dragEnd = s_dragStart;
	}

	// Update drag
	if (s_dragSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing() && !altDown)
	{
		s_dragEnd = ImGui::GetMousePos();
		// Draw rectangle overlay
		ImVec2 rMin(ImMin(s_dragStart.x, s_dragEnd.x), ImMin(s_dragStart.y, s_dragEnd.y));
		ImVec2 rMax(ImMax(s_dragStart.x, s_dragEnd.x), ImMax(s_dragStart.y, s_dragEnd.y));
		// Clamp
		rMin.x = ImClamp(rMin.x, imgMin.x, imgMax.x);
		rMin.y = ImClamp(rMin.y, imgMin.y, imgMax.y);
		rMax.x = ImClamp(rMax.x, imgMin.x, imgMax.x);
		rMax.y = ImClamp(rMax.y, imgMin.y, imgMax.y);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(rMin, rMax, IM_COL32(64, 128, 255, 40));
		dl->AddRect(rMin, rMax, IM_COL32(64, 128, 255, 180), 0.0f, 0, 2.0f);
	}
	else if (s_dragSelecting && altDown)
		s_dragSelecting = false;

	// Finish drag select
	if (s_dragSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
	{
		bool didDrag = fabsf(s_dragEnd.x - s_dragStart.x) > dragThreshold ||
			fabsf(s_dragEnd.y - s_dragStart.y) > dragThreshold;
		if (didDrag && offscreen_buffer)
		{
			// Compute clamped rect in screen space
			ImVec2 rMin(ImMin(s_dragStart.x, s_dragEnd.x), ImMin(s_dragStart.y, s_dragEnd.y));
			ImVec2 rMax(ImMax(s_dragStart.x, s_dragEnd.x), ImMax(s_dragStart.y, s_dragEnd.y));
			rMin.x = ImClamp(rMin.x, imgMin.x, imgMax.x);
			rMin.y = ImClamp(rMin.y, imgMin.y, imgMax.y);
			rMax.x = ImClamp(rMax.x, imgMin.x, imgMax.x);
			rMax.y = ImClamp(rMax.y, imgMin.y, imgMax.y);

			// Convert to local image coords
			ImVec2 localMin(rMin.x - imgMin.x, rMin.y - imgMin.y);
			ImVec2 localMax(rMax.x - imgMin.x, rMax.y - imgMin.y);

			// convert to framebuffer pixel coords
			const int fbW = offscreen_buffer->width;
			const int fbH = offscreen_buffer->height;
			auto toPx = [&](float lx, float ly) -> ImVec2
				{
					float u = (imgSize.x > 0.0f) ? (lx / imgSize.x) : 0.0f;
					float v = (imgSize.y > 0.0f) ? (ly / imgSize.y) : 0.0f;
					int px = static_cast<int>(std::roundf(u * fbW));
					int py = static_cast<int>(std::roundf((1.0f - v) * fbH));
					px = std::clamp(px, 0, fbW - 1);
					py = std::clamp(py, 0, fbH - 1);
					return { static_cast<float>(px), static_cast<float>(py) };
				};
			ImVec2 pMin = toPx(localMin.x, localMin.y);
			ImVec2 pMax = toPx(localMax.x, localMax.y);

			int x0 = static_cast<int>(ImMin(pMin.x, pMax.x));
			int y0 = static_cast<int>(ImMin(pMin.y, pMax.y));
			int x1 = static_cast<int>(ImMax(pMin.x, pMax.x));
			int y1 = static_cast<int>(ImMax(pMin.y, pMax.y));

			// Sample picking buffer on a grid to collect entities
			// TODO: Tune sampling pattern for better performance/accuracy tradeoff
			std::unordered_set<EntityID> picked{};
			const int stepX = std::max(1, (x1 - x0) / 50); // ~2500 samples worst-case
			const int stepY = std::max(1, (y1 - y0) / 50);

			auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
			for (int y = y0; y <= y1; y += stepY)
			{
				for (int x = x0; x <= x1; x += stepX)
				{
					auto [hit, entity] = renderer->PickEntityAt(x, y,
						EditorCamera::GetInstance().GetViewMatrix(),
						EditorCamera::GetInstance().GetProjectionMatrix());
					if (hit && entity != 0)
						picked.insert(entity);
				}
			}

			auto scene = SceneManager::GetInstance().GetActiveScene();
			if (!picked.empty())
			{
				if (ctrlDown)
					for (auto id : picked) Selection::Toggle(scene.get(), id);
				else
					Selection::Set(scene.get(), picked);
			}
			else
				if (!ctrlDown) Selection::Clear(scene.get());
		}
		else
		{
			if (offscreen_buffer && ImGui::IsItemHovered())
			{
				ImGuiIO& io = ImGui::GetIO();
				const float localX = io.MousePos.x - imgMin.x;
				const float localY = io.MousePos.y - imgMin.y;
				if (localX >= 0.0f && localY >= 0.0f && localX <= imgSize.x && localY <= imgSize.y)
				{
					const float u = (imgSize.x > 0.0f) ? (localX / imgSize.x) : 0.0f;
					const float v = (imgSize.y > 0.0f) ? (localY / imgSize.y) : 0.0f;
					const int px = static_cast<int>(u * offscreen_buffer->width);
					const int py = static_cast<int>((1.0f - v) * offscreen_buffer->height);

					auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
					auto [hit, entity] = renderer->PickEntityAt(std::clamp(px, 0, offscreen_buffer->width - 1),
						std::clamp(py, 0, offscreen_buffer->height - 1),
						EditorCamera::GetInstance().GetViewMatrix(),
						EditorCamera::GetInstance().GetProjectionMatrix());

					auto scene = SceneManager::GetInstance().GetActiveScene();
					if (hit && entity != 0)
					{
						if (ctrlDown) Selection::Toggle(scene.get(), entity);
						else Selection::SetSingle(scene.get(), entity);
					}
					else
						if (!ctrlDown) Selection::Clear(scene.get());
				}
			}
		}

		// End drag cycle
		s_dragSelecting = false;
	}

	//ObjectPicking(offscreen_buffer, imgMin, imgSize, overViewCube, s_orbiting);
	if (!(s_dragSelecting || (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
		(fabsf(s_dragEnd.x - s_dragStart.x) > dragThreshold || fabsf(s_dragEnd.y - s_dragStart.y) > dragThreshold))))
		ObjectPicking(offscreen_buffer, imgMin, imgSize, overViewCube, s_orbiting);

	ImGui::EndChild();

	// Dropping assets into viewport to load prefabs
	if (ImGui::BeginDragDropTarget()) { // Begin drag & drop target
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
			const char* cpath = static_cast<const char*>(payload->Data);

			std::filesystem::path path = cpath;

			if (path.extension() != ".prefab")
				EE_CORE_INFO("Ignored drop: {} (only .prefab is allowed here)", path.string());
			else
				PrefabManager::GetInstance().LoadPrefab(path);
		}
		ImGui::EndDragDropTarget(); // End drag & drop target
	}

	// Editor input flags
	Input::SetEditorInputActive(viewportFocused && viewportHovered);

	if (Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyDownEditor(GLFW_KEY_LEFT_SHIFT)
		&& Input::IsKeyPressedEditor(GLFW_KEY_P))
	{
		EditorGUI::s_state = EditorGUI::SimState::stopped;
		SceneManager::GetInstance().LoadTemp();
	}
	else if (Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyPressedEditor(GLFW_KEY_P))
	{
		EditorGUI::s_state = EditorGUI::SimState::playing;
		SceneManager::GetInstance().SaveTemp();
	}

	if (Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyPressedEditor(GLFW_KEY_Z))
		CommandHistory::GetInstance().Undo();

	if (Input::IsKeyDownEditor(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyPressedEditor(GLFW_KEY_Y))
		CommandHistory::GetInstance().Redo();
	

	EditorGUI::isPlaying = EditorGUI::s_state == EditorGUI::SimState::playing;

	Input::SetGameInputActive(EditorGUI::isPlaying && viewportFocused && viewportHovered);

	if (EditorGUI::isPlaying && Input::IsKeyPressed(GLFW_KEY_ESCAPE))
	{
		Window::SetCursorLockState(Window::CursorLockState::None);
		//EditorGUI::s_state = EditorGUI::SimState::stopped;
		//SceneManager::GetInstance().LoadTemp();

		//if (glfwRawMouseMotionSupported())
		//	glfwSetInputMode(glfwGetCurrentContext(), GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
		//glfwSetInputMode(glfwGetCurrentContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	ImGui::End();
}
