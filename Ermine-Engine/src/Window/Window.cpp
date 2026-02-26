/* Start Header ************************************************************************/
/*!
\file       Window.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\co-author  WEE HUNG RU, Curtis, h.wee, 230xxx, h.wee\@digipen.edu (25%)
\date       31/01/2026
\brief      This file contains the definition of the Window system.
            This file is used to create a window using GLFW.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Window.h"

#include "Serialisation.h"
#include "glad/glad.h"
#include "AssetBrowser.h" // For forwarding dropped files to the asset browser
#include "EditorGUI.h"
#include "SettingsGUI.h"
#include "UIButtonSystem.h"

#if defined(_WIN32)
	#define GLFW_EXPOSE_NATIVE_WIN32
	#include <GLFW/glfw3native.h>
#endif

Ermine::Window::CursorLockState Ermine::Window::s_cursorLockState = Ermine::Window::CursorLockState::None;
bool Ermine::Window::s_visibleCursor = true;
GLFWwindow* Ermine::Window::s_window = nullptr;

namespace
{
#ifdef _WIN32
	void ConfineCursorToGLFWWindow(GLFWwindow* window, bool confine) noexcept
    {
        if (!window)
            return;

        if (!confine)
        {
	        ClipCursor(nullptr);
	        return;
        }

		const HWND hwnd = glfwGetWin32Window(window);
        if (!hwnd)
            return;

        RECT clientRect{};
        if (!GetClientRect(hwnd, &clientRect))
            return;

        POINT tl{ clientRect.left, clientRect.top };
        POINT br{ clientRect.right, clientRect.bottom };

        if (!ClientToScreen(hwnd, &tl) || !ClientToScreen(hwnd, &br))
            return;

        RECT clipRect{};
        clipRect.left = tl.x;
        clipRect.top = tl.y;
        clipRect.right = br.x;
        clipRect.bottom = br.y;

        ClipCursor(&clipRect);
    }

    void RefreshCursorConfinement(GLFWwindow* window) noexcept
    {
        // Re-apply clipping based on current state. Safe to call often.
        if (!window)
            return;

        if (Ermine::Window::GetCursorLockState() == Ermine::Window::CursorLockState::Confined)
            ConfineCursorToGLFWWindow(window, true);
        else
            ConfineCursorToGLFWWindow(window, false);
    }
#endif

}

static bool s_pauseOnFocusLoss = true; // Enable by default

void Ermine::Window::SetPausedOnFocusLoss(bool enabled) {
    s_pauseOnFocusLoss = enabled;
}

bool Ermine::Window::IsPausedOnFocusLoss() {  
    return s_pauseOnFocusLoss;
}

/**
 * @brief GLFW callback function for handling file drops.
 * This function is registered with GLFW to receive notifications
 * when files are dropped onto the application window. It collects
 * the file paths and forwards them to the asset browser for processing.
 * @param window Pointer to the GLFW window where files were dropped.
 * @param count Number of files dropped.
 * @param paths Array of C-style strings representing the dropped file paths.
 */
static void GLFW_DropCallback([[maybe_unused]] GLFWwindow* window, int count, const char** paths)
{
    // Collect dropped file paths into a vector of strings
    std::vector<std::string> droppedFiles;
    droppedFiles.reserve(count);

    // Copy paths to the vector
    for (int i = 0; i < count; ++i)
        droppedFiles.emplace_back(paths[i]);

    // Log the dropped files and forward them to the asset browser
    EE_CORE_INFO("Dropped {} files into the editor window.", count);
    Ermine::ImguiUI::AssetBrowser::OnExternalFilesDropped(droppedFiles);
}

/**
 * @brief Initialize the window, You can find openGL (MSAA, V-Sync) settings here
 * 
 * @param width The width of the window
 * @param height The height of the window
 * @param title The title of the window
 * @return GLFWwindow* The window pointer
 */
