/* Start Header ************************************************************************/
/*!
\file       VertexArray.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the definition of the VertexArray system.
            This file is used to create a VertexArray using OpenGL.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "VertexArray.h"

using namespace Ermine::graphics;

/*!
 * @brief Constructor that generates a Vertex Array Object (VAO) and assigns it an ID.
 *        This ID is used to reference the VAO in subsequent OpenGL operations.
 */
VertexArray::VertexArray()
{
    glGenVertexArrays(1, &m_RendererID);
    glBindVertexArray(m_RendererID);
	EE_INFO("Vertex Array: Created Vertex Array Object with ID: {}", m_RendererID);
}

/*!
 * @brief Destructor that deletes the Vertex Array Object (VAO) from OpenGL.
 */
VertexArray::~VertexArray()
{
    glDeleteVertexArrays(1, &m_RendererID);
}

/*!
 * @brief Links a Vertex Buffer Object (VBO) attribute, such as position or color, to the VAO.
 *        This function binds the VBO, specifies the layout of the vertex data, and enables the vertex attribute array.
 * @param layout The layout location of the attribute in the shader (e.g., 0 for position).
 * @param numComponents The number of components in the attribute (e.g., 3 for a vec3 position).
 * @param type The data type of the attribute (e.g., GL_FLOAT).
 * @param stride The stride between consecutive vertex attributes (e.g., the size of the entire vertex).
 * @param offset The offset within the VBO to the attribute data (e.g., where the position starts in a vertex).
 */
void VertexArray::LinkAttribute(GLuint layout, GLuint numComponents, GLenum type, GLsizeiptr stride,
    const void* offset)
{
    glVertexAttribPointer(layout, static_cast<GLint>(numComponents), type, GL_FALSE, static_cast<GLsizei>(stride), offset);
    glEnableVertexAttribArray(layout);
}

/*!
 * @brief Binds the Vertex Array Object (VAO) to the current OpenGL context.
 */
void VertexArray::Bind() const
{
    glBindVertexArray(m_RendererID);
}

/*!
 * @brief Unbinds the Vertex Array Object (VAO) from the current OpenGL context.
 */
void VertexArray::Unbind() const
{
    glBindVertexArray(0);
}
