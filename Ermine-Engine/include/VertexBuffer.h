/* Start Header ************************************************************************/
/*!
\file       VertexBuffer.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the VertexBuffer system.
            This file is used to create a VertexBuffer using OpenGL.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <glad/glad.h>

namespace Ermine::graphics
{
    /**
     * @brief VertexBuffer class
     */
    class VertexBuffer
    {
        GLuint m_RendererID;
        unsigned int m_Size;

        // CPU-side copy for physics / readback
        std::vector<float> m_Data;
    public:
        /**
         * @brief Construct a new Vertex Buffer object
         * @param data The data to be stored in the buffer
         * @param size The size of the data
         */
        VertexBuffer(const void* data, unsigned int size);
        /**
         * @brief Destroy the Vertex Buffer object
         */
        ~VertexBuffer();

        /**
         * @brief Bind the buffer
         */
        void Bind() const;
        /**
         * @brief Unbind the buffer
         */
        void Unbind() const;

        /**
         * @brief Get a pointer to the CPU-side copy of the vertex data
         *
         * Useful for physics calculations or readback operations.
         * @return const void* Pointer to the buffer data
         */
        const void* GetDataPointer() const { return m_Data.data(); }

        /**
         * @brief Get the size of the buffer in bytes
         *
         * @return unsigned int Size of the buffer
         */
        unsigned int GetSize() const { return m_Size; }
    };
    
}
