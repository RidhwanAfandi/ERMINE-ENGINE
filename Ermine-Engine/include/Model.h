/* Start Header ************************************************************************/
/*!
\file       Model.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       27/09/2025
\brief      This file contains the declaration of the Model class for loading and processing
            3D models using Assimp. Provides mesh data, bone data, and animation integration
            for rendering and animation systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace Ermine::graphics
{
    constexpr int MAX_BONE_INFLUENCE = 4;

    /**
     * @brief Per-vertex data including position, normal, UVs, and bone influences.
     *
     * This structure is used for skinned meshes where each vertex can be influenced
     * by up to MAX_BONE_INFLUENCE bones.
     */
    struct VertexData
    {
        float position[3];                  // Position of the vertex
        float normal[3];                    // Normal vector
        float texCoords[2];                 // Texture coordinates
        float tangent[3];                   // Tangent vector for normal mapping
        int IDs[MAX_BONE_INFLUENCE];        // Bone IDs affecting this vertex
        float Weights[MAX_BONE_INFLUENCE];  // Bone weights corresponding to IDs

        /**
         * @brief Default constructor initializes all values to zero.
         */
        VertexData()
        {
            for (int i = 0; i < 3; ++i)
            {
                position[i] = 0.0f;
                normal[i] = 0.0f;
                tangent[i] = 0.0f;
            }

            for (int i = 0; i < 2; ++i)
                texCoords[i] = 0.0f;

            for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
            {
                IDs[i] = 0;
                Weights[i] = 0.0f;
            }
        }

        /**
         * @brief Assigns a bone influence to the vertex.
         * @param boneID The bone index
         * @param weight Influence weight [0,1]
         */
        void AddBoneData(int boneID, float weight)
        {
            for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
            {
                if (Weights[i] == 0.0f)
                {
                    IDs[i] = boneID;
                    Weights[i] = weight;
                    return;
                }
            }
        }
    };

    /**
     * @brief Mesh data wrapper containing GPU buffers and local transform.
     */
    struct MeshData
    {
        std::shared_ptr<VertexArray> vao;  // Vertex array object
        std::shared_ptr<VertexBuffer> vbo; // Vertex buffer object
        std::shared_ptr<IndexBuffer> ibo;  // Index buffer object
        glm::mat4 localTransform{ 1.0f };  // Local transform relative to parent
        std::string meshID;                // Unique mesh identifier (for MeshManager lookup)
        glm::vec3 aabbMin{ 0.0f };        // AABB minimum bounds
        glm::vec3 aabbMax{ 0.0f };        // AABB maximum bounds
        std::vector<glm::vec3> boneAabbMin;   // Per-bone bind-space AABB min
        std::vector<glm::vec3> boneAabbMax;   // Per-bone bind-space AABB max
        std::vector<uint8_t> boneAabbValid;   // Per-bone AABB validity
    };

    /**
     * @brief Represents a 3D model loaded from file via Assimp.
     *
     * The Model class loads meshes, bones, and animations, and provides
     * access to bone mappings and transforms for use in animation systems.
     */
    class Model
    {
    public:
        /**
         * @brief Construct a new Model by loading a file.
         * @param path Path to the 3D model file
         */
        Model(const std::string& path);

        // In Model class, add a new constructor:
        Model(const std::string& path, bool isCacheFile);

        /**
         * @brief Get the directory of the model file.
         * @return Directory as string reference
         */
		const std::string& GetDirectory() const { return m_directory; }
		
        /**
         * @brief Get the name of the model file.
         * @return File name as string reference
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief Get all meshes of the model.
         * @return Vector of MeshData
         */
        const std::vector<MeshData>& GetMeshes() const { return m_meshes; }

        /**
         * @brief Get the raw Assimp scene pointer.
         * @return Pointer to aiScene
         */
        const aiScene* GetAssimpScene() const { return m_Scene; }

        /**
         * @brief Get mapping from bone name to bone index.
         * @return Const reference to bone mapping
         */
        const std::unordered_map<std::string, int>& GetBoneMapping() const { return m_BoneMapping; }

        /**
         * @brief Get the number of bones in the model.
         * @return Number of bones
         */
        int GetBoneCount() const { return static_cast<int>(m_BoneOffsets.size()); }

        /**
         * @brief Get bone offset matrices (bind-pose transforms).
         * @return Const reference to vector of offset matrices
         */
        const std::vector<glm::mat4>& GetBoneOffsets() const { return m_BoneOffsets; }

        /**
         * @brief Get the current animated bone transforms.
         * @return Const reference to vector of bone transforms
         */
        const std::vector<glm::mat4>& GetBoneTransforms() const { return m_BoneTransforms; }

        /**
         * @brief Set the current animated bone transforms.
         * @param transforms Vector of bone transform matrices
         */
        void SetBoneTransforms(const std::vector<glm::mat4>& transforms) { m_BoneTransforms = transforms; }

        /**
         * @brief Load a model from file and process its nodes and meshes.
         * @param path Path to the model file
         */
        void LoadModel(const std::string& path);

        /**
         * @brief Convert an Assimp matrix to a glm::mat4.
         * @param from Input aiMatrix4x4
         * @return Equivalent glm::mat4
         */
        glm::mat4 ToGlm(const aiMatrix4x4& from) {
            glm::mat4 to;
            to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
            to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
            to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
            to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
            return to;
        }

        std::vector<glm::vec3> GetMeshVertices() const;

        std::vector<glm::vec3> GetSkinnedVertices() const;

    private:
        std::string m_directory;                            // Directory of the model
        std::string m_name;                                 // Name of the model
        std::vector<MeshData> m_meshes;                     // All meshes of the model
        uint32_t m_instanceID;                              // Instance ID for this specific file path

        std::unique_ptr<Assimp::Importer> m_Importer;       // Assimp importer
        const aiScene* m_Scene = nullptr;                   // Raw Assimp scene

        // Bone data
        std::unordered_map<std::string, int> m_BoneMapping; // Name-to-index bone map
        std::vector<glm::mat4> m_BoneOffsets;               // Bone offset matrices
        std::vector<glm::mat4> m_BoneTransforms;            // Final bone transforms (for rendering)

        // Per-file instance counters: tracks how many instances of each file have been created
        static std::unordered_map<std::string, std::atomic<uint32_t>> s_fileInstanceCounters;
        static std::mutex s_counterMutex;

        /**
         * @brief Recursively process Assimp nodes into MeshData.
         * @param node Current node
         * @param scene Assimp scene reference
         * @param parentTransform Parent transformation matrix
         */
        void ProcessNode(aiNode* node, const aiScene* scene, const aiMatrix4x4& parentTransform);

        /**
         * @brief Convert an Assimp mesh into engine MeshData.
         * @param mesh Pointer to aiMesh
         * @return Processed MeshData
         */
        MeshData ProcessMesh(aiMesh* mesh);

        bool LoadSkinFile(const std::string& path);
        bool LoadMeshFile(const std::string& path);

    };
}
