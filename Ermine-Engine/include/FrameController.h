/* Start Header ************************************************************************/
/*!
\file       FrameController.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the FrameController system.
            This file is used to control the frame rate of the game.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

namespace Ermine
{
    class FrameController
    {
        inline static float s_targetDeltaTime;
        inline static float s_fixedDeltaTime;
        inline static float s_deltaTime;
        inline static float s_accumulator;
        inline static bool s_framePacing;

        inline static int s_frameCount;
        inline static float s_fpsTimer;
        inline static float s_FPS;

        inline static double s_last_frame_time;
    public:
        // FrameController(float targetFPS = 60.0f, float fixedFPS = 60.0f);

        /**
         * @brief Initialize the frame controller
         * @param targetFPS The target FPS
         * @param fixedFPS The fixed FPS
         */
        static void Init(float targetFPS = 60.f, float fixedFPS = 60.0f);

        /**
         * @brief Call this at the beginning of each frame to calculate the delta time.
         */
        static void BeginFrame();

        /**
         * @brief Check if a fixed update step is needed
         * @return true if a fixed update step is needed
         */
        static bool ShouldUpdateFixed();

        /**
         * @brief Get the delta time
         * @return delta time
         */
        static float GetDeltaTime() { return s_deltaTime; }
        /**
         * @brief Get the fixed delta time
         * @return fixed delta time 
         */
        static float GetFixedDeltaTime() { return s_fixedDeltaTime; }
        /**
         * @brief Get remaining time ratio for interpolation
         * @return interpolation alpha
         */
        static float GetInterpolationAlpha() { return s_accumulator / s_fixedDeltaTime; }

        /**
         * @brief Get the current FPS
         * @return current FPS
         */
        static float GetFPS() { return s_FPS; }
    };
}
