/* Start Header ************************************************************************/
/*!
\file       ScriptEngine.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/08/2025
\brief      This files contains the declaration for Mono Script Engine
			This is the main interface for the scripting engine, it should be used to initialize and shutdown the engine.
			It also provides functions to execute scripts, call functions and get/set variables.
			The script engine is based on Mono, a cross-platform implementation of the .NET framework.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "ScriptEngine.h"

#include "ECS.h"
#include "Components.h"
#include "FrameController.h"
#include "Input.h"
#include "Logger.h"
#include "JobSystem.h"
#include <HierarchySystem.h>
#include "FiniteStateMachine.h"
#include "NavMeshAgentSystem.h"
#include "AudioManager.h"
#include "AudioSystem.h"
#include "Physics.h"
#include "SceneManager.h"
#include "Serialisation.h"
#include "UIRenderSystem.h"
#include "Window.h"
#include "VideoManager.h"
#include "Renderer.h"

namespace fs = std::filesystem;

namespace
{
	constexpr unsigned int FIELD_ACCESS_MASK = 0x0007;
	constexpr unsigned int FIELD_ATTRIBUTE_PUBLIC = 0x0006;
	// Helper: convert file_time_type to local time string
	//std::string FormatFileTime(std::filesystem::file_time_type tp)
	//{
	//	using namespace std::chrono;
	//	if (tp == std::filesystem::file_time_type{}) return "<unset>";

	//	// Convert filesystem clock -> system_clock
	//	const auto sctp = time_point_cast<system_clock::duration>(
	//		tp - std::filesystem::file_time_type::clock::now() + system_clock::now());

	//	const std::time_t tt = system_clock::to_time_t(sctp);
	//	std::tm tm{};
	//#if defined(_WIN32)
	//	localtime_s(&tm, &tt);
	//#else
	//	localtime_r(&tt, &tm);
	//#endif
	//	char buf[32]{};
	//	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
	//	return std::string(buf);
	//}

	static std::mutex s_LateDestroyMutex;
	static std::vector<Ermine::EntityID> s_LateDestroyQueue;

	static inline void EnqueueLateDestory(Ermine::EntityID id)
	{
		if (id == 0) return;
		std::scoped_lock locks(s_LateDestroyMutex);
		if (std::find(s_LateDestroyQueue.begin(), s_LateDestroyQueue.end(), id) == s_LateDestroyQueue.end())
			s_LateDestroyQueue.push_back(id);
	}

	std::string Trim(const std::string& s)
	{
		const char* ws = " \t\r\n";
		const size_t b = s.find_first_not_of(ws);
		if (b == std::string::npos) return {};
		const size_t e = s.find_last_not_of(ws);
		return s.substr(b, e - b + 1);
	}

	std::string GetEnv(const char* name)
	{
#if defined(_WIN32)
		if (!name || !*name) return {};
		char* buf = nullptr;
		size_t len = 0;
		errno_t err = _dupenv_s(&buf, &len, name);
		if (err != 0 || !buf) return {};
		std::string value;
		if (len > 0)
			value.assign(buf, buf + (buf[len - 1] == '\0' ? len - 1 : len));
		free(buf);
		return value;
#else
		const char* v = std::getenv(name);
		return v ? std::string(v) : std::string();
#endif
	}

#if defined(_WIN32)
	std::wstring ToWide(const std::string& s)
	{
		if (s.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(static_cast<size_t>(len), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
		if (!w.empty() && w.back() == L'\0') w.pop_back();
		return w;
	}

	std::string FromBytes(const std::vector<char>& bytes)
	{
		return std::string(bytes.begin(), bytes.end());
	}

	std::wstring QuoteIfNeeded(std::wstring arg)
	{
		bool needs = arg.find_first_of(L" \t\"") != std::wstring::npos;
		if (!needs) return arg;
		size_t pos = 0;
		while ((pos = arg.find(L'"', pos)) != std::wstring::npos)
		{
			arg.insert(pos, L"\\");
			pos += 2;
		}
		return L"\"" + arg + L"\"";
	}

	// Run a process and capture its stdout/stderr output (With best friend's help)
	bool RunProcessCapture(const std::wstring& exePath,
		const std::vector<std::wstring>& args,
		DWORD& exitCode,
		std::string& output)
	{
		output.clear();
		exitCode = DWORD(-1);

		SECURITY_ATTRIBUTES sa{};
		sa.nLength = sizeof(sa);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = nullptr;

		HANDLE hStdOutRd = nullptr, hStdOutWr = nullptr;
		if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0))
			return false;
		// make read end non-inheritable
		SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);

		STARTUPINFOW si{};
		si.cb = sizeof(si);
		si.hStdError = hStdOutWr;
		si.hStdOutput = hStdOutWr;
		si.dwFlags |= STARTF_USESTDHANDLES;

		PROCESS_INFORMATION pi{};
		// build command line (program name + args)
		std::wstring cmdLine;
		cmdLine.reserve(512);
		{
			cmdLine.append(QuoteIfNeeded(exePath));
			for (const auto& a : args)
			{
				cmdLine.push_back(L' ');
				cmdLine.append(QuoteIfNeeded(a));
			}
		}
		// CreateProcess requires mutable buffer
		std::vector cmdBuf(cmdLine.begin(), cmdLine.end());
		cmdBuf.push_back(L'\0');

		BOOL ok = CreateProcessW(
			exePath.c_str(),         // lpApplicationName
			cmdBuf.data(),           // lpCommandLine (mutable)
			nullptr, nullptr, TRUE,  // inherit handles to get pipes
			CREATE_NO_WINDOW,
			nullptr, nullptr,
			&si, &pi
		);

		// Parent does not need write end
		CloseHandle(hStdOutWr);

		if (!ok)
		{
			CloseHandle(hStdOutRd);
			return false;
		}

		// Read until process exits and pipe is closed
		std::vector<char> buffer;
		buffer.reserve(8192);
		const DWORD chunk = 4096;
		for (;;)
		{
			char temp[chunk];
			DWORD read = 0;
			BOOL r = ReadFile(hStdOutRd, temp, chunk, &read, nullptr);
			if (!r || read == 0)
			{
				// may break early if no data; still wait for process
				if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0)
					break;
				continue;
			}
			buffer.insert(buffer.end(), temp, temp + read);
		}

		WaitForSingleObject(pi.hProcess, INFINITE);
		GetExitCodeProcess(pi.hProcess, &exitCode);

		CloseHandle(hStdOutRd);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);

		output = FromBytes(buffer);
		return true;
	}
#endif

	std::string ExecAndCapture(const std::string& cmd)
	{
		std::array<char, 1024> buf{  };
		std::string out;

#if defined(_WIN32)
		FILE* pipe = _popen(cmd.c_str(), "r");
#else
		FILE* pipe = popen(cmd.c_str(), "r");
#endif
		if (!pipe) return "ERROR";
		while (fgets(buf.data(), buf.size(), pipe))
			out.append(buf.data());

#if defined(_WIN32)
		_pclose(pipe);
#else
		pclose(pipe);
#endif
		return out;
	}

	fs::path FindVsWhere()
	{
		auto pf86 = GetEnv("ProgramFiles(x86)");
		if (pf86.empty()) return {};
		fs::path p = fs::path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
		return fs::exists(p) ? p : fs::path();
	}

	std::string TryFindMSBuildWithVsWhere()
	{
#if !defined(_WIN32)
		return {};
#else
		auto vsw = FindVsWhere();
		if (vsw.empty()) return {};

		DWORD code = DWORD(-1);
		std::string out;

		const std::wstring exe = vsw.wstring();
		std::vector<std::wstring> args = {
			L"-latest",
			L"-requires", L"Microsoft.Component.MSBuild",
			L"-find", L"MSBuild\\**\\Bin\\MSBuild.exe"
		};

		if (!RunProcessCapture(exe, args, code, out) || code != 0 || out.empty())
			return {};

		std::istringstream iss(out);
		std::string line;
		while (std::getline(iss, line))
		{
			line = Trim(line);
			if (!line.empty() && fs::exists(line))
				return line;
		}
		return {};
#endif
	}

	std::string TryFindMSBuildInKnownPaths()
	{
		// VS 2022 common installations
		const std::vector<std::string> bases = {
			GetEnv("ProgramFiles") + "\\Microsoft Visual Studio\\2022\\Community",
			GetEnv("ProgramFiles") + "\\Microsoft Visual Studio\\2022\\Professional",
			GetEnv("ProgramFiles") + "\\Microsoft Visual Studio\\2022\\Enterprise",
			GetEnv("ProgramFiles") + "\\Microsoft Visual Studio\\2022\\BuildTools"
		};

		for (const auto& base : bases)
		{
			if (base.empty()) continue;

			// Prefer Current\Bin\MSBuild.exe, fallback to Bin\amd64
			fs::path p1 = fs::path(base) / "MSBuild" / "Current" / "Bin" / "MSBuild.exe";
			if (fs::exists(p1)) return p1.string();

			fs::path p2 = fs::path(base) / "MSBuild" / "Current" / "Bin" / "amd64" / "MSBuild.exe";
			if (fs::exists(p2)) return p2.string();
		}
		return {};
	}
}

void Ermine::scripting::ScriptEngine::InitMono(const std::string& assembly_path)
{
	EE_CORE_TRACE("Init Mono...");

	fs::path base = fs::current_path() / "mono";
	fs::path lib = base / "lib";
	fs::path etc = base / "etc";
	fs::path fx45 = lib / "4.5";

	bool existsAll = true;
	if (!fs::exists(lib)) { EE_CORE_ERROR("Mono lib directory missing: {0}", lib.string());   existsAll = false; }
	if (!fs::exists(etc)) { EE_CORE_ERROR("Mono etc directory missing: {0}", etc.string());   existsAll = false; }
	if (!fs::exists(fx45)) { EE_CORE_ERROR("Mono 4.5 profile missing: {0}", fx45.string());    existsAll = false; }
	assert(existsAll && "Mono asset directories must exist (./mono/lib, ./mono/etc, ./mono/lib/4.5)");

	mono_set_dirs(lib.string().c_str(), etc.string().c_str());
	mono_set_assemblies_path(fx45.string().c_str());

	m_coreDomain = mono_jit_init_version("ErmineCore", "v4.0.30319");
	if (!m_coreDomain)
	{
		EE_CORE_ERROR("Failed to initialize Mono Core Domain");
		return;
	}
	char gdn[] = "ErmineGame";
	m_gameDomain = mono_domain_create_appdomain(gdn, nullptr);
	mono_domain_set(m_gameDomain, true);

	// Loading engine API assembly
	m_apiAssemblyPath = assembly_path;
	m_apiAsm = LoadCSharpAssembly(assembly_path);
	if (!m_apiAsm)
	{
		EE_CORE_ERROR("Failed to load API assembly: {0}", assembly_path);
		return;
	}
	PrintAssemblyTypes(m_apiAsm);

	RegisterInternalCalls();

	ResolveMSBuildPath();
}

void Ermine::scripting::ScriptEngine::Shutdown()
{
	EE_CORE_TRACE("Shutdown Mono...");

	if (!m_coreDomain)
		return;

	mono_jit_cleanup(m_coreDomain);
	m_coreDomain = nullptr;
}

void Ermine::scripting::ScriptEngine::PrintAssemblyTypes(MonoAssembly* assembly)
{
	MonoImage* image = mono_assembly_get_image(assembly);
	const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
	int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

	for (int32_t i = 0; i < numTypes; i++)
	{
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

		const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
		const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

		EE_CORE_TRACE("{0}.{1}", nameSpace, name);
	}
}

char* Ermine::scripting::ScriptEngine::ReadBytes(const std::string& filepath, uint32_t* outSize)
{
	std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

	if (!stream)
	{
		// Failed to open the file
		EE_CORE_WARN("Failed to open the file {0}", filepath);
		return nullptr;
	}

	std::streampos end = stream.tellg();
	stream.seekg(0, std::ios::beg);
	long long size = end - stream.tellg();

	if (size == 0)
	{
		// File is empty
		EE_CORE_WARN("Files is empty! {0}", filepath);
		return nullptr;
	}

	char* buffer = new char[size];
	stream.read((char*)buffer, size);
	stream.close();

	*outSize = (uint32_t)size;
	return buffer;
}

MonoAssembly* Ermine::scripting::ScriptEngine::LoadCSharpAssembly(const std::string& assemblyPath)
{
	uint32_t fileSize = 0;
	char* fileData = ReadBytes(assemblyPath, &fileSize);

	// NOTE: We can't use this image for anything other than loading the assembly because this image doesn't have a reference to the assembly
	MonoImageOpenStatus status;
	MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

	if (status != MONO_IMAGE_OK)
	{
		const char* errorMessage = mono_image_strerror(status);
		EE_CORE_ERROR("{0}", errorMessage);
		return nullptr;
	}

	MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.c_str(), &status, 0);
	mono_image_close(image);

	// Don't forget to free the file data
	delete[] fileData;

	return assembly;
}

MonoAssembly* Ermine::scripting::ScriptEngine::LoadGameAssembly(const std::string& assemblyPath)
{
	m_gameAssemblyPath = assemblyPath;

	if (m_gameAsm) // If we already have a game assembly loaded, close it first
	{
		mono_assembly_close(m_gameAsm);
		m_gameAsm = nullptr;
	}

	m_gameAsm = LoadCSharpAssembly(assemblyPath);
	if (!m_gameAsm)
	{
		EE_CORE_ERROR("Failed to load game assembly: {0}", assemblyPath);
		assert(false && "Failed to load game assembly, Missing DLL?");
		return nullptr;
	}

	EE_CORE_TRACE("Game assembly loaded: {0}", assemblyPath);
	PrintAssemblyTypes(m_gameAsm);
	return m_gameAsm;
}

void Ermine::scripting::ScriptEngine::ReloadGameAssembly()
{
	if (m_gameAssemblyPath.empty())
	{
		EE_CORE_WARN("Cannot reload game assembly: No path specified");
		return;
	}

	mono_domain_set(m_gameDomain, false);

	LoadGameAssembly(m_gameAssemblyPath);
}

#pragma region File Watcher

void Ermine::scripting::ScriptEngine::StartWatchingGameAssembly()
{
	if (m_watching.exchange(true))
		return; // Already watching

	if (m_gameAssemblyPath.empty())
	{
		EE_CORE_WARN("StartWatchingGameAssembly called with empty game assembly path.");
		m_watching.store(false);
		return;
	}

	EE_CORE_INFO("ScriptEngine: Watching for changes: {0}", m_gameAssemblyPath);
	InitializeDllTimestamp();
	ScheduleWatcherTick();
}

void Ermine::scripting::ScriptEngine::StopWatchingGameAssembly()
{
	m_watching.store(false);
}

void Ermine::scripting::ScriptEngine::ScheduleSourceWatcherTick()
{
	if (!m_watching.load())
		return; // Not watching

	job::Declaration d{
	&ScriptEngine::SourceWatcherJobEntry,
	reinterpret_cast<uintptr_t>(this),
	job::Priority::LOW
	};
	job::KickJob(d);
}

void Ermine::scripting::ScriptEngine::ProcessHotReload(const std::function<void()>& pre,
	const std::function<void(bool success)>& post)
{
	if (!m_reloadRequested.exchange(false))
		return; // No reload requested

	EE_CORE_INFO("HotReload: Processing pending reload...");

	if (pre) pre();
	bool ok = ReloadDomainsAndAssemblies();
	if (post) post(ok);

	EE_CORE_INFO("HotReload: Done (success={0})", ok ? "true" : "false");
}

bool Ermine::scripting::ScriptEngine::ReloadDomainsAndAssemblies()
{
	if (m_apiAssemblyPath.empty() || m_gameAssemblyPath.empty())
	{
		EE_CORE_WARN("Reload requested but paths are not set. API='{0}' Game='{1}'",
			m_apiAssemblyPath, m_gameAssemblyPath);
		return false;
	}

	EE_CORE_TRACE("HotReload: Swapping AppDomain...");

	mono_domain_set(m_coreDomain, false);

	if (m_gameDomain)
	{
		mono_domain_unload(m_gameDomain);
		m_gameDomain = nullptr;
		m_gameAsm = nullptr;
	}
	char gdn[] = "ErmineGame";
	m_gameDomain = mono_domain_create_appdomain(gdn, nullptr);
	if (!m_gameDomain)
	{
		EE_CORE_ERROR("HotReload: Failed to create new game AppDomain");
		return false;
	}
	mono_domain_set(m_gameDomain, true);

	m_apiAsm = LoadCSharpAssembly(m_apiAssemblyPath);
	if (!m_apiAsm)
	{
		EE_CORE_ERROR("HotReload: Failed to reload API assembly: {0}", m_apiAssemblyPath);
		return false;
	}

	RegisterInternalCalls();

	constexpr int maxAttempts = 20;
	for (int attempt = 0; attempt < maxAttempts; ++attempt)
	{
		m_gameAsm = LoadCSharpAssembly(m_gameAssemblyPath);
		if (m_gameAsm)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	if (!m_gameAsm)
	{
		EE_CORE_ERROR("HotReload: Failed to reload Game assembly after retries: {0}", m_gameAssemblyPath);
		return false;
	}

	EE_CORE_INFO("HotReload: Reloaded assemblies successfully");
	InitializeDllTimestamp();
	return true;
}

void Ermine::scripting::ScriptEngine::InitializeDllTimestamp()
{
	try
	{
		if (!m_gameAssemblyPath.empty() && fs::exists(m_gameAssemblyPath))
			m_lastWriteTime = fs::last_write_time(m_gameAssemblyPath);
	}
	catch (...) {}
}

void Ermine::scripting::ScriptEngine::WatcherJobEntry(uintptr_t param)
{
	auto* self = reinterpret_cast<ScriptEngine*>(param);
	if (!self) return;
	if (!self->m_watching.load()) return;
	self->WatcherTick();
	self->ScheduleWatcherTick(); // Re-schedule if still active
}

void Ermine::scripting::ScriptEngine::WatcherTick()
{
	try
	{
		if (m_gameAssemblyPath.empty() || !fs::exists(m_gameAssemblyPath))
			return;

		auto t = fs::last_write_time(m_gameAssemblyPath);
		if (t != m_lastWriteTime)
		{
			// Debounce: wait a bit to ensure file write is complete
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			auto t2 = fs::last_write_time(m_gameAssemblyPath);
			if (t2 != m_lastWriteTime)
			{
				m_lastWriteTime = t2;
				EE_CORE_INFO("ScriptEngine: Detected change in {0}", m_gameAssemblyPath);
				m_reloadRequested.store(true);
			}
		}
	}
	catch (...) {}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

bool Ermine::scripting::ScriptEngine::ResolveMSBuildPath()
{
	if (!m_msbuildExe.empty())
	{
		fs::path p(m_msbuildExe);
		if (p.is_absolute() && fs::exists(p))
			return true; // Already set and exists
	}

	// Try vswhere first
	if (auto msb = TryFindMSBuildWithVsWhere(); !msb.empty())
	{
		m_msbuildExe = msb;
		EE_CORE_INFO("MSBuild resolved via vswhere: {0}", m_msbuildExe);
		return true;
	}
	// Try known paths
	if (auto msb = TryFindMSBuildInKnownPaths(); !msb.empty())
	{
		m_msbuildExe = msb;
		EE_CORE_INFO("MSBuild resolved via KnownPaths: {0}", m_msbuildExe);
		return true;
	}

	EE_CORE_WARN("Failed to resolve MSBuild.exe path. Please ensure it's in PATH or specify it manually OR run powershell script in main directory");
	return false;
}

void Ermine::scripting::ScriptEngine::ConfigureBuild(const std::string& csprojPath, const std::string& sourceRoot, const std::string& buildDLLPath, const std::string& configuration, const std::string& platform, const std::string& msbuildExe)
{
	m_csprojPath = csprojPath;
	m_sourceRoot = sourceRoot;
	m_buildDLLPath = buildDLLPath;
	m_buildConfiguration = configuration;
	m_buildPlatform = platform;

	if (!msbuildExe.empty() && fs::exists(msbuildExe)) m_msbuildExe = msbuildExe;
	else ResolveMSBuildPath();

	// initial timestamp
	try
	{
		std::filesystem::file_time_type latest{};
		if (!m_sourceRoot.empty() && fs::exists(m_sourceRoot))
		{
			for (auto& e : fs::recursive_directory_iterator(m_sourceRoot))
			{
				if (!e.is_regular_file()) continue;
				auto path = e.path();
				auto ext = path.extension().string();
				if (ext == ".cs" || ext == ".csproj")
					latest = std::max(latest, fs::last_write_time(path));
			}
		}
		if (!m_csprojPath.empty() && fs::exists(m_csprojPath))
			latest = std::max(latest, fs::last_write_time(m_csprojPath));

		m_lastSourcesStamp = latest;
	}
	catch (...) {}
}

void Ermine::scripting::ScriptEngine::StartWatchingScriptSources()
{
	if (m_sourceWatching.exchange(true))
		return; // Already watching

	if (m_csprojPath.empty() || m_sourceRoot.empty())
	{
		EE_CORE_WARN("StartWatchingScriptSources called with empty csproj path or source root.");
		m_sourceWatching.store(false);
		return;
	}

	EE_CORE_INFO("ScriptEngine: Watching sources: {0}", m_sourceRoot);
	ScheduleSourceWatcherTick();
}

void Ermine::scripting::ScriptEngine::StopWatchingScriptSources()
{
	m_sourceWatching.store(false);
}

void Ermine::scripting::ScriptEngine::ScheduleWatcherTick()
{
	if (!m_watching.load())
		return; // Not watching

	job::Declaration d
	{
		&ScriptEngine::WatcherJobEntry,
		reinterpret_cast<uintptr_t>(this),
		job::Priority::LOW
	};
	KickJob(d);
}

void Ermine::scripting::ScriptEngine::SourceWatcherJobEntry(uintptr_t param)
{
	auto* self = reinterpret_cast<ScriptEngine*>(param);
	if (!self || !self->m_sourceWatching.load()) return;
	self->SourceWatcherTick();
	self->ScheduleSourceWatcherTick();
}

void Ermine::scripting::ScriptEngine::SourceWatcherTick()
{
	std::filesystem::file_time_type latest{};
	try
	{
		if (!m_sourceRoot.empty() && fs::exists(m_sourceRoot))
		{
			for (auto& e : fs::recursive_directory_iterator(m_sourceRoot))
			{
				if (!e.is_regular_file()) continue;
				const auto& path = e.path();
				auto ext = path.extension().string();
				if (ext == ".cs" /*|| ext == ".csproj"*/)
				{
					//EE_CORE_TRACE("Before : {0}", FormatFileTime(latest));
					latest = std::max(latest, fs::last_write_time(path));
					//EE_CORE_TRACE("After : {0}", FormatFileTime(latest));
				}
			}
		}
		if (!m_csprojPath.empty() && fs::exists(m_csprojPath))
			latest = std::max(latest, fs::last_write_time(m_csprojPath));
	}
	catch (...) {}

	// Debounce and trigger build once
	if (latest != std::filesystem::file_time_type{} && latest != m_lastSourcesStamp)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		m_lastSourcesStamp = latest;

		// Request a build if not already running
		if (!m_buildInProgress.load())
		{
			EE_CORE_TRACE("Request build on the way!");
			m_buildRequested.store(true);

			job::Declaration b{
			&ScriptEngine::BuildJobEntry,
			reinterpret_cast<uintptr_t>(this),
			job::Priority::NORMAL };
			job::KickJob(b);
		}
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void Ermine::scripting::ScriptEngine::BuildJobEntry(uintptr_t param)
{
	auto* self = reinterpret_cast<ScriptEngine*>(param);
	if (!self)
	{
		EE_CORE_WARN("Param cast failed! No build done");
		return;
	}

	if (!self->m_buildRequested.exchange(false))
	{
		EE_CORE_WARN("No build requested!");
		return; // No build requested
	}

	if (self->m_buildInProgress.exchange(true))
	{
		EE_CORE_WARN("Return out of build entry, build in progress");
		return;
	}

	const bool ok = self->BuildGameAssembly();
	self->m_buildInProgress.store(false);

	if (ok)
		EE_CORE_INFO("ScriptEngine: Build completed successfully.");
	else
		EE_CORE_ERROR("ScriptEngine: Build failed!");
}

