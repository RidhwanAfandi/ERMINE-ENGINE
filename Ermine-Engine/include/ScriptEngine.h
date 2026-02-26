/* Start Header ************************************************************************/
/*!
\file       ScriptEngine.h
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
#pragma once
#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/appdomain.h>

#include "Entity.h"

namespace Ermine
{
	struct ScriptFieldValue;
}

namespace Ermine::scripting
{
	class ScriptEngine
	{
		MonoDomain* m_coreDomain = nullptr;
		MonoDomain* m_gameDomain = nullptr;
		MonoAssembly* m_apiAsm = nullptr;
		MonoAssembly* m_gameAsm = nullptr;

		std::string m_apiAssemblyPath;
		std::string m_gameAssemblyPath;

		std::atomic<bool> m_watching{ false };
		std::atomic<bool> m_reloadRequested{ false };
		std::filesystem::file_time_type m_lastWriteTime{};

		std::string m_csprojPath;
		std::string m_sourceRoot;
		std::string m_buildDLLPath;
		std::string m_msbuildExe = "MSBuild.exe";
		std::string m_buildConfiguration = "Debug";
		std::string m_buildPlatform = "x64";

		std::atomic<bool> m_sourceWatching{ false };
		std::atomic<bool> m_buildRequested{ false };
		std::atomic<bool> m_buildInProgress{ false };
		std::filesystem::file_time_type m_lastSourcesStamp{};
		/**
		 * @brief Print all types in the given assembly.
		 * @param assembly The MonoAssembly to print types from.
		 */
		void PrintAssemblyTypes(MonoAssembly* assembly);
		/**
		 * @brief Read the bytes from a file.
		 * @param filepath The path to the file to read.
		 * @param outSize The size of the file in bytes.
		 * @return A pointer to the bytes read from the file. The caller is responsible for freeing the memory.
		 */
		char* ReadBytes(const std::string& filepath, uint32_t* outSize);
		/**
		 * @brief Load a C# assembly from the given path.
		 * @param assemblyPath The path to the assembly to load.
		 * @return A pointer to the loaded MonoAssembly, or nullptr if the assembly could not be loaded.
		 */
		MonoAssembly* LoadCSharpAssembly(const std::string& assemblyPath);
		/**
		 * @brief Register internal calls for Mono.
		 * This function should be called after the Mono runtime is initialized.
		 */
		void RegisterInternalCalls() const;
		/**
		 * @brief Reload the domains and assemblies.
		 * @return True if the domains and assemblies were reloaded successfully, false otherwise.
		 */
		bool ReloadDomainsAndAssemblies();
		/**
		 * @brief Initialize the DLL timestamp for hot-reloading.
		 * This function should be called after loading the game assembly.
		 */
		void InitializeDllTimestamp();

		/**
		 * @brief Get the full path to the MSBuild executable via vswhere.
		 * @return true if the path was resolved successfully, false otherwise.
		 */
		bool ResolveMSBuildPath();

#pragma region DLL Watcher jobs
		/**
		 * @brief Job Entry point for watching the game assembly for changes.
		 * @param param A pointer to the ScriptEngine instance
		 */
		static void WatcherJobEntry(uintptr_t param);
		/**
		 * @brief The function that is called periodically to check for changes in the game assembly.
		 */
		void WatcherTick();
		/**
		 * @brief Enqueue next tick of the watcher job.
		 */
		void ScheduleWatcherTick();
#pragma endregion

#pragma region Source Watcher + build jobs
		/**
		 * @brief Start watching the source files for changes.
		 * This will spawn a new thread that will monitor the source files for changes.
		 * When a change is detected, a build will be requested.
		 */
		static void SourceWatcherJobEntry(uintptr_t param);
		/**
		 * @brief The function that is called periodically to check for changes in the source files.
		 */
		void SourceWatcherTick();
		/**
		 * @brief Enqueue next tick of the source watcher job.
		 */
		void ScheduleSourceWatcherTick();
		/**
		 * @brief Start a build job to build the game assembly.
		 */
		static void BuildJobEntry(uintptr_t param);
		/**
		 * @brief Build the game assembly using MSBuild.
		 * @return True if the build was successful, false otherwise.
		 */
		bool BuildGameAssembly();
