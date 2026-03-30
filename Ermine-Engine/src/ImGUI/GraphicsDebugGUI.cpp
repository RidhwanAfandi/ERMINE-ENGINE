/* Start Header ************************************************************************/
/*!
\file       GraphicsDebugGUI.cpp
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       31/01/2026
\brief      This file contains the implementation of the GraphicsDebugGUI class.
            A debug GUI for graphics-related parameters and controls using ImGui.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "GraphicsDebugGUI.h"
#include "imgui.h"
#include "ECS.h"
#include "Renderer.h"
#include "GPUProfiler.h"
#include "Logger.h"
#include "shadow_config.h"
#include "Material.h"
#include "FrameController.h"
#include "AssetManager.h"
#include "GISystem.h"
#include <array>
#include <cstdio>

using namespace Ermine::editor;
using namespace Ermine::graphics;

// Helper function for formatting numbers
namespace
{
    /**
     * @brief Formats a large integer value into a human-readable string with units (K, M, B, T).
     * @param value The value to format.
     * @return Formatted string.
     */
    std::string FormatNumber(uint64_t value)
    {
        struct Unit { uint64_t base; const char* suffix; };
        static constexpr Unit units[] = {
            {.base= 1'000'000'000'000ULL, .suffix= "T"},
            {.base= 1'000'000'000ULL, .suffix= "B"},
            {.base= 1'000'000ULL, .suffix= "M"},
            {.base= 1'000ULL, .suffix= "K"},
            {.base= 1, .suffix= ""}
        };

        for (const auto& u : units)
        {
            if (value >= u.base)
            {
                char buffer[32];
                const double scaled = static_cast<double>(value) / static_cast<double>(u.base);
                const int written = snprintf(buffer, sizeof(buffer), "%.1f%s", scaled, u.suffix);
                if (written < 0)
                {
                    EE_CORE_WARN("FormatNumber error occurred");
                    return std::to_string(value);
                }
                return std::string(buffer);
            }
        }

        char buffer[32];
        const int written = snprintf(buffer, sizeof(buffer), "%llu", value);
        if (written < 0)
        {
            EE_CORE_WARN("FormatNumber error occurred");
            return std::to_string(value);
        }
        return std::string(buffer);
    }
}

/**
 * @brief Constructs a GraphicsDebugGUI window with the given title.
 * @param title The window title.
 */
GraphicsDebugGUI::GraphicsDebugGUI(const std::string& title)
    : ImGUIWindow(title), m_title(title)
{
}

/**
 * @brief Updates the debug GUI, rendering all graphics-related controls and metrics.
 */
void GraphicsDebugGUI::Update()
{
    auto renderer = ECS::GetInstance().GetSystem<Renderer>();
    if (!renderer) {
        ImGui::Begin(m_title.c_str());
        ImGui::Text("Renderer system not available");
        ImGui::End();
    }
}

/**
 * @brief Renders the GUI window. All rendering is handled in Update().
 */
void GraphicsDebugGUI::Render()
{
    // Return if window is closed
    if (!IsOpen()) return;

    if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
    {
        ImGui::End();
        return;
    }

    // Create collapsible sections for organized UI
    DrawRenderingModeControls();
    DrawPostProcessingControls();
    DrawShadowMappingControls();
    DrawLightingControls();
    DrawPerformanceMetrics();
    DrawShaderControls();
    DrawGBufferViewer();

    ImGui::End();
}

/**
 * @brief Draws controls for rendering mode selection and SSAO toggle.
 */
void GraphicsDebugGUI::DrawRenderingModeControls()
{
    auto renderer = ECS::GetInstance().GetSystem<Renderer>();
    
    if (ImGui::CollapsingHeader("Rendering Pipeline", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent(10.0f);
        
        // Shading Model Toggle
        bool isBlinnPhong = renderer->GetShadingMode();
        if (ImGui::RadioButton("PBR Shading", !isBlinnPhong)) {
            renderer->SetShadingMode(false);
            EE_CORE_INFO("Switched to PBR shading");
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Blinn-Phong", isBlinnPhong)) {
            renderer->SetShadingMode(true);
            EE_CORE_INFO("Switched to Blinn-Phong shading");
        }
        DrawTooltip("Choose between Physically Based Rendering (PBR) and classic Blinn-Phong shading");
        
        ImGui::Separator();
        
        // SSAO Toggle
        if (DrawToggleButton("Screen Space Ambient Occlusion", &renderer->m_SSAOEnabled, 
                            "Enable/disable Screen Space Ambient Occlusion for enhanced depth perception")) {
            EE_CORE_INFO("SSAO {}", renderer->m_SSAOEnabled ? "enabled" : "disabled");
        }

        if (DrawToggleButton("Show Skybox", &renderer->m_ShowSkybox,
                            "Toggle skybox rendering on/off")) {
            EE_CORE_INFO("Skybox {}", renderer->m_ShowSkybox ? "enabled" : "disabled");
        }
        
        // SSAO Parameters (shown when SSAO is enabled)
        if (renderer->m_SSAOEnabled && ImGui::TreeNode("SSAO Settings"))
        {
            if (ImGui::SliderInt("Sample Count", &renderer->m_SSAOSamples, 4, 64)) {
                EE_CORE_INFO("SSAO Samples changed to {}", renderer->m_SSAOSamples);
            }
            DrawTooltip("Number of samples for SSAO calculation (higher = better quality but slower)");
            
            DrawFloatSlider("Sampling Radius", &renderer->m_SSAORadius, 0.1f, 50.0f, 
                           "Radius of the sampling hemisphere in world space");
            
            DrawFloatSlider("Bias", &renderer->m_SSAOBias, 0.0f, 0.1f, 
                           "Bias to prevent self-shadowing artifacts");
            
            DrawFloatSlider("Intensity", &renderer->m_SSAOIntensity, 0.0f, 5.0f, 
                           "Strength of the ambient occlusion effect");
            
            DrawFloatSlider("Fadeout Distance", &renderer->m_SSAOFadeout, 0.0f, 1.0f, 
                           "Distance factor for fading out SSAO effect");
            
            DrawFloatSlider("Max Distance", &renderer->m_SSAOMaxDistance, 10.0f, 1000.0f, 
                           "Maximum distance for SSAO calculation");
            
            ImGui::TreePop();
        }

        ImGui::Separator();

        // Ambient Lighting Toggle and Controls
        ImGui::Text("Global Ambient Lighting");
        
        // Color picker for ambient light
        if (ImGui::ColorEdit3("Ambient Color", &renderer->m_AmbientColor.r)) {
            EE_CORE_INFO("Ambient color changed to ({:.2f}, {:.2f}, {:.2f})",
                       renderer->m_AmbientColor.r, renderer->m_AmbientColor.g, renderer->m_AmbientColor.b);
        }
        DrawTooltip("RGB color of the global ambient lighting");

        // Intensity slider
        DrawFloatSlider("Ambient Intensity", &renderer->m_AmbientIntensity, 0.0f, 1.0f,
                       "Brightness of ambient lighting (0=black, 1=full brightness)");

        ImGui::Separator();

        // Fog Toggle
        if (DrawToggleButton("Distance-Based Fog", &renderer->m_FogEnabled,
                            "Enable/disable atmospheric fog based on distance from camera")) {
            EE_CORE_INFO("Fog {}", renderer->m_FogEnabled ? "enabled" : "disabled");
        }

        // Fog Parameters (shown when Fog is enabled)
        if (renderer->m_FogEnabled && ImGui::TreeNode("Fog Settings"))
        {
            const char* fogModes[] = { "Linear", "Exponential", "Exponential Squared" };
            if (ImGui::Combo("Fog Mode", &renderer->m_FogMode, fogModes, 3)) {
                EE_CORE_INFO("Fog mode changed to {}", fogModes[renderer->m_FogMode]);
            }
            DrawTooltip("Linear = smooth fade between start/end\nExponential = natural fog falloff\nExp² = most realistic atmospheric fog");

            // Fog color picker
            if (ImGui::ColorEdit3("Fog Color", &renderer->m_FogColor.r)) {
                EE_CORE_INFO("Fog color changed to ({:.2f}, {:.2f}, {:.2f})",
                           renderer->m_FogColor.r, renderer->m_FogColor.g, renderer->m_FogColor.b);
            }
            DrawTooltip("RGB color of the fog");

            // Linear fog parameters
            if (renderer->m_FogMode == 0) {
                DrawFloatSlider("Fog Start", &renderer->m_FogStart, 0.0f, 500.0f,
                               "Distance where fog begins to appear (linear mode)");
                DrawFloatSlider("Fog End", &renderer->m_FogEnd, 1.0f, 1000.0f,
                               "Distance where fog is fully opaque (linear mode)");
            }
            // Exponential fog parameters
            else {
                DrawFloatSlider("Fog Density", &renderer->m_FogDensity, 0.0f, 0.1f,
                               "Fog density for exponential modes (lower = less dense)");
            }

            ImGui::Separator();

            // Height-based fog parameters
            ImGui::Text("Height-Based Fog");
            DrawFloatSlider("Height Influence", &renderer->m_FogHeightCoefficient, 0.0f, 1.0f,
                "How much height affects fog density (0=disabled, 1=maximum effect)");
            DrawFloatSlider("Height Falloff", &renderer->m_FogHeightFalloff, 1.0f, 100.0f,
                "Height at which fog starts to thin out (lower=fog stays near ground)");

            ImGui::TreePop();
        }

        ImGui::Unindent(10.0f);
    }
}

/**
 * @brief Draws controls for post-processing effects and their parameters.
 */
void GraphicsDebugGUI::DrawPostProcessingControls()
{
    auto renderer = ECS::GetInstance().GetSystem<Renderer>();
    
    if (ImGui::CollapsingHeader("Post-Processing Effects"))
    {
        ImGui::Indent(10.0f);
        
        // Post-processing toggles
        DrawToggleButton("HDR Tone Mapping", &renderer->m_ToneMappingEnabled, 
                        "Enable High Dynamic Range tone mapping for better exposure control");
        DrawToggleButton("Gamma Correction", &renderer->m_GammaCorrectionEnabled, 
                        "Apply gamma correction for proper color space conversion");
        DrawToggleButton("FXAA Anti-Aliasing", &renderer->m_FXAAEnabled, 
                        "Fast Approximate Anti-Aliasing to reduce jagged edges");
        DrawToggleButton("Bloom Effect", &renderer->m_BloomEnabled, 
                        "Bloom effect for bright light sources");
        DrawToggleButton("Vignette Effect", &renderer->m_VignetteEnabled,
                        "Vignette darkening at screen borders");
        DrawToggleButton("Film Grain", &renderer->m_FilmGrainEnabled,
                        "Add film grain noise effect");
        DrawToggleButton("Chromatic Aberration", &renderer->m_ChromaticAberrationEnabled,
                        "Color fringing effect at screen edges");
        DrawToggleButton("Radial Blur", &renderer->m_RadialBlurEnabled,
                        "Screen-space zoom blur from a configurable center");
        DrawToggleButton("Motion Blur", &renderer->m_MotionBlurEnabled,
                        "Per-pixel velocity-based motion blur (camera-attached objects are never blurred)");

        if (renderer->m_MotionBlurEnabled && ImGui::TreeNode("Motion Blur Settings"))
        {
            DrawFloatSlider("Strength", &renderer->m_MotionBlurStrength, 0.0f, 5.0f,
                           "Scale applied to the velocity vector");
            ImGui::SliderInt("Samples", &renderer->m_MotionBlurSamples, 2, 24);
            DrawTooltip("Number of samples along the velocity vector (higher = smoother but slower)");
            ImGui::TreePop();
        }

        ImGui::Separator();
        
        // Exposure and Color Controls
        if (ImGui::TreeNode("Color & Exposure"))
        {
            DrawFloatSlider("Exposure", &renderer->m_Exposure, 0.1f, 5.0f, 
                           "Controls overall scene brightness");
            DrawFloatSlider("Contrast", &renderer->m_Contrast, 0.5f, 2.0f, 
                           "Adjust contrast between light and dark areas");
            DrawFloatSlider("Saturation", &renderer->m_Saturation, 0.0f, 2.0f, 
                           "Color saturation intensity");
            DrawFloatSlider("Gamma", &renderer->m_Gamma, 1.0f, 3.0f, 
                           "Gamma curve for color correction");
            ImGui::TreePop();
        }
        
        // Bloom Controls
        if (ImGui::TreeNode("Bloom Settings"))
        {
            DrawFloatSlider("Bloom Threshold", &renderer->m_BloomThreshold, 0.1f, 3.0f, 
                           "Brightness threshold for bloom effect");
            DrawFloatSlider("Bloom Strength", &renderer->m_BloomStrength, 0.0f, 1.0f, 
                           "Intensity of bloom effect");
            DrawFloatSlider("Bloom Radius", &renderer->m_BloomRadius, 1.0f, 10.0f, 
                           "Blur radius for bloom effect");
            ImGui::TreePop();
        }
        
        // Vignette Controls
        if (ImGui::TreeNode("Vignette Settings"))
        {
            DrawFloatSlider("Vignette Intensity", &renderer->m_VignetteIntensity, 0.0f, 1.0f,
                           "Strength of vignette darkening");
            DrawFloatSlider("Vignette Radius", &renderer->m_VignetteRadius, 0.1f, 1.0f,
                           "Size of vignette effect");
            DrawFloatSlider("Vignette Coverage", &renderer->m_VignetteCoverage, 0.0f, 1.0f,
                           "Coverage across entire screen (1.0 affects full frame)");
            DrawFloatSlider("Vignette Falloff", &renderer->m_VignetteFalloff, 0.01f, 1.0f,
                           "Edge transition softness for vignette");
            DrawFloatSlider("Vignette Map Strength", &renderer->m_VignetteMapStrength, 0.0f, 1.0f,
                           "How much the vignette map influences the mask");
            ImGui::SliderFloat3("Vignette Map RGB Modifier", &renderer->m_VignetteMapRGBModifier.x, 0.0f, 2.0f, "%.3f");
            DrawTooltip("Vignette tint color (0,0,0 = classic black vignette)");

            static std::array<char, 512> vignetteMapPathBuffer{};
            static bool vignetteMapPathInitialized = false;
            if (!vignetteMapPathInitialized)
            {
                std::snprintf(vignetteMapPathBuffer.data(), vignetteMapPathBuffer.size(), "%s",
                    renderer->m_VignetteMapPath.c_str());
                vignetteMapPathInitialized = true;
            }

            if (ImGui::InputText("Vignette Map Path", vignetteMapPathBuffer.data(), vignetteMapPathBuffer.size()))
            {
                renderer->m_VignetteMapPath = vignetteMapPathBuffer.data();
            }
            DrawTooltip("Texture path for optional vignette mask map");

            if (ImGui::Button("Load Vignette Map"))
            {
                renderer->m_VignetteMapPath = vignetteMapPathBuffer.data();
                if (!renderer->m_VignetteMapPath.empty())
                {
                    auto tex = AssetManager::GetInstance().LoadTexture(renderer->m_VignetteMapPath);
                    if (tex && tex->IsValid())
                    {
                        renderer->SetVignetteMapTexture(tex, renderer->m_VignetteMapPath);
                    }
                    else
                    {
                        renderer->SetVignetteMapTexture(nullptr, renderer->m_VignetteMapPath);
                        EE_CORE_WARN("Failed to load vignette map texture: {}", renderer->m_VignetteMapPath);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Vignette Map"))
            {
                renderer->ClearVignetteMapTexture();
                vignetteMapPathBuffer[0] = '\0';
            }

            const bool hasVignetteMap = renderer->HasVignetteMapTexture();
            ImGui::Text("Vignette Map: %s", hasVignetteMap ? "Loaded" : "None");
            ImGui::TreePop();
        }

        // Film Grain Controls
        if (ImGui::TreeNode("Film Grain Settings"))
        {
            DrawFloatSlider("Grain Intensity", &renderer->m_GrainIntensity, 0.0f, 0.1f,
                           "Strength of film grain noise");
            DrawFloatSlider("Grain Scale", &renderer->m_GrainScale, 0.5f, 5.0f,
                           "Scale of film grain pattern");
            ImGui::TreePop();
        }

        // Chromatic Aberration Controls
        if (ImGui::TreeNode("Chromatic Aberration Settings"))
        {
            DrawFloatSlider("Aberration Amount", &renderer->m_ChromaticAmount, 0.0f, 0.02f,
                           "Strength of color separation at edges");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Radial Blur Settings"))
        {
            DrawFloatSlider("Radial Blur Strength", &renderer->m_RadialBlurStrength, 0.0f, 0.35f,
                           "Zoom blur intensity");
            ImGui::SliderInt("Radial Blur Samples", &renderer->m_RadialBlurSamples, 4, 24);
            DrawTooltip("Sample count for radial blur quality/performance");
            ImGui::SliderFloat("Radial Blur Center X", &renderer->m_RadialBlurCenter.x, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Radial Blur Center Y", &renderer->m_RadialBlurCenter.y, 0.0f, 1.0f, "%.3f");
            DrawTooltip("Screen-space blur center in UV space");
            ImGui::TreePop();
        }

        // FXAA Controls
        if (ImGui::TreeNode("FXAA Settings"))
        {
            DrawFloatSlider("FXAA Span Max", &renderer->m_FXAASpanMax, 2.0f, 16.0f,
                           "Maximum search span for edge detection");
            DrawFloatSlider("FXAA Reduce Min", &renderer->m_FXAAReduceMin, 1.0f/256.0f, 1.0f/32.0f,
                           "Minimum luminance reduction threshold");
            DrawFloatSlider("FXAA Reduce Mul", &renderer->m_FXAAReduceMul, 1.0f/16.0f, 1.0f/4.0f,
                           "Luminance reduction multiplier");
            ImGui::TreePop();
        }

        ImGui::Separator();

        ImGui::Unindent(10.0f);
    }
}

/**
 * @brief Draws controls and statistics for shadow mapping configuration.
 */
void GraphicsDebugGUI::DrawShadowMappingControls()
{
    if (ImGui::CollapsingHeader("Shadow Mapping"))
    {
        ImGui::Indent(10.0f);
        
        ImGui::Text("Shadow Map Resolution: %d x %d", SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION);
        DrawTooltip("Current shadow map resolution - defined in shadow_config.h");
        
        ImGui::Text("Max Shadow Layers: %u", SHADOW_MAX_LAYERS);
        DrawTooltip("Maximum number of shadow map layers available");
        
        ImGui::Text("Cascade Count: %d", NUM_CASCADES);
        DrawTooltip("Number of cascades for Cascaded Shadow Maps (CSM)");
        
        ImGui::Text("CSM Lambda: %.2f", SHADOW_MAP_ARRAY_LAMBDA);
        DrawTooltip("Blend factor between logarithmic and linear cascade distribution");
        
        ImGui::Text("Refresh Interval: %d frames", SHADOW_MAP_REFRESH_INTERVAL_IN_FRAMES);
        DrawTooltip("Shadow maps are refreshed every N frames for performance");
        
        if (ImGui::Button("Force Shadow Refresh")) {
            EE_CORE_INFO("Manual shadow map refresh triggered");
        }
        DrawTooltip("Force refresh all shadow maps (normally updated every few frames)");
        
        ImGui::Unindent(10.0f);
    }
}

/**
 * @brief Draws controls and statistics for the lighting system.
 */
void GraphicsDebugGUI::DrawLightingControls()
{
    if (ImGui::CollapsingHeader("Lighting System"))
    {
        ImGui::Indent(10.0f);
        
        auto renderer = ECS::GetInstance().GetSystem<Renderer>();
        const int uploadedLights = renderer ? static_cast<int>(renderer->GetUploadedLightCount()) : 0;
        const int lightCapacity = renderer ? static_cast<int>(renderer->GetLightSSBOCapacity()) : 0;
        ImGui::Text("Uploaded Lights: %d", uploadedLights);
        DrawTooltip("Number of lights uploaded to the light SSBO for the current frame");
        ImGui::Text("Light SSBO Capacity: %d", lightCapacity);
        DrawTooltip("Current light SSBO capacity in light entries before a resize is required");
        
        // Count active lights
        auto lightSystem = ECS::GetInstance().GetSystem<LightSystem>();
        int activeLights = lightSystem ? static_cast<int>(lightSystem->m_Entities.size()) : 0;
        ImGui::Text("Active Lights: %d", activeLights);
        
        // Show light distribution
        if (lightSystem && activeLights > 0) {
            const auto& ecs = ECS::GetInstance();
            int directionalLights = 0;
            int pointLights = 0;
            int spotLights = 0;
            int shadowCasters = 0;
            
            for (EntityID entity : lightSystem->m_Entities) {
                if (ecs.HasComponent<Light>(entity)) {
                    auto& light = ecs.GetComponent<Light>(entity);
                    switch (light.type) {
                        case LightType::DIRECTIONAL: directionalLights++; break;
                        case LightType::POINT: pointLights++; break;
                        case LightType::SPOT: spotLights++; break;
                    }
                    if (light.castsShadows) shadowCasters++;
                }
            }
            
            ImGui::Text("  Directional: %d", directionalLights);
            ImGui::Text("  Point: %d", pointLights);
            ImGui::Text("  Spot: %d", spotLights);
            ImGui::Text("  Shadow Casters: %d", shadowCasters);
        }

        ImGui::Separator();

        // Volumetric Spotlight Rays
        if (renderer) {
            if (DrawToggleButton("Volumetric Spotlight Rays", &renderer->m_SpotlightRaysEnabled,
                                "Enable volumetric god rays for spotlights")) {
                EE_CORE_INFO("Spotlight rays {}", renderer->m_SpotlightRaysEnabled ? "enabled" : "disabled");
            }

            // Spotlight ray parameters (shown when enabled)
            if (renderer->m_SpotlightRaysEnabled && ImGui::TreeNode("Spotlight Ray Settings"))
            {
                DrawFloatSlider("Ray Intensity", &renderer->m_SpotlightRayIntensity, 0.0f, 2.0f,
                               "Brightness of volumetric god rays from spotlights");
                DrawFloatSlider("Ray Falloff", &renderer->m_SpotlightRayFalloff, 0.5f, 5.0f,
                               "How quickly rays fade with distance");

                ImGui::TreePop();
            }
        }

        ImGui::Separator();

        // Light probe volume controls
        if (ImGui::TreeNode("Light Probe Volumes"))
        {
            auto giSystem = ECS::GetInstance().GetSystem<GISystem>();
            int probeCount = giSystem ? static_cast<int>(giSystem->GetProbeEntities().size()) : 0;
            ImGui::Text("Active Volumes: %d", probeCount);

            DrawTooltip("Captures indirect lighting into each probe");

            ImGui::TreePop();
        }

        ImGui::Unindent(10.0f);
    }
}

/**
 * @brief Draws performance metrics including frame timing, draw calls, and memory usage.
 */
void GraphicsDebugGUI::DrawPerformanceMetrics()
{
    auto renderer = ECS::GetInstance().GetSystem<Renderer>();

    if (ImGui::CollapsingHeader("Performance Metrics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent(10.0f);

        const auto& metrics = GPUProfiler::GetMetrics();
        
        // Frame timing
        float avgFps = metrics.averageFrameTimeMs > 0.0f ? 1000.0f / metrics.averageFrameTimeMs : 0.0f;
        ImGui::Text("FPS: %.1f (avg: %.1f)", metrics.fps, avgFps);
        ImGui::Text("Frame Time: %.2f ms", metrics.frameTimeMs);
        ImGui::Text("CPU Time: %.2f ms", metrics.cpuFrameTimeMs);
        ImGui::Text("GPU Time: %.2f ms", metrics.gpuFrameTimeMs);

		ImGui::Separator();

        // Render statistics
        ImGui::Text("Draw Calls: %u", metrics.drawCallCount);
        ImGui::Text("Triangles: %s", FormatNumber(metrics.triangleCount).c_str());
        ImGui::Text("Vertices: %s", FormatNumber(metrics.vertexCount).c_str());
        ImGui::Text("Meshes Culled: %u", metrics.culledMeshes);

        ImGui::Separator();

        // Debug Visualization Controls
        if (renderer) {
            ImGui::Text("Debug Visualization:");

            if (DrawToggleButton("Show AABBs", &renderer->m_DebugDrawAABBs,
                                "Draw bounding boxes for all meshes (Green = visible, Red = culled)")) {
                EE_CORE_INFO("AABB visualization {}", renderer->m_DebugDrawAABBs ? "enabled" : "disabled");
            }

            if (DrawToggleButton("Show Frustum", &renderer->m_DebugDrawFrustum,
                                "Draw camera frustum planes (Cyan)")) {
                EE_CORE_INFO("Frustum visualization {}", renderer->m_DebugDrawFrustum ? "enabled" : "disabled");
            }

            ImGui::Separator();

            // Draw Data Rebuild Control
            if (ImGui::Button("Force Draw Data Rebuild")) {
                renderer->ForceDrawDataRebuild();
                EE_CORE_INFO("Draw data rebuild triggered manually");
            }
            DrawTooltip("Force a full rebuild of draw commands and shadow buffers on the next frame");
        }

        ImGui::Separator();
        
        // Memory usage
        ImGui::Text("VRAM Usage: %llu MB", metrics.totalVRAMUsageMB);
        ImGui::Text("Texture Memory: %llu MB", metrics.textureMemoryMB);
        ImGui::Text("Buffer Memory: %llu MB", metrics.bufferMemoryMB);
        
        // Frame time history graph
        const auto& history = GPUProfiler::GetFrameTimeHistory();
        if (!history.empty()) {
            std::vector<float> values(history.begin(), history.end());
            ImGui::PlotLines("Frame Times (ms)", values.data(), static_cast<int>(values.size()),
                0, nullptr, 0.0f, metrics.maxFrameTimeMs * 1.2f, ImVec2(0, 80));
        }
        
        ImGui::Unindent(10.0f);
    }
}

/**
 * @brief Shows a tooltip for the last hovered ImGui item.
 * @param description The tooltip text.
 */
void GraphicsDebugGUI::DrawTooltip(const char* description)
{
    if (ImGui::IsItemHovered() && description) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

/**
 * @brief Draws a float slider with a label and optional tooltip.
 * @param label The slider label.
 * @param value Pointer to the float value.
 * @param min Minimum slider value.
 * @param max Maximum slider value.
 * @param tooltip Optional tooltip text.
 * @return true if the value was changed.
 */
bool GraphicsDebugGUI::DrawFloatSlider(const char* label, float* value, float min, float max, const char* tooltip)
{
    bool changed = ImGui::SliderFloat(label, value, min, max, "%.3f");
    if (tooltip) DrawTooltip(tooltip);
    return changed;
}

/**
 * @brief Draws a toggle button (checkbox) with a label and optional tooltip.
 * @param label The checkbox label.
 * @param value Pointer to the boolean value.
 * @return true if the value was changed.
 * @param tooltip Optional tooltip text.
 */
bool GraphicsDebugGUI::DrawToggleButton(const char* label, bool* value, const char* tooltip)
{
    bool changed = ImGui::Checkbox(label, value);
    if (tooltip) DrawTooltip(tooltip);
    return changed;
}

void GraphicsDebugGUI::DrawShaderControls()
{
    if (!ImGui::CollapsingHeader("Shader Tools", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    ImGui::Indent(10.0f);
    if (ImGui::Button("Recompile Shaders"))
    {
        AssetManager::GetInstance().ReloadCachedShaders();
    }
    DrawTooltip("Recompile all cached shaders from disk");
    ImGui::Unindent(10.0f);
}

void GraphicsDebugGUI::DrawGBufferViewer()
{
    if (!ImGui::CollapsingHeader("GBuffer Viewer"))
        return;

    auto renderer = ECS::GetInstance().GetSystem<Renderer>();
    if (!renderer) return;

    auto gbuffer    = renderer->GetGBuffer();
    auto ppBuffer   = renderer->GetPostProcessBuffer();
    auto mbBuffer   = renderer->GetMotionBlurBuffer();

    // Thumbnail size: 2 columns filling available width
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    const float thumbW = (panelWidth - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    const float thumbH = thumbW * (9.0f / 16.0f);  // 16:9 aspect

    // Helper lambda: draw one texture thumbnail with a label
    // ImGui::Image expects UV (0,1)→(1,0) to flip OpenGL bottom-left origin to top-left
    auto DrawThumb = [&](const char* label, unsigned int texID)
    {
        ImGui::BeginGroup();
        ImGui::TextUnformatted(label);
        if (texID != 0)
            ImGui::Image((ImTextureID)(intptr_t)texID, ImVec2(thumbW, thumbH), ImVec2(0, 1), ImVec2(1, 0));
        else
            ImGui::Dummy(ImVec2(thumbW, thumbH));
        ImGui::EndGroup();
    };

    ImGui::Indent(4.0f);

    // Row 1
    if (gbuffer)
    {
        DrawThumb("RT0: Albedo (RGBA8)",      gbuffer->PackedTexture0);
        ImGui::SameLine();
        DrawThumb("RT1: Normals (RG16F oct)", gbuffer->PackedTexture1);

        DrawThumb("RT2: Emissive (RGBA8)",    gbuffer->PackedTexture2);
        ImGui::SameLine();
        DrawThumb("RT3: Mat Props (RGBA8)",   gbuffer->PackedTexture3);

        DrawThumb("RT4: Velocity (RG16F)",    gbuffer->PackedTexture4);
        ImGui::SameLine();
        DrawThumb("Depth (32F)",              gbuffer->DepthTexture);
    }

    // Row 4: pipeline outputs
    DrawThumb("Post-Process Output",
        ppBuffer ? ppBuffer->ColorTexture : 0u);
    ImGui::SameLine();
    DrawThumb("Motion Blur Output",
        mbBuffer ? mbBuffer->ColorTexture : 0u);

    ImGui::Unindent(4.0f);
}