bool Ermine::scripting::ScriptEngine::BuildGameAssembly()
{
	if (m_csprojPath.empty())
	{
		EE_CORE_ERROR("BuildGameAssembly: csproj path is empty.");
		return false;
	}

	ResolveMSBuildPath(); // Ensure we have MSBuild path

#if defined(_WIN32)
	std::vector<std::wstring> args;
	args.emplace_back(ToWide(m_csprojPath));
	args.emplace_back(L"/t:Build");
	args.emplace_back(ToWide("/p:Configuration=" + m_buildConfiguration));
	args.emplace_back(ToWide("/p:Platform=" + m_buildPlatform));
	args.emplace_back(L"/nologo");
	args.emplace_back(L"/verbosity:minimal");

	const std::wstring msbuildExeW = ToWide(m_msbuildExe);
	DWORD exitCode = DWORD(-1);
	std::string out;

	EE_CORE_INFO("ScriptEngine: Building with: {0}", m_msbuildExe);
	if (!RunProcessCapture(msbuildExeW, args, exitCode, out))
	{
		EE_CORE_ERROR("Failed to start MSBuild process.");
		return false;
	}

	if (exitCode != 0)
	{
		EE_CORE_ERROR("MSBuild failed with code {0}", exitCode);
		if (!out.empty())
			EE_CORE_WARN("MSBuild output:\n{0}", out);
		return false;
	}
#else
	EE_CORE_ERROR("BuildGameAssembly only implemented for Windows.");
	return false
#endif
		if (m_gameAssemblyPath.empty())
		{
			EE_CORE_WARN("Build succeeded, but game assembly path is empty; skip copy.");
			return true;
		}

	return true;
}

#pragma endregion

namespace
{
	// Caching
	MonoImage* s_APIImage = nullptr;
	MonoClass* s_GameObjectClass = nullptr;
	MonoClass* s_TransformClass = nullptr;
	MonoClass* s_RigidbodyClass = nullptr;
	MonoClass* s_MonoBehaviourClass = nullptr;
	MonoClass* s_ComponentClass = nullptr;
	MonoClass* s_AudioComponentClass = nullptr;
	MonoClass* s_SerializeFieldAttr = nullptr;
	MonoClass* s_AnimatorClass = nullptr;
	MonoClass* s_MaterialClass = nullptr;
	MonoClass* s_UIImageComponentClass = nullptr;

	MonoClass* s_ObjectClass = nullptr;
	MonoClassField* s_EntityIDField = nullptr;

	// Discover field for a script instance
	struct ScriptFieldInfo
	{
		std::string name;
		MonoClassField* field = nullptr;
		enum class Kind { Float, Int, Bool, String, Vector3, Quaternion, Unsupported } kind = Kind::Unsupported;
	};

	void ToTempUTF8(MonoString* str, std::string& out)
	{
		if (!str)
		{
			out.clear();
			return;
		}
		char* raw = mono_string_to_utf8(str);
		out = raw ? raw : "";
		if (raw)
			mono_free(raw);
	}

	MonoClassField* FindFieldInHierarchy(MonoClass* klass, const char* name)
	{
		if (!klass || !name) return nullptr;
		for (MonoClass* c = klass; c; c = mono_class_get_parent(c))
		{
			mono_class_init(c);

			if (MonoClassField* f = mono_class_get_field_from_name(c, name))
				return f;
		}

		return nullptr;
	}

	Ermine::EntityID GetEntityIDFromManaged(MonoObject* obj)
	{
		if (!obj || !s_EntityIDField) return 0;
		Ermine::EntityID id = 0;
		mono_field_get_value(obj, s_EntityIDField, &id);
		return id;
		//MonoClass* klass = mono_object_get_class(obj);
		//if (!klass) return 0;

		//if (MonoClassField* field = FindFieldInHierarchy(klass, "EntityID"))
		//{
		//	Ermine::EntityID id = 0;
		//	mono_field_get_value(obj, field, &id);
		//	return id;
		//}
		//return 0;
	}

	void SetEntityIDOnManaged(MonoObject* obj, Ermine::EntityID id)
	{
		if (!obj || !s_EntityIDField) return;
		mono_field_set_value(obj, s_EntityIDField, &id);
		//MonoClass* klass = mono_object_get_class(obj);
		//if (!klass) return;

		//if (MonoClassField* field = FindFieldInHierarchy(klass, "EntityID"))
		//	mono_field_set_value(obj, field, &id);
	}

	MonoClass* GetAPIClass(const char* nameSpace, const char* name)
	{
		if (!s_APIImage)
			return nullptr;
		return mono_class_from_name(s_APIImage, nameSpace, name);
	}

	bool IsExposedField(MonoClass* owner, MonoClassField* field)
	{
		const unsigned int flags = mono_field_get_flags(field);
		const bool isPublic = (flags & FIELD_ACCESS_MASK) == FIELD_ATTRIBUTE_PUBLIC; // FIELD_ATTRIBUTE_PUBLIC 0x0006
		if (isPublic) return true;

		if (!s_SerializeFieldAttr) return false;

		if (MonoCustomAttrInfo* ca = mono_custom_attrs_from_field(owner, field))
		{
			const bool has = mono_custom_attrs_has_attr(ca, s_SerializeFieldAttr);
			mono_custom_attrs_free(ca);
			if (has) return true;
		}

		return false;
	}

	ScriptFieldInfo::Kind Classify(MonoType* t)
	{
		switch (mono_type_get_type(t))
		{
		case MONO_TYPE_R4: return ScriptFieldInfo::Kind::Float;
		case MONO_TYPE_I4: return ScriptFieldInfo::Kind::Int;
		case MONO_TYPE_BOOLEAN: return ScriptFieldInfo::Kind::Bool;
		case MONO_TYPE_STRING: return ScriptFieldInfo::Kind::String;
		case MONO_TYPE_VALUETYPE:
		{
			MonoClass* c = mono_class_from_mono_type(t);
			const char* ns = mono_class_get_namespace(c);
			const char* nm = mono_class_get_name(c);
			if (ns && nm && std::strcmp(ns, "ErmineEngine") == 0)
				// TODO: This grows for unique types exposed field
			{
				if (std::strcmp(nm, "Vector3") == 0) return ScriptFieldInfo::Kind::Vector3;
				if (std::strcmp(nm, "Quaternion") == 0) return ScriptFieldInfo::Kind::Quaternion;
			}
			break;
		}
		default: break;
		}
		return ScriptFieldInfo::Kind::Unsupported;
	}

	std::vector<ScriptFieldInfo> DiscoverScriptFields(MonoClass* klass)
	{
		std::vector<ScriptFieldInfo> out;
		void* iter = nullptr;
		while (MonoClassField* f = mono_class_get_fields(klass, &iter))
		{
			if (!IsExposedField(klass, f)) continue;

			MonoType* ft = mono_field_get_type(f);
			auto kind = Classify(ft);
			if (kind == ScriptFieldInfo::Kind::Unsupported) continue;

			const char* fname = mono_field_get_name(f);
			out.push_back(ScriptFieldInfo{ .name = fname ? fname : "", .field = f, .kind = kind });
		}

		return out;
	}

	bool IsSubclassOf(MonoClass* klass, MonoClass* parentKlass)
	{
		if (!klass || !parentKlass)
			return false;
		MonoClass* current = klass;
		while (current)
		{
			if (current == parentKlass) return true;
			current = mono_class_get_parent(current);
		}
		return false;
	}

	MonoObject* CreateManagedGameObjectWrapper(Ermine::EntityID id)
	{
		if (!s_GameObjectClass)
			return nullptr;
		auto* dom = Ermine::ECS::GetInstance().GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();
		MonoObject* obj = mono_object_new(dom, s_GameObjectClass); // Creating Gameobject object on managed side
		// Don't call mono_runtime_object_init (its ctor would create a new native entity)
		SetEntityIDOnManaged(obj, id); // ID is from the native side, it when script was created
		return obj;
	}

	MonoObject* CreateManagedTransformWrapper(Ermine::EntityID id)
	{
		if (!s_TransformClass)
			return nullptr;
		auto* dom = Ermine::ECS::GetInstance().GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();
		MonoObject* obj = mono_object_new(dom, s_TransformClass);
		mono_runtime_object_init(obj);
		SetEntityIDOnManaged(obj, id);
		return obj;
	}

	MonoObject* CreateManagedRigidbodyWrapper(Ermine::EntityID id)
	{
		if (!s_RigidbodyClass)
		{
			assert(false && "Missing RigidbodyClass");
			return nullptr;
		}
		auto* dom = Ermine::ECS::GetInstance().GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();
		MonoObject* obj = mono_object_new(dom, s_RigidbodyClass);
		mono_runtime_object_init(obj);
		SetEntityIDOnManaged(obj, id);
		return obj;
	}

	void SetComponentGameObject(MonoObject* componentObj, Ermine::EntityID id)
	{
		if (!componentObj || !s_ComponentClass || !s_GameObjectClass)
			return;
		MonoMethod* setGO = mono_class_get_method_from_name(s_ComponentClass, "set_gameObject", 1);
		if (!setGO)
			return;

		MonoObject* goWrapper = CreateManagedGameObjectWrapper(id);
		void* args[1] = { goWrapper };
		mono_runtime_invoke(setGO, componentObj, args, nullptr);
	}

	Ermine::Vec3 NormalizeSafe(const Ermine::Vec3& v, const Ermine::Vec3& fallback)
	{
		const float lsq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (lsq > 1e-12f) {
			const float inv = 1.0f / std::sqrt(lsq);
			return { v.x * inv, v.y * inv, v.z * inv };
		}
		return fallback;
	}

	Ermine::Vec3 RotateByQuat(const Ermine::Vec3& v, Ermine::Quaternion q)
	{
		// Normalize quaternion
		float qx = q.x, qy = q.y, qz = q.z, qw = q.w;
		const float ql = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
		if (ql > 1e-12f) { qx /= ql; qy /= ql; qz /= ql; qw /= ql; }
		else return v;

		// v' = v + q_w * t + cross(q_xyz, t), t = 2 * cross(q_xyz, v)
		const float vx = v.x, vy = v.y, vz = v.z;
		const float tx = 2.0f * (qy * vz - qz * vy);
		const float ty = 2.0f * (qz * vx - qx * vz);
		const float tz = 2.0f * (qx * vy - qy * vx);
		const float cx = qy * tz - qz * ty;
		const float cy = qz * tx - qx * tz;
		const float cz = qx * ty - qy * tx;
		return { vx + qw * tx + cx, vy + qw * ty + cy, vz + qw * tz + cz };
	}

	struct ManagedVector2 { float x, y; };
	struct ManagedVector3 { float x, y, z; };
	struct ManagedQuaternion { float x, y, z, w; };
	struct ManagedMatrix4x4 {
		float m00, m01, m02, m03,
			m10, m11, m12, m13,
			m20, m21, m22, m23,
			m30, m31, m32, m33;
	};
	struct ManagedRaycastHit
	{
		ManagedVector3 point; // impact point in world space where the ray hit the collider
		ManagedVector3 normal; // the normal of the surface the ray hit
		float distance; // the distance from the ray's origin to the impact point
		uint64_t entityID; // the EntityID of the collider that was hit
	};
	ManagedVector3 ToManagedVec(const Ermine::Vec3& v) { return { v.x, v.y, v.z }; }
	ManagedVector2 ToManagedVec2(const Ermine::Vec2& v) { return { v.x, v.y }; }
	ManagedQuaternion ToManagedQuat(const Ermine::Quaternion& q) { return { q.x, q.y, q.z, q.w }; }
	ManagedMatrix4x4 ToManagedMatrix4x4(const Ermine::Matrix4x4& m4x4) {
		return { m4x4.m00, m4x4.m01, m4x4.m02, m4x4.m03,
					m4x4.m10, m4x4.m11, m4x4.m12, m4x4.m13,
					m4x4.m20, m4x4.m21, m4x4.m22, m4x4.m23,
					m4x4.m30, m4x4.m31, m4x4.m32, m4x4.m33 };
	}

	Ermine::Vec3 ToNativeVec(const ManagedVector3& v) { return { v.x, v.y, v.z }; }
	Ermine::Vec2 ToNativeVec2(const ManagedVector2& v) { return { v.x, v.y }; }
	Ermine::Quaternion ToNativeQuat(const ManagedQuaternion& q) { return { q.x, q.y, q.z, q.w }; }
	Ermine::Matrix4x4 ToNativeMatrix4x4(const ManagedMatrix4x4& m4x4) {
		return { m4x4.m00, m4x4.m01, m4x4.m02, m4x4.m03,
					m4x4.m10, m4x4.m11, m4x4.m12, m4x4.m13,
					m4x4.m20, m4x4.m21, m4x4.m22, m4x4.m23,
					m4x4.m30, m4x4.m31, m4x4.m32, m4x4.m33 };
	}

	Ermine::Transform* GetTransformFromManaged(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
		{
			EE_CORE_WARN("Cannot get Transform for {0} ; either not valid or doesn't have transform component!", id);
			return nullptr;
		}
		return &ECS::GetInstance().GetComponent<Transform>(id);
	}

	Ermine::UIImageComponent* GetUIImageComponentFromManaged(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<UIImageComponent>(id))
		{
			EE_CORE_WARN("Cannot get UIImageComponent for {0}; either not valid or doesn't have UIImageComponent!", id);
			return nullptr;
		}
		return &ECS::GetInstance().GetComponent<UIImageComponent>(id);
	}

	ManagedVector3 icall_uiimage_get_position(MonoObject* thisObj)
	{
		if (auto* ui = GetUIImageComponentFromManaged(thisObj))
			return ToManagedVec(ui->position);

		return { 0.5f, 0.5f, 0.0f };
	}

	void icall_uiimage_set_position(MonoObject* thisObj, ManagedVector3 value)
	{
		if (auto* ui = GetUIImageComponentFromManaged(thisObj))
			ui->position = ToNativeVec(value);
	}

