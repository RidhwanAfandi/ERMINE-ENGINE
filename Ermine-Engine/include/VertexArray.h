/* Start Header ************************************************************************/
/*!
\file       VertexArray.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the VertexArray system.
            This file is used to create a VertexArray object.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include <glad/glad.h>

namespace Ermine::graphics
{
    /*!
     * @brief The VertexArray class is used to create a Vertex Array Object (VAO) in OpenGL.
     *        The VAO is used to store the vertex data and attribute pointers for rendering.
     */
    class VertexArray
    {
        GLuint m_RendererID;
        size_t vertexCount;
    public:
        /*!
         * @brief Constructor that generates a Vertex Array Object (VAO) and assigns it an ID.
         *        This ID is used to reference the VAO in subsequent OpenGL operations.
         */
        VertexArray();
        /*!
         * @brief Destructor that deletes the Vertex Array Object (VAO) from OpenGL.
         */
        ~VertexArray();

        /*!
         * @brief Links a Vertex Buffer Object (VBO) attribute, such as position or color, to the VAO.
         *        This function binds the VBO, specifies the layout of the vertex data, and enables the vertex attribute array.
         * @param layout The layout location of the attribute in the shader (e.g., 0 for position).
         * @param numComponents The number of components in the attribute (e.g., 3 for a vec3 position).
         * @param type The data type of the attribute (e.g., GL_FLOAT).
         * @param stride The stride between consecutive vertex attributes (e.g., the size of the entire vertex).
         * @param offset The offset within the VBO to the attribute data (e.g., where the position starts in a vertex).
         */
        void LinkAttribute(GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride, const void* offset);

        /*!
         * @brief Binds the Vertex Array Object (VAO) to the current OpenGL context.
         */
        void Bind() const;
        
        /*!
         * @brief Unbinds the Vertex Array Object (VAO) from the current OpenGL context.
         */
        void Unbind() const;

        GLuint GetRendererID() const { return m_RendererID; }

		void SetVertexCount(size_t count) { vertexCount = count; }

		size_t GetVertexCount() const { return vertexCount; }
    };
}
