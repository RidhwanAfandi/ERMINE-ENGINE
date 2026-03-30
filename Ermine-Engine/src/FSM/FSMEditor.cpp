/* Start Header ************************************************************************/
/*!
\file       FSMEditor.cpp
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       26/01/2026
\brief      This file contains definitions for imgui window FSM editor.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "FSMEditor.h"
#include "FiniteStateMachine.h"

namespace Ermine
{
    std::deque<std::shared_ptr<ScriptNode>>& FSMEditorImGUI::GetNodesForEntity(EntityID entity)
    {
        if (!ECS::GetInstance().HasComponent<StateMachine>(entity))
            ECS::GetInstance().AddComponent(entity, StateMachine());
        return ECS::GetInstance().GetComponent<StateMachine>(entity).m_Nodes;
    }

    const std::deque<std::shared_ptr<ScriptNode>>& FSMEditorImGUI::GetNodesForEntity(EntityID entity) const
    {
        static const std::deque<std::shared_ptr<ScriptNode>> empty;
        if (!ECS::GetInstance().HasComponent<StateMachine>(entity))
            return empty;
        return ECS::GetInstance().GetComponent<StateMachine>(entity).m_Nodes;
    }

    void FSMEditorImGUI::CreateNode(const std::string& name)
    {
        if (m_SelectedEntity == 0)
            return;

        if (!ECS::GetInstance().HasComponent<StateMachine>(m_SelectedEntity))
            ECS::GetInstance().AddComponent(m_SelectedEntity, StateMachine());

        auto& fsm = ECS::GetInstance().GetComponent<StateMachine>(m_SelectedEntity);

        // Sync next id to existing nodes, prevents collisions when loading a scene
        int maxId = 0;
        for (auto& n : fsm.m_Nodes)
            if (n && n->id > maxId) maxId = n->id;

        if (m_nextNodeId <= maxId)
            m_nextNodeId = maxId + 1;

        // ensure chosen id isn't already taken in case scene has duplicates
        auto idExists = [&](int id)
            {
                for (auto& n : fsm.m_Nodes)
                    if (n && n->id == id) return true;
                return false;
            };
        while (idExists(m_nextNodeId))
            ++m_nextNodeId;

        auto node = std::make_shared<ScriptNode>();
        node->id = m_nextNodeId++;
        node->name = name;
        //node->editorPosition = ImVec2(40.0f + node->id * 30.0f, 40.0f + node->id * 20.0f);
        node->editorPosition = ImVec2(0.0f, 0.0f);
        node->positionInitialized = false;
        node->isAttached = false;
        node->scriptClassName = "";
        fsm.m_Nodes.push_back(node);
    }

    void FSMEditorImGUI::Render()
    {
        if (!ImNodes::GetCurrentContext())
        {
            ImNodes::CreateContext();
            ImNodes::StyleColorsDark();
        }

        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
        {
            ImGui::End();
            return;
        }

        if (m_SelectedEntity == 0)
        {
            ImGui::Text("No entity selected.");
            ImGui::End();
            return;
        }

        if (!ECS::GetInstance().HasComponent<StateMachine>(m_SelectedEntity))
        {
            ImGui::Text("Entity has no StateMachine component.");
            ImGui::End();
            return;
        }

        auto& fsm = ECS::GetInstance().GetComponent<StateMachine>(m_SelectedEntity);

        ImGui::InputText("New Node Name", m_newNodeName, IM_ARRAYSIZE(m_newNodeName));
        ImGui::SameLine();
        if (ImGui::Button("Add Node"))
        {
            if (strlen(m_newNodeName) > 0)
            {
                CreateNode(m_newNodeName);
                m_newNodeName[0] = '\0';
            }
        }
        ImGui::Separator();

        // Draw editor nodes and links
        ImNodes::BeginNodeEditor();

        for (auto& nodePtr : fsm.m_Nodes)
        {
            auto& snode = *nodePtr;
            ImNodes::BeginNode(snode.id);

            // Restore saved position once
            if (!snode.positionInitialized)
            {
#if defined(EE_EDITOR)
                ImNodes::SetNodeGridSpacePos(snode.id, snode.editorPosition);
#endif
                snode.positionInitialized = true;
            }

            ImNodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(snode.name.c_str());
            ImNodes::EndNodeTitleBar();

            // Attach script UI
            if (snode.isAttached)
            {
                ImGui::Text("Script: %s", snode.scriptClassName.c_str());
                ImGui::SameLine();

                if (ImGui::Button(("Remove##" + std::to_string(snode.id)).c_str()))
                    ImGui::OpenPopup(("ConfirmRemoveScript" + std::to_string(snode.id)).c_str());

                if (ImGui::BeginPopup(("ConfirmRemoveScript" + std::to_string(snode.id)).c_str()))
                {
                    ImGui::Text("Remove attached script?");
                    if (ImGui::Button("Yes"))
                    {
                        nodesToDetachScript.push_back(snode.id);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                        ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            else
            {
                if (ImGui::Button(("Attach Script##" + std::to_string(snode.id)).c_str()))
                    ImGui::OpenPopup(("AttachScriptPopup" + std::to_string(snode.id)).c_str());
            }

            if (ImGui::BeginPopup(("AttachScriptPopup" + std::to_string(snode.id)).c_str()))
            {
                static char scriptName[128] = "";
                ImGui::InputText("Class Name", scriptName, IM_ARRAYSIZE(scriptName));
                if (ImGui::Button("Confirm"))
                {
                    snode.isAttached = true;
                    snode.scriptClassName = scriptName;

                    if (!snode.scriptClassName.empty())
                    {
                        // Create instance immediately
                        snode.CreateInstance(m_SelectedEntity);
                        //EE_CORE_INFO("FSMEditor: Attached script '%s' to node '%s' (entity %d)",
                        //    snode.scriptClassName.c_str(), snode.name.c_str(), m_SelectedEntity);
                    }

                    ImGui::CloseCurrentPopup();
                }

                auto fsmManager = ECS::GetInstance().GetSystem<StateManager>();
                if (fsmManager)
                {
                    fsmManager->Init(m_SelectedEntity, nullptr);
                    //EE_CORE_INFO("FSMEditor: Ensured FSM manager assigned for entity %d", m_SelectedEntity);
                }
                else
                {
                    EE_CORE_WARN("FSMEditor: No StateManager system found when attaching script!");
                }
                ImGui::EndPopup();
            }

            // Delete node button
            if (ImGui::Button(("Delete Node##" + std::to_string(snode.id)).c_str()))
            {
                ImGui::OpenPopup(("ConfirmDeleteNode" + std::to_string(snode.id)).c_str());
            }

            if (ImGui::BeginPopup(("ConfirmDeleteNode" + std::to_string(snode.id)).c_str()))
            {
                ImGui::Text("Delete node '%s'?", snode.name.c_str());
                if (ImGui::Button("Yes"))
                {
                    nodesToDelete.push_back(snode.id);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            bool wasStart = snode.isStartNode;
            ImGui::Checkbox(("Start Node##" + std::to_string(snode.id)).c_str(), &snode.isStartNode);

            // Ensure only one node is marked as start at a time
            if (snode.isStartNode && !wasStart)
            {
                for (auto& otherNodePtr : fsm.m_Nodes)
                {
                    if (otherNodePtr->id != snode.id)
                        otherNodePtr->isStartNode = false;
                }

                //EE_CORE_INFO("FSMEditor: Node %d set as Start Node", snode.id);
            }

            ImNodes::BeginInputAttribute(snode.id * 10 + 1);
            ImGui::Text("In");
            ImNodes::EndInputAttribute();

            ImNodes::BeginOutputAttribute(snode.id * 10);
            ImGui::Text("Out");
            ImNodes::EndOutputAttribute();

            ImNodes::EndNode();
        }

        int linkId = 1;
        for (auto& link : fsm.m_Links)
            ImNodes::Link(linkId++, link.first, link.second);

        ImNodes::EndNodeEditor();

        // save current position
        for (auto& nodePtr : fsm.m_Nodes)
        {
            auto& snode = *nodePtr;
            snode.editorPosition = ImNodes::GetNodeGridSpacePos(snode.id);
        }

        if (!nodesToDetachScript.empty())
        {
            for (int id : nodesToDetachScript)
            {
                for (auto& n : fsm.m_Nodes)
                {
                    if (n->id == id)
                    {
                        if (n->instance) n->instance.reset();
                        n->scriptClassName.clear();
                        n->isAttached = false;
                        break;
                    }
                }
            }
            nodesToDetachScript.clear();
        }

        if (!nodesToDelete.empty())
        {
            for (int deleteId : nodesToDelete)
            {
                // If the FSM is currently using this node, reset it
                if (fsm.m_CurrentScript && fsm.m_CurrentScript->id == deleteId)
                {
                    //EE_CORE_INFO("FSMEditor: Current active node (%d) deleted, resetting FSM state.", deleteId);

                    fsm.m_CurrentScript = nullptr;

                    // pick a new start node automatically
                    for (auto& nodePtr : fsm.m_Nodes)
                    {
                        if (nodePtr->isStartNode)
                        {
                            fsm.m_CurrentScript = nodePtr.get();
                            //EE_CORE_INFO("FSMEditor: Reassigned to new start node: %s (id=%d)",
                            //    nodePtr->name.c_str(), nodePtr->id);
                            break;
                        }
                    }

                    // If none are marked as start, fallback to first node
                    if (!fsm.m_CurrentScript && !fsm.m_Nodes.empty())
                    {
                        fsm.m_CurrentScript = fsm.m_Nodes.front().get();
                        fsm.m_CurrentScript->isStartNode = true;
                        //EE_CORE_INFO("FSMEditor: Fallback start node assigned: %s (id=%d)",
                        //    fsm.m_CurrentScript->name.c_str(), fsm.m_CurrentScript->id);
                    }
                }

                // Remove all links referencing this node
                fsm.m_Links.erase(std::remove_if(fsm.m_Links.begin(), fsm.m_Links.end(),
                    [&](const std::pair<int, int>& link)
                    {
                        int fromId = link.first / 10;
                        int toId = (link.second - 1) / 10;
                        return fromId == deleteId || toId == deleteId;
                    }),
                    fsm.m_Links.end());

                // Remove transitions referencing this node
                for (auto it = fsm.scriptTransitions.begin(); it != fsm.scriptTransitions.end();)
                {
                    int fromId = it->first;
                    int toId = it->second;

                    if (fromId == deleteId || toId == deleteId)
                        it = fsm.scriptTransitions.erase(it);
                    else
                        ++it;
                }

                // erase the node
                fsm.m_Nodes.erase(std::remove_if(fsm.m_Nodes.begin(), fsm.m_Nodes.end(),
                    [&](const std::shared_ptr<ScriptNode>& n) { return n->id == deleteId; }),
                    fsm.m_Nodes.end());

                //EE_CORE_INFO("FSMEditor: Node %d deleted", deleteId);
            }

            nodesToDelete.clear();
        }

        // Handle new link creation
        int startAttr, endAttr;
        if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
        {
            auto is_output = [](int attr) { return (attr % 10) == 0; };
            auto is_input = [](int attr) { return (attr % 10) == 1; };

            int fromAttr = startAttr;
            int toAttr = endAttr;
            if (is_input(fromAttr) && is_output(toAttr))
                std::swap(fromAttr, toAttr);

            int fromId = fromAttr / 10;
            int toId = (toAttr - 1) / 10;

            ScriptNode* fromScriptNode = nullptr;
            ScriptNode* toScriptNode = nullptr;

            for (auto& sPtr : fsm.m_Nodes)
            {
                if (sPtr->id == fromId) fromScriptNode = sPtr.get();
                if (sPtr->id == toId)   toScriptNode = sPtr.get();
            }

            if (fromId != 0 && toId != 0)
                fsm.scriptTransitions[fromId] = toId;

            fsm.m_Links.emplace_back(fromAttr, toAttr);
        }

        ImGui::End();
    }
}