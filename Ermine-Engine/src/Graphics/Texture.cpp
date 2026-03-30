/* Start Header ************************************************************************/
/*!
\file       Texture.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (99%)
\co-authors LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (1%)
\date       09/03/2025
\brief      This file contains the definition of the Texture system.
            This file is used to load textures using stb_image.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "GPUProfiler.h"
#include "Window.h"
#include <DirectXTex.h>

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM
#define GL_COMPRESSED_RGBA_BPTC_UNORM     0x8E8C
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM 0x8E8D
#endif

using namespace Ermine::graphics;

namespace
{
    void ConfigureTextureSampling(bool enableMipmaps)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, enableMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (glfwExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
            GLfloat maxAnisotropy = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::max(1.0f, maxAnisotropy));
        }
    }
}

bool Texture::LoadFromDDS(const std::string& ddsFilePath)
{
    using namespace DirectX;

    // Convert string to wide string for DirectXTex
    std::wstring widePath(ddsFilePath.begin(), ddsFilePath.end());

    // Load DDS file using DirectXTex
    TexMetadata metadata;
    ScratchImage image;

    HRESULT hr = LoadFromDDSFile(widePath.c_str(), DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(hr)) {
        EE_CORE_ERROR("Failed to load DDS file with DirectXTex: {0} (HRESULT: 0x{1:x})", ddsFilePath, hr);
        return false;
    }

    bool shouldFlip = false;  // Default: don't flip
    switch (metadata.format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        shouldFlip = true;  // Flip these specific uncompressed formats
        break;
    default:
        shouldFlip = false;  // Don't flip compressed BC formats and others
        break;
    }

    if (shouldFlip) {
        ScratchImage flipped;
        HRESULT flipHR = FlipRotate(
            image.GetImages(),
            image.GetImageCount(),
            metadata,
            TEX_FR_FLIP_VERTICAL,
            flipped
        );
        if (SUCCEEDED(flipHR)) {
            image = std::move(flipped);
        }
        else {
            EE_CORE_WARN("Failed to vertically flip DDS texture: {0} (HRESULT: 0x{1:x})", ddsFilePath, flipHR);
        }
    }

    // Store basic info
    m_Width = static_cast<int>(metadata.width);
    m_Height = static_cast<int>(metadata.height);
    m_filePath = ddsFilePath;

    // Clean up existing texture
    if (m_RendererID != 0) {
        Release(true);
    }

    // Generate and bind OpenGL texture
    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);

    // Set texture parameters
    ConfigureTextureSampling(true);

    // Convert DirectX format to OpenGL format
    GLenum internalFormat;
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    bool isCompressed = true;

    switch (metadata.format) {
    case DXGI_FORMAT_BC1_UNORM:
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC2_UNORM:
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC3_UNORM:
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC4_UNORM:
        internalFormat = GL_COMPRESSED_RED_RGTC1;  // Single-channel compression (grayscale)
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC5_UNORM:
        internalFormat = GL_COMPRESSED_RG_RGTC2;   // Two-channel compression (normal maps)
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC7_UNORM:
        internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC2_UNORM_SRGB:
        internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        internalFormat = GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
        isCompressed = true;
        break;
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
        isCompressed = true;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:      // added typeless support
        internalFormat = GL_RGBA8;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        isCompressed = false;
        break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        internalFormat = GL_RGBA8;
        format = GL_BGRA;
        type = GL_UNSIGNED_BYTE;
        isCompressed = false;
        break;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        internalFormat = GL_SRGB8_ALPHA8;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        isCompressed = false;
        break;
    default:
        EE_CORE_ERROR("Unsupported DXGI format {0} in DDS file: {1}",
            static_cast<int>(metadata.format), ddsFilePath);
        Release(true);
        return false;
    }

    size_t totalSize = 0;

    bool uploadedSingleMip = (metadata.mipLevels <= 1);

    // Upload each mip level
    for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
        const Image* img = image.GetImage(mipLevel, 0, 0);
        if (!img) {
            EE_CORE_ERROR("Failed to get mip level {0} from DDS file: {1}", mipLevel, ddsFilePath);
            Release(true);
            return false;
        }

        if (isCompressed) {
            glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mipLevel), internalFormat,
                static_cast<GLsizei>(img->width),
                static_cast<GLsizei>(img->height),
                0, static_cast<GLsizei>(img->slicePitch), img->pixels);
        }
        else {
            glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(mipLevel), internalFormat,
                static_cast<GLsizei>(img->width),
                static_cast<GLsizei>(img->height),
                0, format, type, img->pixels);
        }

        totalSize += img->slicePitch;
    }

    // If only one mip level and uncompressed, generate mipmaps
    if (uploadedSingleMip) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Track memory usage
    GPUProfiler::TrackMemoryAllocation(totalSize, "Texture");

    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        EE_CORE_ERROR("OpenGL error while loading DDS texture {0}: 0x{1:x}", ddsFilePath, error);
        Release(true);
        return false;
    }

    EE_CORE_INFO("Successfully loaded DDS texture: {0} ({1}x{2}, {3} mip levels, format: {4})",
        ddsFilePath, m_Width, m_Height, metadata.mipLevels, static_cast<int>(metadata.format));
    return true;
}

/**
 * @brief Releases the texture and associated GPU resources.
 * @param contextExpected Whether an OpenGL context is expected.
 */
