/* Start Header ************************************************************************/
/*!
\file       Engine.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the Engine system.
            This file is used to initialize the engine and run the game loop.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once

#include "EngineAPI.h"
#include "GLFW/glfw3.h"

namespace Ermine::engine
{
    /**
     * @brief Initialize the engine, load all resources
     * @return true if initialization is successful, false otherwise
     */
    EE_API bool Init(GLFWwindow* windowContext);

    /**
     * @brief Shutdown the engine, release all resources
     */
    EE_API void Shutdown();
    
    /** 
     * @brief Run the game loop
     */
    EE_API void Update(GLFWwindow* windowContext);

    /**
     * @brief Render the game
     */
    EE_API void Render(GLFWwindow* window);

    /**
     * @brief Handle shading mode toggle (keys 1-4)
     * @param windowContext The GLFW window context
     */
    void HandleShadingToggle(GLFWwindow* windowContext);

    /**
     * @brief Handle fullscreen toggle (F11 key)
     * @param windowContext The GLFW window context
     */
    void HandleFullscreenToggle(GLFWwindow* windowContext);
}
