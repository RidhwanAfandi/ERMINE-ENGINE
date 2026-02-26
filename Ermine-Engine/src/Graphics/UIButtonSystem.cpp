/* Start Header ************************************************************************/
/*!
\file       UIButtonSystem.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       11/2025
\brief      System for handling UI button interactions (hover, click, actions)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "UIButtonSystem.h"
#include "ECS.h"
#include "Input.h"
#include "Components.h"
#include "SceneManager.h"
#include "Logger.h"
#include "AudioManager.h"
#include "AudioSystem.h"
#include "GLFW/glfw3.h"
#include "EditorGUI.h"
#include "Window.h"

#ifdef EE_EDITOR
#include "EditorGUI.h"
#include "imgui.h"
#endif

namespace Ermine
{
    void UIButtonSystem::Init(int screenWidth, int screenHeight)
    {
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_aspectRatio = (screenHeight > 0) ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight) : 1.0f;
        EE_CORE_INFO("UIButtonSystem initialized ({}x{}, aspect ratio: {})", screenWidth, screenHeight, m_aspectRatio);

        auto& ecs = ECS::GetInstance();
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            if (meta.name == "PauseMenu" || meta.name == "PauseBackground" || meta.name == "ResumeButton")
            {
                EE_CORE_INFO("Entity '{}' (ID: {}) has selfActive = {}", meta.name, e, meta.selfActive);
            }
        }

        EE_CORE_INFO("UIButtonSystem::s_isGamePaused = {}", s_isGamePaused);
        EE_CORE_INFO("UIButtonSystem initialized ({}x{}, aspect ratio: {})", screenWidth, screenHeight, m_aspectRatio);

    }

    EntityID UIButtonSystem::GetGlobalAudioEntity()
    {
        auto& ecs = ECS::GetInstance();

        // Check if cached entity is still valid
        if (ecs.IsEntityValid(m_GlobalAudioEntity) &&
            ecs.HasComponent<GlobalAudioComponent>(m_GlobalAudioEntity))
        {
            return m_GlobalAudioEntity;
        }

        // Find and cache GlobalAudio entity
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<GlobalAudioComponent>(e)) continue;

            m_GlobalAudioEntity = e;
            EE_CORE_INFO("UIButtonSystem: Cached GlobalAudio entity (ID: {})", e);
            return e;
        }

        // Not found - invalidate cache
        m_GlobalAudioEntity = MAX_ENTITIES;
        return MAX_ENTITIES;
    }

    void UIButtonSystem::Update(float deltaTime)
    {
        auto& ecs = ECS::GetInstance();

#ifdef EE_EDITOR
        // Only allow button interactions in play mode or UI preview mode when in editor
        if (!editor::EditorGUI::isPlaying && !editor::EditorGUI::isPreviewingUI)
            return;
#endif

        // ==================== PAUSE MENU TOGGLE ====================
        static bool pWasPressed = false;
        bool pIsPressed = Input::IsKeyDown(GLFW_KEY_P) || Input::IsKeyDown(GLFW_KEY_ESCAPE);

        if (pIsPressed && !pWasPressed)
        {
            TogglePauseMenu();
        }
        pWasPressed = pIsPressed;
        // ===========================================================

        EntityID globalAudioEntity = GetGlobalAudioEntity();
        GlobalAudioComponent* globalAudio = nullptr;

        if (globalAudioEntity != MAX_ENTITIES)
        {
            globalAudio = &ecs.GetComponent<GlobalAudioComponent>(globalAudioEntity);
        }

        // Get normalized mouse position once per frame
        float mouseX, mouseY;
        GetNormalizedMousePosition(mouseX, mouseY);

        // ==================== CHECK FOR MODAL/OVERLAY UI ====================
        // If SettingsMenu (or similar overlay) is active, only process UI within that hierarchy
        bool hasActiveOverlay = false;
        EntityID overlayRootEntity = MAX_ENTITIES;

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            // Check for known overlay/modal entities
            if ((meta.name == "SettingsMenu" || meta.name == "PauseMenu") && meta.selfActive)
            {
                hasActiveOverlay = true;
                overlayRootEntity = e;
                break;
            }
        }

        // Iterate through all entities that have UIButtonComponent
        for (EntityID entity : m_Entities)
        {
            if (!ecs.IsEntityValid(entity) || !ecs.HasComponent<UIButtonComponent>(entity))
                continue;

            // ✅ FIX: Check if entity is active in hierarchy (including parents)
            if (!IsEntityActiveInHierarchy(entity))
                continue;

            // ✅ FIX: If overlay is active, only process buttons that are children of the overlay
            if (hasActiveOverlay && !IsEntityChildOf(entity, overlayRootEntity))
            {
                // Reset hover state for buttons outside the overlay
                auto& button = ecs.GetComponent<UIButtonComponent>(entity);
                button.isHovered = false;
                button.isPressed = false;
                continue;
            }

            auto& button = ecs.GetComponent<UIButtonComponent>(entity);

            // Calculate button bounds in normalized space
            float halfW = (button.size.x * 0.5f) / m_aspectRatio;
            float halfH = button.size.y * 0.5f;
            float left = button.position.x - halfW;
            float right = button.position.x + halfW;
            float bottom = button.position.y - halfH;
            float top = button.position.y + halfH;

            // Check if mouse is inside button bounds
            bool inside = (mouseX >= left && mouseX <= right && mouseY >= bottom && mouseY <= top);

            // Update hover state
            if (inside && !button.isHovered)
            {
                button.isHovered = true;

                if (globalAudio)
                {
                    AudioSystem::PlayGlobalSFX(*globalAudio, "Hover");
                    EE_CORE_INFO("Playing hover sound");
                }
            }
            else if (!inside && button.isHovered)
            {
                button.isHovered = false;
                button.isPressed = false;
            }

            // Handle click
            if (inside && Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
            {
                button.isPressed = true;
                if (globalAudio)
                {
                    AudioSystem::PlayGlobalSFX(*globalAudio, "Click");
                    EE_CORE_INFO("Playing click sound");
                }

                ExecuteButtonAction(button);
            }
            else if (!Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
            {
                button.isPressed = false;
            }
        }

        // ==================== UI SLIDER INTERACTION ====================
        for (EntityID entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            if (!ecs.IsEntityValid(entity))
                continue;

            if (!ecs.HasComponent<UISliderComponent>(entity))
                continue;

            if (!IsEntityActiveInHierarchy(entity))
                continue;

            // ✅ FIX: If overlay is active, only process sliders that are children of the overlay
            if (hasActiveOverlay && !IsEntityChildOf(entity, overlayRootEntity))
            {
                auto& slider = ecs.GetComponent<UISliderComponent>(entity);
                slider.isHovered = false;
                slider.isDragging = false;
                continue;
            }

            auto& slider = ecs.GetComponent<UISliderComponent>(entity);

            // Calculate slider bounds in normalized space (matching RenderSlider)
            float halfWidth = slider.size.x * 0.5f;
            float halfHeight = slider.size.y * 0.5f;
            float adjustedHalfWidth = halfWidth / m_aspectRatio;

            float left = slider.position.x - adjustedHalfWidth;
            float right = slider.position.x + adjustedHalfWidth;
            float bottom = slider.position.y - halfHeight;
            float top = slider.position.y + halfHeight;
            float width = adjustedHalfWidth * 2.0f;

            // Expand hit area slightly for easier interaction
            float handleHalfSize = slider.handleSize * 0.5f;
            float adjustedHandleHalfWidth = handleHalfSize / m_aspectRatio;
            float normalizedValue = (slider.value - slider.minValue) / (slider.maxValue - slider.minValue);
            float handleX = left + (width * normalizedValue);

            // Check if mouse is over the slider track or handle
            bool insideTrack = (mouseX >= left && mouseX <= right && mouseY >= bottom && mouseY <= top);
            bool insideHandle = (mouseX >= handleX - adjustedHandleHalfWidth && mouseX <= handleX + adjustedHandleHalfWidth &&
                                mouseY >= bottom - handleHalfSize && mouseY <= top + handleHalfSize);

            bool inside = insideTrack || insideHandle;

            // Update hover state
            slider.isHovered = inside;

            // Handle drag start
            if (inside && Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
            {
                slider.isDragging = true;
            }

            // Handle dragging
            if (slider.isDragging)
            {
                if (Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT))
                {
                    // Calculate new value based on mouse position (matching RenderSlider calculation)
                    float newNormalizedValue = (mouseX - left) / width;
                    newNormalizedValue = std::max(0.0f, std::min(1.0f, newNormalizedValue));
                    slider.value = slider.minValue + (newNormalizedValue * (slider.maxValue - slider.minValue));

                    // Apply value to target
                    ApplySliderValue(slider, globalAudioEntity);
                }
                else
                {
                    // Mouse released, stop dragging
                    slider.isDragging = false;
                }
            }
        }
        // ==============================================================

        // CRITICAL: Process pending scene load AFTER iteration completes
        if (m_HasPendingSceneLoad)
        {
            EE_CORE_INFO("Executing deferred scene load: {}", m_PendingSceneToLoad);
            try
            {
#ifdef EE_EDITOR
                auto& sceneManager = SceneManager::GetInstance();
                sceneManager.OpenScene(m_PendingSceneToLoad);
#else
                SceneManager::GetInstance().OpenScene(m_PendingSceneToLoad);
#endif
            }
            catch (const std::exception& e)
            {
                EE_CORE_ERROR("Failed to load scene '{}': {}", m_PendingSceneToLoad, e.what());
            }

            m_HasPendingSceneLoad = false;
            m_PendingSceneToLoad.clear();
        }
    }

    bool UIButtonSystem::IsEntityActiveInHierarchy(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        // Check if entity itself is valid
        if (!ecs.IsEntityValid(entity))
            return false;

        // Check self active state
        if (ecs.HasComponent<ObjectMetaData>(entity))
        {
            auto& meta = ecs.GetComponent<ObjectMetaData>(entity);
            if (!meta.selfActive)
                return false;
        }

        // Check parent chain via HierarchyComponent
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return true;  // No hierarchy = root entity, considered active

        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);

        // If has a valid parent, check if parent is active
        if (hierarchy.parent == HierarchyComponent::INVALID_PARENT)
            return true;  // No parent = root entity

        // Validate parent entity before recursing
        if (!ecs.IsEntityValid(hierarchy.parent))
            return true;  // Invalid parent reference, treat as root

        // Recursively check parent's active state
        return IsEntityActiveInHierarchy(hierarchy.parent);
    }

    bool UIButtonSystem::IsEntityChildOf(EntityID entity, EntityID potentialParent)
    {
        auto& ecs = ECS::GetInstance();

        // Entity is considered a child of itself (for the overlay root case)
        if (entity == potentialParent)
            return true;

        if (!ecs.IsEntityValid(entity))
            return false;

        // Check parent chain via HierarchyComponent
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return false;

        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);

        // If has a valid parent, check if it matches or recurse
        if (hierarchy.parent == HierarchyComponent::INVALID_PARENT)
            return false;

        // Validate parent entity before recursing
        if (!ecs.IsEntityValid(hierarchy.parent))
            return false;

        if (hierarchy.parent == potentialParent)
            return true;

        // Recursively check parent chain
        return IsEntityChildOf(hierarchy.parent, potentialParent);
    }

    void UIButtonSystem::TogglePauseMenu()
    {
        auto& ecs = ECS::GetInstance();

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            if (meta.name == "PauseMenu")
            {
                meta.selfActive = !meta.selfActive;
                s_isGamePaused = meta.selfActive;

                // ✅ CRITICAL FIX: Use the same pause mechanism as alt-tab (EditorGUI::s_state)
                // This ensures audio and ALL systems respect the pause, not just game logic
                editor::EditorGUI::s_state = s_isGamePaused
                    ? editor::EditorGUI::SimState::paused
                    : editor::EditorGUI::SimState::playing;

                s_isGamePaused ? Window::SetCursorLockState(Window::CursorLockState::None) : Window::SetCursorLockState(Window::CursorLockState::Confined);

                EE_CORE_INFO("Game {} (using EditorGUI::s_state)", s_isGamePaused ? "PAUSED" : "RESUMED");

                return;
            }
        }

        EE_CORE_WARN("PauseMenu entity not found in scene!");
    }

    bool UIButtonSystem::IsGamePaused()
    {
        return s_isGamePaused;
    }

    void UIButtonSystem::ExecuteButtonAction(const UIButtonComponent& button)
    {
        EE_CORE_INFO("Button '{}' clicked with action: {}", button.text, static_cast<int>(button.action));

        switch (button.action)
        {
        case UIButtonComponent::ButtonAction::LoadScene:
            if (!button.actionData.empty())
            {
                EE_CORE_INFO("Queueing scene load: {}", button.actionData);

                // CRITICAL FIX: Don't load immediately - defer until after Update() completes
                // Store the scene path to load at the end of the frame
                m_PendingSceneToLoad = button.actionData;
                m_HasPendingSceneLoad = true;
            }
            else
            {
                EE_CORE_WARN("LoadScene action has no scene path!");
            }
            break;

        case UIButtonComponent::ButtonAction::Quit:
            EE_CORE_INFO("Quit action triggered");
            if (auto* window = glfwGetCurrentContext())
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            break;

        case UIButtonComponent::ButtonAction::Custom:
            if (button.actionData == "Resume" || button.actionData == "resume_game")
            {
                TogglePauseMenu();
                EE_CORE_INFO("Resume button clicked");
            }
            else if (button.actionData == "OpenControls")
            {
                // Show ControlsScreen, hide main menu buttons
                SetEntityActiveByName("ControlsScreen", true);
                SetEntityActiveByName("Play Button", false);
                SetEntityActiveByName("Controls", false);  // Use actual button name
                SetEntityActiveByName("Audio", false);
                SetEntityActiveByName("Quit Button", false);
            }
            else if (button.actionData == "CloseControlsScreen")
            {
                // Hide ControlsScreen, show main menu buttons
                SetEntityActiveByName("ControlsScreen", false);
                SetEntityActiveByName("Play Button", true);
                SetEntityActiveByName("Controls", true);  // Use actual button name
                SetEntityActiveByName("Audio", true);
                SetEntityActiveByName("Quit Button", true);
            }
            else if (button.actionData == "OpenSettings")
            {
                auto& ecs = ECS::GetInstance();

                // Show SettingsMenu
                SetEntityActiveByName("SettingsMenu", true);

                // Hide all buttons EXCEPT Back Button (which is inside SettingsMenu)
                for (EntityID e = 0; e < MAX_ENTITIES; ++e)
                {
                    if (!ecs.IsEntityValid(e)) continue;
                    if (!ecs.HasComponent<ObjectMetaData>(e)) continue;
                    if (!ecs.HasComponent<UIButtonComponent>(e)) continue;

                    auto& meta = ecs.GetComponent<ObjectMetaData>(e);

                    // Don't hide buttons that are inside SettingsMenu
                    if (ecs.HasComponent<HierarchyComponent>(e))
                    {
                        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(e);
                        EntityID parent = hierarchy.parent;

                        // Check if parent is SettingsMenu
                        if (ecs.IsEntityValid(parent) && ecs.HasComponent<ObjectMetaData>(parent))
                        {
                            auto& parentMeta = ecs.GetComponent<ObjectMetaData>(parent);
                            if (parentMeta.name == "SettingsMenu")
                            {
                                continue; // Skip hiding this button
                            }
                        }
                    }

                    // Hide all other buttons
                    meta.selfActive = false;
                }

                // Also hide backgrounds
                SetEntityActiveByName("PauseBackground", false);
                //SetEntityActiveByName("MenuBackground", false);
            }
            else if (button.actionData == "CloseSettings")
            {
                auto& ecs = ECS::GetInstance();

                // Hide SettingsMenu
                SetEntityActiveByName("SettingsMenu", false);

                // Show all buttons that were hidden
                for (EntityID e = 0; e < MAX_ENTITIES; ++e)
                {
                    if (!ecs.IsEntityValid(e)) continue;
                    if (!ecs.HasComponent<ObjectMetaData>(e)) continue;
                    if (!ecs.HasComponent<UIButtonComponent>(e)) continue;

                    auto& meta = ecs.GetComponent<ObjectMetaData>(e);

                    // Don't show buttons that are inside SettingsMenu
                    if (ecs.HasComponent<HierarchyComponent>(e))
                    {
                        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(e);
                        EntityID parent = hierarchy.parent;

                        if (ecs.IsEntityValid(parent) && ecs.HasComponent<ObjectMetaData>(parent))
                        {
                            auto& parentMeta = ecs.GetComponent<ObjectMetaData>(parent);
                            if (parentMeta.name == "SettingsMenu")
                            {
                                continue; // Skip showing this button
                            }
                        }
                    }

                    // Show all other buttons
                    meta.selfActive = true;
                }

                // Show backgrounds
                SetEntityActiveByName("PauseBackground", true);
                //SetEntityActiveByName("MenuBackground", true);
            }
            else if (button.actionData == "ShowTeleportInfo")
            {
                ShowControlInfo("Teleport_Info");
            }
            else if (button.actionData == "ShowShootingInfo")
            {
                ShowControlInfo("Shooting_Info");
            }
            else if (button.actionData == "ShowReturnInfo")
            {
				ShowControlInfo("Return_Info");
            }
            else if (button.actionData == "ShowLightDInfo")
            {
				ShowControlInfo("LightD_Info");
            }
            else if (button.actionData == "CloseControlsScreen")
            {
                CloseControlsScreen();
            }
            else
            {
                EE_CORE_INFO("Custom action triggered: {}", button.actionData);
            }
            break;

        case UIButtonComponent::ButtonAction::None:
        default:
            EE_CORE_WARN("Button '{}' has no action assigned", button.text);
            break;
        }
    }

    void UIButtonSystem::CloseControlsScreen()
    {
        auto& ecs = ECS::GetInstance();

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);

            // Hide the entire ControlsScreen
            if (meta.name == "ControlsScreen")
            {
                meta.selfActive = false;
                return;
            }
        }

    }

    void UIButtonSystem::ShowControlInfo(const std::string& infoToShow)
    {
        auto& ecs = ECS::GetInstance();

        // List of all info panels
        std::vector<std::string> allInfoPanels = {
            "Teleport_Info",
            "Shooting_Info",
			"Return_Info",
			"LightD_Info"
            // Add more as needed
        };

        // Hide all panels first, then show the requested one
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);

            // Check if this entity is an info panel
            for (const auto& panelName : allInfoPanels)
            {
                if (meta.name == panelName)
                {
                    // Show only the requested panel, hide others
                    meta.selfActive = (meta.name == infoToShow);

                    if (meta.selfActive)
                    {
                        EE_CORE_INFO("Showing info panel: {}", meta.name);
                    }
                }
            }
        }
    }

    void UIButtonSystem::ShowPauseMenuOnAltTab()
    {
        auto& ecs = ECS::GetInstance();

        // Search for PauseMenu entity
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            if (meta.name == "PauseMenu")
            {
                meta.selfActive = true;
                s_isGamePaused = true;
                EE_CORE_INFO("Pause menu shown (alt-tab)");
                Window::SetCursorLockState(Window::CursorLockState::None);
                return;
            }
        }

        // No pause menu found - just pause gameplay
        s_isGamePaused = true;
        EE_CORE_INFO("No pause menu in scene - gameplay paused only");
    }

    void UIButtonSystem::TryAutoResumeOnAltTab()
    {
        auto& ecs = ECS::GetInstance();

        // Check if pause menu exists
        bool hasPauseMenu = false;
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            if (meta.name == "PauseMenu")
            {
                hasPauseMenu = true;
                break;
            }
        }

        // If no pause menu exists (e.g., main menu scene), auto-resume
        if (!hasPauseMenu)
        {
#if defined(EE_EDITOR)
            if (editor::EditorGUI::s_state == editor::EditorGUI::SimState::paused)
            {
                editor::EditorGUI::s_state = editor::EditorGUI::SimState::playing;
                s_isGamePaused = false;
                EE_CORE_INFO("Auto-resumed (no pause menu in scene)");
            }
#else
            if (editor::EditorGUI::s_state == editor::EditorGUI::SimState::paused)
            {
                editor::EditorGUI::s_state = editor::EditorGUI::SimState::playing;
                s_isGamePaused = false;
                EE_CORE_INFO("Auto-resumed (no pause menu in scene)");
            }
#endif
        }
        // Otherwise, let the pause menu's Resume button handle it
    }

    void UIButtonSystem::GetNormalizedMousePosition(float& outX, float& outY)
    {
#ifdef EE_EDITOR
        // EDITOR MODE: Get mouse position from ImGui
        ImGuiIO& io = ImGui::GetIO();

        // Get mouse position in screen space
        float screenX = io.MousePos.x;
        float screenY = io.MousePos.y;

        // Convert to viewport-relative coordinates
        float viewportX = screenX - m_viewportMin.x;
        float viewportY = screenY - m_viewportMin.y;

        // Check if mouse is outside viewport
        if (viewportX < 0.0f || viewportY < 0.0f ||
            viewportX > m_viewportSize.x || viewportY > m_viewportSize.y)
        {
            outX = -1.0f;
            outY = -1.0f;
            return;
        }

        // Normalize to 0-1 range
        // ImGui Y goes down (0 at top), OpenGL Y goes up (0 at bottom)
        outX = viewportX / m_viewportSize.x;
        outY = 1.0f - (viewportY / m_viewportSize.y);  // Flip Y

#else
        // GAME MODE: Get mouse position from GLFW
        auto [mouseX, mouseY] = Input::GetMousePosition();
        auto window = glfwGetCurrentContext();
        if (!window)
        {
            outX = -1.0f;
            outY = -1.0f;
            return;
        }

        int windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);

        // Normalize to 0-1 range
        outX = mouseX / static_cast<float>(windowWidth);
        outY = 1.0f - (mouseY / static_cast<float>(windowHeight));  // Flip Y

        // Clamp
        outX = std::max(0.0f, std::min(1.0f, outX));
        outY = std::max(0.0f, std::min(1.0f, outY));
#endif
    }

    void UIButtonSystem::SetEntityActiveByName(const std::string& name, bool active)
    {
        auto& ecs = ECS::GetInstance();

        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<ObjectMetaData>(e)) continue;

            auto& meta = ecs.GetComponent<ObjectMetaData>(e);
            if (meta.name == name)
            {
                meta.selfActive = active;
                EE_CORE_INFO("Set entity '{}' (ID: {}) selfActive = {}", name, e, active);
                return;
            }
        }

        EE_CORE_WARN("Entity '{}' not found!", name);
    }

    void UIButtonSystem::ApplySliderValue(const UISliderComponent& slider, EntityID globalAudioEntity)
    {
        auto& ecs = ECS::GetInstance();

        // Only apply if we have a valid target
        if (slider.target == UISliderComponent::SliderTarget::None)
            return;

        // Get GlobalAudioComponent if needed for audio targets
        if (slider.target != UISliderComponent::SliderTarget::Custom &&
            slider.target != UISliderComponent::SliderTarget::None)
        {
            if (globalAudioEntity == MAX_ENTITIES || !ecs.IsEntityValid(globalAudioEntity))
                return;

            if (!ecs.HasComponent<GlobalAudioComponent>(globalAudioEntity))
                return;

            auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(globalAudioEntity);

            switch (slider.target)
            {
            case UISliderComponent::SliderTarget::MasterVolume:
                globalAudio.masterVolume = slider.value;
                // Re-apply volumes to update all playing sounds
                globalAudio.SetMusicVolume(globalAudio.musicVolume);
                globalAudio.SetSFXVolume(globalAudio.sfxVolume);
                globalAudio.SetAmbienceVolume(globalAudio.ambienceVolume);
                break;

            case UISliderComponent::SliderTarget::MusicVolume:
                globalAudio.SetMusicVolume(slider.value);
                break;

            case UISliderComponent::SliderTarget::SFXVolume:
                globalAudio.SetSFXVolume(slider.value);
                break;

            case UISliderComponent::SliderTarget::AmbienceVolume:
                globalAudio.SetAmbienceVolume(slider.value);
                break;

            default:
                break;
            }
        }
        else if (slider.target == UISliderComponent::SliderTarget::Custom)
        {
            // Custom target handling can be extended here
            EE_CORE_INFO("Custom slider '{}' value: {}", slider.customTarget, slider.value);
        }
    }
}