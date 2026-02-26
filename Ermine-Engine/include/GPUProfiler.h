/* Start Header ************************************************************************/
/*!
\file       GPUProfiler.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       21/04/2025
\brief      This file contains the declaration of the GPU Profiling system.
            Provides tools for measuring GPU performance metrics.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

#include "glad/glad.h"


namespace Ermine::graphics
{
	/**
	 * @brief The GPUProfiler class provides tools to measure GPU performance metrics.
	 */
	class GPUProfiler
	{
	public:
		/**
		 * @brief Performance metrics structure containing all tracked statistics.
		 */
		struct PerformanceMetrics
		{
			// Frame timing metrics
			float frameTimeMs = 0.0f;			// Frame time in milliseconds
			float averageFrameTimeMs = 0.0f;	// Average frame time in milliseconds
			float minFrameTimeMs = FLT_MAX;		// Minimum frame time in milliseconds
			float maxFrameTimeMs = 0.0f;		// Maximum frame time in milliseconds
			float fps = 0.0f;					// Frames per second

			// Draw call metrics
			uint32_t drawCallCount = 0;		// Number of draw calls
			uint32_t triangleCount = 0;			// Number of triangles drawn
			uint32_t vertexCount = 0;			// Number of vertices drawn
			uint32_t culledMeshes = 0;		// Number of culled entities

			// Memory Metrics
			uint64_t totalVRAMUsageMB = 0;		// Total VRAM usage in MB
			uint64_t textureMemoryMB = 0;		// Texture memory usage in MB
			uint64_t bufferMemoryMB = 0;		// Buffer memory usage in MB

			// GPU timing metrics
			float gpuFrameTimeMs = 0.0f;		// GPU frame time in milliseconds
			float cpuFrameTimeMs = 0.0f;		// CPU frame time in milliseconds
		};

	private:
		// Frame timing data
		inline static std::chrono::time_point<std::chrono::high_resolution_clock> s_FrameStartTime;
		inline static std::deque<float> s_FrameTimeHistory;
		inline static size_t s_MaxHistoryLength = 100;

		// GPU query objects for timing
		inline static std::vector<GLuint> s_TimerQueries;
		inline static std::vector<std::string> s_EventNames;
		inline static std::vector<bool> s_EventQueryActive;

		// Current frame metrics
		inline static PerformanceMetrics s_CurrentMetrics;

		// Memory tracking
		struct MemoryAllocation {
			std::string type;
			uint64_t sizeBytes;
		};
		inline static std::unordered_map<std::string, uint64_t> s_MemoryByType;

		// Whether the profiler has been initialized
		inline static bool s_Initialized = false;

		// Update current metrics based on collected data
		static void UpdateMetrics(float frameTimeMs);

		// Check if OpenGL timer queries are available
		static bool IsTimerQuerySupported();

		// Process pending timer query results
		static void ProcessTimerQueries();

	public:
		/**
		 * @brief Initializes the GPU profiler with a specified history length.
		 * @param historyLength The number of frames to keep in history for performance metrics.
		 */
		static void Init(size_t historyLength = 100);

		/**
		 * @brief Shutdown the GPU profiler and release resources.
		 */
		static void Shutdown();

		/**
		 * @brief Begin a new frame for profiling.
		 */
		static void BeginFrame();

		/**
		 * @brief End profiling the current frame
		 */
		static void EndFrame();

		/**
		 * @brief Begin profiling a specific GPU event using timer queries
		 * @param name Name of the event
		 */
		static void BeginEvent(const std::string& name);

		/**
		 * @brief End profiling a specific GPU event using timer queries
		 */
		static void EndEvent();

		/**
		 * @brief Track a new draw call
		 * @param vertexCount Number of vertices in the draw call
		 * @param indexCount Number of indices in the draw call
		 */
		static void TrackDrawCall(uint32_t vertexCount = 0, uint32_t indexCount = 0);

		/**
		 * @brief Track GPU memory allocation
		 * @param sizeBytes Size of allocation in bytes
		 * @param type Memory type (e.g., "Texture", "Buffer")
		 */
		static void TrackMemoryAllocation(uint64_t sizeBytes, const std::string& type);

		/**
		 * @brief Track GPU memory deallocation
		 * @param sizeBytes Size of deallocation in bytes
		 * @param type Memory type (e.g., "Texture", "Buffer")
		 */
		static void TrackMemoryDeallocation(uint64_t sizeBytes, const std::string& type);

		/**
		* @brief Set culled entities count
		* @param count Number of culled entities
		*/
		static void SetCulledMeshesCount(uint32_t count);

		/**
		 * @brief Get the current performance metrics
		 * @return Current performance metrics
		 */
		static const PerformanceMetrics& GetMetrics();

		/**
		 * @brief Get performance history
		 * @return Frame time history
		 */
		static const std::deque<float>& GetFrameTimeHistory();
	};
}