#pragma region Transform ICalls
	ManagedVector3 icall_transform_get_position(MonoObject* thisObj)
	{
		if (auto* t = GetTransformFromManaged(thisObj))
			return ToManagedVec(t->position);
		EE_CORE_WARN("Transform for {0} failed to get unmanaged position", GetEntityIDFromManaged(thisObj));
		return { 0,0,0 };
	}

	ManagedQuaternion icall_transform_get_rotation(MonoObject* thisObj)
	{
		if (auto* t = GetTransformFromManaged(thisObj))
			return ToManagedQuat(t->rotation);
		EE_CORE_WARN("Transform for {0} failed to get unmanaged rotation", GetEntityIDFromManaged(thisObj));
		return { .x = 0, .y = 0, .z = 0, .w = 1.0f };
	}

	ManagedVector3 icall_transform_get_scale(MonoObject* thisObj)
	{
		if (auto* t = GetTransformFromManaged(thisObj))
			return ToManagedVec(t->scale);
		EE_CORE_WARN("Transform for {0} failed to get unmanaged scale", GetEntityIDFromManaged(thisObj));
		return { 1,1,1 };
	}

	ManagedVector3 icall_transform_get_world_forward(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return { 0.f, 0.f, 1.f };

		// Prefer GlobalTransform basis column 2 (+Z forward)
		if (ECS::GetInstance().HasComponent<GlobalTransform>(id)) {
			const auto& gt = ECS::GetInstance().GetComponent<GlobalTransform>(id);
			Ermine::Vec3 f{ gt.worldMatrix.m02, gt.worldMatrix.m12, gt.worldMatrix.m22 };
			f = NormalizeSafe(f, Ermine::Vec3{ 0.f, 0.f, 1.f });
			return ToManagedVec(f);
		}

		// Try HierarchySystem's world rotation
		Quaternion worldRot{};
		if (auto hs = ECS::GetInstance().GetSystem<HierarchySystem>()) {
			worldRot = hs->GetWorldRotation(id);
		}
		else {
			worldRot = ECS::GetInstance().GetComponent<Transform>(id).rotation;
		}
		Ermine::Vec3 fwd = RotateByQuat(Ermine::Vec3{ 0.f, 0.f, 1.f }, worldRot);
		fwd = NormalizeSafe(fwd, Ermine::Vec3{ 0.f, 0.f, 1.f });
		return ToManagedVec(fwd);
	}

	ManagedVector3 icall_transform_get_world_right(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return { 1.f, 0.f, 0.f };

		if (ECS::GetInstance().HasComponent<GlobalTransform>(id)) {
			const auto& gt = ECS::GetInstance().GetComponent<GlobalTransform>(id);
			Ermine::Vec3 r{ gt.worldMatrix.m00, gt.worldMatrix.m10, gt.worldMatrix.m20 };
			r = NormalizeSafe(r, Ermine::Vec3{ 1.f, 0.f, 0.f });
			return ToManagedVec(r);
		}

		Quaternion worldRot{};
		if (auto hs = ECS::GetInstance().GetSystem<HierarchySystem>()) {
			worldRot = hs->GetWorldRotation(id);
		}
		else {
			worldRot = ECS::GetInstance().GetComponent<Transform>(id).rotation;
		}
		Ermine::Vec3 right = RotateByQuat(Ermine::Vec3{ 1.f, 0.f, 0.f }, worldRot);
		right = NormalizeSafe(right, Ermine::Vec3{ 1.f, 0.f, 0.f });
		return ToManagedVec(right);
	}

	ManagedVector3 icall_transform_get_world_up(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return { 0.f, 1.f, 0.f };

		if (ECS::GetInstance().HasComponent<GlobalTransform>(id)) {
			const auto& gt = ECS::GetInstance().GetComponent<GlobalTransform>(id);
			Ermine::Vec3 u{ gt.worldMatrix.m01, gt.worldMatrix.m11, gt.worldMatrix.m21 };
			u = NormalizeSafe(u, Ermine::Vec3{ 0.f, 1.f, 0.f });
			return ToManagedVec(u);
		}

		Quaternion worldRot{};
		if (auto hs = ECS::GetInstance().GetSystem<HierarchySystem>()) {
			worldRot = hs->GetWorldRotation(id);
		}
		else {
			worldRot = ECS::GetInstance().GetComponent<Transform>(id).rotation;
		}
		Ermine::Vec3 up = RotateByQuat(Ermine::Vec3{ 0.f, 1.f, 0.f }, worldRot);
		up = NormalizeSafe(up, Ermine::Vec3{ 0.f, 1.f, 0.f });
		return ToManagedVec(up);
	}

	void icall_transform_set_position(MonoObject* thisObj, ManagedVector3 value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
			return;

		// Use HierarchySystem to properly propagate transform changes
		std::shared_ptr<HierarchySystem> hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();
		if (hierarchySystem) {
			hierarchySystem->SetLocalPosition(id, ToNativeVec(value));
		}
		else {
			// Fallback if hierarchy system not available
			auto& transform = ECS::GetInstance().GetComponent<Transform>(id);
			transform.position = ToNativeVec(value);

			if (!ECS::GetInstance().HasComponent<GlobalTransform>(id))
				ECS::GetInstance().AddComponent<GlobalTransform>(id, GlobalTransform());

			if (ECS::GetInstance().HasComponent<GlobalTransform>(id)) {
				auto& gt = ECS::GetInstance().GetComponent<GlobalTransform>(id);
				gt.worldMatrix = transform.GetLocalMatrix();
				gt.isDirty = true;
			}
		}
	}

	void icall_transform_set_rotation(MonoObject* thisObj, ManagedQuaternion value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
			return;

		// Use HierarchySystem to properly propagate transform changes
		if (std::shared_ptr<HierarchySystem> hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>()) {
			hierarchySystem->SetLocalRotation(id, ToNativeQuat(value));
		}
		else {
			// Fallback if hierarchy system not available
			auto& transform = ECS::GetInstance().GetComponent<Transform>(id);
			transform.rotation = ToNativeQuat(value);
		}
	}

	void icall_transform_set_scale(MonoObject* thisObj, ManagedVector3 value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
			return;

		// Use HierarchySystem to properly propagate transform changes
		std::shared_ptr<HierarchySystem> hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();
		if (hierarchySystem) {
			hierarchySystem->SetLocalScale(id, ToNativeVec(value));
		}
		else {
			// Fallback if hierarchy system not available
			auto& transform = ECS::GetInstance().GetComponent<Transform>(id);
			transform.scale = ToNativeVec(value);
		}
	}

	int icall_transform_get_childcount(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		auto& ecs = ECS::GetInstance();
		if (id == 0 || !ecs.IsEntityValid(id) || !ecs.HasComponent<Transform>(id))
			return -1;

		auto hs = ecs.GetSystem<HierarchySystem>();
		return hs->GetChildCount(id);
	}

	MonoObject* icall_transform_get_transform_parent(uint64_t id)
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		if (id == 0 || !ecs.IsEntityValid(id) || !ecs.HasComponent<Transform>(id))
		{
			assert(false && "Miss Component here!");
			return nullptr;
		}

		auto hs = ecs.GetSystem<HierarchySystem>();
		auto parentID = hs->GetParent(id);

		MonoObject* obj = CreateManagedTransformWrapper(parentID);
		SetComponentGameObject(obj, parentID);
		return obj;
	}

	void icall_transform_add_child(MonoObject* thisObj, MonoObject* childObj)
	{
		using namespace Ermine;

		auto& ecs = ECS::GetInstance();

		EntityID parentID = GetEntityIDFromManaged(thisObj);
		EntityID childID = childObj ? GetEntityIDFromManaged(childObj) : 0;

		// Validate parent
		if (parentID == 0 || !ecs.IsEntityValid(parentID) || !ecs.HasComponent<Transform>(parentID))
			return;

		// Validate child
		if (childID == 0 || !ecs.IsEntityValid(childID) || !ecs.HasComponent<Transform>(childID))
			return;

		// Prevent self-parenting
		if (parentID == childID)
		{
			EE_CORE_WARN("Cannot add entity {} as a child of itself", parentID);
			return;
		}

		// Use HierarchySystem
		if (auto hs = ecs.GetSystem<HierarchySystem>())
		{
			hs->SetParent(childID, parentID);
		}
		else
		{
			EE_CORE_WARN("HierarchySystem not available. Failed to add child.");
		}
	}

	void icall_transform_remove_child(MonoObject* thisObj, MonoObject* childObj)
	{
		using namespace Ermine;

		auto& ecs = ECS::GetInstance();

		EntityID parentID = GetEntityIDFromManaged(thisObj);
		EntityID childID = childObj ? GetEntityIDFromManaged(childObj) : 0;

		// Validate parent
		if (parentID == 0 || !ecs.IsEntityValid(parentID) || !ecs.HasComponent<Transform>(parentID))
			return;

		// Validate child
		if (childID == 0 || !ecs.IsEntityValid(childID) || !ecs.HasComponent<Transform>(childID))
			return;

		if (auto hs = ecs.GetSystem<HierarchySystem>())
		{
			// Make sure this child actually belongs to this parent
			if (hs->GetParent(childID) != parentID)
			{
				EE_CORE_WARN("Entity {} is not a child of {}", childID, parentID);
				return;
			}

			// Unparent parent = 0 (world root)
			hs->SetParent(childID, 0);
		}
		else
		{
			EE_CORE_WARN("HierarchySystem not available. Failed to remove child.");
		}
	}



	MonoObject* icall_transform_get_transform_by_name([[maybe_unused]] MonoObject* thisObj, MonoString* name)
	{
		using namespace Ermine;
		if (!name)
		{
			EE_CORE_WARN("icall_transform_get_transform_by_name: name parameter is empty");
			assert(false && "Issue here!");
			return nullptr;
		}
		std::string fromMonoName;
		ToTempUTF8(name, fromMonoName);
		//ToTempUTF8(name, fromMonoName); // Maybe busy thread
		const auto& eList = SceneManager::GetInstance().GetActiveScene()->GetAllEntities();
		for (const auto& entity : eList)
		{
			auto& o = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
			if (o.name != fromMonoName)
				continue;
			MonoObject* obj = CreateManagedTransformWrapper(entity);
			SetComponentGameObject(obj, entity);
			return obj;
		}
		EE_CORE_WARN("No entity with {} found!", fromMonoName);
		assert(false && "Miss Component here!");
		return nullptr;
	}

	MonoObject* icall_transform_get_transform_by_index(MonoObject* thisObj,int index)
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		auto id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ecs.IsEntityValid(id) || !ecs.HasComponent<Transform>(id))
		{
			assert(false && "Miss Component here!");
			return nullptr;
		}

		auto hs = ecs.GetSystem<HierarchySystem>();
		if (hs->GetChildCount(id) <= 0)
		{
			std::string name = ecs.GetComponent<ObjectMetaData>(id).name;
			EE_CORE_WARN("Object {} has no children!", name);
			assert(false && "Miss Component here!");
			return nullptr;
		}

		auto& children = hs->GetChildren(id);
		MonoObject* obj = CreateManagedTransformWrapper(children[index]);
		SetComponentGameObject(obj, children[index]);
		return obj;
	}
	ManagedVector3 icall_transform_get_world_position(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		auto& ecs = ECS::GetInstance();

		if (id == 0 || !ecs.IsEntityValid(id))
			return { 0.f, 0.f, 0.f };

		if (auto hs = ecs.GetSystem<HierarchySystem>())
		{
			Ermine::Vec3 wp = hs->GetWorldPosition(id);
			return ToManagedVec(wp);
		}

		// fallback (no hierarchy system)
		if (ecs.HasComponent<Transform>(id))
			return ToManagedVec(ecs.GetComponent<Transform>(id).position);

		return { 0.f, 0.f, 0.f };
	}

	ManagedQuaternion icall_transform_get_world_rotation(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		auto& ecs = ECS::GetInstance();

		if (id == 0 || !ecs.IsEntityValid(id))
			return { .x = 0.f, .y = 0.f, .z = 0.f, .w = 1.f };

		if (auto hs = ecs.GetSystem<HierarchySystem>())
		{
			Quaternion wr = hs->GetWorldRotation(id);
			return ToManagedQuat(wr);
		}

		// fallback (no hierarchy system)
		if (ecs.HasComponent<Transform>(id))
			return ToManagedQuat(ecs.GetComponent<Transform>(id).rotation);

		return { .x = 0.f, .y = 0.f, .z = 0.f, .w = 1.f };
	}

	ManagedVector3 icall_transform_get_world_scale(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		auto& ecs = ECS::GetInstance();

		if (id == 0 || !ecs.IsEntityValid(id))
			return { 1.f, 1.f, 1.f };

		if (auto hs = ecs.GetSystem<HierarchySystem>())
			return ToManagedVec(hs->GetWorldScale(id));

		if (ecs.HasComponent<Transform>(id))
			return ToManagedVec(ecs.GetComponent<Transform>(id).scale);

		return { 1.f, 1.f, 1.f };
	}
#pragma endregion

#pragma region Rigidbody ICalls
	void icall_rigidbody_set_position(MonoObject* thisObj, ManagedVector3 value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicsComponent!");
			EE_CORE_WARN("Missing PhysicsComponent!");
			return;
		}

		auto physics = ecs.GetSystem<Physics>();
		physics->SetPosition(id, ToNativeVec(value));
	}

	ManagedVector3 icall_rigidbody_get_position(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicsComponent!");
			EE_CORE_WARN("Missing PhysicsComponent!");
			return {0,0,0};
		}
		
		auto physics = ecs.GetSystem<Physics>();
		return ToManagedVec(physics->GetPosition(id));
	}

	ManagedQuaternion icall_rigidbody_get_rotation(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicsComponent!");
			EE_CORE_WARN("Missing PhysicsComponent!");
			return { 0,0,0 };
		}

		auto physics = ecs.GetSystem<Physics>();
		return ToManagedQuat(physics->GetRotation(id));
	}

	void icall_rigidbody_set_rotation(MonoObject* thisObj, ManagedQuaternion value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicComponent!");
			EE_CORE_WARN("Missing PhysicComponent!");
			return;
		}

		auto physics = ecs.GetSystem<Physics>();
		physics->SetRotation(id, ToNativeQuat(value));
	}

	void icall_rigidbody_set_linear_velocity(MonoObject* thisObj, ManagedVector3 value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicComponent!");
			EE_CORE_WARN("Missing PhysicComponent!");
			return;
		}

		JPH::Body* body = ECS::GetInstance().GetComponent<PhysicComponent>(id).body;
		if (!body) return;


		auto nativeVec = ToNativeVec(value);
		body->SetLinearVelocity({nativeVec.x,nativeVec.y,nativeVec.z});
	}

	ManagedVector3 icall_rigidbody_get_linear_velocity(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);

		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<PhysicComponent>(id))
		{
			assert(false && "Missing PhysicComponent!");
			EE_CORE_WARN("Missing PhysicComponent!");
			return {0,0,0};
		}

		JPH::Body* body = ECS::GetInstance().GetComponent<PhysicComponent>(id).body;
		if (!body) return {0,0,0};

		JPH::Vec3 currentVel = body->GetLinearVelocity();
		return ToManagedVec(Ermine::Vec3{ currentVel.GetX(),currentVel.GetY(),currentVel.GetZ() });
	}
#pragma endregion

#pragma region Time ICalls
	float icall_time_get_deltatime() { return Ermine::FrameController::GetDeltaTime(); }
	float icall_time_get_fixeddeltatime() { return Ermine::FrameController::GetFixedDeltaTime(); }
#pragma endregion

#pragma region Input ICalls
	bool icall_input_getkey(int key)
	{
		return Ermine::Input::IsKeyPressed(key);
	}

	bool icall_input_getkeydown(int key)
	{
		return Ermine::Input::IsKeyDown(key);
	}

	bool icall_input_getkeyup(int key)
	{
		return Ermine::Input::IsKeyReleased(key);
	}

	bool icall_input_getmousebutton(int button)
	{
		return Ermine::Input::IsMouseButtonPressed(button);
	}

	bool icall_input_getmousebuttondown(int button)
	{
		return Ermine::Input::IsMouseButtonDown(button);
	}

	bool icall_input_getmousebuttonrelease(int button)
	{
		return Ermine::Input::IsMouseButtonReleased(button);
	}

	ManagedVector3 icall_input_get_mouseposition()
	{
		auto [x, y] = Ermine::Input::GetMousePosition();
		return ManagedVector3{ x, y, 0.0f };
	}

	ManagedVector3 icall_input_get_mousepositiondelta()
	{
		auto [dx, dy] = Ermine::Input::GetMouseDeltaGame();
		return ManagedVector3{ dx, dy, 0.0f };
	}
#pragma endregion

#pragma region Debug ICalls
	void icall_debug_log_info(MonoString* message)
	{
		std::string temp;
		ToTempUTF8(message, temp);
		EE_CORE_INFO("{}", temp); // TODO: Might have to swap to different channel
	}

	void icall_debug_log_warning(MonoString* message)
	{
		std::string temp;
		ToTempUTF8(message, temp);
		EE_CORE_WARN("{}", temp); // TODO: Might have to swap to different channel
	}

	void icall_debug_log_error(MonoString* message)
	{
		std::string temp;
		ToTempUTF8(message, temp);
		EE_CORE_ERROR("{}", temp); // TODO: Might have to swap to different channel
	}
#pragma endregion

#pragma region SceneManager ICalls
	void icall_scenemanager_loadscene(MonoString* scenePath)
	{
		using namespace Ermine;
		std::string path;
		ToTempUTF8(scenePath, path);

		if (!path.empty())
		{
			EE_CORE_INFO("SceneManager: Loading scene from script: {}", path);
			//SceneManager::GetInstance().OpenScene(path);
			SceneManager::GetInstance().RequestOpenScene(path);
		}
		else
		{
			EE_CORE_ERROR("SceneManager: Scene path is empty!");
		}
	}
#pragma endregion

#pragma region Application ICalls
	void icall_application_quit()
	{
		EE_CORE_INFO("Application: Quit requested from script");
		// Note: In editor, this won't actually quit
#if defined(EE_EDITOR)
		EE_CORE_WARN("Application.Quit() called in editor - this only works in game builds");
#else
		//// In game build, request window close
		//extern GLFWwindow* g_window; // Assume this is available globally
		//if (g_window)
		//{
		//	glfwSetWindowShouldClose(g_window, GLFW_TRUE);
		//}

		if (Ermine::Input::s_Window)
			Ermine::Window::ShouldCloseWindow(Ermine::Input::s_Window);
		else
			assert(false && "Cannot close due to input s_window not populated");
#endif
	}
#pragma endregion

#pragma region Object ICalls
	MonoString* icall_object_get_name(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
		{
			EE_CORE_WARN("Cannot get name for {0}; either not valid or doesn't have object meta data!", id);
			return mono_string_new(mono_domain_get(), "GameObject");
		}
		auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(id);
		return mono_string_new(mono_domain_get(), meta.name.c_str());
	}

	void icall_object_set_name(MonoObject* thisObj, MonoString* value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
		{
			EE_CORE_WARN("Cannot get name for {0}; either not valid or doesn't have object meta data!", id);
			return;
		}

		std::string temp;
		ToTempUTF8(value, temp);
		auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(id);
		meta.name = std::move(temp);
	}
