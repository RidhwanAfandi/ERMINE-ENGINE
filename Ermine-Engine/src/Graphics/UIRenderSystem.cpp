/* Start Header ************************************************************************/
/*!
\file       UIRenderSystem.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       04/11/2025
\brief      Implementation of the UIRenderSystem for rendering HUD elements.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "UIRenderSystem.h"
#include "UIButtonSystem.h"
#include "ECS.h"
#include "AssetManager.h"
#include "Logger.h"
#include "Texture.h"
#include "SettingsManager.h"
#include <cmath>
#include <algorithm>
#include <vector>

#if defined(EE_EDITOR)
#include "EditorGUI.h"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Ermine
{
    void UIRenderSystem::Init(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_aspectRatio = (screenHeight > 0) ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight) : 1.0f;

        // Load UI shader
        m_uiShader = AssetManager::GetInstance().LoadShader(
            "../Resources/Shaders/ui_vertex.glsl",
            "../Resources/Shaders/ui_fragment.glsl"
        );

        if (!m_uiShader || !m_uiShader->IsValid())
        {
            EE_CORE_ERROR("UIRenderSystem: Failed to load UI shaders");
            return;
        }

        // Create orthographic projection matrix (0,0 at bottom-left, 1,1 at top-right)
        m_orthoProjection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

        // Setup VAO and VBO for dynamic quad rendering
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

        // Reserve space for vertex data (position + color + texCoord)
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 2048, nullptr, GL_DYNAMIC_DRAW);

        // Position attribute (x, y)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);

        // Color attribute (r, g, b, a)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));

        // Texture coordinate attribute (u, v)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

        glBindVertexArray(0);

        // Initialize text renderer for keybind labels
        m_textRenderer = std::make_shared<UITextRenderer>();
        // Try to load a TTF font, fallback to simple bitmap if not found
        if (!m_textRenderer->Initialize("../Resources/Fonts/times.ttf", 32))
        {
            EE_CORE_WARN("UITextRenderer: Failed to load font, using fallback");
        }

        EE_CORE_INFO("UIRenderSystem initialized successfully");
    }

    void UIRenderSystem::Update(float deltaTime)
    {
        // Iterate through all entities with UIComponent
        for (EntityID entity : m_Entities)
        {
            // Skip if entity no longer has UIComponent (may have been removed in inspector)
            if (!ECS::GetInstance().HasComponent<UIComponent>(entity))
                continue;

            if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
            {
                const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                if (!meta.selfActive)
                    continue;
            }
            auto& ui = ECS::GetInstance().GetComponent<UIComponent>(entity);

            // Update skill cooldowns and activation animations
            bool anySkillOnCooldown = false;
            for (auto& skill : ui.skills)
            {
                // Update cooldown
                if (skill.isOnCooldown && skill.currentCooldown > 0.0f)
                {
                    skill.currentCooldown -= deltaTime;
                    if (skill.currentCooldown <= 0.0f)
                    {
                        skill.currentCooldown = 0.0f;
                        skill.isOnCooldown = false;
                    }
                    else
                    {
                        anySkillOnCooldown = true;
                    }
                }

                // Update activation flash timer
                if (skill.activationFlashTimer > 0.0f)
                {
                    skill.activationFlashTimer -= deltaTime;
                    if (skill.activationFlashTimer < 0.0f)
                        skill.activationFlashTimer = 0.0f;
                }
            }

            // Life Essence (Health) Regeneration System
            // Only regenerate when no skills are on cooldown (player is idle)
            if (!anySkillOnCooldown && ui.currentHealth < ui.maxHealth)
            {
                ui.healthRegenTimer += deltaTime;

                // Only regenerate health after the delay
                if (ui.healthRegenTimer >= ui.healthRegenDelay)
                {
                    ui.currentHealth += ui.healthRegenRate * deltaTime;
                    if (ui.currentHealth > ui.maxHealth)
                        ui.currentHealth = ui.maxHealth;
                }
            }
            else
            {
                // Reset timer when skills are being used
                ui.healthRegenTimer = 0.0f;
            }
        }

        // Update UIHealthbarComponent (new separate component, skip when paused)
        auto& ecs = ECS::GetInstance();
        if (!UIButtonSystem::IsGamePaused())
        {
            constexpr EntityID MAX_ENTITIES_UPDATE = 10000;
            for (EntityID entity = 1; entity < MAX_ENTITIES_UPDATE; ++entity)
            {
                if (!ecs.IsEntityValid(entity))
                    continue;

                if (!ecs.HasComponent<UIHealthbarComponent>(entity))
                    continue;

                // Skip if entity is inactive
                if (ecs.HasComponent<ObjectMetaData>(entity))
                {
                    const auto& meta = ecs.GetComponent<ObjectMetaData>(entity);
                    if (!meta.selfActive)
                        continue;
                }

                auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entity);

                // Health Regeneration System (Option A: regenerate independently)
                if (healthbar.currentHealth < healthbar.maxHealth)
                {
                    healthbar.healthRegenTimer += deltaTime;

                    // Only regenerate health after the delay
                    if (healthbar.healthRegenTimer >= healthbar.healthRegenDelay)
                    {
                        healthbar.currentHealth += healthbar.healthRegenRate * deltaTime;
                        if (healthbar.currentHealth > healthbar.maxHealth)
                            healthbar.currentHealth = healthbar.maxHealth;
                    }
                }
                else
                {
                    // Reset timer when at full health
                    healthbar.healthRegenTimer = 0.0f;
                }
            }
        }
    }

    void UIRenderSystem::Render()
    {
        if (!m_uiShader || !m_uiShader->IsValid())
        {
            static bool loggedOnce = false;
            if (!loggedOnce)
            {
                EE_CORE_ERROR("UIRenderSystem::Render() - Shader is invalid!");
                loggedOnce = true;
            }
            return;
        }

#if defined(EE_EDITOR)
        // In editor: render UI during play mode OR when the active scene is a menu/UI-focused scene
        if (!editor::EditorGUI::isPlaying)
        {
            // Check if we're viewing a menu scene (heuristic: if only UI entities with no game logic)
            // For now, always render UI in editor to support menu scene previewing
        }
#endif

        // Enable blending for transparency
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Disable depth test for UI overlay
        glDisable(GL_DEPTH_TEST);

        // Use UI shader
        m_uiShader->Bind();
        m_uiShader->SetUniformMatrix4fv("projection", m_orthoProjection);
        m_uiShader->SetUniform1i("uUseTexture", 0); // Default: don't use textures

        static bool firstRender = true;
        if (firstRender)
        {
            EE_CORE_INFO("UIRenderSystem: First render call! Entities count: {}", m_Entities.size());
            firstRender = false;
        }

        // Render UIImageComponent entities sorted by renderOrder
        auto& ecs = ECS::GetInstance();
        constexpr EntityID MAX_ENTITIES = 10000;

        // Collect active image entities and sort by renderOrder
        std::vector<EntityID> imageEntities;
        for (EntityID entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;
            if (!ecs.HasComponent<UIImageComponent>(entity))
                continue;
            if (!IsEntityActiveInHierarchy(entity))
                continue;
            imageEntities.push_back(entity);
        }
        std::sort(imageEntities.begin(), imageEntities.end(), [&ecs](EntityID a, EntityID b)
        {
            return ecs.GetComponent<UIImageComponent>(a).renderOrder < ecs.GetComponent<UIImageComponent>(b).renderOrder;
        });

        for (EntityID entity : imageEntities)
        {
            const auto& imageComp = ecs.GetComponent<UIImageComponent>(entity);

            // Check if this is the gamma preview image
            bool isGammaPreview = false;
            if (const auto& meta = ecs.TryGetComponent<ObjectMetaData>(entity))
            {
                if (meta->name == "Gamma Preview Image")
                    isGammaPreview = true;
            }

            // Set gamma uniform: apply gamma correction only to the preview image
            if (isGammaPreview)
            {
                float gamma = 2.8f - (Ermine::SettingsManager::GetInstance().gammaSliderValue * 1.2f);
                m_uiShader->SetUniform1f("u_Gamma", gamma);
            }
            else
            {
                m_uiShader->SetUniform1f("u_Gamma", 1.0f);
            }

            // Load texture if image path is specified
            std::shared_ptr<graphics::Texture> texture;
            if (!imageComp.imagePath.empty())
            {
                auto it = m_textureCache.find(imageComp.imagePath);
                if (it != m_textureCache.end())
                {
                    texture = it->second;
                }
                else
                {
                    texture = AssetManager::GetInstance().LoadTexture(imageComp.imagePath);
                    if (texture && texture->IsValid())
                    {
                        m_textureCache[imageComp.imagePath] = texture;
                    }
                }
            }

            // Render the image (if texture exists)
            if (texture && texture->IsValid())
            {
                if (imageComp.fullscreen)
                {
                    RenderTexturedSquare(0.5f, 0.5f, imageComp.height, texture, imageComp.tintColor, imageComp.alpha);
					//RenderTexturedRect(0.5f, 0.5f, imageComp.width, imageComp.height, texture, imageComp.tintColor, imageComp.alpha);
                }
                else
                {
                    RenderTexturedSquare(
                        imageComp.position.x,
                        imageComp.position.y,
                        imageComp.height,
                        texture,
                        imageComp.tintColor,
                        imageComp.alpha
                    );
     //               RenderTexturedRect(
     //                   imageComp.position.x,
     //                   imageComp.position.y,
     //                   imageComp.width,
     //                   imageComp.height,
     //                   texture,
     //                   imageComp.tintColor,
     //                   imageComp.alpha
					//);
                }
            }

            // Render caption
            if (imageComp.showCaption && !imageComp.caption.empty() && m_textRenderer)
            {
                float textScale = imageComp.captionFontSize / 24.0f;
                float textAlpha = imageComp.alpha;
                if (imageComp.imagePath.empty() && imageComp.alpha == 0.0f)
                {
                    textAlpha = 1.0f;
                }

                m_textRenderer->RenderText(
                    m_uiShader,
                    imageComp.caption,
                    imageComp.captionPosition.x,
                    imageComp.captionPosition.y,
                    textScale,
                    imageComp.captionColor,
                    textAlpha,
                    m_VAO,
                    m_VBO
                );
            }
        }

        // Reset gamma uniform so other UI elements are not affected
        m_uiShader->SetUniform1f("u_Gamma", 1.0f);

        // Render UI for all entities with UIComponent (legacy support)
        for (EntityID entity : m_Entities)
        {
            // ✅ FIX: Check hierarchy before rendering UIComponent elements
            if (!IsEntityActiveInHierarchy(entity))
                continue;

            const auto& ui = ECS::GetInstance().GetComponent<UIComponent>(entity);

            if (ui.showHealthbar)
                RenderHealthBar(ui);

            if (ui.showBookCounter)
                RenderBookCounter(ui);

            if (ui.showManaBar)
                RenderManaBar(ui);

            if (ui.showSkills)
                RenderSkillSlots(ui);

            if (ui.showCrosshair)
                RenderCrosshair(ui);
        }

        // Render new separate UI components (skip when game is paused — pause background covers them)
        if (!UIButtonSystem::IsGamePaused())
        for (EntityID entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;

            if (!IsEntityActiveInHierarchy(entity))
                continue;

            // Render UIHealthbarComponent
            if (const auto& healthbar =  ecs.TryGetComponent<UIHealthbarComponent>(entity))
            {
                if (healthbar->showHealthbar)
                    RenderHealthBarNew(*healthbar);
            }

            // Render UICrosshairComponent
            if (const auto& crosshair = ecs.TryGetComponent<UICrosshairComponent>(entity))
            {
                if (crosshair->showCrosshair)
                    RenderCrosshairNew(*crosshair);
            }

            // Render UISkillsComponent
            if (const auto& skills = ecs.TryGetComponent<UISkillsComponent>(entity))
            {
                if (skills->showSkills)
                    RenderSkillSlotsNew(*skills, entity);
            }

            // Render UIManaBarComponent
            if (const auto& manaBar = ecs.TryGetComponent<UIManaBarComponent>(entity))
            {
                if (manaBar->showManaBar)
                    RenderManaBarNew(*manaBar);
            }

            // Render UIBookCounterComponent
            if (const auto& bookCounter = ecs.TryGetComponent<UIBookCounterComponent>(entity))
            {
                if (bookCounter->showBookCounter)
                    RenderBookCounterNew(*bookCounter);
            }
        }

        // Render UIButtonComponent entities sorted by renderOrder
        std::vector<EntityID> buttonEntities;
        for (EntityID entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;
            if (!ecs.HasComponent<UIButtonComponent>(entity))
                continue;
            if (!IsEntityActiveInHierarchy(entity))
                continue;
            buttonEntities.push_back(entity);
        }
        ranges::sort(buttonEntities, [&ecs](EntityID a, EntityID b)
        {
	        return ecs.GetComponent<UIButtonComponent>(a).renderOrder < ecs.GetComponent<UIButtonComponent>(b).renderOrder;
        });

        for (EntityID entity : buttonEntities)
        {
            const auto& button = ecs.GetComponent<UIButtonComponent>(entity);
            RenderButton(button);
        }

        // Render UISliderComponent entities sorted by renderOrder
        std::vector<EntityID> sliderEntities;
        for (EntityID entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;
            if (!ecs.HasComponent<UISliderComponent>(entity))
                continue;
            if (!IsEntityActiveInHierarchy(entity))
                continue;
            sliderEntities.push_back(entity);
        }
        ranges::sort(sliderEntities, [&ecs](EntityID a, EntityID b)
        {
	        return ecs.GetComponent<UISliderComponent>(a).renderOrder < ecs.GetComponent<UISliderComponent>(b).renderOrder;
        });

        for (EntityID entity : sliderEntities)
        {
            const auto& slider = ecs.GetComponent<UISliderComponent>(entity);
            RenderSlider(slider);
        }

        // Render UITextComponent entities
        for (EntityID entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;

            if (!ecs.HasComponent<UITextComponent>(entity))
                continue;

            if (!IsEntityActiveInHierarchy(entity))
                continue;

            const auto& textComp = ecs.GetComponent<UITextComponent>(entity);
            RenderTextComponent(textComp);
        }

        // Re-enable depth test
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    bool UIRenderSystem::IsEntityActiveInHierarchy(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        // Check if entity itself is valid
        if (!ecs.IsEntityValid(entity))
            return false;

        // Check self active state
        if (const auto& meta = ecs.TryGetComponent<ObjectMetaData>(entity))
        {
            if (!meta->selfActive)
                return false;
        }

        // Check parent chain via HierarchyComponent
        if (const auto& hierarchy = ecs.TryGetComponent<HierarchyComponent>(entity))
        {
            // If has a valid parent, check if parent is active
            if (hierarchy->parent != HierarchyComponent::INVALID_PARENT)
            {
                // Recursively check parent's active state
                return IsEntityActiveInHierarchy(hierarchy->parent);
            }
        }

        // No parent or parent is active - entity is active
        return true;
    }

    void UIRenderSystem::OnScreenResize(int width, int height)
    {
        m_screenWidth = width;
        m_screenHeight = height;
        m_aspectRatio = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    }

    bool UIRenderSystem::CastSkill(EntityID entity, int skillIndex)
    {
        if (!ECS::GetInstance().HasComponent<UIComponent>(entity))
            return false;

        auto& ui = ECS::GetInstance().GetComponent<UIComponent>(entity);

        if (skillIndex < 0 || skillIndex >= static_cast<int>(ui.skills.size()))
            return false;

        auto& skill = ui.skills[skillIndex];

        // Check if skill is on cooldown
        if (skill.isOnCooldown || skill.currentCooldown > 0.0f)
            return false;

        // Check if enough life essence (health)
        if (ui.currentHealth < skill.manaCost)
            return false;

        // Cast skill - drains life essence!
        ui.currentHealth -= skill.manaCost;
        skill.currentCooldown = skill.maxCooldown;
        skill.isOnCooldown = true;

        // Trigger activation flash effect (0.2 seconds)
        skill.activationFlashTimer = 0.2f;

        // Reset health regeneration timer when skill is cast
        ui.healthRegenTimer = 0.0f;

        return true;
    }

    void UIRenderSystem::RenderHealthBar(const UIComponent& ui)
    {
        float x = ui.healthbarPosition.x;
        float y = ui.healthbarPosition.y;
        float width = ui.healthbarWidth;
        float height = ui.healthbarHeight;

        // Try to load health bar textures if paths are specified
        std::shared_ptr<graphics::Texture> bgTex = nullptr;
        std::shared_ptr<graphics::Texture> fillTex = nullptr;
        std::shared_ptr<graphics::Texture> frameTex = nullptr;

        // Load background texture
        if (!ui.healthbarBgTexture.empty())
        {
            auto it = m_textureCache.find(ui.healthbarBgTexture);
            if (it != m_textureCache.end())
            {
                bgTex = it->second;
            }
            else
            {
                bgTex = AssetManager::GetInstance().LoadTexture(ui.healthbarBgTexture);
                if (bgTex && bgTex->IsValid())
                {
                    m_textureCache[ui.healthbarBgTexture] = bgTex;
                }
            }
        }

        // Load fill texture
        if (!ui.healthbarFillTexture.empty())
        {
            auto it = m_textureCache.find(ui.healthbarFillTexture);
            if (it != m_textureCache.end())
            {
                fillTex = it->second;
            }
            else
            {
                fillTex = AssetManager::GetInstance().LoadTexture(ui.healthbarFillTexture);
                if (fillTex && fillTex->IsValid())
                {
                    m_textureCache[ui.healthbarFillTexture] = fillTex;
                }
            }
        }

        // Load frame texture
        if (!ui.healthbarFrameTexture.empty())
        {
            auto it = m_textureCache.find(ui.healthbarFrameTexture);
            if (it != m_textureCache.end())
            {
                frameTex = it->second;
            }
            else
            {
                frameTex = AssetManager::GetInstance().LoadTexture(ui.healthbarFrameTexture);
                if (frameTex && frameTex->IsValid())
                {
                    m_textureCache[ui.healthbarFrameTexture] = frameTex;
                }
            }
        }

        // ========================================================================
        // RENDER BACKGROUND (textured or solid color)
        // ========================================================================
        if (bgTex && bgTex->IsValid())
        {
            // Render textured background
            RenderTexturedRect(x, y, width, height, bgTex, { 1.0f, 1.0f, 1.0f }, 0.9f);
        }
        else
        {
            // Fallback: render solid color background with border
            float outerBorder = 0.003f;
            Vec3 outerBorderColor = { 0.0f, 0.0f, 0.0f };
            RenderQuad(x - outerBorder, y - outerBorder, width + outerBorder * 2.0f, height + outerBorder * 2.0f, outerBorderColor, 0.9f);

            float innerBorder = 0.0015f;
            Vec3 innerBorderColor = { 0.8f, 0.8f, 0.8f };
            RenderQuad(x - innerBorder, y - innerBorder, width + innerBorder * 2.0f, height + innerBorder * 2.0f, innerBorderColor, 0.8f);

            RenderQuad(x, y, width, height, ui.healthbarBgColor, 0.9f);
        }

        // ========================================================================
        // RENDER HEALTH FILL (textured or solid color)
        // ========================================================================
        float healthPercent = ui.currentHealth / ui.maxHealth;
        if (healthPercent > 0.0f)
        {
            float fillWidth = width * healthPercent;

            if (fillTex && fillTex->IsValid())
            {
                // Render textured fill with UV clipping to show partial fill
                RenderTexturedRectUV(x, y, fillWidth, height, fillTex,
                                     0.0f, 0.0f, healthPercent, 1.0f, // UV coords (clip U based on health %)
                                     { 1.0f, 1.0f, 1.0f }, 1.0f);
            }
            else
            {
                // Fallback: render solid color fill with gradient effect
                Vec3 healthColor = ui.healthbarColor;
                if (healthPercent < 0.25f)
                    healthColor = { 0.75f, 0.20f, 0.10f }; // Dark rusty copper when critical
                else if (healthPercent < 0.5f)
                    healthColor = { 0.85f, 0.45f, 0.15f }; // Dimmer brass/copper when low

                // Main health bar
                RenderQuad(x, y, fillWidth, height, healthColor, 1.0f);

                // Shine effect on top of health bar (brighter overlay)
                float shineHeight = height * 0.4f;
                Vec3 shineColor = { 1.0f, 1.0f, 1.0f };
                RenderQuad(x, y + height - shineHeight, fillWidth, shineHeight, shineColor, 0.3f);
            }
        }

        // ========================================================================
        // RENDER FRAME OVERLAY (textured or shadow effect)
        // ========================================================================
        if (frameTex && frameTex->IsValid())
        {
            // Render textured frame overlay (renders on top of everything)
            RenderTexturedRect(x, y, width, height, frameTex, { 1.0f, 1.0f, 1.0f }, 1.0f);
        }
        else
        {
            // Fallback: inner shadow at the bottom for depth (only if no bg texture)
            if (!bgTex || !bgTex->IsValid())
            {
                float shadowHeight = height * 0.2f;
                Vec3 shadowColor = { 0.0f, 0.0f, 0.0f };
                RenderQuad(x, y, width, shadowHeight, shadowColor, 0.3f);
            }
        }
    }

    void UIRenderSystem::RenderManaBar(const UIComponent& ui)
    {
        float x = ui.manaBarPosition.x;
        float y = ui.manaBarPosition.y;
        float width = ui.manaBarWidth;
        float height = ui.manaBarHeight;

        // Render background
        RenderQuad(x, y, width, height, ui.manaBarBgColor, 0.8f);

        // Render mana fill
        float manaPercent = ui.currentMana / ui.maxMana;
        if (manaPercent > 0.0f)
        {
            RenderQuad(x, y, width * manaPercent, height, ui.manaBarColor, 1.0f);
        }
    }

    void UIRenderSystem::RenderBookCounter(const UIComponent& ui)
    {
        if (!m_textRenderer || !m_uiShader)
            return;

        // Create counter text (e.g., "0/4", "2/4")
        std::string counterText = std::to_string(ui.booksCollected) + "/" + std::to_string(ui.totalBooks);

        // Position at top-left
        float x = ui.bookCounterPosition.x;
        float y = ui.bookCounterPosition.y;

        // Text styling - professional and readable
        float textScale = 1.0f;  // Larger text for visibility
        Vec3 textColor = { 0.95f, 0.85f, 0.55f };  // Bright brass/gold color

        // Render the counter text
        m_textRenderer->RenderText(m_uiShader, counterText, x, y, textScale, textColor, 1.0f, m_VAO, m_VBO);
    }

    void UIRenderSystem::RenderSkillSlots(const UIComponent& ui)
    {
        float startX = ui.skillsPosition.x;
        float startY = ui.skillsPosition.y;
        float slotSize = ui.skillSlotSize;
        float spacing = ui.skillSlotSpacing;
        float radius = slotSize * 0.5f;

        // Collect non-empty skill slots to render
        std::vector<int> activeSlots;
        for (int i = 0; i < 4; ++i)
        {
            if (!ui.skills[i].skillName.empty())
            {
                activeSlots.push_back(i);
            }
        }

        if (activeSlots.empty())
            return;

        // Calculate total width of active slots to center them
        float totalWidth = (slotSize * activeSlots.size()) + (spacing * (activeSlots.size() - 1));
        float currentX = startX - (totalWidth * 0.5f);

        for (size_t slotIdx = 0; slotIdx < activeSlots.size(); ++slotIdx)
        {
            size_t i = activeSlots[slotIdx];
            const auto& skill = ui.skills[i];

            // Calculate center position
            float centerX = currentX + radius;
            float centerY = startY + radius;

            // Check if skill has a texture icon
            std::shared_ptr<graphics::Texture> skillTexture = nullptr;
            if (!skill.iconTexturePath.empty())
            {
                // Check if texture is already cached
                auto it = m_textureCache.find(skill.iconTexturePath);
                if (it != m_textureCache.end())
                {
                    skillTexture = it->second;
                }
                else
                {
                    // Load texture via AssetManager
                    skillTexture = AssetManager::GetInstance().LoadTexture(skill.iconTexturePath);
                    if (skillTexture && skillTexture->IsValid())
                    {
                        m_textureCache[skill.iconTexturePath] = skillTexture; // Cache it
                    }
                }
            }

            // ========================================================================
            // RENDER SKILL ICON (Square with correct aspect ratio)
            // ========================================================================
            if (skillTexture && skillTexture->IsValid())
            {
                // Render clean icon texture with full brightness
                Vec3 tintColor = ui.skillReadyTint;
                float alpha = ui.skillReadyAlpha;

                // Slightly dim when on cooldown
                if (skill.isOnCooldown)
                {
                    tintColor = ui.skillCooldownTint;
                    alpha = ui.skillCooldownAlpha;
                }
                // Slightly dim when insufficient health
                else if (ui.currentHealth < skill.manaCost)
                {
                    tintColor = ui.skillLowHealthTint;
                    alpha = ui.skillLowHealthAlpha;
                }

                // Use square rendering to maintain aspect ratio (size = diameter of old circle)
                RenderTexturedSquare(centerX, centerY, slotSize, skillTexture, tintColor, alpha);
            }
            else
            {
                // Fallback: render simple square if no texture
                Vec3 fallbackColor = ui.skillFallbackColor;
                float halfSize = slotSize * 0.5f;
                float adjustedHalfWidth = halfSize / m_aspectRatio;
                RenderQuad(centerX - adjustedHalfWidth, centerY - halfSize,
                          adjustedHalfWidth * 2.0f, slotSize, fallbackColor, ui.skillFallbackAlpha);
            }

            // ========================================================================
            // RENDER COOLDOWN OVERLAY (Radial pie chart over icon)
            // ========================================================================
            if (skill.isOnCooldown && skill.maxCooldown > 0.0f)
            {
                float progress = skill.currentCooldown / skill.maxCooldown;
                Vec3 cooldownColor = ui.skillCooldownOverlayColor;
                // Use radius for cooldown overlay (centered on square icon)
                RenderRadialCooldown(centerX, centerY, slotSize * 0.5f, progress, cooldownColor, ui.skillCooldownOverlayAlpha);
            }

            // ========================================================================
            // ACTIVATION FLASH EFFECT (Flash when skill is activated)
            // ========================================================================
            if (skill.activationFlashTimer > 0.0f)
            {
                // Calculate flash intensity (fades from 1.0 to 0.0 over duration)
                float flashIntensity = skill.activationFlashTimer / ui.skillFlashDuration;

                // Bright white/yellow flash
                float glowSize = slotSize + ui.skillFlashGlowSize; // Slightly larger than icon
                Vec3 flashColor = ui.skillFlashColor;

                // Render flash as a square border
                float halfSize = glowSize * 0.5f;
                float adjustedHalfWidth = halfSize / m_aspectRatio;
                RenderQuad(centerX - adjustedHalfWidth, centerY - halfSize,
                          adjustedHalfWidth * 2.0f, glowSize, flashColor, flashIntensity * 0.8f);
            }

            // ========================================================================
            // RENDER KEYBIND LABEL BELOW SKILL SLOT
            // ========================================================================
            if (m_textRenderer && m_uiShader && !skill.keyBinding.empty())
            {
                // Calculate label position (centered below the skill slot)
                float textScale = ui.skillKeybindTextScale;
                float textWidth = m_textRenderer->GetTextWidth(skill.keyBinding, textScale);
                float labelX = centerX - (textWidth * 0.5f); // Center horizontally
                float labelY = centerY - (slotSize * 0.5f) - ui.skillKeybindOffsetY; // Position below the square slot

                // Check if skill is ready to use
                bool isReady = !skill.isOnCooldown && ui.currentHealth >= skill.manaCost;

                // Professional white text with slight transparency
                Vec3 labelColor = ui.skillKeybindColor;
                float labelAlpha = isReady ? ui.skillKeybindAlphaReady : ui.skillKeybindAlphaNotReady;

                // Render the keybind text (e.g., "LMB", "RMB", "R")
                m_textRenderer->RenderText(m_uiShader, skill.keyBinding, labelX, labelY, textScale, labelColor, labelAlpha, m_VAO, m_VBO);
            }

            currentX += slotSize + spacing;
        }
    }

    void UIRenderSystem::RenderCrosshair(const UIComponent& ui)
    {
        float centerX = 0.5f;
        float centerY = 0.5f;
        float size = ui.crosshairSize;

        // Check if crosshair has a texture icon
        std::shared_ptr<graphics::Texture> crosshairTexture = nullptr;
        std::string crosshairPath = "../Resources/Textures/UI/crosshair.png";

        // Check if texture is already cached
        auto it = m_textureCache.find(crosshairPath);
        if (it != m_textureCache.end())
        {
            crosshairTexture = it->second;
        }
        else
        {
            // Load texture via AssetManager
            crosshairTexture = AssetManager::GetInstance().LoadTexture(crosshairPath);
            if (crosshairTexture && crosshairTexture->IsValid())
            {
                m_textureCache[crosshairPath] = crosshairTexture; // Cache it
            }
        }

        // Render crosshair icon if texture loaded successfully
        if (crosshairTexture && crosshairTexture->IsValid())
        {
            // Render clean icon texture with full brightness (no tinting)
            Vec3 tintColor = { 1.0f, 1.0f, 1.0f }; // No tinting - show texture as-is
            float alpha = 1.0f;

            // Use square rendering to maintain aspect ratio
            RenderTexturedSquare(centerX, centerY, size, crosshairTexture, tintColor, alpha);
        }
    }

    // ========================================================================
    // NEW RENDER FUNCTIONS FOR SEPARATE UI COMPONENTS
    // ========================================================================

    void UIRenderSystem::RenderHealthBarNew(const UIHealthbarComponent& healthbar)
    {
        float x = healthbar.healthbarPosition.x;
        float y = healthbar.healthbarPosition.y;
        float width = healthbar.healthbarWidth;
        float height = healthbar.healthbarHeight;

        // Try to load health bar textures if paths are specified
        std::shared_ptr<graphics::Texture> bgTex = nullptr;
        std::shared_ptr<graphics::Texture> fillTex = nullptr;
        std::shared_ptr<graphics::Texture> frameTex = nullptr;

        // Load background texture
        if (!healthbar.healthbarBgTexture.empty())
        {
            auto it = m_textureCache.find(healthbar.healthbarBgTexture);
            if (it != m_textureCache.end())
                bgTex = it->second;
            else
            {
                bgTex = AssetManager::GetInstance().LoadTexture(healthbar.healthbarBgTexture);
                if (bgTex && bgTex->IsValid())
                    m_textureCache[healthbar.healthbarBgTexture] = bgTex;
            }
        }

        // Load fill texture
        if (!healthbar.healthbarFillTexture.empty())
        {
            auto it = m_textureCache.find(healthbar.healthbarFillTexture);
            if (it != m_textureCache.end())
                fillTex = it->second;
            else
            {
                fillTex = AssetManager::GetInstance().LoadTexture(healthbar.healthbarFillTexture);
                if (fillTex && fillTex->IsValid())
                    m_textureCache[healthbar.healthbarFillTexture] = fillTex;
            }
        }

        // Load frame texture
        if (!healthbar.healthbarFrameTexture.empty())
        {
            auto it = m_textureCache.find(healthbar.healthbarFrameTexture);
            if (it != m_textureCache.end())
                frameTex = it->second;
            else
            {
                frameTex = AssetManager::GetInstance().LoadTexture(healthbar.healthbarFrameTexture);
                if (frameTex && frameTex->IsValid())
                    m_textureCache[healthbar.healthbarFrameTexture] = frameTex;
            }
        }

        // Render background
        if (bgTex && bgTex->IsValid())
        {
            RenderTexturedRect(x, y, width, height, bgTex, { 1.0f, 1.0f, 1.0f }, 0.9f);
        }
        else
        {
            float outerBorder = 0.003f;
            Vec3 outerBorderColor = { 0.1f, 0.1f, 0.1f };
            RenderQuad(x - outerBorder, y - outerBorder, width + outerBorder * 2.0f, height + outerBorder * 2.0f, outerBorderColor, 0.7f);

            float innerBorder = 0.0015f;
            Vec3 innerBorderColor = { 0.8f, 0.8f, 0.8f };
            RenderQuad(x - innerBorder, y - innerBorder, width + innerBorder * 2.0f, height + innerBorder * 2.0f, innerBorderColor, 0.8f);

            RenderQuad(x, y, width, height, healthbar.healthbarBgColor, 0.9f);
        }

        // Render health fill
        float healthPercent = (healthbar.maxHealth > 0.0f) ? (healthbar.currentHealth / healthbar.maxHealth) : 0.0f;
        float fillWidth = width * healthPercent;

        if (fillWidth > 0.0f)
        {
            if (fillTex && fillTex->IsValid())
            {
                float uMax = healthPercent;
                RenderTexturedRectUV(x, y, fillWidth, height, fillTex, 0.0f, 0.0f, uMax, 1.0f, { 1.0f, 1.0f, 1.0f }, 1.0f);
            }
            else
            {
                // Use configurable colors based on health percentage
                Vec3 healthColor = healthbar.healthbarColor;
                if (healthPercent < 0.25f)
                    healthColor = healthbar.healthbarCriticalColor;
                else if (healthPercent < 0.5f)
                    healthColor = healthbar.healthbarLowColor;

                RenderQuad(x, y, fillWidth, height, healthColor, 1.0f);

                // Shine effect (configurable!)
                float shineHeight = height * 0.4f;
                RenderQuad(x, y + height - shineHeight, fillWidth, shineHeight, healthbar.healthbarShineColor, healthbar.healthbarShineAlpha);
            }
        }

        // Render frame
        if (frameTex && frameTex->IsValid())
        {
            RenderTexturedRect(x, y, width, height, frameTex, { 1.0f, 1.0f, 1.0f }, 1.0f);
        }
    }

    void UIRenderSystem::RenderCrosshairNew(const UICrosshairComponent& crosshair)
    {
        float centerX = 0.5f;
        float centerY = 0.5f;
        float size = crosshair.crosshairSize;

        std::shared_ptr<graphics::Texture> crosshairTexture = nullptr;

        // Use texture path from component (editable in inspector!)
        if (!crosshair.crosshairTexturePath.empty())
        {
            auto it = m_textureCache.find(crosshair.crosshairTexturePath);
            if (it != m_textureCache.end())
            {
                crosshairTexture = it->second;
            }
            else
            {
                crosshairTexture = AssetManager::GetInstance().LoadTexture(crosshair.crosshairTexturePath);
                if (crosshairTexture && crosshairTexture->IsValid())
                    m_textureCache[crosshair.crosshairTexturePath] = crosshairTexture;
            }
        }

        if (crosshairTexture && crosshairTexture->IsValid())
        {
            Vec3 tintColor = { 1.0f, 1.0f, 1.0f };
            float alpha = 1.0f;
            RenderTexturedSquare(centerX, centerY, size, crosshairTexture, tintColor, alpha);
        }
    }

    void UIRenderSystem::RenderSkillSlotsNew(const UISkillsComponent& skills, EntityID entity)
    {
        // Check if entity has healthbar component for health-based tinting
        float currentHealth = 100.0f;
        bool hasHealthbar = ECS::GetInstance().HasComponent<UIHealthbarComponent>(entity);
        if (hasHealthbar)
            currentHealth = ECS::GetInstance().GetComponent<UIHealthbarComponent>(entity).currentHealth;

        // Render each skill slot (only if skillName is set)
        for (size_t i = 0; i < skills.skills.size(); ++i)
        {
            const auto& skill = skills.skills[i];

            // Only render skills with a name set
            if (skill.skillName.empty())
                continue;

            // Use slot's individual size (or fall back to component default)
            float slotSize = (skill.size > 0.0f) ? skill.size : skills.skillSlotSize;
            float radius = slotSize * 0.5f;

            // Use slot's individual position
            float centerX = skill.position.x;
            float centerY = skill.position.y;

            // Determine which icon to use based on selection state
            // Priority: selected/unselected icons > default iconTexturePath
            std::string iconPath = skill.iconTexturePath;  // Default fallback
            if (skill.isSelected && !skill.selectedIconPath.empty())
            {
                iconPath = skill.selectedIconPath;
            }
            else if (!skill.isSelected && !skill.unselectedIconPath.empty())
            {
                iconPath = skill.unselectedIconPath;
            }

            // Load skill icon texture
            std::shared_ptr<graphics::Texture> skillTexture = nullptr;
            if (!iconPath.empty())
            {
                auto it = m_textureCache.find(iconPath);
                if (it != m_textureCache.end())
                {
                    skillTexture = it->second;
                }
                else
                {
                    skillTexture = AssetManager::GetInstance().LoadTexture(iconPath);
                    if (skillTexture && skillTexture->IsValid())
                        m_textureCache[iconPath] = skillTexture;
                }
            }

            // Render skill icon
            if (skillTexture && skillTexture->IsValid())
            {
                Vec3 tintColor = skills.skillReadyTint;
                float alpha = skills.skillReadyAlpha;

                if (skill.isOnCooldown)
                {
                    tintColor = skills.skillCooldownTint;
                    alpha = skills.skillCooldownAlpha;
                }
                else if (currentHealth < skill.manaCost)
                {
                    tintColor = skills.skillLowHealthTint;
                    alpha = skills.skillLowHealthAlpha;
                }

                RenderTexturedSquare(centerX, centerY, slotSize, skillTexture, tintColor, alpha);
            }
            else
            {
                Vec3 fallbackColor = skills.skillFallbackColor;
                float halfSize = slotSize * 0.5f;
                float adjustedHalfWidth = halfSize / m_aspectRatio;
                RenderQuad(centerX - adjustedHalfWidth, centerY - halfSize,
                          adjustedHalfWidth * 2.0f, slotSize, fallbackColor, skills.skillFallbackAlpha);
            }

            // Render cooldown overlay
            if (skill.isOnCooldown && skill.maxCooldown > 0.0f)
            {
                float progress = skill.currentCooldown / skill.maxCooldown;
                Vec3 cooldownColor = skills.skillCooldownOverlayColor;
                RenderRadialCooldown(centerX, centerY, slotSize * 0.5f, progress, cooldownColor, skills.skillCooldownOverlayAlpha);
            }

            // Render activation flash
            if (skill.activationFlashTimer > 0.0f)
            {
                float flashIntensity = skill.activationFlashTimer / skills.skillFlashDuration;
                float glowSize = slotSize + skills.skillFlashGlowSize;
                Vec3 flashColor = skills.skillFlashColor;

                float halfSize = glowSize * 0.5f;
                float adjustedHalfWidth = halfSize / m_aspectRatio;
                RenderQuad(centerX - adjustedHalfWidth, centerY - halfSize,
                          adjustedHalfWidth * 2.0f, glowSize, flashColor, flashIntensity * 0.8f);
            }

            // Render keybind label
            if (m_textRenderer && m_uiShader && !skill.keyBinding.empty())
            {
                float textScale = skills.skillKeybindTextScale;
                float textWidth = m_textRenderer->GetTextWidth(skill.keyBinding, textScale);
                float labelX = centerX - (textWidth * 0.5f);
                float labelY = centerY - (slotSize * 0.5f) - skills.skillKeybindOffsetY;

                bool isReady = !skill.isOnCooldown && currentHealth >= skill.manaCost;
                Vec3 labelColor = skills.skillKeybindColor;
                float labelAlpha = isReady ? skills.skillKeybindAlphaReady : skills.skillKeybindAlphaNotReady;

                m_textRenderer->RenderText(m_uiShader, skill.keyBinding, labelX, labelY, textScale, labelColor, labelAlpha, m_VAO, m_VBO);
            }
        }
    }

    void UIRenderSystem::RenderManaBarNew(const UIManaBarComponent& manaBar)
    {
        float x = manaBar.manaBarPosition.x;
        float y = manaBar.manaBarPosition.y;
        float width = manaBar.manaBarWidth;
        float height = manaBar.manaBarHeight;

        // Render background
        RenderQuad(x, y, width, height, manaBar.manaBarBgColor, 0.9f);

        // Render mana fill
        float manaPercent = (manaBar.maxMana > 0.0f) ? (manaBar.currentMana / manaBar.maxMana) : 0.0f;
        float fillWidth = width * manaPercent;

        if (fillWidth > 0.0f)
        {
            RenderQuad(x, y, fillWidth, height, manaBar.manaBarColor, 1.0f);
        }
    }

    void UIRenderSystem::RenderBookCounterNew(const UIBookCounterComponent& bookCounter)
    {
        if (!m_textRenderer || !m_uiShader)
            return;

        std::string counterText = std::to_string(bookCounter.booksCollected) + "/" + std::to_string(bookCounter.totalBooks);
        float textWidth = m_textRenderer->GetTextWidth(counterText, bookCounter.textScale);

        float textX = bookCounter.bookCounterPosition.x - textWidth;
        float textY = bookCounter.bookCounterPosition.y;

        // Render optional background panel
        if (!bookCounter.backgroundTexture.empty())
        {
            std::shared_ptr<graphics::Texture> bgTexture = nullptr;
            auto it = m_textureCache.find(bookCounter.backgroundTexture);
            if (it != m_textureCache.end())
            {
                bgTexture = it->second;
            }
            else
            {
                bgTexture = AssetManager::GetInstance().LoadTexture(bookCounter.backgroundTexture);
                if (bgTexture && bgTexture->IsValid())
                    m_textureCache[bookCounter.backgroundTexture] = bgTexture;
            }

            if (bgTexture && bgTexture->IsValid())
            {
                float bgX = bookCounter.bookCounterPosition.x - bookCounter.backgroundSize.x;
                float bgY = bookCounter.bookCounterPosition.y;
                RenderTexturedRect(bgX, bgY, bookCounter.backgroundSize.x, bookCounter.backgroundSize.y, bgTexture, { 1.0f, 1.0f, 1.0f }, 0.9f);
            }
        }

        // Render optional book icon
        if (!bookCounter.bookIconTexture.empty())
        {
            std::shared_ptr<graphics::Texture> iconTexture = nullptr;
            auto it = m_textureCache.find(bookCounter.bookIconTexture);
            if (it != m_textureCache.end())
            {
                iconTexture = it->second;
            }
            else
            {
                iconTexture = AssetManager::GetInstance().LoadTexture(bookCounter.bookIconTexture);
                if (iconTexture && iconTexture->IsValid())
                    m_textureCache[bookCounter.bookIconTexture] = iconTexture;
            }

            if (iconTexture && iconTexture->IsValid())
            {
                float iconX = textX + bookCounter.bookIconOffsetX;
                float iconY = textY;
                RenderTexturedSquare(iconX + bookCounter.bookIconSize * 0.5f, iconY + bookCounter.bookIconSize * 0.5f,
                                    bookCounter.bookIconSize, iconTexture, { 1.0f, 1.0f, 1.0f }, 1.0f);
            }
        }

        // Render text (always visible)
        m_textRenderer->RenderText(m_uiShader, counterText, textX, textY, bookCounter.textScale, bookCounter.textColor, bookCounter.textAlpha, m_VAO, m_VBO);
    }

    void UIRenderSystem::RenderQuad(float posX, float posY, float width, float height, const Vec3& color, float alpha)
    {
        // Define quad vertices (2 triangles) with dummy texture coordinates
        float vertices[] = {
            // Position (x, y)    // Color (r, g, b, a)        // TexCoord (u, v)
            posX,         posY,          color.x, color.y, color.z, alpha,  0.0f, 0.0f,  // Bottom-left
            posX + width, posY,          color.x, color.y, color.z, alpha,  0.0f, 0.0f,  // Bottom-right
            posX + width, posY + height, color.x, color.y, color.z, alpha,  0.0f, 0.0f,  // Top-right

            posX,         posY,          color.x, color.y, color.z, alpha,  0.0f, 0.0f,  // Bottom-left
            posX + width, posY + height, color.x, color.y, color.z, alpha,  0.0f, 0.0f,  // Top-right
            posX,         posY + height, color.x, color.y, color.z, alpha,  0.0f, 0.0f   // Top-left
        };

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    void UIRenderSystem::RenderRadialCooldown(float centerX, float centerY, float radius, float progress, const Vec3& color, float alpha)
    {
        if (progress <= 0.0f)
            return;

        const int segments = 32;
        m_vertexData.clear();
        m_vertexData.reserve((segments + 2) * 8); // Center + perimeter vertices (8 floats per vertex)

        // Center vertex
        m_vertexData.push_back(centerX);
        m_vertexData.push_back(centerY);
        m_vertexData.push_back(color.x);
        m_vertexData.push_back(color.y);
        m_vertexData.push_back(color.z);
        m_vertexData.push_back(alpha);
        m_vertexData.push_back(0.0f); // texCoord u
        m_vertexData.push_back(0.0f); // texCoord v

        // Calculate how many segments to draw based on progress
        int segmentsToDraw = static_cast<int>(progress * segments);
        float angleStep = (2.0f * static_cast<float>(M_PI)) / segments;
        float startAngle = static_cast<float>(M_PI) * 0.5f; // Start from top (90 degrees)

        for (int i = 0; i <= segmentsToDraw; ++i)
        {
            float angle = startAngle - (i * angleStep); // Clockwise from top
            float x = centerX + radius * cosf(angle);
            float y = centerY + radius * sinf(angle);

            m_vertexData.push_back(x);
            m_vertexData.push_back(y);
            m_vertexData.push_back(color.x);
            m_vertexData.push_back(color.y);
            m_vertexData.push_back(color.z);
            m_vertexData.push_back(alpha);
            m_vertexData.push_back(0.0f); // texCoord u
            m_vertexData.push_back(0.0f); // texCoord v
        }

        // Render as triangle fan
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, m_vertexData.size() * sizeof(float), m_vertexData.data());
        glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(m_vertexData.size() / 8));
        glBindVertexArray(0);
    }

    void UIRenderSystem::RenderTexturedSquare(float centerX, float centerY, float size,
                                              std::shared_ptr<graphics::Texture> texture,
                                              const Vec3& color, float alpha)
    {
        if (!texture || !texture->IsValid())
            return;

        // Get texture dimensions to calculate its aspect ratio
        int texWidth = texture->GetWidth();
        int texHeight = texture->GetHeight();

        // Calculate texture aspect ratio (width / height)
        float textureAspectRatio = (texHeight > 0) ? static_cast<float>(texWidth) / static_cast<float>(texHeight) : 1.0f;

        // Calculate base dimensions accounting for screen aspect ratio
        float halfSize = size * 0.5f;

        // Adjust dimensions to maintain BOTH screen aspect ratio and texture aspect ratio
        // This prevents stretching of non-square textures
        float adjustedHalfWidth = (halfSize * textureAspectRatio) / m_aspectRatio;
        float adjustedHalfHeight = halfSize;

        // Calculate corner positions
        float left = centerX - adjustedHalfWidth;
        float right = centerX + adjustedHalfWidth;
        float bottom = centerY - adjustedHalfHeight;
        float top = centerY + adjustedHalfHeight;

        // Define quad vertices (2 triangles) with texture coordinates
        // FIXED: Flipped V-axis from (0,0) to (1,1) to (0,1) to (1,0) to correct upside-down UI images
        // This matches the fact that textures are loaded with stbi_set_flip_vertically_on_load(1)
        float vertices[] = {
            // Position (x, y)    // Color (r, g, b, a)                  // TexCoord (u, v)
            left,  bottom,        color.x, color.y, color.z, alpha,     0.0f, 1.0f,  // Bottom-left  (V flipped from 0.0 to 1.0)
            right, bottom,        color.x, color.y, color.z, alpha,     1.0f, 1.0f,  // Bottom-right (V flipped from 0.0 to 1.0)
            right, top,           color.x, color.y, color.z, alpha,     1.0f, 0.0f,  // Top-right    (V flipped from 1.0 to 0.0)

            left,  bottom,        color.x, color.y, color.z, alpha,     0.0f, 1.0f,  // Bottom-left  (V flipped from 0.0 to 1.0)
            right, top,           color.x, color.y, color.z, alpha,     1.0f, 0.0f,  // Top-right    (V flipped from 1.0 to 0.0)
            left,  top,           color.x, color.y, color.z, alpha,     0.0f, 0.0f   // Top-left     (V flipped from 1.0 to 0.0)
        };

        // Enable texture mode in shader
        m_uiShader->SetUniform1i("uUseTexture", 1);
        texture->Bind(0); // Bind to texture unit 0
        m_uiShader->SetUniform1i("uTexture", 0);

        // Render the square
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Disable texture mode
        texture->Unbind();
        m_uiShader->SetUniform1i("uUseTexture", 0);
    }

    void UIRenderSystem::RenderTexturedRect(float posX, float posY, float width, float height,
                                            std::shared_ptr<graphics::Texture> texture,
                                            const Vec3& color, float alpha)
    {
        if (!texture || !texture->IsValid())
            return;

        // Calculate corner positions
        float left = posX;
        float right = posX + width;
        float bottom = posY;
        float top = posY + height;

        // Define quad vertices (2 triangles) with texture coordinates
        float vertices[] = {
            // Position (x, y)    // Color (r, g, b, a)                  // TexCoord (u, v)
            left,  bottom,        color.x, color.y, color.z, alpha,     0.0f, 1.0f,  // Bottom-left
            right, bottom,        color.x, color.y, color.z, alpha,     1.0f, 1.0f,  // Bottom-right
            right, top,           color.x, color.y, color.z, alpha,     1.0f, 0.0f,  // Top-right

            left,  bottom,        color.x, color.y, color.z, alpha,     0.0f, 1.0f,  // Bottom-left
            right, top,           color.x, color.y, color.z, alpha,     1.0f, 0.0f,  // Top-right
            left,  top,           color.x, color.y, color.z, alpha,     0.0f, 0.0f   // Top-left
        };

        // Enable texture mode in shader
        m_uiShader->SetUniform1i("uUseTexture", 1);
        texture->Bind(0);
        m_uiShader->SetUniform1i("uTexture", 0);

        // Render the rectangle
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Disable texture mode
        texture->Unbind();
        m_uiShader->SetUniform1i("uUseTexture", 0);
    }

    void UIRenderSystem::RenderTexturedRectUV(float posX, float posY, float width, float height,
                                               std::shared_ptr<graphics::Texture> texture,
                                               float u0, float v0, float u1, float v1,
                                               const Vec3& color, float alpha)
    {
        if (!texture || !texture->IsValid())
            return;

        // Calculate corner positions
        float left = posX;
        float right = posX + width;
        float bottom = posY;
        float top = posY + height;

        // Flip V coordinates (OpenGL has origin at bottom-left, textures loaded with flip)
        float texV0 = 1.0f - v0;
        float texV1 = 1.0f - v1;

        // Define quad vertices (2 triangles) with custom UV coordinates
        float vertices[] = {
            // Position (x, y)    // Color (r, g, b, a)                  // TexCoord (u, v)
            left,  bottom,        color.x, color.y, color.z, alpha,     u0, texV0,   // Bottom-left
            right, bottom,        color.x, color.y, color.z, alpha,     u1, texV0,   // Bottom-right
            right, top,           color.x, color.y, color.z, alpha,     u1, texV1,   // Top-right

            left,  bottom,        color.x, color.y, color.z, alpha,     u0, texV0,   // Bottom-left
            right, top,           color.x, color.y, color.z, alpha,     u1, texV1,   // Top-right
            left,  top,           color.x, color.y, color.z, alpha,     u0, texV1    // Top-left
        };

        // Enable texture mode in shader
        m_uiShader->SetUniform1i("uUseTexture", 1);
        texture->Bind(0);
        m_uiShader->SetUniform1i("uTexture", 0);

        // Render the rectangle
        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Disable texture mode
        texture->Unbind();
        m_uiShader->SetUniform1i("uUseTexture", 0);
    }

    void UIRenderSystem::RenderButton(const UIButtonComponent& button)
    {
        // Choose color based on button state
        Vec3 currentColor = button.normalColor;
        if (button.isPressed)
            currentColor = button.pressedColor;
        else if (button.isHovered)
            currentColor = button.hoverColor;

        // Calculate button bounds (centered position) WITH ASPECT RATIO CORRECTION
        float halfWidth = button.size.x * 0.5f;
        float halfHeight = button.size.y * 0.5f;

        // Apply aspect ratio correction to width (same as other UI elements)
        float adjustedHalfWidth = halfWidth / m_aspectRatio;

        float left = button.position.x - adjustedHalfWidth;
        float bottom = button.position.y - halfHeight;
        float width = adjustedHalfWidth * 2.0f;
        float height = button.size.y;

        // Determine which image to use based on button state
        // Priority: pressedImage > hoverImage > normalImage (with fallback chain)
        std::string currentImagePath = button.normalImage;
        if (button.isPressed && !button.pressedImage.empty())
            currentImagePath = button.pressedImage;
        else if (button.isPressed && !button.hoverImage.empty())
            currentImagePath = button.hoverImage;  // Fallback pressed to hover
        else if (button.isHovered && !button.hoverImage.empty())
            currentImagePath = button.hoverImage;

        // Try to render with texture if image path is set
        std::shared_ptr<graphics::Texture> buttonTexture = nullptr;
        if (!currentImagePath.empty())
        {
            auto it = m_textureCache.find(currentImagePath);
            if (it != m_textureCache.end())
            {
                buttonTexture = it->second;
            }
            else
            {
                buttonTexture = AssetManager::GetInstance().LoadTexture(currentImagePath);
                if (buttonTexture && buttonTexture->IsValid())
                {
                    m_textureCache[currentImagePath] = buttonTexture;
                }
            }
        }

        // Render button background (textured or solid color)
        if (buttonTexture && buttonTexture->IsValid())
        {
            // Render textured button using button's size (matches hit detection area)
            Vec3 whiteTint = { 1.0f, 1.0f, 1.0f };
            RenderTexturedRect(left, bottom, width, height, buttonTexture, whiteTint, button.backgroundAlpha);
        }
        else
        {
            // Fallback: render solid color background
            RenderQuad(left, bottom, width, height, currentColor, button.backgroundAlpha);
        }

        // Render button text if present
        if (m_textRenderer && !button.text.empty())
        {
            // Calculate text position (centered)
            float textWidth = m_textRenderer->GetTextWidth(button.text, button.textScale);
            float textHeight = button.textScale * 0.04f;

            float textX = button.position.x - (textWidth * 0.5f);
            float textY = button.position.y - (textHeight * 0.5f);

            m_textRenderer->RenderText(
                m_uiShader,
                button.text,
                textX,
                textY,
                button.textScale,
                button.textColor,
                1.0f,
                m_VAO,
                m_VBO
            );
        }
    }

    void UIRenderSystem::RenderSlider(const UISliderComponent& slider)
    {
        // Calculate slider bounds with aspect ratio correction
        float halfWidth = slider.size.x * 0.5f;
        float halfHeight = slider.size.y * 0.5f;
        float adjustedHalfWidth = halfWidth / m_aspectRatio;

        float left = slider.position.x - adjustedHalfWidth;
        float bottom = slider.position.y - halfHeight;
        float width = adjustedHalfWidth * 2.0f;
        float height = slider.size.y;

        // Calculate value as normalized 0-1
        float normalizedValue = (slider.value - slider.minValue) / (slider.maxValue - slider.minValue);
        normalizedValue = std::max(0.0f, std::min(1.0f, normalizedValue));

        // Try to load track texture
        std::shared_ptr<graphics::Texture> trackTexture = nullptr;
        if (!slider.trackImage.empty())
        {
            auto it = m_textureCache.find(slider.trackImage);
            if (it != m_textureCache.end())
                trackTexture = it->second;
            else
            {
                trackTexture = AssetManager::GetInstance().LoadTexture(slider.trackImage);
                if (trackTexture && trackTexture->IsValid())
                    m_textureCache[slider.trackImage] = trackTexture;
            }
        }

        // Render track (background)
        if (trackTexture && trackTexture->IsValid())
        {
            Vec3 whiteTint = { 1.0f, 1.0f, 1.0f };
            RenderTexturedRect(left, bottom, width, height, trackTexture, whiteTint, slider.trackAlpha);
        }
        else
        {
            RenderQuad(left, bottom, width, height, slider.trackColor, slider.trackAlpha);
        }

        // Try to load fill texture
        std::shared_ptr<graphics::Texture> fillTexture = nullptr;
        if (!slider.fillImage.empty())
        {
            auto it = m_textureCache.find(slider.fillImage);
            if (it != m_textureCache.end())
                fillTexture = it->second;
            else
            {
                fillTexture = AssetManager::GetInstance().LoadTexture(slider.fillImage);
                if (fillTexture && fillTexture->IsValid())
                    m_textureCache[slider.fillImage] = fillTexture;
            }
        }

        // Render fill (based on value)
        float fillWidth = width * normalizedValue;
        if (fillWidth > 0.0f)
        {
            if (fillTexture && fillTexture->IsValid())
            {
                RenderTexturedRectUV(left, bottom, fillWidth, height, fillTexture,
                                     0.0f, 0.0f, normalizedValue, 1.0f,
                                     { 1.0f, 1.0f, 1.0f }, 1.0f);
            }
            else
            {
                RenderQuad(left, bottom, fillWidth, height, slider.fillColor, 1.0f);
            }
        }

        // Try to load handle texture
        std::shared_ptr<graphics::Texture> handleTexture = nullptr;
        if (!slider.handleImage.empty())
        {
            auto it = m_textureCache.find(slider.handleImage);
            if (it != m_textureCache.end())
                handleTexture = it->second;
            else
            {
                handleTexture = AssetManager::GetInstance().LoadTexture(slider.handleImage);
                if (handleTexture && handleTexture->IsValid())
                    m_textureCache[slider.handleImage] = handleTexture;
            }
        }

        // Calculate handle position
        float handleX = left + (width * normalizedValue);
        float handleY = slider.position.y;

        // Choose handle color based on state
        Vec3 currentHandleColor = slider.handleColor;
        if (slider.isHovered || slider.isDragging)
            currentHandleColor = slider.handleHoverColor;

        // Render handle
        if (handleTexture && handleTexture->IsValid())
        {
            Vec3 whiteTint = { 1.0f, 1.0f, 1.0f };
            RenderTexturedSquare(handleX, handleY, slider.handleSize, handleTexture, whiteTint, 1.0f);
        }
        else
        {
            // Render circular handle as a square for simplicity
            float handleHalfSize = slider.handleSize * 0.5f;
            float adjustedHandleHalfWidth = handleHalfSize / m_aspectRatio;
            RenderQuad(handleX - adjustedHandleHalfWidth, handleY - handleHalfSize,
                      adjustedHandleHalfWidth * 2.0f, slider.handleSize,
                      currentHandleColor, 1.0f);
        }

        // Render label image if present (swap between normal and active based on state)
        std::string labelImageToUse = slider.isDragging && !slider.labelActiveImagePath.empty()
            ? slider.labelActiveImagePath
            : slider.labelImagePath;

        if (!labelImageToUse.empty())
        {
            std::shared_ptr<graphics::Texture> labelTexture = nullptr;
            auto it = m_textureCache.find(labelImageToUse);
            if (it != m_textureCache.end())
                labelTexture = it->second;
            else
            {
                labelTexture = AssetManager::GetInstance().LoadTexture(labelImageToUse);
                if (labelTexture && labelTexture->IsValid())
                    m_textureCache[labelImageToUse] = labelTexture;
            }

            if (labelTexture && labelTexture->IsValid())
            {
                float labelImgX = slider.position.x + slider.labelOffset.x;
                float labelImgY = slider.position.y + slider.labelOffset.y;
                float labelImgSize = slider.labelScale * 0.1f;  // Scale based on labelScale

                RenderTexturedSquare(labelImgX, labelImgY, labelImgSize, labelTexture, { 1.0f, 1.0f, 1.0f }, 1.0f);
            }
        }

        // Render label text if present
        if (m_textRenderer && !slider.label.empty())
        {
            float labelX = slider.position.x + slider.labelOffset.x;
            float labelY = slider.position.y + slider.labelOffset.y;

            float textWidth = m_textRenderer->GetTextWidth(slider.label, slider.labelScale);
            labelX -= textWidth * 0.5f;  // Center the label

            m_textRenderer->RenderText(
                m_uiShader,
                slider.label,
                labelX,
                labelY,
                slider.labelScale,
                slider.labelColor,
                1.0f,
                m_VAO,
                m_VBO
            );
        }

        // Render value display if enabled
        if (m_textRenderer && slider.showValue)
        {
            std::string valueText;
            if (slider.valueAsPercentage)
            {
                int percent = static_cast<int>(normalizedValue * 100.0f + 0.5f);
                valueText = std::to_string(percent) + "%";
            }
            else
            {
                // Show raw value with 1 decimal place
                char buf[32];
                snprintf(buf, sizeof(buf), "%.1f", slider.value);
                valueText = buf;
            }

            float valueX = slider.position.x + slider.valueOffset.x;
            float valueY = slider.position.y + slider.valueOffset.y;

            // Center the value text vertically relative to the offset position
            float valTextWidth = m_textRenderer->GetTextWidth(valueText, slider.valueScale);
            valueX -= valTextWidth * 0.5f;

            m_textRenderer->RenderText(
                m_uiShader,
                valueText,
                valueX,
                valueY,
                slider.valueScale,
                slider.valueColor,
                1.0f,
                m_VAO,
                m_VBO
            );
        }
    }

    void UIRenderSystem::RenderTextComponent(const UITextComponent& textComp)
    {
        if (!m_textRenderer || textComp.text.empty())
            return;

        float textScale = textComp.fontSize;

        // Calculate starting X based on alignment
        float textX = textComp.position.x;
        if (textComp.alignment == 1) // Center
        {
            float textWidth = m_textRenderer->GetTextWidth(textComp.text, textScale);
            textX -= textWidth * 0.5f;
        }
        else if (textComp.alignment == 2) // Right
        {
            float textWidth = m_textRenderer->GetTextWidth(textComp.text, textScale);
            textX -= textWidth;
        }

        float textY = textComp.position.y;

        m_textRenderer->RenderText(
            m_uiShader,
            textComp.text,
            textX,
            textY,
            textScale,
            textComp.color,
            textComp.alpha,
            m_VAO,
            m_VBO
        );
    }

} // namespace Ermine
