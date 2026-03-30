/* Start Header ************************************************************************/
/*!
\file       Skybox.cpp
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       15/09/2025
\brief      This file contains the implementation of the Skybox class.
            This file is used for rendering skyboxes using cubemaps.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Skybox.h"
#include "Logger.h"
#include <glad/glad.h>

using namespace Ermine::graphics;

Skybox::Skybox()
{
    CreateSkyboxGeometry();
}

Skybox::Skybox(std::shared_ptr<Cubemap> cubemap, std::shared_ptr<Shader> shader)
    : m_cubemap(std::move(cubemap)), m_shader(std::move(shader))
{
    CreateSkyboxGeometry();
}

void Skybox::CreateSkyboxGeometry()
{
    // Skybox vertices (unit cube)
    float skyboxVertices[] = {
        // Positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // Create VAO and VBO
    m_vao = std::make_unique<VertexArray>();
    m_vbo = std::make_unique<VertexBuffer>(skyboxVertices, sizeof(skyboxVertices));

    m_vao->Bind();
    m_vbo->Bind();

    // Position attribute (location 0)
    m_vao->LinkAttribute(0, 3, GL_FLOAT, 3 * sizeof(float), (void*)0);

    m_vao->Unbind();
    m_vbo->Unbind();
}

void Skybox::Render(const Mtx44& view, const Mtx44& projection)
{
    if (!IsValid())
    {
        EE_CORE_WARN("Cannot render skybox: Invalid cubemap or shader");
        return;
    }

    // Save current depth function and change to LEQUAL for skybox
    glDepthFunc(GL_LEQUAL);

    // Bind shader and set uniforms
    m_shader->Bind();
    
    // Remove translation from view matrix for skybox
    Mtx44 skyboxView = view;
    skyboxView.m03 = 0.0f;
    skyboxView.m13 = 0.0f;
    skyboxView.m23 = 0.0f;
    
    m_shader->SetUniformMatrix4fv("view", skyboxView);
    m_shader->SetUniformMatrix4fv("projection", projection);

    // Bind cubemap
    m_cubemap->Bind(0);
    m_shader->SetUniform1i("skybox", 0);

    // Render skybox
    m_vao->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    m_vao->Unbind();

    // Cleanup
    m_cubemap->Unbind();
    m_shader->Unbind();
}

bool Skybox::IsValid() const
{
    return m_cubemap && m_cubemap->IsValid() && 
           m_shader && m_shader->IsValid();
}
