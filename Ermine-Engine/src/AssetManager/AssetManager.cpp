/* Start Header ************************************************************************/
/*!
\file       AssetManager.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (75%)
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (20%)
\co-authors Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (5%)
\date       03/10/2025
\brief      This file contains the definition of the AssetManager system.
            This file is used to manage all the assets in the game.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "AssetManager.h"

#include "Logger.h"
#include "Material.h"

#include <assimp/Importer.hpp>  // for the importer class
#include <assimp/scene.h>       // for the output data structure
#include <assimp/postprocess.h> // for post processing flags
#include <cctype>

using namespace Ermine;

/**
 * @brief Normalize a material path to an absolute, normalized form
 * @param materialPath The input material path
 */
std::string NormalizeMaterialPath(const std::string& materialPath)
    {
        std::filesystem::path p(materialPath);
        if (!p.is_absolute())
            p = std::filesystem::absolute(p);
        p = p.lexically_normal();
        return p.string();
    }

/**
 * @brief Initialize the asset manager with database path
 * @param databasePath Path to the resource database
 * @param projectGuid Project GUID (optional, will try to auto-detect)
 * @return true if initialization successful
 */
bool AssetManager::Initialize(const std::string& databasePath, const std::string& projectGuid)
{
    m_databasePath = databasePath;
    m_projectGuid = projectGuid;

    EE_CORE_INFO("Initializing AssetManager with database: {0}", databasePath);

    // Try to load the resource database
    if (LoadResourceDatabase())
    {
        EE_CORE_INFO("Resource database loaded successfully with {0} entries", m_resourceDatabase.size());
        for (const auto& [key, entry] : m_resourceDatabase)
        {
            // If your DB includes non-texture assets, you can pick type based on entry.typeGUID/typeName
            // For now, if this DB is textures-only, "Texture2D" is fine.
            EnsureMetaForSource(entry.sourcePath, "Texture2D");
        }
        return true;
    }
    else
    {
        EE_CORE_WARN("Could not load resource database - falling back to direct file loading");
        return true; // Don't fail initialization, just fall back to direct loading
    }
}

//void AssetManager::EnsureAllMetaFiles()
//{
//    for (const auto& [key, entry] : m_resourceDatabase)
//    {
//        // entry.sourcePath is the real source file path from the pipeline
//        EnsureMetaForSource(entry.sourcePath, entry.instanceGUID, entry.typeGUID);
//    }
//}

/**
 * @brief Load the resource database from the pipeline output
 * @return true if database loaded successfully
 */
bool AssetManager::LoadResourceDatabase()
{
    // If project GUID is not set, try to find it by scanning the database directory
    if (m_projectGuid.empty())
    {
        if (!std::filesystem::exists(m_databasePath))
        {
            EE_CORE_WARN("Database path does not exist: {0}", m_databasePath);
            return false;
        }

        // Look for project folders (should be GUIDs)
        for (const auto& entry : std::filesystem::directory_iterator(m_databasePath))
        {
            if (entry.is_directory())
            {
                std::string folderName = entry.path().filename().string();
                // Check if this looks like a GUID folder by looking for Browser.dbase
                std::string browserPath = entry.path().string() + "/Browser.dbase";
                if (std::filesystem::exists(browserPath))
                {
                    m_projectGuid = folderName;
                    EE_CORE_INFO("Auto-detected project GUID: {0}", m_projectGuid);
                    break;
                }
            }
        }

        if (m_projectGuid.empty())
        {
            EE_CORE_WARN("Could not auto-detect project GUID in database");
            return false;
        }
    }

    // Build path to resource database file
    std::string resourceDbPath = m_databasePath + "/" + m_projectGuid + "/Browser.dbase/resource_database.txt";

    if (!std::filesystem::exists(resourceDbPath))
    {
        EE_CORE_WARN("Resource database file not found: {0}", resourceDbPath);
        return false;
    }

    // Parse the resource database
    std::ifstream dbFile(resourceDbPath);
    if (!dbFile.is_open())
    {
        EE_CORE_ERROR("Failed to open resource database file: {0}", resourceDbPath);
        return false;
    }

    m_resourceDatabase.clear();

    std::string line;
    ResourceEntry currentEntry = {};
    bool readingEntry = false;

    while (std::getline(dbFile, line))
    {
        if (line.find("RESOURCE_START") == 0)
        {
            readingEntry = true;
            currentEntry = ResourceEntry{};
        }
        else if (line.find("RESOURCE_END") == 0)
        {
            if (readingEntry && !currentEntry.sourcePath.empty())
            {
                // Store using normalized source path as key
                std::string normalizedPath = ConvertToRelativePath(currentEntry.sourcePath);
                m_resourceDatabase[normalizedPath] = currentEntry;

                EE_CORE_TRACE("Loaded resource: {0} -> GUID 0x{1:x}",
                    normalizedPath, currentEntry.instanceGUID);
            }
            readingEntry = false;
        }
        else if (readingEntry)
        {
            if (line.find("InstanceGUID=") == 0)
            {
                currentEntry.instanceGUID = std::stoull(line.substr(13), nullptr, 16);
            }
            else if (line.find("TypeGUID=") == 0)
            {
                currentEntry.typeGUID = std::stoull(line.substr(9), nullptr, 16);
            }
            else if (line.find("SourcePath=") == 0)
            {
                currentEntry.sourcePath = line.substr(11);
            }
            else if (line.find("OutputPath=") == 0)
            {
                currentEntry.outputPath = line.substr(11);
            }
            else if (line.find("LastModified=") == 0)
            {
                auto timeVal = std::stoull(line.substr(13));
                currentEntry.lastModified = std::filesystem::file_time_type(
                    std::chrono::duration<uint64_t>(timeVal));
            }
        }
    }

    m_databaseLoaded = true;
    return true;
}

