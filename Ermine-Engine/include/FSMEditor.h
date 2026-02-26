/* Start Header ************************************************************************/
/*!
\file       FSMEditor.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       06/10/2025
\brief      This file contains declarations for imgui window FSM editor.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "ImGuiUIWindow.h"
#include "FiniteStateMachine.h"
#include "ECS.h"
#include "imgui.h"
#include "imnodes.h"
#include "FSMNode.h"
#include <deque>

namespace Ermine
{
    class FSMEditorImGUI : public ImGUIWindow
    {
    public:
        FSMEditorImGUI() : ImGUIWindow("FSM Editor") {}
        ~FSMEditorImGUI() override = default;
        void Update() override {}
        /*!***********************************************************************
        \brief
         Renders the Finite State Machine editor window using ImGui and ImNodes.
         Displays existing nodes, allows users to attach or remove scripts, and
         create transitions between states.
        \return
         None.
        *************************************************************************/
        void Render() override;
        /*!***********************************************************************
        \brief
         Sets the currently selected entity whose FSM will be edited.
        \param[in] entity
         The ID of the entity whose state machine will be displayed and modified
         in the FSM editor.
        \return
         None.
        *************************************************************************/
        void SetSelectedEntity(EntityID entity) { m_SelectedEntity = entity; }
        /*!***********************************************************************
        \brief
         Retrieves the list of ScriptNode objects associated with the given entity.
         If the entity does not already have a StateMachine component, one will be
         created automatically.
        \param[in] entity
         The entity ID whose FSM nodes should be retrieved or initialized.
        \return
         A reference to the deque of shared pointers to ScriptNode instances.
        *************************************************************************/
        std::deque<std::shared_ptr<ScriptNode>>& GetNodesForEntity(EntityID entity);
        /*!***********************************************************************
        \brief
         Retrieves a read-only reference to the list of ScriptNode objects
         associated with the given entity.
        \param[in] entity
         The entity ID whose FSM nodes should be accessed.
        \return
         A constant reference to the deque of shared pointers to ScriptNode instances.
        *************************************************************************/
        const std::deque<std::shared_ptr<ScriptNode>>& GetNodesForEntity(EntityID entity) const;

    private:
        EntityID m_SelectedEntity = 0;
        int m_nextNodeId = 1;
        char m_newNodeName[64] = "";
        std::vector<int> nodesToDetachScript;
        std::vector<int> nodesToDelete;

        /*!***********************************************************************
        \brief
         Creates a new ScriptNode with the specified name and adds it to the
         currently selected entity’s Finite State Machine.
        \param[in] name
         The name to assign to the newly created node.
        \return
         None.
        *************************************************************************/
        void CreateNode(const std::string& name);
    };
}
