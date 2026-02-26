/* Start Header ************************************************************************/
/*!
\file       SettingsGUI.h
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Jan 31, 2026
\brief      This file contains the implementation of the Inspector GUI window.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "ImguiUIWindow.h"

namespace Ermine
{
    class SettingsGUI : public ImGUIWindow
    {
    public:
        /**
        * @brief Constructor
        * @param EntityID
        * @param Inspector name
        */
        explicit SettingsGUI(const std::string& name = "UI Settings");

        /**
        * @brief Set font size
        * @param font size
        * @param base font size
        */
		static void SetFontSize(float fontSize, float baseFontSize);

        /**
        * @brief Get font size
        * @return font size
        */
        static float GetFontSizeS();

        /**
        * @brief Get base font size
        * @return base font size
        */
        static float GetBaseFontSize();

        /**
        * @brief Set theme mode
        * @param mode
        */
		static void SetMode(int mode);

        /**
        * @brief Get theme mode
        * @return mode
        */
		static int GetMode();

        /**
        * @brief Inherited from ImGUIWindow, render loop
        */
        void Render() override;  // defined in .cpp

    private:
		//int s_mode = 0; // 0: Light, 1: Dark, 2: Pink, 3: Cyberpunk, 4: Overwatch(Dark), 5: Overwatch(Light)
    };
}