/**
 * @brief Reload the resource database
 * @return true if reload successful
 */
bool AssetManager::ReloadResourceDatabase()
{
    EE_CORE_INFO("Reloading resource database...");
    m_databaseLoaded = false;
    return LoadResourceDatabase();
}

/**
 * @brief Find a resource entry by source path
 * @param sourcePath The original source path (PNG file path)
 * @return Resource entry if found, nullptr otherwise
 */
ResourceEntry* AssetManager::FindResourceBySourcePath(const std::string& sourcePath)
{
    // Normalize the search path
    std::filesystem::path searchPath(sourcePath);

    if (!searchPath.is_absolute())
    {
        searchPath = std::filesystem::absolute(searchPath);
    }

    std::string normalizedSearch = searchPath.string();
    std::replace(normalizedSearch.begin(), normalizedSearch.end(), '\\', '/');

    // Extract just "Resources/Textures/filename.png" for comparison
    size_t resourcesPos = normalizedSearch.find("Resources/Textures/");
    if (resourcesPos == std::string::npos)
    {
        EE_CORE_TRACE("Path doesn't contain 'Resources/Textures/', cannot match");
        return nullptr;
    }

    std::string relativeSearch = normalizedSearch.substr(resourcesPos);
    EE_CORE_TRACE("Extracted relative path: {0}", relativeSearch);

    // Search through database with relative path comparison
    for (auto& [key, entry] : m_resourceDatabase)
    {
        std::string normalizedEntry = entry.sourcePath;
        std::replace(normalizedEntry.begin(), normalizedEntry.end(), '\\', '/');

        size_t entryResourcesPos = normalizedEntry.find("Resources/Textures/");
        if (entryResourcesPos != std::string::npos)
        {
            std::string relativeEntry = normalizedEntry.substr(entryResourcesPos);

            if (relativeEntry == relativeSearch)
            {
                EE_CORE_TRACE("MATCH FOUND! {0} == {1}", relativeEntry, relativeSearch);
                return &entry;
            }
        }
    }

    EE_CORE_TRACE("No match found in {0} database entries", m_resourceDatabase.size());
    return nullptr;
}

/**
 * @brief Convert absolute path to relative path for database lookup
 * @param absolutePath The absolute file path
 * @return Relative path for database lookup
 */
std::string AssetManager::ConvertToRelativePath(const std::string& absolutePath)
{
    // If it's already absolute, return as-is
    std::filesystem::path p(absolutePath);
    if (p.is_absolute())
    {
        return absolutePath;
    }

    // If it's relative, convert to absolute from working directory
    std::filesystem::path absPath = std::filesystem::absolute(p);
    return absPath.string();
}

