#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>

// Include the xresource_pipeline header
#include "xresource_pipeline.h"
#include <DirectXTex.h>

// Assimp includes
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef _MSC_VER
#pragma warning(disable : 4566)
#endif

using namespace DirectX;
// Define a texture resource type GUID (you can generate this or use a fixed one)
constexpr xresource::type_guid TEXTURE_TYPE_GUID("TEXTURE_RESOURCE_TYPE");
constexpr xresource::type_guid STATIC_MESH_TYPE_GUID("STATIC_MESH_RESOURCE");   
constexpr xresource::type_guid SKINNED_MESH_TYPE_GUID("SKINNED_MESH_RESOURCE");

// Mesh data structures
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

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct BoneInfo {
    std::string name;
    aiMatrix4x4 offsetMatrix;
    int index;
};

struct SkinnedMeshData {
    std::vector<SkinnedVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<BoneInfo> bones;
};

class ErmineResourcePipeline {
private:
    std::string configPath;
    std::string projectPath;
    std::string sourceAssetsPath;
    std::string databasePath;
    std::string projectGuid;

    // Track processed resources
    std::vector<xresource::full_guid> processedResources;

    // Track existing resources by source file path and modification time
    struct ResourceEntry {
        xresource::full_guid guid;
        std::string sourcePath;
        std::filesystem::file_time_type lastModified;
        std::string outputPath;
        std::string resourceType;
    };
    std::vector<ResourceEntry> existingResources;

