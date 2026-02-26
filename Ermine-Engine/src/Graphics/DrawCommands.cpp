/* Start Header ************************************************************************/
/*!
\file       DrawCommands.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/10/2025
\brief      Implementation of buffer for DrawInfo data

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "DrawCommands.h"
#include "SSBO_Bindings.h"
#include "GPUProfiler.h"
#include "Logger.h"
#include <glad/glad.h>
#include <cstring>

namespace Ermine::graphics
{
    DrawInfoBuffer::~DrawInfoBuffer()
    {
        if (m_MappedPtr)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            m_MappedPtr = nullptr;
        }

        if (m_BufferID != 0)
        {
            GPUProfiler::TrackMemoryDeallocation(m_BufferSize, "DrawInfoBuffer");
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
        }
    }

    bool DrawInfoBuffer::Initialize(size_t maxDrawCalls)
    {
        // Clean up existing buffer if reinitializing
        if (m_BufferID != 0)
        {
            EE_CORE_INFO("DrawInfoBuffer: Cleaning up existing buffer before reinitialization");

            // Unmap if mapped
            if (m_MappedPtr)
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                m_MappedPtr = nullptr;
            }

            // Track deallocation and delete buffer
            GPUProfiler::TrackMemoryDeallocation(m_BufferSize, "DrawInfoBuffer");
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
            m_MaxDrawCalls = 0;
            m_DrawCount = 0;
            m_BufferSize = 0;
        }

        m_MaxDrawCalls = maxDrawCalls;
        m_BufferSize = maxDrawCalls * sizeof(DrawInfo);

        // Create buffer
        glGenBuffers(1, &m_BufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_BufferSize, nullptr, GL_DYNAMIC_DRAW);

        // Check for errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            EE_CORE_ERROR("Failed to create buffer for DrawInfo, error: {0}", error);
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
            return false;
        }

        // Don't map the buffer - we'll use glBufferSubData instead
        m_MappedPtr = nullptr;

        // Bind to SSBO binding point (DRAW_INFO_SSBO_BINDING)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_BufferID);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Track GPU memory allocation
        GPUProfiler::TrackMemoryAllocation(m_BufferSize, "DrawInfoBuffer");

        EE_CORE_INFO("Initialized DrawInfo buffer (glBufferSubData): {0} max draws, {1} bytes, binding {2}",
                     maxDrawCalls, m_BufferSize, DRAW_INFO_SSBO_BINDING);

        return true;
    }

    void DrawInfoBuffer::WriteDrawInfos(const std::vector<DrawInfo>& drawInfos, size_t offset)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("PersistentDrawInfoBuffer not initialized");
            return;
        }

        if (offset + drawInfos.size() > m_MaxDrawCalls)
        {
            EE_CORE_WARN("DrawInfo offset + count ({0}) exceeds max capacity ({1}), clamping",
                         offset + drawInfos.size(), m_MaxDrawCalls);
        }

        // Calculate how many draws to actually write
        size_t drawsToWrite = std::min(drawInfos.size(), m_MaxDrawCalls - offset);
        m_DrawCount = offset + drawsToWrite;

        size_t bytesToWrite = drawsToWrite * sizeof(DrawInfo);
        size_t byteOffset = offset * sizeof(DrawInfo);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOffset, bytesToWrite, drawInfos.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    // ============================================================================
    // DrawCommandBuffer Implementation
    // ============================================================================

    DrawCommandBuffer::~DrawCommandBuffer()
    {
        if (m_MappedPtr)
        {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            m_MappedPtr = nullptr;
        }

        if (m_BufferID != 0)
        {
            GPUProfiler::TrackMemoryDeallocation(m_BufferSize, "DrawCommandBuffer");
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
        }
    }

    bool DrawCommandBuffer::Initialize(size_t maxCommands)
    {
        // Clean up existing buffer if reinitializing
        if (m_BufferID != 0)
        {
            EE_CORE_INFO("DrawCommandBuffer: Cleaning up existing buffer before reinitialization");

            // Unmap if mapped
            if (m_MappedPtr)
            {
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                m_MappedPtr = nullptr;
            }

            // Track deallocation and delete buffer
            GPUProfiler::TrackMemoryDeallocation(m_BufferSize, "DrawCommandBuffer");
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
            m_MaxCommands = 0;
            m_CommandCount = 0;
            m_BufferSize = 0;
        }

        m_MaxCommands = maxCommands;
        m_BufferSize = maxCommands * sizeof(DrawElementsIndirectCommand);

        // Create buffer
        glGenBuffers(1, &m_BufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_BufferSize, nullptr, GL_DYNAMIC_DRAW);

        // Check for errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            EE_CORE_ERROR("Failed to create buffer for DrawCommands, error: {0}", error);
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
            return false;
        }

        // Don't map the buffer - we'll use glBufferSubData instead
        m_MappedPtr = nullptr;

        // Bind to SSBO binding point (DRAW_COMMANDS_SSBO_BINDING)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_COMMANDS_SSBO_BINDING, m_BufferID);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Track GPU memory allocation
        GPUProfiler::TrackMemoryAllocation(m_BufferSize, "DrawCommandBuffer");

        EE_CORE_INFO("Initialized DrawCommand buffer (glBufferSubData): {0} max commands, {1} bytes, binding {2}",
                     maxCommands, m_BufferSize, DRAW_COMMANDS_SSBO_BINDING);

        return true;
    }

    void DrawCommandBuffer::WriteCommands(const std::vector<DrawElementsIndirectCommand>& commands, size_t offset)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("PersistentDrawCommandBuffer not initialized");
            return;
        }

        if (offset + commands.size() > m_MaxCommands)
        {
            EE_CORE_WARN("DrawCommand offset + count ({0}) exceeds max capacity ({1}), clamping",
                         offset + commands.size(), m_MaxCommands);
        }

        if (commands.empty())
        {
            return; // Nothing to write
        }

        // Calculate how many commands to actually write
        size_t commandsToWrite = std::min(commands.size(), m_MaxCommands - offset);
        m_CommandCount = offset + commandsToWrite;

        size_t bytesToWrite = commandsToWrite * sizeof(DrawElementsIndirectCommand);
        size_t byteOffset = offset * sizeof(DrawElementsIndirectCommand);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, byteOffset, bytesToWrite, commands.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

} // namespace Ermine::graphics