GLFWwindow* Ermine::Window::InitWindow(int width, int height, const char* title)
{
    EE_CORE_TRACE("Initializing Window...");

    const std::filesystem::path cfgPath = "Ermine-Engine.config";

    Config cfg{};
    try {
        cfg = LoadConfigFromFile(cfgPath);
        EE_CORE_INFO("Loaded config: {0}x{1}, fullscreen={2}, maximised={3}, title={4}, fontsize={5}, baseFontSize{6}, themeMode{7}",
            cfg.windowWidth, cfg.windowHeight, cfg.fullscreen, cfg.maximized, cfg.title, cfg.fontSize, cfg.baseFontSize, cfg.themeMode);
    }
    catch (const std::exception& e) {
        EE_CORE_WARN("Config not found/invalid ({}). Using defaults.", e.what());
        cfg = { .windowWidth = width, .windowHeight = height, .fullscreen = false, .maximized = false, .title = title };

#ifdef EE_RELEASE
        cfg.fullscreen = true; // Force fullscreen on release build
        cfg.maximized = false; // Prevent maximize instead of fullscreen
#endif

        try { SaveConfigToFile(cfg, cfgPath, /*pretty=*/true); }
        catch (const std::exception& w) { EE_CORE_WARN("Could not write default config: {}", w.what()); }
    }

    window_width = cfg.windowWidth;
    window_height = cfg.windowHeight;
    ImGUIWindow::SetAllWindowStates(cfg.imguiWindows);
	SettingsGUI::SetFontSize(cfg.fontSize, cfg.baseFontSize); // call this after ImGui is initialized
	SettingsGUI::SetMode(cfg.themeMode);

    glfwSetErrorCallback([]([[maybe_unused]] int error , const char* description) { EE_CORE_ERROR("GLFW Error: {0}", description); });
    
    if (!glfwInit())
    {
        EE_CORE_ERROR("Failed to initialize GLFW");
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // MSAA
    glfwWindowHint(GLFW_SAMPLES, 4);

    // borderless full screen
    // GLFWmonitor *mon = NULL;
    // mon = glfwGetPrimaryMonitor();
    //
    // const GLFWvidmode* mode = glfwGetVideoMode( mon );
    // // Hinting these properties lets us use "borderless full screen" mode.
    // glfwWindowHint( GLFW_RED_BITS, mode->redBits );
    // glfwWindowHint( GLFW_GREEN_BITS, mode->greenBits );
    // glfwWindowHint( GLFW_BLUE_BITS, mode->blueBits );
    // glfwWindowHint( GLFW_REFRESH_RATE, mode->refreshRate );
    // GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, title, nullptr, nullptr);

    GLFWwindow* window = glfwCreateWindow(cfg.windowWidth, cfg.windowHeight, cfg.title.c_str(), nullptr, nullptr);
    if (!window)
    {
        EE_CORE_ERROR("Failed to create window");
        glfwTerminate();
        return nullptr;
    }

    if (cfg.fullscreen)
    {
	    GLFWmonitor* mon = glfwGetPrimaryMonitor();
	    const GLFWvidmode* mode = glfwGetVideoMode(mon);
	    glfwSetWindowMonitor(window, mon, 0, 0,
	        mode->width, mode->height,
	        mode->refreshRate);
    }
    else 
    {
        glfwSetWindowSize(window, cfg.windowWidth, cfg.windowHeight);

        if (cfg.maximized) {
            glfwMaximizeWindow(window);
        }
        else {
            glfwRestoreWindow(window);
        }
    }

    glfwMakeContextCurrent(window);

    // Set the drop callback to handle file drops
    glfwSetDropCallback(window, GLFW_DropCallback);

#ifdef _WIN32
    glfwSetWindowSizeCallback(window, [](GLFWwindow* w, [[maybe_unused]] int width, [[maybe_unused]] int height)
        {
            RefreshCursorConfinement(w);
        });

    glfwSetWindowFocusCallback(window, [](GLFWwindow* w, int focused)
        {
            if (!focused)
            {
                // Always release cursor confinement on focus loss
#ifdef _WIN32
                ConfineCursorToGLFWWindow(w, false);
#endif

                // Pause the game if enabled (works in both editor and standalone)
                if (Ermine::Window::IsPausedOnFocusLoss())
                {
#if defined(EE_EDITOR)
                    // In editor, only pause if actively playing
                    if (editor::EditorGUI::isPlaying)
                    {
                        editor::EditorGUI::s_state = editor::EditorGUI::SimState::paused;
                        EE_CORE_INFO("Game paused (window lost focus)");
                        UIButtonSystem::ShowPauseMenuOnAltTab();
                    }
#else
                    // In standalone build, always pause
                    editor::EditorGUI::s_state = editor::EditorGUI::SimState::paused;
                    EE_CORE_INFO("Game paused (window lost focus)");
                    UIButtonSystem::ShowPauseMenuOnAltTab();
#endif
                }

                return;
            }

            // On focus gain, re-apply cursor confinement if needed
#ifdef _WIN32
            RefreshCursorConfinement(w);
#endif

            if (Ermine::Window::IsPausedOnFocusLoss())
            {
                UIButtonSystem::TryAutoResumeOnAltTab();
            }
        });

    glfwSetWindowIconifyCallback(window, [](GLFWwindow* w, int iconified)
        {
            if (iconified)
            {
                ConfineCursorToGLFWWindow(w, false);
                return;
            }

            RefreshCursorConfinement(w);
        });
#endif


    // We'll like to initialize GLAD as well...
    if (!gladLoadGL())
    {
        EE_CORE_ERROR("Failed to initialize GLAD!");
        glfwTerminate(); // GLAD failed, shouldn't continue to run
        return nullptr;
    }

    glViewport(0,0,cfg.windowWidth,cfg.windowHeight);
    
    glfwSwapInterval(1); // Enable V-Sync

    glEnable(GL_DEPTH_TEST);

    std::string glRenderer = std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    std::string glVersion = std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    EE_CORE_TRACE("Renderer: {0}", glRenderer);
    EE_CORE_TRACE("OpenGL version supported {0}", glVersion);
    s_window = window;
    return window;
}

/**
 * @brief Check if the window should close
 * 
 * @param window The window to check
 * @return true if the window should close, false otherwise
 */
bool Ermine::Window::ShouldCloseWindow(GLFWwindow* window)
{
    return glfwWindowShouldClose(window);
}

/**
 * @brief Shut down the window
 * 
 * @param window The window to shut down
 */
void Ermine::Window::ShutDownWindow(GLFWwindow* window)
{
    EE_CORE_TRACE("Shutting down window...");
    glfwDestroyWindow(window);
    glfwTerminate();
    EE_CORE_INFO("Window terminated successfully!");
}

/**
 * @brief Toggle fullscreen mode for the window
 * @param window The window to toggle fullscreen mode
 */
void Ermine::Window::ToggleFullscreenWindow(GLFWwindow* window)
{
    static bool isFullscreen =
#ifdef EE_DEBUG
	    false; // Debug starts in window mode
#else
        true;  // Release starts in fullscreen mode
#endif

    if (!isFullscreen)
    {
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(mon);

        glfwSetWindowMonitor(window, mon, 0, 0,
            mode->width, mode->height, mode->refreshRate);
    }
    else
    {
        // Restore to windowed mode
        glfwSetWindowMonitor(window, nullptr,
            100, 100, window_width, window_height, 0);
    }

    isFullscreen = !isFullscreen;
}

void Ermine::Window::SetVisibleCursor(const bool& value)
{
    s_visibleCursor = value;
    if (!glfwRawMouseMotionSupported())
    {
        EE_CORE_WARN("Current device doesn't support raw mouse motion support, cursor input mode will not be set.");
        return;
    }

    if (!s_visibleCursor)
        glfwSetInputMode(editor::EditorGUI::GetWindowContext(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    else
        glfwSetInputMode(editor::EditorGUI::GetWindowContext(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Ermine::Window::SetCursorLockState(CursorLockState state)
{
    s_cursorLockState = state;
    
    if (!s_window)
        return;

    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(glfwGetCurrentContext(), GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);

    switch (s_cursorLockState)
    {
    case CursorLockState::None:
#ifdef _WIN32
	    ConfineCursorToGLFWWindow(s_window, false);
#endif
		glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		break;
    case CursorLockState::Locked:
#ifdef _WIN32
	    ConfineCursorToGLFWWindow(s_window, false);
#endif
        glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
	case CursorLockState::Confined:
        glfwSetInputMode(s_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#ifdef _WIN32
		ConfineCursorToGLFWWindow(s_window, true);
#endif
        break;
    }
}
