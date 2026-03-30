/* Start Header ************************************************************************/
/*!
\file       Model.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/10/2025
\brief      This file contains the definition of the Model class for loading and processing
            3D models using Assimp. Provides mesh data, bone data, and animation integration
            for rendering and animation systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Model.h"
#include "Renderer.h"
#include "ECS.h"
#include "MeshTypes.h"
#include "TangentSpace.h"

using namespace Ermine::graphics;

// Initialize static per-file instance counters
std::unordered_map<std::string, std::atomic<uint32_t>> Model::s_fileInstanceCounters;
std::mutex Model::s_counterMutex;

/**
 * @brief Construct a new Model by loading a file.
 * @param path Path to the 3D model file
 */
Model::Model(const std::string& path)
{
    // Get or create counter for this file path
    {
        std::lock_guard<std::mutex> lock(s_counterMutex);
        // If this is the first time loading this file, create counter starting at 0
        if (s_fileInstanceCounters.find(path) == s_fileInstanceCounters.end()) {
            s_fileInstanceCounters[path].store(0);
        }
        // Assign instance ID and increment counter for this file
        m_instanceID = s_fileInstanceCounters[path].fetch_add(1);
    }

    LoadModel(path);
}

Model::Model(const std::string& path, bool isCacheFile)
{
    // Get or create counter for this file path
    {
        std::lock_guard<std::mutex> lock(s_counterMutex);
        if (s_fileInstanceCounters.find(path) == s_fileInstanceCounters.end()) {
            s_fileInstanceCounters[path].store(0);
        }
        m_instanceID = s_fileInstanceCounters[path].fetch_add(1);
    }

    if (isCacheFile) {
        // Set directory and name for cache files
        m_directory = path.substr(0, path.find_last_of('/'));
        m_name = path.substr(path.find_last_of('/') + 1);

        // Determine file type by extension
        std::string extension = path.substr(path.find_last_of('.'));

        if (extension == ".skin") {
            if (!LoadSkinFile(path)) {
                EE_CORE_ERROR("Failed to load .skin file: " + path);
            }
        }
        else if (extension == ".mesh") {
            if (!LoadMeshFile(path)) {
                EE_CORE_ERROR("Failed to load .mesh file: " + path);
            }
        }
        else {
            EE_CORE_ERROR("Unknown cache file extension: " + extension);
        }
    }
    else {
        // Load with Assimp for source files (FBX, OBJ, etc.)
        LoadModel(path);
    }
}