std::string AssetManager::GetFullDDSPath(const ResourceEntry& entry) const
{
    // Combine the database path, project GUID, and stored relative DDS path
    std::filesystem::path fullPath = m_databasePath;
    fullPath /= m_projectGuid;
    fullPath /= entry.outputPath;

    // Normalize to string with forward slashes (optional)
    std::string normalized = fullPath.string();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

/**
 * @brief Resolve the texture path for material writing, preferring source files over DDS
 * @param texture The texture to resolve
 * @return The resolved texture path
 */
std::string AssetManager::ResolveTexturePathForMaterialWrite(const std::shared_ptr<graphics::Texture>& texture) const
{
    if (!texture)
        return {};

    auto normalizePath = [](std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        for (char& c : path) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return path;
    };

    auto normalizedExtension = [](const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    };

    std::string texPath = texture->GetFilePath();
    if (texPath.empty())
        return texPath;

    if (normalizedExtension(texPath) != ".dds")
        return texPath;

    // Prefer the key used by AssetManager cache when it already points to a source file.
    for (const auto& [cachedPath, cachedTexture] : m_textures)
    {
        if (cachedTexture.get() != texture.get())
            continue;

        if (!cachedPath.empty() && normalizedExtension(cachedPath) != ".dds")
            return cachedPath;
    }

    if (!m_databaseLoaded)
        return texPath;

    const std::string normalizedTexPath = normalizePath(texPath);
    const std::string texFilename = normalizePath(std::filesystem::path(texPath).filename().string());

    for (const auto& [_, entry] : m_resourceDatabase)
    {
        const std::string outputPath = normalizePath(entry.outputPath);
        const std::string fullOutputPath = normalizePath(m_databasePath + "/" + m_projectGuid + "/" + entry.outputPath);
        const std::string outputFilename = normalizePath(std::filesystem::path(entry.outputPath).filename().string());

        if (normalizedTexPath == outputPath ||
            normalizedTexPath == fullOutputPath ||
            (!texFilename.empty() && texFilename == outputFilename))
        {
            if (!entry.sourcePath.empty())
                return entry.sourcePath;
            return texPath;
        }
    }

    return texPath;
}

/**
 * @brief Load a texture from the file path and store it in the asset manager, if it is loaded before, return the texture
 * @param filePath The file path of the texture
 * @return The texture that is loaded
 */
std::shared_ptr<graphics::Texture> AssetManager::LoadTexture(const std::string& filePath)
{
    EE_CORE_TRACE("Loading texture: {0}", filePath);
    auto it = m_textures.find(filePath);
    if (it != m_textures.end()) // If the texture is already loaded
    {
        EE_CORE_INFO("Texture {0} already loaded, returning cached version", filePath);
        return it->second;
    }


    std::shared_ptr<graphics::Texture> texture;

    // Try to load from resource database first
    if (m_databaseLoaded)
    {
        ResourceEntry* resourceEntry = FindResourceBySourcePath(filePath);
        if (resourceEntry != nullptr)
        {
            std::string ddsFullPath = m_databasePath + "/" + m_projectGuid + "/" + resourceEntry->outputPath;
            EE_CORE_TRACE("Found resource in database: {0} -> DDS path: {1}",
                filePath, ddsFullPath);

            // Create texture from DDS file
            texture = std::make_shared<graphics::Texture>();
            if (texture->LoadFromDDS(ddsFullPath))
            {
                EE_CORE_INFO("Texture loaded from pipeline DDS: {0}", filePath);
                m_textures[filePath] = texture;
                return texture;
            }
            else
            {
                EE_CORE_WARN("Failed to load DDS file: {0}, falling back to direct load", ddsFullPath);
            }
        }
        else
        {
            EE_CORE_TRACE("Resource not found in database: {0}, trying direct load", filePath);
        }
    }

    // Fallback: Load directly from source file (PNG)
    texture = std::make_shared<graphics::Texture>(filePath);
    if (!texture->IsValid())
    {
        EE_CORE_ERROR("Failed to load texture: {0}", filePath);
        return nullptr;
    }

    m_textures[filePath] = texture;
    EE_CORE_INFO("Texture loaded directly: {0}", filePath);
    return texture;
}

/**
 * @brief Load a texture directly by GUID
 * @param instanceGUID The instance GUID of the texture resource
 * @return The loaded texture
 */
std::shared_ptr<graphics::Texture> AssetManager::LoadTextureByGUID(uint64_t instanceGUID)
{
    EE_CORE_TRACE("Loading texture by GUID: 0x{0:x}", instanceGUID);

    // Create cache key from GUID
    std::string cacheKey = "GUID_" + std::to_string(instanceGUID);

    auto it = m_textures.find(cacheKey);
    if (it != m_textures.end())
    {
        return it->second;
    }

    if (!m_databaseLoaded)
    {
        EE_CORE_ERROR("Cannot load by GUID: resource database not loaded");
        return nullptr;
    }

    // Find the resource by GUID
    for (const auto& [sourcePath, entry] : m_resourceDatabase)
    {
        if (entry.instanceGUID == instanceGUID)
        {
            auto texture = std::make_shared<graphics::Texture>();
            if (texture->LoadFromDDS(entry.outputPath))
            {
                m_textures[cacheKey] = texture;
                EE_CORE_INFO("Texture loaded by GUID 0x{0:x} from: {1}", instanceGUID, entry.outputPath);
                return texture;
            }
            break;
        }
    }

    EE_CORE_ERROR("Failed to load texture by GUID: 0x{0:x}", instanceGUID);
    return nullptr;
}

/**
 * @brief Get the texture from the asset manager
 * @param filePath The file path of the texture
 * @return The texture that is loaded
 */
std::shared_ptr<graphics::Texture> AssetManager::GetTexture(const std::string& filePath)
{
    auto it = m_textures.find(filePath);
    return it != m_textures.end() ? it->second : nullptr;
}

/**
 * @brief Get a read-only view of all currently loaded textures.
 * @return A const reference to the unordered_map of loaded textures.
 * The key is the texture file path, and the value is the shared Texture.
 */
const std::unordered_map<std::string, std::shared_ptr<graphics::Texture>>& AssetManager::GetLoadedTextures() const
{
    return m_textures;
}

/**
 * @brief Load a shader from the vertex and fragment file path and store it in the asset manager, if it is loaded before, return the shader
 * @param vertexPath The file path of the vertex shader
 * @param fragmentPath The file path of the fragment shader
 * @return The shader that is loaded
 */
std::shared_ptr<graphics::Shader> AssetManager::LoadShader(const std::string& computePath)
{
    EE_CORE_TRACE("Loading compute shader: {0}", computePath);
    auto it = m_shaders.find(computePath);
    if (it != m_shaders.end())
        return it->second;
    std::shared_ptr<graphics::Shader> shader = std::make_shared<graphics::Shader>(computePath);
    if (!shader->IsValid())
    {
        EE_CORE_ERROR("Failed to load compute shader: {0}", computePath);
        return nullptr;
    }
    m_shaders[computePath] = shader;
    EE_CORE_INFO("Compute shader loaded: {0}", computePath);
	return shader;
}

/**
 * @brief Load a shader from the vertex and fragment file path and store it in the asset manager, if it is loaded before, return the shader
 * @param vertexPath The file path of the vertex shader
 * @param fragmentPath The file path of the fragment shader
 * @return The shader that is loaded
 */
std::shared_ptr<graphics::Shader> AssetManager::LoadShader(const std::string& vertexPath,
    const std::string& fragmentPath)
{
    EE_CORE_TRACE("Loading shader: {0} | {1}", vertexPath, fragmentPath);
    std::string key = vertexPath + "|" + fragmentPath;
    auto it = m_shaders.find(key);
    if (it != m_shaders.end())
    {
        EE_CORE_INFO("Shader {0} already loaded, returning cached version", key);
        return it->second;
    }

    std::shared_ptr<graphics::Shader> shader = std::make_shared<graphics::Shader>(vertexPath, fragmentPath);
    if (!shader->IsValid())
    {
        EE_CORE_ERROR("Failed to load shader: {0} | {1}", vertexPath, fragmentPath);
        return nullptr;
    }
    
    m_shaders[key] = shader;
    EE_CORE_INFO("Shader loaded: {0} | {1}", vertexPath, fragmentPath);
    return shader;
}

/**
 * @brief Load a shader from a vertex, geometry and fragment file
 * @param vertexPath The path to the vertex shader file
 * @param geometryPath The path to the geometry shader file
 * @param fragmentPath The path to the fragment shader file
 * @return The loaded shader
 */
std::shared_ptr<graphics::Shader> AssetManager::LoadShader(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath)
{
    EE_CORE_TRACE("Loading shader: {0} | {1} | {2}", vertexPath, geometryPath, fragmentPath);
    std::string key = vertexPath + "|" + geometryPath + "|" + fragmentPath;
    auto it = m_shaders.find(key);
    if (it != m_shaders.end()) {
        EE_CORE_INFO("Shader {0} already loaded, returning cached version", key);
        return it->second;
    }

    std::shared_ptr<graphics::Shader> shader = std::make_shared<graphics::Shader>(vertexPath, geometryPath, fragmentPath);
    if (!shader->IsValid())
    {
        EE_CORE_ERROR("Failed to load shader: {0} | {1} | {2}", vertexPath, geometryPath, fragmentPath);
        return nullptr;
    }

    m_shaders[key] = shader;
    EE_CORE_INFO("Shader loaded: {0} | {1} | {2}", vertexPath, geometryPath, fragmentPath);
    return shader;
}


/**
 * @brief Get the shader from the asset manager
 * @param shaderName The name of the shader
 * @return The shader that is loaded
 */
std::shared_ptr<graphics::Shader> AssetManager::GetShader(const std::string& shaderName)
{
    auto it = m_shaders.find(shaderName);
    return it != m_shaders.end() ? it->second : nullptr;
}

bool AssetManager::ReloadCachedShaders()
{
    size_t reloadFailures = 0;
    for (auto& [key, shader] : m_shaders)
    {
        if (!shader)
        {
            ++reloadFailures;
            continue;
        }

        if (!shader->Reload())
        {
            EE_CORE_WARN("Shader reload failed: {0}", key);
            ++reloadFailures;
        }
    }

    if (reloadFailures == 0)
    {
        EE_CORE_INFO("Reloaded cached shaders");
    }
    else
    {
        EE_CORE_WARN("Shader reload finished with {0} failures", reloadFailures);
    }

    return reloadFailures == 0;
}

/**
* @brief Load a 3D model from file using Assimp.
* @param filePath The path to the model file (e.g. .fbx, .obj, .gltf).
* @return The loaded model.
*/
std::shared_ptr<graphics::Model> AssetManager::LoadModel(const std::string& filePath)
{
    EE_CORE_TRACE("Loading model: {0}", filePath);

    // Check cache first
    auto it = m_models.find(filePath);
    if (it != m_models.end()) // Already loaded
    {
        EE_CORE_INFO("Model {0} already loaded, returning cached version", filePath);
        return it->second;
    }

    try
    {
        std::shared_ptr<graphics::Model> model;

        // Determine file type by extension
        std::string ext = std::filesystem::path(filePath).extension().string();

        if (ext == ".skin" || ext == ".mesh") {
            // Load binary .skin file from resource pipeline
            EE_CORE_INFO("Loading .skin file: {0}", filePath);
            model = std::make_shared<graphics::Model>(filePath, true);
        }
        else {
            // Load via Assimp (.fbx, .obj, .gltf, etc.)
            EE_CORE_INFO("Loading model via Assimp: {0}", filePath);
            model = std::make_shared<graphics::Model>(filePath);
        }

        if (model) {
            m_models[filePath] = model;
            EE_CORE_INFO("Model loaded successfully: {0}", filePath);
            return model;
        }
        else {
            EE_CORE_ERROR("Failed to create model: {0}", filePath);
            return nullptr;
        }
    }
    catch (const std::exception& e)
    {
        EE_CORE_ERROR("Failed to load model: {0}, reason: {1}", filePath, e.what());
        return nullptr;
    }
}

/**
* @brief Get a model from the cache.
* @param filePath The path to the model file.
* @return The model if it exists, nullptr otherwise.
*/
std::shared_ptr<graphics::Model> AssetManager::GetModel(const std::string& filePath)
{
    auto it = m_models.find(filePath);
    return it != m_models.end() ? it->second : nullptr;
}

/**
 * @brief Get a read-only view of all currently loaded models.
 * @return A const reference to the unordered_map of loaded models.
 * The key is the model file path, and the value is the shared Model.
 */
const std::unordered_map<std::string, std::shared_ptr<graphics::Model>>& Ermine::AssetManager::GetLoadedModels() const
{
    return m_models;
}

/**
 * @brief Unload a specific model from the cache.
 * @param filePath The full path to the model file to unload.
 */
void Ermine::AssetManager::UnloadModel(const std::string& filePath)
{
    auto it = m_models.find(filePath);
    if (it != m_models.end()) {
        EE_CORE_INFO("Unloading model: {0}", filePath);
        m_models.erase(it);
    }
}

/** 
 * @brief Clear all cached models.
 */
void Ermine::AssetManager::ClearModelCache()
{
    EE_CORE_INFO("Clearing all cached models ({} models)", m_models.size());
    m_models.clear();
}

/**
 * @brief Load the contents of a file into a buffer.
 * @param filepath The path to the file to load.
 * @return The contents of the file as a buffer.
 */
const char* AssetManager::load_file_contents(const char* filepath)
{
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        EE_CORE_ERROR("Failed to open file: {0}", filepath);
        return nullptr;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0)
    {
        EE_CORE_ERROR("File is empty or cannot determine size: {0}", filepath);
        return nullptr;
    }

    auto buffer = new char[size + 1];
    file.read(buffer, size);
    buffer[size] = '\0';

    file.close();
    return buffer;
}

