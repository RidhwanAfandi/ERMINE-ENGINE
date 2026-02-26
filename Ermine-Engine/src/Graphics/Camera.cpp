/* Start Header ************************************************************************/
/*!
\file       Camera.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       10/03/2025
\brief      This reflects the brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Camera.h"

#include "Components.h"
#include "ECS.h"
#include "MathUtils.h"

using namespace Ermine::graphics;

void Camera::Update(GLFWwindow* windowContext)
{
    for (auto& entity : m_Entities)
    {
        if (!ECS::GetInstance().IsEntityValid(activeCameraEntity)) // Find the active camera if we don't have one
        {
            auto& camera = ECS::GetInstance().GetComponent<CameraComponent>(entity);
            if (camera.isPrimary)
            {
                activeCameraEntity = entity;
                return;
            }
        }

        auto& camera = ECS::GetInstance().GetComponent<CameraComponent>(entity);
        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);

        // Update projection matrix when window resizes
        int width, height;
        glfwGetFramebufferSize(windowContext, &width, &height);
        camera.aspectRatio = static_cast<float>(width) / static_cast<float>(height);

        Mtx44Perspective(projectionMatrix, camera.fov, camera.aspectRatio, camera.nearPlane, camera.farPlane);

        // Calculate view matrix from transform
        Vector3D forward = CalculateForwardVector(transform.rotation);
        Vector3D up = CalculateUpVector(transform.rotation);

        Mtx44LookAt(viewMatrix,transform.position, transform.position + forward, up);
    }
}

Ermine::Vector3D Camera::CalculateUpVector(const Quaternion& rotation) const
{
    double qx = rotation.x;
    double qy = rotation.y;
    double qz = rotation.z;
    double qw = rotation.w;

    double len = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (len < 1e-12)
        return Vector3D(0.0f, 1.0f, 0.0f);

    qx /= len;
    qy /= len;
    qz /= len;
    qw /= len;

    // Basis up vector (engine uses +Y as up for zero rotation)
    const double vx = 0.0;
    const double vy = 1.0;
    const double vz = 0.0;

    // t = 2 * cross(q_xyz, v)
    double tx = 2.0 * (qy * vz - qz * vy);
    double ty = 2.0 * (qz * vx - qx * vz);
    double tz = 2.0 * (qx * vy - qy * vx);

    // cross(q_xyz, t)
    double cx = qy * tz - qz * ty;
    double cy = qz * tx - qx * tz;
    double cz = qx * ty - qy * tx;

    // v' = v + q_w * t + cross(q_xyz, t)
    double rx = vx + qw * tx + cx;
    double ry = vy + qw * ty + cy;
    double rz = vz + qw * tz + cz;

    return { static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz) };
}

Ermine::Vector3D Camera::CalculateForwardVector(const Quaternion& rotation) const
{
    double qx = rotation.x;
    double qy = rotation.y;
    double qz = rotation.z;
    double qw = rotation.w;

    // Normalize quaternion to avoid scaling the vector by non-unit quaternions
    double len = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (len < 1e-12)
        return Vector3D(0.0f, 0.0f, 1.0f);
    
    qx /= len;
    qy /= len;
    qz /= len;
    qw /= len;

    // Basis forward vector (engine uses +Z as forward for zero rotation)
    const double vx = 0.0;
    const double vy = 0.0;
    const double vz = 1.0;

    // t = 2 * cross(q_xyz, v)
    double tx = 2.0 * (qy * vz - qz * vy);
    double ty = 2.0 * (qz * vx - qx * vz);
    double tz = 2.0 * (qx * vy - qy * vx);

    // v' = v + q_w * t + cross(q_xyz, t)
    double cx = qy * tz - qz * ty;
    double cy = qz * tx - qx * tz;
    double cz = qx * ty - qy * tx;

    double rx = vx + qw * tx + cx;
    double ry = vy + qw * ty + cy;
    double rz = vz + qw * tz + cz;

    return {static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rz)};
}
