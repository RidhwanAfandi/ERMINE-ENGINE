/* Start Header ************************************************************************/
/*!
\file       GraphicsDebugGUI.h
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       31/01/2026
\brief      This file contains the declaration of the GraphicsDebugGUI class.
            A debug GUI for graphics-related parameters and controls using ImGui.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "ImGuiUIWindow.h"

namespace Ermine::graphics
{
    class Renderer;
}

namespace Ermine::editor
{
    /**
     * @brief Graphics Debug GUI window for controlling rendering parameters
     * Provides ImGui interface for renderer settings, post-processing, and performance monitoring
     */
    class GraphicsDebugGUI : public Ermine::ImGUIWindow
    {
    public:
        /**
         * @brief Constructor
         * @param title Window title for ImGui window
         */
        explicit GraphicsDebugGUI(const std::string& title = "Graphics Settings");

        /**
         * @brief Update the GUI - called every frame
         * Renders all the ImGui controls and handles parameter updates
         */
        void Update() override;

        /**
         * @brief Render the GUI - called after Update
         * Required by ImGUIWindow base class
         */
        void Render() override;

    private:
        std::string m_title; /// Window title
        
        // UI organization
        void DrawRenderingModeControls();
        void DrawPostProcessingControls();
        void DrawShadowMappingControls();
        void DrawPerformanceMetrics();
        void DrawLightingControls();
        void DrawShaderControls();
        void DrawGBufferViewer();

        // Helper methods
        void DrawTooltip(const char* description);
        bool DrawFloatSlider(const char* label, float* value, float min, float max, const char* tooltip = nullptr);
        bool DrawToggleButton(const char* label, bool* value, const char* tooltip = nullptr);
    };
}
