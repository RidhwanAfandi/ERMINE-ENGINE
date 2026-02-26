/* Start Header ************************************************************************/
/*!
\file       Particles.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       26/01/2026
\brief      This file contains declarations for ParticleSystem, ParticleEmitter and ParticlesImGUI.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Shader.h"
#include "Renderer.h"
#include "ECS.h"
#include "Components.h"
#include "ImGuiUIWindow.h"
#include "imgui.h"
#include "AssetManager.h"
#include"SceneManager.h"
#include "GeometryFactory.h"

namespace Ermine {
    class ParticleSystem : public System
    {
    public:
        /*!***********************************************************************
        \brief
            Initializes the particle system.
        \param[in] shader
            Shared pointer to the shader used for rendering particles.
        *************************************************************************/
        void Init(std::shared_ptr<graphics::Shader> shader);
        /*!***********************************************************************
        \brief
            Updates all active particles in the system.
        \param[in] dt
            Delta time.
        *************************************************************************/
        void Update(float dt);
        /*!***********************************************************************
        \brief
           Emits a new particle from a given emitter with specified properties.
        \param[in] emitter
           Reference to the emitter component containing emission parameters.
        \param[in] pos
           World-space position where the particle is spawned.
        \param[in] vel
           Initial velocity of the particle.
        \param[in] lifetime
           Lifetime duration of the particle in seconds.
        \param[in] size
           Size of the particle quad.
        *************************************************************************/
        void Emit(const ParticleEmitter& emitter, const Vec3& pos, const Vec3& vel, float lifetime, float size);
        /*!***********************************************************************
        \brief
           Clears all active particles and resets particle data.
        *************************************************************************/
        void Clear();
        /*!***********************************************************************
        \brief
            Retrieves the total number of active particles in the system.
        \return
            Integer count of currently alive particles.
        *************************************************************************/
        int GetParticleCount() const;
    private:
        struct Particle {
            EntityID entity = 0;
            EntityID emitterID = 0;
            Vec3 velocity = { 0.0f, 0.0f, 0.0f };
            float lifetime = 0.0f;
            float age = 0.0f;
        };

        std::vector<Particle> m_Particles;
        Mesh m_QuadMesh;
        std::shared_ptr<graphics::Shader> m_Shader;
        std::shared_ptr<graphics::Texture> m_DefaultTexture;

        int pcount = 0;
    };

    class ParticlesImGUI : public ImGUIWindow
    {
    public:
        ParticlesImGUI() : ImGUIWindow("Particle Editor") {}

        /*!***********************************************************************
        \brief
           Update logic for the Particles ImGui window.
       *************************************************************************/
        void Update() override;
        /*!***********************************************************************
        \brief
           Render the ImGui window for controlling particles.
       *************************************************************************/
        void Render() override;

    private:
        int count = 1;
        std::shared_ptr<graphics::Texture> m_SelectedTexture = nullptr;

        // Presets
        enum class PresetType { Default, SpreadOut, Fireflies };
        PresetType m_CurrentPreset = PresetType::Default;
    };
}
