#include "Engine.h"
#include <Window.h>
#ifdef _WIN32
#include <Windows.h>
#endif

extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001; // Enable NVIDIA Optimus on laptops, requests high-performance GPU
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // Enable AMD Switchable Graphics on laptops, requests high-performance GPU
}

#if (defined(EE_EDITOR) || defined(EE_GAME)) && defined(EE_RELEASE)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
	Ermine::Logger::Init();

    GLFWwindow* window = Ermine::Window::InitWindow(1920, 1080, "Machina");
    if (!window) return -1;

    if (!Ermine::engine::Init(window)) return -1;

    while (!Ermine::Window::ShouldCloseWindow(window))
    {
        Ermine::engine::Update(window);
        Ermine::engine::Render(window);
    }

    Ermine::engine::Shutdown();
    Ermine::Window::ShutDownWindow(window);
    return 0;
}