void AssetManager::Clear()
{
    EE_CORE_INFO("Clearing assets: {0} textures, {1} shaders, {2} cubemaps, {3} materials, {4} models", 
                 m_textures.size(), m_shaders.size(), m_cubemaps.size(), m_materials.size(), m_models.size());
    m_textures.clear();
    m_shaders.clear();
    m_cubemaps.clear();
    m_materials.clear();
    m_materialsByGuid.clear();
    m_materialPathsByGuid.clear();
    m_materialGuidsByPath.clear();
    m_materialCustomFragmentByGuid.clear();
    m_models.clear();
}

/**
 * @brief Load a cubemap from individual face textures and store it in the asset manager
 * @param faces Array of 6 face texture paths in order: +X, -X, +Y, -Y, +Z, -Z  
 * @param name Optional name for the cubemap (for caching)
 * @return The loaded cubemap
 */
std::shared_ptr<graphics::Cubemap> AssetManager::LoadCubemap(const std::array<std::string, 6>& faces, const std::string& name)
{
    // Create a cache key
    std::string key = name.empty() ? 
        (faces[0] + "|" + faces[1] + "|" + faces[2] + "|" + faces[3] + "|" + faces[4] + "|" + faces[5]) : 
        name;
    EE_CORE_TRACE("Loading cubemap: {0}", key);

    // Check if already loaded
    auto it = m_cubemaps.find(key);
    if (it != m_cubemaps.end())
    {
        EE_CORE_INFO("Cubemap {0} already loaded, returning cached version", key);
        return it->second;
    }
    
    // Create new cubemap
    std::shared_ptr<graphics::Cubemap> cubemap = std::make_shared<graphics::Cubemap>(faces);
    if (!cubemap->IsValid())
    {
        EE_CORE_ERROR("Failed to load cubemap: {0}", key);
        return nullptr;
    }
    
    m_cubemaps[key] = cubemap;
    EE_CORE_INFO("Cubemap loaded: {0}", key);
    return cubemap;
}

