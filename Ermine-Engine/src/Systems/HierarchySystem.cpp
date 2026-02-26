/* Start Header ************************************************************************/
/*!
\file       HierarchySystem.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee\@digipen.edu
\date       Sep 05, 2025
\brief      Implementation of HierarchySystem.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "HierarchySystem.h"
#include "Matrix4x4.h"
#include "Physics.h"

namespace Ermine
{
    /**
     * @brief Checks if setting parent would create a cycle in the hierarchy.
     * @param[in] child The entity to be reparented.
     * @param[in] parent The entity to be set as parent.
     * @return True if a cycle would be created, false otherwise.
    */
    bool HierarchySystem::WouldCreateCycle(EntityID child, EntityID parent) const
    {
        EntityID current = parent;
        while (current != 0) {
            if (current == child) return true;
            current = GetParent(current);
        }
        return false;
    }

    /**
     * @brief Decomposes a 4x4 matrix into position, rotation, and scale components.
     * @param[in] matrix The matrix to decompose.
     * @param[out] position The extracted position.
     * @param[out] rotation The extracted rotation as quaternion.
     * @param[out] scale The extracted scale.
    */
    void HierarchySystem::DecomposeMatrix(const Mtx44& matrix, Vec3& position, Quaternion& rotation, Vec3& scale)
    {
        // Extract position (translation) from the last column
        position.x = matrix.m03;
        position.y = matrix.m13;
        position.z = matrix.m23;

        // Extract scale from the lengths of the first three columns
        Vec3 col0(matrix.m00, matrix.m10, matrix.m20);
        Vec3 col1(matrix.m01, matrix.m11, matrix.m21);
        Vec3 col2(matrix.m02, matrix.m12, matrix.m22);

        scale.x = Vec3Length(col0);
        scale.y = Vec3Length(col1);
        scale.z = Vec3Length(col2);

        // Handle negative determinant (reflection)
        float det = matrix.m00 * (matrix.m11 * matrix.m22 - matrix.m12 * matrix.m21) -
            matrix.m01 * (matrix.m10 * matrix.m22 - matrix.m12 * matrix.m20) +
            matrix.m02 * (matrix.m10 * matrix.m21 - matrix.m11 * matrix.m20);

        if (det < 0.0f) {
            scale.x = -scale.x;
        }

        // Create rotation matrix by normalizing the columns
        if (scale.x != 0.0f && scale.y != 0.0f && scale.z != 0.0f) {
            Mtx44 rotMatrix;
            Mtx44Identity(rotMatrix);

            rotMatrix.m00 = col0.x / scale.x; rotMatrix.m01 = col1.x / scale.y; rotMatrix.m02 = col2.x / scale.z;
            rotMatrix.m10 = col0.y / scale.x; rotMatrix.m11 = col1.y / scale.y; rotMatrix.m12 = col2.y / scale.z;
            rotMatrix.m20 = col0.z / scale.x; rotMatrix.m21 = col1.z / scale.y; rotMatrix.m22 = col2.z / scale.z;

            // Convert rotation matrix to quaternion
            rotation = Mtx44GetQuaternion(rotMatrix);
        }
        else {
            // Identity rotation if scale is zero
            rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    /**
     * @brief Sets the parent of an entity, updating hierarchy and depth.
     * @param[in] child The entity to set the parent for.
     * @param[in] parent The entity to set as parent.
     * @param[in] preserveWorldTransform Whether to preserve world transform during reparenting.
     */
    void HierarchySystem::SetParent(EntityID child, EntityID parent, bool preserveWorldTransform)
    {
        if (!ECS::GetInstance().IsEntityValid(child) || !ECS::GetInstance().IsEntityValid(parent))
            return;

        if (WouldCreateCycle(child, parent))
            return;

        // Get references to components - PROPERLY INITIALIZED
        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        auto& parentHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(parent);
        auto& childTransform = ECS::GetInstance().GetComponent<Transform>(child);

        if (ECS::GetInstance().HasComponent<PhysicComponent>(child))
        {
            auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(child);
            p.prevTranPos = ECS::GetInstance().GetComponent<Transform>(parent).position;
            p.prevTranRot = ECS::GetInstance().GetComponent<Transform>(parent).rotation;
            p.prevTranScale = ECS::GetInstance().GetComponent<Transform>(parent).scale;
            p.update = true;
        }

        Mtx44 childWorldMatrix;
        if (preserveWorldTransform) {
            // Get current world matrix before changing parent
            if (ECS::GetInstance().HasComponent<GlobalTransform>(child)) {
                childWorldMatrix = ECS::GetInstance().GetComponent<GlobalTransform>(child).worldMatrix;
            }
            else {
                childWorldMatrix = childTransform.GetLocalMatrix();
            }
        }

        // Remove from old parent if exists (this will preserve world position)
        if (childHierarchy.parent != 0) {
            UnsetParent(child); // This already preserves world position
        }

        // Set new parent relationship
        childHierarchy.parent = parent;
        childHierarchy.depth = parentHierarchy.depth + 1;
        childHierarchy.isDirty = true;
        parentHierarchy.children.push_back(child);

        if (preserveWorldTransform) {
            // Get parent's world matrix
            Mtx44 parentWorldMatrix;
            if (ECS::GetInstance().HasComponent<GlobalTransform>(parent)) {
                parentWorldMatrix = ECS::GetInstance().GetComponent<GlobalTransform>(parent).worldMatrix;
            }
            else {
                auto& parentTransform = ECS::GetInstance().GetComponent<Transform>(parent);
                parentWorldMatrix = parentTransform.GetLocalMatrix();
            }

            // Calculate: LocalMatrix = ParentWorldMatrix^-1 * ChildWorldMatrix
            Mtx44 parentInverse;
            if (Mtx44Inverse(parentInverse, parentWorldMatrix)) {
                Mtx44 newLocalMatrix = parentInverse * childWorldMatrix;

                // Extract position, rotation, scale from the new local matrix
                DecomposeMatrix(newLocalMatrix, childTransform.position, childTransform.rotation, childTransform.scale);

                //EE_CORE_INFO("=== REPARENTING ENTITY {} to PARENT {} ===", child, parent);
                //EE_CORE_INFO("New local position: ({:.3f}, {:.3f}, {:.3f})",
                //    childTransform.position.x, childTransform.position.y, childTransform.position.z);
            }
            else {
                //EE_CORE_WARN("Failed to invert parent matrix during reparenting");
            }
        }
        ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
        MarkDirty(child);
    }

    /**
     * @brief Removes the parent of an entity, updating hierarchy and depth.
     * @param[in] child The entity to unset the parent for.
    */
    void HierarchySystem::UnsetParent(EntityID child)
    {
        if (!ECS::GetInstance().IsEntityValid(child))
            return;

        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        auto& childTransform = ECS::GetInstance().GetComponent<Transform>(child);

        if (childHierarchy.parent != 0)
        {
            // UNITY-STYLE FIX: Preserve world position when unparenting
            // 1. Get current world position/rotation/scale before unparenting
            Vec3 worldPosition = GetWorldPosition(child);
            Quaternion worldRotation = GetWorldRotation(child);
            Vec3 worldScale = GetWorldScale(child);

            //EE_CORE_INFO("=== UNPARENTING ENTITY {} ===", child);
            //EE_CORE_INFO("Preserving world position: ({:.3f}, {:.3f}, {:.3f})",
            //             worldPosition.x, worldPosition.y, worldPosition.z);

            // 2. Remove parent relationship
            auto& parentHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(childHierarchy.parent);
            auto it = std::find(parentHierarchy.children.begin(), parentHierarchy.children.end(), child);
            if (it != parentHierarchy.children.end())
                parentHierarchy.children.erase(it);

            childHierarchy.parent = 0;
            childHierarchy.depth = 0;
            childHierarchy.isDirty = true;

            // 3. CRITICAL: Set local transform to maintain world position
            // Since this entity is now a root (no parent), local = world
            childTransform.position = worldPosition;
            childTransform.rotation = worldRotation;
            childTransform.scale = worldScale;

            //EE_CORE_INFO("Set new local position: ({:.3f}, {:.3f}, {:.3f})", 
            //             childTransform.position.x, childTransform.position.y, childTransform.position.z);
            //EE_CORE_INFO("Entity {} is now a root entity", child);
            if (ECS::GetInstance().HasComponent<PhysicComponent>(child))
            {
                auto& p = ECS::GetInstance().GetComponent<PhysicComponent>(child);
                p.update = true;
            }

            ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
            // Update transforms
            MarkDirty(child);
        }
    }

    /**
     * @brief Recursively updates world transforms for an entity and its children.
     * @param[in] entity The root entity to start updating from.
    */
    void HierarchySystem::UpdateWorldTransform(EntityID entity)
	{
		// Validate entity and required components
		if (!ECS::GetInstance().IsEntityValid(entity))
			return;

		if (!ECS::GetInstance().HasComponent<HierarchyComponent>(entity) ||
			!ECS::GetInstance().HasComponent<Transform>(entity))
			return;

		auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
		auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
		
		// Ensure GlobalTransform exists
		if (!ECS::GetInstance().HasComponent<GlobalTransform>(entity)) {
			ECS::GetInstance().AddComponent<GlobalTransform>(entity, GlobalTransform());
		}
		
		auto& globalTransform = ECS::GetInstance().GetComponent<GlobalTransform>(entity);

		//EE_CORE_INFO("=== Transform Update for Entity {0} ===", entity);
		//EE_CORE_INFO("Local Position: ({0:.3f}, {1:.3f}, {2:.3f})", 
		//			 transform.position.x, transform.position.y, transform.position.z);

		// Build local transform matrix from position, rotation, and scale
		Mtx44 localMatrix = transform.GetLocalMatrix();

		// Calculate world transform based on parent relationship
		if (hierarchy.parent != 0) {
			// Ensure parent has GlobalTransform before using it
			if (!ECS::GetInstance().HasComponent<GlobalTransform>(hierarchy.parent)) {
				// Parent missing GlobalTransform - treat as root
				EE_CORE_WARN("Parent {} missing GlobalTransform, treating child {} as root", hierarchy.parent, entity);
				globalTransform.worldMatrix = localMatrix;
			}
			else {
				auto& parentGlobalTransform = ECS::GetInstance().GetComponent<GlobalTransform>(hierarchy.parent);
				
				//Vec3 parentWorldPos = parentGlobalTransform.GetWorldPosition();
				//EE_CORE_INFO("Parent {0} World Position: ({1:.3f}, {2:.3f}, {3:.3f})", 
				//			 hierarchy.parent, parentWorldPos.x, parentWorldPos.y, parentWorldPos.z);
				
				// FIXED: World transform = Parent's world transform * Local transform
				globalTransform.worldMatrix = parentGlobalTransform.worldMatrix * localMatrix;
			}
		}
		else {
			// Root entity: world transform equals local transform
			globalTransform.worldMatrix = localMatrix;
			//EE_CORE_INFO("Entity {0} is ROOT - World = Local transform", entity);
		}

		// Extract and log the calculated world position
		//Vec3 worldPos = globalTransform.GetWorldPosition();
		//EE_CORE_INFO(">>> Entity {0} World Position: ({1:.3f}, {2:.3f}, {3:.3f})", 
		//			 entity, worldPos.x, worldPos.y, worldPos.z);

		// Mark as clean AFTER calculating transforms
		hierarchy.isDirty = false;
		hierarchy.worldTransformDirty = false;
		transform.isDirty = false;
		globalTransform.isDirty = false;

		// Recursively update all children
		for (auto child : hierarchy.children) {
			//EE_CORE_INFO("--- Updating child entity {0} due to parent {1} change ---", child, entity);
			UpdateWorldTransform(child);
		}
		
		//EE_CORE_INFO("=== End Transform Update for Entity {0} ===\n", entity);
	}

	// NEW: Helper method to ensure entities have GlobalTransform
	void HierarchySystem::EnsureGlobalTransform(EntityID entity)
	{
		if (!ECS::GetInstance().HasComponent<GlobalTransform>(entity)) {
			ECS::GetInstance().AddComponent<GlobalTransform>(entity, GlobalTransform());
		}
	}

	// NEW: Initialize method to call when adding entities to hierarchy
	void HierarchySystem::InitializeEntity(EntityID entity)
	{
		if (!ECS::GetInstance().IsEntityValid(entity))
			return;
			
		if (!ECS::GetInstance().HasComponent<HierarchyComponent>(entity))
			return;
			
		// Ensure the entity has a GlobalTransform component
		EnsureGlobalTransform(entity);
			
		auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
		auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
		auto& globalTransform = ECS::GetInstance().GetComponent<GlobalTransform>(entity);
		
		// Mark as needing initial update
		hierarchy.isDirty = true;
		hierarchy.worldTransformDirty = true;
		transform.isDirty = true;
		globalTransform.isDirty = true;
		
		// Initialize world transform for root entities
		if (hierarchy.parent == 0)
		{
			globalTransform.worldMatrix = transform.GetLocalMatrix();
			globalTransform.isDirty = false;
		}
	}

    /**
     * @brief Updates the hierarchy for all root entities in the system.
    */
    void HierarchySystem::UpdateHierarchy()
    {
        if (m_Entities.empty())
            return;

        for (auto entity : m_Entities)
        {
            // Check for active
            if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
            {
                const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                if (!meta.selfActive)
                    continue;
            }

            // Verify entity still exists
            if (!ECS::GetInstance().IsEntityValid(entity))
                continue;
			
            if (!ECS::GetInstance().HasComponent<HierarchyComponent>(entity) ||
                !ECS::GetInstance().HasComponent<Transform>(entity))
                continue;

            if (!ECS::GetInstance().HasComponent<GlobalTransform>(entity))
            {
                ECS::GetInstance().AddComponent<GlobalTransform>(entity, GlobalTransform());
            }

            auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
            auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);

			const bool dirty = hierarchy.isDirty || hierarchy.worldTransformDirty || transform.isDirty;
            if (hierarchy.parent == HierarchyComponent::INVALID_PARENT && dirty) {
                UpdateWorldTransform(entity);
            }
        }
    }

    /**
     * @brief Marks an entity and all its children as dirty for transform updates.
     * @param[in] entity The entity to mark as dirty.
     */
    void HierarchySystem::MarkDirty(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.IsEntityValid(entity))
            return;

        // Check if entity has required components before accessing
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return;

        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);

        // DEBUG LOG - Can be commented out in production
        //EE_CORE_WARN("MarkDirty called for entity {} - investigate why!", entity);

        // Mark hierarchy as needing update
        hierarchy.isDirty = true;
        hierarchy.worldTransformDirty = true;

        // Only mark Transform dirty if entity has one
        if (ecs.HasComponent<Transform>(entity))
        {
            auto& transform = ecs.GetComponent<Transform>(entity);
            transform.isDirty = true;  // Make sure Transform component is also marked dirty
        }

        // Bubble up so a root (or any ancestor) triggers UpdateWorldTransform
        EntityID ancestor = hierarchy.parent;
        while (ancestor != HierarchyComponent::INVALID_PARENT && ecs.IsEntityValid(ancestor))
        {
            if (!ecs.HasComponent<HierarchyComponent>(ancestor))
                break;

            auto& ancHier = ecs.GetComponent<HierarchyComponent>(ancestor);
            // No need to mark ancestor's local transform dirty unless its own local changed.
            // We only need worldTransform recompute.
            ancHier.worldTransformDirty = true;

            // Optionally mark isDirty if you treat it as "needs recompute" (keeps semantics simple):
            // ancHier.isDirty = true;

            ancestor = ancHier.parent;
        }

        // Mark all children's world transforms as needing update (but not their local transforms)
        for (auto child : hierarchy.children) {
            if (!ecs.IsEntityValid(child) || !ecs.HasComponent<HierarchyComponent>(child))
                continue;

            auto& childHierarchy = ecs.GetComponent<HierarchyComponent>(child);

            // Mark world transform as dirty for children - their local transforms haven't changed
            childHierarchy.worldTransformDirty = true;

            // Only mark Transform dirty if child has one
            if (ecs.HasComponent<Transform>(child))
            {
                auto& childTransform = ecs.GetComponent<Transform>(child);
                childTransform.isDirty = true; // Also mark Transform component dirty for proper rendering
            }
            
            // Recursively mark children's world transforms as dirty
            MarkChildrenWorldTransformDirty(child);
        }
    }

    /**
     * @brief Helper method to recursively mark only world transforms as dirty for children
     * @param[in] entity The entity whose children need world transform updates
     */
    void HierarchySystem::MarkChildrenWorldTransformDirty(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.IsEntityValid(entity))
            return;

        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return;

        const auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);

        for (auto child : hierarchy.children) {
            if (!ecs.IsEntityValid(child) || !ecs.HasComponent<HierarchyComponent>(child))
                continue;

            auto& childHierarchy = ecs.GetComponent<HierarchyComponent>(child);
            childHierarchy.worldTransformDirty = true;

            // Only mark Transform dirty if child has one
            if (ecs.HasComponent<Transform>(child))
            {
                auto& childTransform = ecs.GetComponent<Transform>(child);
                childTransform.isDirty = true; // Also mark Transform component dirty
            }

            // Recursively mark grandchildren
            MarkChildrenWorldTransformDirty(child);
        }
    }

    /**
     * @brief Gets the parent of an entity.
     * @param[in] entity The entity to query.
     * @return The parent entity ID, or 0 if none.
    */
    EntityID HierarchySystem::GetParent(EntityID entity) const
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return 0;
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return 0;
        return ecs.GetComponent<HierarchyComponent>(entity).parent;
    }

    /**
      * @brief Gets the children of an entity.
      * @param[in] entity The entity to query.
      * @return Reference to a vector of child entity IDs.
     */
    const std::vector<EntityID>& HierarchySystem::GetChildren(EntityID entity) const
    {
        static std::vector<EntityID> empty;
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return empty;
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return empty;
        return ecs.GetComponent<HierarchyComponent>(entity).children;
    }

    const int HierarchySystem::GetChildCount(EntityID entity) const
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return 0;
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return 0;
        return ecs.GetComponent<HierarchyComponent>(entity).children.size();
    }

    /**
     * @brief Gets the world position of an entity.
     * @param[in] entity The entity to query.
     * @return The world position vector.
     */
    Vec3 HierarchySystem::GetWorldPosition(EntityID entity) const
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return Vec3(0.0f, 0.0f, 0.0f);

        // Use GlobalTransform
        if (ecs.HasComponent<GlobalTransform>(entity)) {
            const auto& globalTransform = ecs.GetComponent<GlobalTransform>(entity);
            return globalTransform.GetWorldPosition();
        }
        else if (ecs.HasComponent<Transform>(entity)) {
            // Fallback to local position if no GlobalTransform
            const auto& transform = ecs.GetComponent<Transform>(entity);
            return transform.position;
        }
        else {
            // No transform components at all
            return Vec3(0.0f, 0.0f, 0.0f);
        }
    }

    /**
     * @brief Gets the world rotation of an entity.
     * @param[in] entity The entity to query.
     * @return The world rotation quaternion.
     */
    Quaternion HierarchySystem::GetWorldRotation(EntityID entity) const
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);

        // FIXED: Use GlobalTransform instead of transform.transform_matrix
        if (ecs.HasComponent<GlobalTransform>(entity)) {
            const auto& globalTransform = ecs.GetComponent<GlobalTransform>(entity);
            return globalTransform.GetWorldRotation();
        }
        else if (ecs.HasComponent<Transform>(entity)) {
            // Fallback to local rotation if no GlobalTransform
            const auto& transform = ecs.GetComponent<Transform>(entity);
            return transform.rotation;
        }
        else {
            // No transform components - return identity
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    /**
     * @brief Gets the world scale of an entity.
     * @param[in] entity The entity to query.
     * @return The world scale vector.
     */
    Vec3 HierarchySystem::GetWorldScale(EntityID entity) const
    {
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(entity))
            return Vec3(1.0f, 1.0f, 1.0f);

        // FIXED: Use GlobalTransform instead of transform.transform_matrix
        if (ecs.HasComponent<GlobalTransform>(entity)) {
            const auto& globalTransform = ecs.GetComponent<GlobalTransform>(entity);
            return globalTransform.GetWorldScale();
        }
        else if (ecs.HasComponent<Transform>(entity)) {
            // Fallback to local scale if no GlobalTransform
            const auto& transform = ecs.GetComponent<Transform>(entity);
            return transform.scale;
        }
        else {
            // No transform components - return identity scale
            return Vec3(1.0f, 1.0f, 1.0f);
        }
    }

    /**
     * @brief Sets the world position of an entity, updating local transform accordingly.
     * @param[in] entity The entity to modify.
     * @param[in] worldPos The new world position.
     */
    void HierarchySystem::SetWorldPosition(EntityID entity, const Vec3& worldPos)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);

        if (hierarchy.parent != 0) {
            // FIXED: Use parent's GlobalTransform instead of hierarchy.worldTransform
            if (ECS::GetInstance().HasComponent<GlobalTransform>(hierarchy.parent)) {
                auto& parentGlobalTransform = ECS::GetInstance().GetComponent<GlobalTransform>(hierarchy.parent);

                // Get inverse of parent's world transform
                Matrix4x4 invParentWorld;
                Mtx44Inverse(invParentWorld, parentGlobalTransform.worldMatrix);

                // Convert world position to parent-local space using Vector3D
                Vector3D worldPosVec(worldPos.x, worldPos.y, worldPos.z);
                Vector3D localPos = invParentWorld * worldPosVec;

                transform.position = Vec3(localPos.x, localPos.y, localPos.z);
            }
            else {
                // Parent doesn't have GlobalTransform - treat as root
                transform.position = worldPos;
            }
        }
        else {
            // Root entity - world position = local position
            transform.position = worldPos;
        }

        MarkDirty(entity);
    }

    /**
     * @brief Sets the world rotation of an entity, updating local transform accordingly.
     * @param[in] entity The entity to modify.
     * @param[in] worldRot The new world rotation.
     */
    void HierarchySystem::SetWorldRotation(EntityID entity, const Quaternion& worldRot)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);

        if (hierarchy.parent != 0) {
            // FIXED: Use parent's GlobalTransform instead of hierarchy.worldTransform
            if (ECS::GetInstance().HasComponent<GlobalTransform>(hierarchy.parent)) {
                auto& parentGlobalTransform = ECS::GetInstance().GetComponent<GlobalTransform>(hierarchy.parent);

                // Extract parent's world rotation from GlobalTransform
                Quaternion parentWorldRot = parentGlobalTransform.GetWorldRotation();

                // Calculate local rotation: parentWorld^-1 * worldRot 
                Quaternion invParentRot(-parentWorldRot.x, -parentWorldRot.y, -parentWorldRot.z, parentWorldRot.w);
                transform.rotation = invParentRot * worldRot;
            }
            else {
                // Parent doesn't have GlobalTransform - treat as root
                transform.rotation = worldRot;
            }
        }
        else {
            // Root entity - world rotation = local rotation
            transform.rotation = worldRot;
        }

        MarkDirty(entity);
    }

    /**
     * @brief Sets the world scale of an entity, updating local transform accordingly.
     * @param[in] entity The entity to modify.
     * @param[in] worldScale The new world scale.
     */
    void HierarchySystem::SetWorldScale(EntityID entity, const Vec3& worldScale)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);

        if (hierarchy.parent != 0) {
            // Get parent's world scale
            Vec3 parentWorldScale = GetWorldScale(hierarchy.parent);

            // Calculate local scale needed to achieve desired world scale
            transform.scale.x = (parentWorldScale.x != 0.0f) ? worldScale.x / parentWorldScale.x : worldScale.x;
            transform.scale.y = (parentWorldScale.y != 0.0f) ? worldScale.y / parentWorldScale.y : worldScale.y;
            transform.scale.z = (parentWorldScale.z != 0.0f) ? worldScale.z / parentWorldScale.z : worldScale.z;
        }
        else {
            // Root entity - world scale = local scale
            transform.scale = worldScale;
        }

        MarkDirty(entity);
    }

    /**
     * @brief Adds a child entity to a parent, creating the hierarchy relationship.
     * @param[in] parent The parent entity ID.
     * @param[in] child The child entity ID.
     * @return True if successful, false if it would create a cycle or entities are invalid.
     */
    bool HierarchySystem::AddChild(EntityID parent, EntityID child)
    {
        if (!ECS::GetInstance().IsEntityValid(parent) || !ECS::GetInstance().IsEntityValid(child))
            return false;
            
        if (WouldCreateCycle(child, parent))
            return false;
            
        SetParent(child, parent);
        return true;
    }

    /**
     * @brief Removes a child from its parent.
     * @param[in] child The child entity ID.
     * @return True if successful, false if child has no parent.
     */
    bool HierarchySystem::RemoveChild(EntityID child)
    {
        if (!ECS::GetInstance().IsEntityValid(child))
            return false;
            
        auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        if (hierarchy.parent == 0)
            return false;
            
        UnsetParent(child);
        return true;
    }

    /**
     * @brief Call this when an entity's local transform has been modified to ensure proper propagation.
     * @param[in] entity The entity whose transform was modified.
     */
    void HierarchySystem::OnTransformChanged(EntityID entity)
    {
        MarkDirty(entity);
    }

    /**
     * @brief Force update transforms for all entities (useful after loading/creating scenes)
     */
    void HierarchySystem::ForceUpdateAllTransforms()
    {
        // Mark all entities as dirty
        for (auto entity : m_Entities)
        {
            auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
            auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
            
            hierarchy.isDirty = true;
            hierarchy.worldTransformDirty = true;
            transform.isDirty = true;
        }
        
        // Force update hierarchy
        UpdateHierarchy();
    }

    /**
     * @brief Sets the local position of an entity and marks it dirty for transform updates.
     * @param[in] entity The entity to modify.
     * @param[in] localPos The new local position.
     */
    void HierarchySystem::SetLocalPosition(EntityID entity, const Vec3& localPos)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        transform.position = localPos;
        
        // Mark as dirty to trigger transform update propagation
        MarkDirty(entity);
    }

    /**
     * @brief Sets the local rotation of an entity and marks it dirty for transform updates.
     * @param[in] entity The entity to modify.
     * @param[in] localRot The new local rotation.
     */
    void HierarchySystem::SetLocalRotation(EntityID entity, const Quaternion& localRot)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        transform.rotation = localRot;
        
        // Mark as dirty to trigger transform update propagation
        MarkDirty(entity);
    }

    /**
     * @brief Sets the local scale of an entity and marks it dirty for transform updates.
     * @param[in] entity The entity to modify.
     * @param[in] localScale The new local scale.
     */
    void HierarchySystem::SetLocalScale(EntityID entity, const Vec3& localScale)
    {
        if (!ECS::GetInstance().IsEntityValid(entity))
            return;

        auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
        transform.scale = localScale;
        
        // Mark as dirty to trigger transform update propagation
        MarkDirty(entity);
    }

    /**
     * @brief Move a child to a specific position in its parent's children list
     * @param[in] child The entity to reorder
     * @param[in] newIndex The new position in the parent's children list
     * @return True if successful, false otherwise
     */
    bool HierarchySystem::ReorderChild(EntityID child, size_t newIndex)
    {
        if (!ECS::GetInstance().IsEntityValid(child))
            return false;

        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        
        if (childHierarchy.parent == 0)
            return false; // Root entities can't be reordered this way

        auto& parentHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(childHierarchy.parent);
        
        // Find current index
        auto it = std::find(parentHierarchy.children.begin(), parentHierarchy.children.end(), child);
        if (it == parentHierarchy.children.end())
            return false; // Child not found in parent's list

        // Clamp newIndex to valid range
        newIndex = std::min(newIndex, parentHierarchy.children.size() - 1);

        size_t currentIndex = std::distance(parentHierarchy.children.begin(), it);
        
        if (currentIndex == newIndex)
            return true; // Already at the correct position

        // Remove from current position
        parentHierarchy.children.erase(it);
        
        // Insert at new position
        parentHierarchy.children.insert(parentHierarchy.children.begin() + newIndex, child);
        
        return true;
    }

    /**
     * @brief Move a child up one position in its parent's children list
     * @param[in] child The entity to move up
     * @return True if successful, false otherwise
     */
    bool HierarchySystem::MoveChildUp(EntityID child)
    {
        if (!ECS::GetInstance().IsEntityValid(child))
            return false;

        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        
        if (childHierarchy.parent == 0)
            return false; // Root entities can't be moved this way

        auto& parentHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(childHierarchy.parent);
        
        // Find current index
        auto it = std::find(parentHierarchy.children.begin(), parentHierarchy.children.end(), child);
        if (it == parentHierarchy.children.end() || it == parentHierarchy.children.begin())
            return false; // Child not found or already at the top

        // Swap with previous
        std::iter_swap(it, it - 1);
        
        return true;
    }

    /**
     * @brief Move a child down one position in its parent's children list
     * @param[in] child The entity to move down
     * @return True if successful, false otherwise
     */
    bool HierarchySystem::MoveChildDown(EntityID child)
    {
        if (!ECS::GetInstance().IsEntityValid(child))
            return false;

        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);
        
        if (childHierarchy.parent == 0)
            return false; // Root entities can't be moved this way

        auto& parentHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(childHierarchy.parent);
        
        // Find current index
        auto it = std::find(parentHierarchy.children.begin(), parentHierarchy.children.end(), child);
        if (it == parentHierarchy.children.end() || it == parentHierarchy.children.end() - 1)
            return false; // Child not found or already at the bottom

        // Swap with next
        std::iter_swap(it, it + 1);
        
        return true;
    }

    /**
     * @brief Insert a child at a specific position in a parent's children list
     * @param[in] parent The parent entity
     * @param[in] child The child entity to insert
     * @param[in] index The position to insert at
     * @return True if successful, false otherwise
     */
    bool HierarchySystem::InsertChildAt(EntityID parent, EntityID child, size_t index)
    {
        if (!ECS::GetInstance().IsEntityValid(parent) || !ECS::GetInstance().IsEntityValid(child))
            return false;

        if (WouldCreateCycle(child, parent))
            return false;

        auto& childHierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(child);

        // Check if we're actually changing parents (not just reordering)
        bool changingParent = (childHierarchy.parent != parent);

        if (changingParent) {
            // Use SetParent which already handles world transform preservation
            SetParent(child, parent, true);  // preserveWorldTransform = true

            // Now reorder to the desired index
            return ReorderChild(child, index);
        }
        else {
            // Just reordering within same parent - use ReorderChild
            return ReorderChild(child, index);
        }
    }
}