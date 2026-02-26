/* Start Header ************************************************************************/
/*!
\file       SkeletalSSBO.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       27/10/2025
\brief      Skeletal animation SSBO system

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <cstdint> 

namespace Ermine::graphics
{
    // Maximum bones per skeleton
    constexpr size_t MAX_BONES_PER_SKELETON = 128;

    /**
     * @brief Buffer for skeletal animation bone transforms using glBufferSubData
     *
     * This SSBO stores bone transforms for ALL animated entities in a single contiguous buffer.
     * Each entity gets its own offset (allocated via AllocateBoneSpace), allowing multiple
     * instances of the same model to maintain independent skeletal animation states.
     *
     * Architecture:
     * - Global buffer: [Entity1 bones][Entity2 bones][Entity3 bones]...
     * - Each entity allocated bone space: boneOffset = AllocateBoneSpace(boneCount)
     * - Shader accesses: boneTransforms[boneOffset + boneID]
     */
    class SkeletalSSBO
    {
    public:
        SkeletalSSBO() = default;
        ~SkeletalSSBO();

        // Disable copy
        SkeletalSSBO(const SkeletalSSBO&) = delete;
        SkeletalSSBO& operator=(const SkeletalSSBO&) = delete;

        /**
         * @brief Initialize the skeletal SSBO with glBufferSubData
         * @param maxSkeletons Maximum number of concurrent skeletons
         * @return true if successful
         */
        bool Initialize(size_t maxSkeletons);

        /**
         * @brief Allocate bone transform space for an entity
         * @param boneCount Number of bones needed
         * @return Starting bone index, or -1 if allocation failed
         */
        int AllocateBoneSpace(size_t boneCount);

        /**
         * @brief Update bone transforms for an entity using glBufferSubData
         * @param startBoneIndex Starting index from AllocateBoneSpace()
         * @param boneTransforms Vector of bone transform matrices
         */
        void UpdateBoneTransforms(int startBoneIndex, const std::vector<glm::mat4>& boneTransforms);

        /**
         * @brief Get the OpenGL buffer ID
         * @return Buffer ID
         */
        uint32_t GetBufferID() const { return m_BufferID; }

        /**
         * @brief Check if the buffer is initialized
         * @return true if initialized
         */
        bool IsValid() const { return m_BufferID != 0; }

        /**
         * @brief Get total number of bones allocated
         * @return Total bone count
         */
        size_t GetTotalBoneCount() const { return m_CurrentBoneCount; }

    private:
        uint32_t m_BufferID = 0;            // OpenGL buffer object
        size_t m_MaxBones = 0;              // Maximum total bones
        size_t m_CurrentBoneCount = 0;      // Current number of bones allocated
        size_t m_BufferSize = 0;            // Total buffer size in bytes
    };

} // namespace Ermine::graphics