/**
 * @brief Load a cubemap from an equirectangular texture
 * @param equirectangularPath Path to the equirectangular texture
 * @param name Optional name for the cubemap (for caching)
 * @return The loaded cubemap
 */
std::shared_ptr<graphics::Cubemap> AssetManager::LoadCubemapFromEquirectangular(const std::string& equirectangularPath, const std::string& name)
{
    std::string key = name.empty() ? equirectangularPath : name;
    
    EE_CORE_TRACE("Loading cubemap from equirectangular: {0}", key);
    
    // Check if already loaded
    auto it = m_cubemaps.find(key);
    if (it != m_cubemaps.end())
    {
        EE_CORE_INFO("Cubemap {0} already loaded, returning cached version", key);
        return it->second;
    }
    
    // Create new cubemap from equirectangular
    std::shared_ptr<graphics::Cubemap> cubemap = std::make_shared<graphics::Cubemap>(equirectangularPath);
    if (!cubemap->IsValid())
    {
        EE_CORE_ERROR("Failed to load cubemap from equirectangular: {0}", key);
        return nullptr;
    }
    
    m_cubemaps[key] = cubemap;
    EE_CORE_INFO("Cubemap loaded from equirectangular: {0}", key);
    return cubemap;
}

/**
 * @brief Get a cubemap from the cache
 * @param name The name of the cubemap
 * @return The cubemap if it exists, nullptr otherwise
 */
