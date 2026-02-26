/* Start Header ************************************************************************/
/*!
\file       dllmain.cpp
\author     Wong Jun Yu, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      Generally this file shouldn't be modified. This file is used to initialize the DLL.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include <PreCompile.h>
#ifndef APIENTRY
#include <windows.h>
#endif

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, [[maybe_unused]] LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // Perform initialization when the DLL is loaded
        DisableThreadLibraryCalls(hModule);
        break;
    // case DLL_THREAD_ATTACH:
    //     // Perform thread-specific initialization
    //     break;
    // case DLL_THREAD_DETACH:
    //     // Perform thread-specific cleanup
    //     break;
    // case DLL_PROCESS_DETACH:
    //     // Perform cleanup when the DLL is unloaded
    //     break;
    default:
        break;
    }
	
    return TRUE;
}

