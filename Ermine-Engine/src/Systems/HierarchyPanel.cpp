/* Start Header ************************************************************************/
/*!
\file       HierarchyPanel.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee\@digipen.edu
\date       Sep 05, 2025
\brief      Hierarchy panel UI for scene entity tree management with drag-and-drop
            parenting support.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "HierarchyPanel.h"
#include "Components.h"
#include "HierarchySystem.h"
#include "ECS.h"
#include "GeometryFactory.h"
#include "AssetManager.h"
#include "Physics.h"
#include "Serialisation.h" // Added for prefab support
#include "Selection.h"
#include "EditorCamera.h"

namespace Ermine {
    void HierarchyPanel::SetScene(Scene* scene) {
        m_ActiveScene = scene;
        EE_CORE_INFO("HierarchyPanel::SetScene called with scene: {}", scene ? scene->GetName() : "null");
    }

    Scene* HierarchyPanel::GetScene() const {
        return m_ActiveScene;
    }

    void HierarchyPanel::OnImGuiRender() {
        if (!m_IsVisible) return;
        ImGui::Begin("Scene Hierarchy", &m_IsVisible);

        if (!m_ActiveScene) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No active scene");
            ImGui::End();
            return;
        }

        // Scene header
        ImGui::Text("Scene: %s", m_ActiveScene->GetName().c_str());
        ImGui::Text("Entities: %zu", m_ActiveScene->GetEntityCount());
        ImGui::Text("Selected: %zu", editor::Selection::All().size());
        ImGui::Separator();

        // Toolbar
        if (ImGui::Button("Create Entity")) {
            EntityID newEntity = m_ActiveScene->CreateEntity("New Entity");
            editor::Selection::SelectSingle(m_ActiveScene, newEntity);
            ImGui::SetWindowFocus("Inspector");
        }
        ImGui::SameLine();

        EntityID primary = editor::Selection::Primary();
        if (primary != 0 && ImGui::Button("Delete Selected")) {
            // Delete all currently selected entities
            auto sel = editor::Selection::All();
            std::vector<EntityID> toDelete(sel.begin(), sel.end());
            for (auto id : toDelete)
            {
                ECS::GetInstance().GetSystem<Physics>()->RemovePhysic(id);
                m_ActiveScene->DestroyEntity(id);
                ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
            }
            editor::Selection::Clear(m_ActiveScene);
        }

        //ImGui::SameLine();
        //ImGui::Checkbox("Show Inactive", &m_ShowInactive);
        //if (ImGui::IsItemHovered()) {
        //    ImGui::SetTooltip("Show inactive entities (grayed out)");
        //}

        ImGui::Separator();

        // Search bar
        ImGui::PushItemWidth(-1);
        if (ImGui::InputTextWithHint(
            "##HierarchySearch",
            "Search entities...",
            m_SearchBuffer,
            sizeof(m_SearchBuffer)))
        {
            m_IsSearching = (strlen(m_SearchBuffer) > 0);
        }
        ImGui::PopItemWidth();

        ImGui::Separator();

        // Entity hierarchy
        if (!m_IsSearching)
        {
            // Normal hierarchy view
            auto rootEntities = m_ActiveScene->GetRootEntities();
            for (auto entity : rootEntities)
                DrawEntityNode(entity, 0);
        }
        else
        {
            // Search result view (flat list)
            auto& ecs = ECS::GetInstance();

            for (EntityID id = 1; id < MAX_ENTITIES; ++id)
            {
                if (!ecs.IsEntityValid(id))
                    continue;

                if (!ecs.HasComponent<ObjectMetaData>(id))
                    continue;

                const auto& meta = ecs.GetComponent<ObjectMetaData>(id);

                if (!NameMatchesSearch(meta.name, m_SearchBuffer))
                    continue;

                DrawEntityNode(id, 0);
            }
        }

        // Add invisible button to catch drops on empty space
        ImVec2 space = ImGui::GetContentRegionAvail();
        if (space.x != 0.0f && space.y != 0.0f)
            ImGui::InvisibleButton("UnparentDropZone", ImGui::GetContentRegionAvail());
        HandleUnparentDrop();

        // Right-click context menu
        DrawContextMenu();

        // Handle keyboard shortcuts for reordering
        if (primary != 0 && ImGui::IsWindowFocused()) {
            auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();

            // Check if entity is a root entity
            bool isRootEntity = false;
            if (ECS::GetInstance().HasComponent<HierarchyComponent>(primary)) {
                const auto& hierarchy = ECS::GetInstance().GetComponent<HierarchyComponent>(primary);
                isRootEntity = (hierarchy.parent == 0);
            }

            // Ctrl+Up to move up
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && ImGui::GetIO().KeyCtrl) {
                if (isRootEntity) {
                    // Move root entity up
                    if (m_ActiveScene->MoveRootEntityUp(primary)) {
                        EE_CORE_INFO("Moved root entity {} up via keyboard", primary);
                    }
                } else {
                    // Move child entity up within parent
                    if (hierarchySystem->MoveChildUp(primary)) {
                        EE_CORE_INFO("Moved entity {} up via keyboard", primary);
                    }
                }
            }

            // Ctrl+Down to move down
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && ImGui::GetIO().KeyCtrl) {
                if (isRootEntity) {
                    // Move root entity down
                    if (m_ActiveScene->MoveRootEntityDown(primary)) {
                        EE_CORE_INFO("Moved root entity {} down via keyboard", primary);
                    }
                } else {
                    // Move child entity down within parent
                    if (hierarchySystem->MoveChildDown(primary)) {
                        EE_CORE_INFO("Moved entity {} down via keyboard", primary);
                    }
                }
            }
        }

        ImGui::End();
    }

    void HierarchyPanel::DuplicateEntity(EntityID sourceEntity) {
        if (sourceEntity == 0) return;

        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(sourceEntity)) return;

        // Store the original transform before duplication
        Transform originalTransform;
        if (ecs.HasComponent<Transform>(sourceEntity)) {
            originalTransform = ecs.GetComponent<Transform>(sourceEntity);
        }

        // Create a temporary prefab path in a system temp directory
        const auto tempPrefabPath = std::filesystem::temp_directory_path() / "temp_duplicate.prefab";

        try {
            // Save source entity as a prefab
            SavePrefabToFile(ecs, sourceEntity, tempPrefabPath);

            // Load the prefab which creates our new entity
            EntityID newEntity = LoadPrefabFromFile(ecs, tempPrefabPath);

            // Ensure we maintain the exact transform values
            if (ecs.HasComponent<Transform>(newEntity)) {
                auto& newTransform = ecs.GetComponent<Transform>(newEntity);
                newTransform = originalTransform; // Copy the exact transform
            }

            // Update the name to indicate it's a copy using Unity-style numbering
            if (ecs.HasComponent<ObjectMetaData>(newEntity)) {
                auto& meta = ecs.GetComponent<ObjectMetaData>(newEntity);
                const auto& sourceMeta = ecs.GetComponent<ObjectMetaData>(sourceEntity);

                // Get the base name (without any existing numeric suffix)
                std::string baseName = sourceMeta.name;
                size_t parenPos = baseName.find(" (");
                if (parenPos != std::string::npos) {
                    baseName = baseName.substr(0, parenPos);
                }

                // Find the next available number
                int suffix = 1;
                std::string newName;
                bool nameExists;
                do {
                    newName = baseName + " (" + std::to_string(suffix) + ")";
                    nameExists = false;

                    // Check if this name is already taken
                    for (EntityID id = 1; id < MAX_ENTITIES; ++id) {
                        if (id != newEntity && ecs.IsEntityValid(id) && ecs.HasComponent<ObjectMetaData>(id)) {
                            const auto& otherMeta = ecs.GetComponent<ObjectMetaData>(id);
                            if (otherMeta.name == newName) {
                                nameExists = true;
                                break;
                            }
                        }
                    }
                    suffix++;
                } while (nameExists);

                meta.name = newName;
            }

            // Make sure the new entity is added to the scene and selected
            //m_ActiveScene->SetSelectedEntity(newEntity);
            editor::Selection::SelectSingle(m_ActiveScene, newEntity);
            ImGui::SetWindowFocus("Inspector");

            // Clean up the temporary prefab file
            std::filesystem::remove(tempPrefabPath);

            EE_CORE_INFO("Duplicated entity {} to new entity {} using prefab system", sourceEntity, newEntity);
        }
        catch (const std::exception& e) {
            EE_CORE_ERROR("Failed to duplicate entity: {}", e.what());
        }
    }

    void HierarchyPanel::DrawEntityNode(EntityID entity, int depth) {
        if (!ECS::GetInstance().IsEntityValid(entity)) return;

        // Prevent recursive hierarchy during search
        if (m_IsSearching && depth > 0) return;

        auto& ecs = ECS::GetInstance();
        auto& metadata = ecs.GetComponent<ObjectMetaData>(entity);
        bool isSelected = editor::Selection::IsSelected(entity);

        // Check if entity is inactive
        bool isInactive = !metadata.selfActive;

        // Push gray color for inactive entities
        bool pushedColor = false;
        if (isInactive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            pushedColor = true;
        }

        ImGui::PushID((int)entity); // keep this

        // children
        std::vector<EntityID> children;
        if (ecs.HasComponent<HierarchyComponent>(entity)) {
            children = ecs.GetComponent<HierarchyComponent>(entity).children;
        }

        // indent
        float indent = static_cast<float>(depth) * m_indentPadding;
        if (indent > 0) ImGui::Indent(indent);

        // visible name + unique ID suffix
        std::string visible = metadata.name.empty() ? "Entity" : metadata.name;
        std::string label = visible + "##" + std::to_string((uint64_t)entity); // unique ID

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_FramePadding;

        if (isSelected) nodeFlags |= ImGuiTreeNodeFlags_Selected;
        if (children.empty()) nodeFlags |= ImGuiTreeNodeFlags_Leaf;

        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), nodeFlags);

        if (isSelected && ImGui::IsWindowAppearing())
            ImGui::SetScrollHereY(0.5f);

        HandleDragDrop(entity);

        // Handle single click for selection (don't focus inspector)
        if (ImGui::IsItemClicked()) {
            if (ImGui::GetIO().KeyShift && m_LastClickedEntity != 0)
            {
                // Shift+Click: Range selection (Unity/Unreal style)
                SelectRange(entity);
            }
            else if (ImGui::GetIO().KeyCtrl)
            {
                // Ctrl+Click: Toggle selection (add/remove from selection)
                editor::Selection::Toggle(m_ActiveScene, entity);
                m_LastClickedEntity = entity;
            }
            else
            {
                // Normal click: Single select (clears previous selection)
                editor::Selection::SelectSingle(m_ActiveScene, entity);
                m_LastClickedEntity = entity;
            }
        }

        // Handle double click to focus Inspector (Unity/Unreal style)
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            editor::Selection::SelectSingle(m_ActiveScene, entity);
            m_LastClickedEntity = entity;
            editor::EditorCamera::GetInstance().Focus(ecs.GetComponent<Transform>(entity).position, 2.5f);
            ImGui::SetWindowFocus("Inspector");
            Vector3D position = ecs.GetComponent<Transform>(entity).position;
            editor::EditorCamera::GetInstance().Focus(position, 2.5f);
        }

        if (ImGui::BeginPopupContextItem(("ctx##" + std::to_string((uint64_t)entity)).c_str())) {
            if (ImGui::MenuItem("Delete")) {
                // Delete all selected entities, not just the right-clicked one
                auto sel = editor::Selection::All();
                if (sel.empty() || !editor::Selection::IsSelected(entity))
                {
                    // If nothing selected or right-clicked entity isn't selected, just delete the right-clicked one
                    ECS::GetInstance().GetSystem<Physics>()->RemovePhysic(entity);
                    m_ActiveScene->DestroyEntity(entity);
                    ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
                }
                else
                {
                    // Delete all selected entities
                    std::vector<EntityID> toDelete(sel.begin(), sel.end());
                    for (auto id : toDelete)
                    {
                        ECS::GetInstance().GetSystem<Physics>()->RemovePhysic(id);
                        m_ActiveScene->DestroyEntity(id);
                    }
                    ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
                    editor::Selection::Clear(m_ActiveScene);
                }
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::MenuItem("Duplicate")) {
                // Duplicate all selected entities, not just the right-clicked one
                auto sel = editor::Selection::All();
                if (sel.empty() || !editor::Selection::IsSelected(entity))
                {
                    // If nothing selected or right-clicked entity isn't selected, just duplicate the right-clicked one
                    DuplicateEntity(entity);
                }
                else
                {
                    // Duplicate all selected entities
                    std::vector<EntityID> toDuplicate(sel.begin(), sel.end());
                    for (auto id : toDuplicate)
                    {
                        DuplicateEntity(id);
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            // === NEW: Reordering options ===
            auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();
            EntityID parent = hierarchySystem->GetParent(entity);
            
            if (parent != 0) {
                // Entity has a parent - show child reordering options
                ImGui::Separator();
                if (ImGui::MenuItem("Move Up", "Ctrl+Up")) {
                    if (hierarchySystem->MoveChildUp(entity)) {
                        EE_CORE_INFO("Moved entity {} up in hierarchy", entity);
                    }
                }
                if (ImGui::MenuItem("Move Down", "Ctrl+Down")) {
                    if (hierarchySystem->MoveChildDown(entity)) {
                        EE_CORE_INFO("Moved entity {} down in hierarchy", entity);
                    }
                }
            } else {
                // Entity is a root - show root reordering options
                ImGui::Separator();
                if (ImGui::MenuItem("Move Up", "Ctrl+Up")) {
                    if (m_ActiveScene->MoveRootEntityUp(entity)) {
                        EE_CORE_INFO("Moved root entity {} up", entity);
                    }
                }
                if (ImGui::MenuItem("Move Down", "Ctrl+Down")) {
                    if (m_ActiveScene->MoveRootEntityDown(entity)) {
                        EE_CORE_INFO("Moved root entity {} down", entity);
                    }
                }
            }

            ImGui::EndPopup();
        }

        if (nodeOpen) {
            for (auto child : children) {
                DrawEntityNode(child, depth + 1);
            }
            ImGui::TreePop();
        }

        if (indent > 0) ImGui::Unindent(indent);

        // Pop the color style if we pushed it
        if (pushedColor) {
            ImGui::PopStyleColor();
        }

        ImGui::PopID();
    }


    void HierarchyPanel::HandleDragDrop(EntityID entity) {
        // Drag source code
        if (ImGui::BeginDragDropSource()) {
            // Clear pending focus when drag starts
            m_PendingFocusEntity = 0;

            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(EntityID));
            auto& metadata = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
            ImGui::Text("Moving: %s", metadata.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target with VISUAL FEEDBACK
        if (ImGui::BeginDragDropTarget()) {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 p_min = ImGui::GetItemRectMin();
            ImVec2 p_max = ImGui::GetItemRectMax();

            bool shiftHeld = ImGui::GetIO().KeyShift;

            // Show different visual feedback based on modifier
            if (shiftHeld) {
                // Yellow line ABOVE the entity = "insert before"
                draw_list->AddLine(
                    ImVec2(p_min.x, p_min.y),
                    ImVec2(p_max.x, p_min.y),
                    IM_COL32(255, 255, 0, 255), // Yellow
                    3.0f
                );

                // Show tooltip
                auto& metadata = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                ImGui::SetTooltip("Insert before '%s'", metadata.name.c_str());
            }
            else {
                // Blue border = "make child of"
                draw_list->AddRect(
                    p_min, p_max,
                    IM_COL32(100, 200, 255, 255), // Blue
                    0.0f, 0, 2.0f
                );

                // Show tooltip
                auto& metadata = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                ImGui::SetTooltip("Make child of '%s'", metadata.name.c_str());
            }

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                EntityID droppedEntity = *(EntityID*)payload->Data;

                if (droppedEntity != entity) {
                    if (auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>()) {
                        if (!hierarchySystem->WouldCreateCycle(droppedEntity, entity)) {
                            if (shiftHeld) {
                                // Shift = insert BEFORE this entity (reorder)
                                auto parent = hierarchySystem->GetParent(entity);
                                
                                if (parent != 0) {
                                    // Both have a parent - reorder within parent's children
                                    const auto& siblings = hierarchySystem->GetChildren(parent);
                                    auto it = std::find(siblings.begin(), siblings.end(), entity);
                                    if (it != siblings.end()) {
                                        size_t targetIndex = std::distance(siblings.begin(), it);
                                        hierarchySystem->InsertChildAt(parent, droppedEntity, targetIndex);
                                        EE_CORE_INFO("Inserted entity {} at index {} under parent {}",
                                            droppedEntity, targetIndex, parent);
                                    }
                                }
                                else {
                                    // Target is a root entity - use scene's root reordering
                                    auto rootEntities = m_ActiveScene->GetRootEntities();
                                    auto it = std::find(rootEntities.begin(), rootEntities.end(), entity);
                                    if (it != rootEntities.end()) {
                                        size_t targetIndex = std::distance(rootEntities.begin(), it);
                                        m_ActiveScene->InsertRootEntityAt(droppedEntity, targetIndex);
                                        EE_CORE_INFO("Inserted root entity {} at index {}", 
                                                     droppedEntity, targetIndex);
                                    }
                                }
                            }
                            else {
                                // Default = make child (parenting)
                                hierarchySystem->SetParent(droppedEntity, entity);
                                hierarchySystem->MarkDirty(entity);
                                hierarchySystem->MarkDirty(droppedEntity);
                                EE_CORE_INFO("Parented entity {} to {}", droppedEntity, entity);
                            }
                        }
                        else {
                            EE_CORE_WARN("Cannot parent entity {} to {} - would create cycle!", droppedEntity, entity);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void HierarchyPanel::HandleUnparentDrop() {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                EntityID droppedEntity = *(EntityID*)payload->Data;

                auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();

                // Check if entity currently has a parent
                EntityID currentParent = hierarchySystem->GetParent(droppedEntity);
                if (currentParent != 0) {
                    hierarchySystem->UnsetParent(droppedEntity);
                    EE_CORE_INFO("Unparented entity {} - now a root entity", droppedEntity);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void HierarchyPanel::DrawContextMenu() {
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::MenuItem("Create Empty Entity")) {
                EntityID newEntity = m_ActiveScene->CreateEntity("Empty Entity");
                //m_ActiveScene->SetSelectedEntity(newEntity); // Auto-select the new entity
                editor::Selection::SelectSingle(m_ActiveScene, newEntity);
                ImGui::SetWindowFocus("Inspector");
            }

            if (ImGui::BeginMenu("Create Primitive")) {
                if (ImGui::MenuItem("Cube")) {
                    EntityID entity = m_ActiveScene->CreateEntity("Cube");
                    ECS::GetInstance().AddComponent(entity, graphics::GeometryFactory::CreateCube(1, 1, 1));

                    auto shader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/vertex.glsl", "../Resources/Shaders/fragment.glsl");
                    auto materialPtr = std::make_shared<graphics::Material>(shader);
                    materialPtr->SetVec3("material.albedo", Vec3(1.0f, 1.0f, 1.0f)); // white color

                    ECS::GetInstance().AddComponent(entity, Material(materialPtr));

                    // Log cube creation
                    EE_CORE_INFO("=== Cube Created ===");
                    EE_CORE_INFO("Entity ID: {}", entity);
                    if (ECS::GetInstance().HasComponent<Transform>(entity)) {
                        const auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
                        EE_CORE_INFO("Initial Position: {},{},{}",
                            transform.position.x,
                            transform.position.y,
                            transform.position.z);
                    }
                    EE_CORE_INFO("==================");

                    //m_ActiveScene->SetSelectedEntity(entity);
                    editor::Selection::SelectSingle(m_ActiveScene, entity);
                    ImGui::SetWindowFocus("Inspector");
                }
                if (ImGui::MenuItem("Sphere")) {
                    EntityID entity = m_ActiveScene->CreateEntity("Sphere");
                    ECS::GetInstance().AddComponent(entity, graphics::GeometryFactory::CreateSphere(1.0f));

                    auto shader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/vertex.glsl", "../Resources/Shaders/fragment.glsl");
                    auto materialPtr = std::make_shared<graphics::Material>(shader);
                    materialPtr->SetVec3("material.albedo", Vec3(0.8f, 0.8f, 0.8f)); // light grey color

                    ECS::GetInstance().AddComponent(entity, Material(materialPtr));

                    // Log sphere creation
                    EE_CORE_INFO("=== Sphere Created ===");
                    EE_CORE_INFO("Entity ID: {}", entity);
                    if (ECS::GetInstance().HasComponent<Transform>(entity)) {
                        const auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
                        EE_CORE_INFO("Initial Position: {},{},{}",
                            transform.position.x,
                            transform.position.y,
                            transform.position.z);
                    }
                    EE_CORE_INFO("==================");
                    //m_ActiveScene->SetSelectedEntity(entity);
                    editor::Selection::SelectSingle(m_ActiveScene, entity);
                    ImGui::SetWindowFocus("Inspector");
                }
                if (ImGui::MenuItem("Cone")) {
                    EntityID entity = m_ActiveScene->CreateEntity("Cone");
                    ECS::GetInstance().AddComponent(entity, graphics::GeometryFactory::CreateCone(1.0f, 2.0f, 32));

                    auto shader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/vertex.glsl", "../Resources/Shaders/fragment.glsl");
                    auto materialPtr = std::make_shared<graphics::Material>(shader);
                    materialPtr->SetVec3("material.albedo", Vec3(0.9f, 0.7f, 0.5f)); // tan/beige color

                    ECS::GetInstance().AddComponent(entity, Material(materialPtr));

                    // Log cone creation
                    EE_CORE_INFO("=== Cone Created ===");
                    EE_CORE_INFO("Entity ID: {}", entity);
                    if (ECS::GetInstance().HasComponent<Transform>(entity)) {
                        const auto& transform = ECS::GetInstance().GetComponent<Transform>(entity);
                        EE_CORE_INFO("Initial Position: {},{},{}",
                            transform.position.x,
                            transform.position.y,
                            transform.position.z);
                    }
                    EE_CORE_INFO("==================");
                    //m_ActiveScene->SetSelectedEntity(entity);
                    editor::Selection::SelectSingle(m_ActiveScene, entity);
                    ImGui::SetWindowFocus("Inspector");
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Create Light")) {
                EntityID entity = m_ActiveScene->CreateEntity("Light");
                ECS::GetInstance().AddComponent(entity, Light());
                /*m_ActiveScene->SetSelectedEntity(entity); */
                editor::Selection::SelectSingle(m_ActiveScene, entity);
                ImGui::SetWindowFocus("Inspector");
            }


            ImGui::Separator();

            EntityID primary = editor::Selection::Primary();
            if (primary != 0)
            {
                auto hs = ECS::GetInstance().GetSystem<HierarchySystem>();
                if (hs->GetParent(primary) != 0)
                {
                    if (ImGui::MenuItem("Unparent Selected"))
                        for (auto id : editor::Selection::All())
                        {
                            hs->UnsetParent(id);
                            EE_CORE_INFO("Unparented entity {}", id);
                        }
                }

                if (ImGui::MenuItem("Delete Selected"))
                {
                    auto sel = editor::Selection::All();
                    for (auto id : sel)
                    {
                        m_ActiveScene->DestroyEntity(id);
                    }
                    editor::Selection::Clear(m_ActiveScene);
                    ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
                }
            }

            ImGui::EndPopup();
        }
    }

    const char* HierarchyPanel::GetEntityIcon(EntityID entity) const {
        // Return simple text prefixes instead of symbols
        if (ECS::GetInstance().HasComponent<Light>(entity)) {
            return "[Light] ";
        }
        if (ECS::GetInstance().HasComponent<AudioComponent>(entity)) {
            return "[Audio] ";
        }
        if (ECS::GetInstance().HasComponent<Mesh>(entity)) {
            return "[Mesh] ";
        }
        return ""; // No prefix for basic entities
    }

    bool HierarchyPanel::HasSelectedDescendant(EntityID entity) const
    {
        if (editor::Selection::IsSelected(entity))
            return true;

        auto& ecs = ECS::GetInstance();
        if (!ecs.HasComponent<HierarchyComponent>(entity))
            return false;

		const auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
        for (auto child : hierarchy.children)
        {
            if (HasSelectedDescendant(child))
                return true;
        }
		return false;
    }

    bool HierarchyPanel::NameMatchesSearch(const std::string& name, const char* search) const
    {
        if (!search || search[0] == '\0')
            return true;

        std::string lowerName = name;
        std::string lowerSearch = search;

        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

        return lowerName.find(lowerSearch) != std::string::npos;
    }

    void HierarchyPanel::BuildVisibleEntityList(std::vector<EntityID>& outList) const
    {
        outList.clear();
        if (!m_ActiveScene) return;

        if (!m_IsSearching)
        {
            // Normal hierarchy view - collect in tree order
            auto rootEntities = m_ActiveScene->GetRootEntities();
            for (auto entity : rootEntities)
            {
                CollectVisibleEntitiesRecursive(entity, outList);
            }
        }
        else
        {
            // Search result view - flat list of matching entities
            auto& ecs = ECS::GetInstance();
            for (EntityID id = 1; id < MAX_ENTITIES; ++id)
            {
                if (!ecs.IsEntityValid(id))
                    continue;

                if (!ecs.HasComponent<ObjectMetaData>(id))
                    continue;

                const auto& meta = ecs.GetComponent<ObjectMetaData>(id);

                if (NameMatchesSearch(meta.name, m_SearchBuffer))
                    outList.push_back(id);
            }
        }
    }

    void HierarchyPanel::CollectVisibleEntitiesRecursive(EntityID entity, std::vector<EntityID>& outList) const
    {
        if (!ECS::GetInstance().IsEntityValid(entity)) return;

        outList.push_back(entity);

        auto& ecs = ECS::GetInstance();
        if (ecs.HasComponent<HierarchyComponent>(entity))
        {
            const auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
            for (auto child : hierarchy.children)
            {
                CollectVisibleEntitiesRecursive(child, outList);
            }
        }
    }

    void HierarchyPanel::SelectRange(EntityID clickedEntity)
    {
        if (m_LastClickedEntity == 0 || clickedEntity == 0)
        {
            // No previous selection, just select single
            editor::Selection::SelectSingle(m_ActiveScene, clickedEntity);
            m_LastClickedEntity = clickedEntity;
            return;
        }

        // Build the visible entity list in display order
        std::vector<EntityID> visibleEntities;
        BuildVisibleEntityList(visibleEntities);

        // Find indices of both entities
        auto startIt = std::find(visibleEntities.begin(), visibleEntities.end(), m_LastClickedEntity);
        auto endIt = std::find(visibleEntities.begin(), visibleEntities.end(), clickedEntity);

        if (startIt == visibleEntities.end() || endIt == visibleEntities.end())
        {
            // One of the entities not found in visible list, just select single
            editor::Selection::SelectSingle(m_ActiveScene, clickedEntity);
            m_LastClickedEntity = clickedEntity;
            return;
        }

        // Ensure start <= end
        if (startIt > endIt)
            std::swap(startIt, endIt);

        // Select all entities in range
        std::unordered_set<EntityID> rangeSelection;
        for (auto it = startIt; it <= endIt; ++it)
        {
            rangeSelection.insert(*it);
        }

        editor::Selection::Set(m_ActiveScene, rangeSelection);
        // Note: m_LastClickedEntity stays the same for chained Shift+clicks
    }
}
