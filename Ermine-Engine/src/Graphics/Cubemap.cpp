/* Start Header ************************************************************************/
/*!
\file       Cubemap.cpp
\author     Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\date       15/09/2025
\brief      This file contains the implementation of the Cubemap class.
            This file is used to load and manage cubemap textures for environment mapping and skyboxes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Cubemap.h"
#include "GPUProfiler.h"
#include "Window.h"
#include "Logger.h"

#include <stb_image.h>

using namespace Ermine::graphics;
/**
* 
 * @brief Releases the cubemap texture and associated resources.
 * @param contextExpected Whether an OpenGL context is expected to be valid.
 * @note Safe to call multiple times; will warn if texture is already deleted.
 */
void Cubemap::Release(bool contextExpected) noexcept
{
    if (!m_RendererID)
        return;

    GLFWwindow* current = nullptr;
    try {
        current = glfwGetCurrentContext();
    } catch (...) {
        // Context might be destroyed during shutdown
        current = nullptr;
    }

    if (contextExpected && !glIsTexture(m_RendererID))
        EE_CORE_WARN("GL object {0} not recognized as texture (already deleted?)", m_RendererID);

    // Estimate memory usage (6 faces * width * height * 4 bytes per pixel)
    GPUProfiler::TrackMemoryDeallocation(static_cast<size_t>(m_Width) * m_Height * 6 * 4, "Cubemap");
    glDeleteTextures(1, &m_RendererID);
    m_RendererID = 0;
    m_Width = m_Height = 0;
    m_IsValid = false;
}

/**
 * @brief Default constructor. Initializes members to default values.
 */
Cubemap::Cubemap() : m_RendererID(0), m_Width(0), m_Height(0), m_IsValid(false)
{
}

/**
 * @brief Constructs a cubemap from 6 face textures.
 * @param faces Array of 6 file paths in order: +X, -X, +Y, -Y, +Z, -Z.
 */
Cubemap::Cubemap(const std::array<std::string, 6>& faces) : Cubemap()
{
    m_FacePaths = faces;
    m_IsValid = LoadFromFaces(faces);
}

/**
 * @brief Constructs a cubemap from an equirectangular texture.
 * @param equirectangularPath Path to the equirectangular texture.
 */
Cubemap::Cubemap(const std::string& equirectangularPath) : Cubemap()
{
    m_EquirectangularPath = equirectangularPath;
    m_IsValid = LoadFromEquirectangular(equirectangularPath);
}

/**
 * @brief Destructor. Releases resources.
 */
Cubemap::~Cubemap()
{
    Release(true);
}

/**
 * @brief Move constructor.
 * @param other Cubemap to move from.
 */
Cubemap::Cubemap(Cubemap&& other) noexcept
{
    *this = std::move(other);
}

/**
 * @brief Move assignment operator.
 * @param other Cubemap to move from.
 * @return Reference to this cubemap.
 */
Cubemap& Cubemap::operator=(Cubemap&& other) noexcept
{
    if (this != &other)
    {
        Release(true);
        m_RendererID = other.m_RendererID;
        m_FacePaths = std::move(other.m_FacePaths);
        m_EquirectangularPath = std::move(other.m_EquirectangularPath);
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_IsValid = other.m_IsValid;

        other.m_RendererID = 0;
        other.m_Width = other.m_Height = 0;
        other.m_IsValid = false;
    }

    return *this;
}

/**
 * @brief Loads cubemap from 6 individual face textures.
 * @param faces Array of 6 file paths in order: +X, -X, +Y, -Y, +Z, -Z.
 * @return true if successful, false otherwise.
 */
