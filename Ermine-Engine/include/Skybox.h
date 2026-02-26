/* Start Header ************************************************************************/
/*!
\file       Skybox.h
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       15/09/2025
\brief      This file contains the declaration of the Skybox class.
            This file is used for rendering skyboxes using cubemaps.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "Cubemap.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "MathVector.h"

namespace Ermine::graphics
{
    /**
     * @brief Skybox class for rendering environment backgrounds
     */
    class Skybox
    {
    private:
        std::shared_ptr<Cubemap> m_cubemap;   /// Cubemap texture used for the skybox
        std::shared_ptr<Shader> m_shader;     /// Shader used for rendering the skybox
        std::unique_ptr<VertexArray> m_vao;   /// Vertex Array Object for skybox geometry
        std::unique_ptr<VertexBuffer> m_vbo;  /// Vertex Buffer Object for skybox geometry


        /**
         * @brief Creates the geometry for the skybox cube.
         */
        void CreateSkyboxGeometry();

    public:
        /**
         * @brief Default constructor
         */
        Skybox();

        /**
         * @brief Constructor with cubemap and shader
         * @param cubemap The cubemap to use for the skybox
         * @param shader The shader to use for rendering
         */
        Skybox(std::shared_ptr<Cubemap> cubemap, std::shared_ptr<Shader> shader);

        /**
         * @brief Destructor
         */
        ~Skybox() = default;

        // Non-copyable but movable
        Skybox(const Skybox&) = delete;
        Skybox& operator=(const Skybox&) = delete;
        Skybox(Skybox&&) = default;
        Skybox& operator=(Skybox&&) = default;

        /**
         * @brief Set the cubemap for the skybox
         * @param cubemap The cubemap to use
         */
        void SetCubemap(std::shared_ptr<Cubemap> cubemap) { m_cubemap = std::move(cubemap); }

        /**
         * @brief Set the shader for the skybox
         * @param shader The shader to use
         */
        void SetShader(std::shared_ptr<Shader> shader) { m_shader = std::move(shader); }

        /**
         * @brief Get the cubemap
         * @return The current cubemap
         */
        std::shared_ptr<Cubemap> GetCubemap() const { return m_cubemap; }

        /**
         * @brief Render the skybox
         * @param view The view matrix (translation will be removed)
         * @param projection The projection matrix
         * @param exposure Exposure value for tone mapping
         * @param gamma Gamma correction value
         */
        void Render(const Mtx44& view, const Mtx44& projection, float exposure = 1.0f, float gamma = 2.2f);

        /**
         * @brief Check if the skybox is ready to render
         * @return true if both cubemap and shader are valid
         */
        bool IsValid() const;
    };
}