bool Model::LoadMeshFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        EE_CORE_ERROR("Failed to open .mesh file: " + path);
        return false;
    }

    // Read and verify magic number
    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "MESH") {
        EE_CORE_ERROR("Invalid .mesh file format (bad magic): " + path);
        return false;
    }

    // Read version
    uint32_t version;
    file.read((char*)&version, sizeof(version));
    if (version != 2) {
        EE_CORE_ERROR("Unsupported .mesh version: " + std::to_string(version) + ". Reimport the mesh cache.");
        return false;
    }

    // Read vertex count
    uint32_t vertexCount;
    file.read((char*)&vertexCount, sizeof(vertexCount));

    // Define the pipeline's Vertex structure (must match ResourcePipeline.cpp)
    struct PipelineVertex {
        float position[3];
        float normal[3];
        float texCoord[2];
        float tangent[4];
    };

    // Read all vertices from file
    std::vector<PipelineVertex> meshVertices(vertexCount);
    file.read((char*)meshVertices.data(), vertexCount * sizeof(PipelineVertex));

    // Convert to engine's VertexData format
    std::vector<VertexData> vertices(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const auto& src = meshVertices[i];
        auto& dst = vertices[i];

        // Copy position, normal, texcoords, tangent
        memcpy(dst.position, src.position, sizeof(float) * 3);
        memcpy(dst.normal, src.normal, sizeof(float) * 3);
        memcpy(dst.texCoords, src.texCoord, sizeof(float) * 2);
        memcpy(dst.tangent, src.tangent, sizeof(float) * 4);

        // Initialize bone data to zero (not used for static meshes)
        for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
            dst.IDs[j] = 0;
            dst.Weights[j] = 0.0f;
        }
    }

    // Read index count
    uint32_t indexCount;
    file.read((char*)&indexCount, sizeof(indexCount));

    // Read indices
    std::vector<uint32_t> indices(indexCount);
    file.read((char*)indices.data(), indexCount * sizeof(uint32_t));

    if (!file) {
        EE_CORE_ERROR("Error reading .mesh file (corrupted?): " + path);
        return false;
    }

    // Create GPU buffers
    auto vao = std::make_shared<VertexArray>();
    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(VertexData));
    auto ibo = std::make_shared<IndexBuffer>(indices.data(), (unsigned int)indices.size());

    vao->Bind();
    vbo->Bind();
    ibo->Bind();

    // Setup vertex attributes (same as ProcessMesh for static meshes)
    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, position));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, normal));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, texCoords));
    vao->LinkAttribute(3, 4, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, tangent));

    vao->Unbind();
    vbo->Unbind();
    ibo->Unbind();

    // Calculate AABB
    glm::vec3 aabbMin(FLT_MAX);
    glm::vec3 aabbMax(-FLT_MAX);
    for (const auto& v : vertices) {
        glm::vec3 pos(v.position[0], v.position[1], v.position[2]);
        aabbMin = glm::min(aabbMin, pos);
        aabbMax = glm::max(aabbMax, pos);
    }

    // Create mesh ID
    std::string meshID = m_name + "_mesh0";

    // ✅ Register with MeshManager (for static meshes)
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Convert to MeshManager's Vertex format (not SkinnedVertex)
        std::vector<Vertex> meshVerts;
        meshVerts.reserve(vertices.size());

        for (const auto& v : vertices) {
            Vertex mv;
            mv.position = glm::vec3(v.position[0], v.position[1], v.position[2]);
            mv.normal = glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
            mv.texCoord = glm::vec2(v.texCoords[0], v.texCoords[1]);
            mv.tangent = glm::vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]);
            meshVerts.push_back(mv);
        }

        // Use RegisterMesh (not RegisterSkinnedMesh) for static meshes
        renderer->m_MeshManager.RegisterMesh(meshVerts, indices, meshID);

        EE_CORE_INFO("LoadMeshFile: Registered static mesh '{}' with MeshManager", meshID);
    }
    else {
        EE_CORE_ERROR("LoadMeshFile: Failed to get Renderer system - mesh will not render!");
    }

    // Add mesh to model with all metadata
    MeshData meshData{ vao, vbo, ibo, glm::mat4(1.0f), meshID, aabbMin, aabbMax, {}, {}, {} };
    m_meshes.push_back(meshData);

    // Success! Log the results
    EE_CORE_INFO("Loaded .mesh file: " + path);
    EE_CORE_INFO("  Vertices: " + std::to_string(vertexCount));
    EE_CORE_INFO("  Indices: " + std::to_string(indexCount));

    return true;
}

