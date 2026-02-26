/* Start Header ************************************************************************/
/*!
\file       GameCamera.h
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       11/03/2025
\brief      First-person game camera that attaches to entities

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "Systems.h"
#include "Matrix4x4.h"

namespace Ermine::graphics
{
    /**
     * @brief First-person game camera system that follows an entity
     */
    class CameraSystem : public System
    {
    public:
        /**
         * @brief Get the singleton instance
         */
         //static CameraSystem& GetInstance()
         //{
         //    static CameraSystem instance;
         //    return instance;
         //}

        CameraSystem() = default;

        /**
         * @brief Constructor with default perspective parameters (matching EditorCamera)
         */
         //CameraSystem(float fov = 45.0f, float aspectRatio = 16.0f / 9.0f,
         //    float nearClip = 0.1f, float farClip = 1000.0f);

         /**
          * @brief Update camera each frame
          */
        void Update();

        /**
         * @brief Set which entity the camera is attached to
         */
        void SetCameraEntity(EntityID entity);

        /**
         * @brief Get the current camera entity
         */
        EntityID GetCameraEntity() const { return m_CameraEntity; }

        /**
         * @brief Check if camera is attached to a valid entity
         */
        bool HasValidCamera();

        // Matrix getters
        const Mtx44& GetViewMatrix() const { return m_ViewMatrix; }
        const Mtx44& GetProjectionMatrix() const { return m_ProjectionMatrix; }

        // Camera controls
        void SetPosition(const Vector3D& position) { m_Position = position; }
        Vector3D GetPosition() const { return m_Position; }

        void SetPerspective(float fov, float aspectRatio, float nearClip, float farClip);
        void SetViewportSize(float width, float height);

        // Get camera vectors
        Vector3D GetForward() const { return m_Front; }
        Vector3D GetRight() const { return m_Right; }
        Vector3D GetUp() const { return m_Up; }

        float GetNearClip() const { return m_NearClip; }
        float GetFarClip() const { return m_FarClip; }

    private:
        void UpdateViewMatrix();
        void UpdateProjectionMatrix();
        void UpdateTransformFromEntity();
        void InitializeOrientationFromEntity();

        // Camera entity
        EntityID m_CameraEntity = 0;

        // Camera vectors
        Vector3D m_Position{ 0.0f, 1.7f, 0.0f }; // Eye height
        Vector3D m_Front{ 0.0f, 0.0f, -1.0f };
        Vector3D m_Up{ 0.0f, 1.0f, 0.0f };
        Vector3D m_Right{ 1.0f, 0.0f, 0.0f };
        Vector3D m_WorldUp{ 0.0f, 1.0f, 0.0f };

        // Camera control (matching EditorCamera defaults)
        float m_MouseSensitivity = 0.1f;
        float m_Yaw = -90.0f;
        float m_Pitch = 0.0f;

        // Perspective parameters
        float m_FOV = 0.0f;
        float m_AspectRatio = 0.0f;
        float m_NearClip = 0.0f;
        float m_FarClip = 0.0f;

        // Matrices
        Mtx44 m_ViewMatrix;
        Mtx44 m_ProjectionMatrix;

        // Offset from entity position (eye height)
        Vector3D m_CameraOffset{ 0.0f, 1.7f, 0.0f };
    };
}