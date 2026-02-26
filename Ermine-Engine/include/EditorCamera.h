/* Start Header ************************************************************************/
/*!
\file       EditorCamera.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       11/03/2025
\brief      This file contains the declaration of the EditorCamera class.
            Provides camera functionality for the editor view.
Copyright (C) 2025 DigiPen Institute of Technology.
*/
/* End Header **************************************************************************/

#pragma once
#include "Matrix4x4.h"

namespace Ermine::editor
{
    class EditorCamera
    {
    public:
		/**
		 * @brief Get the singleton instance of the EditorCamera
		 * @return EditorCamera& The singleton instance
		 */
		static EditorCamera& GetInstance()
		{
			static EditorCamera instance;
			return instance;
		}

		// Delete copy constructor and assignment operator
		EditorCamera(const EditorCamera&) = delete;
		EditorCamera& operator=(const EditorCamera&) = delete;

		/**
		 * @brief Construct a new Editor Camera object
		 * @param fov The field of view
		 * @param aspectRatio The aspect ratio
		 * @param near The near plane
		 * @param far The far plane
		 */
        EditorCamera(float fov = 45.0f, float aspectRatio = 16.0f/9.0f, float near = 1.0f, float far = 1000.0f);

		/**
		 * @brief Update the camera
		 */
        void Update();

        // Getters for view and projection matrices
		/**
		 * @brief Get the view matrix
		 * @return const Mtx44& The view matrix
		 */
        const Mtx44& GetViewMatrix() const { return m_ViewMatrix;}
		/**
		 * @brief Get the projection matrix
		 * @return const Mtx44& The projection matrix
		 */
		const Mtx44& GetProjectionMatrix() const { return m_ProjectionMatrix; }

        // Camera controls
		/**
		 * @brief Set the position of the camera
		 * @param position The new position
		 */
        void SetPosition(const Vector3D& position) { m_Position = position; }
		/**
		 * @brief Get the position of the camera
		 * @return Vector3D The position of the camera
		 */
		Vector3D GetPosition() const { return m_Position; }

		/**
		 * @brief Set the perspective of the camera
		 * @param fov The field of view
		 * @param aspectRatio The aspect ratio
		 * @param near The near plane
		 * @param far The far plane
		 */
        void SetPerspective(float fov, float aspectRatio, float near, float far);
		/**
		 * @brief Set the viewport size
		 * @param width The width of the viewport
		 * @param height The height of the viewport
		 */
        void SetViewportSize(float width, float height);

		/**
		 * @brief Process keyboard input to move the camera
		 * @param deltaTime The time between frames
		 */
		void ProcessKeyboardInput(float deltaTime);
		/**
		 * @brief Process mouse movement to rotate the camera
		 */
		void ProcessMouseMovement();
		/**
		 * @brief Process scroll wheel to zoom the camera
		 * @param yOffset The offset of the scroll wheel
		 */
		void ProcessScrollWheel(float yOffset);
		/**
		 * @brief Directly set the yaw and pitch of the camera
		 * @param yawDeg The yaw angle in degrees
		 * @param pitchDeg The pitch angle in degrees
		 */
		void SetYawPitch(float yawDeg, float pitchDeg);
		/**
		 * @brief Focus the camera on a target point
		 * @param target The target point to focus on
		 * @param distance Optional distance from the target (default -1.0f to maintain current distance)
		 */
		void Focus(const Vector3D& target, float distance = -1.0f);
		/**
		 * @brief Orbit the camera around a target point
		 * @param pivot The target point to orbit around
		 * @param deltaX The change in x (yaw)
		 * @param deltaY The change in y (pitch)
		 * @param distance The distance from the target
		 */
		void OrbitAround(const Vector3D& pivot, float deltaX, float deltaY, float distance);

		/**
		 * @brief Get the near and far clip planes
		 */
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip() const { return m_FarClip; }

    private:
		/**
		 * @brief Update the view matrix
		 */
		void UpdateViewMatrix();
		/**
		 * @brief Update the projection matrix
		 */
		void UpdateProjectionMatrix();

        
        // Camera attributes
        Vector3D m_Position{0.0f, 0.0f, 3.0f};
        Vector3D m_Front{0.0f, 0.0f, -1.0f};
        Vector3D m_Up{0.0f, 1.0f, 0.0f};
        Vector3D m_Right{1.0f, 0.0f, 0.0f};
        Vector3D m_WorldUp{0.0f, 1.0f, 0.0f};
    
        // Camera options
        float m_MovementSpeed = 15.0f;
        float m_MouseSensitivity = 0.1f;
    
        // Euler angles
        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;

        // Perspective parameters
        float m_FOV;
        float m_AspectRatio;
        float m_NearClip;
        float m_FarClip;
    
        // Mouse tracking
        bool m_FirstMouse = true;
        float m_LastX = 0.0f;
        float m_LastY = 0.0f;
    
        // Matrices
        Mtx44 m_ViewMatrix;
        Mtx44 m_ProjectionMatrix;
    };
}

