/* Start Header ************************************************************************/
/*!
\file       MeshManager.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/09/2025
\brief      Implementation of MeshManager for multi-draw indirect rendering.

Example Usage:
    // At engine startup
    meshManager.Initialize();

    // During scene load
    meshManager.RegisterMesh(cubeVerts, cubeIndices, "cube");
    meshManager.RegisterMesh(sphereVerts, sphereIndices, "sphere");
    meshManager.UploadAndBuild();

    // During rendering
    glBindVertexArray(meshManager.GetVertexVAO());
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, meshManager.GetIndirectBuffer());
    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                                meshManager.GetMeshCount(), 0);

    // When loading new scene
    meshManager.Clear();  // Start fresh for new scene

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "MeshManager.h"
#include "SSBO_Bindings.h"

namespace Ermine::graphics {

    namespace {
        bool IsPersistentPrimitiveMeshID(const std::string& meshID)
        {
            return meshID.rfind("Cube_", 0) == 0 ||
                   meshID.rfind("Sphere_", 0) == 0 ||
                   meshID.rfind("Quad_", 0) == 0 ||
                   meshID.rfind("Cone_", 0) == 0;
        }
    }

    MeshManager::MeshManager()
    {
        // Default initialization - member variables are already initialized to 0
        // due to their default member initializers in the header
    }

    MeshManager::~MeshManager()
    {
        if (m_IndexSSBO != 0) {
            glDeleteBuffers(1, &m_IndexSSBO);
            m_IndexSSBO = 0;
        }
        if (m_DrawCommandsSSBO != 0) {
            glDeleteBuffers(1, &m_DrawCommandsSSBO);
            m_DrawCommandsSSBO = 0;
        }
        if (m_DrawInfoSSBO != 0) {
            glDeleteBuffers(1, &m_DrawInfoSSBO);
            m_DrawInfoSSBO = 0;
        }
        if (m_IndirectBuffer.bufferID != 0) {
            glDeleteBuffers(1, &m_IndirectBuffer.bufferID);
            m_IndirectBuffer.bufferID = 0;
        }

        // Cleanup VBO and VAO resources
        if (m_VertexVBO != 0) {
            glDeleteBuffers(1, &m_VertexVBO);
            m_VertexVBO = 0;
        }
        if (m_StandardVAO != 0) {
            glDeleteVertexArrays(1, &m_StandardVAO);
            m_StandardVAO = 0;
        }
        if (m_SkinnedVBO != 0) {
            glDeleteBuffers(1, &m_SkinnedVBO);
            m_SkinnedVBO = 0;
        }
        if (m_SkinnedVAO != 0) {
            glDeleteVertexArrays(1, &m_SkinnedVAO);
            m_SkinnedVAO = 0;
        }
    }

    void MeshManager::Initialize(size_t estimatedDrawCount)
    {
        // Create GPU buffers (SSBOs)
        CreateBuffers();

        // Setup VAOs with attribute bindings
        SetupStandardVAO();
        SetupSkinnedVAO();

        // Calculate buffer size based on scene complexity
        size_t bufferSize;
        if (estimatedDrawCount > 0) {
            // Scene-based sizing: Add 50% headroom for dynamic objects
            bufferSize = static_cast<size_t>(estimatedDrawCount * 1.5f);

            // Clamp to reasonable range
            bufferSize = std::max(bufferSize, size_t(1000));    // Min 1k draws
            bufferSize = std::min(bufferSize, size_t(100000));  // Max 100k draws

            EE_CORE_INFO("MeshManager: Scene-based buffer sizing - Estimated {} draws, allocating for {} draws",
                         estimatedDrawCount, bufferSize);
        } else {
            // No estimate provided, use default maximum
            bufferSize = 100000;
            EE_CORE_INFO("MeshManager: Using default buffer sizing - {} draws", bufferSize);
        }

        // Initialize separate buffers for each pass and VAO type
        // Each pass gets its own buffers to avoid overwrites (simpler, cleaner code)

        // Depth prepass buffers (early-z rejection - opaque objects only)
        if (!m_DepthPrepassStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_DepthPrepassStandardDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize depth prepass standard buffers");
        }
        if (!m_DepthPrepassSkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_DepthPrepassSkinnedDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize depth prepass skinned buffers");
        }

        // Picking pass buffers (object selection - all visible objects)
        if (!m_PickingStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_PickingStandardDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize picking standard buffers");
        }
        if (!m_PickingSkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_PickingSkinnedDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize picking skinned buffers");
        }

        // Geometry pass buffers
        if (!m_GeometryStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_GeometryStandardDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize geometry standard buffers");
        }
        if (!m_GeometrySkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_GeometrySkinnedDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize geometry skinned buffers");
        }

        // Forward pass buffers
        if (!m_ForwardStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_ForwardStandardDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize forward standard buffers");
        }
        if (!m_ForwardSkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_ForwardSkinnedDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize forward skinned buffers");
        }

        // Shadow pass buffers
        if (!m_ShadowStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_ShadowStandardDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize shadow standard buffers");
        }
        if (!m_ShadowSkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_ShadowSkinnedDrawInfoBuffer.Initialize(bufferSize)) {
            EE_CORE_ERROR("Failed to initialize shadow skinned buffers");
        }

        // Outline pass buffers
        if (!m_OutlineStandardDrawCommandBuffer.Initialize(bufferSize) ||
            !m_OutlineStandardDrawInfoBuffer.Initialize(bufferSize))
            EE_CORE_ERROR("Failed to initialize outline standard buffers");

        if (!m_OutlineSkinnedDrawCommandBuffer.Initialize(bufferSize) ||
            !m_OutlineSkinnedDrawInfoBuffer.Initialize(bufferSize))
            EE_CORE_ERROR("Failed to initialize outline skinned buffers");

        // Initialize skeletal SSBO (max 100 skeletons = 100 * 128 bones = 12800 bones)
        constexpr size_t MAX_SKELETONS = 10;
        if (!m_SkeletalSSBO.Initialize(MAX_SKELETONS))
        {
            EE_CORE_ERROR("Failed to initialize skeletal SSBO");
        }

        EE_CORE_INFO("MeshManager: Initialized");
    }

    void MeshManager::LogBufferUtilization() const
    {
        size_t capacity = m_GeometryStandardDrawCommandBuffer.GetMaxCommands();

        // Calculate total memory allocated
        size_t totalMemoryMB = (capacity * sizeof(DrawInfo) * 6 + capacity * sizeof(DrawElementsIndirectCommand) * 6) / (1024 * 1024);

        // Get current usage from each buffer
        size_t geomStandardCount = m_GeometryStandardDrawCommandBuffer.GetCommandCount();
        size_t geomSkinnedCount = m_GeometrySkinnedDrawCommandBuffer.GetCommandCount();
        size_t forwardStandardCount = m_ForwardStandardDrawCommandBuffer.GetCommandCount();
        size_t forwardSkinnedCount = m_ForwardSkinnedDrawCommandBuffer.GetCommandCount();
        size_t shadowStandardCount = m_ShadowStandardDrawCommandBuffer.GetCommandCount();
        size_t shadowSkinnedCount = m_ShadowSkinnedDrawCommandBuffer.GetCommandCount();

        size_t totalUsed = geomStandardCount + geomSkinnedCount + forwardStandardCount +
                          forwardSkinnedCount + shadowStandardCount + shadowSkinnedCount;
        size_t totalCapacity = capacity * 6;

        float utilizationPercent = (totalCapacity > 0) ? (totalUsed * 100.0f / totalCapacity) : 0.0f;

        EE_CORE_INFO("=== MeshManager Buffer Utilization ===");
        EE_CORE_INFO("  Buffer Capacity: {} draws per buffer ({} total)", capacity, totalCapacity);
        EE_CORE_INFO("  Total Memory Allocated: {} MB", totalMemoryMB);
        EE_CORE_INFO("  Current Usage:");
        EE_CORE_INFO("    Geometry Standard:  {}/{} ({:.1f}%)", geomStandardCount, capacity,
                     capacity > 0 ? (geomStandardCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("    Geometry Skinned:   {}/{} ({:.1f}%)", geomSkinnedCount, capacity,
                     capacity > 0 ? (geomSkinnedCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("    Forward Standard:   {}/{} ({:.1f}%)", forwardStandardCount, capacity,
                     capacity > 0 ? (forwardStandardCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("    Forward Skinned:    {}/{} ({:.1f}%)", forwardSkinnedCount, capacity,
                     capacity > 0 ? (forwardSkinnedCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("    Shadow Standard:    {}/{} ({:.1f}%)", shadowStandardCount, capacity,
                     capacity > 0 ? (shadowStandardCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("    Shadow Skinned:     {}/{} ({:.1f}%)", shadowSkinnedCount, capacity,
                     capacity > 0 ? (shadowSkinnedCount * 100.0f / capacity) : 0.0f);
        EE_CORE_INFO("  Total Utilization: {}/{} ({:.1f}%)", totalUsed, totalCapacity, utilizationPercent);

        if (utilizationPercent < 10.0f) {
            EE_CORE_WARN("  WARNING: Buffer utilization is very low (<10%%). Consider using scene-based sizing to reduce memory waste.");
            EE_CORE_WARN("           Estimated optimal capacity: ~{} draws per buffer",
                        static_cast<size_t>((totalUsed / 6) * 1.5f));
        }
    }

    void MeshManager::CreateBuffers()
    {
        // Clean up existing buffers if reinitializing
        if (m_VertexVBO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Vertex VBO before reinitialization");
            glDeleteBuffers(1, &m_VertexVBO);
            m_VertexVBO = 0;
        }
        if (m_SkinnedVBO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Skinned VBO before reinitialization");
            glDeleteBuffers(1, &m_SkinnedVBO);
            m_SkinnedVBO = 0;
        }
        if (m_IndexSSBO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Index SSBO before reinitialization");
            glDeleteBuffers(1, &m_IndexSSBO);
            m_IndexSSBO = 0;
        }
        if (m_DrawCommandsSSBO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Draw Commands SSBO before reinitialization");
            glDeleteBuffers(1, &m_DrawCommandsSSBO);
            m_DrawCommandsSSBO = 0;
        }
        if (m_DrawInfoSSBO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Draw Info SSBO before reinitialization");
            glDeleteBuffers(1, &m_DrawInfoSSBO);
            m_DrawInfoSSBO = 0;
        }
        if (m_IndirectBuffer.bufferID != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Indirect Buffer before reinitialization");
            glDeleteBuffers(1, &m_IndirectBuffer.bufferID);
            m_IndirectBuffer.bufferID = 0;
        }

        // Create Vertex VBO for standard vertices (64 bytes each)
        glGenBuffers(1, &m_VertexVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexVBO);
        // Allocate empty buffer - will be filled in UploadAndBuild()
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        EE_CORE_INFO("MeshManager: Created Vertex VBO (standard vertices)");

        // Create Skinned VBO for skinned vertices (96 bytes each)
        glGenBuffers(1, &m_SkinnedVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_SkinnedVBO);
        // Allocate empty buffer - will be filled in UploadAndBuild()
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        EE_CORE_INFO("MeshManager: Created Skinned VBO (skinned vertices)");

        // Create Index SSBO (Binding 1)
        glGenBuffers(1, &m_IndexSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_IndexSSBO);
        // Allocate empty buffer - will be filled in UploadAndBuild()
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, INDEX_SSBO_BINDING, m_IndexSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        EE_CORE_INFO("MeshManager: Created Index SSBO at binding {}", INDEX_SSBO_BINDING);

        // Create Draw Commands SSBO (Binding 2)
        glGenBuffers(1, &m_DrawCommandsSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawCommandsSSBO);
        // Allocate empty buffer - will be filled in BuildIndirectCommands()
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_COMMANDS_SSBO_BINDING, m_DrawCommandsSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        EE_CORE_INFO("MeshManager: Created Draw Commands SSBO at binding {}", DRAW_COMMANDS_SSBO_BINDING);

        // Create Draw Info SSBO (Binding 3)
        glGenBuffers(1, &m_DrawInfoSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DrawInfoSSBO);
        // Allocate empty buffer - will be filled in BuildIndirectCommands()
        glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_DrawInfoSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        EE_CORE_INFO("MeshManager: Created Draw Info SSBO at binding {}", DRAW_INFO_SSBO_BINDING);
    }

    void MeshManager::SetupStandardVAO()
    {
        // Clean up existing VAO if reinitializing
        if (m_StandardVAO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Standard VAO before reinitialization");
            glDeleteVertexArrays(1, &m_StandardVAO);
            m_StandardVAO = 0;
        }

        // Create Standard VAO
        glGenVertexArrays(1, &m_StandardVAO);
        glBindVertexArray(m_StandardVAO);

        // Bind vertex buffer (VBO)
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexVBO);

        // Bind index buffer (still using SSBO for indices)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexSSBO);

        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, position));

        // Normal attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, normal));

        // TexCoord attribute (location 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoord));

        // Tangent attribute (location 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, tangent));

        EE_CORE_INFO("MeshManager: Configured Standard VAO (locations 0-3)");

        // Unbind array buffer to avoid VAO 0 corruption
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Unbind VAO (element buffer will be bound later during UploadAndBuild)
        glBindVertexArray(0);
    }

    void MeshManager::SetupSkinnedVAO()
    {
        // Clean up existing VAO if reinitializing
        if (m_SkinnedVAO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Skinned VAO before reinitialization");
            glDeleteVertexArrays(1, &m_SkinnedVAO);
            m_SkinnedVAO = 0;
        }

        // Create Skinned VAO
        glGenVertexArrays(1, &m_SkinnedVAO);
        glBindVertexArray(m_SkinnedVAO);

        // Bind skinned vertex buffer (VBO)
        glBindBuffer(GL_ARRAY_BUFFER, m_SkinnedVBO);

        // Bind index buffer (still using SSBO for indices)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexSSBO);

        // Position attribute (location 0) - offset 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, position));

        // Normal attribute (location 1) - offset 16
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, normal));

        // TexCoord attribute (location 2) - offset 32
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, texCoord));

        // Tangent attribute (location 3) - offset 48
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, tangent));

        // BoneIDs attribute (location 4) - offset 64 (ivec4)
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 4, GL_INT, sizeof(SkinnedVertex),
                              (void*)offsetof(SkinnedVertex, boneIDs));

        // BoneWeights attribute (location 5) - offset 80 (vec4)
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, boneWeights));

        EE_CORE_INFO("MeshManager: Configured Skinned VAO (locations 0-5)");

        // Unbind array buffer to avoid VAO 0 corruption
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Unbind VAO (element buffer will be bound later during UploadAndBuild)
        glBindVertexArray(0);
    }

    MeshHandle MeshManager::RegisterMesh(const std::vector<Vertex>& vertices,
                                         const std::vector<uint32_t>& indices,
                                         const std::string& meshID)
    {
        // Check if mesh already exists
        auto it = m_MeshCache.find(meshID);
        if (it != m_MeshCache.end()) {
            EE_CORE_WARN("MeshManager: Mesh '{}' already exists in cache (handle index: {}), returning cached handle",
                         meshID, it->second.index);
            return it->second;
        }

        // Create new mesh subset
        MeshSubset subset;
        subset.meshID = meshID;
        subset.vertexOffset = static_cast<uint32_t>(m_StagedVertices.size());
		subset.vertexCount = static_cast<uint32_t>(vertices.size());
        subset.indexOffset = static_cast<uint32_t>(m_StagedIndices.size());
        subset.indexCount = static_cast<uint32_t>(indices.size());
        subset.baseVertex = subset.vertexOffset;

        // Add to staged data
        m_StagedVertices.insert(m_StagedVertices.end(), vertices.begin(), vertices.end());
        m_StagedIndices.insert(m_StagedIndices.end(), indices.begin(), indices.end());

        // Register mesh
        MeshHandle handle;
        handle.index = static_cast<uint32_t>(m_LoadedMeshes.size());
        m_LoadedMeshes.push_back(subset);
        m_MeshCache[meshID] = handle;

        // Mark indirect buffer as dirty so it will be uploaded
        m_IndirectBuffer.MarkDirty();

		EE_CORE_INFO("MeshManager: Registered NEW mesh '{}' (handle index: {}, vertexOffset: {}, vertices: {}, indexOffset: {}, indices: {})",
                     meshID, handle.index, subset.vertexOffset, vertices.size(), subset.indexOffset, indices.size());

        return handle;
    }

    MeshHandle MeshManager::RegisterSkinnedMesh(const std::vector<SkinnedVertex>& vertices,
                                                const std::vector<uint32_t>& indices,
                                                const std::string& meshID)
    {
        // Check if mesh already exists
        auto it = m_MeshCache.find(meshID);
        if (it != m_MeshCache.end()) {
            EE_CORE_WARN("MeshManager: Skinned mesh '{}' already exists in cache (handle index: {}), returning cached handle",
                         meshID, it->second.index);
            return it->second;
        }

        // Create new mesh subset
        MeshSubset subset;
        subset.meshID = meshID;
        subset.vertexOffset = static_cast<uint32_t>(m_StagedSkinnedVertices.size());
        subset.vertexCount = static_cast<uint32_t>(vertices.size());
        subset.indexOffset = static_cast<uint32_t>(m_StagedIndices.size());
        subset.indexCount = static_cast<uint32_t>(indices.size());
        subset.baseVertex = subset.vertexOffset;

        // Add to staged data
        m_StagedSkinnedVertices.insert(m_StagedSkinnedVertices.end(), vertices.begin(), vertices.end());
        m_StagedIndices.insert(m_StagedIndices.end(), indices.begin(), indices.end());

        // Register mesh
        MeshHandle handle;
        handle.index = static_cast<uint32_t>(m_LoadedMeshes.size());
        m_LoadedMeshes.push_back(subset);
        m_MeshCache[meshID] = handle;

        // Mark indirect buffer as dirty so it will be uploaded
        m_IndirectBuffer.MarkDirty();

        EE_CORE_INFO("MeshManager: Registered NEW skinned mesh '{}' (handle index: {}, vertexOffset: {}, vertices: {}, indexOffset: {}, indices: {})",
                     meshID, handle.index, subset.vertexOffset, vertices.size(), subset.indexOffset, indices.size());

        return handle;
    }

    MeshHandle MeshManager::GetMeshHandle(const std::string& meshID) const
    {
        auto it = m_MeshCache.find(meshID);
        if (it != m_MeshCache.end()) {
            return it->second;
        }
        return MeshHandle(); // Returns invalid handle
    }

    const MeshSubset* MeshManager::GetMeshData(MeshHandle handle) const
    {
        if (!handle.isValid() || handle.index >= m_LoadedMeshes.size()) {
            return nullptr;
        }
        return &m_LoadedMeshes[handle.index];
    }

    void MeshManager::UploadAndBuild()
    {
        EE_CORE_INFO("MeshManager::UploadAndBuild() called - Staged: {} regular vertices, {} skinned vertices, {} indices",
                     m_StagedVertices.size(), m_StagedSkinnedVertices.size(), m_StagedIndices.size());

        glBindVertexArray(0);

        // Upload Vertex SSBO data
        if (!m_StagedVertices.empty()) {
            size_t vertexBufferSize = m_StagedVertices.size() * sizeof(Vertex);
            glBindBuffer(GL_ARRAY_BUFFER, m_VertexVBO);
            glBufferData(GL_ARRAY_BUFFER,
                        vertexBufferSize,
                        m_StagedVertices.data(),
                        GL_STATIC_DRAW);

            // DIAGNOSTIC: Verify VBO has data
            GLint vboSize = 0;
            glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vboSize);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            EE_CORE_INFO("MeshManager: Uploaded {} vertices ({} bytes) to Vertex VBO (ID: {})",
                         m_StagedVertices.size(), vertexBufferSize, m_VertexVBO);
            EE_CORE_INFO("MeshManager: VBO size verified: {} bytes", vboSize);
        }

        // Upload Index SSBO data
        if (!m_StagedIndices.empty()) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_IndexSSBO);
            size_t indexBufferSize = m_StagedIndices.size() * sizeof(uint32_t);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                        indexBufferSize,
                        m_StagedIndices.data(),
                        GL_STATIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            EE_CORE_INFO("MeshManager: Uploaded {} indices ({} bytes) to Index SSBO",
                         m_StagedIndices.size(), indexBufferSize);
        }

        // Upload Skinned Vertex SSBO data
        if (!m_StagedSkinnedVertices.empty()) {
            size_t skinnedVertexBufferSize = m_StagedSkinnedVertices.size() * sizeof(SkinnedVertex);

            // Also upload to Skinned VBO
            glBindBuffer(GL_ARRAY_BUFFER, m_SkinnedVBO);
            glBufferData(GL_ARRAY_BUFFER,
                        skinnedVertexBufferSize,
                        m_StagedSkinnedVertices.data(),
                        GL_STATIC_DRAW);

            // Verify SkinnedVBO has data
            GLint skinnedVboSize = 0;
            glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &skinnedVboSize);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            EE_CORE_INFO("MeshManager: Uploaded {} skinned vertices ({} bytes) to Skinned VBO (ID: {})",
                         m_StagedSkinnedVertices.size(), skinnedVertexBufferSize, m_SkinnedVBO);
            EE_CORE_INFO("MeshManager: Skinned VBO size verified: {} bytes", skinnedVboSize);
        }

		// Clear staged data to free CPU memory (need to reregister meshes for new scene)
        //m_StagedVertices.clear();
        //m_StagedVertices.shrink_to_fit();
        //m_StagedSkinnedVertices.clear();
        //m_StagedSkinnedVertices.shrink_to_fit();
        //m_StagedIndices.clear();
        //m_StagedIndices.shrink_to_fit();

        // Clear dirty flag
        m_IndirectBuffer.MarkClean();


        EE_CORE_INFO("MeshManager: Upload complete. {} meshes, {} vertices ({} regular + {} skinned), {} indices",
            m_LoadedMeshes.size(),
            m_StagedVertices.size() + m_StagedSkinnedVertices.size(),
            m_StagedVertices.size(),
            m_StagedSkinnedVertices.size(),
            m_StagedIndices.size());
    }

    void MeshManager::Clear(bool preservePrimitiveMeshes)
    {
        if (preservePrimitiveMeshes) {
            std::vector<MeshSubset> preservedMeshes;
            preservedMeshes.reserve(m_LoadedMeshes.size());

            std::unordered_map<std::string, MeshHandle> preservedCache;
            preservedCache.reserve(m_MeshCache.size());

            for (const auto& subset : m_LoadedMeshes) {
                if (!IsPersistentPrimitiveMeshID(subset.meshID)) {
                    continue;
                }

                MeshHandle handle;
                handle.index = static_cast<uint32_t>(preservedMeshes.size());
                preservedMeshes.push_back(subset);
                preservedCache[subset.meshID] = handle;
            }

            m_LoadedMeshes = std::move(preservedMeshes);
            m_MeshCache = std::move(preservedCache);

            // Keep staged vertex/index data intact so preserved mesh offsets
            // remain valid without forcing a re-upload on scene/play reload.
            m_IndirectBuffer.commandCount = 0;
            m_IndirectBuffer.bufferSize = 0;
            m_IndirectBuffer.MarkClean();

            EE_CORE_INFO("MeshManager: Cleared scene state, preserved {} primitive meshes",
                         m_LoadedMeshes.size());
            return;
        }

        // Clear all CPU-side data
        m_LoadedMeshes.clear();
        m_MeshCache.clear();
        m_StagedVertices.clear();
        m_StagedSkinnedVertices.clear();
        m_StagedIndices.clear();

        // Free memory
        m_LoadedMeshes.shrink_to_fit();
        m_StagedVertices.shrink_to_fit();
        m_StagedSkinnedVertices.shrink_to_fit();
        m_StagedIndices.shrink_to_fit();

        // Reset indirect buffer metadata
        m_IndirectBuffer.commandCount = 0;
        m_IndirectBuffer.bufferSize = 0;
        m_IndirectBuffer.MarkClean();
    }

    void MeshManager::SetupShadowVAOs()
    {
        // Clean up existing shadow VAOs if reinitializing
        if (m_StandardShadowVAO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Standard Shadow VAO before reinitialization");
            glDeleteVertexArrays(1, &m_StandardShadowVAO);
            m_StandardShadowVAO = 0;
        }
        if (m_SkinnedShadowVAO != 0) {
            EE_CORE_INFO("MeshManager: Cleaning up existing Skinned Shadow VAO before reinitialization");
            glDeleteVertexArrays(1, &m_SkinnedShadowVAO);
            m_SkinnedShadowVAO = 0;
        }

        // ==================== STANDARD SHADOW VAO ====================
        // Create shadow VAO for standard (non-skinned) meshes
        glGenVertexArrays(1, &m_StandardShadowVAO);
        glBindVertexArray(m_StandardShadowVAO);

        // Bind standard vertex buffer for attributes 0-3
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexVBO);

        // Bind index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexSSBO);

        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, position));

        // Normal attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, normal));

        // TexCoord attribute (location 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, texCoord));

        // Tangent attribute (location 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                             (void*)offsetof(Vertex, tangent));

        EE_CORE_INFO("MeshManager: Configured Standard Shadow VAO (locations 0-3)");

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // ==================== SKINNED SHADOW VAO ====================
        // Create shadow VAO for skinned meshes
        glGenVertexArrays(1, &m_SkinnedShadowVAO);
        glBindVertexArray(m_SkinnedShadowVAO);

        // Bind skinned vertex buffer for attributes 0-5
        glBindBuffer(GL_ARRAY_BUFFER, m_SkinnedVBO);

        // Bind index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexSSBO);

        // Position attribute (location 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, position));

        // Normal attribute (location 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, normal));

        // TexCoord attribute (location 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, texCoord));

        // Tangent attribute (location 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, tangent));

        // BoneIDs attribute (location 4)
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 4, GL_INT, sizeof(SkinnedVertex),
                              (void*)offsetof(SkinnedVertex, boneIDs));

        // BoneWeights attribute (location 5)
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex),
                             (void*)offsetof(SkinnedVertex, boneWeights));

        EE_CORE_INFO("MeshManager: Configured Skinned Shadow VAO (locations 0-5)");

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    static void ReadBackBuffers(
        const DrawCommandBuffer& cmdBuffer,
        const DrawInfoBuffer& infoBuffer,
        std::vector<DrawElementsIndirectCommand>& outCmds,
        std::vector<DrawInfo>& outInfos)
    {
        outCmds.clear();
        outInfos.clear();

        const auto cmdCount = cmdBuffer.GetCommandCount();
        const auto infoCount = infoBuffer.GetDrawCount();
        if (cmdCount == 0 || infoCount == 0)
            return;

        const size_t n = std::min(cmdCount, infoCount);

        outCmds.resize(n);
        outInfos.resize(n);

        glBindBuffer(GL_COPY_READ_BUFFER, cmdBuffer.GetBufferID());
        glGetBufferSubData(
            GL_COPY_READ_BUFFER,
            0,
            static_cast<GLsizeiptr>(n * sizeof(DrawElementsIndirectCommand)),
            outCmds.data());

        glBindBuffer(GL_COPY_READ_BUFFER, infoBuffer.GetBufferID());
        glGetBufferSubData(
            GL_COPY_READ_BUFFER,
            0,
            static_cast<GLsizeiptr>(n * sizeof(DrawInfo)),
            outInfos.data());

        glBindBuffer(GL_COPY_READ_BUFFER, 0);
    }

    void MeshManager::RenderOutlineMaskIndirect(const glm::mat4& view, const glm::mat4& projection,
	    const std::function<bool(EntityID)>& filterFn, Shader& standardShader, Shader& skinnedShader)
    {
        // Outline uses the same DrawInfo layout the shaders expect.
                // We re-filter from picking buffers (contains all visible objects).
                // NOTE: This uses glGetBufferSubData (readback). If you want the fast path,
                // we should instead filter from the CPU vectors at CompileDrawData-time.

                // ---------- Standard ----------
        std::vector<DrawElementsIndirectCommand> srcStdCmds;
        std::vector<DrawInfo> srcStdInfos;
        ReadBackBuffers(m_PickingStandardDrawCommandBuffer, m_PickingStandardDrawInfoBuffer, srcStdCmds, srcStdInfos);

        std::vector<DrawElementsIndirectCommand> outStdCmds;
        std::vector<DrawInfo> outStdInfos;
        outStdCmds.reserve(srcStdCmds.size());
        outStdInfos.reserve(srcStdInfos.size());

        for (size_t i = 0; i < srcStdInfos.size(); ++i)
        {
            const EntityID id = static_cast<EntityID>(srcStdInfos[i].entityID);
            if (!filterFn || filterFn(id))
            {
                outStdCmds.push_back(srcStdCmds[i]);
                outStdInfos.push_back(srcStdInfos[i]);
            }
        }

        if (!outStdCmds.empty())
        {
            m_OutlineStandardDrawCommandBuffer.WriteCommands(outStdCmds);
            m_OutlineStandardDrawInfoBuffer.WriteDrawInfos(outStdInfos);

            standardShader.Bind();
            standardShader.SetUniformMatrix4fv("view", view);
            standardShader.SetUniformMatrix4fv("projection", projection);
            standardShader.SetUniform1ui("baseDrawID", 0);

            glBindVertexArray(m_StandardVAO);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_OutlineStandardDrawInfoBuffer.GetBufferID());
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_OutlineStandardDrawCommandBuffer.GetBufferID());

            glMultiDrawElementsIndirect(
                GL_TRIANGLES,
                GL_UNSIGNED_INT,
                nullptr,
                static_cast<GLsizei>(outStdCmds.size()),
                0);

            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            glBindVertexArray(0);

            standardShader.Unbind();
        }

        // ---------- Skinned ----------
        std::vector<DrawElementsIndirectCommand> srcSkinCmds;
        std::vector<DrawInfo> srcSkinInfos;
        ReadBackBuffers(m_PickingSkinnedDrawCommandBuffer, m_PickingSkinnedDrawInfoBuffer, srcSkinCmds, srcSkinInfos);

        std::vector<DrawElementsIndirectCommand> outSkinCmds;
        std::vector<DrawInfo> outSkinInfos;
        outSkinCmds.reserve(srcSkinCmds.size());
        outSkinInfos.reserve(srcSkinInfos.size());

        for (size_t i = 0; i < srcSkinInfos.size(); ++i)
        {
            const EntityID id = static_cast<EntityID>(srcSkinInfos[i].entityID);
            if (!filterFn || filterFn(id))
            {
                outSkinCmds.push_back(srcSkinCmds[i]);
                outSkinInfos.push_back(srcSkinInfos[i]);
            }
        }

        if (!outSkinCmds.empty())
        {
            m_OutlineSkinnedDrawCommandBuffer.WriteCommands(outSkinCmds);
            m_OutlineSkinnedDrawInfoBuffer.WriteDrawInfos(outSkinInfos);

            skinnedShader.Bind();
            skinnedShader.SetUniformMatrix4fv("view", view);
            skinnedShader.SetUniformMatrix4fv("projection", projection);
            skinnedShader.SetUniform1ui("baseDrawID", 0);

            // Bind skeleton SSBO too (the shader reads boneTransforms)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_SkeletalSSBO.GetBufferID());

            glBindVertexArray(m_SkinnedVAO);

            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_OutlineSkinnedDrawInfoBuffer.GetBufferID());
            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_OutlineSkinnedDrawCommandBuffer.GetBufferID());

            glMultiDrawElementsIndirect(
                GL_TRIANGLES,
                GL_UNSIGNED_INT,
                nullptr,
                static_cast<GLsizei>(outSkinCmds.size()),
                0);

            glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            glBindVertexArray(0);

            skinnedShader.Unbind();
        }
    }
} // namespace Ermine::graphics
