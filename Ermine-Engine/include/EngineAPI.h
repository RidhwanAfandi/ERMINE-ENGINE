/* Start Header ************************************************************************/
/*!
\file       EngineAPI.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       Mar 9, 2025
\brief      Defines the macro for the engine's API

Copyright (C) 2024 TwoJumpingRabbits
*/
/* End Header **************************************************************************/
#pragma once

#ifdef EE_PLATFORM_WINDOWS
    #ifdef EE_BUILD_DLL
        #define EE_API __declspec(dllexport)
    #else
        #define EE_API __declspec(dllimport)
    #endif
#endif

#define ERMINE_VERSION_MAJOR 0
#define ERMINE_VERSION_MINOR 2