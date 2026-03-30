/* Start Header ************************************************************************/
/*!
\file       AnimationGUI.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       28/02/2026
\brief      This file contains the definition of the animation editor GUI.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AnimationGUI.h"
#include "Animator.h"   // for animator access
#include "Components.h" // for AnimationComponent

namespace Ermine
{
    /**
     * @brief Creates a new animation state with the given name.
     * @param name The name of the new state.
     */
    void AnimationEditorImGUI::CreateState(const std::string& name)
    {
        // Check if entity selected
        if (m_SelectedEntity == 0) return;

        // Create new state node
        auto& graph = ECS::GetInstance().GetComponent<AnimationComponent>(m_SelectedEntity).m_animationGraph;
        auto s = std::make_shared<AnimationStateNode>();
        s->id = m_nextNodeId++;
        s->name = name;
        s->isAttached = false;
        s->clipName.clear();

        // Stagger each new node
        float offset = static_cast<float>(graph->states.size()) * 10.f;
        s->editorPos = ImVec2(100.f + offset, 100.f + offset);

        graph->states.push_back(s);

        // Set visible position
        ImNodes::SetNodeEditorSpacePos(s->id, s->editorPos);
    }

    /**
     * @brief Find state pointer by id.
     * @param graph The animation graph to search.
     * @param id The ID of the state to find.
     * @return Shared pointer to the found AnimationStateNode, or nullptr if not found.
     */
    std::shared_ptr<AnimationStateNode> AnimationEditorImGUI::FindStateById(const std::shared_ptr<AnimationGraph>& graph, int id)
    {
        for (auto& s : graph->states)
            if (s->id == id) return s;
        return nullptr;
    }

    /**
     * @brief Draws the node editor for the animation graph.
     * @param graph The animation graph being edited.
     * @param animator The animator associated with the entity.
     */
    void AnimationEditorImGUI::DrawNodeEditor(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator)
    {
        ImGui::BeginChild("NodeEditor", ImGui::GetContentRegionAvail(), true);

        // Check ImNodes context
        if (!ImNodes::GetCurrentContext()) return;

        // Begin node editor
        ImNodes::BeginNodeEditor();

        // Render nodes
        for (auto& nPtr : graph->states)
            DrawNode(*nPtr, graph, animator);

        // Draw links
        for (auto& link : graph->links) {
            int startAttr = link.fromNodeId * 10; // Out attr
            int endAttr = link.toNodeId * 10 + 1; // In attr
            ImNodes::Link(link.id, startAttr, endAttr);
        }

        // End node editor
        ImNodes::EndNodeEditor();

        // Handle new link creation
        int startAttr = 0, endAttr = 0;
        if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
            if (startAttr > 0 && endAttr > 0) {
                int fromId = startAttr / 10;
                int toId = (endAttr - 1) / 10;

                // Prevent self-links and validate nodes
                if (fromId != toId) {
                    bool fromExists = false, toExists = false;
                    for (auto& s : graph->states) {
                        if (s->id == fromId) fromExists = true;
                        if (s->id == toId)   toExists = true;
                    }

                    if (fromExists && toExists) {
                        // Assign unique link ID
                        static int nextLinkId = 1;
                        int newLinkId = nextLinkId++;

                        graph->links.push_back({ newLinkId, fromId, toId });
                        graph->transitions.push_back(AnimationTransition{ fromId, toId, 0.0f, 0.25f });
                        EE_CORE_INFO("Created link %d: %d -> %d", newLinkId, fromId, toId);
                    }
                    else
                        EE_CORE_WARN("Ignored invalid link: missing nodes (%d -> %d)", fromId, toId);
                }
                else
                    EE_CORE_WARN("Ignored self-link (%d -> %d)", fromId, toId);
            }
            else
                EE_CORE_WARN("Invalid attr ids (%d, %d)", startAttr, endAttr);
        }

        // Handle link deletion
        int destroyedLinkId;
        while (ImNodes::IsLinkDestroyed(&destroyedLinkId)) {
            auto it = std::find_if(graph->links.begin(), graph->links.end(),
                [&](const AnimationLink& l) { return l.id == destroyedLinkId; });

            if (it != graph->links.end()) {
                EE_CORE_INFO("Deleted link %d -> %d", it->fromNodeId, it->toNodeId);

                // Remove corresponding transition
                graph->transitions.erase(std::remove_if(graph->transitions.begin(), graph->transitions.end(),
                    [&](const AnimationTransition& t) { return (t.fromNodeId == it->fromNodeId && t.toNodeId == it->toNodeId); }), graph->transitions.end());

                // Remove the actual link
                graph->links.erase(it);
            }
        }

        // Handle right-click context menu
        static int selectedLinkId = -1;

        // Detect hovered link and open context menu
        int hoveredLinkId = -1;
        if (ImNodes::IsLinkHovered(&hoveredLinkId) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            selectedLinkId = hoveredLinkId;
            ImGui::OpenPopup("LinkContextMenu");
        }

        // Context menu for links
        if (ImGui::BeginPopup("LinkContextMenu")) {
            if (selectedLinkId > 0) {
                // Find link object
                auto it = std::find_if(graph->links.begin(), graph->links.end(),
                    [&](const AnimationLink& l) { return l.id == selectedLinkId; });

                if (it != graph->links.end()) {
                    // Find readable state names
                    std::string fromName = "Unknown";
                    std::string toName = "Unknown";
                    if (auto fromNode = FindStateById(graph, it->fromNodeId))
                        fromName = fromNode->name.empty() ? ("State " + std::to_string(it->fromNodeId)) : fromNode->name;
                    if (auto toNode = FindStateById(graph, it->toNodeId))
                        toName = toNode->name.empty() ? ("State " + std::to_string(it->toNodeId)) : toNode->name;

                    ImGui::Text("Link: %s -> %s", fromName.c_str(), toName.c_str());
                    ImGui::Separator();

                    // Delete link option
                    if (ImGui::MenuItem("Delete Link")) {
                        EE_CORE_INFO("Deleted link via context menu %s -> %s", fromName.c_str(), toName.c_str());

                        // Remove the corresponding transition
                        graph->transitions.erase(std::remove_if(graph->transitions.begin(), graph->transitions.end(),
                            [&](const AnimationTransition& t) { return (t.fromNodeId == it->fromNodeId && t.toNodeId == it->toNodeId); }), graph->transitions.end());

                        // Remove the link itself
                        graph->links.erase(it);
                        selectedLinkId = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            else
                ImGui::TextDisabled("No link selected");

            ImGui::EndPopup();
        }

        // Handle node deletions
        if (!nodesToDelete.empty()) {
            for (int del : nodesToDelete) {
                graph->states.erase(std::remove_if(graph->states.begin(), graph->states.end(),
                    [&](auto& s) { return s->id == del; }), graph->states.end());

                graph->links.erase(std::remove_if(graph->links.begin(), graph->links.end(),
                    [&](const AnimationLink& l) { return l.fromNodeId == del || l.toNodeId == del; }), graph->links.end());

                graph->transitions.erase(std::remove_if(graph->transitions.begin(), graph->transitions.end(),
                    [&](auto& t) { return t.fromNodeId == del || t.toNodeId == del; }), graph->transitions.end());
            }
            nodesToDelete.clear();
        }

        ImGui::EndChild(); // NodeEditor end
    }

    /**
     * @brief Draws a single animation state node.
     * @param n The animation state node to draw.
     * @param graph The animation graph being edited.
     * @param animator The animator associated with the entity.
     */
    void AnimationEditorImGUI::DrawNode(AnimationStateNode& n, const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator)
    {
        // Check if this is the active node
        const bool isActive = (graph->current && graph->current->id == n.id);

        // Highlight current playing node
        ImNodes::PushColorStyle(ImNodesCol_TitleBar, isActive ? IM_COL32(80, 150, 255, 255) : IM_COL32(50, 50, 80, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(100, 180, 255, 255));
        ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(100, 180, 255, 255));

        ImNodes::BeginNode(n.id);
        {
            // Title bar
            ImNodes::BeginNodeTitleBar();

            // Handle inline renaming
            if (m_isRenaming && m_renameNodeId == n.id) {
                ImGui::SetKeyboardFocusHere();
                ImGui::PushItemWidth(140.0f);
                if (ImGui::InputText("##Rename", m_renameBuffer, IM_ARRAYSIZE(m_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    n.name = m_renameBuffer;
                    m_isRenaming = false;
                    m_renameNodeId = -1;
                }

                // Cancel rename if clicked outside or lost focus
                if (!ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                    m_isRenaming = false;
                    m_renameNodeId = -1;
                }

                ImGui::PopItemWidth();
            }
            else {
                ImGui::TextUnformatted(n.name.c_str());

                // Double-click to rename
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    m_isRenaming = true;
                    m_renameNodeId = n.id;
                    strncpy_s(m_renameBuffer, sizeof(m_renameBuffer), n.name.c_str(), _TRUNCATE);
                    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                }
            }

            ImNodes::EndNodeTitleBar();

            // Input attribute
            ImNodes::BeginInputAttribute(n.id * 10 + 1);
            ImGui::Text("In");
            ImNodes::EndInputAttribute();

            ImGui::Spacing();

            // Main node body
            ImGui::PushItemWidth(200.f);

            // Clip attach
            if (!n.isAttached) {
                if (ImGui::BeginCombo(("Attach##" + std::to_string(n.id)).c_str(), "Select Clip...")) {
                    for (auto& clip : animator->GetClips()) {
                        if (ImGui::Selectable(clip.name.c_str())) {
                            n.clipName = clip.name;
                            n.isAttached = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else {
                ImGui::Text("Clip: %s", n.clipName.c_str());
                ImGui::SameLine();
                if (ImGui::Button(("Detach##" + std::to_string(n.id)).c_str())) {
                    n.clipName.clear();
                    n.isAttached = false;
                }
            }

            // Start state
            bool prevStart = n.isStartState;
            ImGui::Checkbox(("Start##" + std::to_string(n.id)).c_str(), &n.isStartState);
            if (n.isStartState && !prevStart) {
                for (auto& other : graph->states)
                    if (other->id != n.id) other->isStartState = false;
                graph->current = FindStateById(graph, n.id);
            }

            // Parameters
            ImGui::DragFloat(("Speed##" + std::to_string(n.id)).c_str(), &n.speed, 0.01f, 0.01f, 10.f);
            ImGui::DragFloat(("Blend##" + std::to_string(n.id)).c_str(), &n.blendWeight, 0.01f, 0.f, 1.f);
            ImGui::Checkbox(("Loop##" + std::to_string(n.id)).c_str(), &n.loop);

            // Action buttons
            if (ImGui::Button(("Preview##" + std::to_string(n.id)).c_str())) {
                if (n.isAttached && animator) {
                    animator->PlayAnimation(n.clipName, n.loop);
                    graph->current = FindStateById(graph, n.id);
                    graph->playing = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(("Delete##" + std::to_string(n.id)).c_str()))
                nodesToDelete.push_back(n.id);

            ImGui::PopItemWidth();

            // Output attribute
            ImNodes::BeginOutputAttribute(n.id * 10);
            ImGui::Text("Out");
            ImNodes::EndOutputAttribute();

            // Save editor pos
            n.editorPos = ImNodes::GetNodeEditorSpacePos(n.id);
        }
        ImNodes::EndNode();

        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
        ImNodes::PopColorStyle();
    }

    /**
     * @brief Draws the information inspector panel.
     * @param graph The animation graph being edited.
     * @param animator The animator associated with the entity.
     */
    void AnimationEditorImGUI::DrawInformationInspector(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator)
    {
        ImGui::BeginChild("InformationInspector", ImVec2(0, 0), true);
        ImGui::SeparatorText("Animator Info");

        if (animator) {
            if (const auto* clip = animator->GetCurrentClip()) {
                ImGui::Text("Current Clip: %s", clip->name.c_str());
                ImGui::Text("Duration: %.2fs", clip->duration / clip->ticksPerSecond);
                ImGui::Text("Ticks: %.2f, TPS: %.2f", clip->duration, clip->ticksPerSecond);
            }
            else
                ImGui::Text("Current Clip: <None>");
            ImGui::Text("Paused: %s", animator->IsPaused() ? "Yes" : "No");
            ImGui::Text("Looping: %s", animator->IsLooping() ? "Yes" : "No");

            const auto& clips = animator->GetClips();
            ImGui::Text("Clip Count: %zu", clips.size());
            if (!clips.empty()) {
                ImGui::SeparatorText("Available Clips:");
                for (const auto& clip : clips)
                    ImGui::BulletText("%s", clip.name.c_str());
            }
        }
        else
            ImGui::TextDisabled("No Animator Found.");

        ImGui::SeparatorText("Animation Graph Info");
        if (graph) {
            ImGui::Text("States: %zu", graph->states.size());
            ImGui::Text("Transitions: %zu", graph->transitions.size());
            ImGui::Text("Parameters: %zu", graph->parameters.size());
            ImGui::Text("Current State: %s",
                (graph->current && !graph->current->name.empty()) ? graph->current->name.c_str() : "<None>");
            ImGui::Text("Playback Speed: %.2f", graph->playbackSpeed);
            ImGui::Text("Is Playing: %s", graph->playing ? "Yes" : "No");
        }
        else
            ImGui::TextDisabled("No Animation Graph Found.");

        //ImGui::Separator();
        //if (ImGui::Button("Save Graph", ImVec2(0, 0))) {
        //    // TODO: Hook into your graph serialization (JSON/YAML)
        //    EE_CORE_INFO("Animation graph saved (placeholder)");
        //}
        //ImGui::SameLine();
        //if (ImGui::Button("Load Graph", ImVec2(0, 0))) {
        //    // TODO: Hook into your graph serialization (JSON/YAML)
        //    EE_CORE_INFO("Animation graph loaded (placeholder)");
        //}

        ImGui::EndChild();
    }

    /**
     * @brief Draws the state inspector panel.
     * @param graph The animation graph being edited.
     * @param animator The animator associated with the entity.
     */
    void AnimationEditorImGUI::DrawStateInspector(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator)
    {
        ImGui::BeginChild("StateInspector", ImVec2(0, 160), true);

        // State creation
        ImGui::InputTextWithHint("##NewState", "New state name...", m_newStateName, IM_ARRAYSIZE(m_newStateName));
        ImGui::SameLine();
        if (ImGui::Button("Add State") && strlen(m_newStateName)) {
            CreateState(m_newStateName);
            m_newStateName[0] = '\0';
        }

        if (auto clip = animator->GetCurrentClip()) {
            // Small timeline scrubber
            float durationSec = static_cast<float>(clip->duration / clip->ticksPerSecond);
            ImGui::SliderFloat("Timeline", &graph->currentTime, 0.f, durationSec, "%.2fs");
            if (ImGui::IsItemActive() || ImGui::IsItemDeactivatedAfterEdit()) {
                animator->Seek(graph->currentTime);
                graph->playing = false;
            }

            // Playback speed
            ImGui::DragFloat("Speed", &graph->playbackSpeed, 0.1f, 0.1f, 5.f);
        }

        // Playback controls
        if (ImGui::Button("Play")) {
            graph->playing = true;
            if (graph->current && animator)
                animator->PlayAnimation(graph->current->clipName, graph->current->loop);
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            graph->playing = false;
            if (animator) animator->PauseAnimation();
        }
        ImGui::SameLine();
        if (ImGui::Button("Resume")) {
            graph->playing = true;
            if (animator) animator->ResumeAnimation();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            graph->playing = false;
            if (animator) animator->StopAnimation();
        }
        ImGui::EndChild(); // StateInspector end
    }

    /**
     * @brief Draws the parameter inspector panel.
     * @param graph The animation graph being edited.
     */
    void AnimationEditorImGUI::DrawParameterInspector(const std::shared_ptr<AnimationGraph>& graph)
    {
        ImGui::BeginChild("ParameterInspector", ImVec2(0, 0), true);

        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Parameters");
        ImGui::Separator();

        // Create new parameter
        static char paramName[64] = "";
        static int paramType = 1;
        ImGui::InputText("Name", paramName, IM_ARRAYSIZE(paramName));
        ImGui::Combo("Type", &paramType, "Bool\0Float\0Int\0Trigger\0");
        if (ImGui::Button("Add") && strlen(paramName) > 0) {
            AnimationParameter p;
            p.name = paramName;
            p.type = static_cast<AnimationParameter::Type>(paramType);
            graph->parameters.push_back(p);
            paramName[0] = '\0';
        }

        ImGui::Separator();

        // List parameters
        for (size_t i = 0; i < graph->parameters.size(); ++i) {
            auto& p = graph->parameters[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%s", p.name.c_str());
            ImGui::SameLine(ImGui::GetColumnWidth(0) - 60);
            if (ImGui::Button("X")) {
                graph->parameters.erase(graph->parameters.begin() + i);
                ImGui::PopID();
                break;
            }

            // Value editor
            switch (p.type) {
            case AnimationParameter::Type::Bool:
                ImGui::Checkbox("##val", &p.boolValue);
                break;
            case AnimationParameter::Type::Float:
                ImGui::DragFloat("##val", &p.floatValue, 0.1f);
                break;
            case AnimationParameter::Type::Int:
                ImGui::DragInt("##val", &p.intValue, 1);
                break;
            case AnimationParameter::Type::Trigger:
                if (ImGui::Button("Trigger")) p.triggerValue = true;
                break;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        ImGui::EndChild(); // ParameterInspector end
    }

    /**
     * @brief Draws the transition inspector panel.
     * @param graph The animation graph being edited.
     */
    void AnimationEditorImGUI::DrawTransitionInspector(const std::shared_ptr<AnimationGraph>& graph)
    {
        ImGui::BeginChild("TransitionInspector", ImVec2(0, 0), true);

        if (graph->transitions.empty()) {
            ImGui::TextDisabled("No transitions yet. Connect states in the graph.");
            ImGui::EndChild();
            return;
        }

        // List transitions
        for (size_t i = 0; i < graph->transitions.size(); ++i) {
            auto& t = graph->transitions[i];
            ImGui::PushID(static_cast<int>(i));

            // Resolve readable state names
            std::string fromName = "Unknown";
            std::string toName = "Unknown";

            // Find states by id
            if (auto from = FindStateById(graph, t.fromNodeId))
                fromName = from->name.empty() ? ("State " + std::to_string(from->id)) : from->name;
            if (auto to = FindStateById(graph, t.toNodeId))
                toName = to->name.empty() ? ("State " + std::to_string(to->id)) : to->name;

            // Transition header
            std::string label = fromName + " -> " + toName;
            bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            if (open) {
                ImGui::Indent();

                ImGui::DragFloat("Exit Time", &t.exitTime, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Blend Duration", &t.duration, 0.01f, 0.0f, 10.0f);

                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.f, 1), "Conditions");

                // Each condition within this transition
                for (size_t c = 0; c < t.conditions.size(); ++c) {
                    auto& cond = t.conditions[c];
                    ImGui::PushID(static_cast<int>(c));

                    // Parameter selection
                    if (ImGui::BeginCombo("Parameter", cond.parameterName.empty() ? "Select..." : cond.parameterName.c_str())) {
                        for (auto& p : graph->parameters)
                            if (ImGui::Selectable(p.name.c_str(), p.name == cond.parameterName))
                                cond.parameterName = p.name;
                        ImGui::EndCombo();
                    }

                    // Comparison operator dropdown
                    const char* comparisons[] = { "==", "!=", ">", "<", ">=", "<=" };
                    int compIndex = 0;
                    for (int ci = 0; ci < 6; ++ci)
                        if (cond.comparison == comparisons[ci]) compIndex = ci;

                    if (ImGui::BeginCombo("Operator", comparisons[compIndex])) {
                        for (int ci = 0; ci < 6; ++ci)
                            if (ImGui::Selectable(comparisons[ci], compIndex == ci))
                                cond.comparison = comparisons[ci];
                        ImGui::EndCombo();
                    }

                    // Value field (bool/float/int/trigger)
                    if (!cond.parameterName.empty()) {
                        auto it = std::find_if(graph->parameters.begin(), graph->parameters.end(),
                            [&](auto& p) { return p.name == cond.parameterName; });

                        if (it != graph->parameters.end()) {
                            switch (it->type) {
                            case AnimationParameter::Type::Bool:
                                ImGui::Checkbox("Value", &cond.boolValue);
                                break;
                            case AnimationParameter::Type::Float:
                            case AnimationParameter::Type::Int:
                                ImGui::DragFloat("Threshold", &cond.threshold, 0.1f);
                                break;
                            case AnimationParameter::Type::Trigger:
                                ImGui::TextDisabled("(Trigger)");
                                break;
                            }
                        }
                    }

                    // Remove condition button
                    if (ImGui::Button("Remove Condition")) {
                        t.conditions.erase(t.conditions.begin() + c);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                // Add new condition button
                if (ImGui::Button("Add Condition"))
                    t.conditions.push_back(AnimationCondition{});

                ImGui::Separator();

                // Delete transition button
                if (ImGui::Button("Delete Transition")) {
                    // Get transition info before erasing
                    int fromId = t.fromNodeId;
                    int toId = t.toNodeId;

                    // Remove transition itself
                    graph->transitions.erase(graph->transitions.begin() + i);

                    // Remove matching link in the node editor
                    graph->links.erase(std::remove_if(graph->links.begin(), graph->links.end(),
                        [&](const AnimationLink& l) { return (l.fromNodeId == fromId && l.toNodeId == toId); }), graph->links.end());

                    EE_CORE_INFO("Deleted transition %d -> %d and its corresponding link", fromId, toId);
                    ImGui::PopID();
                    break;
                }

                ImGui::Unindent();
            }

            ImGui::PopID();
        }

        ImGui::EndChild(); // TransitionInspector end
    }

    /**
     * @brief Renders the animation editor UI.
     */
    void AnimationEditorImGUI::Render()
    {
        // Initialize ImNodes context
        if (!ImNodes::GetCurrentContext()) {
            ImNodes::CreateContext();
            ImNodes::StyleColorsDark();
        }

        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr())) // Window begin
        {
            ImGui::End();
            return;
        }

        // Check if no entity selected
        if (m_SelectedEntity == 0) {
            ImGui::Text("No entity selected.");
            ImGui::End();
            return;
        }

        // Check if animation component exist
        if (!ECS::GetInstance().HasComponent<AnimationComponent>(m_SelectedEntity)) {
            ImGui::Text("Entity has no Animator component.");
            ImGui::End();
            return;
        }

        // Get animator and graph
        auto& animator = ECS::GetInstance().GetComponent<AnimationComponent>(m_SelectedEntity).m_animator;
        auto& graph = ECS::GetInstance().GetComponent<AnimationComponent>(m_SelectedEntity).m_animationGraph;

        // Check if animator exists
        if (!animator) {
            ImGui::Text("Animator is null.");
            ImGui::End();
            return;
        }

        // Check if graph exists
        if (!graph) {
            ImGui::Text("Graph is null.");
            ImGui::End();
            return;
        }

        ImGui::Columns(2, "AnimationEditorColumns", false);
        ImGui::SetColumnWidth(0, 400.0f);

        // Left Column: Parameters
        ImGui::BeginChild("LeftColumn", ImVec2(0, 0), false);
        {
            // Parameters
            DrawParameterInspector(graph);
        }
        ImGui::EndChild(); // LeftColumn end

        ImGui::NextColumn();

        // Right Column: Information + States + Transitions + Node Editor
        ImGui::BeginChild("RightColumn", ImVec2(0, 0), false);
        {
            if (ImGui::BeginTabBar("RightTopTabs", ImGuiTabBarFlags_None)) {

                // Information
                if (ImGui::BeginTabItem("Info")) {
                    DrawInformationInspector(graph, animator);
                    ImGui::EndTabItem();
                }

                // States + Node Editor
                if (ImGui::BeginTabItem("States")) {
                    // States
                    DrawStateInspector(graph, animator);
                    ImGui::EndTabItem();

                    // Node Editor
                    DrawNodeEditor(graph, animator);
                }

                // Transitions
                if (ImGui::BeginTabItem("Transitions")) {
                    DrawTransitionInspector(graph);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild(); // RightColumn end

        ImGui::Columns(1);
        ImGui::End(); // Window end
    }
}
