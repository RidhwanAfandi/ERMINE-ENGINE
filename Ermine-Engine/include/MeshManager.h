/* Start Header ************************************************************************/
/*!
\file       MeshManager.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/09/2025
\brief      This file contains the declaration of MeshManager for multi-draw indirect rendering.

MeshManager centralizes all mesh data into contiguous GPU buffers to enable efficient
multi-draw indirect rendering. All meshes (static and skinned) are stored in single
large vertex and index buffers, with offsets tracked per mesh.

Usage Pattern (per scene):
1. Call Initialize() once at engine startup
2. Register all meshes using RegisterMesh() or RegisterSkinnedMesh() during scene load
3. Call UploadAndBuild() once after all meshes are registered
4. Access VAOs and buffers via GetStandardVAO(), GetSkinnedVAO(), GetIndirectBuffer() for rendering
5. Call Clear() when loading a new scene, then repeat from step 2

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "MeshTypes.h"
#include "DrawCommands.h"
#include "SkeletalSSBO.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <glad/glad.h>

#include "Entity.h"
#include "Shader.h"

namespace Ermine::graphics {

    /*!***********************************************************************
    \brief
        MeshManager handles centralized mesh storage for multi-draw indirect rendering.

        All meshes are stored in contiguous GPU buffers with per-mesh offsets.
        This enables efficient batched rendering using glMultiDrawElementsIndirect.

    Simplified Workflow:
        Scene Load:
            1. RegisterMesh/RegisterSkinnedMesh for all meshes (stages data in CPU)
            2. UploadAndBuild() once when done (uploads to GPU and builds indirect commands)
            3. Data is cleared from CPU, ready for rendering

        New Scene:
            1. Clear() to reset everything
            2. Repeat scene load workflow
    *************************************************************************/
    class MeshManager {
    public:
        MeshManager();
        ~MeshManager();

        // Mesh registration and lookup
        MeshHandle RegisterMesh(const std::vector<Vertex>& vertices,
            const std::vector<uint32_t>& indices,
            const std::string& meshID);

        MeshHandle RegisterSkinnedMesh(const std::vector<SkinnedVertex>& vertices,
            const std::vector<uint32_t>& indices,
            const std::string& meshID);

        MeshHandle GetMeshHandle(const std::string& meshID) const;
        const MeshSubset* GetMeshData(MeshHandle handle) const;

        // GPU buffer management
        void Initialize(size_t estimatedDrawCount = 0);  // Initialize with optional scene-based sizing
        void UploadAndBuild();  // Upload staged data and build indirect commands (call once per scene)
        void Clear();           // Clear all data for new scene

        // Getters for rendering (SSBOs)
        GLuint GetIndirectBuffer() const { return m_IndirectBuffer.bufferID; }
        const IndirectDrawBuffer& GetIndirectBufferInfo() const { return m_IndirectBuffer; }
        size_t GetMeshCount() const { return m_LoadedMeshes.size(); }

        GLuint GetStandardVAO() const { return m_StandardVAO; }
        GLuint GetSkinnedVAO() const { return m_SkinnedVAO; }

        // Shadow VAOs with pre-skinned position attribute for optimized shadow rendering
        GLuint GetStandardShadowVAO() const { return m_StandardShadowVAO; }
        GLuint GetSkinnedShadowVAO() const { return m_SkinnedShadowVAO; }

        // Check if new meshes have been registered and need uploading
        bool IsDirty() const { return m_IndirectBuffer.isDirty; }
        bool HasStagedData() const { return !m_StagedVertices.empty() || !m_StagedSkinnedVertices.empty(); }

        // Get buffer capacity and usage statistics
        size_t GetBufferCapacity() const { return m_GeometryStandardDrawCommandBuffer.GetMaxCommands(); }
        void LogBufferUtilization() const;

        // Public buffer handles for Renderer access
        GLuint m_DrawCommandsSSBO = 0;    // Binding 2 - Draw commands [DEPRECATED]
        GLuint m_DrawInfoSSBO = 0;        // Binding 3 - Draw info (per-draw data) [DEPRECATED]
        GLuint m_IndexSSBO = 0;           // Binding 1 - All indices
		GLuint GetVertexVBO() const { return m_VertexVBO; }

        // Separate buffers for each pass and VAO type (no shared buffers = no overwrites)
        // Depth prepass buffers (early-z rejection - opaque objects only)
        DrawCommandBuffer m_DepthPrepassStandardDrawCommandBuffer;
        DrawInfoBuffer m_DepthPrepassStandardDrawInfoBuffer;
        DrawCommandBuffer m_DepthPrepassSkinnedDrawCommandBuffer;
        DrawInfoBuffer m_DepthPrepassSkinnedDrawInfoBuffer;

        // Picking pass buffers (object selection - all visible objects)
        DrawCommandBuffer m_PickingStandardDrawCommandBuffer;
        DrawInfoBuffer m_PickingStandardDrawInfoBuffer;
        DrawCommandBuffer m_PickingSkinnedDrawCommandBuffer;
        DrawInfoBuffer m_PickingSkinnedDrawInfoBuffer;

        // Geometry pass buffers
        DrawCommandBuffer m_GeometryStandardDrawCommandBuffer;
        DrawInfoBuffer m_GeometryStandardDrawInfoBuffer;
        DrawCommandBuffer m_GeometrySkinnedDrawCommandBuffer;
        DrawInfoBuffer m_GeometrySkinnedDrawInfoBuffer;

        // Forward pass buffers
        DrawCommandBuffer m_ForwardStandardDrawCommandBuffer;
        DrawInfoBuffer m_ForwardStandardDrawInfoBuffer;
        DrawCommandBuffer m_ForwardSkinnedDrawCommandBuffer;
        DrawInfoBuffer m_ForwardSkinnedDrawInfoBuffer;

        // Shadow pass buffers
        DrawCommandBuffer m_ShadowStandardDrawCommandBuffer;
        DrawInfoBuffer m_ShadowStandardDrawInfoBuffer;
        DrawCommandBuffer m_ShadowSkinnedDrawCommandBuffer;
        DrawInfoBuffer m_ShadowSkinnedDrawInfoBuffer;

		// Outline mask pass buffers
        DrawCommandBuffer m_OutlineStandardDrawCommandBuffer;
        DrawInfoBuffer m_OutlineStandardDrawInfoBuffer;
        DrawCommandBuffer m_OutlineSkinnedDrawCommandBuffer;
        DrawInfoBuffer m_OutlineSkinnedDrawInfoBuffer;

        // Skeletal animation SSBO for bone transforms (Binding 7)
        SkeletalSSBO m_SkeletalSSBO;

		// VBO/VAO system for hardware vertex fetch
        void SetupShadowVAOs();  // Configure shadow VAOs for shadow pass

        // Outline mask pass
        void RenderOutlineMaskIndirect(
            const glm::mat4& view,
            const glm::mat4& projection,
            const std::function<bool(EntityID)>& filterFn,
            Shader& standardShader,
            Shader& skinnedShader);
    private:
        void CreateBuffers();
        void SetupStandardVAO();   // Configure StandardVAO attribute bindings (locations 0-3)
        void SetupSkinnedVAO();    // Configure SkinnedVAO attribute bindings (locations 0-5)

        // Mesh registry
        std::vector<MeshSubset> m_LoadedMeshes;
        std::unordered_map<std::string, MeshHandle> m_MeshCache;

        // OpenGL SSBO buffers (no VAOs needed for SSBO-based rendering)

        GLuint m_VertexVBO = 0;           // VBO for standard vertices (64 bytes each)
        GLuint m_StandardVAO = 0;         // VAO for standard vertices (locations 0-3)
        GLuint m_SkinnedVBO = 0;          // VBO for skinned vertices (96 bytes each)
        GLuint m_SkinnedVAO = 0;          // VAO for skinned vertices (locations 0-5)

        // Shadow VAOs with pre-skinned position attribute (location 6)
        GLuint m_StandardShadowVAO = 0;   // Shadow VAO for standard meshes + pre-skinned attribute
        GLuint m_SkinnedShadowVAO = 0;    // Shadow VAO for skinned meshes + pre-skinned attribute

        IndirectDrawBuffer m_IndirectBuffer;  // Draw commands buffer metadata

        // Staged data (cleared after upload)
        std::vector<Vertex> m_StagedVertices;
        std::vector<SkinnedVertex> m_StagedSkinnedVertices;
        std::vector<uint32_t> m_StagedIndices;
    };

}
