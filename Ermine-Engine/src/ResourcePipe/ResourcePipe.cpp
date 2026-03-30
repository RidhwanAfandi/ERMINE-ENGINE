/* Start Header ************************************************************************/
/*!
\file       ResourcePipe.cpp
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

#include <PreCompile.h>
#include "ResourcePipe.h"
#include "TangentSpace.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

// DirectXTex
#include <DirectXTex.h>

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace Ermine {

    //=============================================================================
    // TextureImportSettings
    //=============================================================================
    TextureImportSettings::TextureImportSettings()
        : targetFormat(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
    }

    //=============================================================================
    // ResourcePipeline
    //=============================================================================
    ResourcePipeline::ResourcePipeline()
        : m_Initialized(false)
        , m_COMInitialized(false) {
    }

    ResourcePipeline::~ResourcePipeline() {
        Shutdown();
    }

    bool ResourcePipeline::Initialize(const std::string& projectRootPath) {
        if (m_Initialized) {
            std::cout << "[ResourcePipeline] Already initialized." << std::endl;
            return true;
        }

        m_ProjectPath = std::filesystem::absolute(projectRootPath).string();
        m_CachePath = (std::filesystem::path(m_ProjectPath) / "Cache").string();

        std::cout << "[ResourcePipeline] Initializing..." << std::endl;
        std::cout << "  Project Path: " << m_ProjectPath << std::endl;
        std::cout << "  Cache Path: " << m_CachePath << std::endl;

        // Create cache directory
        std::filesystem::create_directories(m_CachePath);

        // Initialize DirectXTex (COM)
        if (!InitializeDirectXTex()) {
            std::cerr << "[ResourcePipeline] Failed to initialize DirectXTex." << std::endl;
            return false;
        }

        // Initialize asset database
        if (!m_Database.Initialize(m_ProjectPath)) {
            std::cerr << "[ResourcePipeline] Failed to initialize asset database." << std::endl;
            CleanupDirectXTex();
            return false;
        }

        m_Initialized = true;
        std::cout << "[ResourcePipeline] Initialization complete!" << std::endl;

        return true;
    }

    void ResourcePipeline::Shutdown() {
        if (!m_Initialized) return;

        std::cout << "[ResourcePipeline] Shutting down..." << std::endl;

        // Save database before shutdown
        m_Database.Save();

        // Cleanup DirectXTex
        CleanupDirectXTex();

        m_Initialized = false;
    }

    bool ResourcePipeline::InitializeDirectXTex() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            std::cerr << "[ResourcePipeline] Failed to initialize COM for DirectXTex." << std::endl;
            return false;
        }
        m_COMInitialized = true;
        return true;
    }

    void ResourcePipeline::CleanupDirectXTex() {
        if (m_COMInitialized) {
            CoUninitialize();
            m_COMInitialized = false;
        }
    }

    const char* ResourcePipeline::GetFormatName(DXGI_FORMAT format) {
        switch (format) {
        case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8 (Uncompressed)";
        case DXGI_FORMAT_B8G8R8A8_UNORM: return "BGRA8 (Uncompressed)";
        case DXGI_FORMAT_BC1_UNORM: return "BC1 (DXT1) - No Alpha";
        case DXGI_FORMAT_BC2_UNORM: return "BC2 (DXT3) - Sharp Alpha";
        case DXGI_FORMAT_BC3_UNORM: return "BC3 (DXT5) - Smooth Alpha";
        //case DXGI_FORMAT_BC7_UNORM: return "BC7 (High Quality)";
        case DXGI_FORMAT_BC4_UNORM: return "BC4 (Grayscale)";
        case DXGI_FORMAT_BC5_UNORM: return "BC5 (Normal Maps)";
        default: return "Unknown";
        }
    }

    std::vector<DXGI_FORMAT> ResourcePipeline::GetSupportedFormats() {
        return {
            DXGI_FORMAT_R8G8B8A8_UNORM,  // Good for UI, requires alpha
            DXGI_FORMAT_B8G8R8A8_UNORM,  // Default uncompressed
            DXGI_FORMAT_BC1_UNORM,       // Best compression, no alpha
            DXGI_FORMAT_BC3_UNORM,       // Good compression with alpha
            //DXGI_FORMAT_BC7_UNORM,       // Best quality compression
            DXGI_FORMAT_BC5_UNORM,        // For normal maps
            DXGI_FORMAT_BC4_UNORM
        };
    }

    ImportResult ResourcePipeline::ImportTexture(const std::string& sourcePath,
        const TextureImportSettings& settings) {
        if (!m_Initialized) {
            return { false, "Pipeline not initialized" };
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        // Convert to absolute path
        std::string absoluteSource = std::filesystem::absolute(sourcePath).string();

        if (!std::filesystem::exists(absoluteSource)) {
            return { false, "Source file does not exist: " + absoluteSource };
        }

        // Check if asset already exists in database
        AssetEntry* existingEntry = m_Database.FindBySourcePath(absoluteSource);
        xresource::full_guid guid;

        if (existingEntry) {
            // Reuse existing GUID
            guid = existingEntry->guid;
            std::cout << "[ResourcePipeline] Reimporting texture: " << std::filesystem::path(sourcePath).filename() << std::endl;
        }
        else {
            // Generate new GUID
            auto instanceGuid = xresource::instance_guid::GenerateGUIDCopy();
            guid = xresource::full_guid{ instanceGuid, TEXTURE_TYPE_GUID };
            std::cout << "[ResourcePipeline] Importing new texture: " << std::filesystem::path(sourcePath).filename() << std::endl;
        }

        // Generate output path
        std::string outputPath = GenerateCachePath(guid, ".dds");

        // Import the texture
        ImportResult result = ImportTextureInternal(absoluteSource, outputPath, settings);

        if (result.success) {
            // Update database
            AssetEntry entry;
            entry.guid = guid;
            entry.sourcePath = absoluteSource;
            entry.cachedPath = outputPath;
            entry.lastModified = std::filesystem::last_write_time(absoluteSource);
            entry.resourceType = "TEXTURE";

            if (existingEntry) {
                m_Database.UpdateAsset(absoluteSource, entry);
            }
            else {
                m_Database.RegisterAsset(entry);
            }

            // Save database
            m_Database.Save();

            result.resourceGuid = guid;
            result.outputPath = outputPath;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.importTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        return result;
    }

    ImportResult ResourcePipeline::ImportMesh(const std::string& sourcePath,
        const MeshImportSettings& settings) {
        if (!m_Initialized) {
            return { false, "Pipeline not initialized" };
        }

        auto startTime = std::chrono::high_resolution_clock::now();

        // Convert to absolute path
        std::string absoluteSource = std::filesystem::absolute(sourcePath).string();

        if (!std::filesystem::exists(absoluteSource)) {
            return { false, "Source file does not exist: " + absoluteSource };
        }

        // Load scene to determine if it's skinned or static
        Assimp::Importer importer;
        const aiScene* scene = nullptr;
        if (!LoadAssimpScene(absoluteSource, scene, importer, settings)) {
            return { false, "Failed to load mesh with Assimp" };
        }

        if (scene->mNumMeshes == 0) {
            return { false, "No meshes found in file" };
        }

        // Determine if skinned or static
        bool isSkinned = HasSkinning(scene->mMeshes[0]);
        xresource::type_guid typeGuid = isSkinned ? SKINNED_MESH_TYPE_GUID : STATIC_MESH_TYPE_GUID;
        std::string resourceType = isSkinned ? "SKINNED_MESH" : "STATIC_MESH";
        std::string extension = isSkinned ? ".skin" : ".mesh";

        // Check if asset already exists in database
        AssetEntry* existingEntry = m_Database.FindBySourcePath(absoluteSource);
        xresource::full_guid guid;

        if (existingEntry) {
            guid = existingEntry->guid;
            std::cout << "[ResourcePipeline] Reimporting " << resourceType << ": "
                << std::filesystem::path(sourcePath).filename() << std::endl;
        }
        else {
            auto instanceGuid = xresource::instance_guid::GenerateGUIDCopy();
            guid = xresource::full_guid{ instanceGuid, typeGuid };
            std::cout << "[ResourcePipeline] Importing new " << resourceType << ": "
                << std::filesystem::path(sourcePath).filename() << std::endl;
        }

        // Generate output path
        std::string outputPath = GenerateCachePath(guid, extension);

        // Import the mesh
        ImportResult result = ImportMeshInternal(absoluteSource, outputPath, settings);

        if (result.success) {
            // Update database
            AssetEntry entry;
            entry.guid = guid;
            entry.sourcePath = absoluteSource;
            entry.cachedPath = outputPath;
            entry.lastModified = std::filesystem::last_write_time(absoluteSource);
            entry.resourceType = resourceType;

            if (existingEntry) {
                m_Database.UpdateAsset(absoluteSource, entry);
            }
            else {
                m_Database.RegisterAsset(entry);
            }

            m_Database.Save();

            result.resourceGuid = guid;
            result.outputPath = outputPath;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        result.importTimeMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        return result;
    }

    bool ResourcePipeline::NeedsReimport(const std::string& sourcePath) const {
        return m_Database.NeedsReimport(sourcePath);
    }

    ImportResult ResourcePipeline::ReimportAsset(const std::string& sourcePath) {
        std::string extension = std::filesystem::path(sourcePath).extension().string();

        if (IsTextureFile(extension)) {
            return ImportTexture(sourcePath);
        }
        else if (IsMeshFile(extension)) {
            return ImportMesh(sourcePath);
        }

        return { false, "Unknown file type: " + extension };
    }

    void ResourcePipeline::ScanAndImportAssets(const std::string& assetsFolder) {
        if (!m_Initialized) {
            std::cerr << "[ResourcePipeline] Cannot scan - pipeline not initialized." << std::endl;
            return;
        }

        std::string fullAssetsPath = (std::filesystem::path(m_ProjectPath) / assetsFolder).string();

        if (!std::filesystem::exists(fullAssetsPath)) {
            std::cerr << "[ResourcePipeline] Assets folder does not exist: " << fullAssetsPath << std::endl;
            return;
        }

        std::cout << "[ResourcePipeline] Scanning for assets in: " << fullAssetsPath << std::endl;

        int importedCount = 0;
        int skippedCount = 0;
        int failedCount = 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(fullAssetsPath)) {
            if (!entry.is_regular_file()) continue;

            std::string extension = entry.path().extension().string();
            std::string filePath = entry.path().string();

            // Skip if not a supported asset type
            if (!IsTextureFile(extension) && !IsMeshFile(extension)) {
                continue;
            }

            // Check if needs reimport
            if (!NeedsReimport(filePath)) {
                std::cout << "  ⏭ Skipping (up-to-date): " << entry.path().filename() << std::endl;
                skippedCount++;
                continue;
            }

            // Import the asset
            ImportResult result = ReimportAsset(filePath);

            if (result.success) {
                std::cout << "  ✓ Imported: " << entry.path().filename()
                    << " (" << result.importTimeMs << "ms)" << std::endl;
                importedCount++;
            }
            else {
                std::cerr << "  ✗ Failed: " << entry.path().filename()
                    << " - " << result.errorMessage << std::endl;
                failedCount++;
            }
        }

        std::cout << "[ResourcePipeline] Scan complete: " << importedCount << " imported, "
            << skippedCount << " skipped, " << failedCount << " failed" << std::endl;
    }

    //=============================================================================
    // Helper Functions
    //=============================================================================
    std::string ResourcePipeline::GenerateCachePath(const xresource::full_guid& guid,
        const std::string& extension) const {
        std::string guidStr = std::to_string(guid.m_Instance.m_Value);
        return (std::filesystem::path(m_CachePath) / (guidStr + extension)).string();
    }

    bool ResourcePipeline::IsTextureFile(const std::string& extension) const {
        std::string ext = extension;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".tga" || ext == ".bmp" || ext == ".dds";
    }

    bool ResourcePipeline::IsMeshFile(const std::string& extension) const {
        std::string ext = extension;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
            ext == ".glb" || ext == ".dae";
    }

    //=============================================================================
    // Texture Import Implementation
    //=============================================================================

    // Determine optimal compression format based on texture filename
    DXGI_FORMAT ResourcePipeline::DetermineOptimalFormat(const std::string& filename) {
        // Convert filename to lowercase for case-insensitive matching
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);

        // ✅ NORMAL MAPS: BC5 (always linear - stores XY direction vectors)
        if (lowerFilename.find("normal") != std::string::npos ||
            lowerFilename.find("norm") != std::string::npos ||
            lowerFilename.find("_n.") != std::string::npos ||
            lowerFilename.find("_nrm") != std::string::npos) {
            return DXGI_FORMAT_BC5_UNORM;  // Linear - critical!
        }

        // ✅ DATA TEXTURES: BC4 Linear (grayscale data, not for display)
        if (lowerFilename.find("roughness") != std::string::npos ||
            lowerFilename.find("metallic") != std::string::npos ||
            lowerFilename.find("metalness") != std::string::npos ||
            lowerFilename.find("ao") != std::string::npos ||
            lowerFilename.find("ambient") != std::string::npos ||
            lowerFilename.find("occlusion") != std::string::npos ||
            lowerFilename.find("height") != std::string::npos ||
            lowerFilename.find("displacement") != std::string::npos ||
            lowerFilename.find("_r.") != std::string::npos ||
            lowerFilename.find("_m.") != std::string::npos) {
            return DXGI_FORMAT_BC4_UNORM;  // Linear - stores data values
        }

        // ✅ COLOR TEXTURES: BC3 sRGB (gamma-corrected colors for display)
        if (lowerFilename.find("albedo") != std::string::npos ||
            lowerFilename.find("diffuse") != std::string::npos ||
            lowerFilename.find("color") != std::string::npos ||
            lowerFilename.find("basecolor") != std::string::npos ||
            lowerFilename.find("_d.") != std::string::npos ||
            lowerFilename.find("_a.") != std::string::npos) {
            return DXGI_FORMAT_BC3_UNORM_SRGB;  // sRGB - for display colors!
        }

        // ✅ EMISSIVE: BC3 sRGB (emissive colors)
        if (lowerFilename.find("emissive") != std::string::npos ||
            lowerFilename.find("emission") != std::string::npos) {
            return DXGI_FORMAT_BC3_UNORM_SRGB;  // sRGB
        }

        // ✅ OPAQUE TEXTURES: BC1 sRGB (no alpha, gamma-corrected)
        if (lowerFilename.find("opaque") != std::string::npos ||
            lowerFilename.find("noalpha") != std::string::npos) {
            return DXGI_FORMAT_BC1_UNORM_SRGB;  // sRGB
        }

        // ✅ DEFAULT: BC3 sRGB (safe for most color textures with alpha)
        return DXGI_FORMAT_BC3_UNORM_SRGB;
    }

    ImportResult ResourcePipeline::ImportTextureInternal(const std::string& sourcePath,
        const std::string& outputPath,
        const TextureImportSettings& settings) {
        if (!ConvertTextureToDDS(sourcePath, outputPath, settings)) {
            return { false, "Failed to convert texture to DDS" };
        }

        size_t fileSize = std::filesystem::file_size(outputPath);

        return {
            true,
            "",
            outputPath,
            {},
            fileSize
        };
    }

    bool ResourcePipeline::ConvertTextureToDDS(const std::string& inputPath,
        const std::string& outputPath,
        const TextureImportSettings& settings) {
        using namespace DirectX;

        // Convert paths to wide strings
        std::wstring wInput(inputPath.begin(), inputPath.end());
        std::wstring wOutput(outputPath.begin(), outputPath.end());

        // Load the image
        ScratchImage image;
        HRESULT hr = LoadFromWICFile(wInput.c_str(), WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
        if (FAILED(hr)) {
            std::cerr << "    Failed to load texture with WIC" << std::endl;
            return false;
        }

        TexMetadata metadata = image.GetMetadata();
        const Image* sourceImages = image.GetImages();
        size_t sourceImageCount = image.GetImageCount();

        // ✅ Step 1: Flip based on target format
        // Compressed formats need flipping during conversion
        // Uncompressed formats will be flipped during loading
        bool isCompressedFormat = IsCompressed(settings.targetFormat);

        ScratchImage flipped;
        if (isCompressedFormat) {
            hr = FlipRotate(sourceImages, sourceImageCount, metadata,
                TEX_FR_FLIP_VERTICAL, flipped);
            if (SUCCEEDED(hr)) {
                sourceImages = flipped.GetImages();
                sourceImageCount = flipped.GetImageCount();
                metadata = flipped.GetMetadata();
                std::cout << "    Flipped texture vertically for OpenGL (compressed format)" << std::endl;
            }
            else {
                std::cerr << "    Warning: Failed to flip texture" << std::endl;
            }
        }

        // ✅ Step 2: Generate mipmaps (after flip, before compression)
        ScratchImage mipChain;
        if (settings.generateMipmaps && metadata.mipLevels == 1) {
            hr = GenerateMipMaps(sourceImages, sourceImageCount, metadata,
                TEX_FILTER_DEFAULT, 0, mipChain);
            if (SUCCEEDED(hr)) {
                sourceImages = mipChain.GetImages();
                sourceImageCount = mipChain.GetImageCount();
                metadata = mipChain.GetMetadata();
                std::cout << "    Generated " << metadata.mipLevels << " mip levels" << std::endl;
            }
        }

        // ✅ Step 3: Compress or Convert
        ScratchImage finalImage;

        if (isCompressedFormat) {
            // ✅ COMPRESS to BC1/BC3/BC5/BC7
            std::cout << "    Compressing to " << GetFormatName(settings.targetFormat) << "..." << std::endl;

            hr = Compress(sourceImages, sourceImageCount, metadata,
                settings.targetFormat,
                TEX_COMPRESS_DEFAULT,
                TEX_THRESHOLD_DEFAULT,
                finalImage);

            if (FAILED(hr)) {
                std::cerr << "    Failed to compress texture (HRESULT: 0x" << std::hex << hr << std::dec << ")" << std::endl;
                return false;
            }
        }
        else {
            // ✅ Just convert pixel format (RGBA, BGRA, etc.)
            if (metadata.format != settings.targetFormat) {
                std::cout << "    Converting format to " << GetFormatName(settings.targetFormat) << "..." << std::endl;

                hr = Convert(sourceImages, sourceImageCount, metadata,
                    settings.targetFormat,
                    TEX_FILTER_DEFAULT,
                    TEX_THRESHOLD_DEFAULT,
                    finalImage);

                if (FAILED(hr)) {
                    std::cerr << "    Failed to convert texture format" << std::endl;
                    return false;
                }
            }
            else {
                // No conversion needed, use source directly
                if (settings.generateMipmaps && mipChain.GetImageCount() > 0) {
                    finalImage = std::move(mipChain);
                }
                else if (flipped.GetImageCount() > 0) {
                    finalImage = std::move(flipped);
                }
                else {
                    finalImage = std::move(image);
                }
            }
        }

        // ✅ Step 4: Save to DDS
        std::cout << "    Saving DDS file..." << std::endl;
        hr = SaveToDDSFile(finalImage.GetImages(), finalImage.GetImageCount(),
            finalImage.GetMetadata(), DDS_FLAGS_NONE, wOutput.c_str());

        if (FAILED(hr)) {
            std::cerr << "    Failed to save DDS file" << std::endl;
            return false;
        }

        // ✅ Print compression stats
        size_t inputSize = std::filesystem::file_size(inputPath);
        size_t outputSize = std::filesystem::file_size(outputPath);
        float ratio = (float)inputSize / (float)outputSize;

        std::cout << "    ✓ Compressed: " << inputSize / 1024 << " KB → "
            << outputSize / 1024 << " KB (" << std::fixed << std::setprecision(1)
            << ratio << "x compression)" << std::endl;

        return true;
    }

    //=============================================================================
    // Mesh Import Implementation
    //=============================================================================
    ImportResult ResourcePipeline::ImportMeshInternal(const std::string& sourcePath,
        const std::string& outputPath,
        const MeshImportSettings& settings) {
        // Load scene
        Assimp::Importer importer;
        const aiScene* scene = nullptr;

        if (!LoadAssimpScene(sourcePath, scene, importer, settings)) {
            return { false, "Failed to load scene with Assimp" };
        }

        if (scene->mNumMeshes == 0) {
            return { false, "No meshes found in scene" };
        }

        // Determine if skinned
        bool isSkinned = HasSkinning(scene->mMeshes[0]);

        bool success = false;
        if (isSkinned) {
            // ✅ CHANGED: Pass settings parameter
            SkinnedMeshData skinnedData;
            if (ProcessSkinnedMeshCombined(scene, skinnedData, settings)) {
                success = WriteSkinFile(outputPath, skinnedData);
            }
        }
        else {
            // ✅ CHANGED: Pass settings parameter
            MeshData meshData;
            if (ProcessStaticMesh(scene->mMeshes[0], meshData, settings)) {
                success = WriteMeshFile(outputPath, meshData);
            }
        }

        if (!success) {
            return { false, "Failed to process and write mesh data" };
        }

        size_t fileSize = std::filesystem::file_size(outputPath);

        return {
            true,
            "",
            outputPath,
            {},
            fileSize
        };
    }

    bool ResourcePipeline::LoadAssimpScene(const std::string& filePath,
        const aiScene*& outScene,
        Assimp::Importer& importer,
        const MeshImportSettings& settings) {
        unsigned int importFlags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_SortByPType |
            aiProcess_LimitBoneWeights;

        if (settings.generateNormals) {
            importFlags |= aiProcess_GenNormals;
        }

        if (settings.generateTangents) {
            importFlags |= aiProcess_CalcTangentSpace;
        }

        if (settings.flipUVs) {
            importFlags |= aiProcess_FlipUVs;
        }

        if (settings.optimizeVertices) {
            importFlags |= aiProcess_ImproveCacheLocality;
        }

        outScene = importer.ReadFile(filePath, importFlags);

        if (!outScene || outScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !outScene->mRootNode) {
            std::cerr << "    Assimp error: " << importer.GetErrorString() << std::endl;
            return false;
        }

        return true;
    }

    bool ResourcePipeline::HasSkinning(const aiMesh* mesh) const {
        return mesh->HasBones();
    }

    // Helper function to create a 4x4 transformation matrix
    void ResourcePipeline::BuildTransformMatrix(const MeshImportSettings& settings, float outMatrix[16]) {
        // Initialize as identity matrix
        for (int i = 0; i < 16; i++) {
            outMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }

        if (!settings.applyPreTransform) return;

        // Convert rotation from degrees to radians
        float rx = settings.rotation[0] * 3.14159265f / 180.0f;
        float ry = settings.rotation[1] * 3.14159265f / 180.0f;
        float rz = settings.rotation[2] * 3.14159265f / 180.0f;

        // Build rotation matrices
        float cosX = cosf(rx), sinX = sinf(rx);
        float cosY = cosf(ry), sinY = sinf(ry);
        float cosZ = cosf(rz), sinZ = sinf(rz);

        // Combined rotation matrix (ZYX order - typical for 3D engines)
        float rotMatrix[16] = {
            cosY * cosZ,
            cosY * sinZ,
            -sinY,
            0,

            sinX * sinY * cosZ - cosX * sinZ,
            sinX * sinY * sinZ + cosX * cosZ,
            sinX * cosY,
            0,

            cosX * sinY * cosZ + sinX * sinZ,
            cosX * sinY * sinZ - sinX * cosZ,
            cosX * cosY,
            0,

            0, 0, 0, 1
        };

        // Apply scale to rotation matrix
        for (int col = 0; col < 3; col++) {
            for (int row = 0; row < 3; row++) {
                outMatrix[col * 4 + row] = rotMatrix[col * 4 + row] * settings.scale[row];
            }
        }

        // Apply translation
        outMatrix[12] = settings.translation[0];
        outMatrix[13] = settings.translation[1];
        outMatrix[14] = settings.translation[2];
        outMatrix[15] = 1.0f;
    }

    // Helper function to transform a 3D vector by a 4x4 matrix
    void ResourcePipeline::TransformVector(const float matrix[16], const float in[3], float out[3]) {
        out[0] = matrix[0] * in[0] + matrix[4] * in[1] + matrix[8] * in[2] + matrix[12];
        out[1] = matrix[1] * in[0] + matrix[5] * in[1] + matrix[9] * in[2] + matrix[13];
        out[2] = matrix[2] * in[0] + matrix[6] * in[1] + matrix[10] * in[2] + matrix[14];
    }

    // Helper function to transform a normal/tangent (ignores translation)
    void ResourcePipeline::TransformNormal(const float matrix[16], const float in[3], float out[3]) {
        out[0] = matrix[0] * in[0] + matrix[4] * in[1] + matrix[8] * in[2];
        out[1] = matrix[1] * in[0] + matrix[5] * in[1] + matrix[9] * in[2];
        out[2] = matrix[2] * in[0] + matrix[6] * in[1] + matrix[10] * in[2];

        // Normalize the result
        float length = sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]);
        if (length > 0.0001f) {
            out[0] /= length;
            out[1] /= length;
            out[2] /= length;
        }
    }


    bool ResourcePipeline::ProcessStaticMesh(const aiMesh* mesh, MeshData& outData,
        const MeshImportSettings& settings) {
        // Build transformation matrix if needed
        float transformMatrix[16];
        BuildTransformMatrix(settings, transformMatrix);

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex = {};

            // Position - apply full transformation
            float pos[3] = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (settings.applyPreTransform) {
                TransformVector(transformMatrix, pos, vertex.position);
            }
            else {
                vertex.position[0] = pos[0];
                vertex.position[1] = pos[1];
                vertex.position[2] = pos[2];
            }

            // Normal - apply rotation and scale only
            if (mesh->HasNormals()) {
                float norm[3] = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
                if (settings.applyPreTransform) {
                    TransformNormal(transformMatrix, norm, vertex.normal);
                }
                else {
                    vertex.normal[0] = norm[0];
                    vertex.normal[1] = norm[1];
                    vertex.normal[2] = norm[2];
                }
            }

            // Texture coordinates (unchanged)
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord[0] = mesh->mTextureCoords[0][i].x;
                vertex.texCoord[1] = mesh->mTextureCoords[0][i].y;
            }

            // Tangent - apply rotation and scale only
            if (mesh->HasTangentsAndBitangents()) {
                glm::vec3 tangent(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                glm::vec3 bitangent(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
                if (settings.applyPreTransform) {
                    float transformedTangent[3];
                    float transformedBitangent[3];
                    const float tangentIn[3] = { tangent.x, tangent.y, tangent.z };
                    const float bitangentIn[3] = { bitangent.x, bitangent.y, bitangent.z };
                    TransformNormal(transformMatrix, tangentIn, transformedTangent);
                    TransformNormal(transformMatrix, bitangentIn, transformedBitangent);
                    Ermine::StoreTangent(
                        vertex.tangent,
                        Ermine::BuildTangentData(
                            glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]),
                            glm::vec3(transformedTangent[0], transformedTangent[1], transformedTangent[2]),
                            glm::vec3(transformedBitangent[0], transformedBitangent[1], transformedBitangent[2])));
                }
                else {
                    Ermine::StoreTangent(
                        vertex.tangent,
                        Ermine::BuildTangentData(
                            glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]),
                            tangent,
                            bitangent));
                }
            }
            else {
                Ermine::StoreTangent(vertex.tangent, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            }

            outData.vertices.push_back(vertex);
        }

        // Process indices (unchanged)
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                outData.indices.push_back(face.mIndices[j]);
            }
        }

        if (settings.applyPreTransform) {
            std::cout << "    ✓ Applied pre-transformation: "
                << "Scale(" << settings.scale[0] << "," << settings.scale[1] << "," << settings.scale[2] << ") "
                << "Rotation(" << settings.rotation[0] << "°," << settings.rotation[1] << "°," << settings.rotation[2] << "°) "
                << "Translation(" << settings.translation[0] << "," << settings.translation[1] << "," << settings.translation[2] << ")"
                << std::endl;
        }

        return !outData.vertices.empty();
    }

    bool ResourcePipeline::ProcessSkinnedMeshCombined(const aiScene* scene, SkinnedMeshData& outData,
        const MeshImportSettings& settings) {
        // Build transformation matrix if needed
        float transformMatrix[16];
        BuildTransformMatrix(settings, transformMatrix);

        // Build bone mapping from ALL meshes
        std::map<std::string, int> boneMapping;

        // First pass: collect all unique bones (unchanged)
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            const aiMesh* sceneMesh = scene->mMeshes[meshIdx];
            if (!sceneMesh->HasBones()) continue;

            for (unsigned int i = 0; i < sceneMesh->mNumBones; i++) {
                aiBone* bone = sceneMesh->mBones[i];
                std::string boneName = bone->mName.C_Str();

                if (boneMapping.find(boneName) == boneMapping.end()) {
                    BoneInfo boneInfo;
                    boneInfo.name = boneName;

                    // Convert aiMatrix4x4 to flat array
                    for (int row = 0; row < 4; row++) {
                        for (int col = 0; col < 4; col++) {
                            boneInfo.offsetMatrix[row * 4 + col] = bone->mOffsetMatrix[row][col];
                        }
                    }

                    boneInfo.index = (int)outData.bones.size();

                    outData.bones.push_back(boneInfo);
                    boneMapping[boneName] = boneInfo.index;
                }
            }
        }

        // Process all meshes
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            const aiMesh* mesh = scene->mMeshes[meshIdx];

            if (!mesh->HasBones()) {
                std::cout << "    ⚠ Skipping non-skinned mesh: " << mesh->mName.C_Str() << std::endl;
                continue;
            }

            uint32_t baseVertex = (uint32_t)outData.vertices.size();

            // Initialize vertex weights
            std::vector<std::vector<std::pair<int, float>>> vertexWeights(mesh->mNumVertices);

            // Process bone weights (unchanged)
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
                aiBone* bone = mesh->mBones[boneIndex];
                int globalBoneIndex = boneMapping[bone->mName.C_Str()];

                for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
                    aiVertexWeight weight = bone->mWeights[weightIndex];
                    vertexWeights[weight.mVertexId].push_back({ globalBoneIndex, weight.mWeight });
                }
            }

            // Process vertices with transformation
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                SkinnedVertex vertex = {};

                // Position - apply full transformation
                float pos[3] = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
                if (settings.applyPreTransform) {
                    TransformVector(transformMatrix, pos, vertex.position);
                }
                else {
                    vertex.position[0] = pos[0];
                    vertex.position[1] = pos[1];
                    vertex.position[2] = pos[2];
                }

                // Normal - apply rotation and scale only
                if (mesh->HasNormals()) {
                    float norm[3] = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
                    if (settings.applyPreTransform) {
                        TransformNormal(transformMatrix, norm, vertex.normal);
                    }
                    else {
                        vertex.normal[0] = norm[0];
                        vertex.normal[1] = norm[1];
                        vertex.normal[2] = norm[2];
                    }
                }

                // Texture coordinates (unchanged)
                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoord[0] = mesh->mTextureCoords[0][i].x;
                    vertex.texCoord[1] = mesh->mTextureCoords[0][i].y;
                }

                // Tangent - apply rotation and scale only
                if (mesh->HasTangentsAndBitangents()) {
                    glm::vec3 tangent(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                    glm::vec3 bitangent(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
                    if (settings.applyPreTransform) {
                        float transformedTangent[3];
                        float transformedBitangent[3];
                        const float tangentIn[3] = { tangent.x, tangent.y, tangent.z };
                        const float bitangentIn[3] = { bitangent.x, bitangent.y, bitangent.z };
                        TransformNormal(transformMatrix, tangentIn, transformedTangent);
                        TransformNormal(transformMatrix, bitangentIn, transformedBitangent);
                        Ermine::StoreTangent(
                            vertex.tangent,
                            Ermine::BuildTangentData(
                                glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]),
                                glm::vec3(transformedTangent[0], transformedTangent[1], transformedTangent[2]),
                                glm::vec3(transformedBitangent[0], transformedBitangent[1], transformedBitangent[2])));
                    }
                    else {
                        Ermine::StoreTangent(
                            vertex.tangent,
                            Ermine::BuildTangentData(
                                glm::vec3(vertex.normal[0], vertex.normal[1], vertex.normal[2]),
                                tangent,
                                bitangent));
                    }
                }
                else {
                    Ermine::StoreTangent(vertex.tangent, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                }

                // Bone weights (unchanged)
                auto& weights = vertexWeights[i];
                std::sort(weights.begin(), weights.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

                for (int j = 0; j < 4; j++) {
                    vertex.boneIndices[j] = 0;
                    vertex.boneWeights[j] = 0.0f;
                }

                float totalWeight = 0.0f;
                int weightCount = std::min(4, (int)weights.size());

                for (int j = 0; j < weightCount; j++) {
                    vertex.boneIndices[j] = weights[j].first;
                    vertex.boneWeights[j] = weights[j].second;
                    totalWeight += weights[j].second;
                }

                if (totalWeight > 0.0f) {
                    for (int j = 0; j < 4; j++) {
                        vertex.boneWeights[j] /= totalWeight;
                    }
                }
                else {
                    vertex.boneIndices[0] = 0;
                    vertex.boneWeights[0] = 1.0f;
                }

                outData.vertices.push_back(vertex);
            }

            // Process indices (unchanged)
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    outData.indices.push_back(baseVertex + face.mIndices[j]);
                }
            }
        }

        if (settings.applyPreTransform) {
            std::cout << "    ✓ Applied pre-transformation to skinned mesh: "
                << "Scale(" << settings.scale[0] << "," << settings.scale[1] << "," << settings.scale[2] << ") "
                << "Rotation(" << settings.rotation[0] << "°," << settings.rotation[1] << "°," << settings.rotation[2] << "°) "
                << "Translation(" << settings.translation[0] << "," << settings.translation[1] << "," << settings.translation[2] << ")"
                << std::endl;
        }

        return !outData.vertices.empty();
    }

    bool ResourcePipeline::WriteMeshFile(const std::string& outputPath, const MeshData& meshData) {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "    Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        // Write header
        file.write("MESH", 4);
        uint32_t version = 2;
        file.write((char*)&version, sizeof(version));

        // Write vertex count and data
        uint32_t vertexCount = (uint32_t)meshData.vertices.size();
        file.write((char*)&vertexCount, sizeof(vertexCount));
        file.write((char*)meshData.vertices.data(), vertexCount * sizeof(Vertex));

        // Write index count and data
        uint32_t indexCount = (uint32_t)meshData.indices.size();
        file.write((char*)&indexCount, sizeof(indexCount));
        file.write((char*)meshData.indices.data(), indexCount * sizeof(uint32_t));

        return true;
    }

    bool ResourcePipeline::WriteSkinFile(const std::string& outputPath, const SkinnedMeshData& meshData) {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "    Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        // Write header
        file.write("SKIN", 4);
        uint32_t version = 2;
        file.write((char*)&version, sizeof(version));

        // Write vertex count and data
        uint32_t vertexCount = (uint32_t)meshData.vertices.size();
        file.write((char*)&vertexCount, sizeof(vertexCount));
        file.write((char*)meshData.vertices.data(), vertexCount * sizeof(SkinnedVertex));

        // Write index count and data
        uint32_t indexCount = (uint32_t)meshData.indices.size();
        file.write((char*)&indexCount, sizeof(indexCount));
        file.write((char*)meshData.indices.data(), indexCount * sizeof(uint32_t));

        // Write bone count
        uint32_t boneCount = (uint32_t)meshData.bones.size();
        file.write((char*)&boneCount, sizeof(boneCount));

        // Write bone data
        for (const auto& bone : meshData.bones) {
            // Write bone name
            uint32_t nameLen = (uint32_t)bone.name.length();
            file.write((char*)&nameLen, sizeof(nameLen));
            file.write(bone.name.c_str(), nameLen);

            // Write bone index
            file.write((char*)&bone.index, sizeof(bone.index));

            // Write offset matrix
            file.write((char*)bone.offsetMatrix, sizeof(bone.offsetMatrix));
        }

        return true;
    }

} // namespace Ermine
