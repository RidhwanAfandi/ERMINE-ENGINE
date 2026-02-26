/* Start Header ************************************************************************/
/*!
\file       EditorCamera.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the implementation of the EditorCamera class.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "EditorCamera.h"

#include <glm/ext/matrix_transform.hpp>

#include "FrameController.h"
#include "Input.h"
#include "MathUtils.h"
#include "Renderer.h"
#include "GLFW/glfw3.h"

using namespace Ermine::editor;

/**
 * @brief Construct a new Editor Camera object
 * @param fov The field of view
 * @param aspectRatio The aspect ratio
 * @param near The near plane
 * @param far The far plane
 */
EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
	: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
{
	UpdateViewMatrix();
	UpdateProjectionMatrix();
}

/**
* @brief Update the camera based on input and time between frames
* @param deltaTime The time between frames
*/
void EditorCamera::Update()
{
	UpdateViewMatrix();
	UpdateProjectionMatrix();
}

/**
* @brief Set the perspective of the camera
* @param fov The field of view
* @param aspectRatio The aspect ratio
* @param nearClip The near plane
* @param farClip The far plane
*/
void EditorCamera::SetPerspective(float fov, float aspectRatio, float nearClip, float farClip)
{
	m_FOV = fov;
	m_AspectRatio = aspectRatio;
	m_NearClip = nearClip;
	m_FarClip = farClip;
	UpdateProjectionMatrix();
}

/**
* @brief Set the viewport size
* @param width The width of the viewport
* @param height The height of the viewport
*/
void EditorCamera::SetViewportSize(float width, float height)
{
	m_AspectRatio = width / height;

	UpdateProjectionMatrix();
}

/**
* @brief Update the view matrix based on the camera's position and orientation
*/
void EditorCamera::UpdateViewMatrix()
{
	// Calculate the new front vector
	Vector3D front;
	front.x = cos(radian(m_Yaw)) * cos(radian(m_Pitch));
	front.y = sin(radian(m_Pitch));
	front.z = sin(radian(m_Yaw)) * cos(radian(m_Pitch));

	Vector3D normalized_front;
	Vec3Normalize(normalized_front, front);
	m_Front = normalized_front;

	// Re-calculate the Right and Up vector
	Vector3D right;
	Vec3CrossProduct(right, m_Front, m_WorldUp);
	Vec3Normalize(right, right);
	m_Right = right;

	Vector3D up;
	Vec3CrossProduct(up, m_Right, m_Front);
	Vec3Normalize(up, up);
	m_Up = up;

	// Create the view matrix
	glm::mat4 tempLookAtMatrix = glm::lookAt(glm::vec3(m_Position.x, m_Position.y, m_Position.z), glm::vec3(m_Position.x + m_Front.x, m_Position.y + m_Front.y, m_Position.z + m_Front.z), glm::vec3(m_Up.x, m_Up.y, m_Up.z));
	m_ViewMatrix = Mtx44(&tempLookAtMatrix[0][0]);
	// Mtx44LookAt(m_ViewMatrix, m_Position, m_Position + m_Front, m_Up);
}

/**
* @brief Update the projection matrix based on the camera's perspective parameters
*/
void EditorCamera::UpdateProjectionMatrix()
{
	Mtx44Perspective(m_ProjectionMatrix, m_FOV, m_AspectRatio, m_NearClip, m_FarClip);
}