#pragma endregion

#pragma region Component ICalls
	MonoString* icall_component_get_tag(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
		{
			EE_CORE_WARN("Cannot get tag for {0} ; either not valid or doesn't have object meta data!", id);
			return mono_string_new(mono_domain_get(), "unnamedTag");
		}
		auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(id);
		return mono_string_new(mono_domain_get(), meta.tag.c_str());
	}

	void icall_component_set_tag(MonoObject* thisObj, MonoString* value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
		{
			EE_CORE_WARN("Cannot set tag for {0} ; either not valid or doesn't have object meta data!", id);
			return;
		}

		std::string temp;
		ToTempUTF8(value, temp);
		auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(id);
		meta.tag = std::move(temp);
	}
#pragma endregion

#pragma region GlobalAudio ICalls
	void icall_globalaudio_play_sfx(MonoString* name)
	{
		using namespace Ermine;
		std::string sfxName;
		ToTempUTF8(name, sfxName);

		// Find GlobalAudioComponent entity
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.PlaySFX(sfxName);
				return;
			}
		}
		EE_CORE_WARN("GlobalAudio: No GlobalAudioComponent found in scene");
	}

	void icall_globalaudio_play_sfx_with_reverb(MonoString* name, mono_bool useReverb, float wetLevel, float dryLevel, float decayTime, float earlyDelay, float lateDelay)
	{
		using namespace Ermine;
		std::string sfxName;
		ToTempUTF8(name, sfxName);

		// Find GlobalAudioComponent entity
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				AudioSystem::PlayGlobalSFX(globalAudio, sfxName, useReverb != 0, wetLevel, dryLevel, decayTime, earlyDelay, lateDelay);
				return;
			}
		}
		EE_CORE_WARN("GlobalAudio: No GlobalAudioComponent found in scene");
	}

	void icall_globalaudio_play_music(MonoString* name)
	{
		using namespace Ermine;
		std::string musicName;
		ToTempUTF8(name, musicName);

		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				int index = globalAudio.GetMusicIndex(musicName);
				if (index >= 0)
					globalAudio.PlayMusic(index);
				return;
			}
		}
	}

	void icall_globalaudio_set_music_volume(float volume)
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.SetMusicVolume(volume);
				return;
			}
		}
	}

	void icall_globalaudio_set_sfx_volume(float volume)
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.SetSFXVolume(volume);
				return;
			}
		}
	}

	void icall_globalaudio_play_voice(MonoString* name)
	{
		using namespace Ermine;
		std::string voiceName;
		ToTempUTF8(name, voiceName);

		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.PlayVoice(voiceName);
				return;
			}
		}
		EE_CORE_WARN("GlobalAudio: No GlobalAudioComponent found in scene");
	}

	void icall_globalaudio_stop_voice()
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.StopVoice();
				return;
			}
		}
	}

	void icall_globalaudio_stop_sfx(MonoString* name)
	{
		using namespace Ermine;
		std::string sfxName;
		ToTempUTF8(name, sfxName);

		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.StopSFX(sfxName);
				return;
			}
		}
	}

	void icall_globalaudio_set_voice_volume(float volume)
	{
		using namespace Ermine;
		auto& ecs = ECS::GetInstance();
		for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
		{
			if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
			{
				auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
				globalAudio.SetVoiceVolume(volume);
				return;
			}
		}
	}
#pragma endregion

#pragma region AudioListener ICalls
	void icall_audiolistener_set_position(ManagedVector3 pos)
	{
		// Convert ManagedVector3 -> Vector3D
		Ermine::Vector3D position{ pos.x, pos.y, pos.z };
		Ermine::CAudioEngine::SetListenerPosition(position);
	}

	void icall_audiolistener_set_orientation(ManagedVector3 forward, ManagedVector3 up)
	{
		// Convert ManagedVector3 -> Vector3D
		Ermine::Vector3D fwd{ forward.x, forward.y, forward.z };
		Ermine::Vector3D upVec{ up.x, up.y, up.z };
		Ermine::CAudioEngine::SetListenerOrientation(fwd, upVec);
	}

	void icall_audiolistener_set_attributes(ManagedVector3 pos, ManagedVector3 velocity,
		ManagedVector3 forward, ManagedVector3 up)
	{
		// Convert all ManagedVector3 -> Vector3D
		Ermine::Vector3D position{ pos.x, pos.y, pos.z };
		Ermine::Vector3D vel{ velocity.x, velocity.y, velocity.z };
		Ermine::Vector3D fwd{ forward.x, forward.y, forward.z };
		Ermine::Vector3D upVec{ up.x, up.y, up.z };

		Ermine::CAudioEngine::SetListenerAttributes(position, vel, fwd, upVec);
	}
#pragma endregion

#pragma region AudioComponent ICalls
	Ermine::AudioComponent* GetAudioComponentFromManaged(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<AudioComponent>(id))
		{
			EE_CORE_WARN("Cannot get AudioComponent for {0}; either not valid or doesn't have AudioComponent!", id);
			return nullptr;
		}
		return &ECS::GetInstance().GetComponent<AudioComponent>(id);
	}

	Ermine::Material* GetMaterialComponentFromManaged(MonoObject* thisObj)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Ermine::Material>(id))
		{
			return nullptr;
		}
		return &ECS::GetInstance().GetComponent<Ermine::Material>(id);
	}

	float icall_material_get_fill(MonoObject* thisObj)
	{
		auto* matComp = GetMaterialComponentFromManaged(thisObj);
		if (!matComp)
			return 1.0f;

		auto* material = matComp->GetMaterial();
		if (!material)
			return 1.0f;

		if (const auto* param = material->GetParameter("materialFillAmount"))
		{
			if (!param->floatValues.empty())
				return param->floatValues[0];
		}

		return 1.0f;
	}

	void icall_material_set_fill(MonoObject* thisObj, float value)
	{
		using namespace Ermine;
		const EntityID id = GetEntityIDFromManaged(thisObj);

		auto* matComp = GetMaterialComponentFromManaged(thisObj);
		if (!matComp)
		{
			EE_CORE_WARN("[Material.Fill] Entity {0}: Material component missing", id);
			return;
		}

		auto* material = matComp->GetMaterial();
		if (!material)
		{
			EE_CORE_WARN("[Material.Fill] Entity {0}: graphics::Material missing", id);
			return;
		}

		if (value < 0.0f) value = 0.0f;
		if (value > 1.0f) value = 1.0f;
		material->SetFloat("materialFillAmount", value);

		// Ensure GPU material buffer is updated immediately after script-side changes.
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		if (!renderer)
			return;

		const int materialIndex = material->GetMaterialIndex();
		if (materialIndex >= 0)
		{
			const auto ssboData = material->GetSSBOData();
			renderer->UpdateMaterialSSBO(ssboData, static_cast<uint32_t>(materialIndex));
		}
		else
		{
			// Fallback: force material recompilation path if this material is not indexed yet.
			EE_CORE_WARN("[Material.Fill] Entity {0}: invalid material index ({1}), marking materials dirty", id, materialIndex);
			renderer->MarkMaterialsDirty();
		}
	}

	mono_bool icall_material_get_flicker_emissive(MonoObject* thisObj)
	{
		auto* matComp = GetMaterialComponentFromManaged(thisObj);
		if (!matComp)
			return 0;

		return matComp->flickerEmissive ? 1 : 0;
	}

	void icall_material_set_flicker_emissive(MonoObject* thisObj, mono_bool value)
	{
		using namespace Ermine;
		const EntityID id = GetEntityIDFromManaged(thisObj);

		auto* matComp = GetMaterialComponentFromManaged(thisObj);
		if (!matComp)
		{
			EE_CORE_WARN("[Material.FlickerEmissive] Entity {0}: Material component missing", id);
			return;
		}

		const bool enabled = (value != 0);
		if (matComp->flickerEmissive == enabled)
			return;

		matComp->flickerEmissive = enabled;

		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		if (renderer)
		{
			renderer->MarkDrawDataForRebuild();
		}
	}

	mono_bool icall_audiocomponent_get_shouldplay(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->shouldPlay ? 1 : 0;
		return 0;
	}

	void icall_audiocomponent_set_shouldplay(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->shouldPlay = (value != 0);
	}

	mono_bool icall_audiocomponent_get_shouldstop(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->shouldStop ? 1 : 0;
		return 0;
	}

	void icall_audiocomponent_set_shouldstop(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->shouldStop = (value != 0);
	}

	mono_bool icall_audiocomponent_get_isplaying(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->isPlaying ? 1 : 0;
		return 0;
	}

	void icall_audiocomponent_set_isplaying(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->isPlaying = (value != 0);
	}

	float icall_audiocomponent_get_volume(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->volume;
		return 1.0f;
	}

	void icall_audiocomponent_set_volume(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->volume = value;
	}

	MonoString* icall_audiocomponent_get_soundname(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return mono_string_new(mono_domain_get(), ac->soundName.c_str());
		return mono_string_new(mono_domain_get(), "");
	}

	void icall_audiocomponent_set_soundname(MonoObject* thisObj, MonoString* value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
		{
			std::string temp;
			ToTempUTF8(value, temp);
			ac->soundName = std::move(temp);
		}
	}

	mono_bool icall_audiocomponent_get_playonstart(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->playOnStart ? 1 : 0;
		return 0;
	}

	void icall_audiocomponent_set_playonstart(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->playOnStart = (value != 0);
	}

	mono_bool icall_audiocomponent_get_is3d(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->is3D ? 1 : 0;
		return 1; // Default to true
	}

	void icall_audiocomponent_set_is3d(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->is3D = (value != 0);
	}

	float icall_audiocomponent_get_mindistance(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->minDistance;
		return 1.0f;
	}

	void icall_audiocomponent_set_mindistance(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->minDistance = value;
	}

	float icall_audiocomponent_get_maxdistance(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->maxDistance;
		return 100.0f;
	}

	void icall_audiocomponent_set_maxdistance(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->maxDistance = value;
	}

	mono_bool icall_audiocomponent_get_followtransform(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->followTransform ? 1 : 0;
		return 1;
	}

	void icall_audiocomponent_set_followtransform(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->followTransform = (value != 0);
	}

	// Reverb properties
	mono_bool icall_audiocomponent_get_usereverb(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->useReverb ? 1 : 0;
		return 0;
	}

	void icall_audiocomponent_set_usereverb(MonoObject* thisObj, mono_bool value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->useReverb = (value != 0);
	}

	float icall_audiocomponent_get_reverbwetlevel(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->reverbWetLevel;
		return -12.0f;
	}

	void icall_audiocomponent_set_reverbwetlevel(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->reverbWetLevel = value;
	}

	float icall_audiocomponent_get_reverbdrylevel(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->reverbDryLevel;
		return 0.0f;
	}

	void icall_audiocomponent_set_reverbdrylevel(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->reverbDryLevel = value;
	}

	float icall_audiocomponent_get_reverbdecaytime(MonoObject* thisObj)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			return ac->reverbDecayTime;
		return 1.0f;
	}

	void icall_audiocomponent_set_reverbdecaytime(MonoObject* thisObj, float value)
	{
		if (auto* ac = GetAudioComponentFromManaged(thisObj))
			ac->reverbDecayTime = value;
	}
#pragma endregion

#pragma region GameObject ICalls
	MonoObject* icall_gameobject_wrap_existing(uint64_t entityID)
	{
		using namespace Ermine;
		if (entityID == 0 || !ECS::GetInstance().IsEntityValid(entityID))
			return nullptr;
		MonoObject* obj = CreateManagedGameObjectWrapper(entityID);
		return obj;
	}

	mono_bool icall_gameobject_bind_existing(MonoObject* self, uint64_t entityID)
	{
		using namespace Ermine;
		if (!self) return 0;
		if (entityID == 0 || !ECS::GetInstance().IsEntityValid(entityID))
			return 0;

		SetEntityIDOnManaged(self, entityID);
		return 1;
	}

	void icall_gameobject_creategameobject(MonoObject* self, MonoString* name)
	{
		EE_CORE_WARN("GameObject created!");
		using namespace Ermine;
		if (!self)
			return;

		std::string name_;
		ToTempUTF8(name, name_);
		if (name_.empty()) name_ = "GameObject";

		EntityID id = ECS::GetInstance().CreateEntity();
		ECS::GetInstance().AddComponent(id, Transform());

		if (!ECS::GetInstance().HasComponent<HierarchyComponent>(id))
			ECS::GetInstance().AddComponent(id, HierarchyComponent());
		if (!ECS::GetInstance().HasComponent<GlobalTransform>(id))
			ECS::GetInstance().AddComponent(id, GlobalTransform());

		// Initialize in hierarchy so GlobalTransform is set from local immediately
		if (auto hs = ECS::GetInstance().GetSystem<HierarchySystem>())
			hs->InitializeEntity(id);

		ObjectMetaData meta;
		meta.name = name_;
		meta.tag = "Untagged";
		meta.selfActive = true;
		ECS::GetInstance().AddComponent(id, std::move(meta));

		SetEntityIDOnManaged(self, id); // Set the entityID on the managed object (Which is 'GameObject'). Which is primarily what managed object only needs to know
	}

	MonoObject* icall_gameobject_get_transform(MonoObject* self)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
		{
			assert(false && "No Transform component on entity!");
			return nullptr;
		}
		MonoObject* obj = CreateManagedTransformWrapper(id);
		SetComponentGameObject(obj, id);
		return obj;
	}

	bool icall_gameobject_get_active(MonoObject* self)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
			return false;
		return ECS::GetInstance().GetComponent<ObjectMetaData>(id).selfActive;
	}

	void icall_gameobject_set_active(MonoObject* self, bool value)
	{
		using namespace Ermine;
		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<ObjectMetaData>(id))
			return;
		ECS::GetInstance().GetComponent<ObjectMetaData>(id).selfActive = value;
	}

	MonoObject* icall_gameobject_add_component(MonoObject* self, MonoReflectionType* reflType)
	{
		using namespace Ermine;
		if (!self || !reflType) return nullptr;

		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return nullptr;

		MonoType* mtype = mono_reflection_type_get_type(reflType);
		MonoClass* klass = mono_class_from_mono_type(mtype);
		if (!klass) return nullptr;

		if (klass == s_TransformClass)
		{
			if (!ECS::GetInstance().HasComponent<Transform>(id))
				ECS::GetInstance().AddComponent(id, Transform());
			MonoObject* obj = CreateManagedTransformWrapper(id);
			SetComponentGameObject(obj, id);
			return obj;
		}
		// TODO: PLEASE COME BACK AND FIX THIS LATER FOR MULTIPLE SCRIPTS PER GAMEOBJECT
		// MonoBehaviour derived -> Script component
		if (IsSubclassOf(klass, s_MonoBehaviourClass))
		{
			const char* cname = mono_class_get_name(klass);
			EE_CORE_WARN("Is subclass of MonoBehaviour!");
			if (!ECS::GetInstance().HasComponent<ScriptsComponent>(id))
			{
				ECS::GetInstance().AddComponent(id, ScriptsComponent());
				auto& scs = ECS::GetInstance().GetComponent<ScriptsComponent>(id);
				scs.Add(std::string(cname), id);
				//ECS::GetInstance().AddComponent(id, Script(std::string(cname), id));
			}
			else
			{
				ECS::GetInstance().RemoveComponent<Script>(id);
				ECS::GetInstance().AddComponent(id, Script(std::string(cname), id));
			}

			auto& scriptComp = ECS::GetInstance().GetComponent<Script>(id);
			if (scriptComp.m_instance && scriptComp.m_instance->GetManaged())
			{
				SetComponentGameObject(scriptComp.m_instance->GetManaged(), id);
				scripting::ScriptEngine::PushCacheToManagedFields(scriptComp.m_instance->GetManaged(), scriptComp.m_fields);
			}
			return scriptComp.m_instance ? scriptComp.m_instance->GetManaged() : nullptr;
		}
		// TODO: Adding other component like Rigidbody, Collider, etc.?
		EE_CORE_WARN("AddComponent: Unsupported component type '{0}'", mono_class_get_name(klass));
		return nullptr;
	}

	MonoObject* icall_gameobject_get_component(MonoObject* self, MonoReflectionType* relfType)
	{
		using namespace Ermine;
		if (!self || !relfType) return nullptr;

		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
		{
			assert(false && "id not valid!");
			return nullptr;
		}

		MonoType* mtype = mono_reflection_type_get_type(relfType);
		MonoClass* klass = mono_class_from_mono_type(mtype);
		if (!klass)
		{
			assert(false && "Class invalid!");
			return nullptr;
		}

		// return back the obj when requesting Transform component
		if (klass == s_TransformClass)
		{
			if (!ECS::GetInstance().HasComponent<Transform>(id))
				return nullptr;
			MonoObject* obj = CreateManagedTransformWrapper(id);
			SetComponentGameObject(obj, id);
			return obj;
		}

		// Handle AudioComponent
		if (klass == s_AudioComponentClass)
		{
			if (!ECS::GetInstance().HasComponent<AudioComponent>(id))
				return nullptr;

			auto* dom = Ermine::ECS::GetInstance()
				.GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();

			MonoObject* obj = mono_object_new(dom, s_AudioComponentClass);
			mono_runtime_object_init(obj);
			SetEntityIDOnManaged(obj, id);
			SetComponentGameObject(obj, id);
			return obj;
		}

		if (klass == s_RigidbodyClass)
		{
			if (!ECS::GetInstance().HasComponent<PhysicComponent>(id))
			{
				assert(false && "No physicComponent on entity!");
				return nullptr;
			}
			MonoObject* obj = CreateManagedRigidbodyWrapper(id);
			SetComponentGameObject(obj, id);
			return obj;
		}

		// Handle Animator component
		if (klass == s_AnimatorClass)
		{
			if (!ECS::GetInstance().HasComponent<AnimationComponent>(id))
				return nullptr;

			auto* dom = Ermine::ECS::GetInstance()
				.GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();

			MonoObject* obj = mono_object_new(dom, s_AnimatorClass);
			mono_runtime_object_init(obj);
			SetEntityIDOnManaged(obj, id);
			SetComponentGameObject(obj, id);

			return obj;
		}

		// Handle Material component
		if (klass == s_MaterialClass)
		{
			if (!ECS::GetInstance().HasComponent<Ermine::Material>(id))
				return nullptr;

			auto* dom = Ermine::ECS::GetInstance()
				.GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();

			MonoObject* obj = mono_object_new(dom, s_MaterialClass);
			mono_runtime_object_init(obj);
			SetEntityIDOnManaged(obj, id);
			SetComponentGameObject(obj, id);

			return obj;
		}

		// Handle UIImage component
		if (klass == s_UIImageComponentClass)
		{
			if (!ECS::GetInstance().HasComponent<UIImageComponent>(id))
				return nullptr;

			auto* dom = Ermine::ECS::GetInstance()
				.GetSystem<Ermine::scripting::ScriptSystem>()->m_ScriptEngine->GetGameDomain();

			MonoObject* obj = mono_object_new(dom, s_UIImageComponentClass);
			mono_runtime_object_init(obj);
			SetEntityIDOnManaged(obj, id);
			SetComponentGameObject(obj, id);

			return obj;
		}

		// return back the obj when requesting MonoBehaviour derived -> Script component
		if (IsSubclassOf(klass, s_MonoBehaviourClass))
		{
			if (!ECS::GetInstance().HasComponent<ScriptsComponent>(id))
				return nullptr;
			auto& scs = ECS::GetInstance().GetComponent<ScriptsComponent>(id);
			const char* cname = mono_class_get_name(klass);
			auto& scriptComp = scs.GetByClass(cname);
			//auto& scriptComp = ECS::GetInstance().GetComponent<Script>(id);
			if (scriptComp.m_instance && scriptComp.m_instance->GetManaged())
				SetComponentGameObject(scriptComp.m_instance->GetManaged(), id);
			return scriptComp.m_instance ? scriptComp.m_instance->GetManaged() : nullptr;
		}

		EE_CORE_WARN("GetComponent: Unsupported component type '{}'", mono_class_get_name(klass));
		assert(false && "GetComponent: Unsupported component type");
		return nullptr;
	}

	bool icall_gameobject_has_component(MonoObject* self, MonoReflectionType* relfType)
	{
		using namespace Ermine;
		if (!self || !relfType) return false;
		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return false;

		MonoType* mtype = mono_reflection_type_get_type(relfType);
		MonoClass* klass = mono_class_from_mono_type(mtype);
		if (!klass) return false;

		if (klass == s_TransformClass)
			return ECS::GetInstance().HasComponent<Transform>(id);

		if (klass == s_AudioComponentClass)
			return ECS::GetInstance().HasComponent<AudioComponent>(id);

		if (klass == s_AnimatorClass)
			return ECS::GetInstance().HasComponent<AnimationComponent>(id);

		if (klass == s_MaterialClass)
			return ECS::GetInstance().HasComponent<Ermine::Material>(id);

		if (klass == s_UIImageComponentClass)
			return ECS::GetInstance().HasComponent<UIImageComponent>(id);

		if (IsSubclassOf(klass, s_MonoBehaviourClass))
			return ECS::GetInstance().HasComponent<Script>(id);

		EE_CORE_WARN("HasComponent: Unsupported component type '{}'", mono_class_get_name(klass));
		return false;
	}

	void icall_gameobject_remove_component(MonoObject* self, MonoReflectionType* relfType)
	{
		using namespace Ermine;
		if (!self || !relfType) return;
		EntityID id = GetEntityIDFromManaged(self);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return;
		MonoType* mtype = mono_reflection_type_get_type(relfType);
		MonoClass* klass = mono_class_from_mono_type(mtype);
		if (!klass) return;
		if (klass == s_TransformClass)
		{
			//if (ECS::GetInstance().HasComponent<Transform>(id))
			//	ECS::GetInstance().RemoveComponent<Transform>(id);
			EE_CORE_WARN("RemoveComponent<Transform> is not allowed!");
			return;
		}
		if (IsSubclassOf(klass, s_MonoBehaviourClass))
		{
			if (ECS::GetInstance().HasComponent<Script>(id))
				ECS::GetInstance().RemoveComponent<Script>(id);
			return;
		}
		EE_CORE_WARN("RemoveComponent: Unsupported component type '{}'", mono_class_get_name(klass));
	}

	MonoObject* icall_gameobject_find_by_name(MonoString* name)
	{
		using namespace Ermine;
		std::string name_;
		ToTempUTF8(name, name_);
		if (name_.empty()) return nullptr;
		auto& ecs = ECS::GetInstance();

		std::vector<EntityID> FreshEntity = SceneManager::GetInstance().GetActiveScene()->GetAllEntities();
		for (auto id : FreshEntity)
		{
			if (!ecs.IsEntityValid(id) || !ecs.HasComponent<ObjectMetaData>(id))
				continue;
			auto& meta = ecs.GetComponent<ObjectMetaData>(id);
			if (meta.name == name_)
				return CreateManagedGameObjectWrapper(id);
		}
		return nullptr;
	}

	MonoObject* icall_gameobject_find_with_tag(MonoString* tag)
	{
		using namespace Ermine;
		std::string tag_;
		ToTempUTF8(tag, tag_);
		if (tag_.empty()) return nullptr;
		auto& ecs = ECS::GetInstance();
		std::vector<EntityID> FreshEntity = SceneManager::GetInstance().GetActiveScene()->GetAllEntities();
		for (auto id : FreshEntity)
		{
			if (!ecs.IsEntityValid(id) || !ecs.HasComponent<ObjectMetaData>(id))
				continue;
			auto& meta = ecs.GetComponent<ObjectMetaData>(id);
			if (meta.tag == tag_)
				return CreateManagedGameObjectWrapper(id);
		}
		return nullptr;
	}

	MonoArray* icall_gameobject_find_gameobjects_with_tag([[maybe_unused]] MonoString* mtag)
	{
		EE_CORE_WARN("Not ready yet.");
		return nullptr;
	}

	MonoObject* icall_gameobject_instantiate(MonoObject* original)
	{
		using namespace Ermine;
		if (!original) return nullptr;
		EntityID srcID = GetEntityIDFromManaged(original);
		if (srcID == 0 || !ECS::GetInstance().IsEntityValid(srcID))
			return nullptr;

		EntityID dstID = ECS::GetInstance().CloneEntity(srcID);

		MonoObject* obj = CreateManagedGameObjectWrapper(dstID);
		SetComponentGameObject(obj, dstID);
		return obj;
	}

	void icall_gameobject_destroy(MonoObject* target)
	{
		using namespace Ermine;
		if (!target) return;
		EntityID id = GetEntityIDFromManaged(target);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return;
		if (ECS::GetInstance().HasComponent<PhysicComponent>(id))
			ECS::GetInstance().GetComponent<PhysicComponent>(id).isDead = true;
		
		//ECS::GetInstance().DestroyEntity(id);
		EnqueueLateDestory(id);
	}
