/* Start Header ************************************************************************/
/*!
\file       GPUProfiler.cpp
\author     Created for Ermine Engine
\date       21/04/2025
\brief      This file contains the implementation of the GPU Profiling system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "GPUProfiler.h"

#include "FrameController.h"

using namespace Ermine::graphics;

void GPUProfiler::Init(size_t historyLength)
{
    if (s_Initialized)
        return;

    s_MaxHistoryLength = historyLength;
    s_FrameTimeHistory.clear();
    s_TimerQueries.clear();
    s_EventNames.clear();
    s_EventQueryActive.clear();
    s_MemoryByType.clear();

    // Reset metrics
    s_CurrentMetrics = PerformanceMetrics{};

    // Check if timer query is supported
    if (IsTimerQuerySupported())
    {
        EE_CORE_INFO("GPU timer queries are supported. GPU timing metrics will be available.");
    }
    else
    {
        EE_CORE_WARN("GPU timer queries are not supported. GPU timing metrics will not be available.");
    }

    s_Initialized = true;
}

void GPUProfiler::Shutdown()
{
    if (!s_Initialized)
        return;

    // Delete any remaining timer queries
    for (auto query : s_TimerQueries)
    {
        if (query != 0)
            glDeleteQueries(1, &query);
    }

    s_TimerQueries.clear();
    s_EventNames.clear();
    s_EventQueryActive.clear();
    s_FrameTimeHistory.clear();

    s_Initialized = false;
}

void GPUProfiler::BeginFrame()
{
    if (!s_Initialized)
        return;

    // Process any pending timer queries from previous frames
    ProcessTimerQueries();

    // Reset frame statistics
    s_CurrentMetrics.drawCallCount = 0;
    s_CurrentMetrics.triangleCount = 0;
    s_CurrentMetrics.vertexCount = 0;

    // Start frame timing
    s_FrameStartTime = std::chrono::high_resolution_clock::now();

    // Make sure there are no active queries when starting a new frame
    for (size_t i = 0; i < s_EventQueryActive.size(); i++) {
        if (s_EventQueryActive[i]) {
            EE_CORE_WARN("Found active query at BeginFrame. Forcibly ending it.");
            glEndQuery(GL_TIME_ELAPSED);
            s_EventQueryActive[i] = false;
        }
    }
}

void GPUProfiler::EndFrame()
{
    if (!s_Initialized)
        return;

    // End GPU timing for the entire frame
    if (IsTimerQuerySupported())
        EndEvent();

    // Calculate frame time
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    float processTimeMs = std::chrono::duration<float, std::milli>(frameEndTime - s_FrameStartTime).count();

    // Includes v-sync and any pacing (present-to-present time)
    const float effectiveTimeMs = FrameController::GetDeltaTime() * 1000.0f;

    // Update metrics with this frame's data
    UpdateMetrics(effectiveTimeMs);

    s_CurrentMetrics.cpuFrameTimeMs = processTimeMs;

    // Add to history
    s_FrameTimeHistory.push_back(effectiveTimeMs);
    if (s_FrameTimeHistory.size() > s_MaxHistoryLength)
        s_FrameTimeHistory.pop_front();
}

bool GPUProfiler::IsTimerQuerySupported()
{
    // Check for GL_ARB_timer_query extension or OpenGL 3.3+ which includes this functionality
    if (!GLAD_GL_ARB_timer_query && !(GLVersion.major > 3 || (GLVersion.major == 3 && GLVersion.minor >= 3)))
        return false;

    // Test if we can actually create a query
    GLuint testQuery = 0;
    glGenQueries(1, &testQuery);
    bool supported = (testQuery != 0 && glGetError() == GL_NO_ERROR);
    if (testQuery)
        glDeleteQueries(1, &testQuery);

    return supported;
}

void GPUProfiler::BeginEvent(const std::string& name)
{
    if (!s_Initialized || !IsTimerQuerySupported())
        return;

    // Check if we already have an active query - don't allow nested queries
    for (size_t i = 0; i < s_EventQueryActive.size(); ++i) {
        if (s_EventQueryActive[i]) {
            EE_CORE_WARN("Attempt to begin timer query '{0}' while another query is active. Nested queries are not supported.", name);
            return;
        }
    }

    // Create a new query object
    GLuint query;
    glGenQueries(1, &query);

    // Start the timer query
    glBeginQuery(GL_TIME_ELAPSED, query);

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        EE_CORE_ERROR("OpenGL error in BeginEvent: {0} (Event: {1})", error, name);
        return;
    }

    // Store the query info
    s_TimerQueries.push_back(query);
    s_EventNames.push_back(name);
    s_EventQueryActive.push_back(true);
}

void GPUProfiler::EndEvent()
{
    if (!s_Initialized || !IsTimerQuerySupported())
        return;

    // Find the last active query
    for (int i = static_cast<int>(s_TimerQueries.size()) - 1; i >= 0; --i)
    {
        if (s_EventQueryActive[i])
        {
            // End the query
            glEndQuery(GL_TIME_ELAPSED);
            s_EventQueryActive[i] = false;
            break;
        }
    }
}

void GPUProfiler::TrackDrawCall(uint32_t vertexCount, uint32_t indexCount)
{
    if (!s_Initialized)
        return;

    s_CurrentMetrics.drawCallCount++;
    //s_CurrentMetrics.vertexCount += vertexCount;

    // If indexCount is provided, calculate triangles
    if (indexCount > 0)
    {
        s_CurrentMetrics.vertexCount += indexCount / 4; // vertices processed ~= indices submitted
        s_CurrentMetrics.triangleCount += indexCount / 4 / 3; // triangle list
    }
    else
    {
        s_CurrentMetrics.vertexCount += vertexCount; // vertices processed
        s_CurrentMetrics.triangleCount += vertexCount / 3; // triangle list
    }
    //s_CurrentMetrics.triangleCount += indexCount / 3;
}

void GPUProfiler::TrackMemoryAllocation(uint64_t sizeBytes, const std::string& type)
{
    if (!s_Initialized)
        return;

    // Add to memory type counter
    s_MemoryByType[type] += sizeBytes;

    // Update memory metrics
    if (type == "Texture")
        s_CurrentMetrics.textureMemoryMB = s_MemoryByType[type] / (1024 * 1024);
    else if (type == "Buffer")
        s_CurrentMetrics.bufferMemoryMB = s_MemoryByType[type] / (1024 * 1024);

    // Update total VRAM usage
    uint64_t totalBytes = 0;
    for (const auto& [memType, bytes] : s_MemoryByType)
        totalBytes += bytes;

    s_CurrentMetrics.totalVRAMUsageMB = totalBytes / (1024 * 1024);
}

void GPUProfiler::TrackMemoryDeallocation(uint64_t sizeBytes, const std::string& type)
{
    if (!s_Initialized)
        return;

    // Subtract from memory type counter
    if (s_MemoryByType.find(type) != s_MemoryByType.end())
    {
        s_MemoryByType[type] = (s_MemoryByType[type] > sizeBytes) ? (s_MemoryByType[type] - sizeBytes) : 0;

        // Update memory metrics
        if (type == "Texture")
            s_CurrentMetrics.textureMemoryMB = s_MemoryByType[type] / (1024 * 1024);
        else if (type == "Buffer")
            s_CurrentMetrics.bufferMemoryMB = s_MemoryByType[type] / (1024 * 1024);

        // Update total VRAM usage
        uint64_t totalBytes = 0;
        for (const auto& [memType, bytes] : s_MemoryByType)
            totalBytes += bytes;

        s_CurrentMetrics.totalVRAMUsageMB = totalBytes / (1024 * 1024);
    }
}

void GPUProfiler::SetCulledMeshesCount(uint32_t count)
{
    if (!s_Initialized)
        return;
    s_CurrentMetrics.culledMeshes = static_cast<uint32_t>(count);
}

void GPUProfiler::ProcessTimerQueries()
{
    if (!s_Initialized || !IsTimerQuerySupported() || s_TimerQueries.empty())
        return;

    // Check if queries are available
    for (size_t i = 0; i < s_TimerQueries.size(); i++)
    {
        if (!s_EventQueryActive[i])
        {
            GLint available = 0;
            glGetQueryObjectiv(s_TimerQueries[i], GL_QUERY_RESULT_AVAILABLE, &available);

            if (available)
            {
                // Get the query result (time in nanoseconds)
                GLuint64 timeElapsed;
                glGetQueryObjectui64v(s_TimerQueries[i], GL_QUERY_RESULT, &timeElapsed);

                // Convert to milliseconds
                float timeMs = static_cast<float>(timeElapsed) / 1000000.0f;

                // Store the result
                if (s_EventNames[i] == "Frame")
                {
                    s_CurrentMetrics.gpuFrameTimeMs = timeMs;
                }

                // Clean up the query
                glDeleteQueries(1, &s_TimerQueries[i]);

                // Remove this query from tracking
                s_TimerQueries.erase(s_TimerQueries.begin() + i);
                s_EventNames.erase(s_EventNames.begin() + i);
                s_EventQueryActive.erase(s_EventQueryActive.begin() + i);
                i--; // Adjust index since we removed an element
            }
        }
    }
}

void GPUProfiler::UpdateMetrics(float frameTimeMs)
{
    s_CurrentMetrics.frameTimeMs = frameTimeMs;
    s_CurrentMetrics.cpuFrameTimeMs = frameTimeMs;

    // Calculate FPS
    s_CurrentMetrics.fps = frameTimeMs > 0.0f ? 1000.0f / frameTimeMs : 0.0f;

    // Update min/max frame time (avoid Windows min/max macros)
    s_CurrentMetrics.minFrameTimeMs = (std::min)(s_CurrentMetrics.minFrameTimeMs, frameTimeMs);
    s_CurrentMetrics.maxFrameTimeMs = (std::max)(s_CurrentMetrics.maxFrameTimeMs, frameTimeMs);

    // Calculate average frame time
    if (!s_FrameTimeHistory.empty())
    {
        float sum = 0.0f;
        for (float time : s_FrameTimeHistory)
            sum += time;
        s_CurrentMetrics.averageFrameTimeMs = sum / static_cast<float>(s_FrameTimeHistory.size());
    }
    else
    {
        s_CurrentMetrics.averageFrameTimeMs = frameTimeMs;
    }
}

const GPUProfiler::PerformanceMetrics& GPUProfiler::GetMetrics()
{
    return s_CurrentMetrics;
}

const std::deque<float>& GPUProfiler::GetFrameTimeHistory()
{
    return s_FrameTimeHistory;
}
