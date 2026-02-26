/* Start Header ************************************************************************/
/*!
\file       Cubemap.h
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       15/09/2025
\brief      This file contains the declaration of the Cubemap class.
            This file is used to load and bind cubemap textures for environment mapping and skyboxes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include <glad/glad.h>
#include <array>

namespace Ermine::graphics
{
    /**
     * @brief Cubemap class for handling cube texture maps
     * Supports both individual face loading and single equirectangular texture conversion
     */
    class Cubemap
    {
        GLuint m_RendererID;
        std::array<std::string, 6> m_FacePaths;
        std::string m_EquirectangularPath;
        int m_Width, m_Height;
        bool m_IsValid;

        void Release(bool contextExpected) noexcept;
        
        /**
         * @brief Load cubemap from 6 individual face textures
         * @param faces Array of 6 file paths in order: +X, -X, +Y, -Y, +Z, -Z
         * @return true if successful, false otherwise
         */
        bool LoadFromFaces(const std::array<std::string, 6>& faces);
        
        /**
         * @brief Load cubemap from equirectangular (panoramic) texture
         * @param equirectangularPath Path to the equirectangular texture
         * @return true if successful, false otherwise
         */
        bool LoadFromEquirectangular(const std::string& equirectangularPath);

    public:
        /**
         * @brief Default constructor
         */
        Cubemap();

        /**
         * @brief Constructor for loading cubemap from 6 individual face textures
         * @param faces Array of 6 file paths in order: right(+X), left(-X), top(+Y), bottom(-Y), front(+Z), back(-Z)
         */
        explicit Cubemap(const std::array<std::string, 6>& faces);

        /**
         * @brief Constructor for loading cubemap from equirectangular texture
         * @param equirectangularPath Path to the equirectangular texture
         */
        explicit Cubemap(const std::string& equirectangularPath);

        /**
         * @brief Destructor
         */
        ~Cubemap();

        // Non-copyable but movable
        Cubemap(const Cubemap&) = delete;
        Cubemap& operator=(const Cubemap&) = delete;
        Cubemap(Cubemap&& other) noexcept;
        Cubemap& operator=(Cubemap&& other) noexcept;

        /**
         * @brief Check if the cubemap is valid
         * @return true if the cubemap is valid, false otherwise
         */
        bool IsValid() const { return m_IsValid; }

        /**
         * @brief Bind the cubemap to a texture slot
         * @param slot The texture slot to bind to (default: 0)
         */
        void Bind(unsigned int slot = 0) const;

        /**
         * @brief Unbind the cubemap
         */
        void Unbind() const;

        /**
         * @brief Get the OpenGL texture ID
         * @return The OpenGL texture ID
         */
        GLuint GetRendererID() const { return m_RendererID; }

        /**
         * @brief Get the dimensions of the cubemap
         * @return Pair of width and height
         */
        std::pair<int, int> GetDimensions() const { return {m_Width, m_Height}; }

        /**
         * @brief Generate a cubemap from an equirectangular texture at runtime
         * @param equirectangularTexture OpenGL texture ID of the equirectangular texture
         * @param resolution Resolution of each face of the resulting cubemap
         * @return true if successful, false otherwise
         */
        bool GenerateFromEquirectangular(GLuint equirectangularTexture, int resolution = 512);

        /**
         * @brief Generate mipmaps for the cubemap (useful for reflections)
         */
        void GenerateMipmaps();

        /**
         * @brief Get common cubemap face order for reference
         * @return Array of face names in the expected order
         */
        static std::array<std::string, 6> GetFaceOrder()
        {
            return {"right", "left", "top", "bottom", "front", "back"};
        }

        //const std::array<std::string, 6>& GetFaces() const { return m_FacePaths; }
        //const std::string& GetEquirectangularPath() const { return m_EquirectangularPath; }

        //bool IsFromFaces() const { return !m_FacePaths[0].empty(); }
        //bool IsFromEquirect() const { return !m_EquirectangularPath.empty(); }
    };
}