std::shared_ptr<graphics::Cubemap> AssetManager::GetCubemap(const std::string& name)
{
    auto it = m_cubemaps.find(name);
    return it != m_cubemaps.end() ? it->second : nullptr;
}

std::string AssetManager::SanitizeAssetName(std::string name)
{
    if (name.empty())
        return "Material";

    for (char& ch : name)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.'))
            ch = '_';
    }
    return name;
}

/**
 * @brief Create and cache a material with the given name
 * @param name The name/key for the material
 * @param shader The shader to use for the material
 * @param materialTemplate Optional material template to apply
 * @return The created material
 */
std::shared_ptr<graphics::Material> AssetManager::CreateMaterial(const std::string& name, 
                                                               std::shared_ptr<graphics::Shader> shader,
                                                               const std::string& materialTemplate)
{
    EE_CORE_TRACE("Creating material: {0}", name);
    
    // Check if already exists
    auto it = m_materials.find(name);
    if (it != m_materials.end())
    {
        EE_CORE_TRACE("Material {0} already exists, returning cached version", name);
        return it->second;
    }
    
    if (!shader || !shader->IsValid())
    {
        EE_CORE_ERROR("Cannot create material {0} with invalid shader", name);
        return nullptr;
    }
    
    // Create new material
    auto material = std::make_shared<graphics::Material>(shader);
    
    // Apply template if specified
    if (!materialTemplate.empty())
    {
        if (materialTemplate == "PBR_WHITE")
            material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        else if (materialTemplate == "PBR_RED")
            material->LoadTemplate(graphics::MaterialTemplates::PBR_RED());
        else if (materialTemplate == "PBR_METAL")
            material->LoadTemplate(graphics::MaterialTemplates::PBR_METAL());
        else if (materialTemplate == "EMISSIVE_WHITE")
            material->LoadTemplate(graphics::MaterialTemplates::EMISSIVE(Vec3(1.0f, 1.0f, 1.0f), 10.0f));
        else if (materialTemplate == "PBR_GLASS")
            material->LoadTemplate(graphics::MaterialTemplates::PBR_GLASS(0.9f));
        else if (materialTemplate == "PBR_WATER")
            material->LoadTemplate(graphics::MaterialTemplates::PBR_WATER(0.7f));
        else
            EE_CORE_WARN("Unknown material template: {0}", materialTemplate);
    }
    
    m_materials[name] = material;
    EE_CORE_INFO("Material created and cached: {0}", name);
    return material;
}

/**
 * @brief Get a material from the cache
 * @param name The name of the material
 * @return The material if it exists, nullptr otherwise
 */
std::shared_ptr<graphics::Material> AssetManager::GetMaterial(const std::string& name)
{
    auto it = m_materials.find(name);
    return it != m_materials.end() ? it->second : nullptr;
}

/**
 * @brief Create a shared material for common use cases
 * @param materialType Type of material (e.g., "wood", "metal", "plastic")
 * @param shader The shader to use
 * @param baseTexture Optional base texture
 * @return The created shared material
 */