bool Model::LoadSkinFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        EE_CORE_ERROR("Failed to open .skin file: " + path);
        return false;
    }

    // Read and verify magic number
    char magic[4];
    file.read(magic, 4);
    if (std::string(magic, 4) != "SKIN") {
        EE_CORE_ERROR("Invalid .skin file format (bad magic): " + path);
        return false;
    }

    // Read version
    uint32_t version;
    file.read((char*)&version, sizeof(version));
    if (version != 2) {
        EE_CORE_ERROR("Unsupported .skin version: " + std::to_string(version) + ". Reimport the skinned mesh cache.");
        return false;
    }

    // Read vertex count
    uint32_t vertexCount;
    file.read((char*)&vertexCount, sizeof(vertexCount));

    // Define the pipeline's SkinnedVertex structure
    struct SkinnedVertex {
        float position[3];
        float normal[3];
        float texCoord[2];
        float tangent[4];
        int boneIndices[4];
        float boneWeights[4];
    };

    // Read all vertices from file
    std::vector<SkinnedVertex> skinVertices(vertexCount);
    file.read((char*)skinVertices.data(), vertexCount * sizeof(SkinnedVertex));

    // Convert to engine's VertexData format
    std::vector<VertexData> vertices(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const auto& src = skinVertices[i];
        auto& dst = vertices[i];

        // Copy position
        memcpy(dst.position, src.position, sizeof(float) * 3);
        memcpy(dst.normal, src.normal, sizeof(float) * 3);
        memcpy(dst.texCoords, src.texCoord, sizeof(float) * 2);
        memcpy(dst.tangent, src.tangent, sizeof(float) * 4);
        memcpy(dst.IDs, src.boneIndices, sizeof(int) * MAX_BONE_INFLUENCE);
        memcpy(dst.Weights, src.boneWeights, sizeof(float) * MAX_BONE_INFLUENCE);
    }

    // Read index count
    uint32_t indexCount;
    file.read((char*)&indexCount, sizeof(indexCount));

    // Read indices
    std::vector<uint32_t> indices(indexCount);
    file.read((char*)indices.data(), indexCount * sizeof(uint32_t));

    // Read bone count
    uint32_t boneCount;
    file.read((char*)&boneCount, sizeof(boneCount));

    // Reserve space for bones
    m_BoneOffsets.reserve(boneCount);
    m_BoneMapping.reserve(boneCount);

    for (uint32_t i = 0; i < boneCount; ++i) {
        uint32_t nameLen;
        file.read((char*)&nameLen, sizeof(nameLen));

        if (nameLen > 256) {
            EE_CORE_ERROR("Invalid bone name length in .skin file: " + std::to_string(nameLen));
            return false;
        }

        std::vector<char> nameBuffer(nameLen);
        file.read(nameBuffer.data(), nameLen);
        std::string boneName(nameBuffer.begin(), nameBuffer.end());

        uint32_t boneIndex;
        file.read((char*)&boneIndex, sizeof(boneIndex));

        float matrix[16];
        file.read((char*)matrix, sizeof(matrix));

        if (!file.good()) {
            EE_CORE_ERROR("Failed to read bone " + std::to_string(i) + " from .skin file");
            return false;
        }

        glm::mat4 offsetMatrix;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                offsetMatrix[col][row] = matrix[row * 4 + col];
            }
        }

        m_BoneMapping[boneName] = static_cast<int>(i);
        m_BoneOffsets.push_back(offsetMatrix);
    }

    if (!file) {
        EE_CORE_ERROR("Error reading .skin file (corrupted?): " + path);
        return false;
    }

    // Create GPU buffers
    auto vao = std::make_shared<VertexArray>();
    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(VertexData));
    auto ibo = std::make_shared<IndexBuffer>(indices.data(), (unsigned int)indices.size());

    vao->Bind();
    vbo->Bind();
    ibo->Bind();

    // Setup vertex attributes
    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, position));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, normal));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, texCoords));
    vao->LinkAttribute(3, 4, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, tangent)); // Tangent xyz + handedness

    // Bone IDs (integer attribute)
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(VertexData), (void*)offsetof(VertexData, IDs));

    // Bone weights (float attribute)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, Weights));

    vao->Unbind();
    vbo->Unbind();
    ibo->Unbind();

    // Calculate AABB
    glm::vec3 aabbMin(FLT_MAX);
    glm::vec3 aabbMax(-FLT_MAX);
    for (const auto& v : vertices) {
        glm::vec3 pos(v.position[0], v.position[1], v.position[2]);
        aabbMin = glm::min(aabbMin, pos);
        aabbMax = glm::max(aabbMax, pos);
    }

    // Create mesh ID
    std::string meshID = m_name + "_mesh0";

    // ✅ CRITICAL FIX: Register with MeshManager
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        // Convert to MeshManager's SkinnedVertex format
        std::vector<graphics::SkinnedVertex> skinnedVertices;
        skinnedVertices.reserve(vertices.size());

        for (const auto& v : vertices) {
            graphics::SkinnedVertex sv;
            sv.position = glm::vec3(v.position[0], v.position[1], v.position[2]);
            sv.normal = glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
            sv.texCoord = glm::vec2(v.texCoords[0], v.texCoords[1]);
            sv.tangent = glm::vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]);
            sv.boneIDs = glm::ivec4(v.IDs[0], v.IDs[1], v.IDs[2], v.IDs[3]);
            sv.boneWeights = glm::vec4(v.Weights[0], v.Weights[1], v.Weights[2], v.Weights[3]);
            skinnedVertices.push_back(sv);
        }

        renderer->m_MeshManager.RegisterSkinnedMesh(skinnedVertices, indices, meshID);

        EE_CORE_INFO("LoadSkinFile: Registered skinned mesh '{}' with MeshManager", meshID);
    }
    else {
        EE_CORE_ERROR("LoadSkinFile: Failed to get Renderer system - mesh will not render!");
    }

    // Add mesh to model with all metadata
    std::vector<glm::vec3> boneAabbMin(boneCount, glm::vec3(FLT_MAX));
    std::vector<glm::vec3> boneAabbMax(boneCount, glm::vec3(-FLT_MAX));
    std::vector<uint8_t> boneAabbValid(boneCount, 0);
    for (const auto& v : vertices) {
        glm::vec3 pos(v.position[0], v.position[1], v.position[2]);
        for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
            if (v.Weights[j] <= 0.0f) {
                continue;
            }
            int boneID = v.IDs[j];
            if (boneID < 0 || boneID >= static_cast<int>(boneCount)) {
                continue;
            }
            boneAabbMin[boneID] = glm::min(boneAabbMin[boneID], pos);
            boneAabbMax[boneID] = glm::max(boneAabbMax[boneID], pos);
            boneAabbValid[boneID] = 1;
        }
    }

    MeshData meshData{ vao, vbo, ibo, glm::mat4(1.0f), meshID, aabbMin, aabbMax,
        std::move(boneAabbMin), std::move(boneAabbMax), std::move(boneAabbValid) };
    m_meshes.push_back(meshData);

    // Initialize bone transforms to identity
    m_BoneTransforms.resize(boneCount, glm::mat4(1.0f));

    // Success! Log the results
    EE_CORE_INFO("Loaded .skin file: " + path);
    EE_CORE_INFO("  Vertices: " + std::to_string(vertexCount));
    EE_CORE_INFO("  Indices: " + std::to_string(indexCount));
    EE_CORE_INFO("  Bones: " + std::to_string(boneCount));

    return true;
}

