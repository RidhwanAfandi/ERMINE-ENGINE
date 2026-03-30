/* Start Header ************************************************************************/
/*!
\file       Input.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       12/03/2025
\brief      This file contains the Input system for handling keyboard, mouse and gamepad input.
            This file is used to manage the input system for the engine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

#include "imgui.h"

struct GLFWwindow;

namespace Ermine
{
    class EE_API Input
    {
    public:
        // Gamepad
        /*!
        \brief Checks if a gamepad button is currently pressed.
        \param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
        \param ButtonID The button ID to check.
        \return True if the button is pressed, false otherwise.
        */
        static bool IsGamepadButtonPressed(int JoystickID, int ButtonID);

        /*!
        \brief Checks if a gamepad button is triggered (pressed for the first time).
        \param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
        \param ButtonID The button ID to check.
        \return True if the button is triggered, false otherwise.
        */
        static bool IsGamepadButtonTriggered(int JoystickID, int ButtonID);

        /*!
        \brief Gets the joystick axes values.
        \param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
        \param deadzone The deadzone value for the joystick axes.
        \return A vector of float values representing joystick axis positions, or empty if joystick is not present.
        */
        static std::vector<float> GetJoystickAxes(int JoystickID, float deadzone);
        
        // Keyboard
        /**
         * @brief Check if the key is pressed
         * @param keyCode The key to check
         * @return true if the key is pressed
         */
        static bool IsKeyPressed(int keyCode);
        /**
         * @brief Check if the key is released
         * @param keyCode The key to check
         * @return true if the key is released
         */
        static bool IsKeyReleased(int keyCode);
        /**
         * @brief Check if the key is down
         * @param keyCode The key to check
         * @return true if the key is down
         */
        static bool IsKeyDown(int keyCode);
        
        // Mouse
        /**
         * @brief Check if the mouse button is pressed
         * @param button The button to check
         * @return true if the button is pressed
         */
        static bool IsMouseButtonPressed(int button);
        /**
         * @brief Check if the mouse button is released
         * @param button The button to check
         * @return true if the button is released
         */
        static bool IsMouseButtonReleased(int button);
        /**
         * @brief Check if the mouse button is down
         * @param button The button to check
         * @return true if the button is down
         */
        static bool IsMouseButtonDown(int button);
        /**
         * @brief Get the mouse position
         * @return The mouse position
         */
        static std::pair<float, float> GetMousePosition();
        /**
         * @brief Get the mouse x position
         * @return The mouse x position
         */
        static float GetMouseX();
        /**
         * @brief Get the mouse y position
         * @return The mouse y position
         */
        static float GetMouseY();
        /**
         * @brief Get the mouse scroll offset
         * @return The mouse scroll offset
         */
        static float GetMouseScrollOffset();
        /**
         * @brief Reset the mouse scroll accumulation
         */
        static void ResetMouseScrollOffset();
        /**
         * @brief Get the mouse delta
         * @return The mouse delta
         */
        static std::pair<float, float> GetMouseDelta();
        
        /**
         * @brief Get the mouse delta for game mode (unaffected by editor blocking)
         * @return The mouse delta for game
         */
        static std::pair<float, float> GetMouseDeltaGame();
        /**
         * @brief Reset the next game mouse delta sample after cursor/input mode changes
         */
        static void ResetGameMouseDelta();
        
        // Initialization
        /**
         * @brief Initialize the input system
         * @param window The window to initialize the input system with
         */
        static void Init(GLFWwindow* window);
        /**
         * @brief Update the input system
         */
        static void Update();

        /**
         * @brief Set whether game input is active (for ImGui integration)
         * @param active True to activate game input, false to deactivate
		 */
        static void SetGameInputActive(bool active);
        /**
         * @brief Check if game input is active (for ImGui integration)
		 * @return True if game input is active, false otherwise
		 */
        static bool IsGameInputActive();
        /**
		 * @brief Set whether editor input is active (for ImGui integration)
		 * @param active True to activate editor input, false to deactivate
         */
        static void SetEditorInputActive(bool active);
        /**
		 * @brief Check if editor input is active (for ImGui integration)
		 * @return True if editor input is active, false otherwise
		 */
		static bool IsEditorInputActive();

        /**
         * @brief Check if the key is down in editor mode (ignores game input blocking)
         * @param keyCode The key to check
         * @return true if the key is down
		 */
        static bool IsKeyDownEditor(int keyCode);
        /**
         * @brief Check if the key is pressed in editor mode (ignores game input blocking)
         * @param keyCode The key to check
		 * @return true if the key is pressed
		 */
        static bool IsKeyPressedEditor(int keyCode);
        /**
         * @brief Check if the mouse button is down in editor mode (ignores game input blocking)
         * @param button The button to check
		 * @return true if the button is down
		 */
        static bool IsMouseButtonDownEditor(int button);
        /**
         * @brief Get the mouse position in editor mode (ignores game input blocking)
		 * @return The mouse position
		 */
        static float GetMouseScrollOffsetEditor();
        /**
		 * @brief Reset the mouse scroll accumulation in editor mode (ignores game input blocking)
		 */ 
        static void ResetMouseScrollOffsetEditor();

        static GLFWwindow* s_Window; // TODO: This got borrowed from ScriptEngine, might want to think about proper usage?
    private:
        static float s_LastMouseX;
        static float s_LastMouseY;
        static float s_MouseDeltaX;
        static float s_MouseDeltaY;
        static float s_MouseScrollOffset;

        static float s_MouseScrollOffsetEditor;

        // Game mode mouse tracking
        static float s_GameLastMouseX;
        static float s_GameLastMouseY;
        static float s_GameMouseDeltaX;
        static float s_GameMouseDeltaY;
        static bool s_GameMouseFirstMove;
        
        // Track previous frame's key/mouse button states
        static std::unordered_map<int, bool> s_PreviousKeyStates;
        static std::unordered_map<int, bool> s_PreviousMouseButtonStates;

		// Track previous frame's key states for editor mode
        static std::unordered_map<int, bool> s_PreviousKeyStatesEditor;

        // Gate flags
        static bool s_GameInputActive;
        static bool s_BlockKeyboard;
		static bool s_BlockMouse;

        static bool s_EditorInputActive;

		// Helper function to convert GLFW key to ImGui key
        static ImGuiKey GlfwKeyToImguiKey(int key);
    };
}
