/* Start Header ************************************************************************/
/*!
\file       GPUParticles.h
\author     Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\date       02/02/2026
\brief      GPU-based particle system

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "ECS.h"
#include "Shader.h"
#include "Components.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

namespace Ermine {

    // GPU Particle data structure (must match shader)
    struct GPUParticle {
        glm::vec4 position;     // xyz = position, w = life
        glm::vec4 velocity;     // xyz = velocity, w = age
        glm::vec4 color;        // rgba
        glm::vec4 params;       // x = sizeStart, y = sizeEnd, z = randomSeed, w = unused
    };

    // GPU Particle System
    class GPUParticleSystem : public System {
    public:
        /*!***********************************************************************
        \brief
            Initializes the GPU particle system with required shaders.
        *************************************************************************/
        void Init();

        /*!***********************************************************************
        \brief
            Updates all particle emitters using compute shader.
        \param[in] dt
            Delta time since last frame.
        *************************************************************************/
        void Update(float dt);

        /*!***********************************************************************
        \brief
            Renders all active particle systems.
        \param[in] view
            View matrix.
        \param[in] projection
            Projection matrix.
        \param[in] cameraPos
            Camera position for billboarding.
        *************************************************************************/
        void Render(const Mtx44& view, const Mtx44& projection, const Vec3& cameraPos);

        /*!***********************************************************************
        \brief
            Cleans up all GPU resources.
        *************************************************************************/
        void Cleanup();

        /*!***********************************************************************
        \brief
            Checks if there are any active emitters to render.
        *************************************************************************/
        bool HasActiveEmitters() const;

        /*!***********************************************************************
        \brief
            Initializes particle buffer for a specific emitter.
        \param[in] emitter
            The emitter component to initialize.
        *************************************************************************/
        void InitializeEmitter(GPUParticleEmitter& emitter);

        /*!***********************************************************************
        \brief
            Destroys particle buffer for a specific emitter.
        \param[in] emitter
            The emitter component to cleanup.
        *************************************************************************/
        void DestroyEmitter(GPUParticleEmitter& emitter);

        /*!***********************************************************************
        \brief
            Renders debug visualization for particle emitters.
        \param[in] view
            View matrix.
        \param[in] projection
            Projection matrix.
        *************************************************************************/
        void RenderDebug(const Mtx44& view, const Mtx44& projection);

    private:
        std::shared_ptr<graphics::Shader> m_ComputeShader;
        std::shared_ptr<graphics::Shader> m_RenderShader;

        unsigned int m_QuadVAO = 0;
        unsigned int m_QuadVBO = 0;
        unsigned int m_QuadEBO = 0;
        unsigned int m_SmokeNoiseTex = 0;
        unsigned int m_SmokeDistortTex = 0;
        unsigned int m_SmokePuffTex = 0;
        unsigned long long m_SmokeNoiseHandle = 0;
        unsigned long long m_SmokeDistortHandle = 0;
        unsigned long long m_SmokePuffHandle = 0;

        float m_ElapsedTime = 0.0f;
        bool m_Initialized = false;
    };

} // namespace Ermine
