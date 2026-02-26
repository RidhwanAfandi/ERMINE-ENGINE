/* Start Header ************************************************************************/
/*!
\file       Texture.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (99%)
\co-authors LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (1%)
\date       09/03/2025
\brief      This file contains the declaration of the Texture class.
            This file is used to load and bind textures to the renderer.

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
    class Texture
    {
        GLuint m_RendererID = 0;
        std::string m_filePath;
        unsigned char* m_LocalBuffer;
        int m_Width, m_Height, m_BPP;

        void Release(bool contextExpected) noexcept;
    public:
        /**
         * @brief Default Construct
         */
        Texture();
        
        /**
         * @brief Construct a new Texture object
         */
        explicit Texture(const std::string& filePath);

        /**
         * @brief Destroy the Texture object
         */
        ~Texture();

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture && other) noexcept;
        Texture& operator=(Texture&& other) noexcept;

        bool LoadFromDDS(const std::string& ddsFilePath);

        /**
         * @brief check if the texture is valid
         * @return true if the texture is valid, false otherwise
         */
        bool IsValid() const;

        /**
         * @brief Bind the texture
         * @param slot the slot to bind the texture to
         */
        void Bind(unsigned int slot = 0) const;

        /**
         * @brief Unbind the texture
         */
        void Unbind() const;

        /**
         * @brief  Getter for GLuint m_RendererID
         */
        GLuint GetRendererID() const;

        /**
         * @brief  Getter for file path
         */
        std::string GetFilePath();

        /**
         * @brief  Getter for texture width
         */
        int GetWidth() const { return m_Width; }

        /**
         * @brief  Getter for texture height
         */
        int GetHeight() const { return m_Height; }
    };
}
