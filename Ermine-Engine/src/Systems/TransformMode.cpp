/* Start Header ************************************************************************/
/*!
\file       TransformMode.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       Jan 25, 2025
\brief      Implementation of transform mode helpers for gizmo positioning

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "TransformMode.h"
#include "Components.h"
#include "ECS.h"
#include "HierarchySystem.h"
#include "Model.h"

namespace Ermine
{
    /**
     * @brief Calculate the position where the gizmo should appear
     * @param entity The entity to manipulate
     * @param mode Current transform mode
     * @return Position for gizmo placement
     */
    Vec3 TransformModeHelper::GetManipulationPosition(EntityID entity, TransformMode mode)
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity)) return Vec3();

        auto hierarchySystem = ecs.GetSystem<HierarchySystem>();

        if (mode == TransformMode::Pivot) {
            // STANDARD BEHAVIOR: Return the selected entity's world position (pivot point)
            Vec3 pivotPos = hierarchySystem->GetWorldPosition(entity);
            
            // DEBUG: Log pivot position
            //EE_CORE_INFO("=== PIVOT MODE ===");
            //EE_CORE_INFO("Entity {}: Pivot Position = ({:.3f}, {:.3f}, {:.3f})",
            //    entity, pivotPos.x, pivotPos.y, pivotPos.z);
            
            return pivotPos;
        }
        else { // Center mode
            // STANDARD BEHAVIOR: Return the selected entity's geometric center
            Vec3 centerPos = CalculateGeometricCenter(entity);
            
            // DEBUG: Log center position
            //EE_CORE_INFO("=== CENTER MODE ===");
            //EE_CORE_INFO("Entity {}: Center Position = ({:.3f}, {:.3f}, {:.3f})",
            //    entity, centerPos.x, centerPos.y, centerPos.z);

            return centerPos;
        }
    }

    /**
     * @brief Calculate the geometric center of an entity's mesh/model
     * @param entity The entity to calculate center for
     * @return Bounding box center in world space
     */
    Vec3 TransformModeHelper::CalculateGeometricCenter(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity)) return Vec3();

        // Get the world transforms for THIS entity only (not parent or children)
        auto hierarchySystem = ecs.GetSystem<HierarchySystem>();
        Vec3 worldPos = hierarchySystem->GetWorldPosition(entity);
        Quaternion worldRot = hierarchySystem->GetWorldRotation(entity);
        Vec3 worldScale = hierarchySystem->GetWorldScale(entity);

        // Try to calculate geometric center from mesh/model bounds
        Vec3 minBounds(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
        Vec3 maxBounds(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
        bool hasBounds = false;

        // Helper to transform a local vertex to world space
        auto TransformToWorld = [&](const Vec3& localVertex) -> Vec3 {
            // Apply scale
            Vec3 scaled = Vec3(
                localVertex.x * worldScale.x,
                localVertex.y * worldScale.y,
                localVertex.z * worldScale.z
            );

            // Apply rotation using quaternion
            // q * v * q^-1 formula for rotating a vector by quaternion
            float qx = worldRot.x, qy = worldRot.y, qz = worldRot.z, qw = worldRot.w;
            
            // Normalize quaternion
            float len = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
            if (len > 0.0001f) {
                qx /= len; qy /= len; qz /= len; qw /= len;
            }

            // t = 2 * cross(q.xyz, v)
            float tx = 2.0f * (qy * scaled.z - qz * scaled.y);
            float ty = 2.0f * (qz * scaled.x - qx * scaled.z);
            float tz = 2.0f * (qx * scaled.y - qy * scaled.x);

            // v' = v + q.w * t + cross(q.xyz, t)
            Vec3 rotated;
            rotated.x = scaled.x + qw * tx + (qy * tz - qz * ty);
            rotated.y = scaled.y + qw * ty + (qz * tx - qx * tz);
            rotated.z = scaled.z + qw * tz + (qx * ty - qy * tx);

            // Apply translation
            return Vec3(
                rotated.x + worldPos.x,
                rotated.y + worldPos.y,
                rotated.z + worldPos.z
            );
        };

        // Check for Mesh component (primitives)
        if (ecs.HasComponent<Mesh>(entity))
        {
            const auto& mesh = ecs.GetComponent<Mesh>(entity);
            if (mesh.kind == MeshKind::Primitive)
            {
                // For primitives, transform all 8 corners of the bounding box to world space
                Vec3 halfSize = mesh.primitive.size * 0.5f;
                
                // Define 8 corners of the local bounding box
                Vec3 corners[8] = {
                    Vec3(-halfSize.x, -halfSize.y, -halfSize.z),
                    Vec3( halfSize.x, -halfSize.y, -halfSize.z),
                    Vec3(-halfSize.x,  halfSize.y, -halfSize.z),
                    Vec3( halfSize.x,  halfSize.y, -halfSize.z),
                    Vec3(-halfSize.x, -halfSize.y,  halfSize.z),
                    Vec3( halfSize.x, -halfSize.y,  halfSize.z),
                    Vec3(-halfSize.x,  halfSize.y,  halfSize.z),
                    Vec3( halfSize.x,  halfSize.y,  halfSize.z)
                };

                // Transform each corner to world space and find bounds
                for (int i = 0; i < 8; ++i)
                {
                    Vec3 worldCorner = TransformToWorld(corners[i]);
                    
                    minBounds.x = std::min(minBounds.x, worldCorner.x);
                    minBounds.y = std::min(minBounds.y, worldCorner.y);
                    minBounds.z = std::min(minBounds.z, worldCorner.z);
                    
                    maxBounds.x = std::max(maxBounds.x, worldCorner.x);
                    maxBounds.y = std::max(maxBounds.y, worldCorner.y);
                    maxBounds.z = std::max(maxBounds.z, worldCorner.z);
                }
                hasBounds = true;
            }
        }

        // Check for ModelComponent (imported meshes)
        if (!hasBounds && ecs.HasComponent<ModelComponent>(entity))
        {
            const auto& modelComp = ecs.GetComponent<ModelComponent>(entity);
            if (modelComp.m_model)
            {
                // Get model vertices and calculate bounds
                auto vertices = modelComp.m_model->GetMeshVertices();
                if (!vertices.empty())
                {
                    for (const auto& vertex : vertices)
                    {
                        // Transform vertex from local to world space
                        Vec3 localVertex(vertex.x, vertex.y, vertex.z);
                        Vec3 worldVertex = TransformToWorld(localVertex);
                        
                        minBounds.x = std::min(minBounds.x, worldVertex.x);
                        minBounds.y = std::min(minBounds.y, worldVertex.y);
                        minBounds.z = std::min(minBounds.z, worldVertex.z);
                        
                        maxBounds.x = std::max(maxBounds.x, worldVertex.x);
                        maxBounds.y = std::max(maxBounds.y, worldVertex.y);
                        maxBounds.z = std::max(maxBounds.z, worldVertex.z);
                    }
                    hasBounds = true;
                }
            }
        }

        // Return bounding box center if we have bounds, otherwise fallback to pivot
        if (hasBounds) {
            Vec3 center = Vec3(
                (minBounds.x + maxBounds.x) * 0.5f,
                (minBounds.y + maxBounds.y) * 0.5f,
                (minBounds.z + maxBounds.z) * 0.5f
            );
            
            // DEBUG: Print bounds to console
            //EE_CORE_INFO("Entity {}: Min({:.3f}, {:.3f}, {:.3f}) Max({:.3f}, {:.3f}, {:.3f}) Center({:.3f}, {:.3f}, {:.3f})",
            //    entity,
            //    minBounds.x, minBounds.y, minBounds.z,
            //    maxBounds.x, maxBounds.y, maxBounds.z,
            //    center.x, center.y, center.z);
            
            return center;
        }

        return worldPos; // Fallback to pivot if no mesh data
    }

    /**
     * @brief Calculate center position for multiple selected entities
     * @param entities List of selected entities
     * @param mode Current transform mode
     * @return Average position based on mode
     */
    Vec3 TransformModeHelper::GetMultiSelectionCenter(const std::vector<EntityID>& entities, TransformMode mode)
    {
        if (entities.empty()) return Vec3();
        if (entities.size() == 1) return GetManipulationPosition(entities[0], mode);

        // STANDARD BEHAVIOR: Calculate combined bounding box or average pivot
        if (mode == TransformMode::Pivot)
        {
            // Pivot mode: Use the LAST selected entity's pivot (Unity behavior)
            // NOTE: This requires tracking selection order, which we don't have yet.
            // For now, just use the first entity in the list
            //EE_CORE_INFO("=== MULTI-SELECTION PIVOT MODE ===");
            //EE_CORE_INFO("Using first entity's pivot (Unity uses last selected)");
            return GetManipulationPosition(entities[0], mode);
        }
        else
        {
            // Center mode: Calculate combined bounding box of ALL selected entities
            Vec3 minBounds(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            Vec3 maxBounds(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
            bool hasBounds = false;

            auto& ecs = ECS::GetInstance();
            auto hierarchySystem = ecs.GetSystem<HierarchySystem>();

            for (auto entityId : entities)
            {
                if (!ecs.IsEntityValid(entityId))
                    continue;

                Vec3 worldPos = hierarchySystem->GetWorldPosition(entityId);
                Quaternion worldRot = hierarchySystem->GetWorldRotation(entityId);
                Vec3 worldScale = hierarchySystem->GetWorldScale(entityId);

                // Helper to transform a local vertex to world space
                auto TransformToWorld = [&](const Vec3& localVertex) -> Vec3 {
                    Vec3 scaled = Vec3(
                        localVertex.x * worldScale.x,
                        localVertex.y * worldScale.y,
                        localVertex.z * worldScale.z
                    );

                    float qx = worldRot.x, qy = worldRot.y, qz = worldRot.z, qw = worldRot.w;
                    float len = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
                    if (len > 0.0001f) {
                        qx /= len; qy /= len; qz /= len; qw /= len;
                    }

                    float tx = 2.0f * (qy * scaled.z - qz * scaled.y);
                    float ty = 2.0f * (qz * scaled.x - qx * scaled.z);
                    float tz = 2.0f * (qx * scaled.y - qy * scaled.x);

                    Vec3 rotated;
                    rotated.x = scaled.x + qw * tx + (qy * tz - qz * ty);
                    rotated.y = scaled.y + qw * ty + (qz * tx - qx * tz);
                    rotated.z = scaled.z + qw * tz + (qx * ty - qy * tx);

                    return Vec3(
                        rotated.x + worldPos.x,
                        rotated.y + worldPos.y,
                        rotated.z + worldPos.z
                    );
                };

                // Check for Mesh component
                if (ecs.HasComponent<Mesh>(entityId))
                {
                    const auto& mesh = ecs.GetComponent<Mesh>(entityId);
                    if (mesh.kind == MeshKind::Primitive)
                    {
                        Vec3 halfSize = mesh.primitive.size * 0.5f;
                        Vec3 corners[8] = {
                            Vec3(-halfSize.x, -halfSize.y, -halfSize.z),
                            Vec3( halfSize.x, -halfSize.y, -halfSize.z),
                            Vec3(-halfSize.x,  halfSize.y, -halfSize.z),
                            Vec3( halfSize.x,  halfSize.y, -halfSize.z),
                            Vec3(-halfSize.x, -halfSize.y,  halfSize.z),
                            Vec3( halfSize.x, -halfSize.y,  halfSize.z),
                            Vec3(-halfSize.x,  halfSize.y,  halfSize.z),
                            Vec3( halfSize.x,  halfSize.y,  halfSize.z)
                        };

                        for (int i = 0; i < 8; ++i)
                        {
                            Vec3 worldCorner = TransformToWorld(corners[i]);
                            minBounds.x = std::min(minBounds.x, worldCorner.x);
                            minBounds.y = std::min(minBounds.y, worldCorner.y);
                            minBounds.z = std::min(minBounds.z, worldCorner.z);
                            maxBounds.x = std::max(maxBounds.x, worldCorner.x);
                            maxBounds.y = std::max(maxBounds.y, worldCorner.y);
                            maxBounds.z = std::max(maxBounds.z, worldCorner.z);
                        }
                        hasBounds = true;
                    }
                }

                // Check for ModelComponent
                if (ecs.HasComponent<ModelComponent>(entityId))
                {
                    const auto& modelComp = ecs.GetComponent<ModelComponent>(entityId);
                    if (modelComp.m_model)
                    {
                        auto vertices = modelComp.m_model->GetMeshVertices();
                        if (!vertices.empty())
                        {
                            for (const auto& vertex : vertices)
                            {
                                Vec3 localVertex(vertex.x, vertex.y, vertex.z);
                                Vec3 worldVertex = TransformToWorld(localVertex);
                                minBounds.x = std::min(minBounds.x, worldVertex.x);
                                minBounds.y = std::min(minBounds.y, worldVertex.y);
                                minBounds.z = std::min(minBounds.z, worldVertex.z);
                                maxBounds.x = std::max(maxBounds.x, worldVertex.x);
                                maxBounds.y = std::max(maxBounds.y, worldVertex.y);
                                maxBounds.z = std::max(maxBounds.z, worldVertex.z);
                            }
                            hasBounds = true;
                        }
                    }
                }
            }

            if (hasBounds)
            {
                Vec3 center = Vec3(
                    (minBounds.x + maxBounds.x) * 0.5f,
                    (minBounds.y + maxBounds.y) * 0.5f,
                    (minBounds.z + maxBounds.z) * 0.5f
                );

               // EE_CORE_INFO("=== MULTI-SELECTION CENTER MODE ===");
               // EE_CORE_INFO("Combined {} entities: Center({:.3f}, {:.3f}, {:.3f})",
               //     entities.size(), center.x, center.y, center.z);

                return center;
            }

            // Fallback: average of all pivots
            Vec3 centerSum;
            int validCount = 0;
            for (auto entityId : entities)
            {
                if (ecs.IsEntityValid(entityId))
                {
                    centerSum += hierarchySystem->GetWorldPosition(entityId);
                    validCount++;
                }
            }

            if (validCount > 0)
                return centerSum / static_cast<float>(validCount);
        }

        return Vec3();
    }
}
