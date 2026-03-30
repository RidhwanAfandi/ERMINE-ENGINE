/* Start Header ************************************************************************/
/*!
\file       GPUParticles.cpp
\author     Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\date       02/02/2026
\brief      GPU-based particle system implementation

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "GPUParticles.h"
#include "AssetManager.h"
#include "Logger.h"
#include "FrameController.h"
#include "Renderer.h"
#include "Matrix4x4.h"
#include <cstdlib>
#include <algorithm>
#include <random>

namespace Ermine {

    void GPUParticleSystem::Init()
    {
        // Load compute shader
        m_ComputeShader = AssetManager::GetInstance().LoadShader(
            "../Resources/Shaders/orb_particles_compute.glsl");

        if (!m_ComputeShader || !m_ComputeShader->IsValid()) {
            EE_CORE_ERROR("Failed to load orb particles compute shader!");
            return;
        }

        // Load render shader
        m_RenderShader = AssetManager::GetInstance().LoadShader(
            "../Resources/Shaders/particle_emitter_vertex.glsl",
            "../Resources/Shaders/particle_emitter_fragment.glsl");

        if (!m_RenderShader || !m_RenderShader->IsValid()) {
            EE_CORE_ERROR("Failed to load orb particles render shader!");
            return;
        }

        // Create a quad for instanced rendering
        if (m_QuadVAO == 0) {
            struct QuadVertex {
                float x, y;
                float u, v;
            };

            const QuadVertex quadVertices[] = {
                { -0.5f, -0.5f, 0.0f, 0.0f },
                {  0.5f, -0.5f, 1.0f, 0.0f },
                {  0.5f,  0.5f, 1.0f, 1.0f },
                { -0.5f,  0.5f, 0.0f, 1.0f }
            };

            const unsigned int quadIndices[] = { 0, 1, 2, 0, 2, 3 };

            glGenVertexArrays(1, &m_QuadVAO);
            glBindVertexArray(m_QuadVAO);

            glGenBuffers(1, &m_QuadVBO);
            glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

            glGenBuffers(1, &m_QuadEBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), (void*)(sizeof(float) * 2));

            glBindVertexArray(0);
        }

        // Create 3D noise textures for smoke (bindless if available)
        if (m_SmokeNoiseTex == 0 || m_SmokeDistortTex == 0 || m_SmokePuffTex == 0) {
            constexpr int kNoiseSize = 32;
            auto buildNoise = [kNoiseSize](unsigned int& tex, int seed) {
                std::vector<unsigned char> noise(kNoiseSize * kNoiseSize * kNoiseSize);
                std::mt19937 rng(seed);
                std::uniform_int_distribution<int> dist(0, 255);
                for (auto& v : noise) {
                    v = static_cast<unsigned char>(dist(rng));
                }

                glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_3D, tex);
                glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, kNoiseSize, kNoiseSize, kNoiseSize, 0, GL_RED, GL_UNSIGNED_BYTE, noise.data());
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
                glBindTexture(GL_TEXTURE_3D, 0);
            };

            if (m_SmokeNoiseTex == 0) {
                buildNoise(m_SmokeNoiseTex, 1337);
            }
            if (m_SmokeDistortTex == 0) {
                buildNoise(m_SmokeDistortTex, 7331);
            }
            if (m_SmokePuffTex == 0) {
                buildNoise(m_SmokePuffTex, 42);
            }

            if (GL_ARB_bindless_texture) {
                m_SmokeNoiseHandle = glGetTextureHandleARB(m_SmokeNoiseTex);
                m_SmokeDistortHandle = glGetTextureHandleARB(m_SmokeDistortTex);
                m_SmokePuffHandle = glGetTextureHandleARB(m_SmokePuffTex);
                if (m_SmokeNoiseHandle != 0) glMakeTextureHandleResidentARB(m_SmokeNoiseHandle);
                if (m_SmokeDistortHandle != 0) glMakeTextureHandleResidentARB(m_SmokeDistortHandle);
                if (m_SmokePuffHandle != 0) glMakeTextureHandleResidentARB(m_SmokePuffHandle);
                if (m_SmokeNoiseHandle == 0 || m_SmokeDistortHandle == 0 || m_SmokePuffHandle == 0) {
                    EE_CORE_WARN("Failed to create bindless handle(s) for smoke noise textures.");
                }
            } else {
                EE_CORE_WARN("GL_ARB_bindless_texture not supported. Smoke noise will use regular binding.");
            }
        }

        m_Initialized = true;
        EE_CORE_INFO("GPU Orb Particle System initialized");

        // Load Swirl01.png for shockwave effect (renderMode 3)
        m_Swirl01TexRef = AssetManager::GetInstance().LoadTexture("../Resources/Textures/Swirl01.png");
        if (m_Swirl01TexRef && m_Swirl01TexRef->IsValid()) {
            m_Swirl01Tex = m_Swirl01TexRef->GetRendererID();
        } else {
            EE_CORE_WARN("Failed to load Swirl01.png for shockwave effect.");
        }
    }

    void GPUParticleSystem::InitializeEmitter(GPUParticleEmitter& emitter)
    {
        if (emitter.particleBuffer != 0) {
            DestroyEmitter(emitter);
        }

        // Create particle buffer
        std::vector<GPUParticle> particles(emitter.maxParticles);

        // Initialize all particles as dead (age >= life)
        for (int i = 0; i < emitter.maxParticles; ++i) {
            particles[i].position = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // life = 1
            particles[i].velocity = glm::vec4(0.0f, 0.0f, 0.0f, 2.0f);  // age = 2 (> life, so dead)
            particles[i].color = glm::vec4(1.0f);
            particles[i].params = glm::vec4(0.05f, 0.05f, 0.0f, 0.0f);
        }

        glGenBuffers(1, &emitter.particleBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter.particleBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     sizeof(GPUParticle) * emitter.maxParticles,
                     particles.data(),
                     GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        if (emitter.spawnCounterBuffer == 0) {
            glGenBuffers(1, &emitter.spawnCounterBuffer);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter.spawnCounterBuffer);
        unsigned int zero = 0;
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int), &zero, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        emitter.initialized = true;
        EE_CORE_INFO("Initialized GPU particle emitter with {} particles", emitter.maxParticles);
    }

    void GPUParticleSystem::DestroyEmitter(GPUParticleEmitter& emitter)
    {
        if (emitter.particleBuffer != 0) {
            glDeleteBuffers(1, &emitter.particleBuffer);
            emitter.particleBuffer = 0;
        }
        if (emitter.spawnCounterBuffer != 0) {
            glDeleteBuffers(1, &emitter.spawnCounterBuffer);
            emitter.spawnCounterBuffer = 0;
        }
        emitter.initialized = false;
    }

    void GPUParticleSystem::Update(float dt)
    {
        if (!m_Initialized) return;

        m_ElapsedTime += dt;

        auto& ecs = ECS::GetInstance();
        auto renderer = ecs.GetSystem<graphics::Renderer>();

        for (EntityID entity : m_Entities) {
            if (!ecs.HasComponent<GPUParticleEmitter>(entity)) continue;
            if (!ecs.HasComponent<Transform>(entity)) continue;

            auto& emitter = ecs.GetComponent<GPUParticleEmitter>(entity);
            if (!emitter.active) continue;

            if (ecs.HasComponent<ObjectMetaData>(entity)) {
                if (!ecs.GetComponent<ObjectMetaData>(entity).selfActive) continue;
            }

            // Initialize if needed
            if (!emitter.initialized) {
                InitializeEmitter(emitter);
            }

            const auto& transform = ecs.GetComponent<Transform>(entity);

            // Build emitter basis from rotation
            Quaternion q = QuaternionNormalize(transform.rotation);
            Vec3 right = QuaternionRotateVector(q, Vec3(1.0f, 0.0f, 0.0f));
            Vec3 up = QuaternionRotateVector(q, Vec3(0.0f, 1.0f, 0.0f));
            Vec3 forward = QuaternionRotateVector(q, Vec3(0.0f, 0.0f, 1.0f));

            // Apply local position offset in emitter's local space
            Vec3 emitterPos = transform.position;
            emitterPos.x += emitter.localPositionOffset.x * right.x + emitter.localPositionOffset.y * up.x + emitter.localPositionOffset.z * forward.x;
            emitterPos.y += emitter.localPositionOffset.x * right.y + emitter.localPositionOffset.y * up.y + emitter.localPositionOffset.z * forward.y;
            emitterPos.z += emitter.localPositionOffset.x * right.z + emitter.localPositionOffset.y * up.z + emitter.localPositionOffset.z * forward.z;

            // Compute spawn count (rate + bursts)
            int spawnCount = 0;
            if (emitter.spawnRate > 0.0f) {
                emitter.spawnAccumulator += emitter.spawnRate * dt;
                if (emitter.spawnAccumulator >= 1.0f) {
                    int rateCount = static_cast<int>(emitter.spawnAccumulator);
                    emitter.spawnAccumulator -= static_cast<float>(rateCount);
                    spawnCount += rateCount;
                }
            }

            if (emitter.burstOnStart && !emitter.burstPrimed) {
                int minBurst = std::max(0, emitter.burstCountMin);
                int maxBurst = std::max(minBurst, emitter.burstCountMax);
                int count = minBurst;
                if (maxBurst > minBurst) {
                    count += (rand() % (maxBurst - minBurst + 1));
                }
                spawnCount += count;
                emitter.burstPrimed = true;
                if (emitter.burstInterval > 0.0f) {
                    emitter.burstTimer = emitter.burstInterval;
                }
            }

            if (emitter.burstInterval > 0.0f && emitter.burstCountMax > 0) {
                emitter.burstTimer -= dt;
                if (emitter.burstTimer <= 0.0f) {
                    int minBurst = std::max(0, emitter.burstCountMin);
                    int maxBurst = std::max(minBurst, emitter.burstCountMax);
                    int count = minBurst;
                    if (maxBurst > minBurst) {
                        count += (rand() % (maxBurst - minBurst + 1));
                    }
                    spawnCount += count;
                    emitter.burstTimer = emitter.burstInterval;
                }
            }

            if (spawnCount < 0) spawnCount = 0;
            if (spawnCount > emitter.maxParticles) spawnCount = emitter.maxParticles;

            // Dispatch compute shader
            GLuint program = m_ComputeShader->GetRendererID();
            glUseProgram(program);

            // Bind particle buffer
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, emitter.particleBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, emitter.spawnCounterBuffer);

            // Reset spawn counter
            unsigned int zero = 0;
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, emitter.spawnCounterBuffer);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int), &zero);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            // Set uniforms
            glUniform1f(glGetUniformLocation(program, "u_DeltaTime"), dt);
            glUniform1f(glGetUniformLocation(program, "u_Time"), m_ElapsedTime);
            glUniform3f(glGetUniformLocation(program, "u_EmitterPosition"),
                        emitterPos.x, emitterPos.y, emitterPos.z);
            glUniform3f(glGetUniformLocation(program, "u_EmitterRight"), right.x, right.y, right.z);
            glUniform3f(glGetUniformLocation(program, "u_EmitterUp"), up.x, up.y, up.z);
            glUniform3f(glGetUniformLocation(program, "u_EmitterForward"), forward.x, forward.y, forward.z);
            glUniform3f(glGetUniformLocation(program, "u_EmitterScale"),
                        transform.scale.x, transform.scale.y, transform.scale.z);
            glUniform1i(glGetUniformLocation(program, "u_MaxParticles"), emitter.maxParticles);
            glUniform1i(glGetUniformLocation(program, "u_EmissionShape"), emitter.emissionShape);
            
            // Apply overall scale to spawn parameters
            glUniform3f(glGetUniformLocation(program, "u_SpawnBoxExtents"),
                        emitter.spawnBoxExtents.x * emitter.overallScale, 
                        emitter.spawnBoxExtents.y * emitter.overallScale, 
                        emitter.spawnBoxExtents.z * emitter.overallScale);
            glUniform1f(glGetUniformLocation(program, "u_SpawnRadius"), emitter.spawnRadius * emitter.overallScale);
            glUniform1f(glGetUniformLocation(program, "u_SpawnRadiusInner"), emitter.spawnRadiusInner * emitter.overallScale);
            glUniform1ui(glGetUniformLocation(program, "u_SpawnCount"), static_cast<unsigned int>(spawnCount));
            glUniform1i(glGetUniformLocation(program, "u_DirectionMode"), emitter.directionMode);
            glUniform3f(glGetUniformLocation(program, "u_Direction"),
                        emitter.direction.x, emitter.direction.y, emitter.direction.z);
            glUniform1f(glGetUniformLocation(program, "u_ConeAngle"), emitter.coneAngle);
            glUniform1f(glGetUniformLocation(program, "u_ConeInnerAngle"), emitter.coneInnerAngle);
            glUniform1i(glGetUniformLocation(program, "u_BoundsMode"), emitter.boundsMode);
            glUniform1i(glGetUniformLocation(program, "u_BoundsShape"), emitter.boundsShape);

            float avgScale = (transform.scale.x + transform.scale.y + transform.scale.z) / 3.0f;

            // Apply overall scale AND entity transform scale to bounds parameters
            glUniform3f(glGetUniformLocation(program, "u_BoundsBoxExtents"),
                        emitter.boundsBoxExtents.x * emitter.overallScale * transform.scale.x, 
                        emitter.boundsBoxExtents.y * emitter.overallScale * transform.scale.y, 
                        emitter.boundsBoxExtents.z * emitter.overallScale * transform.scale.z);
            glUniform1f(glGetUniformLocation(program, "u_BoundsRadius"), emitter.boundsRadius * emitter.overallScale * avgScale);
            glUniform1f(glGetUniformLocation(program, "u_BoundsRadiusInner"), emitter.boundsRadiusInner * emitter.overallScale * avgScale);
            
            glUniform1f(glGetUniformLocation(program, "u_SpeedMin"), emitter.speedMin);
            glUniform1f(glGetUniformLocation(program, "u_SpeedMax"), emitter.speedMax);
            glUniform3f(glGetUniformLocation(program, "u_Gravity"),
                        emitter.gravity.x, emitter.gravity.y, emitter.gravity.z);
            glUniform1f(glGetUniformLocation(program, "u_Drag"), emitter.drag);
            glUniform1f(glGetUniformLocation(program, "u_TurbulenceStrength"), emitter.turbulenceStrength);
            glUniform1f(glGetUniformLocation(program, "u_TurbulenceScale"), emitter.turbulenceScale);
            glUniform3f(glGetUniformLocation(program, "u_ColorStart"),
                        emitter.colorStart.x, emitter.colorStart.y, emitter.colorStart.z);
            glUniform3f(glGetUniformLocation(program, "u_ColorEnd"),
                        emitter.colorEnd.x, emitter.colorEnd.y, emitter.colorEnd.z);
            glUniform1f(glGetUniformLocation(program, "u_AlphaStart"), emitter.alphaStart);
            glUniform1f(glGetUniformLocation(program, "u_AlphaEnd"), emitter.alphaEnd);
            
            // Apply overall scale AND entity transform scale to particle sizes
            glUniform1f(glGetUniformLocation(program, "u_SizeStartMin"), emitter.sizeStartMin * emitter.overallScale * avgScale);
            glUniform1f(glGetUniformLocation(program, "u_SizeStartMax"), emitter.sizeStartMax * emitter.overallScale * avgScale);
            glUniform1f(glGetUniformLocation(program, "u_SizeEndMin"), emitter.sizeEndMin * emitter.overallScale * avgScale);
            glUniform1f(glGetUniformLocation(program, "u_SizeEndMax"), emitter.sizeEndMax * emitter.overallScale * avgScale);
            glUniform1f(glGetUniformLocation(program, "u_LifetimeMin"), emitter.lifetimeMin);
            glUniform1f(glGetUniformLocation(program, "u_LifetimeMax"), emitter.lifetimeMax);

            // Dispatch
            GLuint numGroups = (emitter.maxParticles + 63) / 64;
            glDispatchCompute(numGroups, 1, 1);

            // Memory barrier
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
        }

        glUseProgram(0);
    }

    void GPUParticleSystem::Render(const Mtx44& view, const Mtx44& projection, const Vec3& cameraPos)
    {
        if (!m_Initialized) return;

        auto& ecs = ECS::GetInstance();

        // Convert Mtx44 to glm::mat4
        glm::mat4 viewMat(
            view.m00, view.m01, view.m02, view.m03,
            view.m10, view.m11, view.m12, view.m13,
            view.m20, view.m21, view.m22, view.m23,
            view.m30, view.m31, view.m32, view.m33
        );
        glm::mat4 projMat(
            projection.m00, projection.m01, projection.m02, projection.m03,
            projection.m10, projection.m11, projection.m12, projection.m13,
            projection.m20, projection.m21, projection.m22, projection.m23,
            projection.m30, projection.m31, projection.m32, projection.m33
        );
        glm::vec3 camPos(cameraPos.x, cameraPos.y, cameraPos.z);

        // Calculate camera right and up vectors for billboarding (use inverse view)
        glm::mat4 invView = glm::inverse(viewMat);
        glm::vec3 cameraRight = glm::vec3(invView[0][0], invView[0][1], invView[0][2]);
        glm::vec3 cameraUp = glm::vec3(invView[1][0], invView[1][1], invView[1][2]);

        // Setup render state
        GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        GLint prevDepthFunc = GL_LESS;
        glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Additive blending for glow
        glDepthMask(GL_FALSE);  // Don't write to depth buffer
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);

        GLuint program = m_RenderShader->GetRendererID();
        glUseProgram(program);

        // Set view/projection uniforms
        glUniformMatrix4fv(glGetUniformLocation(program, "u_View"), 1, GL_FALSE, &viewMat[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_Projection"), 1, GL_FALSE, &projMat[0][0]);
        glUniform3f(glGetUniformLocation(program, "u_CameraRight"), cameraRight.x, cameraRight.y, cameraRight.z);
        glUniform3f(glGetUniformLocation(program, "u_CameraUp"), cameraUp.x, cameraUp.y, cameraUp.z);
        glUniform1f(glGetUniformLocation(program, "u_Time"), m_ElapsedTime);
		auto renderer = ecs.GetSystem<graphics::Renderer>();
        if (renderer) {
            auto gbuffer = renderer->GetGBuffer();
            if (gbuffer && gbuffer->DepthTexture != 0) {
                if (GL_ARB_bindless_texture && gbuffer->HandleDepthTexture != 0) {
                    glUniformHandleui64ARB(glGetUniformLocation(program, "u_SceneDepth"), gbuffer->HandleDepthTexture);
                } else {
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, gbuffer->DepthTexture);
                    glUniform1i(glGetUniformLocation(program, "u_SceneDepth"), 4);
                }
                glUniform2f(glGetUniformLocation(program, "u_ScreenSize"),
                            static_cast<float>(gbuffer->width),
                            static_cast<float>(gbuffer->height));
            }
        }

        for (EntityID entity : m_Entities) {
            if (!ecs.HasComponent<GPUParticleEmitter>(entity)) continue;

            auto& emitter = ecs.GetComponent<GPUParticleEmitter>(entity);
            if (!emitter.active || !emitter.initialized) continue;

            if (ecs.HasComponent<ObjectMetaData>(entity)) {
                if (!ecs.GetComponent<ObjectMetaData>(entity).selfActive) continue;
            }

            // Bind particle buffer
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, emitter.particleBuffer);

            // Set sparkle shape
            glUniform1i(glGetUniformLocation(program, "u_SparkleShape"), emitter.sparkleShape);
            glUniform1i(glGetUniformLocation(program, "u_RenderMode"), emitter.renderMode);
            glUniform1f(glGetUniformLocation(program, "u_SmokeOpacity"), emitter.smokeOpacity);
            glUniform1f(glGetUniformLocation(program, "u_SmokeSoftness"), emitter.smokeSoftness);
            glUniform1f(glGetUniformLocation(program, "u_SmokeNoiseScale"), emitter.smokeNoiseScale);
            glUniform1f(glGetUniformLocation(program, "u_SmokeDistortScale"), emitter.smokeDistortScale);
            glUniform1f(glGetUniformLocation(program, "u_SmokeDistortStrength"), emitter.smokeDistortStrength);
            glUniform1f(glGetUniformLocation(program, "u_SmokePuffScale"), emitter.smokePuffScale);
            glUniform1f(glGetUniformLocation(program, "u_SmokePuffStrength"), emitter.smokePuffStrength);
            glUniform1f(glGetUniformLocation(program, "u_SmokeStretch"), emitter.smokeStretch);
            glUniform1f(glGetUniformLocation(program, "u_SmokeUpBias"), emitter.smokeUpBias);
            glUniform1f(glGetUniformLocation(program, "u_SmokeDepthFade"), emitter.smokeDepthFade);
            glUniform1f(glGetUniformLocation(program, "u_ElectricIntensity"), emitter.electricIntensity);
            glUniform1f(glGetUniformLocation(program, "u_ElectricFrequency"), emitter.electricFrequency);
            glUniform1f(glGetUniformLocation(program, "u_ElectricBoltThickness"), emitter.electricBoltThickness);
            glUniform1f(glGetUniformLocation(program, "u_ElectricBoltVariation"), emitter.electricBoltVariation);
            glUniform1f(glGetUniformLocation(program, "u_ElectricGlow"), emitter.electricGlow);
            glUniform1i(glGetUniformLocation(program, "u_ElectricBoltCount"), emitter.electricBoltCount);

            if (m_SmokeNoiseHandle != 0 && GL_ARB_bindless_texture) {
                glUniformHandleui64ARB(glGetUniformLocation(program, "u_SmokeNoise"), m_SmokeNoiseHandle);
                glUniformHandleui64ARB(glGetUniformLocation(program, "u_SmokeDistort"), m_SmokeDistortHandle);
                glUniformHandleui64ARB(glGetUniformLocation(program, "u_SmokePuff"), m_SmokePuffHandle);
            } else if (m_SmokeNoiseTex != 0) {
                glActiveTexture(GL_TEXTURE3);
                glBindTexture(GL_TEXTURE_3D, m_SmokeNoiseTex);
                glUniform1i(glGetUniformLocation(program, "u_SmokeNoise"), 3);
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_3D, m_SmokeDistortTex);
                glUniform1i(glGetUniformLocation(program, "u_SmokeDistort"), 5);
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_3D, m_SmokePuffTex);
                glUniform1i(glGetUniformLocation(program, "u_SmokePuff"), 6);
            }

            if (emitter.renderMode == 1) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            } else if (emitter.renderMode == 2) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive for electric
            } else if (emitter.renderMode == 3) {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive for shockwave sprite
                // Bind Swirl01.png to texture unit 7
                glActiveTexture(GL_TEXTURE7);
                glBindTexture(GL_TEXTURE_2D, m_Swirl01Tex);
                glUniform1i(glGetUniformLocation(program, "u_SpriteTexture"), 7);
            } else {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }

            // Draw particles via instancing
            glBindVertexArray(m_QuadVAO);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, emitter.maxParticles);
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_3D, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // Restore render state
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        if (!depthTestEnabled) {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthFunc(prevDepthFunc);
        glEnable(GL_CULL_FACE);
    }

    bool GPUParticleSystem::HasActiveEmitters() const
    {
        return !m_Entities.empty();
    }

    void GPUParticleSystem::Cleanup()
    {
        auto& ecs = ECS::GetInstance();

        for (EntityID entity : m_Entities) {
            if (ecs.HasComponent<GPUParticleEmitter>(entity)) {
                auto& emitter = ecs.GetComponent<GPUParticleEmitter>(entity);
                DestroyEmitter(emitter);
            }
        }

        if (m_QuadEBO != 0) {
            glDeleteBuffers(1, &m_QuadEBO);
            m_QuadEBO = 0;
        }
        if (m_QuadVBO != 0) {
            glDeleteBuffers(1, &m_QuadVBO);
            m_QuadVBO = 0;
        }
        if (m_QuadVAO != 0) {
            glDeleteVertexArrays(1, &m_QuadVAO);
            m_QuadVAO = 0;
        }
        if (GL_ARB_bindless_texture) {
            if (m_SmokeNoiseHandle != 0) glMakeTextureHandleNonResidentARB(m_SmokeNoiseHandle);
            if (m_SmokeDistortHandle != 0) glMakeTextureHandleNonResidentARB(m_SmokeDistortHandle);
            if (m_SmokePuffHandle != 0) glMakeTextureHandleNonResidentARB(m_SmokePuffHandle);
            m_SmokeNoiseHandle = 0;
            m_SmokeDistortHandle = 0;
            m_SmokePuffHandle = 0;
        }
        if (m_SmokeNoiseTex != 0) {
            glDeleteTextures(1, &m_SmokeNoiseTex);
            m_SmokeNoiseTex = 0;
        }
        if (m_SmokeDistortTex != 0) {
            glDeleteTextures(1, &m_SmokeDistortTex);
            m_SmokeDistortTex = 0;
        }
        if (m_SmokePuffTex != 0) {
            glDeleteTextures(1, &m_SmokePuffTex);
            m_SmokePuffTex = 0;
        }

        // Release Swirl01 reference (AssetManager owns the GL texture, don't delete it)
        m_Swirl01TexRef = nullptr;
        m_Swirl01Tex = 0;

        m_Initialized = false;
        EE_CORE_INFO("GPU Orb Particle System cleaned up");
    }

    void GPUParticleSystem::RenderDebug(const Mtx44& view, const Mtx44& projection)
    {
        auto& ecs = ECS::GetInstance();
        auto renderer = ecs.GetSystem<graphics::Renderer>();
        if (!renderer) return;

        for (EntityID entity : m_Entities) {
            if (!ecs.HasComponent<GPUParticleEmitter>(entity)) continue;
            if (!ecs.HasComponent<Transform>(entity)) continue;

            auto& emitter = ecs.GetComponent<GPUParticleEmitter>(entity);
            if (!emitter.active || !emitter.showDebugBounds) continue;

            const auto& transform = ecs.GetComponent<Transform>(entity);
            Vec3 pos = transform.position;

            // Build emitter basis from rotation
            Quaternion q = QuaternionNormalize(transform.rotation);
            Vec3 right = QuaternionRotateVector(q, Vec3(1.0f, 0.0f, 0.0f));
            Vec3 up = QuaternionRotateVector(q, Vec3(0.0f, 1.0f, 0.0f));
            Vec3 forward = QuaternionRotateVector(q, Vec3(0.0f, 0.0f, 1.0f));

            // Apply local position offset in emitter's local space
            pos.x += emitter.localPositionOffset.x * right.x + emitter.localPositionOffset.y * up.x + emitter.localPositionOffset.z * forward.x;
            pos.y += emitter.localPositionOffset.x * right.y + emitter.localPositionOffset.y * up.y + emitter.localPositionOffset.z * forward.y;
            pos.z += emitter.localPositionOffset.x * right.z + emitter.localPositionOffset.y * up.z + emitter.localPositionOffset.z * forward.z;

            glm::vec3 glmPos(pos.x, pos.y, pos.z);
            glm::vec3 glmRight(right.x, right.y, right.z);
            glm::vec3 glmUp(up.x, up.y, up.z);
            glm::vec3 glmForward(forward.x, forward.y, forward.z);

            // Draw emission shape
            glm::vec3 emissionColor(0.0f, 1.0f, 0.5f); // Green for emission

            if (emitter.emissionShape == 0) { // Point
                // Draw small cross
                float size = 0.05f;
                renderer->SubmitDebugLine(glmPos - glmRight * size, glmPos + glmRight * size, emissionColor);
                renderer->SubmitDebugLine(glmPos - glmUp * size, glmPos + glmUp * size, emissionColor);
                renderer->SubmitDebugLine(glmPos - glmForward * size, glmPos + glmForward * size, emissionColor);
            }
            else if (emitter.emissionShape == 1) { // Sphere
                // Draw sphere wireframe
                const int segments = 16;
                float radius = emitter.spawnRadius * emitter.overallScale * transform.scale.x;
                
                // Draw circles in XY, XZ, YZ planes
                for (int axis = 0; axis < 3; axis++) {
                    for (int i = 0; i < segments; i++) {
                        float angle1 = (float)i / segments * 6.28318530718f;
                        float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                        
                        glm::vec3 p1, p2;
                        if (axis == 0) { // XY plane
                            p1 = glmPos + (glmRight * cosf(angle1) + glmUp * sinf(angle1)) * radius;
                            p2 = glmPos + (glmRight * cosf(angle2) + glmUp * sinf(angle2)) * radius;
                        }
                        else if (axis == 1) { // XZ plane
                            p1 = glmPos + (glmRight * cosf(angle1) + glmForward * sinf(angle1)) * radius;
                            p2 = glmPos + (glmRight * cosf(angle2) + glmForward * sinf(angle2)) * radius;
                        }
                        else { // YZ plane
                            p1 = glmPos + (glmUp * cosf(angle1) + glmForward * sinf(angle1)) * radius;
                            p2 = glmPos + (glmUp * cosf(angle2) + glmForward * sinf(angle2)) * radius;
                        }
                        renderer->SubmitDebugLine(p1, p2, emissionColor);
                    }
                }

                // Draw inner radius if present
                if (emitter.spawnRadiusInner > 0.0f) {
                    float innerRadius = emitter.spawnRadiusInner * emitter.overallScale * transform.scale.x;
                    for (int i = 0; i < segments; i++) {
                        float angle1 = (float)i / segments * 6.28318530718f;
                        float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                        glm::vec3 p1 = glmPos + (glmRight * cosf(angle1) + glmUp * sinf(angle1)) * innerRadius;
                        glm::vec3 p2 = glmPos + (glmRight * cosf(angle2) + glmUp * sinf(angle2)) * innerRadius;
                        renderer->SubmitDebugLine(p1, p2, emissionColor * 0.6f);
                    }
                }
            }
            else if (emitter.emissionShape == 2) { // Box
                // Draw box wireframe
                Vec3 extents = Vec3(
                    emitter.spawnBoxExtents.x * emitter.overallScale * transform.scale.x,
                    emitter.spawnBoxExtents.y * emitter.overallScale * transform.scale.y,
                    emitter.spawnBoxExtents.z * emitter.overallScale * transform.scale.z
                );

                glm::vec3 corners[8];
                for (int i = 0; i < 8; i++) {
                    float sx = (i & 1) ? 1.0f : -1.0f;
                    float sy = (i & 2) ? 1.0f : -1.0f;
                    float sz = (i & 4) ? 1.0f : -1.0f;
                    corners[i] = glmPos + glmRight * (sx * extents.x) +
                                         glmUp * (sy * extents.y) +
                                         glmForward * (sz * extents.z);
                }

                // Draw 12 edges of the box
                int edges[12][2] = {
                    {0,1}, {1,3}, {3,2}, {2,0}, // Bottom
                    {4,5}, {5,7}, {7,6}, {6,4}, // Top
                    {0,4}, {1,5}, {2,6}, {3,7}  // Vertical
                };
                for (auto& edge : edges) {
                    renderer->SubmitDebugLine(corners[edge[0]], corners[edge[1]], emissionColor);
                }
            }
            else if (emitter.emissionShape == 3) { // Disc (XZ)
                // Draw disc wireframe
                const int segments = 24;
                float radius = emitter.spawnRadius * emitter.overallScale * transform.scale.x;
                
                for (int i = 0; i < segments; i++) {
                    float angle1 = (float)i / segments * 6.28318530718f;
                    float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                    glm::vec3 p1 = glmPos + (glmRight * cosf(angle1) + glmForward * sinf(angle1)) * radius;
                    glm::vec3 p2 = glmPos + (glmRight * cosf(angle2) + glmForward * sinf(angle2)) * radius;
                    renderer->SubmitDebugLine(p1, p2, emissionColor);
                }

                // Draw cross lines
                renderer->SubmitDebugLine(glmPos - glmRight * radius, glmPos + glmRight * radius, emissionColor);
                renderer->SubmitDebugLine(glmPos - glmForward * radius, glmPos + glmForward * radius, emissionColor);
            }

            // Draw bounds if enabled
            if (emitter.boundsMode > 0) {
                glm::vec3 boundsColor(1.0f, 0.5f, 0.0f); // Orange for bounds

                if (emitter.boundsShape == 0) { // Sphere
                    const int segments = 16;
                    float radius = emitter.boundsRadius * emitter.overallScale * transform.scale.x;
                    
                    for (int axis = 0; axis < 3; axis++) {
                        for (int i = 0; i < segments; i++) {
                            float angle1 = (float)i / segments * 6.28318530718f;
                            float angle2 = (float)(i + 1) / segments * 6.28318530718f;
                            
                            glm::vec3 p1, p2;
                            if (axis == 0) {
                                p1 = glmPos + (glmRight * cosf(angle1) + glmUp * sinf(angle1)) * radius;
                                p2 = glmPos + (glmRight * cosf(angle2) + glmUp * sinf(angle2)) * radius;
                            }
                            else if (axis == 1) {
                                p1 = glmPos + (glmRight * cosf(angle1) + glmForward * sinf(angle1)) * radius;
                                p2 = glmPos + (glmRight * cosf(angle2) + glmForward * sinf(angle2)) * radius;
                            }
                            else {
                                p1 = glmPos + (glmUp * cosf(angle1) + glmForward * sinf(angle1)) * radius;
                                p2 = glmPos + (glmUp * cosf(angle2) + glmForward * sinf(angle2)) * radius;
                            }
                            renderer->SubmitDebugLine(p1, p2, boundsColor);
                        }
                    }
                }
                else if (emitter.boundsShape == 1) { // Box
                    Vec3 extents = Vec3(
                        emitter.boundsBoxExtents.x * emitter.overallScale * transform.scale.x,
                        emitter.boundsBoxExtents.y * emitter.overallScale * transform.scale.y,
                        emitter.boundsBoxExtents.z * emitter.overallScale * transform.scale.z
                    );

                    glm::vec3 corners[8];
                    for (int i = 0; i < 8; i++) {
                        float sx = (i & 1) ? 1.0f : -1.0f;
                        float sy = (i & 2) ? 1.0f : -1.0f;
                        float sz = (i & 4) ? 1.0f : -1.0f;
                        corners[i] = glmPos + glmRight * (sx * extents.x) +
                                             glmUp * (sy * extents.y) +
                                             glmForward * (sz * extents.z);
                    }

                    int edges[12][2] = {
                        {0,1}, {1,3}, {3,2}, {2,0},
                        {4,5}, {5,7}, {7,6}, {6,4},
                        {0,4}, {1,5}, {2,6}, {3,7}
                    };
                    for (auto& edge : edges) {
                        renderer->SubmitDebugLine(corners[edge[0]], corners[edge[1]], boundsColor);
                    }
                }
            }
        }
    }

} // namespace Ermine
