/* Start Header ************************************************************************/
/*!
\file       Camera.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       10/03/2025
\brief      This reflects the brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Systems.h"
#include "Matrix4x4.h"
#include "Window.h"

namespace Ermine::graphics
{
    class Camera : public System
    {
        Mtx44 viewMatrix;
        Mtx44 projectionMatrix;
        EntityID activeCameraEntity;
    public:
        void Update(GLFWwindow* windowContext);

        Mtx44 GetViewMatrix() const { return viewMatrix; }
        Mtx44 GetProjectionMatrix() const { return projectionMatrix; }
        EntityID GetActiveCameraEntity() const { return activeCameraEntity; }

        Vector3D CalculateForwardVector(const Quaternion& rotation) const;
        Vector3D CalculateUpVector(const Quaternion& rotation) const;
    };
}
