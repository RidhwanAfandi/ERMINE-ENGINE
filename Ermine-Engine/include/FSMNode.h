/* Start Header ************************************************************************/
/*!
\file       FSMNode.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       21/10/2025
\brief      This file defines the ScriptNode structure, which represents an individual
            nodefor Finite State Machine. Each node corresponds to a scriptable state 
            through a C# script.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <string>

namespace Ermine
{
    struct ScriptNode
    {
        int id = 0;
        std::string name;
        std::string scriptClassName;
        bool isAttached = false;
        bool isStartNode = false;

        std::unique_ptr<scripting::ScriptInstance> instance;

        ImVec2 editorPosition = ImVec2(100.0f, 100.0f);
        bool positionInitialized = false;

        // Default constructor
        ScriptNode() = default;

        // Move constructor / assignment
        ScriptNode(ScriptNode&&) noexcept = default;
        ScriptNode& operator=(ScriptNode&&) noexcept = default;

        // Delete copy
        ScriptNode(const ScriptNode&) = delete;
        ScriptNode& operator=(const ScriptNode&) = delete;

        /*!***********************************************************************
        \brief
         Creates and initializes a script instance for this node's associated script class.
        \param[in] entity
         The entity ID that this script instance will be attached to.
        \return
         None.
        *************************************************************************/
        void CreateInstance(EntityID entity)
        {
            //EE_CORE_INFO("ScriptNode::CreateInstance(%d): attached=%d, class=%s", entity, isAttached, scriptClassName.c_str());

            if (!isAttached || scriptClassName.empty()) return;
            auto sc = std::make_unique<scripting::ScriptClass>("", scriptClassName);
            instance = std::make_unique<scripting::ScriptInstance>(std::move(sc), entity);

            //EE_CORE_INFO("ScriptNode::CreateInstance: instance created");
        }
        /*!***********************************************************************
        \brief
         Invoked when this node becomes the active state. Calls the Start()
         method of the attached script instance, if it exists.
        \return
         None.
        *************************************************************************/
        void OnEnter() { if (instance) instance->Start(); }
        /*!***********************************************************************
        \brief
         Called every frame while this node remains active.
         Executes the Update() method of the attached script instance.
        \return
         None.
        *************************************************************************/
        void OnUpdate() { if (instance) instance->Update(); }
        /*!***********************************************************************
        \brief
         Invoked when this node is exited or deactivated.
         Calls the OnDisable() method of the attached script instance.
        \return
         None.
        *************************************************************************/
        void OnExit() { if (instance) instance->OnDisable(); }
    };
}