/**
 * @brief Load a model from file and process its nodes and meshes.
 * @param path Path to the model file
 */
void Model::LoadModel(const std::string& path)
{
    // Reset model data before loading
    m_meshes.clear();
    m_BoneMapping.clear();
    m_BoneOffsets.clear();
    m_BoneTransforms.clear();

    // Create importer owned by the Model instance
    m_Importer = std::make_unique<Assimp::Importer>();

    // Choose the flags required
    unsigned int flags = aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_FlipUVs
        | aiProcess_LimitBoneWeights
        | aiProcess_JoinIdenticalVertices
        | aiProcess_CalcTangentSpace;

    const aiScene* scene = m_Importer->ReadFile(path, flags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::string err = m_Importer->GetErrorString();
        EE_CORE_ERROR("ERROR::ASSIMP:: " + err);
        m_Scene = nullptr;
        return;
    }
    m_Scene = scene;

    m_directory = path.substr(0, path.find_last_of('/'));
	m_name = path.substr(path.find_last_of('/') + 1);

    EE_CORE_INFO("Model::LoadModel - Loading model: {}", m_name);
    EE_CORE_INFO("  Total meshes in scene: {}", scene->mNumMeshes);

    ProcessNode(scene->mRootNode, scene, aiMatrix4x4());

    EE_CORE_INFO("  Processed {} meshes from model", m_meshes.size());

    const size_t boneCount = m_BoneOffsets.size();
    if (boneCount > 0) {
        for (auto& mesh : m_meshes) {
            if (mesh.boneAabbMin.size() < boneCount) {
                mesh.boneAabbMin.resize(boneCount, glm::vec3(FLT_MAX));
                mesh.boneAabbMax.resize(boneCount, glm::vec3(-FLT_MAX));
                mesh.boneAabbValid.resize(boneCount, 0);
            }
        }
    }

    // Init bone transforms to identity
    m_BoneTransforms.resize(boneCount, glm::mat4(1.0f));
}

/**
 * @brief Recursively process Assimp nodes into MeshData.
 * @param node Current node
 * @param scene Assimp scene reference
 * @param parentTransform Parent transformation matrix
 */
