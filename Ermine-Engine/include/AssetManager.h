/* Start Header ************************************************************************/
/*!
\file       AssetManager.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu (75%)   
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu (20%)
\co-authors Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (5%)
\date       03/10/2025
\brief      This reflects the brief of the AssetManager system.
            This file is used to manage all the assets in the game.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "Shader.h"
#include "Texture.h"
#include "Cubemap.h"
#include "Model.h"
#include "Serialisation.h"
#include "Guid.h"
#include <functional>
#include <string_view>

// Forward declaration to avoid circular includes
namespace Ermine::graphics {
    class Material;
}
namespace Ermine
{

    /**
     * @brief Structure to hold resource database entry information
     */
    struct ResourceEntry {
        uint64_t instanceGUID;
        uint64_t typeGUID;
        std::string sourcePath;
        std::string outputPath;
        std::filesystem::file_time_type lastModified;
    };

    /**
     * @brief The AssetManager class is a singleton class that manages all the assets in the game.
     *        This includes textures and shaders.
     */
    class AssetManager
    {
        AssetManager() = default; // Private constructor for singleton
        ~AssetManager() = default;

        // Internal caching for assets
        std::unordered_map<std::string, std::shared_ptr<graphics::Texture>> m_textures;
        std::unordered_map<std::string, std::shared_ptr<graphics::Shader>> m_shaders;
        std::unordered_map<std::string, std::shared_ptr<graphics::Cubemap>> m_cubemaps;
        std::unordered_map<std::string, std::shared_ptr<graphics::Material>> m_materials;
        std::unordered_map<std::string, std::shared_ptr<graphics::Model>> m_models;
        std::unordered_map<Guid, std::shared_ptr<graphics::Material>> m_materialsByGuid;
        std::unordered_map<Guid, std::string> m_materialPathsByGuid;
        std::unordered_map<std::string, Guid> m_materialGuidsByPath;
        std::unordered_map<Guid, std::string> m_materialCustomFragmentByGuid;

        // Resource database management
        std::unordered_map<std::string, ResourceEntry> m_resourceDatabase; // sourcePath -> ResourceEntry
        std::string m_databasePath = "./Ermine-Game.lion_rcdbase"; // Default database path
        std::string m_projectGuid = ""; // Will be loaded from config or database
        bool m_databaseLoaded = false;

        // Internal methods for resource database
        bool LoadResourceDatabase();
        ResourceEntry* FindResourceBySourcePath(const std::string& sourcePath);
        std::string ConvertToRelativePath(const std::string& absolutePath);
        std::string GetFullDDSPath(const ResourceEntry& entry) const;
    
public:
        static AssetManager& GetInstance()
        {
            static AssetManager instance;
            return instance;
        }

        // ================== Database Management ==================
        /**
         * @brief Initialize the asset manager with database path
         * @param databasePath Path to the resource database
         * @param projectGuid Project GUID (optional, will try to auto-detect)
         * @return true if initialization successful
         */
        bool Initialize(const std::string& databasePath = "./Ermine-Game.lion_rcdbase",
            const std::string& projectGuid = "");

        /**
         * @brief Reload the resource database (useful after running resource pipeline)
         * @return true if reload successful
         */
        bool ReloadResourceDatabase();

        // ================== Texture Management ==================
        /**
         * @brief Load a texture from a file
         * @param filePath The path to the texture file
         * @return The loaded texture
         */
        std::shared_ptr<graphics::Texture> LoadTexture(const std::string& filePath);

         /**
         * @brief Load a texture directly by GUID (for advanced usage)
         * @param instanceGUID The instance GUID of the texture resource
         * @return The loaded texture
         */
        std::shared_ptr<graphics::Texture> LoadTextureByGUID(uint64_t instanceGUID);

        /**
         * @brief Get a texture from the cache
         * @param filePath The path to the texture file
         * @return The texture if it exists, nullptr otherwise
         */
        std::shared_ptr<graphics::Texture> GetTexture(const std::string& filePath);
        /**
         * @brief Get a read-only view of all currently loaded textures.
         * @return A const reference to the unordered_map of loaded textures.
         * The key is the texture file path, and the value is the shared Texture.
         */
        const std::unordered_map<std::string, std::shared_ptr<graphics::Texture>>& GetLoadedTextures() const;
        /**
         * @brief Resolve the best source path to persist in .mat files for a texture.
         * @param texture Texture reference currently bound to material parameter.
         * @return Source path when resolvable, otherwise the texture's current file path.
         */
        std::string ResolveTexturePathForMaterialWrite(const std::shared_ptr<graphics::Texture>& texture) const;

        // ================== Shader Management ==================
        /**
         * @brief Load a shader from a vertex and fragment file
         * @param vertexPath The path to the vertex shader file
         * @param fragmentPath The path to the fragment shader file
         * @return The loaded shader
         */
        std::shared_ptr<graphics::Shader> LoadShader(const std::string& compouteShader);
        /**
         * @brief Load a shader from a vertex and fragment file
         * @param vertexPath The path to the vertex shader file
         * @param fragmentPath The path to the fragment shader file
         * @return The loaded shader
         */
        std::shared_ptr<graphics::Shader> LoadShader(const std::string& vertexPath, const std::string& fragmentPath);

        /*@brief Load a shader from a vertex, geometry and fragment file
        * @param vertexPath The path to the vertex shader file
        * @param geometryPath The path to the geometry shader file
        * @param fragmentPath The path to the fragment shader file
        * @return The loaded shader
        */
        std::shared_ptr<graphics::Shader> LoadShader(const std::string & vertexPath, const std::string & geometryPath, const std::string & fragmentPath);
        /**
         * @brief Get a shader from the cache
         * @param shaderName The name of the shader
         * @return The shader if it exists, nullptr otherwise
         */
        std::shared_ptr<graphics::Shader> GetShader(const std::string& shaderName);
        /**
         * @brief Recompile all cached shaders in place.
         * @return true if all reloads succeeded, false otherwise.
         */
        bool ReloadCachedShaders();

        // ================== Model Management ==================
        /**
         * @brief Load a 3D model from file using Assimp.
         * @param filePath The path to the model file (e.g. .fbx, .obj, .gltf).
         * @return The loaded model.
         */
        std::shared_ptr<graphics::Model> LoadModel(const std::string& filePath);
        /**
         * @brief Get a model from the cache.
         * @param filePath The path to the model file.
         * @return The model if it exists, nullptr otherwise.
         */
        std::shared_ptr<graphics::Model> GetModel(const std::string& filePath);
        /**
         * @brief Get a read-only view of all currently loaded models.
         * @return A const reference to the unordered_map of loaded models.
         * The key is the model file path, and the value is the shared Model.
         */
        const std::unordered_map<std::string, std::shared_ptr<graphics::Model>>& GetLoadedModels() const;
        /**
         * @brief Unload a specific model from the cache.
         * @param filePath The full path to the model file to unload.
         */
        void UnloadModel(const std::string& filePath);

        /**
         * @brief Clear all cached models.
         *
         * This should be called when MeshManager is cleared to ensure Models
         * are reloaded and re-register their meshes.
         */
        void ClearModelCache();

        // ================== Utilities ==================
        /**
         * @brief Load the contents of a file into a buffer.
         * @param filepath The path to the file to load.
         * @return The contents of the file as a buffer.
         */
        const char* load_file_contents(const char* filepath);

        // ================== Cubemap management ==================
        /**
         * @brief Load a cubemap from individual face textures
         * @param faces Array of 6 face texture paths in order: +X, -X, +Y, -Y, +Z, -Z
         * @param name Optional name for the cubemap (for caching)
         * @return The loaded cubemap
         */
        std::shared_ptr<graphics::Cubemap> LoadCubemap(const std::array<std::string, 6>& faces, const std::string& name = "");
        
        /**
         * @brief Load a cubemap from an equirectangular texture
         * @param equirectangularPath Path to the equirectangular texture
         * @param name Optional name for the cubemap (for caching)
         * @return The loaded cubemap
         */
        std::shared_ptr<graphics::Cubemap> LoadCubemapFromEquirectangular(const std::string& equirectangularPath, const std::string& name = "");
        
        /**
         * @brief Get a cubemap from the cache
         * @param name The name of the cubemap
         * @return The cubemap if it exists, nullptr otherwise
         */
        std::shared_ptr<graphics::Cubemap> GetCubemap(const std::string& name);

        // ================== Material management ==================
        /**
         * @brief Sanitize a material/asset name for file-safe usage.
         * @param name Raw name (e.g. mesh name)
         * @return Sanitized name safe for filenames
         */
        static std::string SanitizeAssetName(std::string name);

        /**
         * @brief Create and cache a material with the given name
         * @param name The name/key for the material
         * @param shader The shader to use for the material
         * @param materialTemplate Optional material template to apply
         * @return The created material
         */
        std::shared_ptr<graphics::Material> CreateMaterial(const std::string& name, 
                                                         std::shared_ptr<graphics::Shader> shader,
                                                         const std::string& materialTemplate = "");
        
        /**
         * @brief Get a material from the cache
         * @param name The name of the material
         * @return The material if it exists, nullptr otherwise
         */
        std::shared_ptr<graphics::Material> GetMaterial(const std::string& name);
        
        /**
         * @brief Create a shared material for common use cases
         * @param materialType Type of material (e.g., "wood", "metal", "plastic")
         * @param shader The shader to use
         * @param baseTexture Optional base texture
         * @return The created shared material
         */
        std::shared_ptr<graphics::Material> CreateSharedMaterial(const std::string& materialType,
                                                               std::shared_ptr<graphics::Shader> shader,
                                                               std::shared_ptr<graphics::Texture> baseTexture = nullptr);

        /**
         * @brief Load a material asset from a .mat file (by path). Uses GUID meta for caching.
         * @param materialPath Path to .mat file
         * @param forceReload If true, reloads from disk even if cached
         * @return Loaded material (shared)
         */
        std::shared_ptr<graphics::Material> LoadMaterialAsset(const std::string& materialPath, bool forceReload = false);

        /**
         * @brief Get a cached material by GUID (loads from disk if known but not loaded).
         * @param guid Material GUID
         * @return Loaded material or nullptr
         */
        std::shared_ptr<graphics::Material> GetMaterialByGuid(const Guid& guid);

        /**
         * @brief Find the GUID for a loaded material pointer.
         * @param material Raw material pointer
         * @return GUID if found, otherwise default/invalid
         */
        Guid FindMaterialGuid(const graphics::Material* material) const;

        /**
         * @brief Get or create a GUID for a material path and cache the mapping.
         * @param materialPath Path to .mat file
         * @return GUID for the material asset
         */
        Guid GetMaterialGuidForPath(const std::string& materialPath);

        /**
         * @brief Create or load a material asset by name. If missing, saves a new .mat.
         * @param materialName Name (filename stem) for the material asset
         * @param initializer Optional initializer to populate a new material
         * @param outGuid Optional GUID out-parameter
         * @param materialsDir Directory for material assets
         * @return Loaded material (shared)
         */
        std::shared_ptr<graphics::Material> CreateMaterialAsset(
            const std::string& materialName,
            const std::function<void(graphics::Material&)>& initializer,
            Guid* outGuid = nullptr,
            const std::string& materialsDir = "../Resources/Materials/");

        /**
         * @brief Save a material asset to disk and register it (creates GUID meta if missing).
         * @param materialName Name (filename stem) for the material asset
         * @param material Material to save
         * @param overwrite If true, overwrite existing file
         * @param materialsDir Directory for material assets
         * @return GUID for the saved asset
         */
        Guid SaveMaterialAsset(
            const std::string& materialName,
            const graphics::Material& material,
            bool overwrite = false,
            std::string_view customFragmentShader = {},
            const std::string& materialsDir = "../Resources/Materials/");

        /**
         * @brief Scan the materials directory and cache GUID->path mappings.
         * @param materialsDir Directory to scan
         * @param createMissingMeta When true, generates missing .meta files for .mat assets.
         */
        void ScanMaterialAssets(const std::string& materialsDir = "../Resources/Materials/",
            bool createMissingMeta = false);

        /**
         * @brief Get cached GUID->path mappings for material assets.
         * @return Map of GUID to path
         */
        const std::unordered_map<Guid, std::string>& GetMaterialPathsByGuid() const { return m_materialPathsByGuid; }

        std::string GetMaterialPathByGuid(const Guid& guid) const;
        std::string GetMaterialNameByGuid(const Guid& guid) const;
        const std::string* GetMaterialCustomFragmentShader(const Guid& guid) const;
        void SetMaterialCustomFragmentShader(const Guid& guid, std::string_view fragmentPath);

        // Clear all loaded assets
        void Clear();
    };
}