#pragma endregion
	public:
		/**
		 * @brief Initialize the Mono runtime and create the core and game domains.
		 * This function should be called before any other functions in this class.
		 * @param assembly_path The path to the C# assembly to load.
		 */
		void InitMono(const std::string& assembly_path);
		/**
		 * @brief Shutdown the Mono runtime and clean up resources.
		 */
		void Shutdown();
		/**
		 * @brief Load the game assembly from the given path. The dll is the one that contains the game logic.
		 * @param assemblyPath The path to the game assembly to load.
		 * @return A pointer to the loaded MonoAssembly, or nullptr if the assembly could not be loaded.
		 */
		MonoAssembly* LoadGameAssembly(const std::string& assemblyPath);
		/**
		 * @brief Reload the game assembly. This is useful for hot-reloading scripts during development.
		 * It will close the current game assembly and load a new one from the same path.
		 */
		void ReloadGameAssembly();

		// Jobsystem watcher
		/**
		 * @brief Start watching the game assembly for changes.
		 * This will spawn a new thread that will monitor the game assembly file for changes.
		 * When a change is detected, the game assembly will be reloaded.
		 */
		void StartWatchingGameAssembly();
		/**
		 * @brief Stop watching the game assembly for changes.
		 * This will stop the thread that is monitoring the game assembly file for changes.
		 */
		void StopWatchingGameAssembly();
		/**
		 * @brief Request a hot-reload of the game assembly.
		 * @param csprojPath the path to the .csproj file of the game assembly.
		 * @param sourceRoot the root directory of the source files.
		 * @param buildDLLPath
		 * @param configuration the build configuration to use (e.g. "Debug" or "Release").
		 * @param platform the build platform to use (e.g. "AnyCPU", "x86", "x64").
		 * @param msbuildExe the path to the MSBuild executable. Defaults to "MSBuild.exe".
		 */
		void ConfigureBuild(const std::string& csprojPath,
		                    const std::string& sourceRoot,
		                    const std::string& buildDLLPath,
		                    const std::string& configuration = "Debug",
		                    const std::string& platform = "AnyCPU", const std::string& msbuildExe = "MSBuild.exe");
		/**
		 * @brief Start watching the source files for changes.
		 */
		void StartWatchingScriptSources();
		/**
		 * @brief Stop watching the source files for changes.
		 */
		void StopWatchingScriptSources();
			
		/**
		 * @brief Process a hot-reload request if one is pending.
		 * This function should be called periodically in the main loop.
		 * It will call the pre and post functions before and after the reload respectively.
		 * @param pre A function to call before the reload. This can be used to clean up resources.
		 * @param post A function to call after the reload. This can be used to re-initialize resources.
		 */
		void ProcessHotReload(const std::function<void()>& pre, const std::function<void(bool success)>& post);

		/**
		 * @brief Pull the managed fields from a MonoObject into a cache.
		 * @param obj The MonoObject to pull the fields from.
		 * @param cache The cache to store the fields in.
		 */
		static void PullManagedFieldsToCache(MonoObject* obj, std::unordered_map<std::string, ScriptFieldValue>& cache);

		/**
		 * @brief Push the cached fields back to the MonoObject.
		 * @param obj The MonoObject to push the fields to.
		 * @param cache The cache to get the fields from.
		 */
		static void PushCacheToManagedFields(MonoObject* obj, const std::unordered_map<std::string, ScriptFieldValue>& cache);

		/**
		 * @brief Flush and destroy all entities that were queued for late destruction.
		 * This should be called at a safe point in the game loop to avoid issues with dangling references.
		 */
		void FlushLateDestroy();

		/**
		 * @brief Queue an entity for late destruction.
		 * The entity will be destroyed when FlushLateDestroy is called.
		 * @param id The EntityID of the entity to destroy.
		 */
		void QueueLateDestroy(EntityID id);

		/**
		 * @brief Set a single field on a MonoObject.
		 * @param obj the MonoObject to set the field on.
		 * @param name the name of the field to set.
		 * @param val the value to set the field to.
		 */
		void PushSingleField(MonoObject* obj,
		                                   const std::string& name,
		                                   const ScriptFieldValue& val);

		MonoAssembly* GetGameAsm() const { return m_gameAsm; }
		MonoDomain* GetGameDomain() const { assert(m_gameDomain != nullptr && "Game Domain missing?"); return m_gameDomain; }
		MonoAssembly* GetAPIAsm() const { return m_apiAsm; }
	};
}