#pragma endregion

#pragma region Physics ICalls
	using namespace Ermine;
	mono_bool icall_physics_raycast(ManagedVector3 mOrigin, ManagedVector3 mDirection, ManagedRaycastHit* outHit, float maxDistance)
	{
		auto physicsSys = ECS::GetInstance().GetSystem<Physics>();
		if (!physicsSys || !outHit)
			return false;

		const RVec3 rOrigin = RVec3{ mOrigin.x, mOrigin.y, mOrigin.z };
		const RVec3 rDirection = RVec3{ mDirection.x, mDirection.y, mDirection.z };

		double lenSq = rDirection.GetX() * rDirection.GetX() + rDirection.GetY() * rDirection.GetY() + rDirection.GetZ() * rDirection.GetZ();
		if (lenSq < 1e-12) return 0;
		RVec3 dirNorm = rDirection.Normalized();

		RayCastResult native_hit{};
		if (!physicsSys->Raycast(rOrigin, dirNorm, maxDistance, native_hit))
			return false;

		float distance = native_hit.mFraction * maxDistance;

		RVec3 hitPoint = rOrigin + dirNorm * distance;

		RVec3 normalWorld(0.0f, 1.0f, 0.0f);
		{
			JPH::BodyID bodyID = native_hit.mBodyID;
			// Acquire read lock
			auto& ps = physicsSys->GetBodyLockInterface(); // Ensure Physics exposes this
			JPH::BodyLockRead lock(ps, bodyID);
			if (lock.Succeeded())
			{
				const JPH::Body& body = lock.GetBody();
				const JPH::Shape* shape = body.GetShape();
				if (shape)
				{
					// World transform
					JPH::RMat44 worldM = body.GetWorldTransform();

					// Local point (COM based)
					JPH::RVec3 worldTranslation = worldM.GetTranslation();
					JPH::Vec3 localPos = JPH::Vec3(worldM.Inversed().Multiply3x3(hitPoint - worldTranslation));

					// Query surface normal in local space using sub-shape ID hierarchy
					JPH::Vec3 localNormal = shape->GetSurfaceNormal(native_hit.mSubShapeID2, localPos);
					// Transform to world (rotation only)
					normalWorld = worldM.Multiply3x3(localNormal);
					Vector3D tempVec3Holder{};
					Vec3Normalize(tempVec3Holder, { normalWorld.GetX(),normalWorld.GetY(),normalWorld.GetZ() });
					/*normalWorld.Normalize();*/
					normalWorld = {tempVec3Holder.x,tempVec3Holder.y,tempVec3Holder.z};
				}
			}
		}

		uint64_t entityID = physicsSys->GetEntityID(native_hit.mBodyID);

		outHit->point = { hitPoint.GetX(), hitPoint.GetY(), hitPoint.GetZ() };
		outHit->normal = { normalWorld.GetX(), normalWorld.GetY(), normalWorld.GetZ() };
		outHit->distance = distance;
		outHit->entityID = entityID;

		return true;
	}

	MonoObject* icall_physics_gettransform(uint64_t id)
	{
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<Transform>(id))
		{
			{ //TEMP
				std::cerr << "[Physics] GetTransform failed. EntityID = "
					<< id << std::endl;
				return nullptr;
			}
			assert(false && "Here!");
			return nullptr;
		}
		MonoObject* obj = CreateManagedTransformWrapper(id);
		SetComponentGameObject(obj, id);
		return obj;
	}

	MonoObject* icall_rigidbody_get_rigidbody(uint64_t id)
	{
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id) || !ECS::GetInstance().HasComponent<PhysicComponent>(id))
		{
			assert(false && "Here!");
			return nullptr;
		}
		//auto physics = ECS::GetInstance().GetSystem<Physics>();
		//if (!physics->HasRigidbody((EntityID)entityID))
		//	return nullptr;
		MonoObject* obj = CreateManagedRigidbodyWrapper(id);
		SetComponentGameObject(obj, id);
		return obj;
	}

	static void icall_Physics_SetPosition(uint64_t entityID, Ermine::Vec3 pos)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->SetPosition((EntityID)entityID, pos);
	}

	static void icall_Physics_SetRotationEuler(uint64_t entityID, Ermine::Vec3 euler)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->SetRotation((EntityID)entityID, euler);
	}

	static void icall_Physics_SetRotationQuat(uint64_t entityID, Quaternion q)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->SetRotation((EntityID)entityID, q);
	}

	static void icall_Physics_MoveEuler(uint64_t entityID, Ermine::Vec3 pos, Ermine::Vec3 euler)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->Move((EntityID)entityID, pos, euler);
	}

	static void icall_Physics_MoveQuat(uint64_t entityID, Ermine::Vec3 pos, Quaternion q)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->Move((EntityID)entityID, pos, q);
	}
	static void icall_Physics_RemovePhysic(uint64_t entityID)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->RemovePhysic((EntityID)entityID);
	}
	static bool icall_Physics_HasPhysicComp(uint64_t entityID)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		return physics->HasPhysicComp((EntityID)entityID);
	}
	static void icall_Physics_Jump(uint64_t entityID,float jump)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->Jump((EntityID)entityID,jump);
	}
	static int icall_Physics_CheckMotionType(uint64_t entityID)
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		return physics->GetMotionType((EntityID)entityID);
	}
	static void icall_Physics_ForceUpdate()
	{
		auto physics = ECS::GetInstance().GetSystem<Physics>();
		physics->ForceUpdate();
	}
	void icall_physics_set_light_value(uint64_t entityID, float value)
	{
		auto& ecs = ECS::GetInstance();
		if (entityID == 0 || !ecs.IsEntityValid(entityID))
			return;

		if (auto physics = ecs.GetSystem<Physics>())
		{
			physics->SetLightValue(entityID, value);
		}
	}

	float icall_physics_get_light_value(uint64_t entityID)
	{
		using namespace Ermine;

		auto& ecs = ECS::GetInstance();
		if (entityID == 0 || !ecs.IsEntityValid(entityID))
			return 0.0f;

		if (!ecs.HasComponent<Light>(entityID))
			return 0.0f;

		const auto& lightobj = ecs.GetComponent<Light>(entityID);
		return lightobj.intensity;
	}

#pragma endregion

#pragma region Prefab ICalls
	MonoObject* icall_prefab_instantiate(MonoString* mpath)
	{
		if (!mpath) return nullptr;

		std::string rel;
		ToTempUTF8(mpath, rel);
		if (rel.empty()) return nullptr;

		fs::path p(rel);

		if (p.is_relative())
		{
			if (p.extension() != ".prefab")
				p += ".prefab";
			if (p.empty() || !p.string().starts_with("../Resources"))
				p = fs::path("Resources") / p;
		}

		try
		{
			Ermine::EntityID id = LoadPrefabFromFile(Ermine::ECS::GetInstance(), p);
			if (id == 0 || !Ermine::ECS::GetInstance().IsEntityValid(id))
			{
				EE_CORE_WARN("Prefab instantiate failed for path: {}", p.string());
				return nullptr;
			}
			MonoObject* go = CreateManagedGameObjectWrapper(id);
			SetComponentGameObject(go, id);
			return go;
		}
		catch (const std::exception& e)
		{
			EE_CORE_ERROR("Prefab instantiate exception for path: {} ; what(): {}", p.string(), e.what());
			return nullptr;
		}
	}
#pragma endregion

#pragma region StateMachine ICalls
	static void icall_statemachine_request_next_state(uint64_t entityID)
	{
		using namespace Ermine;

		if (entityID == 0 || !ECS::GetInstance().IsEntityValid(entityID))
			return;

		if (!ECS::GetInstance().HasComponent<StateMachine>(entityID))
			return;

		auto& fsm = ECS::GetInstance().GetComponent<StateMachine>(entityID);

		// Rebind runtime-only manager pointer (common after loading a scene)
		if (!fsm.manager)
			fsm.manager = ECS::GetInstance().GetSystem<StateManager>().get();

		if (!fsm.manager)
			return;

		// Optional safety: make sure there's a current script before transitioning
		if (fsm.m_CurrentScript == nullptr)
			fsm.Init(entityID);

		fsm.manager->RequestNextState(entityID);
	}

	static void icall_statemachine_request_previous_state(uint64_t entityID)
	{
		using namespace Ermine;

		if (entityID == 0 || !ECS::GetInstance().IsEntityValid(entityID))
			return;

		if (!ECS::GetInstance().HasComponent<StateMachine>(entityID))
			return;

		auto& fsm = ECS::GetInstance().GetComponent<StateMachine>(entityID);

		// Rebind runtime-only manager pointer (common after loading a scene)
		if (!fsm.manager)
			fsm.manager = ECS::GetInstance().GetSystem<StateManager>().get();

		if (!fsm.manager)
			return;

		// Optional safety: make sure there's a current script before transitioning
		if (fsm.m_CurrentScript == nullptr)
			fsm.Init(entityID);

		fsm.manager->RequestPreviousState(entityID);
	}

#pragma endregion

#pragma region NavAgent ICalls
	static bool SnapPointToNavMesh(EntityID navEntity, const Ermine::Vec3& inPos, Ermine::Vec3& outPos, const float extents[3])
	{
		auto& ecs = Ermine::ECS::GetInstance();

		if (navEntity == 0 || !ecs.HasComponent<NavMeshComponent>(navEntity))
			return false;

		const auto& nav = ecs.GetComponent<NavMeshComponent>(navEntity);
		if (!nav.runtime || !nav.runtime->query)
			return false;

		dtNavMeshQuery* q = nav.runtime->query;
		dtQueryFilter filter; // default filter is fine if you don�t use flags

		float p[3] = { inPos.x, inPos.y, inPos.z };
		dtPolyRef ref = 0;
		float nearestPt[3] = {};

		dtStatus st = q->findNearestPoly(p, extents, &filter, &ref, nearestPt);
		if (dtStatusFailed(st) || ref == 0)
			return false;

		outPos = Ermine::Vec3{ nearestPt[0], nearestPt[1], nearestPt[2] };
		return true;
	}

	void icall_navagent_start_jump(uint64_t agentEntityID, uint64_t linkEntityID)
	{
		using namespace Ermine;

		auto& ecs = ECS::GetInstance();

		EntityID agentID = (EntityID)agentEntityID;
		EntityID linkID = (EntityID)linkEntityID;

		if (!ecs.IsEntityValid(agentID) || !ecs.IsEntityValid(linkID)) return;
		if (!ecs.HasComponent<NavMeshAgent>(agentID)) return;
		if (!ecs.HasComponent<Transform>(agentID)) return;
		if (!ecs.HasComponent<NavJumpLink>(linkID)) return;

		auto& agent = ecs.GetComponent<NavMeshAgent>(agentID);
		auto& tr = ecs.GetComponent<Transform>(agentID);
		const auto& link = ecs.GetComponent<NavJumpLink>(linkID);

		if (agent.isJumping) return;

		auto isUnset = [](const Ermine::Vec3& v) -> bool
			{
				return std::fabs(v.x) < 1e-4f && std::fabs(v.y) < 1e-4f && std::fabs(v.z) < 1e-4f;
			};

		// current navmesh under agent
		EntityID currentNav = 0;
		EntityID prevNav = agent.lastJumpFromNavMesh;
		auto navSys = ecs.GetSystem<NavMeshAgentSystem>();
		if (navSys)
			currentNav = navSys->FindNearestNavMeshEntity(tr.position);

		// update BEFORE picking next
		agent.lastJumpFromNavMesh = currentNav;

		// takeoff anchor: JumpArea transform if present
		Ermine::Vec3 takeoff = tr.position;
		if (ecs.HasComponent<Transform>(linkID))
			takeoff = ecs.GetComponent<Transform>(linkID).position;

		Ermine::Vec3 landing = link.landingPosition;

		// auto fill landing, when landingPosition is not authored
		if (isUnset(landing))
		{
			if (!navSys)
			{
				EE_CORE_WARN("[NavAgentJump] No NavMeshAgentSystem. Jump cancelled.");
				return;
			}

			// Pick the "other" navmesh by asking for nearest EXCLUDING current.
			EntityID targetNav = navSys->FindNearestNavMeshEntityExcluding(takeoff, currentNav, prevNav);

			// Fallback: if excluding returns 0 for any reason, try using agent pos
			if (targetNav == 0)
				targetNav = navSys->FindNearestNavMeshEntityExcluding(tr.position, currentNav, prevNav);

			if (targetNav == 0 || !ecs.HasComponent<Transform>(targetNav))
			{
				EE_CORE_WARN("[NavAgentJump] No target navmesh found. Jump cancelled.");
				return;
			}

			// Use the target navmesh entity's transform position as a probe
			Ermine::Vec3 probe = ecs.GetComponent<Transform>(targetNav).position;

			// Snap probe onto that navmesh (use large extents so it always finds a nearby poly)
			float ext[3] = { 10.0f, 30.0f, 10.0f };

			Ermine::Vec3 snapped;
			if (!SnapPointToNavMesh(targetNav, probe, snapped, ext))
			{
				EE_CORE_WARN("[NavAgentJump] Failed to snap to target navmesh (ent={}). Jump cancelled.", targetNav);
				return;
			}

			landing = snapped;
		}

		// Start jump
		agent.isJumping = true;
		agent.navPaused = true;
		agent.hasPath = false;

		agent.jumpStart = tr.position;
		agent.jumpTarget = landing;
		agent.jumpTimer = 0.0f;

		// Auto duration/height from distance
		Ermine::Vec3 d = agent.jumpTarget - agent.jumpStart;
		float dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);

		float autoDuration = std::clamp(dist / 6.0f, 0.25f, 1.5f);
		float autoHeight = std::clamp(dist / 4.0f, 0.5f, 3.0f);

		agent.jumpDuration = (link.jumpDuration > 0.0f) ? link.jumpDuration : autoDuration;
		agent.jumpHeight = (link.jumpHeight > 0.0f) ? link.jumpHeight : autoHeight;
	}

	void icall_navagent_set_auto_rotate(uint64_t agentEntityID, bool enabled)
	{
		using namespace Ermine;

		auto& ecs = ECS::GetInstance();
		EntityID agentID = (EntityID)agentEntityID;

		if (!ecs.IsEntityValid(agentID))
			return;

		if (!ecs.HasComponent<NavMeshAgent>(agentID))
			return;

		auto& agent = ecs.GetComponent<NavMeshAgent>(agentID);

		agent.autoRotate = enabled;
	}