std::shared_ptr<graphics::Material> AssetManager::CreateSharedMaterial(const std::string& materialType,
                                                                     std::shared_ptr<graphics::Shader> shader,
                                                                     std::shared_ptr<graphics::Texture> baseTexture)
{
    std::string materialName = materialType + "_shared";
    
    // Check if this shared material already exists
    auto existing = GetMaterial(materialName);
    if (existing)
        return existing;
    
    EE_CORE_TRACE("Creating shared material: {0}", materialName);
    
    auto material = std::make_shared<graphics::Material>(shader);
    
    // Set up material based on type
    if (materialType == "wood")
    {
        material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        material->SetFloat("material.roughness", 0.8f);
        material->SetFloat("material.metallic", 0.0f);
    }
    else if (materialType == "metal")
    {
        material->LoadTemplate(graphics::MaterialTemplates::PBR_METAL());
        material->SetFloat("material.roughness", 0.2f);
        material->SetFloat("material.metallic", 1.0f);
    }
    else if (materialType == "plastic")
    {
        material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        material->SetFloat("material.roughness", 0.7f);
        material->SetFloat("material.metallic", 0.0f);
    }
    else if (materialType == "ceramic")
    {
        material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        material->SetFloat("material.roughness", 0.1f);
        material->SetFloat("material.metallic", 0.0f);
    }
    else if (materialType == "rubber")
    {
        material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        material->SetFloat("material.roughness", 0.9f);
        material->SetFloat("material.metallic", 0.0f);
    }
    else
    {
        // Default material
        material->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
        EE_CORE_WARN("Unknown material type {0}, using default PBR_WHITE", materialType);
    }
    
    // Apply base texture if provided
    if (baseTexture && baseTexture->IsValid())
    {
        material->SetTexture("materialAlbedoMap", baseTexture);
        material->SetBool("materialHasAlbedoMap", true);
    }
    
    m_materials[materialName] = material;
    EE_CORE_INFO("Shared material created and cached: {0}", materialName);
    return material;
}

std::shared_ptr<graphics::Material> AssetManager::LoadMaterialAsset(const std::string& materialPath, bool forceReload)
{
    const std::string normalizedPath = NormalizeMaterialPath(materialPath);
    if (!std::filesystem::exists(normalizedPath)) {
        EE_CORE_WARN("Material asset not found: {}", normalizedPath);
        return nullptr;
    }

    Guid guid = GetMaterialGuidForPath(normalizedPath);
    if (!forceReload) {
        auto it = m_materialsByGuid.find(guid);
        if (it != m_materialsByGuid.end())
            return it->second;
    }

    std::string customFragmentShader;
    graphics::Material loaded = LoadMaterialFromFile(normalizedPath, &customFragmentShader);
    auto material = std::make_shared<graphics::Material>(loaded);

    // Ensure a valid shader is assigned
    if (!material->GetShader() || !material->GetShader()->IsValid()) {
        auto defaultShader = LoadShader(
            "../Resources/Shaders/vertex.glsl",
            "../Resources/Shaders/fragment_enhanced.glsl"
        );
        if (defaultShader && defaultShader->IsValid())
            material->SetShader(defaultShader);
    }

    m_materialsByGuid[guid] = material;
    m_materialPathsByGuid[guid] = normalizedPath;
    m_materialGuidsByPath[normalizedPath] = guid;
    if (!customFragmentShader.empty())
        m_materialCustomFragmentByGuid[guid] = customFragmentShader;
    return material;
}

std::shared_ptr<graphics::Material> AssetManager::GetMaterialByGuid(const Guid& guid)
{
    if (!guid.IsValid())
        return nullptr;

    auto it = m_materialsByGuid.find(guid);
    if (it != m_materialsByGuid.end())
        return it->second;

    auto pathIt = m_materialPathsByGuid.find(guid);
    if (pathIt != m_materialPathsByGuid.end())
        return LoadMaterialAsset(pathIt->second, false);

    ScanMaterialAssets("../Resources/Materials/", false);
    pathIt = m_materialPathsByGuid.find(guid);
    if (pathIt != m_materialPathsByGuid.end())
        return LoadMaterialAsset(pathIt->second, false);

    return nullptr;
}

Guid AssetManager::FindMaterialGuid(const graphics::Material* material) const
{
    if (!material)
        return {};

    for (const auto& [guid, mat] : m_materialsByGuid) {
        if (mat.get() == material)
            return guid;
    }
    return {};
}

Guid AssetManager::GetMaterialGuidForPath(const std::string& materialPath)
{
    const std::string normalizedPath = NormalizeMaterialPath(materialPath);
    auto it = m_materialGuidsByPath.find(normalizedPath);
    if (it != m_materialGuidsByPath.end())
        return it->second;

    if (!std::filesystem::exists(normalizedPath)) {
        EE_CORE_WARN("GetMaterialGuidForPath skipped non-existent material path: {}", normalizedPath);
        return {};
    }

    Guid guid = EnsureMetaForSource(normalizedPath, "Material");
    m_materialGuidsByPath[normalizedPath] = guid;
    m_materialPathsByGuid[guid] = normalizedPath;
    return guid;
}