void Model::ProcessNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform)
{
    aiMatrix4x4 nodeTransform = parentTransform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshData meshData = ProcessMesh(mesh);
        meshData.localTransform = ToGlm(nodeTransform);
        m_meshes.push_back(meshData);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene, nodeTransform);
}

/**
 * @brief Convert an Assimp mesh into engine MeshData.
 * @param mesh Pointer to aiMesh
 * @return Processed MeshData
 */
MeshData Model::ProcessMesh(aiMesh* mesh)
{
    std::vector<unsigned int> indices;
    size_t vertexCount = mesh->mNumVertices;
    std::vector<VertexData> vertices(vertexCount);

    // Base vertex attributes
    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        VertexData& vertex = vertices[i];

        // Position
        vertex.position[0] = mesh->mVertices[i].x;
        vertex.position[1] = mesh->mVertices[i].y;
        vertex.position[2] = mesh->mVertices[i].z;

        // Normal
        if (mesh->HasNormals())
        {
            vertex.normal[0] = mesh->mNormals[i].x;
            vertex.normal[1] = mesh->mNormals[i].y;
            vertex.normal[2] = mesh->mNormals[i].z;
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0])
        {
            vertex.texCoords[0] = mesh->mTextureCoords[0][i].x;
            vertex.texCoords[1] = mesh->mTextureCoords[0][i].y;
        }

        // Tangents (calculated by Assimp via aiProcess_CalcTangentSpace)
        if (mesh->HasTangentsAndBitangents())
        {
            const glm::vec4 tangentData = Ermine::BuildTangentData(
                glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z),
                glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z),
                glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z));
            Ermine::StoreTangent(vertex.tangent, tangentData);
        }
        else
        {
            Ermine::StoreTangent(vertex.tangent, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }
    }

    // Process bones
    for (unsigned int i = 0; i < mesh->mNumBones; ++i)
    {
        aiBone* ai_bone = mesh->mBones[i];
        std::string boneName(ai_bone->mName.C_Str());

        int boneIndex = 0;
        if (m_BoneMapping.find(boneName) == m_BoneMapping.end())
        {
            boneIndex = static_cast<int>(m_BoneOffsets.size());
            m_BoneMapping[boneName] = boneIndex;
            m_BoneOffsets.push_back(ToGlm(ai_bone->mOffsetMatrix));
        }
        else
            boneIndex = m_BoneMapping[boneName];

        // Add bone weights to vertices
        for (unsigned int w = 0; w < ai_bone->mNumWeights; ++w)
        {
            auto vw = ai_bone->mWeights[w];
            vertices[vw.mVertexId].AddBoneData(boneIndex, vw.mWeight);
        }
    }

    // Normalize weights per vertex
    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        float total = 0.0f;
        for (int j = 0; j < MAX_BONE_INFLUENCE; ++j)
            total += vertices[i].Weights[j];
        if (total > 0.0f)
            for (int j = 0; j < MAX_BONE_INFLUENCE; ++j)
                vertices[i].Weights[j] /= total;
    }

    std::vector<glm::vec3> boneAabbMin(m_BoneOffsets.size(), glm::vec3(FLT_MAX));
    std::vector<glm::vec3> boneAabbMax(m_BoneOffsets.size(), glm::vec3(-FLT_MAX));
    std::vector<uint8_t> boneAabbValid(m_BoneOffsets.size(), 0);
    for (const auto& v : vertices) {
        glm::vec3 pos(v.position[0], v.position[1], v.position[2]);
        for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
            if (v.Weights[j] <= 0.0f) {
                continue;
            }
            int boneID = v.IDs[j];
            if (boneID < 0 || boneID >= static_cast<int>(boneAabbMin.size())) {
                continue;
            }
            boneAabbMin[boneID] = glm::min(boneAabbMin[boneID], pos);
            boneAabbMax[boneID] = glm::max(boneAabbMax[boneID], pos);
            boneAabbValid[boneID] = 1;
        }
    }

    // Build indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            indices.push_back(face.mIndices[j]);
    }

    // Create GPU buffers
    auto vao = std::make_shared<VertexArray>();
    auto vbo = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(VertexData));
    auto ibo = std::make_shared<IndexBuffer>(indices.data(), (unsigned int)indices.size());

    vao->Bind();
    vbo->Bind();
    ibo->Bind();

    // Vertex attributes
    vao->LinkAttribute(0, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, position));
    vao->LinkAttribute(1, 3, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, normal));
    vao->LinkAttribute(2, 2, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, texCoords));
    vao->LinkAttribute(3, 4, GL_FLOAT, sizeof(VertexData), (void*)offsetof(VertexData, tangent));

    // Bone IDs (integer)
    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(VertexData), (void*)offsetof(VertexData, IDs));

    // Bone weights (float)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, Weights));

    vao->Unbind();
    vbo->Unbind();
    ibo->Unbind();

    // Register mesh with MeshManager for indirect rendering
    auto renderer = Ermine::ECS::GetInstance().GetSystem<Renderer>();
    if (renderer) {
        bool hasBones = mesh->HasBones();

        // Create unique mesh ID based on model name, mesh name, and mesh index
        // Use m_meshes.size() as the mesh index to ensure uniqueness even if mesh names are the same/empty
        std::string meshName = std::string(mesh->mName.C_Str());
        std::string meshID = m_name + "_" + meshName + "_mesh" + std::to_string(m_meshes.size());

        // Debug logging
        EE_CORE_INFO("Model::ProcessMesh - Processing mesh: '{}'", meshName.empty() ? "[unnamed]" : meshName);
        EE_CORE_INFO("  Generated meshID: '{}'", meshID);
        EE_CORE_INFO("  Vertex count: {}, Index count: {}", vertices.size(), indices.size());
        EE_CORE_INFO("  Has bones: {}", hasBones==true?"true":"false");

        // Log bone information for this model
        if (hasBones) {
            EE_CORE_INFO("  Total bones in model: {}", m_BoneOffsets.size());
            EE_CORE_INFO("  Bone count for this mesh: {}", mesh->mNumBones);
        }

        if (hasBones) {
            // Register as skinned mesh
            std::vector<SkinnedVertex> skinnedVertices;
            skinnedVertices.reserve(vertices.size());
            for (const auto& v : vertices) {
                SkinnedVertex sv;
                sv.position = glm::vec3(v.position[0], v.position[1], v.position[2]);
                sv.normal = glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
                sv.texCoord = glm::vec2(v.texCoords[0], v.texCoords[1]);
                sv.tangent = glm::vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]);
                sv.boneIDs = glm::ivec4(v.IDs[0], v.IDs[1], v.IDs[2], v.IDs[3]);
                sv.boneWeights = glm::vec4(v.Weights[0], v.Weights[1], v.Weights[2], v.Weights[3]);
                skinnedVertices.push_back(sv);
            }
            renderer->m_MeshManager.RegisterSkinnedMesh(skinnedVertices, indices, meshID);
        } else {
            // Register as regular mesh
            std::vector<Vertex> meshVertices;
            meshVertices.reserve(vertices.size());
            for (const auto& v : vertices) {
                Vertex mv;
                mv.position = glm::vec3(v.position[0], v.position[1], v.position[2]);
                mv.normal = glm::vec3(v.normal[0], v.normal[1], v.normal[2]);
                mv.texCoord = glm::vec2(v.texCoords[0], v.texCoords[1]);
                mv.tangent = glm::vec4(v.tangent[0], v.tangent[1], v.tangent[2], v.tangent[3]);
                meshVertices.push_back(mv);
            }
            renderer->m_MeshManager.RegisterMesh(meshVertices, indices, meshID);
        }
        
        // Calculate AABB from vertex positions
        glm::vec3 aabbMin(FLT_MAX);
        glm::vec3 aabbMax(-FLT_MAX);
        for (const auto& v : vertices) {
            glm::vec3 pos(v.position[0], v.position[1], v.position[2]);
            aabbMin = glm::min(aabbMin, pos);
            aabbMax = glm::max(aabbMax, pos);
        }

        return MeshData{ vao, vbo, ibo, glm::mat4(1.0f), meshID, aabbMin, aabbMax,
            std::move(boneAabbMin), std::move(boneAabbMax), std::move(boneAabbValid) };
    }

    return MeshData{ vao, vbo, ibo, glm::mat4(1.0f), "", glm::vec3(0.0f), glm::vec3(0.0f), {}, {}, {} };

}

