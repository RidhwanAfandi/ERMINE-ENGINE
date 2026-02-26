/* Start Header ************************************************************************/
/*!
\file       GameCamera.cpp
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       11/03/2025
\brief      First-person game camera implementation

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "CameraSystem.h"
#include "ECS.h"
#include "Components.h"
#include "Input.h"
#include <glm/ext/matrix_transform.hpp>

namespace Ermine::graphics
{
    // Private constructor for singleton
    //CameraSystem::CameraSystem(float fov, float aspectRatio, float nearClip, float farClip)
    //    : m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
    //{
    //    UpdateProjectionMatrix();
    //    UpdateViewMatrix();
    //}

    void CameraSystem::Update()
    {
        if (!HasValidCamera())
        {
            EE_CORE_ERROR("Missing a valid Camera within scene!");
            return;
        }

        auto& ecs = ECS::GetInstance();
        auto& transform = ecs.GetComponent<Transform>(m_CameraEntity);
        auto& camComp = ecs.GetComponent<CameraComponent>(m_CameraEntity);

        // Sync perspective from component
        m_FOV = camComp.fov;
        m_AspectRatio = camComp.aspectRatio;
        m_NearClip = camComp.nearPlane;
        m_FarClip = camComp.farPlane;
        UpdateProjectionMatrix();

        Vec3 worldPos = transform.position;
		Quaternion worldRot = transform.rotation;
        if (ecs.HasComponent<GlobalTransform>(m_CameraEntity))
        {
			const auto& g = ecs.GetComponent<GlobalTransform>(m_CameraEntity);
			worldPos = g.GetWorldPosition();
			worldRot = g.GetWorldRotation();
        }
        m_Position = worldPos;

        // Rotate basis vectors by the entity quaternion
        const Quaternion& q = worldRot;

        auto rotateVecByQuat = [](const Vector3D& v, const Quaternion& r)
            {
                // Normalize quaternion
                double qx = r.x, qy = r.y, qz = r.z, qw = r.w;
                double len = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
                if (len < 1e-12) return v;
                qx /= len; qy /= len; qz /= len; qw /= len;

                // v' = v + q_w * t + cross(q_xyz, t), where t = 2 * cross(q_xyz, v)
                const double vx = v.x, vy = v.y, vz = v.z;
                const double tx = 2.0 * (qy * vz - qz * vy);
                const double ty = 2.0 * (qz * vx - qx * vz);
                const double tz = 2.0 * (qx * vy - qy * vx);
                const double cx = qy * tz - qz * ty;
                const double cy = qz * tx - qx * tz;
                const double cz = qx * ty - qy * tx;
                return Vector3D{
                    static_cast<float>(vx + qw * tx + cx),
                    static_cast<float>(vy + qw * ty + cy),
                    static_cast<float>(vz + qw * tz + cz)
                };
            };

        // Engine forward = +Z, up = +Y
        Vector3D fwd = rotateVecByQuat(Vector3D{ 0.0f, 0.0f, 1.0f }, q);
        //Vector3D up = rotateVecByQuat(Vector3D{ 0.0f, 1.0f, 0.0f }, q);

        // Normalize and build basis
        Vector3D nf, nr, nu;
        Vec3Normalize(nf, fwd);
        m_Front = nf;

        Vec3CrossProduct(nr, m_Front, m_WorldUp);
        Vec3Normalize(nr, nr);
        m_Right = nr;

        Vec3CrossProduct(nu, m_Right, m_Front);
        Vec3Normalize(nu, nu);
        m_Up = nu;

        UpdateViewMatrix();
    }

    void CameraSystem::SetCameraEntity(EntityID entity)
    {
        m_CameraEntity = entity;
        if (entity != 0)
        {
            UpdateTransformFromEntity();
            
            // Initialize yaw and pitch from default values
            InitializeOrientationFromEntity();
        }
    }

    bool CameraSystem::HasValidCamera()
    {
        // Prefer primary camera; otherwise take the first valid
        auto& ecs = ECS::GetInstance();
        EntityID firstValid = 0, primary = 0;

        for (auto id : m_Entities)
        {
            if (!ecs.IsEntityValid(id)) continue;
            if (!ecs.HasComponent<Transform>(id) || !ecs.HasComponent<CameraComponent>(id)) continue;

            if (!firstValid) firstValid = id;
            const auto& cam = ecs.GetComponent<CameraComponent>(id);
            if (cam.isPrimary) { primary = id; break; }
        }

        EntityID chosen = primary ? primary : firstValid;
        if (chosen)
        {
            if (m_CameraEntity != chosen)
            {
                m_CameraEntity = chosen;
                UpdateTransformFromEntity();
                InitializeOrientationFromEntity();
            }
            return true;
        }
        return false;
    }

    void CameraSystem::SetPerspective(float fov, float aspectRatio, float nearClip, float farClip)
    {
        m_FOV = fov;
        m_AspectRatio = aspectRatio;
        m_NearClip = nearClip;
        m_FarClip = farClip;
        UpdateProjectionMatrix();
    }

    void CameraSystem::SetViewportSize(float width, float height)
    {
        if (height > 0.0f)
        {
            m_AspectRatio = width / height;
            UpdateProjectionMatrix();
        }
    }

    void CameraSystem::UpdateViewMatrix()
    {
        glm::mat4 tempLookAtMatrix = glm::lookAt(
            glm::vec3(m_Position.x, m_Position.y, m_Position.z),
            glm::vec3(m_Position.x + m_Front.x, m_Position.y + m_Front.y, m_Position.z + m_Front.z),
            glm::vec3(m_Up.x, m_Up.y, m_Up.z)
        );
        m_ViewMatrix = Mtx44(&tempLookAtMatrix[0][0]);
    }

    void CameraSystem::UpdateProjectionMatrix()
    {
        Mtx44Perspective(m_ProjectionMatrix, DegToRad(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
    }

    void CameraSystem::UpdateTransformFromEntity()
    {
        auto& ecs = ECS::GetInstance();
        auto& transform = ecs.GetComponent<Transform>(m_CameraEntity);

        // Update camera position from entity transform (with eye height offset)
        if (ecs.HasComponent<GlobalTransform>(m_CameraEntity))
        {
	        const auto& g = ecs.GetComponent<GlobalTransform>(m_CameraEntity);
            m_Position = g.GetWorldPosition();
        }
        else
			m_Position = transform.position;
    }

    void CameraSystem::InitializeOrientationFromEntity()
    {
        // Recalculate camera vectors using default orientation
        Vector3D front;
        front.x = cos(DegToRad(m_Yaw)) * cos(DegToRad(m_Pitch));
        front.y = sin(DegToRad(m_Pitch));
        front.z = sin(DegToRad(m_Yaw)) * cos(DegToRad(m_Pitch));

        Vector3D normalized_front;
        Vec3Normalize(normalized_front, front);
        m_Front = normalized_front;

        Vector3D right;
        Vec3CrossProduct(right, m_Front, m_WorldUp);
        Vec3Normalize(right, right);
        m_Right = right;

        Vector3D up;
        Vec3CrossProduct(up, m_Right, m_Front);
        Vec3Normalize(up, up);
        m_Up = up;
    }
}
