/* Start Header ************************************************************************/
/*!
\file       EditorGUI.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (85%)
\co-authors LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (2%)
\co-authors Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (8%)
\co-authors Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (5%)
\date       26/01/2026
\brief      This file contains the declaration of the EditorGUI class.
            Function just like a wrapper for the ImGUI library.
            Each window for teh editor should be encapsulated into a function in this class.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "imgui.h"
#include "GLFW/glfw3.h"
#include "ImGuiUIWindow.h"
#include <type_traits> // for std::is_base_of
#include <utility> // for std::forward
#include <memory> // Add this include for shared_ptr

namespace Ermine {
    using EntityID = unsigned long long int;
    class Scene;
    class HierarchyPanel;
}

namespace Ermine::editor
{
    class HierarchyInspector;
    /**
     * @brief The EditorGUI class, function just like a wrapper for the ImGUI library
     */
    class EE_API EditorGUI
    {
        // Keeps track of all registered ImGui windows
        static std::vector<std::unique_ptr<ImGUIWindow>> m_Windows;

        static std::shared_ptr<Ermine::Scene> s_ActiveScene;

        static std::unique_ptr<Ermine::HierarchyPanel> s_HierarchyPanel;

        static std::unique_ptr<Ermine::editor::HierarchyInspector> s_Inspector;

        /**
         * @brief Top menu bar for the editor
         */
        static void TopMenuBar(GLFWwindow* windowContext);

        /**
         * @brief Toolbar for the editor
         */
        static void Toolbar();

        /**
         * @brief Profiling window for the editor
         */
        static void ProfilingWindow();

        /**
         * @brief Show the viewport for the editor
         * @param show
         */
        static void ViewPortWindow(bool& show);

        /**
         * @brief Handle play mode start - attach game camera to primary camera entity
         */
        static void StartPlayMode();

        /**
         * @brief Handle play mode stop - restore editor camera
         */
        static void StopPlayMode();

    public:
        enum class SimState : uint8_t { stopped, playing, paused };
        static SimState s_state;
        static bool isPlaying;
        static bool isPreviewingUI; // Enable UI preview in editor viewport

        /**
         * @brief Initialize the ImGUI context
         * @param window The window to initialize the ImGUI context
         */
        static void Init(GLFWwindow* window);

        /**
         * @brief Check if the ImGUI context is initialized
         */
        static bool IsInit();

        /**
         * @brief Dock the ImGUI window
         */
        static void DockingWindow();

        /**
        * @brief Update the ImGUI context (Render)
        */
        static void Update(GLFWwindow* windowContext);

        /**
         * @brief Render the ImGUI context
         */
        static void Render();

        /**
         * @brief Shut down the ImGUI context
         */
        static void ShutDown();

        /*!***********************************************************************
        \brief
         Create and register an ImGUIWindow
        \param[in/out] T Window type
         Must derive from ImGUIWindow
        \param[in/out] Args
         Constructor arguments
        \return
         Pointer to the created window
        *************************************************************************/
        template<typename T, typename... Args>
        static T* CreateImGUIWindow(Args&&... args) {
            static_assert(std::is_base_of<ImGUIWindow, T>::value, "T must derive from ImGUIWindow");
            auto window = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = window.get();
            m_Windows.emplace_back(std::move(window));
            return ptr;
        }

        /*!***********************************************************************
        \brief
            Get a pointer to an existing ImGUI window of type T.
            Returns nullptr if not found.
        *************************************************************************/
        template<typename T>
        static T* GetWindow()
        {
            for (auto& window : m_Windows)
            {
                if (auto* casted = dynamic_cast<T*>(window.get()))
                    return casted;
            }
            return nullptr;
        }

        

        /*!***********************************************************************
        \brief
            Focus an ImGUI window by its name (brings it to the front).
        \param[in] windowName
            Name of the window to focus.
        *************************************************************************/
        static void FocusWindow(const std::string& windowName);

        static void SetActiveScene(std::shared_ptr<Ermine::Scene> scene);
        static std::shared_ptr<Ermine::Scene> GetActiveScene() { return s_ActiveScene; }

		static HierarchyPanel* GetHierarchyPanel() { return s_HierarchyPanel.get(); }

        /**
         * @brief Get the window context (for cursor locking)
         */
        static GLFWwindow* GetWindowContext() { return s_WindowContext; }

    private:
        static GLFWwindow* s_WindowContext;
        static Ermine::EntityID s_PrimaryCameraEntity;
    };
}