void Texture::Release(bool contextExpected) noexcept
{
    if (!m_RendererID)
        return;

    GLFWwindow* current = glfwGetCurrentContext();
    if (!current)
    {
        EE_CORE_ERROR("No current OpenGL context while deleting texture ID={0} path={1} (Leaking GPU resource)", m_RendererID, m_filePath);
        m_RendererID = 0;
        return;
    }

    if (contextExpected && !glIsTexture(m_RendererID))
        EE_CORE_WARN("GL object {0} not recognized as texture (already deleted?) path={1}", m_RendererID, m_filePath);

    GPUProfiler::TrackMemoryDeallocation(static_cast<size_t>(m_Width) * m_Height * 4, "Texture");
    glDeleteTextures(1, &m_RendererID);
    m_RendererID = 0;
	m_Width = m_Height = m_BPP = 0;
    m_LocalBuffer = nullptr;
}

/**
 * @brief Default Constructor
 */
Texture::Texture() : m_RendererID(0), m_LocalBuffer(nullptr), m_Width(0), m_Height(0), m_BPP(0)
{
}

/**
 * @brief Constructor for the Texture system
 * @param filePath The file path of the texture
 */
Texture::Texture(const std::string& filePath) : m_filePath(filePath)
{
    // Check if it's a DDS file
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".dds") {
        // Use DDS loader
        if (!LoadFromDDS(filePath)) {
            EE_CORE_ERROR("Failed to load DDS texture: {0}", filePath);
        }
        return;
    }
    
    // Original stb_image path for other formats
    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
    
    ConfigureTextureSampling(true);
    
    stbi_set_flip_vertically_on_load(1);
    m_LocalBuffer = stbi_load(filePath.c_str(), &m_Width, &m_Height, &m_BPP, 4);
    if (!m_LocalBuffer)
    {
        EE_CORE_WARN("Failed to load texture: {0}", filePath);
        return;
    }
    GPUProfiler::TrackMemoryAllocation(m_Width * m_Height * 4, "Texture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_LocalBuffer);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(m_LocalBuffer);
}

/**
 * @brief Destructor for the Texture system
 */
Texture::~Texture()
{
    Release(true);
	/*GPUProfiler::TrackMemoryDeallocation(m_Width * m_Height * 4, "Texture");
	glDeleteTextures(1, &m_RendererID);*/
}

/**
 * @brief Move constructor.
 * @param other Texture to move from.
 */
Texture::Texture(Texture&& other) noexcept
{
    *this = std::move(other);
}

/**
 * @brief Move assignment operator.
 * @param other Texture to move from.
 * @return Reference to this texture.
 */
Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Release(true);
        m_RendererID = other.m_RendererID;
        m_filePath = std::move(other.m_filePath);
        m_LocalBuffer = other.m_LocalBuffer;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
		m_BPP = other.m_BPP;

        other.m_RendererID = 0;
        other.m_LocalBuffer = nullptr;
        other.m_Width = other.m_Height = other.m_BPP = 0;
    }

    return *this;
}

/**
 * @brief Check if the texture is valid
 * @return true if the texture is valid
 */
bool Texture::IsValid() const
{
    return m_RendererID != 0;
}

/**
 * @brief Bind the texture
 * @param slot The texture slot to bind
 */
void Texture::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
}

/**
 * @brief Unbind the texture
 */
void Texture::Unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * @brief Gets the OpenGL renderer ID for this texture.
 * @return Renderer ID (GLuint).
 */
GLuint Texture::GetRendererID() const
{
    return m_RendererID;
}

/**
 * @brief Gets the file path of the texture.
 * @return File path as a string.
 */
std::string Ermine::graphics::Texture::GetFilePath()
{
    return m_filePath;
}
