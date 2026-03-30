#pragma once

#include <glm/glm.hpp>

namespace Ermine
{
    inline constexpr float kTangentSpaceEpsilon = 1e-12f;

    inline float ComputeTangentHandedness(
        const glm::vec3& normal,
        const glm::vec3& tangent,
        const glm::vec3& bitangent)
    {
        const float normalLenSq = glm::dot(normal, normal);
        const float tangentLenSq = glm::dot(tangent, tangent);
        const float bitangentLenSq = glm::dot(bitangent, bitangent);
        if (normalLenSq <= kTangentSpaceEpsilon ||
            tangentLenSq <= kTangentSpaceEpsilon ||
            bitangentLenSq <= kTangentSpaceEpsilon) {
            return 1.0f;
        }

        const glm::vec3 normalizedNormal = normal * glm::inversesqrt(normalLenSq);
        const glm::vec3 normalizedTangent = tangent * glm::inversesqrt(tangentLenSq);
        const glm::vec3 normalizedBitangent = bitangent * glm::inversesqrt(bitangentLenSq);
        return glm::dot(glm::cross(normalizedNormal, normalizedTangent), normalizedBitangent) < 0.0f ? -1.0f : 1.0f;
    }

    inline glm::vec4 BuildTangentData(
        const glm::vec3& normal,
        const glm::vec3& tangent,
        const glm::vec3& bitangent)
    {
        const float normalLenSq = glm::dot(normal, normal);
        if (normalLenSq <= kTangentSpaceEpsilon) {
            return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        const glm::vec3 orthogonalTangent = tangent - normal * (glm::dot(normal, tangent) / normalLenSq);
        const float orthogonalTangentLenSq = glm::dot(orthogonalTangent, orthogonalTangent);
        if (orthogonalTangentLenSq <= kTangentSpaceEpsilon) {
            return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        const glm::vec3 normalizedTangent = orthogonalTangent * glm::inversesqrt(orthogonalTangentLenSq);
        return glm::vec4(
            normalizedTangent,
            ComputeTangentHandedness(normal, normalizedTangent, bitangent));
    }

    inline void StoreTangent(float packedTangent[4], const glm::vec4& tangentData)
    {
        packedTangent[0] = tangentData.x;
        packedTangent[1] = tangentData.y;
        packedTangent[2] = tangentData.z;
        packedTangent[3] = tangentData.w;
    }
}
