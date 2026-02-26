/* Start Header ************************************************************************/
/*!
\file       IndexBuffer.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the IndexBuffer system.
            This file is used to create an index buffer for the rendering system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <glad/glad.h>

namespace Ermine::graphics
{
    class IndexBuffer
    {
        GLuint m_RendererID;
        unsigned int m_Count;
    public:
        IndexBuffer(const unsigned int* data, unsigned int count);
        ~IndexBuffer();

        void Bind() const;
        void Unbind() const;

        unsigned int GetCount() const { return m_Count; }
    };
}
