/* Start Header ************************************************************************/
/*!
\file       VertexBuffer.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the implementation of the VertexBuffer system.
            This file is used to create a vertex buffer object.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "VertexBuffer.h"

#include "GPUProfiler.h"

using namespace Ermine::graphics;

VertexBuffer::VertexBuffer(const void* data, unsigned int size)
{
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
    m_Size = size;
    GPUProfiler::TrackMemoryAllocation(size, "Buffer");
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW); // usage is static draw because the data will not change, unless we are using dynamic draw

    // Store CPU-side copy
    m_Data.resize(size / sizeof(float));
    std::memcpy(m_Data.data(), data, size);
}

VertexBuffer::~VertexBuffer()
{
    GPUProfiler::TrackMemoryDeallocation(m_Size, "Buffer");
    glDeleteBuffers(1, &m_RendererID);
}

void VertexBuffer::Bind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
}

void VertexBuffer::Unbind() const
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}