/* Start Header ************************************************************************/
/*!
\file       FiniteStateMachine.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu
\date       16/09/2025
\brief      This file contains declarations for Finite State Machine.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "Entity.h"
#include <unordered_map>
#include "ECS.h"
#include "Components.h"

namespace Ermine
{
    // State Manager
    class StateManager : public System
    {
    public:
        /*!***********************************************************************
        \brief
            Initializes a finite state machine for the specified entity and sets
            its starting script node.
        \param[in] entity
            The entity ID whose Finite State Machine should be initialized.
        \param[in] startScript
            Pointer to the script node that represents the FSM’s starting state.
            Can be nullptr if the FSM will determine its start node later.
        \return
            None.
        *************************************************************************/
        void Init(EntityID entity, ScriptNode* startScript);
        /*!***********************************************************************
        \brief
            Updates all managed finite state machines.
            Iterates through every entity registered with a StateMachine component
            and calls its update logic each frame.
        \param[in] dt
            The delta time since the last frame, used to drive state updates.
        \return
            None.
        *************************************************************************/
        void Update(float dt);
        /*!***********************************************************************
        \brief
            Requests a transition from the entity’s current state to the next
            state as defined in its transition map.
            Calls OnExit() on the current script, switches to the next script node,
            and then calls OnEnter() on the new state.
        \param[in] entity
            The entity ID whose FSM should transition to its next state.
        \return
            None.
        *************************************************************************/
        void RequestNextState(EntityID entity);
        /*!***********************************************************************
        \brief
            Requests a transition from the entity’s current state back to its
            previously active state.
            Calls OnExit() on the current script, switches to the previous node,
            and then reinitializes and enters that state.
        \param[in] entity
            The entity ID whose FSM should revert to its previous state.
        \return
            None.
        *************************************************************************/
        void RequestPreviousState(EntityID entity);
    };
}