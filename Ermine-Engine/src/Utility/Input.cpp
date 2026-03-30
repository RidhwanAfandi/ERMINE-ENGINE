/* Start Header ************************************************************************/
/*!
\file       Input.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       12/03/2025
\brief      This file contains the implementation of the Input system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Input.h"
#include <GLFW/glfw3.h>

#include "AssetManager.h"
#include "imgui.h"

namespace Ermine
{
	GLFWwindow* Input::s_Window = nullptr;
	float Input::s_LastMouseX = 0.0f;
	float Input::s_LastMouseY = 0.0f;
	float Input::s_MouseDeltaX = 0.0f;
	float Input::s_MouseDeltaY = 0.0f;
	float Input::s_MouseScrollOffset = 0.0f;
	float Input::s_MouseScrollOffsetEditor = 0.0f;

	// Game mode mouse tracking
	float Input::s_GameLastMouseX = 0.0f;
	float Input::s_GameLastMouseY = 0.0f;
	float Input::s_GameMouseDeltaX = 0.0f;
	float Input::s_GameMouseDeltaY = 0.0f;
	bool Input::s_GameMouseFirstMove = true;

	std::unordered_map<int, bool> Input::s_PreviousKeyStates;
	std::unordered_map<int, bool> Input::s_PreviousMouseButtonStates;
	std::unordered_map<int, bool> Input::s_PreviousKeyStatesEditor;

	bool Input::s_GameInputActive = true;
	bool Input::s_BlockKeyboard = false;
	bool Input::s_BlockMouse = false;

	bool Input::s_EditorInputActive = false;

	/**
	 * @brief Initialize the input system
	 * @param window The window to initialize the input system with
	 */
	void Input::Init(GLFWwindow* window)
	{
		s_Window = window;
		if (!s_Window)
		{
			EE_CORE_ERROR("Input system initialized with null window!");
			return;
		}

#if defined(EE_EDITOR)
		// Scroll callback
		glfwSetScrollCallback(window, []([[maybe_unused]] GLFWwindow* window, [[maybe_unused]] double offsetX, double offsetY)
			{
				ImGuiIO& io = ImGui::GetIO();
				io.AddMouseWheelEvent(static_cast<float>(offsetX), static_cast<float>(offsetY));

				if (s_GameInputActive)
					s_MouseScrollOffset += static_cast<float>(offsetY);
				if (s_EditorInputActive)
					s_MouseScrollOffsetEditor += static_cast<float>(offsetY);
			});

		// Mouse button callback
		glfwSetMouseButtonCallback(window, []([[maybe_unused]] GLFWwindow* window, int button, int action, [[maybe_unused]] int mods)
			{
				ImGuiIO& io = ImGui::GetIO();
				io.AddMouseButtonEvent(button, action == GLFW_PRESS);

				//if (s_GameInputActive)
				//{
				//	// Update mouse button states
				//	s_PreviousMouseButtonStates[button] = (action == GLFW_PRESS);
				//}
			});

		// Key callback
		glfwSetKeyCallback(window, []([[maybe_unused]] GLFWwindow* window, int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods)
			{
				ImGuiIO& io = ImGui::GetIO();
				io.AddKeyEvent(GlfwKeyToImguiKey(key), action == GLFW_PRESS || action == GLFW_REPEAT);

				if (s_GameInputActive)
				{
					// Update key states
					s_PreviousKeyStates[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
				}
			});

		// Cursor position callback
		glfwSetCursorPosCallback(window, []([[maybe_unused]] GLFWwindow* window, double xpos, double ypos)
			{

				ImGuiIO& io = ImGui::GetIO();
				io.AddMousePosEvent(static_cast<float>(xpos), static_cast<float>(ypos));

				s_LastMouseX = static_cast<float>(xpos);
				s_LastMouseY = static_cast<float>(ypos);
			});
#endif

		double mouseX, mouseY;
		glfwGetCursorPos(s_Window, &mouseX, &mouseY);
		s_LastMouseX = static_cast<float>(mouseX);
		s_LastMouseY = static_cast<float>(mouseY);

		if (const char* keyMap = AssetManager::GetInstance().load_file_contents("../Resources/gamecontrollerdb.txt"))
		{
			glfwUpdateGamepadMappings(keyMap);
			delete[] keyMap; // Release the keymap buffer
		}

		EE_CORE_INFO("Input system initialized");
	}

	/**
	 * @brief Update the input system
	 */
	void Input::Update()
	{
		// Read ImGUI capture intent and compute gameplay blocks
		s_BlockKeyboard = !s_GameInputActive;
		s_BlockMouse = !s_GameInputActive;
		
		// Update mouse delta
		double mouseX, mouseY;
		glfwGetCursorPos(s_Window, &mouseX, &mouseY);

		s_MouseDeltaX = static_cast<float>(mouseX) - s_LastMouseX;
		s_MouseDeltaY = static_cast<float>(mouseY) - s_LastMouseY;

		s_LastMouseX = static_cast<float>(mouseX);
		s_LastMouseY = static_cast<float>(mouseY);

		if (s_BlockMouse)
		{
			s_MouseDeltaX = 0.0f;
			s_MouseDeltaY = 0.0f;
		}

		//if (!s_BlockKeyboard)
		//{
		//	for (auto& [key, state] : s_PreviousKeyStates)
		//		state = IsKeyDown(key);
		//}
		//if (!s_BlockMouse)
		//{
		//	for (auto& [button, state] : s_PreviousMouseButtonStates)
		//		state = IsMouseButtonDown(button);
		//}

		//if (s_EditorInputActive)
		//{
		//	for (auto& [key, state] : s_PreviousKeyStatesEditor)
		//		state = IsKeyDownEditor(key);
		//}
	}

	void Input::SetGameInputActive(bool active)
	{
		if (active && !s_GameInputActive)
		{
			ResetGameMouseDelta();
		}

		s_GameInputActive = active;
	}

	bool Input::IsGameInputActive()
	{
		return s_GameInputActive;
	}

	void Input::SetEditorInputActive(bool active)
	{
		s_EditorInputActive = active;
	}

	bool Input::IsEditorInputActive()
	{
		return s_EditorInputActive;
	}

	ImGuiKey Input::GlfwKeyToImguiKey(int key)
	{
		switch (key)
		{
		case GLFW_KEY_TAB: return ImGuiKey_Tab;
		case GLFW_KEY_LEFT: return ImGuiKey_LeftArrow;
		case GLFW_KEY_RIGHT: return ImGuiKey_RightArrow;
		case GLFW_KEY_UP: return ImGuiKey_UpArrow;
		case GLFW_KEY_DOWN: return ImGuiKey_DownArrow;
		case GLFW_KEY_PAGE_UP: return ImGuiKey_PageUp;
		case GLFW_KEY_PAGE_DOWN: return ImGuiKey_PageDown;
		case GLFW_KEY_HOME: return ImGuiKey_Home;
		case GLFW_KEY_END: return ImGuiKey_End;
		case GLFW_KEY_INSERT: return ImGuiKey_Insert;
		case GLFW_KEY_DELETE: return ImGuiKey_Delete;
		case GLFW_KEY_BACKSPACE: return ImGuiKey_Backspace;
		case GLFW_KEY_SPACE: return ImGuiKey_Space;
		case GLFW_KEY_ENTER: return ImGuiKey_Enter;
		case GLFW_KEY_ESCAPE: return ImGuiKey_Escape;
		case GLFW_KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
		case GLFW_KEY_COMMA: return ImGuiKey_Comma;
		case GLFW_KEY_MINUS: return ImGuiKey_Minus;
		case GLFW_KEY_PERIOD: return ImGuiKey_Period;
		case GLFW_KEY_SLASH: return ImGuiKey_Slash;
		case GLFW_KEY_SEMICOLON: return ImGuiKey_Semicolon;
		case GLFW_KEY_EQUAL: return ImGuiKey_Equal;
		case GLFW_KEY_LEFT_BRACKET: return ImGuiKey_LeftBracket;
		case GLFW_KEY_BACKSLASH: return ImGuiKey_Backslash;
		case GLFW_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
		case GLFW_KEY_GRAVE_ACCENT: return ImGuiKey_GraveAccent;
		case GLFW_KEY_CAPS_LOCK: return ImGuiKey_CapsLock;
		case GLFW_KEY_SCROLL_LOCK: return ImGuiKey_ScrollLock;
		case GLFW_KEY_NUM_LOCK: return ImGuiKey_NumLock;
		case GLFW_KEY_PRINT_SCREEN: return ImGuiKey_PrintScreen;
		case GLFW_KEY_PAUSE: return ImGuiKey_Pause;
		case GLFW_KEY_KP_0: return ImGuiKey_Keypad0;
		case GLFW_KEY_KP_1: return ImGuiKey_Keypad1;
		case GLFW_KEY_KP_2: return ImGuiKey_Keypad2;
		case GLFW_KEY_KP_3: return ImGuiKey_Keypad3;
		case GLFW_KEY_KP_4: return ImGuiKey_Keypad4;
		case GLFW_KEY_KP_5: return ImGuiKey_Keypad5;
		case GLFW_KEY_KP_6: return ImGuiKey_Keypad6;
		case GLFW_KEY_KP_7: return ImGuiKey_Keypad7;
		case GLFW_KEY_KP_8: return ImGuiKey_Keypad8;
		case GLFW_KEY_KP_9: return ImGuiKey_Keypad9;
		case GLFW_KEY_KP_DECIMAL: return ImGuiKey_KeypadDecimal;
		case GLFW_KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
		case GLFW_KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
		case GLFW_KEY_KP_SUBTRACT: return ImGuiKey_KeypadSubtract;
		case GLFW_KEY_KP_ADD: return ImGuiKey_KeypadAdd;
		case GLFW_KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
		case GLFW_KEY_KP_EQUAL: return ImGuiKey_KeypadEqual;
		case GLFW_KEY_LEFT_SHIFT: return ImGuiKey_LeftShift;
		case GLFW_KEY_LEFT_CONTROL: return ImGuiKey_LeftCtrl;
		case GLFW_KEY_LEFT_ALT: return ImGuiKey_LeftAlt;
		case GLFW_KEY_LEFT_SUPER: return ImGuiKey_LeftSuper;
		case GLFW_KEY_RIGHT_SHIFT: return ImGuiKey_RightShift;
		case GLFW_KEY_RIGHT_CONTROL: return ImGuiKey_RightCtrl;
		case GLFW_KEY_RIGHT_ALT: return ImGuiKey_RightAlt;
		case GLFW_KEY_RIGHT_SUPER: return ImGuiKey_RightSuper;
		case GLFW_KEY_MENU: return ImGuiKey_Menu;
		case GLFW_KEY_0: return ImGuiKey_0;
		case GLFW_KEY_1: return ImGuiKey_1;
		case GLFW_KEY_2: return ImGuiKey_2;
		case GLFW_KEY_3: return ImGuiKey_3;
		case GLFW_KEY_4: return ImGuiKey_4;
		case GLFW_KEY_5: return ImGuiKey_5;
		case GLFW_KEY_6: return ImGuiKey_6;
		case GLFW_KEY_7: return ImGuiKey_7;
		case GLFW_KEY_8: return ImGuiKey_8;
		case GLFW_KEY_9: return ImGuiKey_9;
		case GLFW_KEY_A: return ImGuiKey_A;
		case GLFW_KEY_B: return ImGuiKey_B;
		case GLFW_KEY_C: return ImGuiKey_C;
		case GLFW_KEY_D: return ImGuiKey_D;
		case GLFW_KEY_E: return ImGuiKey_E;
		case GLFW_KEY_F: return ImGuiKey_F;
		case GLFW_KEY_G: return ImGuiKey_G;
		case GLFW_KEY_H: return ImGuiKey_H;
		case GLFW_KEY_I: return ImGuiKey_I;
		case GLFW_KEY_J: return ImGuiKey_J;
		case GLFW_KEY_K: return ImGuiKey_K;
		case GLFW_KEY_L: return ImGuiKey_L;
		case GLFW_KEY_M: return ImGuiKey_M;
		case GLFW_KEY_N: return ImGuiKey_N;
		case GLFW_KEY_O: return ImGuiKey_O;
		case GLFW_KEY_P: return ImGuiKey_P;
		case GLFW_KEY_Q: return ImGuiKey_Q;
		case GLFW_KEY_R: return ImGuiKey_R;
		case GLFW_KEY_S: return ImGuiKey_S;
		case GLFW_KEY_T: return ImGuiKey_T;
		case GLFW_KEY_U: return ImGuiKey_U;
		case GLFW_KEY_V: return ImGuiKey_V;
		case GLFW_KEY_W: return ImGuiKey_W;
		case GLFW_KEY_X: return ImGuiKey_X;
		case GLFW_KEY_Y: return ImGuiKey_Y;
		case GLFW_KEY_Z: return ImGuiKey_Z;
		case GLFW_KEY_F1: return ImGuiKey_F1;
		case GLFW_KEY_F2: return ImGuiKey_F2;
		case GLFW_KEY_F3: return ImGuiKey_F3;
		case GLFW_KEY_F4: return ImGuiKey_F4;
		case GLFW_KEY_F5: return ImGuiKey_F5;
		case GLFW_KEY_F6: return ImGuiKey_F6;
		case GLFW_KEY_F7: return ImGuiKey_F7;
		case GLFW_KEY_F8: return ImGuiKey_F8;
		case GLFW_KEY_F9: return ImGuiKey_F9;
		case GLFW_KEY_F10: return ImGuiKey_F10;
		case GLFW_KEY_F11: return ImGuiKey_F11;
		case GLFW_KEY_F12: return ImGuiKey_F12;
		default: return ImGuiKey_None;
		}
	}

	/*!
	\brief Checks if a gamepad button is currently pressed.
	\param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
	\param ButtonID The button ID to check.
	\return True if the button is pressed, false otherwise.
	*/
	bool Input::IsGamepadButtonPressed(int JoystickID, int ButtonID)
	{
		if (!glfwJoystickIsGamepad(JoystickID) || ButtonID == GLFW_GAMEPAD_BUTTON_LAST)
			return false;

		GLFWgamepadstate state;
		if (glfwGetGamepadState(JoystickID, &state) && ButtonID >= 0 && ButtonID <= GLFW_GAMEPAD_BUTTON_LAST)
		{
			return state.buttons[ButtonID] == GLFW_PRESS;
		}

		return false;
	}

	/*!
	\brief Checks if a gamepad button is triggered (pressed for the first time).
	\param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
	\param ButtonID The button ID to check.
	\return True if the button is triggered, false otherwise.
	*/
	bool Input::IsGamepadButtonTriggered(int JoystickID, int ButtonID)
	{
		if (!glfwJoystickIsGamepad(JoystickID) || ButtonID == GLFW_GAMEPAD_BUTTON_LAST)
			return false;

		GLFWgamepadstate state;
		int uniqueKey = (JoystickID << 16) | ButtonID;
		if (glfwGetGamepadState(JoystickID, &state) && ButtonID >= 0 && ButtonID <= GLFW_GAMEPAD_BUTTON_LAST)
		{
			if (state.buttons[ButtonID] == GLFW_PRESS && !s_PreviousKeyStates[uniqueKey])
			{
				s_PreviousKeyStates[uniqueKey] = true;
				return true;
			}
		}

		if (state.buttons[ButtonID] == GLFW_RELEASE)
		{
			s_PreviousKeyStates[uniqueKey] = false;
		}

		return false;
	}

	/*!
	\brief Gets the joystick axes values.
	\param JoystickID The ID of the joystick (GLFW_JOYSTICK_1 through GLFW_JOYSTICK_16).
	\param deadzone The deadzone value for the joystick axes.
	\return A vector of float values representing joystick axis positions, or empty if joystick is not present.
	*/
	std::vector<float> Input::GetJoystickAxes(int JoystickID, float deadzone)
	{
		if (!glfwJoystickIsGamepad(JoystickID))
			return {};

		// Clamp deadzone to valid range [0.0, 1.0]
		deadzone = std::max(0.0f, std::min(deadzone, 1.0f));

		int axesCount;
		const float* axes = glfwGetJoystickAxes(JoystickID, &axesCount);

		if (axes == nullptr)
			return {};

		// Convert the raw pointer to a vector with deadzone applied
		std::vector<float> processedAxes;
		processedAxes.reserve(axesCount);

		for (int i = 0; i < axesCount; ++i)
		{
			float value = axes[i];

			// Apply deadzone
			if (std::abs(value) < deadzone)
			{
				value = 0.0f;
			}
			else
			{
				// Rescale the values outside deadzone to full range
				// This creates a smooth transition from deadzone to max value
				float sign = (value > 0.0f) ? 1.0f : -1.0f;
				value = sign * (std::abs(value) - deadzone) / (1.0f - deadzone);
			}

			processedAxes.push_back(value);
		}

		return processedAxes;
	}

	bool Input::IsKeyPressed(int keyCode)
	{
		if (!s_Window /*|| s_BlockKeyboard*/)
			return false;

		// Check if key exists in previous states map
		if (!s_PreviousKeyStates.contains(keyCode))
			s_PreviousKeyStates[keyCode] = false;
		//auto it = s_PreviousKeyStates.find(keyCode);
		//if (it == s_PreviousKeyStates.end())
		//{
		//	s_PreviousKeyStates[keyCode] = false;
		//}

		bool previous = s_PreviousKeyStates[keyCode];
		bool current = IsKeyDown(keyCode);

		bool pressed = current && !previous;
		s_PreviousKeyStates[keyCode] = current;
		return pressed;
	}

	bool Input::IsKeyReleased(int keyCode)
	{
		if (!s_Window /*|| s_BlockKeyboard*/)
			return false;

		// Check if key exists in previous states map
		if (!s_PreviousKeyStates.contains(keyCode))
			s_PreviousKeyStates[keyCode] = false;

		bool previous = s_PreviousKeyStates[keyCode];
		bool current = IsKeyDown(keyCode);

		bool released = !current && previous;
		s_PreviousKeyStates[keyCode] = current; // update after computing result
		return released;
	}

	bool Input::IsKeyDown(int keyCode)
	{
		if (!s_Window /*|| s_BlockKeyboard*/)
			return false;

		auto state = glfwGetKey(s_Window, keyCode);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonPressed(int button)
	{
		if (!s_Window /*|| s_BlockMouse*/)
			return false;

		// Check if button exists in previous states map
		if (!s_PreviousMouseButtonStates.contains(button))
			s_PreviousMouseButtonStates[button] = false;

		bool previous = s_PreviousMouseButtonStates[button];
		bool current = IsMouseButtonDown(button);

		bool pressed = current && !previous;
		s_PreviousMouseButtonStates[button] = current; // update after computing result
		return pressed;
	}

	bool Input::IsMouseButtonReleased(int button)
	{
		if (!s_Window /*|| s_BlockMouse*/)
			return false;

		// Check if button exists in previous states map
		if (!s_PreviousMouseButtonStates.contains(button))
			s_PreviousMouseButtonStates[button] = false;

		bool previous = s_PreviousMouseButtonStates[button];
		bool current = IsMouseButtonDown(button);

		bool released = !current && previous;
		s_PreviousMouseButtonStates[button] = current; // update after computing result
		return released;
	}

	bool Input::IsMouseButtonDown(int button)
	{
		if (!s_Window /*|| s_BlockMouse*/)
			return false;

		auto state = glfwGetMouseButton(s_Window, button);
		return state == GLFW_PRESS;
	}

	bool Input::IsKeyDownEditor(int keyCode)
	{
		if (!s_Window || !s_EditorInputActive)
			return false;
		auto state = glfwGetKey(s_Window, keyCode);
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsKeyPressedEditor(int keyCode)
	{
		if (!s_Window || !s_EditorInputActive)
			return false;
		// Check if key exists in previous states map
		auto it = s_PreviousKeyStatesEditor.find(keyCode);
		if (it == s_PreviousKeyStatesEditor.end())
			s_PreviousKeyStatesEditor[keyCode] = false;
		
		bool previous = s_PreviousKeyStatesEditor[keyCode];
		bool current = IsKeyDownEditor(keyCode);

		s_PreviousKeyStatesEditor[keyCode] = current;

		return current && !previous;
	}

	bool Input::IsMouseButtonDownEditor(int button)
	{
		if (!s_Window || !s_EditorInputActive)
			return false;
		auto state = glfwGetMouseButton(s_Window, button);
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		if (!s_Window)
			return { 0.0f, 0.0f };

		double mouseX, mouseY;
		glfwGetCursorPos(s_Window, &mouseX, &mouseY);

		return { static_cast<float>(mouseX), static_cast<float>(mouseY) };
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().first;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().second;
	}

	float Input::GetMouseScrollOffset()
	{
		return s_MouseScrollOffset;
	}

	void Input::ResetMouseScrollOffset()
	{
		s_MouseScrollOffset = 0.0f;
	}

	float Input::GetMouseScrollOffsetEditor()
	{
		return s_MouseScrollOffsetEditor;
	}

	void Input::ResetMouseScrollOffsetEditor()
	{
		s_MouseScrollOffsetEditor = 0.0f;
	}



	std::pair<float, float> Input::GetMouseDelta()
	{
		return { s_MouseDeltaX, s_MouseDeltaY };
	}

	std::pair<float, float> Input::GetMouseDeltaGame()
	{
		if (!s_Window)
			return { 0.0f, 0.0f };

		double mouseX, mouseY;
		glfwGetCursorPos(s_Window, &mouseX, &mouseY);

		// Handle first mouse movement to avoid large delta jump
		if (s_GameMouseFirstMove)
		{
			s_GameLastMouseX = static_cast<float>(mouseX);
			s_GameLastMouseY = static_cast<float>(mouseY);
			s_GameMouseFirstMove = false;
			return { 0.0f, 0.0f };
		}

		s_GameMouseDeltaX = static_cast<float>(mouseX) - s_GameLastMouseX;
		s_GameMouseDeltaY = s_GameLastMouseY - static_cast<float>(mouseY); // Reversed: y-coordinates range from bottom to top

		s_GameLastMouseX = static_cast<float>(mouseX);
		s_GameLastMouseY = static_cast<float>(mouseY);

		return { s_GameMouseDeltaX, s_GameMouseDeltaY };
	}

	void Input::ResetGameMouseDelta()
	{
		s_GameMouseDeltaX = 0.0f;
		s_GameMouseDeltaY = 0.0f;
		s_GameMouseFirstMove = true;
	}
}
