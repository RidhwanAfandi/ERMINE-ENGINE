/* Start Header ************************************************************************/
/*!
\file       Logger.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the Logger system.
			This file is used to create a logger using spdlog.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Logger.h"
#if defined(ERMINE_USE_SPDLOG)
#include "ConsoleGUI.h"
#endif

using namespace Ermine;

std::shared_ptr<spdlog::logger> Logger::s_CoreLogger;
std::shared_ptr<spdlog::logger> Logger::s_ClientLogger;

void Logger::Init()
{
	spdlog::set_pattern("%^[%T] %n: %v%$");

	s_CoreLogger = spdlog::stdout_color_mt("Ermine Engine");
	s_CoreLogger->set_level(spdlog::level::trace);

	s_ClientLogger = spdlog::stdout_color_mt("Ermine Editor");
	s_ClientLogger->set_level(spdlog::level::trace);

#if defined(ERMINE_USE_SPDLOG)
	// Register spdlog sink to capture logs into the editor console
	auto imguiSink = std::make_shared<Ermine::ImGuiConsoleSink_mt>();
	imguiSink->set_pattern("%^[%T] %v%$");
	s_CoreLogger->sinks().push_back(imguiSink);
	s_ClientLogger->sinks().push_back(imguiSink);
#endif
}