/* Start Header ************************************************************************/
/*!
\file       ResourcePipe.h
\author     HURNG Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu (100%)
\date       01/11/2025
\brief      This file contains the implementation of the ResourcePipeline system.
            It handles importing, converting, and caching of various asset types
            such as textures and meshes. The pipeline integrates with DirectXTex
            for texture processing (format conversion, mipmap generation, compression)
            and Assimp for mesh importing (static and skinned meshes). It also manages
            an asset database that tracks asset metadata, cache locations, and
            reimport status. The pipeline automates scanning project directories,
            reimporting outdated resources, and generating binary cache files
            (.dds, .mesh, .skin) used by the engine at runtime.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <dxgiformat.h> 
#include "AssetDatabase.h"


struct aiScene;
struct aiMesh;
struct aiBone;
struct aiVertexWeight;
namespace Assimp {
    class Importer;
}

namespace Ermine {

    // Type GUIDs for resources (should match your offline pipeline)
    inline constexpr xresource::type_guid TEXTURE_TYPE_GUID("TEXTURE_RESOURCE_TYPE");
    inline constexpr xresource::type_guid STATIC_MESH_TYPE_GUID("STATIC_MESH_RESOURCE");
    inline constexpr xresource::type_guid SKINNED_MESH_TYPE_GUID("SKINNED_MESH_RESOURCE");

    //=============================================================================
    // Mesh Data Structures
    //=============================================================================
    struct Vertex {
        float position[3];
        float normal[3];
        float texCoord[2];
        float tangent[3];
    };

    struct SkinnedVertex {
        float position[3];
        float normal[3];
        float texCoord[2];
        float tangent[3];
        int boneIndices[4];
        float boneWeights[4];
    };

    struct BoneInfo {
        std::string name;
        float offsetMatrix[16]; // 4x4 matrix stored as flat array
        int index;
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct SkinnedMeshData {
        std::vector<SkinnedVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<BoneInfo> bones;
    };

    //=============================================================================
    // Import Settings
    //=============================================================================
    struct TextureImportSettings {
        bool generateMipmaps = true;
        bool useCompression = false;
        DXGI_FORMAT targetFormat;

        TextureImportSettings();
    };

    struct MeshImportSettings {
        bool generateNormals = true;
        bool generateTangents = true;
        bool optimizeVertices = true;
        bool flipUVs = true;
        bool combineMeshes = true; // For skinned meshes with multiple sub-meshes

        bool applyPreTransform = false;
        float scale[3] = { 1.0f, 1.0f, 1.0f };        // Uniform or per-axis scale
        float translation[3] = { 0.0f, 0.0f, 0.0f };  // Position offset
        float rotation[3] = { 0.0f, 0.0f, 0.0f };     // Euler angles in degrees (X, Y, Z)

        MeshImportSettings() = default;

    };

    //=============================================================================
    // Import Result
    //=============================================================================
    struct ImportResult {
        bool success = false;
        std::string errorMessage;
        std::string outputPath;
        xresource::full_guid resourceGuid;
        size_t outputSize = 0;
        float importTimeMs = 0.0f;
    };

    //=============================================================================
    // ResourcePipeline - Main runtime asset pipeline
    //=============================================================================
    class ResourcePipeline {
    public:
        ResourcePipeline();
        ~ResourcePipeline();

        // Initialize the pipeline with project paths
        bool Initialize(const std::string& projectRootPath);

        // Shutdown and cleanup
        void Shutdown();

        // Import a texture file (PNG, JPG, TGA, etc.) to DDS
        ImportResult ImportTexture(const std::string& sourcePath,
            const TextureImportSettings& settings = {});

        // Import a mesh file (FBX, OBJ, GLTF, etc.)
        ImportResult ImportMesh(const std::string& sourcePath,
            const MeshImportSettings& settings = {});

        // Check if a file needs reimporting (modified since last import)
        bool NeedsReimport(const std::string& sourcePath) const;

        // Force reimport of an asset even if unchanged
        ImportResult ReimportAsset(const std::string& sourcePath);

        // Scan project for new/modified assets and import them
        void ScanAndImportAssets(const std::string& assetsFolder = "Assets");

        // Get the asset database
        AssetDatabase& GetDatabase() { return m_Database; }
        const AssetDatabase& GetDatabase() const { return m_Database; }

        // Get paths
        std::string GetCachePath() const { return m_CachePath; }
        std::string GetProjectPath() const { return m_ProjectPath; }

        // Check if pipeline is initialized
        bool IsInitialized() const { return m_Initialized; }

        static const char* GetFormatName(DXGI_FORMAT format);
        static std::vector<DXGI_FORMAT> GetSupportedFormats();

    private:
        // Internal import functions
        ImportResult ImportTextureInternal(const std::string& sourcePath,
            const std::string& outputPath,
            const TextureImportSettings& settings);

        ImportResult ImportMeshInternal(const std::string& sourcePath,
            const std::string& outputPath,
            const MeshImportSettings& settings);

        // Helper functions
        bool InitializeDirectXTex();
        void CleanupDirectXTex();

        std::string GenerateCachePath(const xresource::full_guid& guid, const std::string& extension) const;
        bool IsTextureFile(const std::string& extension) const;
        bool IsMeshFile(const std::string& extension) const;

        // DirectXTex texture conversion
        bool ConvertTextureToDDS(const std::string& inputPath,
            const std::string& outputPath,
            const TextureImportSettings& settings);

        // Determine optimal compression format based on filename
        DXGI_FORMAT DetermineOptimalFormat(const std::string& filename);

        // Assimp mesh conversion helpers
        bool LoadAssimpScene(const std::string& filePath,
            const aiScene*& outScene,
            Assimp::Importer& importer,
            const MeshImportSettings& settings);

        bool HasSkinning(const aiMesh* mesh) const;

        void BuildTransformMatrix(const MeshImportSettings& settings, float outMatrix[16]);
        void TransformVector(const float matrix[16], const float in[3], float out[3]);
        void TransformNormal(const float matrix[16], const float in[3], float out[3]);

        bool ProcessStaticMesh(const aiMesh* mesh, MeshData& outData, const MeshImportSettings& settings);

        bool ProcessSkinnedMeshCombined(const aiScene* scene, SkinnedMeshData& outData, const MeshImportSettings& settings);

        // File writing
        bool WriteMeshFile(const std::string& outputPath, const MeshData& meshData);
        bool WriteSkinFile(const std::string& outputPath, const SkinnedMeshData& meshData);

    private:
        bool m_Initialized;
        bool m_COMInitialized;

        std::string m_ProjectPath;      // Root project directory
        std::string m_CachePath;        // Cache directory for processed assets

        AssetDatabase m_Database;       // Asset metadata database
    };

} // namespace Ermine