/**
* @brief Process keyboard input to move the camera
* @param deltaTime The time between frames
*/
void EditorCamera::ProcessKeyboardInput(float deltaTime)
{
	if (!Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
		return;
	
	float velocity = m_MovementSpeed * deltaTime;
	if (Input::IsKeyDownEditor(GLFW_KEY_LEFT_SHIFT))
		velocity *= 2.0f;

	if (Input::IsKeyDownEditor(GLFW_KEY_W))
		m_Position = m_Position + m_Front * velocity;
	if (Input::IsKeyDownEditor(GLFW_KEY_S))
		m_Position = m_Position - m_Front * velocity;
	if (Input::IsKeyDownEditor(GLFW_KEY_A))
		m_Position = m_Position - m_Right * velocity;
	if (Input::IsKeyDownEditor(GLFW_KEY_D))
		m_Position = m_Position + m_Right * velocity;
	if (Input::IsKeyDownEditor(GLFW_KEY_Q))
		m_Position = m_Position - m_WorldUp * velocity;
	if (Input::IsKeyDownEditor(GLFW_KEY_E))
		m_Position = m_Position + m_WorldUp * velocity;
}

/**
* @brief Process mouse movement to rotate the camera
*/
void EditorCamera::ProcessMouseMovement()
{
	// Only rotate camera if right mouse button is pressed
	if (Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT))
	{
		double xpos = Input::GetMouseX(), ypos = Input::GetMouseY();

		if (m_FirstMouse)
		{
			m_LastX = static_cast<float>(xpos);
			m_LastY = static_cast<float>(ypos);
			m_FirstMouse = false;
		}

		float xoffset = static_cast<float>(xpos) - m_LastX;
		float yoffset = m_LastY - static_cast<float>(ypos); // Reversed: y ranges bottom to top

		m_LastX = static_cast<float>(xpos);
		m_LastY = static_cast<float>(ypos);

		xoffset *= m_MouseSensitivity;
		yoffset *= m_MouseSensitivity;

		m_Yaw += xoffset;
		m_Pitch += yoffset;

		// Constrain pitch to avoid flipping
		m_Pitch = std::min(m_Pitch, 89.0f);
		m_Pitch = std::max(m_Pitch, -89.0f);
	}
	else
	{
		m_FirstMouse = true;
	}
}

/**
* @brief Process scroll wheel to zoom the camera
* @param yOffset The offset of the scroll wheel
*/
void EditorCamera::ProcessScrollWheel(float yOffset)
{
	m_FOV -= yOffset * FrameController::GetFixedDeltaTime();
	m_FOV = std::max(m_FOV, 1.0f);
	m_FOV = std::min(m_FOV, 45.0f);
	Input::ResetMouseScrollOffsetEditor();
}

/**
 * @brief Directly set the yaw and pitch of the camera
 * @param yawDeg The yaw angle in degrees
 * @param pitchDeg The pitch angle in degrees
 */
void EditorCamera::SetYawPitch(float yawDeg, float pitchDeg)
{
	m_Yaw = yawDeg;
	m_Pitch = std::min(std::max(pitchDeg, -89.0f), 89.0f);
	UpdateViewMatrix();
}

void EditorCamera::Focus(const Vector3D& target, float distance)
{
	const Vector3D diff = m_Position - target;
	float currentDist = Vec3Length(diff);
	if (distance <= 0.0f)
		distance = currentDist > 0.001f ? currentDist : 5.0f;

	Vector3D forward;
	forward.x = target.x - m_Position.x;
	forward.y = target.y - m_Position.y;
	forward.z = target.z - m_Position.z;

	float len = Vec3Length(forward);
	if (len > 1e-6f)
	{
		forward = forward / len;

		auto rad2deg = [](float r) { return r * 57.29577951308232f; }; // TODO: Need add to math library
		float yaw = rad2deg(std::atan2(forward.z, forward.x));
		float pitch = rad2deg(std::asin(std::clamp(forward.y, -1.0f, 1.0f)));

		SetYawPitch(yaw, pitch);
	}

	m_Position = target - m_Front * distance;
	UpdateViewMatrix();
}

void EditorCamera::OrbitAround(const Vector3D& pivot, float deltaX, float deltaY, float distance)
{
	const float rotSpeed = m_MouseSensitivity;
	m_Yaw += deltaX * rotSpeed;
	m_Pitch += -deltaY * rotSpeed;
	m_Pitch = std::min(std::max(m_Pitch, -89.0f), 89.0f);

	UpdateViewMatrix();

	m_Position = pivot - m_Front * distance;

	UpdateViewMatrix();
}