std::shared_ptr<graphics::Material> AssetManager::CreateMaterialAsset(
    const std::string& materialName,
    const std::function<void(graphics::Material&)>& initializer,
    Guid* outGuid,
    const std::string& materialsDir)
{
    std::filesystem::path dir = materialsDir;
    if (!dir.is_absolute())
        dir = std::filesystem::absolute(dir);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        EE_CORE_ERROR("Failed to create materials directory: {}", dir.string());
        return nullptr;
    }

    std::string safeName = SanitizeAssetName(materialName);
    std::filesystem::path path = dir / (safeName + ".mat");

    if (std::filesystem::exists(path)) {
        Guid guid = GetMaterialGuidForPath(path.string());
        if (outGuid) *outGuid = guid;
        return LoadMaterialAsset(path.string(), false);
    }

    graphics::Material material;
    material.LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
    if (initializer)
        initializer(material);

    SaveMaterialToFile(material, path.string(), true);

    Guid guid = GetMaterialGuidForPath(path.string());
    if (outGuid) *outGuid = guid;
    return LoadMaterialAsset(path.string(), true);
}

Guid AssetManager::SaveMaterialAsset(
    const std::string& materialName,
    const graphics::Material& material,
    bool overwrite,
    std::string_view customFragmentShader,
    const std::string& materialsDir)
{
    std::filesystem::path dir = materialsDir;
    if (!dir.is_absolute())
        dir = std::filesystem::absolute(dir);

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        EE_CORE_ERROR("Failed to create materials directory: {}", dir.string());
        return {};
    }

    std::string safeName = SanitizeAssetName(materialName);
    std::filesystem::path path = dir / (safeName + ".mat");

    if (!overwrite && std::filesystem::exists(path)) {
        return GetMaterialGuidForPath(path.string());
    }

    SaveMaterialToFile(material, path.string(), true, customFragmentShader);
    Guid guid = GetMaterialGuidForPath(path.string());
    const std::string normalizedPath = NormalizeMaterialPath(path.string());

    auto materialPtr = std::make_shared<graphics::Material>(material);
    if (!materialPtr->GetShader() || !materialPtr->GetShader()->IsValid()) {
        auto defaultShader = LoadShader(
            "../Resources/Shaders/vertex.glsl",
            "../Resources/Shaders/fragment_enhanced.glsl"
        );
        if (defaultShader && defaultShader->IsValid())
            materialPtr->SetShader(defaultShader);
    }

    m_materialsByGuid[guid] = materialPtr;
    m_materialPathsByGuid[guid] = normalizedPath;
    m_materialGuidsByPath[normalizedPath] = guid;
    if (!customFragmentShader.empty())
        m_materialCustomFragmentByGuid[guid] = std::string(customFragmentShader);
    else
        m_materialCustomFragmentByGuid.erase(guid);
    return guid;
}

void AssetManager::ScanMaterialAssets(const std::string& materialsDir, bool createMissingMeta)
{
    std::filesystem::path dir = materialsDir;
    if (!dir.is_absolute())
        dir = std::filesystem::absolute(dir);

    if (!std::filesystem::exists(dir))
        return;

    m_materialGuidsByPath.clear();
    m_materialPathsByGuid.clear();

    std::size_t missingMetaCount = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".mat")
            continue;

        const std::string materialPath = NormalizeMaterialPath(entry.path().string());
        std::filesystem::path metaPath = entry.path();
        metaPath += ".meta";
        Guid guid{};

        if (LoadAssetMetaGuid(metaPath, guid)) {
            m_materialGuidsByPath[materialPath] = guid;
            m_materialPathsByGuid[guid] = materialPath;
            continue;
        }

        if (createMissingMeta) {
            guid = EnsureMetaForSource(materialPath, "Material");
            m_materialGuidsByPath[materialPath] = guid;
            m_materialPathsByGuid[guid] = materialPath;
            continue;
        }

        ++missingMetaCount;
    }

    if (missingMetaCount > 0) {
        EE_CORE_WARN("ScanMaterialAssets skipped {} .mat files without valid .meta in '{}'",
            missingMetaCount, dir.string());
    }
}

std::string AssetManager::GetMaterialPathByGuid(const Guid& guid) const
{
    auto it = m_materialPathsByGuid.find(guid);
    return it != m_materialPathsByGuid.end() ? it->second : std::string();
}

std::string AssetManager::GetMaterialNameByGuid(const Guid& guid) const
{
    auto path = GetMaterialPathByGuid(guid);
    if (path.empty())
        return {};
    return std::filesystem::path(path).stem().string();
}

const std::string* AssetManager::GetMaterialCustomFragmentShader(const Guid& guid) const
{
    auto it = m_materialCustomFragmentByGuid.find(guid);
    return it != m_materialCustomFragmentByGuid.end() ? &it->second : nullptr;
}

void AssetManager::SetMaterialCustomFragmentShader(const Guid& guid, std::string_view fragmentPath)
{
    if (!guid.IsValid())
        return;
    if (fragmentPath.empty())
        m_materialCustomFragmentByGuid.erase(guid);
    else
        m_materialCustomFragmentByGuid[guid] = std::string(fragmentPath);
}