std::vector<glm::vec3> Ermine::graphics::Model::GetMeshVertices() const
{
    std::vector<glm::vec3> vertices;

    // Iterate through all meshes
    for (const auto& mesh : m_meshes)
    {
        if (!mesh.vbo) continue;

        const VertexData* vertexData = reinterpret_cast<const VertexData*>(mesh.vbo->GetDataPointer());
        if (!vertexData) continue;

        // Number of vertices
        unsigned int numVertices = mesh.vbo->GetSize() / sizeof(VertexData);

        for (unsigned int i = 0; i < numVertices; ++i)
        {
            vertices.emplace_back(
                vertexData[i].position[0],
                vertexData[i].position[1],
                vertexData[i].position[2]
            );
        }
    }

    return vertices;
}

std::vector<glm::vec3> Model::GetSkinnedVertices() const
{
    std::vector<glm::vec3> vertices;

    for (const auto& mesh : m_meshes)
    {
        if (!mesh.vbo) continue;

        const VertexData* vertexData = reinterpret_cast<const VertexData*>(mesh.vbo->GetDataPointer());
        if (!vertexData) continue;

        unsigned int numVertices = mesh.vbo->GetSize() / sizeof(VertexData);
        vertices.reserve(vertices.size() + numVertices);

        for (unsigned int i = 0; i < numVertices; ++i)
        {
            const VertexData& v = vertexData[i];
            glm::vec4 skinnedPos = glm::vec4(0.0f);

            // Apply bone transforms
            for (int j = 0; j < MAX_BONE_INFLUENCE; ++j)
            {
                int boneID = v.IDs[j];
                float weight = v.Weights[j];

                if (boneID < 0 || boneID >= static_cast<int>(m_BoneTransforms.size()))
                    continue;

                // BoneTransform = globalTransform * offset
                glm::mat4 transform = m_BoneTransforms[boneID];
                skinnedPos += transform * glm::vec4(v.position[0], v.position[1], v.position[2], 1.0f) * weight;
            }

            vertices.emplace_back(skinnedPos.x, skinnedPos.y, skinnedPos.z);
        }
    }

    return vertices;
}