#pragma endregion

#pragma region UI ICalls
	static float Internal_GetHealth(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		// Try UIHealthbarComponent first (new system)
		if (ecs.HasComponent<UIHealthbarComponent>(entityID))
		{
			auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
			return healthbar.currentHealth;
		}
		// Fall back to UIComponent (legacy)
		if (ecs.HasComponent<UIComponent>(entityID))
		{
			auto& ui = ecs.GetComponent<UIComponent>(entityID);
			return ui.GetHealth();
		}
		return 0.0f;
	}

	static void Internal_SetHealth(uint64_t entityID, float value)
	{
		auto& ecs = ECS::GetInstance();
		// Try UIHealthbarComponent first (new system)
		if (ecs.HasComponent<UIHealthbarComponent>(entityID))
		{
			auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
			healthbar.currentHealth = value;
			return;
		}
		// Fall back to UIComponent (legacy)
		if (ecs.HasComponent<UIComponent>(entityID))
		{
			auto& ui = ecs.GetComponent<UIComponent>(entityID);
			ui.SetHealth(value);
		}
	}

	// temporary reference to health bar, to be removed
	static uint64_t Internal_GetHealthBar()
	{
		return (uint64_t)SceneManager::GetHealthBar();
	}

	// ========================================================================
	// NEW UI COMPONENT BINDINGS
	// ========================================================================

	// UIHealthbarComponent bindings
	static float Internal_Healthbar_GetHealth(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return 0.0f;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		return healthbar.GetHealth();
	}

	static void Internal_Healthbar_SetHealth(uint64_t entityID, float value)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		healthbar.SetHealth(value);
	}

	static float Internal_Healthbar_GetMaxHealth(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return 100.0f;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		return healthbar.maxHealth;
	}

	static float Internal_Healthbar_GetRegenRate(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return 0.0f;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		return healthbar.healthRegenRate;
	}

	// GameplayHUD wrappers for UIHealthbarComponent
	static float Internal_GameplayHUD_GetMaxHealth(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return 100.0f;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		return healthbar.maxHealth;
	}

	static float Internal_GameplayHUD_GetRegenRate(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIHealthbarComponent>(entityID))
			return 0.0f;

		auto& healthbar = ecs.GetComponent<UIHealthbarComponent>(entityID);
		return healthbar.healthRegenRate;
	}

	// UIBookCounterComponent bindings
	static int Internal_BookCounter_GetCollected(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIBookCounterComponent>(entityID))
			return 0;

		auto& bookCounter = ecs.GetComponent<UIBookCounterComponent>(entityID);
		return bookCounter.booksCollected;
	}

	static void Internal_BookCounter_SetCollected(uint64_t entityID, int value)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIBookCounterComponent>(entityID))
			return;

		auto& bookCounter = ecs.GetComponent<UIBookCounterComponent>(entityID);
		bookCounter.booksCollected = std::clamp(value, 0, bookCounter.totalBooks);
	}

	static void Internal_BookCounter_AddBook(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIBookCounterComponent>(entityID))
			return;

		auto& bookCounter = ecs.GetComponent<UIBookCounterComponent>(entityID);
		bookCounter.booksCollected = std::min(bookCounter.booksCollected + 1, bookCounter.totalBooks);
	}

	static int Internal_BookCounter_GetTotal(uint64_t entityID)
	{
		auto& ecs = ECS::GetInstance();
		if (!ecs.HasComponent<UIBookCounterComponent>(entityID))
			return 0;

		auto& bookCounter = ecs.GetComponent<UIBookCounterComponent>(entityID);
		return bookCounter.totalBooks;
	}

#pragma endregion

#pragma region Cursor ICalls

	void icall_cursor_set_visible(bool value)
	{
		Window::SetVisibleCursor(value);
	}

	bool icall_cursor_get_visible()
	{
		return Window::GetVisibleCursor();
	}

	void icall_cursor_set_locked(std::int32_t state) noexcept
	{
		Window::SetCursorLockState(static_cast<Window::CursorLockState>(state));
	}

	std::int32_t icall_cursor_get_locked()
	{
		return static_cast<std::int32_t>(Window::GetCursorLockState());
	}
#pragma endregion

#pragma region PostProcess ICalls

	std::shared_ptr<Ermine::graphics::Renderer> GetPostProcessRenderer()
	{
		auto renderer = Ermine::ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>();
		if (!renderer)
			EE_CORE_WARN("PostProcess internal call: Renderer system not registered.");
		return renderer;
	}

	mono_bool icall_postprocess_get_vignette_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteEnabled ? 1 : 0;
		return 0;
	}

	void icall_postprocess_set_vignette_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteEnabled = (enabled != 0);
	}

	float icall_postprocess_get_vignette_intensity()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteIntensity;
		return 0.0f;
	}

	void icall_postprocess_set_vignette_intensity(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteIntensity = std::clamp(value, 0.0f, 1.0f);
	}

	float icall_postprocess_get_vignette_radius()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteRadius;
		return 0.8f;
	}

	void icall_postprocess_set_vignette_radius(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteRadius = std::clamp(value, 0.0f, 2.0f);
	}

	float icall_postprocess_get_vignette_coverage()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteCoverage;
		return 0.0f;
	}

	void icall_postprocess_set_vignette_coverage(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteCoverage = std::clamp(value, 0.0f, 1.0f);
	}

	float icall_postprocess_get_vignette_falloff()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteFalloff;
		return 0.2f;
	}

	void icall_postprocess_set_vignette_falloff(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteFalloff = std::max(value, 0.01f);
	}

	float icall_postprocess_get_vignette_map_strength()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteMapStrength;
		return 1.0f;
	}

	void icall_postprocess_set_vignette_map_strength(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteMapStrength = std::clamp(value, 0.0f, 1.0f);
	}

	ManagedVector3 icall_postprocess_get_vignette_map_rgb_modifier()
	{
		if (auto renderer = GetPostProcessRenderer())
		{
			return ManagedVector3{
				renderer->m_VignetteMapRGBModifier.r,
				renderer->m_VignetteMapRGBModifier.g,
				renderer->m_VignetteMapRGBModifier.b
			};
		}
		return ManagedVector3{ 1.0f, 1.0f, 1.0f };
	}

	void icall_postprocess_set_vignette_map_rgb_modifier(ManagedVector3 value)
	{
		if (auto renderer = GetPostProcessRenderer())
		{
			renderer->m_VignetteMapRGBModifier = glm::vec3(
				std::max(value.x, 0.0f),
				std::max(value.y, 0.0f),
				std::max(value.z, 0.0f)
			);
		}
	}

	MonoString* icall_postprocess_get_vignette_map_path()
	{
		if (auto renderer = GetPostProcessRenderer())
			return mono_string_new(mono_domain_get(), renderer->m_VignetteMapPath.c_str());
		return mono_string_new(mono_domain_get(), "");
	}

	void icall_postprocess_set_vignette_map_path(MonoString* path)
	{
		if (auto renderer = GetPostProcessRenderer())
		{
			std::string pathText;
			ToTempUTF8(path, pathText);
			renderer->m_VignetteMapPath = pathText;

			if (pathText.empty())
			{
				renderer->ClearVignetteMapTexture();
				return;
			}

			auto texture = AssetManager::GetInstance().LoadTexture(pathText);
			if (texture && texture->IsValid())
			{
				renderer->SetVignetteMapTexture(texture, pathText);
			}
			else
			{
				renderer->SetVignetteMapTexture(nullptr, pathText);
				EE_CORE_WARN("PostProcess: failed to load vignette map '{}'", pathText);
			}
		}
	}

	mono_bool icall_postprocess_get_radial_blur_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_RadialBlurEnabled ? 1 : 0;
		return 0;
	}

	void icall_postprocess_set_radial_blur_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_RadialBlurEnabled = (enabled != 0);
	}

	float icall_postprocess_get_radial_blur_strength()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_RadialBlurStrength;
		return 0.0f;
	}

	void icall_postprocess_set_radial_blur_strength(float value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_RadialBlurStrength = std::clamp(value, 0.0f, 0.35f);
	}

	int icall_postprocess_get_radial_blur_samples()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_RadialBlurSamples;
		return 12;
	}

	void icall_postprocess_set_radial_blur_samples(int value)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_RadialBlurSamples = std::clamp(value, 4, 24);
	}

	ManagedVector2 icall_postprocess_get_radial_blur_center()
	{
		if (auto renderer = GetPostProcessRenderer())
		{
			return ManagedVector2{
				renderer->m_RadialBlurCenter.x,
				renderer->m_RadialBlurCenter.y
			};
		}
		return ManagedVector2{ 0.5f, 0.5f };
	}

	void icall_postprocess_set_radial_blur_center(ManagedVector2 value)
	{
		if (auto renderer = GetPostProcessRenderer())
		{
			renderer->m_RadialBlurCenter = glm::vec2(
				std::clamp(value.x, 0.0f, 1.0f),
				std::clamp(value.y, 0.0f, 1.0f)
			);
		}
	}

#pragma endregion

#pragma region VideoManager ICalls

	std::shared_ptr<Ermine::VideoManager> GetVideoManager()
	{
		auto videoSystem = Ermine::ECS::GetInstance().GetSystem<Ermine::VideoManager>();
		if (!videoSystem)
			EE_CORE_WARN("VideoManager internal call: VideoManager system not registered.");
		return videoSystem;
	}

	mono_bool icall_videomanager_load(MonoString* name, MonoString* filepath, mono_bool loop)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name || !filepath)
			return 0;

		std::string videoName;
		std::string videoPath;
		ToTempUTF8(name, videoName);
		ToTempUTF8(filepath, videoPath);
		return videoSystem->LoadVideo(videoName, videoPath, loop != 0) ? 1 : 0;
	}

	void icall_videomanager_set_current(MonoString* name)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name)
			return;

		std::string videoName;
		ToTempUTF8(name, videoName);
		videoSystem->SetCurrentVideo(videoName);
	}

	MonoString* icall_videomanager_get_current()
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem)
			return mono_string_new(mono_domain_get(), "");

		return mono_string_new(mono_domain_get(), videoSystem->GetCurrentVideo().c_str());
	}

	void icall_videomanager_play()
	{
		if (auto videoSystem = GetVideoManager())
			videoSystem->Play();
	}

	void icall_videomanager_pause()
	{
		if (auto videoSystem = GetVideoManager())
			videoSystem->Pause();
	}

	void icall_videomanager_stop()
	{
		if (auto videoSystem = GetVideoManager())
			videoSystem->Stop();
	}

	mono_bool icall_videomanager_is_playing()
	{
		if (auto videoSystem = GetVideoManager())
			return videoSystem->IsVideoPlaying() ? 1 : 0;
		return 0;
	}

	mono_bool icall_videomanager_is_done(MonoString* name)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name)
			return 0;

		std::string videoName;
		ToTempUTF8(name, videoName);
		return videoSystem->IsVideoDonePlaying(videoName) ? 1 : 0;
	}

	mono_bool icall_videomanager_exists(MonoString* name)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name)
			return 0;

		std::string videoName;
		ToTempUTF8(name, videoName);
		return videoSystem->VideoExists(videoName) ? 1 : 0;
	}

	void icall_videomanager_free(MonoString* name)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name)
			return;

		std::string videoName;
		ToTempUTF8(name, videoName);
		videoSystem->FreeVideo(videoName);
	}

	void icall_videomanager_cleanup_all()
	{
		if (auto videoSystem = GetVideoManager())
			videoSystem->CleanupAllVideos();
	}

	void icall_videomanager_set_fit_mode(int mode)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem)
			return;

		using FitMode = Ermine::VideoManager::VideoFitMode;
		if (mode <= static_cast<int>(FitMode::AspectFit))
			videoSystem->SetFitMode(FitMode::AspectFit);
		else
			videoSystem->SetFitMode(FitMode::StretchToFill);
	}

	int icall_videomanager_get_fit_mode()
	{
		if (auto videoSystem = GetVideoManager())
			return static_cast<int>(videoSystem->GetFitMode());
		return static_cast<int>(Ermine::VideoManager::VideoFitMode::AspectFit);
	}

	void icall_videomanager_set_render_enabled(mono_bool enabled)
	{
		if (auto videoSystem = GetVideoManager())
			videoSystem->SetRenderEnabled(enabled != 0);
	}

	mono_bool icall_videomanager_get_render_enabled()
	{
		if (auto videoSystem = GetVideoManager())
			return videoSystem->IsRenderEnabled() ? 1 : 0;
		return 0;
	}

	void icall_videomanager_set_audio_volume(MonoString* name, float volume)
	{
		auto videoSystem = GetVideoManager();
		if (!videoSystem || !name) return;
		char* nameStr = mono_string_to_utf8(name);
		videoSystem->SetAudioVolume(nameStr, volume);
		mono_free(nameStr);
	}

#pragma endregion

#pragma region Animation ICalls
	static AnimationComponent* GetAnimationFromManaged(MonoObject* thisObj)
	{
		using namespace Ermine;

		EntityID id = GetEntityIDFromManaged(thisObj);
		if (id == 0 || !ECS::GetInstance().IsEntityValid(id))
			return nullptr;

		if (!ECS::GetInstance().HasComponent<AnimationComponent>(id))
			return nullptr;

		auto& anim = ECS::GetInstance().GetComponent<AnimationComponent>(id);
		if (!anim.m_animationGraph)
			return nullptr;

		return &anim;
	}

	bool icall_animator_get_bool(MonoObject* thisObj, MonoString* name)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return false;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Bool)
				return p.boolValue;
		}

		return false;
	}

	void icall_animator_set_bool(MonoObject* thisObj, MonoString* name, bool value)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Bool)
			{
				p.boolValue = value;
				return;
			}
		}
	}

	float icall_animator_get_float(MonoObject* thisObj, MonoString* name)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return 0.f;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Float)
				return p.floatValue;
		}
		return 0.f;
	}

	void icall_animator_set_float(MonoObject* thisObj, MonoString* name, float value)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Float)
			{
				p.floatValue = value;
				return;
			}
		}
	}

	int icall_animator_get_int(MonoObject* thisObj, MonoString* name)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return 0;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Int)
				return p.intValue;
		}
		return 0;
	}

	void icall_animator_set_int(MonoObject* thisObj, MonoString* name, int value)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Int)
			{
				p.intValue = value;
				return;
			}
		}
	}

	void icall_animator_set_trigger(MonoObject* thisObj, MonoString* name)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return;

		std::string param;
		ToTempUTF8(name, param);

		for (auto& p : anim->m_animationGraph->parameters)
		{
			if (p.name == param && p.type == AnimationParameter::Type::Trigger)
			{
				p.triggerValue = true;
				return;
			}
		}
	}

	MonoString* icall_animator_get_current_state(MonoObject* thisObj)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !anim->m_animationGraph->current)
			return nullptr;

		return mono_string_new(
			mono_domain_get(),
			anim->m_animationGraph->current->name.c_str()
		);
	}

	void icall_animator_set_state(MonoObject* thisObj, MonoString* name)
	{
		auto* anim = GetAnimationFromManaged(thisObj);
		if (!anim || !name) return;

		std::string stateName;
		ToTempUTF8(name, stateName);

		for (auto& s : anim->m_animationGraph->states)
		{
			if (s->name == stateName)
			{
				anim->m_animationGraph->current = s;
				anim->m_animationGraph->currentTime = 0.f;
				anim->m_animationGraph->playing = true;
				return;
			}
		}
	}
#pragma endregion

#pragma region PostEffects ICalls
	void icall_posteffects_set_exposure(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_Exposure = value;
	}

	void icall_posteffects_set_contrast(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_Contrast = value;
	}

	void icall_posteffects_set_saturation(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_Saturation = value;
	}

	void icall_posteffects_set_gamma(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 3.0f);
		ecs.GetSystem<graphics::Renderer>()->m_Gamma = value;
	}

	void icall_posteffects_set_vignetteintensity(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_VignetteIntensity = value;
	}

	void icall_posteffects_set_vignetteradius(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_VignetteRadius = value;
	}

	void icall_posteffects_set_bloomStrength(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_BloomStrength = value;
	}

	void icall_posteffects_set_grainintensity(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_GrainIntensity = value;
	}

	void icall_posteffects_set_grainsize(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_GrainScale = value;
	}

	void icall_posteffects_set_chromaticaberration(float value)
	{
		auto& ecs = ECS::GetInstance();
		value = std::clamp(value, 0.0f, 2.0f);
		ecs.GetSystem<graphics::Renderer>()->m_ChromaticAmount = value;
	}

	mono_bool icall_posteffects_get_vignette_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_VignetteEnabled ? 1 : 0;
		return 0;
	}

	void icall_posteffects_set_vignette_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_VignetteEnabled = (enabled != 0);
	}

	mono_bool icall_posteffects_get_flim_grain_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_FilmGrainEnabled ? 1 : 0;
		return 0;
	}

	void icall_posteffects_set_flim_grain_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_FilmGrainEnabled = (enabled != 0);
	}

	mono_bool icall_posteffects_get_chromatic_aberration_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_ChromaticAberrationEnabled ? 1 : 0;
		return 0;
	}

	void icall_posteffects_set_chromatic_aberration_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_ChromaticAberrationEnabled = (enabled != 0);
	}

	mono_bool icall_posteffects_get_bloom_enabled()
	{
		if (auto renderer = GetPostProcessRenderer())
			return renderer->m_BloomEnabled ? 1 : 0;
		return 0;
	}

	void icall_posteffects_set_bloom_enabled(mono_bool enabled)
	{
		if (auto renderer = GetPostProcessRenderer())
			renderer->m_BloomEnabled = (enabled != 0);
	}
#pragma endregion
}

