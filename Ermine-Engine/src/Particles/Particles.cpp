/* Start Header ************************************************************************/
/*!
\file       Particles.cpp
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       26/01/2026
\brief      This file contains definitions for ParticleSystem, ParticleEmitter and ParticlesImGUI.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Particles.h"

namespace Ermine {
    void ParticleSystem::Init(std::shared_ptr<graphics::Shader> shader)
    {
        m_QuadMesh = graphics::GeometryFactory::CreateQuad(1.0f, 1.0f);
        m_Shader = shader;
    }

    void ParticleSystem::Update(float dt)
    {
        auto& ecs = ECS::GetInstance();
        std::vector<size_t> toRemove;

        // Update emitters
        for (auto entity : m_Entities)
        {
            if (!ecs.HasComponent<ParticleEmitter>(entity))
                continue;

            if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
            {
                const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                if (!meta.selfActive)
                    continue;
            }

            auto& emitter = ecs.GetComponent<ParticleEmitter>(entity);
            if (!emitter.active)
                continue;

            // constantly emit particles
            //emitter.timeAccumulator += dt;
            //float emitInterval = 1.0f / emitter.emissionRate;

            //while (emitter.timeAccumulator >= emitInterval)
            //{
            //    emitter.timeAccumulator -= emitInterval;
            //    auto& transform = ecs.GetComponent<Transform>(entity);
            //    Emit(emitter, transform.position, emitter.velocity, emitter.particleLifetime, emitter.particleSize, emitter.color);
            //}
        }

        // update particle entities
        for (size_t i = 0; i < m_Particles.size();)
        {
            auto& p = m_Particles[i];
            p.age += dt;

            if (p.age >= p.lifetime)
            {
                ecs.DestroyEntity(p.entity);
                m_Particles[i] = m_Particles.back();
                m_Particles.pop_back();
                --pcount;
                continue;
            }

            auto& transform = ecs.GetComponent<Transform>(p.entity);
            transform.position += p.velocity * dt;
            ++i;
        }
    }

    void ParticleSystem::Emit(const ParticleEmitter& emitter, const Vec3& pos, const Vec3& vel, float lifetime, float size)
    {
        auto& ecs = ECS::GetInstance();
        auto& assetManager = AssetManager::GetInstance();

        EntityID e = ecs.CreateEntity();

        Transform transform{};
        transform.position = pos;
        transform.scale = Vec3(size, size, size);
        ecs.AddComponent(e, transform);

        ecs.AddComponent(e, m_QuadMesh);

        // Load texture
        std::shared_ptr<graphics::Texture> texture = nullptr;
        if (!emitter.textureName.empty())
            texture = assetManager.GetTexture(emitter.textureName);
        if (!texture || !texture->IsValid())
            texture = m_DefaultTexture;

        //EE_CORE_INFO("Particle texture: {}", emitter.textureName);
        //if (texture)
        //    EE_CORE_INFO("Texture valid? {}", texture->IsValid() ? "yes" : "NO");
        //else
        //    EE_CORE_INFO("Texture is nullptr!");

        auto gfxMaterial = std::make_shared<graphics::Material>(m_Shader);
        if (texture && texture->IsValid())
        {
            gfxMaterial->SetTexture("materialAlbedoMap", texture);
            gfxMaterial->SetBool("materialHasAlbedoMap", true);
        }

        Material material(gfxMaterial);
        ecs.AddComponent(e, material);

        Particle p;
        p.entity = e;
        p.velocity = vel;
        p.lifetime = lifetime;
        p.age = 0.0f;

        m_Particles.push_back(p);
        ++pcount;
    }

    void ParticleSystem::Clear()
    {
        for (auto& p : m_Particles)
            ECS::GetInstance().DestroyEntity(p.entity);

        m_Particles.clear();
        pcount = 0;
    }

    int ParticleSystem::GetParticleCount() const
    {
        return pcount;
    }

    void ParticlesImGUI::Update() {}

    void ParticlesImGUI::Render()
    {
        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
        {
            ImGui::End();
            return;
        }

        EntityID selected = SceneManager::GetInstance().EnsureActiveScene().GetSelectedEntity();
        if (selected == 0) {
            ImGui::Text("No entity selected");
            ImGui::End();
            return;
        }

        auto& ecs = ECS::GetInstance();
        if (ecs.HasComponent<ParticleEmitter>(selected))
        {
            auto& emitter = ecs.GetComponent<ParticleEmitter>(selected);
            ImGui::Checkbox("Active", &emitter.active);
            ImGui::DragFloat("Emission Rate", &emitter.emissionRate, 1.0f, 0.0f, 500.0f);
            ImGui::DragFloat("Lifetime", &emitter.particleLifetime, 0.1f, 0.1f, 10.0f);
            ImGui::DragFloat("Size", &emitter.particleSize, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat3("Velocity", reinterpret_cast<float*>(&emitter.velocity), 0.1f);
            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&emitter.position), 0.1f);
            ImGui::InputInt("Particle Count", &count);

            auto& assetManager = AssetManager::GetInstance();
            const auto& textureFiles = assetManager.GetLoadedTextures();

            ImGui::Text("Texture");
            if (ImGui::BeginCombo("##textureCombo", emitter.textureName.c_str()))
            {
                for (const auto& [name, texturePtr] : textureFiles)
                {
                    bool isSelected = (emitter.textureName == name);
                    if (ImGui::Selectable(name.c_str(), isSelected))
                        emitter.textureName = name;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const char* presetNames[] = { "Default", "Spread Out", "Fireflies" };
            int currentPresetIndex = static_cast<int>(m_CurrentPreset);

            if (ImGui::Combo("Preset", &currentPresetIndex, presetNames, IM_ARRAYSIZE(presetNames)))
            {
                m_CurrentPreset = static_cast<PresetType>(currentPresetIndex);

                switch (m_CurrentPreset)
                {
                case PresetType::Default:
                    emitter.velocity = { 0.0f, 2.0f, 0.0f };
                    emitter.emissionRate = 50.0f;
                    emitter.particleLifetime = 2.0f;
                    emitter.particleSize = 0.2f;
                    emitter.textureName = "../Resources/Textures/greybox_red_solid.png";
                    emitter.position = { 0.0f, 0.0f, 0.0f };
                    break;

                case PresetType::SpreadOut:
                    emitter.velocity = { 0.0f, 2.5f, 0.0f };
                    emitter.emissionRate = 80.0f;
                    emitter.particleLifetime = 1.5f;
                    emitter.particleSize = 0.25f;
                    emitter.textureName = "../Resources/Textures/greybox_orange_solid.png";
                    emitter.position = { 0.0f, 0.0f, 0.0f };
                    break;

                case PresetType::Fireflies:
                    emitter.velocity = { 0.0f, 1.0f, 0.0f };
                    emitter.emissionRate = 30.0f;
                    emitter.particleLifetime = 3.5f;
                    emitter.particleSize = 0.15f;
                    emitter.textureName = "../Resources/Textures/greybox_yellow_solid.png";
                    emitter.position = { 0.0f, 0.5f, 0.0f };
                    break;
                }

                // load the preset�s texture
                m_SelectedTexture = AssetManager::GetInstance().GetTexture(emitter.textureName);
            }

            if (ImGui::Button("Emit"))
            {
                //EntityID selected = SceneManager::GetInstance().EnsureActiveScene().GetSelectedEntity();

                if (ecs.HasComponent<Transform>(selected) && ecs.HasComponent<ParticleEmitter>(selected))
                {
                    //auto& emitter = ecs.GetComponent<ParticleEmitter>(selected);
                    auto& transform = ecs.GetComponent<Transform>(selected);

                    auto particleSystem = ecs.GetSystem<ParticleSystem>();
                    for (int i = 0; i < count; ++i)
                    {
                        Vec3 pos = emitter.position;
                        Vec3 vel = emitter.velocity;
                        float lifetime = emitter.particleLifetime;
                        float size = emitter.particleSize;

                        // add preset specific behaviour here
                        switch (m_CurrentPreset)
                        {
                        case PresetType::SpreadOut:
                            vel.x += ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
                            vel.z += ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
                            break;

                        case PresetType::Fireflies:
                            pos.x += ((rand() % 100) / 100.0f - 0.5f) * 2.0f;
                            pos.y += ((rand() % 100) / 100.0f) * 2.0f;
                            vel = {
                                ((rand() % 100) / 100.0f - 0.5f) * 0.5f,
                                ((rand() % 100) / 100.0f) * 1.0f,
                                ((rand() % 100) / 100.0f - 0.5f) * 0.5f
                            };
                            lifetime = 3.0f + (rand() % 100) / 100.0f * 2.0f;
                            size = 0.1f + (rand() % 100) / 100.0f * 0.2f;
                            break;

                        default:
                            break;
                        }

                        if (emitter.active)
                            particleSystem->Emit(emitter, transform.position + pos, vel, lifetime, size);
                    }
                }
            }
        }
        else
        {
            ImGui::Text("Select an entity with a Particle Emitter Component.");
        }

        ImGui::End();
    }
}