    // DirectXTex helper methods
    bool InitializeDirectXTex() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr)) {
            std::cout << "Failed to initialize COM" << std::endl;
            return false;
        }
        return true;
    }

    void CleanupDirectXTex() {
        CoUninitialize();
    }

    // Assimp helper methods
    bool LoadAssimpScene(const std::string& filePath, const aiScene*& outScene, Assimp::Importer& importer) {
        // Import flags for optimal mesh processing
        unsigned int importFlags =
            aiProcess_Triangulate |           // Convert to triangles
            aiProcess_GenNormals |            // Generate normals if missing
            aiProcess_CalcTangentSpace |      // Calculate tangents for normal mapping
            aiProcess_JoinIdenticalVertices | // Index optimization
            aiProcess_SortByPType |           // Split by primitive type
            aiProcess_FlipUVs |               // Flip V coordinate (DirectX convention)
            aiProcess_LimitBoneWeights;      // Limit to 4 bones per vertex
            //aiProcess_ConvertToLeftHanded;    // Convert to DirectX coordinate system

        outScene = importer.ReadFile(filePath, importFlags);

        if (!outScene || outScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !outScene->mRootNode) {
            std::cout << "ERROR: Assimp failed to load file: " << importer.GetErrorString() << std::endl;
            return false;
        }

        std::cout << "  ✓ Loaded scene with " << outScene->mNumMeshes << " mesh(es)" << std::endl;
        return true;
    }

    bool HasSkinning(const aiMesh* mesh) {
        return mesh->HasBones();
    }

    bool ProcessStaticMesh(const aiMesh* mesh, MeshData& outData) {
        std::cout << "    Processing static mesh: " << mesh->mName.C_Str() << std::endl;
        std::cout << "    Vertices: " << mesh->mNumVertices << ", Faces: " << mesh->mNumFaces << std::endl;

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex = {};

            // Position
            vertex.position[0] = mesh->mVertices[i].x;
            vertex.position[1] = mesh->mVertices[i].y;
            vertex.position[2] = mesh->mVertices[i].z;

            // Normal
            if (mesh->HasNormals()) {
                vertex.normal[0] = mesh->mNormals[i].x;
                vertex.normal[1] = mesh->mNormals[i].y;
                vertex.normal[2] = mesh->mNormals[i].z;
            }

            // Texture coordinates (use first UV channel)
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord[0] = mesh->mTextureCoords[0][i].x;
                vertex.texCoord[1] = mesh->mTextureCoords[0][i].y;
            }

            // Tangent
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent[0] = mesh->mTangents[i].x;
                vertex.tangent[1] = mesh->mTangents[i].y;
                vertex.tangent[2] = mesh->mTangents[i].z;
            }

            outData.vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                outData.indices.push_back(face.mIndices[j]);
            }
        }

        std::cout << "    ✓ Processed " << outData.vertices.size() << " vertices, "
            << outData.indices.size() << " indices" << std::endl;

        return !outData.vertices.empty();
    }

    bool ProcessSkinnedMesh(const aiScene* scene, const aiMesh* mesh, SkinnedMeshData& outData) {
        std::cout << "    Processing skinned mesh: " << mesh->mName.C_Str() << std::endl;
        std::cout << "    Vertices: " << mesh->mNumVertices << ", Bones: " << mesh->mNumBones << std::endl;

        // Build bone mapping from ALL meshes in the scene (not just this one)
        std::map<std::string, int> boneMapping;

        // First pass: collect all unique bones from all meshes
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            const aiMesh* sceneMesh = scene->mMeshes[meshIdx];
            if (!sceneMesh->HasBones()) continue;

            for (unsigned int i = 0; i < sceneMesh->mNumBones; i++) {
                aiBone* bone = sceneMesh->mBones[i];
                std::string boneName = bone->mName.C_Str();

                // Only add if not already added
                if (boneMapping.find(boneName) == boneMapping.end()) {
                    BoneInfo boneInfo;
                    boneInfo.name = boneName;
                    boneInfo.offsetMatrix = bone->mOffsetMatrix;
                    boneInfo.index = (int)outData.bones.size();

                    outData.bones.push_back(boneInfo);
                    boneMapping[boneName] = boneInfo.index;
                }
            }
        }

        std::cout << "    Total unique bones in skeleton: " << outData.bones.size() << std::endl;

        // Initialize vertices with zero bone weights
        std::vector<std::vector<std::pair<int, float>>> vertexWeights(mesh->mNumVertices);

        // Process bone weights (only for THIS mesh)
        for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
            aiBone* bone = mesh->mBones[boneIndex];
            std::string boneName = bone->mName.C_Str();

            // Get the global bone index from our complete skeleton
            int globalBoneIndex = boneMapping[boneName];

            for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
                aiVertexWeight weight = bone->mWeights[weightIndex];
                unsigned int vertexId = weight.mVertexId;
                float weightValue = weight.mWeight;

                vertexWeights[vertexId].push_back({ globalBoneIndex, weightValue });
            }
        }

        // Process vertices
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            SkinnedVertex vertex = {};

            // Position
            vertex.position[0] = mesh->mVertices[i].x;
            vertex.position[1] = mesh->mVertices[i].y;
            vertex.position[2] = mesh->mVertices[i].z;

            // Normal
            if (mesh->HasNormals()) {
                vertex.normal[0] = mesh->mNormals[i].x;
                vertex.normal[1] = mesh->mNormals[i].y;
                vertex.normal[2] = mesh->mNormals[i].z;
            }

            // Texture coordinates
            if (mesh->HasTextureCoords(0)) {
                vertex.texCoord[0] = mesh->mTextureCoords[0][i].x;
                vertex.texCoord[1] = mesh->mTextureCoords[0][i].y;
            }

            // Tangent
            if (mesh->HasTangentsAndBitangents()) {
                vertex.tangent[0] = mesh->mTangents[i].x;
                vertex.tangent[1] = mesh->mTangents[i].y;
                vertex.tangent[2] = mesh->mTangents[i].z;
            }

            // Bone weights (limit to 4, sorted by weight)
            auto& weights = vertexWeights[i];
            std::sort(weights.begin(), weights.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            // Initialize with defaults
            for (int j = 0; j < 4; j++) {
                vertex.boneIndices[j] = 0;
                vertex.boneWeights[j] = 0.0f;
            }

            // Fill in actual weights (up to 4)
            float totalWeight = 0.0f;
            int weightCount = std::min(4, (int)weights.size());

            for (int j = 0; j < weightCount; j++) {
                vertex.boneIndices[j] = weights[j].first;
                vertex.boneWeights[j] = weights[j].second;
                totalWeight += weights[j].second;
            }

            // Normalize weights to sum to 1.0
            if (totalWeight > 0.0f) {
                for (int j = 0; j < 4; j++) {
                    vertex.boneWeights[j] /= totalWeight;
                }
            }
            else {
                // Vertex has no weights - assign to bone 0
                vertex.boneIndices[0] = 0;
                vertex.boneWeights[0] = 1.0f;
            }

            outData.vertices.push_back(vertex);
        }

        // Process indices
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                outData.indices.push_back(face.mIndices[j]);
            }
        }

        std::cout << "    ✓ Processed " << outData.vertices.size() << " skinned vertices, "
            << outData.indices.size() << " indices, " << outData.bones.size() << " bones" << std::endl;

        return !outData.vertices.empty();
    }

    bool ProcessSkinnedMeshCombined(const aiScene* scene, SkinnedMeshData& outData) {
        std::cout << "    Processing combined skinned meshes" << std::endl;

        // Build bone mapping from ALL meshes in the scene
        std::map<std::string, int> boneMapping;

        // First pass: collect all unique bones from all meshes
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            const aiMesh* sceneMesh = scene->mMeshes[meshIdx];
            if (!sceneMesh->HasBones()) continue;

            for (unsigned int i = 0; i < sceneMesh->mNumBones; i++) {
                aiBone* bone = sceneMesh->mBones[i];
                std::string boneName = bone->mName.C_Str();

                if (boneMapping.find(boneName) == boneMapping.end()) {
                    BoneInfo boneInfo;
                    boneInfo.name = boneName;
                    boneInfo.offsetMatrix = bone->mOffsetMatrix;
                    boneInfo.index = (int)outData.bones.size();

                    outData.bones.push_back(boneInfo);
                    boneMapping[boneName] = boneInfo.index;
                }
            }
        }

        std::cout << "    Total unique bones in skeleton: " << outData.bones.size() << std::endl;

        // Process all meshes
        uint32_t totalVertices = 0;
        uint32_t totalFaces = 0;

        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            const aiMesh* mesh = scene->mMeshes[meshIdx];

            // Skip non-skinned meshes
            if (!mesh->HasBones()) {
                std::cout << "    ⚠ Skipping non-skinned mesh: " << mesh->mName.C_Str() << std::endl;
                continue;
            }

            std::cout << "    Processing mesh " << meshIdx << ": " << mesh->mName.C_Str()
                << " (vertices: " << mesh->mNumVertices << ")" << std::endl;

            uint32_t baseVertex = (uint32_t)outData.vertices.size();

            // Initialize vertex weights for this mesh
            std::vector<std::vector<std::pair<int, float>>> vertexWeights(mesh->mNumVertices);

            // Process bone weights for this mesh
            for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++) {
                aiBone* bone = mesh->mBones[boneIndex];
                std::string boneName = bone->mName.C_Str();
                int globalBoneIndex = boneMapping[boneName];

                for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++) {
                    aiVertexWeight weight = bone->mWeights[weightIndex];
                    unsigned int vertexId = weight.mVertexId;
                    float weightValue = weight.mWeight;

                    vertexWeights[vertexId].push_back({ globalBoneIndex, weightValue });
                }
            }

            // Process vertices
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                SkinnedVertex vertex = {};

                // Position
                vertex.position[0] = mesh->mVertices[i].x;
                vertex.position[1] = mesh->mVertices[i].y;
                vertex.position[2] = mesh->mVertices[i].z;

                // Normal
                if (mesh->HasNormals()) {
                    vertex.normal[0] = mesh->mNormals[i].x;
                    vertex.normal[1] = mesh->mNormals[i].y;
                    vertex.normal[2] = mesh->mNormals[i].z;
                }

                // Texture coordinates
                if (mesh->HasTextureCoords(0)) {
                    vertex.texCoord[0] = mesh->mTextureCoords[0][i].x;
                    vertex.texCoord[1] = mesh->mTextureCoords[0][i].y;
                }

                // Tangent
                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent[0] = mesh->mTangents[i].x;
                    vertex.tangent[1] = mesh->mTangents[i].y;
                    vertex.tangent[2] = mesh->mTangents[i].z;
                }

                // Bone weights (limit to 4, sorted by weight)
                auto& weights = vertexWeights[i];
                std::sort(weights.begin(), weights.end(),
                    [](const auto& a, const auto& b) { return a.second > b.second; });

                // Initialize with defaults
                for (int j = 0; j < 4; j++) {
                    vertex.boneIndices[j] = 0;
                    vertex.boneWeights[j] = 0.0f;
                }

                // Fill in actual weights (up to 4)
                float totalWeight = 0.0f;
                int weightCount = std::min(4, (int)weights.size());

                for (int j = 0; j < weightCount; j++) {
                    vertex.boneIndices[j] = weights[j].first;
                    vertex.boneWeights[j] = weights[j].second;
                    totalWeight += weights[j].second;
                }

                // Normalize weights to sum to 1.0
                if (totalWeight > 0.0f) {
                    for (int j = 0; j < 4; j++) {
                        vertex.boneWeights[j] /= totalWeight;
                    }
                }
                else {
                    // Vertex has no weights - assign to bone 0
                    vertex.boneIndices[0] = 0;
                    vertex.boneWeights[0] = 1.0f;
                }

                outData.vertices.push_back(vertex);
            }

            // Process indices (offset by base vertex)
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    outData.indices.push_back(baseVertex + face.mIndices[j]);
                }
            }

            totalVertices += mesh->mNumVertices;
            totalFaces += mesh->mNumFaces;
        }

        std::cout << "    ✓ Combined " << scene->mNumMeshes << " meshes into one" << std::endl;
        std::cout << "    ✓ Total vertices: " << totalVertices
            << ", indices: " << outData.indices.size()
            << ", bones: " << outData.bones.size() << std::endl;

        return !outData.vertices.empty();
    }

    bool WriteMeshFile(const std::string& outputPath, const MeshData& meshData) {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "    ❌ Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        // Write header
        file.write("MESH", 4);
        uint32_t version = 1;
        file.write((char*)&version, sizeof(version));

        // Write vertex count and data
        uint32_t vertexCount = (uint32_t)meshData.vertices.size();
        file.write((char*)&vertexCount, sizeof(vertexCount));
        file.write((char*)meshData.vertices.data(), vertexCount * sizeof(Vertex));

        // Write index count and data
        uint32_t indexCount = (uint32_t)meshData.indices.size();
        file.write((char*)&indexCount, sizeof(indexCount));
        file.write((char*)meshData.indices.data(), indexCount * sizeof(uint32_t));

        std::cout << "    ✓ Written .mesh file: " << std::filesystem::path(outputPath).filename() << std::endl;
        std::cout << "      Size: " << file.tellp() << " bytes" << std::endl;

        return true;
    }

    bool WriteSkinFile(const std::string& outputPath, const SkinnedMeshData& meshData) {
        std::ofstream file(outputPath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "    ❌ Failed to open output file: " << outputPath << std::endl;
            return false;
        }

        // Write header
        file.write("SKIN", 4);
        uint32_t version = 1;
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

            // Write offset matrix (4x4 float matrix)
            float matrix[16];
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 4; j++) {
                    matrix[i * 4 + j] = bone.offsetMatrix[i][j];
                }
            }
            file.write((char*)matrix, sizeof(matrix));
        }

        std::cout << "    ✓ Written .skin file: " << std::filesystem::path(outputPath).filename() << std::endl;
        std::cout << "      Size: " << file.tellp() << " bytes" << std::endl;

        return true;
    }

    bool ConvertPNGtoDDS(const std::string& inputPath, const std::string& outputPath) {
        using namespace DirectX;

        // Convert paths to wide strings for DirectXTex
        std::wstring wInput(inputPath.begin(), inputPath.end());
        std::wstring wOutput(outputPath.begin(), outputPath.end());

        // Load the PNG
        ScratchImage image;
        HRESULT hr = LoadFromWICFile(wInput.c_str(), WIC_FLAGS_NONE, nullptr, image);
        if (FAILED(hr))
            return false;

        TexMetadata metadata = image.GetMetadata();

        // Flip texture vertically for DirectX/UI compatibility
        //ScratchImage flipped;
        //hr = FlipRotate(image.GetImages(), image.GetImageCount(), metadata,
        //    TEX_FR_FLIP_VERTICAL, flipped);
        //if (FAILED(hr)) {
        //    std::cout << "      ⚠ Failed to flip texture, continuing without flip" << std::endl;
        //    flipped = std::move(image);
        //}
        //else {
        //    std::cout << "      ✓ Flipped texture vertically" << std::endl;
        //    metadata = flipped.GetMetadata();
        //}

        // Convert to engine-supported DXGI format
        // B8G8R8A8_UNORM is safe for your engine
        ScratchImage standardized;
        if (metadata.format != DXGI_FORMAT_B8G8R8A8_UNORM)
        {
            hr = Convert(image.GetImages(), image.GetImageCount(), metadata,
                DXGI_FORMAT_B8G8R8A8_UNORM, TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, standardized);
            if (FAILED(hr))
                return false;
            metadata = standardized.GetMetadata();
        }
        else
        {
            standardized = std::move(image);
        }

        // Generate mipmaps
        ScratchImage mipChain;
        hr = GenerateMipMaps(standardized.GetImages(), standardized.GetImageCount(),
            metadata, TEX_FILTER_DEFAULT, 0, mipChain);
        if (FAILED(hr)) {
            std::cout << "      Warning: mip generation failed (HRESULT: 0x"
                << std::hex << hr << std::dec << "), saving single-mip DDS" << std::endl;
            mipChain = std::move(standardized);
        }
        else {
            std::cout << "      Generated " << mipChain.GetMetadata().mipLevels
                << " mip levels" << std::endl;
        }

        std::string filename = std::filesystem::path(inputPath).filename().string();
        DXGI_FORMAT targetFormat = DetermineOptimalFormat(filename);

        // Compress to target format
        ScratchImage compressed;
        hr = Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(),
            targetFormat, TEX_COMPRESS_DEFAULT, TEX_THRESHOLD_DEFAULT, compressed);

        if (FAILED(hr)) {
            std::cout << "      ⚠ Compression failed (HRESULT: 0x" << std::hex << hr << std::dec
                << "), saving uncompressed" << std::endl;
            // Fallback: save uncompressed
            hr = SaveToDDSFile(mipChain.GetImages(), mipChain.GetImageCount(),
                mipChain.GetMetadata(), DDS_FLAGS_NONE, wOutput.c_str());
        }
        else {
            // Print compression stats
            size_t originalSize = metadata.width * metadata.height * 4; // Rough estimate for RGBA
            size_t compressedSize = compressed.GetPixelsSize();
            float ratio = (float)originalSize / (float)compressedSize;

            std::cout << "      ✓ Compressed: " << std::fixed << std::setprecision(1)
                << ratio << "x reduction" << std::endl;

            // Save compressed DDS
            hr = SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(),
                compressed.GetMetadata(), DDS_FLAGS_NONE, wOutput.c_str());
        }
        //// Optional: Force fully opaque alpha for debug/placeholder textures
        //auto imgs = mipChain.GetImages();
        //for (size_t i = 0; i < mipChain.GetImageCount(); ++i)
        //{
        //    uint32_t* pixels = reinterpret_cast<uint32_t*>(imgs[i].pixels);
        //    for (size_t p = 0; p < imgs[i].width * imgs[i].height; ++p)
        //    {
        //        uint8_t alpha = pixels[p] >> 24;
        //        if (alpha == 0)
        //            pixels[p] |= 0xFF000000;
        //    }
        //}

        return SUCCEEDED(hr);
    }