bool Cubemap::LoadFromFaces(const std::array<std::string, 6>& faces)
{
    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

    int width, height, nrComponents;
    bool success = true;

    // Load each face of the cubemap
    // Order: +X (right), -X (left), +Y (top), -Y (bottom), +Z (front), -Z (back)
    for (unsigned int i = 0; i < 6; i++)
    {
        // Set flip for each individual face to avoid affecting other texture loads
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        stbi_set_flip_vertically_on_load(false); // Reset immediately
        
        if (data)
        {
            // Store dimensions from first successful load
            if (m_Width == 0)
            {
                m_Width = width;
                m_Height = height;
            }

            GLenum format = GL_RGB;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 3)
                format = GL_RGB;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);

            EE_CORE_TRACE("Loaded cubemap face {0}: {1}", i, faces[i]);
        }
        else
        {
            EE_CORE_ERROR("Failed to load cubemap face {0}: {1}", i, faces[i]);
            success = false;
            stbi_image_free(data);
        }
    }

    if (success)
    {
        // Set cubemap parameters
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // Track memory usage
        GPUProfiler::TrackMemoryAllocation(static_cast<size_t>(m_Width) * m_Height * 6 * 4, "Texture");
        
        EE_CORE_INFO("Cubemap loaded successfully with {0} faces", faces.size());
    }
    else
    {
        glDeleteTextures(1, &m_RendererID);
        m_RendererID = 0;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return success;
}

/**
 * @brief Loads cubemap from an equirectangular (panoramic) texture.
 * @param equirectangularPath Path to the equirectangular texture.
 * @return true if successful, false otherwise.
 */
bool Cubemap::LoadFromEquirectangular(const std::string& equirectangularPath)
{
    // Set stb_image to flip images vertically for correct OpenGL orientation
    stbi_set_flip_vertically_on_load(true);

    // Load the equirectangular texture
    int width, height, nrComponents;
    float* data = stbi_loadf(equirectangularPath.c_str(), &width, &height, &nrComponents, 0);
    
    // Reset flip setting to avoid affecting other texture loads
    stbi_set_flip_vertically_on_load(false);
    
    if (!data)
    {
        EE_CORE_ERROR("Failed to load equirectangular texture: {0}", equirectangularPath);
        return false;
    }

    // Create temporary equirectangular texture
    GLuint equirectangularTexture;
    glGenTextures(1, &equirectangularTexture);
    glBindTexture(GL_TEXTURE_2D, equirectangularTexture);
    
    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB16F;
    if (nrComponents == 1)
    {
        format = GL_RED;
        internalFormat = GL_R16F;
    }
    else if (nrComponents == 3)
    {
        format = GL_RGB;
        internalFormat = GL_RGB16F;
    }
    else if (nrComponents == 4)
    {
        format = GL_RGBA;
        internalFormat = GL_RGBA16F;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    // Generate cubemap from equirectangular texture
    int cubemapSize = std::min(512, width / 4); // Choose reasonable cubemap size
    bool success = GenerateFromEquirectangular(equirectangularTexture, cubemapSize);

    // Clean up temporary texture
    glDeleteTextures(1, &equirectangularTexture);

    if (success)
    {
        EE_CORE_INFO("Cubemap generated from equirectangular texture: {0}", equirectangularPath);
    }

    return success;
}

/**
 * @brief Generates a cubemap from an equirectangular texture.
 * @param equirectangularTexture OpenGL texture ID of the equirectangular texture.
 * @param resolution Resolution of each face of the resulting cubemap.
 * @return true if successful, false otherwise.
 */
bool Cubemap::GenerateFromEquirectangular([[maybe_unused]] GLuint equirectangularTexture, int resolution)
{    
    // For now, I created a simple cubemap and note that proper conversion
    // would require shader-based projection from equirectangular to cube faces
    
    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

    m_Width = m_Height = resolution;

    // Create empty cubemap faces
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 
                     resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Track memory usage
    GPUProfiler::TrackMemoryAllocation(static_cast<size_t>(resolution) * resolution * 6 * 8, "Texture"); // 8 bytes for RGB16F

    EE_CORE_WARN("Equirectangular to cubemap conversion is not fully implemented. Created empty cubemap.");

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

/**
 * @brief Generates mipmaps for the cubemap texture.
 */
void Cubemap::GenerateMipmaps()
{
    if (!m_IsValid) return;
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    // Update filtering to use mipmaps
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

/**
 * @brief Binds the cubemap to a texture slot.
 * @param slot The texture slot to bind to.
 */
void Cubemap::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
}

/**
 * @brief Unbinds the cubemap texture.
 */
void Cubemap::Unbind() const
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}