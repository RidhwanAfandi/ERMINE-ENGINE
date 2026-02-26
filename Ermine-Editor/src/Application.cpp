/* Start Header ************************************************************************/
/*!
\file       Application.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       15/03/2025
\brief      Main application file for the Ermine editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include <Window.h> // Window management for editor
#include <Engine.h> // Core engine systems
#include <EditorGUI.h> // ImGUI wrapper for editor
//#include <FrameController.h> // Frame rate controller
//#include <Logger.h> // Logging system

extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001; // Enable NVIDIA Optimus on laptops, requests high-performance GPU
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // Enable AMD Switchable Graphics on laptops, requests high-performance GPU
}

using namespace Ermine;

#if (defined(EE_EDITOR) || defined(EE_GAME)) && defined(EE_RELEASE)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    Logger::Init();

    GLFWwindow* window = Window::InitWindow(1920,1080, "Ermine Editor 0.4");
    if (window == nullptr)
        return -1;

    if (!engine::Init(window)) // if engine fails to initialize
        return -1;

    editor::EditorGUI::Init(window);

    bool running = true;
    while (running && !Window::ShouldCloseWindow(window))
    {
        engine::Update(window);

        if (editor::EditorGUI::IsInit())
            editor::EditorGUI::Update(window); // Update the ImGUI context

        engine::Render(window);
    }

    editor::EditorGUI::ShutDown();

    engine::Shutdown();
    Window::ShutDownWindow(window);

    return 0;
}