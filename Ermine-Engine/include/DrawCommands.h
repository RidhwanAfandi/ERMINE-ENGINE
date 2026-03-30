/* Start Header ************************************************************************/
/*!
\file       DrawCommands.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       25/10/2025
\brief      This file contains the declaration of draw command and draw info structs
            for the Ermine graphics system. These structures are used for indirect
            rendering and batch drawing operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <cstdint>
#include <glm/glm.hpp>

namespace Ermine::graphics
{
    // DrawInfo flag bits (must match shader constants)
    constexpr uint32_t FLAG_SKINNING = 1u << 0;        // bit 0: Skeletal animation enabled
    constexpr uint32_t FLAG_CAMERA_ATTACHED = 1u << 1; // bit 1: Object attached to camera (no motion blur)
    constexpr uint32_t FLAG_FLICKER_EMISSIVE = 1u << 2; // bit 2: Enable emissive flicker in the g-buffer pass

    /**
     * @brief DrawElementsIndirectCommand structure for glDrawElementsIndirect
     * Matches the OpenGL spec for indirect draw commands
     */
    struct DrawElementsIndirectCommand
    {
        uint32_t count;         // Number of elements (indices) to draw
        uint32_t instanceCount; // Number of instances to draw
        uint32_t firstIndex;    // Starting index in the index buffer
        uint32_t baseVertex;    // Value added to each index before indexing into the vertex buffer
        uint32_t baseInstance;  // Starting instance ID
    };

    /**
     * @brief DrawArraysIndirectCommand structure for glDrawArraysIndirect
     * Matches the OpenGL spec for indirect array draw commands
     */
    struct DrawArraysIndirectCommand
    {
        uint32_t count;         // Number of vertices to draw
        uint32_t instanceCount; // Number of instances to draw
        uint32_t first;         // Starting vertex index
        uint32_t baseInstance;  // Starting instance ID
    };

    /**
     * @brief Draw info structure containing metadata for a draw call
     * Used to pass additional information alongside draw commands
     * Note: This struct must be std140/std430 compatible for SSBO usage
     */
    struct DrawInfo
    {
        glm::mat4 modelMatrix;      // 64 bytes (offset 0-63) - Model transformation matrix
        glm::vec3 aabbMin;          // 12 bytes (offset 64-75) - AABB minimum bounds
        uint32_t materialIndex;     // 4 bytes (offset 76-79) - Index into the material SSBO
        glm::vec3 aabbMax;          // 12 bytes (offset 80-91) - AABB maximum bounds
        uint32_t entityID;          // 4 bytes (offset 92-95) - Entity ID for identification
        uint32_t flags;             // 4 bytes (offset 96-99) - Flags (bit 0: useSkinning, bits 1-31: reserved)
        uint32_t boneTransformOffset; // 4 bytes (offset 100-103) - Starting index in skeletal SSBO
        uint32_t _pad0;             // 4 bytes (offset 104-107) - Padding
        uint32_t _pad1;             // 4 bytes (offset 108-111) - Padding to 16-byte boundary for vec3
        glm::vec4 normalMatrixCol0; // 16 bytes (offset 112-127) - Normal matrix column 0 (xyz used)
        glm::vec4 normalMatrixCol1; // 16 bytes (offset 128-143) - Normal matrix column 1 (xyz used)
        glm::vec4 normalMatrixCol2; // 16 bytes (offset 144-159) - Normal matrix column 2 (xyz used)
        // Total: 160 bytes
    };

    /**
     * @brief Batch draw info containing all data needed for a batched draw call
     * Combines draw command with associated metadata
     */
    struct BatchDrawInfo
    {
        DrawElementsIndirectCommand command;
        DrawInfo info;

        BatchDrawInfo() = default;
        BatchDrawInfo(uint32_t count, uint32_t instanceCount, uint32_t firstIndex,
                     uint32_t baseVertex, uint32_t baseInstance,
                     const glm::mat4& modelMatrix, const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                     uint32_t materialIndex, uint32_t entityID)
            : command{count, instanceCount, firstIndex, baseVertex, baseInstance}
            , info{modelMatrix, aabbMin, materialIndex, aabbMax, entityID}
        {}
    };

    /**
     * @brief Multi-draw indirect structure for efficient batched rendering
     * Contains an array of draw commands that can be executed in a single draw call
     */
    struct MultiDrawIndirect
    {
        std::vector<DrawElementsIndirectCommand> commands;
        std::vector<DrawInfo> drawInfos;
        uint32_t drawCount = 0; // Number of valid draw commands

        void Clear()
        {
            commands.clear();
            drawInfos.clear();
            drawCount = 0;
        }

        void Reserve(size_t count)
        {
            commands.reserve(count);
            drawInfos.reserve(count);
        }

        void AddDraw(const DrawElementsIndirectCommand& cmd, const DrawInfo& info)
        {
            commands.push_back(cmd);
            drawInfos.push_back(info);
            ++drawCount;
        }
    };

    /**
     * @brief GPU buffer info for managing indirect draw buffers
     */
    struct IndirectDrawBuffer
    {
        uint32_t bufferID = 0;           // OpenGL buffer object ID
        uint32_t commandCount = 0;       // Number of commands in buffer
        size_t bufferSize = 0;           // Size of buffer in bytes
        bool isDirty = false;            // Flag to track if buffer needs updating (starts clean)

        void MarkDirty() { isDirty = true; }
        void MarkClean() { isDirty = false; }
        bool IsValid() const { return bufferID != 0; }
    };

    /**
     * @brief Persistent mapped buffer for DrawInfo data
     * Uses persistent mapping for efficient CPU writes without explicit buffer uploads
     */
    class DrawInfoBuffer
    {
    public:
        DrawInfoBuffer() = default;
        ~DrawInfoBuffer();

        // Disable copy
        DrawInfoBuffer(const DrawInfoBuffer&) = delete;
        DrawInfoBuffer& operator=(const DrawInfoBuffer&) = delete;

        /**
         * @brief Initialize the persistent mapped buffer
         * @param maxDrawCalls Maximum number of draw calls to support
         * @return true if successful
         */
        bool Initialize(size_t maxDrawCalls);

        /**
         * @brief Write draw info data to the mapped buffer
         * @param drawInfos Vector of draw info to write
         * @param offset Offset in number of DrawInfo elements (default 0)
         */
        void WriteDrawInfos(const std::vector<DrawInfo>& drawInfos, size_t offset = 0);

        /**
         * @brief Get the OpenGL buffer ID
         * @return Buffer ID
         */
        uint32_t GetBufferID() const { return m_BufferID; }

        /**
         * @brief Check if the buffer is initialized
         * @return true if initialized
         */
        bool IsValid() const { return m_BufferID != 0; }

        /**
         * @brief Get the current number of draw infos
         * @return Number of draw infos
         */
        size_t GetDrawCount() const { return m_DrawCount; }

        /**
         * @brief Get the maximum capacity of the buffer
         * @return Maximum number of draw infos
         */
        size_t GetMaxDrawCalls() const { return m_MaxDrawCalls; }

    private:
        uint32_t m_BufferID = 0;        // OpenGL buffer object
        void* m_MappedPtr = nullptr;    // Persistent mapped pointer
        size_t m_MaxDrawCalls = 0;      // Maximum capacity
        size_t m_DrawCount = 0;         // Current number of draws
        size_t m_BufferSize = 0;        // Total buffer size in bytes
    };

    /**
     * @brief Persistent mapped buffer for DrawElementsIndirectCommand data
     * Uses persistent mapping for efficient CPU writes without explicit buffer uploads
     * Eliminates glBufferData/glBufferSubData stalls by using direct memory mapping
     */
    class DrawCommandBuffer
    {
    public:
        DrawCommandBuffer() = default;
        ~DrawCommandBuffer();

        // Disable copy
        DrawCommandBuffer(const DrawCommandBuffer&) = delete;
        DrawCommandBuffer& operator=(const DrawCommandBuffer&) = delete;

        /**
         * @brief Initialize the persistent mapped buffer
         * @param maxCommands Maximum number of draw commands to support
         * @return true if successful
         */
        bool Initialize(size_t maxCommands);

        /**
         * @brief Write draw commands to the mapped buffer
         * @param commands Vector of draw commands to write
         * @param offset Offset in number of DrawElementsIndirectCommand elements (default 0)
         */
        void WriteCommands(const std::vector<DrawElementsIndirectCommand>& commands, size_t offset = 0);

        /**
         * @brief Get the OpenGL buffer ID
         * @return Buffer ID
         */
        uint32_t GetBufferID() const { return m_BufferID; }

        /**
         * @brief Check if the buffer is initialized
         * @return true if initialized
         */
        bool IsValid() const { return m_BufferID != 0; }

        /**
         * @brief Get the current number of draw commands
         * @return Number of draw commands
         */
        size_t GetCommandCount() const { return m_CommandCount; }

        /**
         * @brief Get the maximum capacity of the buffer
         * @return Maximum number of draw commands
         */
        size_t GetMaxCommands() const { return m_MaxCommands; }

    private:
        uint32_t m_BufferID = 0;        // OpenGL buffer object
        void* m_MappedPtr = nullptr;    // Persistent mapped pointer
        size_t m_MaxCommands = 0;       // Maximum capacity
        size_t m_CommandCount = 0;      // Current number of commands
        size_t m_BufferSize = 0;        // Total buffer size in bytes
    };

} // namespace Ermine::graphics