public:
    ErmineResourcePipeline(const std::string& configFile) : configPath(configFile) {
        LoadConfig();
        InitializeDirectXTex();
    }

    ~ErmineResourcePipeline() {
        CleanupDirectXTex();
    }

    bool LoadConfig() {
        std::cout << "Loading config from: " << configPath << std::endl;

        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cout << "ERROR: Cannot open config file!" << std::endl;
            return false;
        }

        // Get the directory where the config file is located (the project root)
        std::filesystem::path configDir = std::filesystem::path(configPath).parent_path().parent_path();
        std::cout << "Project root (config parent): " << std::filesystem::absolute(configDir) << std::endl;

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == '[') {
                continue;
            }

            if (line.find("SourceAssetsPath=") == 0) {
                sourceAssetsPath = line.substr(17);
                sourceAssetsPath.erase(0, sourceAssetsPath.find_first_not_of(" \t"));
                // Make it absolute relative to project root
                sourceAssetsPath = std::filesystem::absolute(configDir / sourceAssetsPath).string();
            }
            else if (line.find("DatabasePath=") == 0) {
                databasePath = line.substr(13);
                databasePath.erase(0, databasePath.find_first_not_of(" \t"));
                // Make it absolute relative to project root
                databasePath = std::filesystem::absolute(configDir / databasePath).string();
            }
            else if (line.find("ProjectPath=") == 0) {
                projectPath = line.substr(12);
                projectPath.erase(0, projectPath.find_first_not_of(" \t"));
                // Make it absolute relative to project root
                projectPath = std::filesystem::absolute(configDir / projectPath).string();
            }
            else if (line.find("ProjectGUID=") == 0) {
                projectGuid = line.substr(12);
                projectGuid.erase(0, projectGuid.find_first_not_of(" \t"));
                // Remove curly braces if present
                projectGuid.erase(std::remove(projectGuid.begin(), projectGuid.end(), '{'), projectGuid.end());
                projectGuid.erase(std::remove(projectGuid.begin(), projectGuid.end(), '}'), projectGuid.end());
            }
        }

        std::cout << "=== Resolved Paths ===" << std::endl;
        std::cout << "Project GUID: " << projectGuid << std::endl;
        std::cout << "Source Assets Path: " << sourceAssetsPath << std::endl;
        std::cout << "Database Path: " << databasePath << std::endl;
        std::cout << "Project Path: " << projectPath << std::endl;

        // Validate that source assets path exists
        if (!std::filesystem::exists(sourceAssetsPath)) {
            std::cout << "WARNING: Source assets path does not exist: " << sourceAssetsPath << std::endl;
            std::cout << "Please check your config file paths!" << std::endl;
        }

        return true;
    }

    bool LoadExistingResources() {
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::string resourceDbPath = projectFolder + "/Browser.dbase/resource_database.txt";

        if (!std::filesystem::exists(resourceDbPath)) {
            std::cout << "No existing resource database found, starting fresh." << std::endl;
            return true;
        }

        std::ifstream dbFile(resourceDbPath);
        if (!dbFile.is_open()) return false;

        std::string line;
        ResourceEntry currentEntry;
        bool readingEntry = false;

        while (std::getline(dbFile, line)) {
            if (line.find("RESOURCE_START") == 0) {
                readingEntry = true;
                currentEntry = ResourceEntry{};
            }
            else if (line.find("RESOURCE_END") == 0) {
                if (readingEntry) {
                    existingResources.push_back(currentEntry);
                    readingEntry = false;
                }
            }
            else if (readingEntry) {
                if (line.find("InstanceGUID=") == 0) {
                    currentEntry.guid.m_Instance.m_Value = std::stoull(line.substr(13), nullptr, 16);
                }
                else if (line.find("TypeGUID=") == 0) {
                    currentEntry.guid.m_Type.m_Value = std::stoull(line.substr(9), nullptr, 16);
                }
                else if (line.find("SourcePath=") == 0) {
                    currentEntry.sourcePath = line.substr(11);
                }
                else if (line.find("LastModified=") == 0) {
                    auto timeVal = std::stoull(line.substr(13));
                    currentEntry.lastModified = std::filesystem::file_time_type(
                        std::chrono::duration<uint64_t>(timeVal));
                }
                else if (line.find("OutputPath=") == 0) {
                    currentEntry.outputPath = line.substr(11);
                }
                else if (line.find("ResourceType=") == 0) {
                    currentEntry.resourceType = line.substr(13);
                }
            }
        }

        std::cout << "Loaded " << existingResources.size() << " existing resources." << std::endl;
        return true;
    }

    void CleanupUnusedResources() {
        std::cout << "\n=== Cleaning Up Unused Resources ===" << std::endl;

        std::string projectFolder = databasePath + "/" + projectGuid;
        std::string dataFolder = projectFolder + "/Windows.platform/Data";
        std::string browserFolder = projectFolder + "/Browser.dbase";

        size_t removedCount = 0;

        for (auto it = existingResources.begin(); it != existingResources.end();) {

            const ResourceEntry& entry = *it;

            // If source file does NOT exist anymore → it's unused
            if (!std::filesystem::exists(entry.sourcePath)) {
                std::cout << "  🗑 Removing unused resource: "
                    << entry.outputPath << std::endl;

                // Delete generated binary (.dds / .mesh / .skin)
                std::filesystem::path outputPath = dataFolder + "/" +
                    std::filesystem::path(entry.outputPath).filename().string();

                if (std::filesystem::exists(outputPath)) {
                    std::filesystem::remove(outputPath);
                    std::cout << "    Deleted: " << outputPath.filename().string() << std::endl;
                }

                // Delete Browser.dbase metadata
                std::string typeFolder = browserFolder + "/" +
                    std::to_string(entry.guid.m_Type.m_Value);

                std::string infoFile = typeFolder + "/" +
                    std::to_string(entry.guid.m_Instance.m_Value) + "_info.txt";

                if (std::filesystem::exists(infoFile)) {
                    std::filesystem::remove(infoFile);
                    std::cout << "    Deleted metadata: "
                        << std::filesystem::path(infoFile).filename().string() << std::endl;
                }

                // Erase from DB entry list
                it = existingResources.erase(it);
                removedCount++;
            }
            else {
                ++it;
            }
        }

        std::cout << "✓ Cleanup complete. Removed " << removedCount << " unused assets.\n";
    }

    void SaveResourceDatabase() {
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::string resourceDbPath = projectFolder + "/Browser.dbase/resource_database.txt";

        std::ofstream dbFile(resourceDbPath);
        if (!dbFile.is_open()) return;

        for (const auto& entry : existingResources) {
            dbFile << "RESOURCE_START" << std::endl;
            dbFile << "InstanceGUID=" << std::hex << entry.guid.m_Instance.m_Value << std::endl;
            dbFile << "TypeGUID=" << std::hex << entry.guid.m_Type.m_Value << std::endl;
            dbFile << "SourcePath=" << entry.sourcePath << std::endl;
            dbFile << "LastModified=" << std::dec << entry.lastModified.time_since_epoch().count() << std::endl;
            dbFile << "OutputPath=" << entry.outputPath << std::endl;
            dbFile << "ResourceType=" << entry.resourceType << std::endl;
            dbFile << "RESOURCE_END" << std::endl;
        }

        std::cout << "Saved resource database with " << existingResources.size() << " entries." << std::endl;
    }

    ResourceEntry* FindExistingResource(const std::string& sourcePath) {
        auto it = std::find_if(existingResources.begin(), existingResources.end(),
            [&sourcePath](const ResourceEntry& entry) {
                return entry.sourcePath == sourcePath;
            });
        return (it != existingResources.end()) ? &(*it) : nullptr;
    }

    bool IsFileModified(const std::string& filePath, std::filesystem::file_time_type lastKnown) {
        if (!std::filesystem::exists(filePath)) return false;
        auto currentModTime = std::filesystem::last_write_time(filePath);
        return currentModTime > lastKnown;
    }

    void CreateDatabaseStructure() {
        std::cout << "\n=== Creating Database Structure ===" << std::endl;

        // Create main database directory
        std::filesystem::create_directories(databasePath);

        // Create project GUID folder (following the expected structure)
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::filesystem::create_directories(projectFolder);

        // Create Browser.dbase for logging
        std::filesystem::create_directories(projectFolder + "/Browser.dbase");

        // Create platform-specific directories
        std::filesystem::create_directories(projectFolder + "/Windows.platform/Data");

        // Create Generated.dbase for temporary files
        std::filesystem::create_directories(projectFolder + "/Generated.dbase");

        std::cout << "✓ Created database structure at: " << projectFolder << std::endl;
        std::cout << "✓ Platform: Windows.platform/Data" << std::endl;
        std::cout << "✓ Browser: Browser.dbase" << std::endl;
        std::cout << "✓ Generated: Generated.dbase" << std::endl;
    }

    void ScanSourceAssets() {
        std::cout << "\n=== Scanning Source Assets ===" << std::endl;

        if (!std::filesystem::exists(sourceAssetsPath)) {
            std::cout << "❌ Source assets path does not exist: " << sourceAssetsPath << std::endl;
            return;
        }

        LoadExistingResources();

        std::vector<std::string> pngFiles;
        std::vector<std::string> meshFiles;

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceAssetsPath)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();

                    if (extension == ".png" || extension == ".PNG") {
                        pngFiles.push_back(entry.path().string());
                        std::cout << "Found PNG: " << entry.path().filename().string() << std::endl;
                    }
                    else if (extension == ".fbx" || extension == ".FBX" ||
                        extension == ".obj" || extension == ".OBJ" ||
                        extension == ".gltf" || extension == ".glb") {
                        meshFiles.push_back(entry.path().string());
                        std::cout << "Found mesh: " << entry.path().filename().string() << std::endl;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << "Error scanning directory: " << e.what() << std::endl;
            return;
        }

        std::cout << "\nTotal PNG files found: " << pngFiles.size() << std::endl;
        std::cout << "Total mesh files found: " << meshFiles.size() << std::endl;

        ProcessTextures(pngFiles);
        ProcessMeshFiles(meshFiles);

        CleanupUnusedResources();
        SaveResourceDatabase();
    }

    void ProcessTextures(const std::vector<std::string>& assetPaths) {
        if (assetPaths.empty()) return;

        std::cout << "\n=== Processing Textures ===" << std::endl;
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::string outputDir = projectFolder + "/Windows.platform/Data";

        int processed = 0, skipped = 0;

        for (const auto& assetPath : assetPaths) {
            ResourceEntry* existing = FindExistingResource(assetPath);

            if (existing && !IsFileModified(assetPath, existing->lastModified)) {
                std::cout << "  ⏭ Skipping (unchanged): " << std::filesystem::path(assetPath).filename() << std::endl;
                skipped++;
                continue;
            }

            xresource::full_guid resourceGuid;
            if (existing) {
                resourceGuid = existing->guid;
                std::cout << "  🔄 Updating: " << std::filesystem::path(assetPath).filename() << std::endl;
            }
            else {
                auto instanceGuid = xresource::instance_guid::GenerateGUIDCopy();
                resourceGuid = xresource::full_guid{ instanceGuid, TEXTURE_TYPE_GUID };
                std::cout << "  ➕ New: " << std::filesystem::path(assetPath).filename() << std::endl;
            }

            std::string guidStr = std::to_string(resourceGuid.m_Instance.m_Value);
            std::string finalOutput = outputDir + "/" + guidStr + ".dds";

            if (ConvertPNGtoDDS(assetPath, finalOutput)) {
                std::cout << "    ✓ Converted to: " << guidStr << ".dds" << std::endl;

                if (existing) {
                    existing->lastModified = std::filesystem::last_write_time(assetPath);
                }
                else {
                    ResourceEntry newEntry;
                    newEntry.guid = resourceGuid;
                    newEntry.sourcePath = assetPath;
                    newEntry.lastModified = std::filesystem::last_write_time(assetPath);
                    newEntry.outputPath = "Windows.platform/Data/" + guidStr + ".dds";
                    newEntry.resourceType = "TEXTURE";
                    existingResources.push_back(newEntry);
                }
                processed++;
            }
        }

        std::cout << "\n📊 Textures: " << processed << " processed, " << skipped << " skipped" << std::endl;
    }


    void ProcessMeshFiles(const std::vector<std::string>& assetPaths) {
        if (assetPaths.empty()) return;

        std::cout << "\n=== Processing Mesh Files ===" << std::endl;
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::string outputDir = projectFolder + "/Windows.platform/Data";

        int staticProcessed = 0, skinnedProcessed = 0, skipped = 0, failed = 0;

        for (const auto& assetPath : assetPaths) {
            std::cout << "\n  📦 " << std::filesystem::path(assetPath).filename() << std::endl;

            ResourceEntry* existing = FindExistingResource(assetPath);

            if (existing && !IsFileModified(assetPath, existing->lastModified)) {
                std::cout << "    ⏭ Skipping (unchanged)" << std::endl;
                skipped++;
                continue;
            }

            // Load scene with Assimp
            Assimp::Importer importer;
            const aiScene* scene = nullptr;

            if (!LoadAssimpScene(assetPath, scene, importer)) {
                failed++;
                continue;
            }

            // Process first mesh in the scene
            if (scene->mNumMeshes == 0) {
                std::cout << "    ❌ No meshes found in file" << std::endl;
                failed++;
                continue;
            }

            const aiMesh* mesh = scene->mMeshes[0];
            bool isSkinned = HasSkinning(mesh);

            // Determine resource type and GUID
            xresource::full_guid resourceGuid;
            xresource::type_guid typeGuid = isSkinned ? SKINNED_MESH_TYPE_GUID : STATIC_MESH_TYPE_GUID;
            std::string resourceType = isSkinned ? "SKINNED_MESH" : "STATIC_MESH";
            std::string extension = isSkinned ? ".skin" : ".mesh";

            if (existing) {
                resourceGuid = existing->guid;
                std::cout << "    🔄 Updating existing resource" << std::endl;
            }
            else {
                auto instanceGuid = xresource::instance_guid::GenerateGUIDCopy();
                resourceGuid = xresource::full_guid{ instanceGuid, typeGuid };
                std::cout << "    ➕ Creating new resource" << std::endl;
            }

            std::string guidStr = std::to_string(resourceGuid.m_Instance.m_Value);
            std::string finalOutput = outputDir + "/" + guidStr + extension;

            bool success = false;

            if (isSkinned) {
                // Process as skinned mesh (combine all meshes)
                SkinnedMeshData skinnedData;
                if (ProcessSkinnedMeshCombined(scene, skinnedData)) {
                    success = WriteSkinFile(finalOutput, skinnedData);
                    if (success) skinnedProcessed++;
                }
            }
            else {
                // Process as static mesh
                MeshData meshData;
                if (ProcessStaticMesh(mesh, meshData)) {
                    success = WriteMeshFile(finalOutput, meshData);
                    if (success) staticProcessed++;
                }
            }

            if (success) {
                // Update or add resource entry
                if (existing) {
                    existing->lastModified = std::filesystem::last_write_time(assetPath);
                    existing->resourceType = resourceType;
                }
                else {
                    ResourceEntry newEntry;
                    newEntry.guid = resourceGuid;
                    newEntry.sourcePath = assetPath;
                    newEntry.lastModified = std::filesystem::last_write_time(assetPath);
                    newEntry.outputPath = "Windows.platform/Data/" + guidStr + extension;
                    newEntry.resourceType = resourceType;
                    existingResources.push_back(newEntry);
                }

                processedResources.push_back(resourceGuid);
                LogResourceInfo(resourceGuid, assetPath, finalOutput, resourceType);
            }
            else {
                std::cout << "    ❌ Failed to process mesh" << std::endl;
                failed++;
            }
        }

        std::cout << "\n📊 Meshes: " << staticProcessed << " static, "
            << skinnedProcessed << " skinned, "
            << skipped << " skipped, "
            << failed << " failed" << std::endl;
    }

    void LogResourceInfo(const xresource::full_guid& resourceGuid, const std::string& sourcePath,
        const std::string& outputPath, const std::string& resourceType) {
        std::string projectFolder = databasePath + "/" + projectGuid;

        // Log to Browser.dbase with resource type folder structure
        std::string typeGuidStr = std::to_string(resourceGuid.m_Type.m_Value);
        std::string instanceGuidStr = std::to_string(resourceGuid.m_Instance.m_Value);

        std::string browserPath = projectFolder + "/Browser.dbase/" + typeGuidStr;
        std::filesystem::create_directories(browserPath);

        std::string resourceLogPath = browserPath + "/" + instanceGuidStr + "_info.txt";
        std::ofstream logFile(resourceLogPath);
        if (logFile.is_open()) {
            logFile << "Resource Information" << std::endl;
            logFile << "===================" << std::endl;
            logFile << "Instance GUID: 0x" << std::hex << resourceGuid.m_Instance.m_Value << std::dec << std::endl;
            logFile << "Type GUID: 0x" << std::hex << resourceGuid.m_Type.m_Value << std::dec << std::endl;
            logFile << "Resource Type: " << resourceType << std::endl;
            logFile << "Source: " << sourcePath << std::endl;
            logFile << "Output: " << outputPath << std::endl;
            logFile << "Platform: WINDOWS" << std::endl;
            logFile << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
            logFile.close();
        }

        // Also log to main resource log
        std::string mainLogPath = projectFolder + "/Browser.dbase/resource_log.txt";
        std::ofstream mainLog(mainLogPath, std::ios::app);
        if (mainLog.is_open()) {
            mainLog << "Resource: 0x" << std::hex << resourceGuid.m_Instance.m_Value
                << " (Type: 0x" << resourceGuid.m_Type.m_Value << ")" << std::dec << std::endl;
            mainLog << "  Type: " << resourceType << std::endl;
            mainLog << "  Source: " << sourcePath << std::endl;
            mainLog << "  Output: " << outputPath << std::endl;
            mainLog << "  ---" << std::endl;
            mainLog.close();
        }
    }

    void GenerateReport() {
        std::cout << "\n=== Pipeline Report ===" << std::endl;
        std::string projectFolder = databasePath + "/" + projectGuid;
        std::cout << "Project Database: " << projectFolder << std::endl;

        // Count files by type in Windows platform data
        std::string dataPath = projectFolder + "/Windows.platform/Data";
        if (std::filesystem::exists(dataPath)) {
            int textureCount = 0, meshCount = 0, skinCount = 0;
            size_t totalSize = 0;

            for (const auto& entry : std::filesystem::directory_iterator(dataPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    size_t size = std::filesystem::file_size(entry.path());
                    totalSize += size;

                    if (ext == ".dds") textureCount++;
                    else if (ext == ".mesh") meshCount++;
                    else if (ext == ".skin") skinCount++;

                    std::cout << "  " << entry.path().filename().string()
                        << " (" << size << " bytes)" << std::endl;
                }
            }

            std::cout << "\n📊 Total Assets:" << std::endl;
            std::cout << "  Textures: " << textureCount << std::endl;
            std::cout << "  Static Meshes: " << meshCount << std::endl;
            std::cout << "  Skinned Meshes: " << skinCount << std::endl;
            std::cout << "  Total Size: " << totalSize << " bytes ("
                << (totalSize / 1024.0 / 1024.0) << " MB)" << std::endl;
        }

        // Show processed resources with their GUIDs
        std::cout << "\n=== Resource GUIDs ===" << std::endl;
        for (const auto& guid : processedResources) {
            std::string typeName;
            if (guid.m_Type.m_Value == TEXTURE_TYPE_GUID.m_Value) typeName = "TEXTURE";
            else if (guid.m_Type.m_Value == STATIC_MESH_TYPE_GUID.m_Value) typeName = "STATIC_MESH";
            else if (guid.m_Type.m_Value == SKINNED_MESH_TYPE_GUID.m_Value) typeName = "SKINNED_MESH";

            std::cout << "  " << typeName << ": Instance=0x" << std::hex << guid.m_Instance.m_Value
                << ", Type=0x" << guid.m_Type.m_Value << std::dec << std::endl;
        }

        // Show log file if it exists
        std::string logPath = projectFolder + "/Browser.dbase/resource_log.txt";
        if (std::filesystem::exists(logPath)) {
            std::cout << "\n📄 Main resource log: " << logPath << std::endl;
        }

        // Show Browser.dbase structure
        std::string browserPath = projectFolder + "/Browser.dbase";
        if (std::filesystem::exists(browserPath)) {
            std::cout << "\n📁 Resource type folders in Browser.dbase:" << std::endl;
            for (const auto& entry : std::filesystem::directory_iterator(browserPath)) {
                if (entry.is_directory()) {
                    std::cout << "  Type: " << entry.path().filename().string() << std::endl;
                }
            }
        }
    }


    /**
     * @brief Determines the optimal DXGI compression format based on texture filename conventions
     * @param filename The source texture filename (used for heuristic detection)
     * @return Optimal DXGI_FORMAT for compression
     */
    DXGI_FORMAT DetermineOptimalFormat(const std::string& filename) {
        // Convert filename to lowercase for case-insensitive matching
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);

        // Normal maps: Use BC5 (two-channel compression for X and Y, reconstruct Z in shader)
        if (lowerFilename.find("_normal") != std::string::npos ||
            lowerFilename.find("_norm") != std::string::npos ||
            lowerFilename.find("_nrm") != std::string::npos ||
            lowerFilename.find("normal_") != std::string::npos) {
            std::cout << "      Detected: Normal Map → BC5_UNORM" << std::endl;
            return DXGI_FORMAT_BC5_UNORM;
        }

        // Roughness/Metallic/AO maps: Use BC4 (single-channel grayscale compression)
        if (lowerFilename.find("_rough") != std::string::npos ||
            lowerFilename.find("_metallic") != std::string::npos ||
            lowerFilename.find("_metal") != std::string::npos ||
            lowerFilename.find("_ao") != std::string::npos ||
            lowerFilename.find("_occlusion") != std::string::npos ||
            lowerFilename.find("_orm") != std::string::npos ||  // Occlusion-Roughness-Metallic packed
            lowerFilename.find("_mr") != std::string::npos) {
            std::cout << "      Detected: PBR Map (Grayscale) → BC4_UNORM" << std::endl;
            return DXGI_FORMAT_BC4_UNORM;
        }

        // Height/Displacement maps: Also single-channel
        if (lowerFilename.find("_height") != std::string::npos ||
            lowerFilename.find("_disp") != std::string::npos ||
            lowerFilename.find("_displacement") != std::string::npos) {
            std::cout << "      Detected: Height Map → BC4_UNORM" << std::endl;
            return DXGI_FORMAT_BC4_UNORM;
        }

        // Albedo/Color maps with potential alpha: Use BC3 (high-quality color + alpha)
        if (lowerFilename.find("_albedo") != std::string::npos ||
            lowerFilename.find("_diffuse") != std::string::npos ||
            lowerFilename.find("_color") != std::string::npos ||
            lowerFilename.find("_base") != std::string::npos) {
            std::cout << "      Detected: Albedo/Color Map → BC3_UNORM" << std::endl;
            return DXGI_FORMAT_BC3_UNORM;
        }

        // Textures explicitly named as having alpha: Use BC3 to preserve transparency gradient
        if (lowerFilename.find("_alpha") != std::string::npos ||
            lowerFilename.find("alpha_") != std::string::npos) {
            std::cout << "      Detected: Alpha Texture → BC3_UNORM" << std::endl;
            return DXGI_FORMAT_BC3_UNORM;
        }

        // Default: BC1 for simple RGB textures (no alpha)
        // BC1 offers 6:1 compression and is suitable for most color textures
        std::cout << "      Default: Color Texture → BC1_UNORM_SRGB" << std::endl;
        return DXGI_FORMAT_BC1_UNORM_SRGB;
    }
};