namespace Ermine::scripting
{
	// For the forward declaration in ScriptInstance.h
	void NativeBindComponentGameObject(MonoObject* componentObj, EntityID id)
	{
		if (!componentObj || !s_ComponentClass || !s_GameObjectClass)
			return;

		MonoMethod* setGO = mono_class_get_method_from_name(s_ComponentClass, "set_gameObject", 1);
		if (!setGO)
			return;

		MonoObject* goWrapper = CreateManagedGameObjectWrapper(id);
		if (!goWrapper)
			return;

		void* args[1] = { goWrapper };
		MonoObject* exc = nullptr;
		mono_runtime_invoke(setGO, componentObj, args, &exc);
		if (exc)
		{
			MonoString* s = mono_object_to_string(exc, nullptr);
			char* utf8 = mono_string_to_utf8(s);
			EE_CORE_ERROR("NativeBindComponentGameObject exception: {}", utf8);
			if (utf8) mono_free(utf8);
		}
	}

	void ScriptEngine::QueueLateDestroy(EntityID id)
	{
		EnqueueLateDestory(id);
	}

	void ScriptEngine::FlushLateDestroy()
	{
		std::vector<EntityID> toDestroy;
		{
			std::scoped_lock lock(s_LateDestroyMutex);
			if (s_LateDestroyQueue.empty()) return;
			toDestroy.swap(s_LateDestroyQueue);
		}

		auto& ecs = ECS::GetInstance();
		static bool physchange = false;
		for (EntityID id : toDestroy)
		{
			if (ECS::GetInstance().HasComponent<PhysicComponent>(id))
			{
				physchange = true;
				ECS::GetInstance().GetComponent<PhysicComponent>(id).isDead = true;
			}


			if (id != 0 && ecs.IsEntityValid(id))
				ecs.DestroyEntity(id);
		}
		if (physchange)
		{
			ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
			physchange = false;
		}

	}

	void ScriptEngine::PushSingleField(MonoObject* obj,
		const std::string& name,
		const ScriptFieldValue& val)
	{
		if (!obj) return;
		MonoClass* klass = mono_object_get_class(obj);
		auto fields = DiscoverScriptFields(klass);
		for (auto& f : fields)
		{
			if (f.name != name) continue;

			switch (f.kind)
			{
			case ScriptFieldInfo::Kind::Float:
				if (val.kind == ScriptFieldValue::Kind::Float)
					mono_field_set_value(obj, f.field, const_cast<float*>(&std::get<float>(val.value)));
				break;
			case ScriptFieldInfo::Kind::Int:
				if (val.kind == ScriptFieldValue::Kind::Int)
					mono_field_set_value(obj, f.field, const_cast<int*>(&std::get<int>(val.value)));
				break;
			case ScriptFieldInfo::Kind::Bool:
				if (val.kind == ScriptFieldValue::Kind::Bool)
				{
					mono_bool mb = std::get<bool>(val.value) ? 1 : 0;
					mono_field_set_value(obj, f.field, &mb);
				}
				break;
			case ScriptFieldInfo::Kind::String:
				if (val.kind == ScriptFieldValue::Kind::String)
				{
					MonoString* ms = mono_string_new(mono_domain_get(), std::get<std::string>(val.value).c_str());
					mono_field_set_value(obj, f.field, ms);
				}
				break;
			case ScriptFieldInfo::Kind::Vector3:
				if (val.kind == ScriptFieldValue::Kind::Vector3)
				{
					auto& v = std::get<Ermine::Vec3>(val.value);
					struct ManagedVector3 { float x, y, z; } mv{ v.x, v.y, v.z };
					mono_field_set_value(obj, f.field, &mv);
				}
				break;
			case ScriptFieldInfo::Kind::Quaternion:
				if (val.kind == ScriptFieldValue::Kind::Quaternion)
				{
					auto& v = std::get<Ermine::Quaternion>(val.value);
					struct ManagedQuaternion { float x, y, z, w; } mq{ v.x, v.y, v.z, v.w };
					mono_field_set_value(obj, f.field, &mq);
				}
				break;
			default: break;
			}
			return; // done
		}
	}
}

void Ermine::scripting::ScriptEngine::PullManagedFieldsToCache(MonoObject* obj,
	std::unordered_map<std::string, ScriptFieldValue>& cache)
{
	if (!obj) return;
	MonoClass* klass = mono_object_get_class(obj);
	auto fields = DiscoverScriptFields(klass);

	for (const auto& f : fields)
	{
		switch (f.kind)
		{
		case ScriptFieldInfo::Kind::Float:
		{
			float v = 0.0f; mono_field_get_value(obj, f.field, &v);
			cache[f.name] = Ermine::ScriptFieldValue::MakeFloat(v);
			break;
		}
		case ScriptFieldInfo::Kind::Int:
		{
			int v = 0; mono_field_get_value(obj, f.field, &v);
			cache[f.name] = Ermine::ScriptFieldValue::MakeInt(v);
			break;
		}
		case ScriptFieldInfo::Kind::Bool:
		{
			mono_bool v = 0; mono_field_get_value(obj, f.field, &v);
			cache[f.name] = Ermine::ScriptFieldValue::MakeBool(v != 0);
			break;
		}
		case ScriptFieldInfo::Kind::String:
		{
			MonoString* ms = nullptr; mono_field_get_value(obj, f.field, &ms);
			std::string s; ToTempUTF8(ms, s);
			cache[f.name] = Ermine::ScriptFieldValue::MakeString(std::move(s));
			break;
		}
		case ScriptFieldInfo::Kind::Vector3:
		{
			ManagedVector3 mv{ 0,0,0 }; mono_field_get_value(obj, f.field, &mv);
			cache[f.name] = Ermine::ScriptFieldValue::MakeVec3(Ermine::Vec3{ mv.x, mv.y, mv.z });
			break;
		}
		case ScriptFieldInfo::Kind::Quaternion:
		{
			ManagedQuaternion mq{ 0,0,0,1 }; mono_field_get_value(obj, f.field, &mq);
			cache[f.name] = Ermine::ScriptFieldValue::MakeQuat(Ermine::Quaternion{ mq.x, mq.y, mq.z, mq.w });
			break;
		}
		default: break;
		}
	}
}

void Ermine::scripting::ScriptEngine::PushCacheToManagedFields(MonoObject* obj, const std::unordered_map<std::string, ScriptFieldValue>& cache)
{
	if (!obj || cache.empty()) return;
	MonoClass* klass = mono_object_get_class(obj);
	auto fields = DiscoverScriptFields(klass);

	// name -> Mono field
	std::unordered_map<std::string, MonoClassField*> monoByName;
	std::unordered_map<std::string, ScriptFieldInfo::Kind> kindByName;
	monoByName.reserve(fields.size());
	kindByName.reserve(fields.size());
	for (const auto& f : fields)
	{
		monoByName[f.name] = f.field;
		kindByName[f.name] = f.kind;
	}

	for (const auto& kv : cache)
	{
		const auto it = monoByName.find(kv.first);
		if (it == monoByName.end()) continue;

		const auto kindIt = kindByName.find(kv.first);
		if (kindIt == kindByName.end()) continue;

		MonoClassField* fld = it->second;
		auto kind = kindIt->second;
		const auto& val = kv.second;

		switch (kind)
		{
		case ScriptFieldInfo::Kind::Float:
			if (val.kind == Ermine::ScriptFieldValue::Kind::Float) mono_field_set_value(obj, fld, const_cast<float*>(&std::get<float>(val.value)));
			break;
		case ScriptFieldInfo::Kind::Int:
			if (val.kind == Ermine::ScriptFieldValue::Kind::Int) mono_field_set_value(obj, fld, const_cast<int*>(&std::get<int>(val.value)));
			break;
		case ScriptFieldInfo::Kind::Bool:
		{
			if (val.kind == Ermine::ScriptFieldValue::Kind::Bool) { mono_bool mb = std::get<bool>(val.value) ? 1 : 0; mono_field_set_value(obj, fld, &mb); }
			break;
		}
		case ScriptFieldInfo::Kind::String:
		{
			if (val.kind == Ermine::ScriptFieldValue::Kind::String)
			{
				MonoString* ms = mono_string_new(mono_domain_get(), std::get<std::string>(val.value).c_str());
				mono_field_set_value(obj, fld, ms);
			}
			break;
		}
		case ScriptFieldInfo::Kind::Vector3:
		{
			if (val.kind == Ermine::ScriptFieldValue::Kind::Vector3)
			{
				ManagedVector3 mv{ .x = std::get<Vec3>(val.value).x, .y = std::get<Vec3>(val.value).y, .z = std::get<Vec3>(val.value).z };
				mono_field_set_value(obj, fld, &mv);
			}
			break;
		}
		case ScriptFieldInfo::Kind::Quaternion:
		{
			if (val.kind == Ermine::ScriptFieldValue::Kind::Quaternion)
			{
				ManagedQuaternion mq{ .x = std::get<Quaternion>(val.value).x, .y = std::get<Quaternion>(val.value).y, .z =
					std::get<Quaternion>(val.value).z, .w = std::get<Quaternion>(val.value).w };
				mono_field_set_value(obj, fld, &mq);
			}
			break;
		}
		default: break;
		}
	}
}

void Ermine::scripting::ScriptEngine::RegisterInternalCalls() const
{
	s_APIImage = mono_assembly_get_image(m_apiAsm);
	s_GameObjectClass = GetAPIClass("ErmineEngine", "GameObject");
	s_TransformClass = GetAPIClass("ErmineEngine", "Transform");
	s_RigidbodyClass = GetAPIClass("ErmineEngine", "Rigidbody");
	s_ComponentClass = GetAPIClass("ErmineEngine", "Component");
	s_MonoBehaviourClass = GetAPIClass("ErmineEngine", "MonoBehaviour");
	s_AudioComponentClass = GetAPIClass("ErmineEngine", "AudioComponent");
	s_SerializeFieldAttr = mono_class_from_name(s_APIImage, "ErmineEngine", "SerializeFieldAttribute");
	if (!s_SerializeFieldAttr)
		s_SerializeFieldAttr = mono_class_from_name(s_APIImage, "ErmineEngine", "SerializeField");
	s_AnimatorClass = GetAPIClass("ErmineEngine", "Animator");
	s_MaterialClass = GetAPIClass("ErmineEngine", "Material");
	s_UIImageComponentClass = GetAPIClass("ErmineEngine", "UIImage");

	s_ObjectClass = GetAPIClass("ErmineEngine", "Object");
	if (s_ObjectClass && !s_EntityIDField)
	{
		mono_class_init(s_ObjectClass);
		s_EntityIDField = mono_class_get_field_from_name(s_ObjectClass, "EntityID");
	}

#pragma region Transform ICalls
	mono_add_internal_call("ErmineEngine.Transform::get_position", (const void*)icall_transform_get_position);
	mono_add_internal_call("ErmineEngine.Transform::get_rotation", (const void*)icall_transform_get_rotation);
	mono_add_internal_call("ErmineEngine.Transform::get_scale", (const void*)icall_transform_get_scale);
	mono_add_internal_call("ErmineEngine.Transform::set_position", (const void*)icall_transform_set_position);
	mono_add_internal_call("ErmineEngine.Transform::set_rotation", (const void*)icall_transform_set_rotation);
	mono_add_internal_call("ErmineEngine.Transform::set_scale", (const void*)icall_transform_set_scale);

	mono_add_internal_call("ErmineEngine.Transform::get_childCount", (const void*)icall_transform_get_childcount);

	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldForward", (const void*)icall_transform_get_world_forward);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldRight", (const void*)icall_transform_get_world_right);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldUp", (const void*)icall_transform_get_world_up);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetParentTransform", (const void*)icall_transform_get_transform_parent);
	mono_add_internal_call("ErmineEngine.Transform::Internal_AddChild", (const void*)icall_transform_add_child);
	mono_add_internal_call("ErmineEngine.Transform::Internal_RemoveChild", (const void*)icall_transform_remove_child);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetChildTransformByName", (const void*)icall_transform_get_transform_by_name);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetChildTransformByIndex", (const void*)icall_transform_get_transform_by_index);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldPosition",(const void*)icall_transform_get_world_position);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldRotation",(const void*)icall_transform_get_world_rotation);
	mono_add_internal_call("ErmineEngine.Transform::Internal_GetWorldScale",(const void*)icall_transform_get_world_scale);
#pragma endregion

#pragma region Rigidbody ICalls
	mono_add_internal_call("ErmineEngine.Rigidbody::set_position", (const void*)icall_rigidbody_set_position);
	mono_add_internal_call("ErmineEngine.Rigidbody::set_rotation", (const void*)icall_rigidbody_set_rotation);
	mono_add_internal_call("ErmineEngine.Rigidbody::get_position", (const void*)icall_rigidbody_get_position);
	mono_add_internal_call("ErmineEngine.Rigidbody::get_rotation", (const void*)icall_rigidbody_get_rotation);

	mono_add_internal_call("ErmineEngine.Rigidbody::set_linearVelocity", (const void*)icall_rigidbody_set_linear_velocity);
	mono_add_internal_call("ErmineEngine.Rigidbody::get_linearVelocity", (const void*)icall_rigidbody_get_linear_velocity);
#pragma endregion

#pragma region Time ICalls
	mono_add_internal_call("ErmineEngine.Time::get_deltaTime", (const void*)icall_time_get_deltatime);
	mono_add_internal_call("ErmineEngine.Time::get_fixedDeltaTime", (const void*)icall_time_get_fixeddeltatime);
#pragma endregion

#pragma region Input ICalls
	mono_add_internal_call("ErmineEngine.Input::InternalGetKey", (const void*)icall_input_getkey);
	mono_add_internal_call("ErmineEngine.Input::InternalGetKeyDown", (const void*)icall_input_getkeydown);
	mono_add_internal_call("ErmineEngine.Input::InternalGetKeyUp", (const void*)icall_input_getkeyup);
	mono_add_internal_call("ErmineEngine.Input::InternalGetMouseButton", (const void*)icall_input_getmousebutton);
	mono_add_internal_call("ErmineEngine.Input::InternalGetMouseButtonDown", (const void*)icall_input_getmousebuttondown);
	mono_add_internal_call("ErmineEngine.Input::InternalGetMouseButtonUp", (const void*)icall_input_getmousebuttonrelease);

	mono_add_internal_call("ErmineEngine.Input::get_mousePosition", (const void*)icall_input_get_mouseposition);
	mono_add_internal_call("ErmineEngine.Input::get_mousePositionDelta", (const void*)icall_input_get_mousepositiondelta);
#pragma endregion

#pragma region GlobalAudio ICalls
	mono_add_internal_call("ErmineEngine.GlobalAudio::PlaySFX", (const void*)icall_globalaudio_play_sfx);
	mono_add_internal_call("ErmineEngine.GlobalAudio::PlaySFXWithReverb", (const void*)icall_globalaudio_play_sfx_with_reverb);
	mono_add_internal_call("ErmineEngine.GlobalAudio::StopSFX", (const void*)icall_globalaudio_stop_sfx);
	mono_add_internal_call("ErmineEngine.GlobalAudio::PlayMusic", (const void*)icall_globalaudio_play_music);
	mono_add_internal_call("ErmineEngine.GlobalAudio::SetMusicVolume", (const void*)icall_globalaudio_set_music_volume);
	mono_add_internal_call("ErmineEngine.GlobalAudio::SetSFXVolume", (const void*)icall_globalaudio_set_sfx_volume);
	mono_add_internal_call("ErmineEngine.GlobalAudio::PlayVoice", (const void*)icall_globalaudio_play_voice);
	mono_add_internal_call("ErmineEngine.GlobalAudio::StopVoice", (const void*)icall_globalaudio_stop_voice);
	mono_add_internal_call("ErmineEngine.GlobalAudio::SetVoiceVolume", (const void*)icall_globalaudio_set_voice_volume);
#pragma endregion

#pragma region AudioListener ICalls
	mono_add_internal_call("ErmineEngine.AudioListener::SetPosition", (const void*)icall_audiolistener_set_position);
	mono_add_internal_call("ErmineEngine.AudioListener::SetOrientation", (const void*)icall_audiolistener_set_orientation);
	mono_add_internal_call("ErmineEngine.AudioListener::SetAttributes", (const void*)icall_audiolistener_set_attributes);
#pragma endregion

#pragma region AudioComponent ICalls
	mono_add_internal_call("ErmineEngine.AudioComponent::get_shouldPlay", (const void*)icall_audiocomponent_get_shouldplay);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_shouldPlay", (const void*)icall_audiocomponent_set_shouldplay);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_shouldStop", (const void*)icall_audiocomponent_get_shouldstop);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_shouldStop", (const void*)icall_audiocomponent_set_shouldstop);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_isPlaying", (const void*)icall_audiocomponent_get_isplaying);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_isPlaying", (const void*)icall_audiocomponent_set_isplaying);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_volume", (const void*)icall_audiocomponent_get_volume);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_volume", (const void*)icall_audiocomponent_set_volume);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_soundName", (const void*)icall_audiocomponent_get_soundname);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_soundName", (const void*)icall_audiocomponent_set_soundname);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_playOnStart", (const void*)icall_audiocomponent_get_playonstart);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_playOnStart", (const void*)icall_audiocomponent_set_playonstart);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_is3D", (const void*)icall_audiocomponent_get_is3d);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_is3D", (const void*)icall_audiocomponent_set_is3d);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_minDistance", (const void*)icall_audiocomponent_get_mindistance);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_minDistance", (const void*)icall_audiocomponent_set_mindistance);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_maxDistance", (const void*)icall_audiocomponent_get_maxdistance);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_maxDistance", (const void*)icall_audiocomponent_set_maxdistance);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_followTransform", (const void*)icall_audiocomponent_get_followtransform);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_followTransform", (const void*)icall_audiocomponent_set_followtransform);
	// Reverb properties
	mono_add_internal_call("ErmineEngine.AudioComponent::get_useReverb", (const void*)icall_audiocomponent_get_usereverb);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_useReverb", (const void*)icall_audiocomponent_set_usereverb);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_reverbWetLevel", (const void*)icall_audiocomponent_get_reverbwetlevel);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_reverbWetLevel", (const void*)icall_audiocomponent_set_reverbwetlevel);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_reverbDryLevel", (const void*)icall_audiocomponent_get_reverbdrylevel);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_reverbDryLevel", (const void*)icall_audiocomponent_set_reverbdrylevel);
	mono_add_internal_call("ErmineEngine.AudioComponent::get_reverbDecayTime", (const void*)icall_audiocomponent_get_reverbdecaytime);
	mono_add_internal_call("ErmineEngine.AudioComponent::set_reverbDecayTime", (const void*)icall_audiocomponent_set_reverbdecaytime);
#pragma endregion

#pragma region Material ICalls
	mono_add_internal_call("ErmineEngine.Material::Internal_GetFill", (const void*)icall_material_get_fill);
	mono_add_internal_call("ErmineEngine.Material::Internal_SetFill", (const void*)icall_material_set_fill);
	mono_add_internal_call("ErmineEngine.Material::Internal_GetFlickerEmissive", (const void*)icall_material_get_flicker_emissive);
	mono_add_internal_call("ErmineEngine.Material::Internal_SetFlickerEmissive", (const void*)icall_material_set_flicker_emissive);
