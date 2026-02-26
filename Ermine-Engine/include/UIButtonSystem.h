/* Start Header ************************************************************************/
/*!
\file       UIButtonSystem.h
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       11/2025
\brief      System for handling UI button interactions (hover, click, actions)
Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "Systems.h"
#include "Components.h"

#ifdef EE_EDITOR
#include "imgui.h"
#endif

namespace Ermine
{
    class UIButtonSystem : public System
    {
    public:
        UIButtonSystem() = default;
        ~UIButtonSystem() = default;

        void Init(int screenWidth = 1920, int screenHeight = 1080);
        void Update(float deltaTime);

        // Update screen dimensions and aspect ratio (should match UIRenderSystem)
        void OnScreenResize(int width, int height)
        {
            m_screenWidth = width;
            m_screenHeight = height;
            m_aspectRatio = (height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        }

        static bool IsGamePaused();  // ADD THIS

        static void ShowPauseMenuOnAltTab();
        static void TryAutoResumeOnAltTab();

#ifdef EE_EDITOR
        // Set viewport info for editor mode (called from EditorGUI)
        // NOTE: Only updates viewport position/size for mouse conversion.
        // Aspect ratio must stay synced with UIRenderSystem (screen-based).
        void SetViewportInfo(const ImVec2& min, const ImVec2& size)
        {
            m_viewportMin = min;
            m_viewportSize = size;
            // Do NOT update m_aspectRatio here - it must match UIRenderSystem
        }
#endif

    private:
        // Check if mouse position is within button bounds
        bool IsMouseOverButton(const UIButtonComponent& button, float mouseX, float mouseY);

        // Execute button action
        void ExecuteButtonAction(const UIButtonComponent& button);

        // Get normalized mouse position (0-1 range)
        void GetNormalizedMousePosition(float& outX, float& outY);

        // Apply slider value to its target (audio volume, etc.)
        void ApplySliderValue(const UISliderComponent& slider, EntityID globalAudioEntity);

        static inline bool s_isGamePaused = false;  // ADD THIS
        bool IsEntityActiveInHierarchy(EntityID entity);
        bool IsEntityChildOf(EntityID entity, EntityID potentialParent);  // Check if entity is child of another
        void TogglePauseMenu();  // ADD THIS
        void SetEntityActiveByName(const std::string& name, bool active);  // Show/hide entities by name
        void ShowControlInfo(const std::string& infoToShow);
        void CloseControlsScreen();

        EntityID GetGlobalAudioEntity();
        EntityID m_GlobalAudioEntity = MAX_ENTITIES;

        // Track mouse state
        bool m_wasMousePressed = false;

        // Screen dimensions and aspect ratio (must match UIRenderSystem)
        int m_screenWidth = 1920;
        int m_screenHeight = 1080;
        float m_aspectRatio = 1.0f; // width / height

#ifdef EE_EDITOR
        // Viewport tracking for editor mode
        ImVec2 m_viewportMin = { 0, 0 };      // Top-left corner of viewport in screen space
        ImVec2 m_viewportSize = { 1920, 1080 }; // Size of viewport
#endif

        // NEW: Deferred scene loading to prevent iterator invalidation
        bool m_HasPendingSceneLoad = false;
        std::string m_PendingSceneToLoad;
    };
}