/* Start Header ************************************************************************/
/*!
\file       SkeletalSSBO.cpp
\author     Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\date       27/10/2025
\brief      Implementation of skeletal animation SSBO

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "SkeletalSSBO.h"
#include "SSBO_Bindings.h"
#include "Logger.h"
#include <glad/glad.h>
#include <cstring>
#include <set>

namespace Ermine::graphics
{
    SkeletalSSBO::~SkeletalSSBO()
    {
        if (m_BufferID != 0)
        {
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
        }
    }

    bool SkeletalSSBO::Initialize(size_t maxSkeletons)
    {
        if (m_BufferID != 0)
        {
            EE_CORE_WARN("SkeletalSSBO already initialized");
            return false;
        }

        // Calculate total bones: maxSkeletons * MAX_BONES_PER_SKELETON
        m_MaxBones = maxSkeletons * MAX_BONES_PER_SKELETON;
        m_BufferSize = m_MaxBones * sizeof(glm::mat4); // mat4 = 64 bytes

        // Create buffer
        glGenBuffers(1, &m_BufferID);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_BufferSize, nullptr, GL_DYNAMIC_DRAW);

        // Check for errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            EE_CORE_ERROR("Failed to create buffer for skeletal data, error: {0}", error);
            glDeleteBuffers(1, &m_BufferID);
            m_BufferID = 0;
            return false;
        }

        // Bind to SSBO binding point
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SKELETAL_SSBO_BINDING, m_BufferID);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        EE_CORE_INFO("Initialized skeletal SSBO (glBufferSubData): {0} max skeletons, {1} max bones, {2} bytes, binding {3}",
                     maxSkeletons, m_MaxBones, m_BufferSize, SKELETAL_SSBO_BINDING);

        return true;
    }

    int SkeletalSSBO::AllocateBoneSpace(size_t boneCount)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("SkeletalSSBO not initialized");
            return -1;
        }

        if (boneCount > MAX_BONES_PER_SKELETON)
        {
            EE_CORE_ERROR("Bone count ({0}) exceeds MAX_BONES_PER_SKELETON ({1})",
                          boneCount, MAX_BONES_PER_SKELETON);
            return -1;
        }

        if (m_CurrentBoneCount + boneCount > m_MaxBones)
        {
            EE_CORE_ERROR("Not enough space in skeletal buffer (need {0}, available {1})",
                          boneCount, m_MaxBones - m_CurrentBoneCount);
            return -1;
        }

        // Allocate space and return starting index
        int startIndex = static_cast<int>(m_CurrentBoneCount);
        m_CurrentBoneCount += boneCount;

        return startIndex;
    }

    void SkeletalSSBO::UpdateBoneTransforms(int startBoneIndex, const std::vector<glm::mat4>& boneTransforms)
    {
        if (!IsValid())
        {
            EE_CORE_ERROR("SkeletalSSBO not initialized");
            return;
        }

        if (startBoneIndex < 0 || static_cast<size_t>(startBoneIndex) >= m_MaxBones)
        {
            EE_CORE_ERROR("Invalid bone start index: {0}", startBoneIndex);
            return;
        }

        if (boneTransforms.empty())
        {
            return; // Nothing to update
        }

        // Check bounds
        size_t endIndex = startBoneIndex + boneTransforms.size();
        if (endIndex > m_MaxBones)
        {
            EE_CORE_ERROR("Bone transforms exceed buffer bounds (start: {0}, count: {1}, max: {2})",
                          startBoneIndex, boneTransforms.size(), m_MaxBones);
            return;
        }

        size_t offsetBytes = startBoneIndex * sizeof(glm::mat4);
        size_t sizeBytes = boneTransforms.size() * sizeof(glm::mat4);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BufferID);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, offsetBytes, sizeBytes, boneTransforms.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

} // namespace Ermine::graphics