void Ermine::graphics::Model::GetCollisionMesh(std::vector<glm::vec3>& outVerts, std::vector<uint32_t>& outIndices, bool applyLocalTransform) const
{
    outVerts.clear();
    outIndices.clear();

    uint32_t baseVertex = 0;

    for (const auto& mesh : m_meshes)
    {
        if (!mesh.vbo || !mesh.ibo) continue;

        const VertexData* vtx = reinterpret_cast<const VertexData*>(mesh.vbo->GetDataPointer());
        if (!vtx) continue;

        const uint32_t vtxCount = static_cast<uint32_t>(mesh.vbo->GetSize() / sizeof(VertexData));
        if (vtxCount == 0) continue;

        // Append vertices
        outVerts.reserve(outVerts.size() + vtxCount);

        for (uint32_t i = 0; i < vtxCount; ++i)
        {
            glm::vec4 p(vtx[i].position[0], vtx[i].position[1], vtx[i].position[2], 1.0f);

            if (applyLocalTransform)
                p = mesh.localTransform * p;

            outVerts.emplace_back(p.x, p.y, p.z);
        }

        // Append indices with base vertex offset
        const unsigned int* idx = reinterpret_cast<const unsigned int*>(mesh.ibo->GetDataPointer());
        if (!idx) { baseVertex += vtxCount; continue; }

        const uint32_t idxCount = static_cast<uint32_t>(mesh.ibo->GetCount());
        if (idxCount < 3) { baseVertex += vtxCount; continue; }

        outIndices.reserve(outIndices.size() + idxCount);
        for (uint32_t i = 0; i < idxCount; ++i)
            outIndices.push_back(baseVertex + static_cast<uint32_t>(idx[i]));

        baseVertex += vtxCount;
    }
}

