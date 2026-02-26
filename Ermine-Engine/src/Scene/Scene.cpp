/* Start Header ************************************************************************/
/*!
\file       Scene.cpp
\author     Edwin Lee Zirui, edwinzirui.lee 2301299, edwinzirui.lee\@digipen.edu
\date       Jan 24, 2025
\brief      Updated components with modular material system

Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Scene.h"
#include "Components.h"
#include "ECS.h"
#include "HierarchySystem.h"
#include "NavMesh.h"

namespace Ermine {
    Scene::Scene(const std::string& name) : m_Name(name) {
        EE_CORE_INFO("Created scene: {}", name);
    }

    Scene::~Scene() {
        //Clear();
    }

    EntityID Scene::CreateEntity(const std::string& name, bool needsTransform, bool needsHierarchy) {
        EntityID entity = ECS::GetInstance().CreateEntity();

        // Add essential components
        ECS::GetInstance().AddComponent(entity, ObjectMetaData(name, "Untagged", true));
        if (needsTransform) {
            ECS::GetInstance().AddComponent(entity, Transform());
        }

        if (needsHierarchy) {
            ECS::GetInstance().AddComponent(entity, HierarchyComponent());
            // New entities with hierarchy start as root entities
            m_RootEntitiesOrder.push_back(entity);
        }

        m_Entities.insert(entity);
        EE_CORE_TRACE("Created entity {} in scene {}", entity, m_Name);
        return entity;
    }

    void Scene::DestroyEntity(EntityID entity) {
        if (!HasEntity(entity)) return;

        // Destroy Nav Mesh Build and Runtime when deleting an ECS entity with Nav Mesh component. If not memory leak!
        auto navMeshSystem = ECS::GetInstance().GetSystem<Ermine::NavMeshSystem>();
        if (navMeshSystem && ECS::GetInstance().HasComponent<Ermine::NavMeshComponent>(entity))
        {
            auto& navComp = ECS::GetInstance().GetComponent<Ermine::NavMeshComponent>(entity);
            navMeshSystem->DestroyBuild(navComp);
            navMeshSystem->DestroyRuntime(navComp);
        }

        auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();

        // Get children before destroying to avoid accessing invalid data
        std::vector<EntityID> children;
        if (ECS::GetInstance().HasComponent<HierarchyComponent>(entity)) {
            const auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
            children = hierarchy.children;
        }

        // Unparent this entity
        hierarchySystem->UnsetParent(entity);

        // Delete all children recursively if any
        for (auto child : children) 
        {
            DestroyEntity(child);
        }

        // Remove from root entities order list
        auto it = std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), entity);
        if (it != m_RootEntitiesOrder.end()) {
            m_RootEntitiesOrder.erase(it);
        }

        // Remove from scene
        m_Entities.erase(entity);

        // Clear selection if this was selected
        if (m_SelectedEntity == entity) {
            m_SelectedEntity = 0;
        }

        ECS::GetInstance().DestroyEntity(entity);
        EE_CORE_TRACE("Destroyed entity {} from scene {}", entity, m_Name);
    }

    bool Scene::HasEntity(EntityID entity) const {
        return m_Entities.find(entity) != m_Entities.end();
    }

    std::vector<EntityID> Scene::GetRootEntities() const {
        EnsureSyncedWithECS();
        
        // Return the ordered list of root entities
        // This ensures consistent ordering across frames
        std::vector<EntityID> validRoots;
        for (auto entity : m_RootEntitiesOrder) {
            if (ECS::GetInstance().IsEntityValid(entity) && HasEntity(entity)) {
                if (ECS::GetInstance().HasComponent<HierarchyComponent>(entity)) {
                    const auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
                    if (hierarchy.parent == 0) {
                        validRoots.push_back(entity);
                    }
                }
            }
        }
        
        return validRoots;
    }

    // === NEW: Root entity reordering methods ===
    
    bool Scene::ReorderRootEntity(EntityID entity, size_t newIndex) {
        if (!HasEntity(entity)) return false;
        
        // Verify it's a root entity
        if (ECS::GetInstance().HasComponent<HierarchyComponent>(entity)) {
            const auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
            if (hierarchy.parent != 0) {
                EE_CORE_WARN("Cannot reorder entity {} - it has a parent", entity);
                return false;
            }
        }
        
        // Find current index
        auto it = std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), entity);
        if (it == m_RootEntitiesOrder.end()) {
            // Entity not in list, add it
            m_RootEntitiesOrder.push_back(entity);
            it = m_RootEntitiesOrder.end() - 1;
        }
        
        // Clamp newIndex to valid range
        newIndex = std::min(newIndex, m_RootEntitiesOrder.size() - 1);
        
        size_t currentIndex = std::distance(m_RootEntitiesOrder.begin(), it);
        if (currentIndex == newIndex) return true; // Already at correct position
        
        // Remove from current position
        m_RootEntitiesOrder.erase(it);
        
        // Insert at new position
        m_RootEntitiesOrder.insert(m_RootEntitiesOrder.begin() + newIndex, entity);
        
        EE_CORE_INFO("Reordered root entity {} to index {}", entity, newIndex);
        return true;
    }
    
    bool Scene::MoveRootEntityUp(EntityID entity) {
        if (!HasEntity(entity)) return false;
        
        auto it = std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), entity);
        if (it == m_RootEntitiesOrder.end() || it == m_RootEntitiesOrder.begin()) {
            return false; // Not found or already at top
        }
        
        // Swap with previous
        std::iter_swap(it, it - 1);
        EE_CORE_INFO("Moved root entity {} up", entity);
        return true;
    }
    
    bool Scene::MoveRootEntityDown(EntityID entity) {
        if (!HasEntity(entity)) return false;
        
        auto it = std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), entity);
        if (it == m_RootEntitiesOrder.end() || it == m_RootEntitiesOrder.end() - 1) {
            return false; // Not found or already at bottom
        }
        
        // Swap with next
        std::iter_swap(it, it + 1);
        EE_CORE_INFO("Moved root entity {} down", entity);
        return true;
    }
    
    bool Scene::InsertRootEntityAt(EntityID entity, size_t index) {
        if (!HasEntity(entity)) return false;
        
        // If entity has a parent, unparent it first
        if (ECS::GetInstance().HasComponent<HierarchyComponent>(entity)) {
            auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(entity);
            if (hierarchy.parent != 0) {
                auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();
                if (hierarchySystem) {
                    hierarchySystem->UnsetParent(entity);
                }
            }
        }
        
        // Remove from current position if already in list
        auto it = std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), entity);
        if (it != m_RootEntitiesOrder.end()) {
            m_RootEntitiesOrder.erase(it);
        }
        
        // Insert at new position
        index = std::min(index, m_RootEntitiesOrder.size());
        m_RootEntitiesOrder.insert(m_RootEntitiesOrder.begin() + index, entity);
        
        EE_CORE_INFO("Inserted root entity {} at index {}", entity, index);
        return true;
    }

    void Scene::SetSelectedEntity(EntityID entity) {
        // Validate entity exists in this scene
        if (entity != 0 && !HasEntity(entity)) {
            EE_CORE_WARN("Trying to select entity {} that doesn't exist in scene {}", entity, m_Name);
            return;
        }

        EntityID previousSelection = m_SelectedEntity;
        m_SelectedEntity = entity;

        if (previousSelection != entity) {
            EE_CORE_TRACE("Selected entity {} in scene {}", entity, m_Name);
        }
    }

    std::vector<EntityID> Scene::GetAllEntities() const {
        EnsureSyncedWithECS();
        return std::vector<EntityID>(m_Entities.begin(), m_Entities.end());
    }

    void Scene::Clear() {
        // Create a copy since DestroyEntity modifies m_Entities
        std::vector<EntityID> entitiesToDestroy(m_Entities.begin(), m_Entities.end());

        for (auto entity : entitiesToDestroy) {
            ECS::GetInstance().DestroyEntity(entity);
        }

        m_Entities.clear();
        m_SelectedEntity = 0;
        EE_CORE_INFO("Cleared scene: {}", m_Name);
    }
    void Scene::EnsureSyncedWithECS(bool force) const
    {
        auto& ecs = ECS::GetInstance();

        // Count how many ECS entities *should* be in the scene snapshot
        size_t ecsCount = 0;
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
            if (ecs.IsEntityValid(e) && ecs.HasComponent<HierarchyComponent>(e))
                ++ecsCount;

        // Rebuild if forced or if snapshot is out-of-date
        if (!force && !m_Entities.empty() && m_Entities.size() == ecsCount)
            return;

        m_Entities.clear();
        
        // Track which root entities we've seen
        std::unordered_set<EntityID> foundRoots;

        for (EntityID e = 0; e < MAX_ENTITIES; ++e) {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<HierarchyComponent>(e)) continue;

            auto& hc = ecs.GetComponent<HierarchyComponent>(e);
            if (hc.parent != 0 &&
                (!ecs.IsEntityValid(hc.parent) || !ecs.HasComponent<HierarchyComponent>(hc.parent))) {
                hc.parent = 0;
                hc.depth = 0;
            }
            m_Entities.insert(e);
            
            // Track root entities
            if (hc.parent == 0) {
                foundRoots.insert(e);
            }
        }
        
        // Update root entities order list
        // Remove entities that are no longer root or don't exist
        m_RootEntitiesOrder.erase(
            std::remove_if(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(),
                [&foundRoots](EntityID e) { return foundRoots.find(e) == foundRoots.end(); }),
            m_RootEntitiesOrder.end()
        );
        
        // Add new root entities that aren't in the order list yet
        for (EntityID rootEntity : foundRoots) {
            if (std::find(m_RootEntitiesOrder.begin(), m_RootEntitiesOrder.end(), rootEntity) 
                == m_RootEntitiesOrder.end()) {
                m_RootEntitiesOrder.push_back(rootEntity);
            }
        }
    }
}