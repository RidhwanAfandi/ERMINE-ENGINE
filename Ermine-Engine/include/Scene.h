/* Start Header ************************************************************************/
/*!
\file       Scene.h
\author     Edwin Lee Zirui, edwinzirui.lee 2301299, edwinzirui.lee\@digipen.edu
\date       Jan 24, 2025
\brief      Declares the Scene class for managing collections of entities and their
            hierarchical relationships within the game world.
Copyright (C) 2024 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "ECS.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace Ermine {
    /*!
    \class Scene
    \brief Manages a collection of entities and provides scene-level operations
    \details Handles entity creation/destruction, hierarchy queries, entity selection,
             and scene-wide operations like clearing and naming.
    */
    class Scene {
    private:
        std::string m_Name;                          ///< Name of the scene
        mutable std::unordered_set<EntityID> m_Entities;     ///< Set of all entities in this scene
        EntityID m_SelectedEntity = 0;               ///< Currently selected entity (0 = none)
        mutable std::vector<EntityID> m_RootEntitiesOrder;   ///< Ordered list of root entities (NEW)

    public:
        /*!
        \brief Constructs a new scene with an optional name
        \param name Name of the scene (defaults to "Untitled Scene")
        */
        explicit Scene(const std::string& name = "Untitled Scene");

        /*!
        \brief Destructor - cleans up scene resources
        */
        ~Scene();

        // Entity management
        /*!
        \brief Creates a new entity in the scene
        \param name Name for the new entity
        \param needsTransform If true, adds a Transform component to the entity (default: true)
        \param needsHierarchy If true, adds a Hierarchy component to the entity (default: true)
        \return EntityID of the newly created entity
        */
        EntityID CreateEntity(const std::string& name, bool needsTransform = true, bool needsHierarchy = true);

        /*!
        \brief Destroys an entity and removes it from the scene
        \param entity The entity ID to destroy
        */
        void DestroyEntity(EntityID entity);

        /*!
        \brief Checks if an entity exists in this scene
        \param entity The entity ID to check
        \return True if the entity exists in this scene, false otherwise
        */
        bool HasEntity(EntityID entity) const;

        // Hierarchy queries
        /*!
        \brief Gets all root-level entities (entities without parents) in the scene
        \return Vector of root entity IDs
        */
        std::vector<EntityID> GetRootEntities() const;

        /*!
        \brief Gets all entities in the scene
        \return Vector of all entity IDs
        */
        std::vector<EntityID> GetAllEntities() const;

        // === NEW: Root entity reordering methods ===
        /*!
        \brief Reorder a root entity to a specific index
        \param entity The root entity to reorder
        \param newIndex The new position in the root list
        \return True if successful, false otherwise
        */
        bool ReorderRootEntity(EntityID entity, size_t newIndex);

        /*!
        \brief Move a root entity up one position
        \param entity The root entity to move up
        \return True if successful, false otherwise
        */
        bool MoveRootEntityUp(EntityID entity);

        /*!
        \brief Move a root entity down one position
        \param entity The root entity to move down
        \return True if successful, false otherwise
        */
        bool MoveRootEntityDown(EntityID entity);

        /*!
        \brief Insert a root entity at a specific index
        \param entity The entity to insert (will be made root if not already)
        \param index The index to insert at
        \return True if successful, false otherwise
        */
        bool InsertRootEntityAt(EntityID entity, size_t index);

        // Selection
        /*!
        \brief Sets the currently selected entity
        \param entity The entity ID to select (0 to clear selection)
        */
        void SetSelectedEntity(EntityID entity);

        /*!
        \brief Gets the currently selected entity
        \return EntityID of the selected entity, or 0 if none is selected
        */
        EntityID GetSelectedEntity() const { return m_SelectedEntity; }

        /*!
        \brief Clears the current entity selection
        */
        void ClearSelection() { m_SelectedEntity = 0; }

        /*!
        \brief Checks if a specific entity is currently selected
        \param entity The entity ID to check
        \return True if the entity is selected, false otherwise
        */
        bool IsEntitySelected(EntityID entity) const { return m_SelectedEntity == entity; }

        // Scene properties
        /*!
        \brief Gets the name of the scene
        \return Reference to the scene name string
        */
        const std::string& GetName() const { return m_Name; }

        /*!
        \brief Sets the name of the scene
        \param name New name for the scene
        */
        void SetName(const std::string_view& name) { m_Name = name; }

        // Cleanup
        /*!
        \brief Clears all entities from the scene
        \details Destroys all entities and resets the scene to an empty state
        */
        void Clear();

        /*!
        \brief Gets the total number of entities in the scene
        \return Number of entities
        */
        size_t GetEntityCount() const { return m_Entities.size(); }

        /*!
        \brief Ensures hierarchy is synced with ECS
        \param Force sync
        */
        void EnsureSyncedWithECS(bool force = false) const;
    };
}