int main() {
    std::cout << "=== Ermine Resource Pipeline Runner ===" << std::endl;
    std::cout << "Using Assimp for mesh processing" << std::endl;

    // Get the directory where the exe is located
    std::filesystem::path exePath = std::filesystem::current_path();
    std::cout << "Working directory: " << exePath << std::endl;

    // Look for config in multiple locations
    std::vector<std::string> configPaths = {
        "Ermine-ResourcePipeline/Config/ResourcePipeline.config",  // From solution root
        "./Config/ResourcePipeline.config",  // From project root (if run from there)
        "../../Config/ResourcePipeline.config",  // From x64/Debug
        "../../../Config/ResourcePipeline.config"  // Alternative
    };

    std::string foundConfigPath;
    for (const auto& path : configPaths) {
        std::filesystem::path absolutePath = std::filesystem::absolute(path);
        std::cout << "Checking: " << absolutePath << std::endl;

        if (std::filesystem::exists(absolutePath)) {
            foundConfigPath = absolutePath.string();
            std::cout << "✓ Found config at: " << foundConfigPath << std::endl;
            break;
        }
    }

    if (foundConfigPath.empty()) {
        std::cout << "ERROR: Could not find ResourcePipeline.config in any expected location!" << std::endl;
        std::cout << "Please ensure Config/ResourcePipeline.config exists." << std::endl;
        return 1;
    }

    ErmineResourcePipeline pipeline(foundConfigPath);

    pipeline.CreateDatabaseStructure();
    pipeline.ScanSourceAssets();
    pipeline.GenerateReport();

    std::cout << "\n Pipeline execution complete!" << std::endl;

    return 0;
}