#pragma endregion

#pragma region Debug ICalls
	mono_add_internal_call("ErmineEngine.Debug::LogInternal", (const void*)icall_debug_log_info);
	mono_add_internal_call("ErmineEngine.Debug::LogWarningInternal", (const void*)icall_debug_log_warning);
	mono_add_internal_call("ErmineEngine.Debug::LogErrorInternal", (const void*)icall_debug_log_error);
#pragma endregion

#pragma region SceneManager ICalls
	mono_add_internal_call("ErmineEngine.SceneManager::LoadSceneInternal", (const void*)icall_scenemanager_loadscene);
#pragma endregion

#pragma region Application ICalls
	mono_add_internal_call("ErmineEngine.Application::QuitInternal", (const void*)icall_application_quit);
#pragma endregion

#pragma region Object ICalls
	mono_add_internal_call("ErmineEngine.Object::get_name", (const void*)icall_object_get_name);
	mono_add_internal_call("ErmineEngine.Object::set_name", (const void*)icall_object_set_name);
#pragma endregion

#pragma region Component ICalls
	mono_add_internal_call("ErmineEngine.Component::get_tag", (const void*)icall_component_get_tag);
	mono_add_internal_call("ErmineEngine.Component::set_tag", (const void*)icall_component_set_tag);
#pragma endregion

#pragma region GameObject ICalls
	mono_add_internal_call("ErmineEngine.GameObject::Internal_CreateGameObject", (const void*)icall_gameobject_creategameobject);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_GetTransform", (const void*)icall_gameobject_get_transform);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_GetActive", (const void*)icall_gameobject_get_active);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_SetActive", (const void*)icall_gameobject_set_active);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_AddComponent", (const void*)icall_gameobject_add_component);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_GetComponent", (const void*)icall_gameobject_get_component);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_HasComponent", (const void*)icall_gameobject_has_component);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_RemoveComponent", (const void*)icall_gameobject_remove_component);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_FindByName", (const void*)icall_gameobject_find_by_name);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_FindWithTag", (const void*)icall_gameobject_find_with_tag);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_FindGameObjectsWithTag", (const void*)icall_gameobject_find_gameobjects_with_tag);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_Instantiate", (const void*)icall_gameobject_instantiate);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_Destroy", (const void*)icall_gameobject_destroy);

	mono_add_internal_call("ErmineEngine.GameObject::Internal_WrapExisting", (const void*)icall_gameobject_wrap_existing);
	mono_add_internal_call("ErmineEngine.GameObject::Internal_BindExisting", (const void*)icall_gameobject_bind_existing);
#pragma endregion

#pragma region Prefab ICalls
	mono_add_internal_call("ErmineEngine.Prefab::Internal_LoadPrefab", (const void*)icall_prefab_instantiate);
#pragma endregion

#pragma region StateMachine ICalls
	mono_add_internal_call("ErmineEngine.StateMachine::RequestNextState", (const void*)icall_statemachine_request_next_state);
	mono_add_internal_call("ErmineEngine.StateMachine::RequestPreviousState", (const void*)icall_statemachine_request_previous_state);
#pragma endregion

#pragma region NavAgent ICalls
	mono_add_internal_call("ErmineEngine.NavAgent::SetDestination",
		(const void*)+[](uint64_t entityID, glm::vec3 dest)
		{
			Ermine::Vec3 v;
			v.x = dest.x;
			v.y = dest.y;
			v.z = dest.z;
			Ermine::RequestPathForAgent((Ermine::EntityID)entityID, v);
		});
	mono_add_internal_call("ErmineEngine.NavAgent::StartJump", (const void*)icall_navagent_start_jump);
	mono_add_internal_call("ErmineEngine.NavAgent::SetAutoRotate", (void*)icall_navagent_set_auto_rotate);
#pragma endregion

#pragma region Physics ICalls
	mono_add_internal_call("ErmineEngine.Physics::SetPosition", (const void*)icall_Physics_SetPosition);
	mono_add_internal_call("ErmineEngine.Physics::SetRotationEuler", (const void*)icall_Physics_SetRotationEuler);
	mono_add_internal_call("ErmineEngine.Physics::SetRotationQuat", (const void*)icall_Physics_SetRotationQuat);
	mono_add_internal_call("ErmineEngine.Physics::MoveEuler", (const void*)icall_Physics_MoveEuler);
	mono_add_internal_call("ErmineEngine.Physics::MoveQuat", (const void*)icall_Physics_MoveQuat);
	mono_add_internal_call("ErmineEngine.Physics::Internal_Raycast", (const void*)icall_physics_raycast);
	mono_add_internal_call("ErmineEngine.Physics::Internal_GetTransform", (const void*)icall_physics_gettransform);
	mono_add_internal_call("ErmineEngine.Physics::Internal_GetRigidbody", (const void*)icall_rigidbody_get_rigidbody);
	mono_add_internal_call("ErmineEngine.Physics::RemovePhysic", (const void*)&icall_Physics_RemovePhysic);
	mono_add_internal_call("ErmineEngine.Physics::Jump", (const void*)icall_Physics_Jump);
	mono_add_internal_call("ErmineEngine.Physics::CheckMotionType", (const void*)icall_Physics_CheckMotionType);
	mono_add_internal_call("ErmineEngine.Physics::ForceUpdate", (const void*)icall_Physics_ForceUpdate);
	mono_add_internal_call("ErmineEngine.Physics::HasPhysicComp", (const void*)icall_Physics_HasPhysicComp);
	mono_add_internal_call("ErmineEngine.Physics::Internal_SetLightValue",(const void*)icall_physics_set_light_value);
	mono_add_internal_call("ErmineEngine.Physics::Internal_GetLightValue",(const void*)icall_physics_get_light_value);
#pragma endregion

#pragma region Cursor ICalls
	mono_add_internal_call("ErmineEngine.Cursor::set_visible", (const void*)icall_cursor_set_visible);
	mono_add_internal_call("ErmineEngine.Cursor::get_visible", (const void*)icall_cursor_get_visible);
	mono_add_internal_call("ErmineEngine.Cursor::set_lockState", (const void*)icall_cursor_set_locked);
	mono_add_internal_call("ErmineEngine.Cursor::get_lockState", (const void*)icall_cursor_get_locked);

#pragma endregion

#pragma region UI ICalls
	mono_add_internal_call("ErmineEngine.GameplayHUD::Internal_GetHealth", (const void*)Internal_GetHealth);
	mono_add_internal_call("ErmineEngine.GameplayHUD::Internal_SetHealth", (const void*)Internal_SetHealth);
	mono_add_internal_call("ErmineEngine.GameplayHUD::Internal_GetMaxHealth", (const void*)Internal_GameplayHUD_GetMaxHealth);
	mono_add_internal_call("ErmineEngine.GameplayHUD::Internal_GetRegenRate", (const void*)Internal_GameplayHUD_GetRegenRate);
	// temporary reference to health bar, to be removed
	mono_add_internal_call("ErmineEngine.GameplayHUD::Internal_GetHealthBar", Internal_GetHealthBar);

	// New UI Component Bindings
	mono_add_internal_call("ErmineEngine.UIHealthbar::Internal_GetHealth", (const void*)Internal_Healthbar_GetHealth);
	mono_add_internal_call("ErmineEngine.UIHealthbar::Internal_SetHealth", (const void*)Internal_Healthbar_SetHealth);
	mono_add_internal_call("ErmineEngine.UIHealthbar::Internal_GetMaxHealth", (const void*)Internal_Healthbar_GetMaxHealth);
	mono_add_internal_call("ErmineEngine.UIBookCounter::Internal_GetCollected", (const void*)Internal_BookCounter_GetCollected);
	mono_add_internal_call("ErmineEngine.UIBookCounter::Internal_SetCollected", (const void*)Internal_BookCounter_SetCollected);
	mono_add_internal_call("ErmineEngine.UIBookCounter::Internal_AddBook", (const void*)Internal_BookCounter_AddBook);
	mono_add_internal_call("ErmineEngine.UIBookCounter::Internal_GetTotal", (const void*)Internal_BookCounter_GetTotal);

	mono_add_internal_call("ErmineEngine.UIImage::get_position", (const void*)icall_uiimage_get_position);
	mono_add_internal_call("ErmineEngine.UIImage::set_position", (const void*)icall_uiimage_set_position);

#pragma endregion UI ICalls

#pragma region UISystem ICalls
	mono_add_internal_call("ErmineEngine.UISystem::Internal_CastSkill",
		(const void*)+[](uint64_t entityID, int skillIndex) -> bool
		{
			auto uiSystem = Ermine::ECS::GetInstance().GetSystem<Ermine::UIRenderSystem>();
			if (!uiSystem)
				return false;
			return uiSystem->CastSkill((Ermine::EntityID)entityID, skillIndex);
		});

	// Set skill selected state by entity name
	mono_add_internal_call("ErmineEngine.UISystem::Internal_SetSkillSelected",
		(const void*)+[](MonoString* entityNameMono, int skillIndex, bool isSelected)
		{
			if (!entityNameMono) return;
			char* entityName = mono_string_to_utf8(entityNameMono);

			auto& ecs = Ermine::ECS::GetInstance();
			for (Ermine::EntityID e = 0; e < 10000; ++e)
			{
				if (!ecs.IsEntityValid(e)) continue;
				if (!ecs.HasComponent<Ermine::ObjectMetaData>(e)) continue;
				if (!ecs.HasComponent<Ermine::UISkillsComponent>(e)) continue;

				auto& meta = ecs.GetComponent<Ermine::ObjectMetaData>(e);
				if (meta.name == entityName)
				{
					auto& skills = ecs.GetComponent<Ermine::UISkillsComponent>(e);
					if (skillIndex >= 0 && skillIndex < static_cast<int>(skills.skills.size()))
					{
						skills.skills[skillIndex].isSelected = isSelected;
					}
					break;
				}
			}
			mono_free(entityName);
		});

	// Get skill selected state by entity name
	mono_add_internal_call("ErmineEngine.UISystem::Internal_GetSkillSelected",
		(const void*)+[](MonoString* entityNameMono, int skillIndex) -> bool
		{
			if (!entityNameMono) return false;
			char* entityName = mono_string_to_utf8(entityNameMono);

			auto& ecs = Ermine::ECS::GetInstance();
			for (Ermine::EntityID e = 0; e < 10000; ++e)
			{
				if (!ecs.IsEntityValid(e)) continue;
				if (!ecs.HasComponent<Ermine::ObjectMetaData>(e)) continue;
				if (!ecs.HasComponent<Ermine::UISkillsComponent>(e)) continue;

				auto& meta = ecs.GetComponent<Ermine::ObjectMetaData>(e);
				if (meta.name == entityName)
				{
					auto& skills = ecs.GetComponent<Ermine::UISkillsComponent>(e);
					if (skillIndex >= 0 && skillIndex < static_cast<int>(skills.skills.size()))
					{
						mono_free(entityName);
						return skills.skills[skillIndex].isSelected;
					}
					break;
				}
			}
			mono_free(entityName);
			return false;
		});

	// Select only one skill (deselect all others)
	mono_add_internal_call("ErmineEngine.UISystem::Internal_SelectOnlySkill",
		(const void*)+[](MonoString* entityNameMono, int skillIndexToSelect)
		{
			if (!entityNameMono) return;
			char* entityName = mono_string_to_utf8(entityNameMono);

			auto& ecs = Ermine::ECS::GetInstance();
			for (Ermine::EntityID e = 0; e < 10000; ++e)
			{
				if (!ecs.IsEntityValid(e)) continue;
				if (!ecs.HasComponent<Ermine::ObjectMetaData>(e)) continue;
				if (!ecs.HasComponent<Ermine::UISkillsComponent>(e)) continue;

				auto& meta = ecs.GetComponent<Ermine::ObjectMetaData>(e);
				if (meta.name == entityName)
				{
					auto& skills = ecs.GetComponent<Ermine::UISkillsComponent>(e);
					for (size_t i = 0; i < skills.skills.size(); ++i)
					{
						skills.skills[i].isSelected = (static_cast<int>(i) == skillIndexToSelect);
					}
					break;
				}
			}
			mono_free(entityName);
		});
#pragma endregion

#pragma region VideoManager ICalls
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_LoadVideo", (const void*)icall_videomanager_load);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_SetCurrentVideo", (const void*)icall_videomanager_set_current);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_GetCurrentVideo", (const void*)icall_videomanager_get_current);

	mono_add_internal_call("ErmineEngine.VideoManager::Internal_Play", (const void*)icall_videomanager_play);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_Pause", (const void*)icall_videomanager_pause);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_Stop", (const void*)icall_videomanager_stop);

	mono_add_internal_call("ErmineEngine.VideoManager::Internal_IsPlaying", (const void*)icall_videomanager_is_playing);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_IsDonePlaying", (const void*)icall_videomanager_is_done);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_VideoExists", (const void*)icall_videomanager_exists);

	mono_add_internal_call("ErmineEngine.VideoManager::Internal_FreeVideo", (const void*)icall_videomanager_free);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_CleanupAllVideos", (const void*)icall_videomanager_cleanup_all);

	mono_add_internal_call("ErmineEngine.VideoManager::Internal_SetFitMode", (const void*)icall_videomanager_set_fit_mode);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_GetFitMode", (const void*)icall_videomanager_get_fit_mode);

	mono_add_internal_call("ErmineEngine.VideoManager::Internal_SetRenderEnabled", (const void*)icall_videomanager_set_render_enabled);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_GetRenderEnabled", (const void*)icall_videomanager_get_render_enabled);
	mono_add_internal_call("ErmineEngine.VideoManager::Internal_SetAudioVolume", (const void*)icall_videomanager_set_audio_volume);
#pragma endregion VideoManager ICalls

#pragma region Animation ICalls
	mono_add_internal_call("ErmineEngine.Animator::Internal_GetBool", (const void*)icall_animator_get_bool);
	mono_add_internal_call("ErmineEngine.Animator::Internal_SetBool", (const void*)icall_animator_set_bool);

	mono_add_internal_call("ErmineEngine.Animator::Internal_GetFloat", (const void*)icall_animator_get_float);
	mono_add_internal_call("ErmineEngine.Animator::Internal_SetFloat", (const void*)icall_animator_set_float);

	mono_add_internal_call("ErmineEngine.Animator::Internal_GetInt", (const void*)icall_animator_get_int);
	mono_add_internal_call("ErmineEngine.Animator::Internal_SetInt", (const void*)icall_animator_set_int);

	mono_add_internal_call("ErmineEngine.Animator::Internal_SetTrigger", (const void*)icall_animator_set_trigger);

	mono_add_internal_call("ErmineEngine.Animator::Internal_GetCurrentStateName", (const void*)icall_animator_get_current_state);

	mono_add_internal_call("ErmineEngine.Animator::Internal_SetState", (const void*)icall_animator_set_state);
#pragma endregion

#pragma region PostEffects ICalls
	mono_add_internal_call("ErmineEngine.PostEffects::SetExposure", (const void*)icall_posteffects_set_exposure);
	mono_add_internal_call("ErmineEngine.PostEffects::SetContrast", (const void*)icall_posteffects_set_contrast);
	mono_add_internal_call("ErmineEngine.PostEffects::SetSaturation", (const void*)icall_posteffects_set_saturation);
	mono_add_internal_call("ErmineEngine.PostEffects::SetGamma", (const void*)icall_posteffects_set_gamma);
	mono_add_internal_call("ErmineEngine.PostEffects::SetBloomStrength", (const void*)icall_posteffects_set_bloomStrength);
	mono_add_internal_call("ErmineEngine.PostEffects::SetGrainIntensity", (const void*)icall_posteffects_set_grainintensity);
	mono_add_internal_call("ErmineEngine.PostEffects::SetGrainSize", (const void*)icall_posteffects_set_grainsize);
	mono_add_internal_call("ErmineEngine.PostEffects::SetChromaticAberrationIntensity", (const void*)icall_posteffects_set_chromaticaberration);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteEnabled", (const void*)icall_posteffects_get_vignette_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteEnabled", (const void*)icall_posteffects_set_vignette_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::GetFlimGrainEnabled", (const void*)icall_posteffects_get_flim_grain_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::SetFlimGrainEnabled", (const void*)icall_posteffects_set_flim_grain_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::GetChromaticAberrationEnabled", (const void*)icall_posteffects_get_chromatic_aberration_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::SetChromaticAberrationEnabled", (const void*)icall_posteffects_set_chromatic_aberration_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::GetBloomEnabled", (const void*)icall_posteffects_get_bloom_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::SetBloomEnabled", (const void*)icall_posteffects_set_bloom_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteIntensity", (const void*)icall_postprocess_get_vignette_intensity);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteIntensity", (const void*)icall_postprocess_set_vignette_intensity);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteRadius", (const void*)icall_postprocess_get_vignette_radius);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteRadius", (const void*)icall_postprocess_set_vignette_radius);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteCoverage", (const void*)icall_postprocess_get_vignette_coverage);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteCoverage", (const void*)icall_postprocess_set_vignette_coverage);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteFalloff", (const void*)icall_postprocess_get_vignette_falloff);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteFalloff", (const void*)icall_postprocess_set_vignette_falloff);
	mono_add_internal_call("ErmineEngine.PostEffects::GetVignetteMapStrength", (const void*)icall_postprocess_get_vignette_map_strength);
	mono_add_internal_call("ErmineEngine.PostEffects::SetVignetteMapStrength", (const void*)icall_postprocess_set_vignette_map_strength);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_GetVignetteMapRGBModifier", (const void*)icall_postprocess_get_vignette_map_rgb_modifier);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_SetVignetteMapRGBModifier", (const void*)icall_postprocess_set_vignette_map_rgb_modifier);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_GetVignetteMapPath", (const void*)icall_postprocess_get_vignette_map_path);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_SetVignetteMapPath", (const void*)icall_postprocess_set_vignette_map_path);
	mono_add_internal_call("ErmineEngine.PostEffects::GetRadialBlurEnabled", (const void*)icall_postprocess_get_radial_blur_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::SetRadialBlurEnabled", (const void*)icall_postprocess_set_radial_blur_enabled);
	mono_add_internal_call("ErmineEngine.PostEffects::GetRadialBlurStrength", (const void*)icall_postprocess_get_radial_blur_strength);
	mono_add_internal_call("ErmineEngine.PostEffects::SetRadialBlurStrength", (const void*)icall_postprocess_set_radial_blur_strength);
	mono_add_internal_call("ErmineEngine.PostEffects::GetRadialBlurSamples", (const void*)icall_postprocess_get_radial_blur_samples);
	mono_add_internal_call("ErmineEngine.PostEffects::SetRadialBlurSamples", (const void*)icall_postprocess_set_radial_blur_samples);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_GetRadialBlurCenter", (const void*)icall_postprocess_get_radial_blur_center);
	mono_add_internal_call("ErmineEngine.PostEffects::Internal_SetRadialBlurCenter", (const void*)icall_postprocess_set_radial_blur_center);
#pragma endregion
}
