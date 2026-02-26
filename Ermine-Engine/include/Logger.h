/* Start Header ************************************************************************/
/*!
\file       Logger.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This reflects the brief of the Logger.h file.
            This file is used to create a logger using spdlog.
            This file is included in all the source files

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifndef VERBOSE_LOGGING
#define VERBOSE_LOGGING 1
#endif

namespace Ermine
{
    class EE_API Logger
    {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };

// Logging levels:
// TRACE (Capture execution of code)
// INFO (Capture an event that occurred)
// WARN (Indicate unexpected event, disrupt or delay)
// ERROR (Capture a system interfering with functionalities)
// FATAL (Capture a system crash)
// Core logger macros
#define EE_CORE_TRACE(...)    ::Ermine::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define EE_CORE_INFO(...)     ::Ermine::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define EE_CORE_WARN(...)     ::Ermine::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define EE_CORE_ERROR(...)    ::Ermine::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define EE_CORE_FATAL(...)    ::Ermine::Logger::GetCoreLogger()->critical(__VA_ARGS__)

// Client logger macros
#define EE_TRACE(...)         ::Ermine::Logger::GetClientLogger()->trace(__VA_ARGS__)
#define EE_INFO(...)          ::Ermine::Logger::GetClientLogger()->info(__VA_ARGS__)
#define EE_WARN(...)          ::Ermine::Logger::GetClientLogger()->warn(__VA_ARGS__)
#define EE_ERROR(...)         ::Ermine::Logger::GetClientLogger()->error(__VA_ARGS__)
#define EE_FATAL(...)         ::Ermine::Logger::GetClientLogger()->critical(__VA_ARGS__)
}
