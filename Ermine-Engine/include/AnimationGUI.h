/* Start Header ************************************************************************/
/*!
\file       AnimationGUI.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       28/02/2026
\brief      This file contains the declaration of the animation editor GUI.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "ImGuiUIWindow.h"
#include "imgui.h"
#include "imnodes.h"
#include "ECS.h"
#include <deque>
#include <vector>
#include <memory>
#include <string>

namespace Ermine::graphics { class Animator; } // Forward declaration

namespace Ermine
{
    /**
     * @brief Represents a state in the animation state machine.
     */
    struct AnimationStateNode
    {
        int id = 0;                // Unique identifier for the state node
        std::string name;          // Name of the animation state
        std::string clipName;      // Name of the associated animation clip

        bool isStartState = false; // Indicates if this is the start state
        bool isAttached = false;   // Indicates if the state is attached to the graph

        float speed = 1.0f;        // Playback speed for the animation
        float blendWeight = 1.0f;  // Blend weight for transitions

        bool loop = true;          // Whether the animation should loop

        ImVec2 editorPos = { 100.f, 100.f }; // default placement in editor space
    };

    /**
     * @brief Represents a link between two animation states for ImNodes rendering.
     */
    struct AnimationLink
    {
        int id;         // Unique identifier for the link
        int fromNodeId; // Source node ID
        int toNodeId;   // Destination node ID
    };

    /**
     * @brief Represents a condition for transitioning between animation states.
     */
    struct AnimationCondition
    {
        std::string parameterName;     // which parameter it uses
        std::string comparison = "=="; // "==", "!=", ">", "<", ">=", "<="
        float threshold = 0.0f;        // used for float/int comparison
        bool boolValue = false;        // used if the param is a bool
    };

    /**
     * @brief Represents a transition between two animation states.
     */
    struct AnimationTransition
    {
        int fromNodeId = 0;                         // ID of the source state node
        int toNodeId = 0;                           // ID of the destination state node
        float exitTime = 0.0f;                      // Normalized time to exit the source state
        float duration = 0.25f;                     // Duration of the transition
        std::vector<AnimationCondition> conditions; // Conditions for the transition
    };

    /**
     * @brief Represents a parameter used in the animation state machine.
     */
    struct AnimationParameter
    {
        enum class Type { Bool, Float, Int, Trigger }; // Parameter types
        std::string name;                              // Name of the parameter
        Type type = Type::Bool;                        // Type of the parameter
        bool boolValue = false;                        // Default value for bool type
        float floatValue = 0.0f;                       // Default value for float type
        int intValue = 0;                              // Default value for int type
        bool triggerValue = false;                     // Default value for trigger type
    };

    /**
     * @brief Represents the entire animation graph for an entity.
     */
    struct AnimationGraph
    {
        std::deque<std::shared_ptr<AnimationStateNode>> states; // List of animation states
        std::vector<AnimationLink> links;                       // List of links for ImNodes rendering
        std::vector<AnimationTransition> transitions;		    // List of transitions between states
        std::vector<AnimationParameter> parameters;             // List of animation parameters

        std::shared_ptr<AnimationStateNode> current;            // Currently active state
        float currentTime = 0.0f;		                        // Current time within the active state's animation
        bool playing = false;			                        // Indicates if the animation is currently playing
        float playbackSpeed = 1.0f;	                            // Global playback speed modifier
    };

    /**
     * @class AnimationEditorImGUI
     * @brief ImGui window for editing animation state machines.
     *
     */
    class AnimationEditorImGUI : public ImGUIWindow
    {
    public:
        /**
         * @brief Constructor for AnimationEditorImGUI.
         */
        AnimationEditorImGUI() : ImGUIWindow("Animation Editor") {}

        /**
         * @brief Destructor for AnimationEditorImGUI.
         */
        ~AnimationEditorImGUI() override = default;

        /**
         * @brief Renders the animation editor UI.
         */
        void Render() override;

        /**
         * @brief Sets the currently selected entity for editing.
         * @param entity The entity ID to select.
         */
        void SetSelectedEntity(EntityID entity) { m_SelectedEntity = entity; }

    private:
        EntityID m_SelectedEntity = 0;  // Currently selected entity
        int m_nextNodeId = 1;           // Next available node ID
        char m_newStateName[64] = "";   // Buffer for new state name input
        std::vector<int> nodesToDelete; // Nodes marked for deletion

        // Rename popup state
        bool m_isRenaming = false;      // Whether we are currently renaming a node inline
        int m_renameNodeId = -1;        // Node ID being renamed
        char m_renameBuffer[128] = "";  // Buffer for rename input

        /**
         * @brief Creates a new animation state with the given name.
         * @param name The name of the new state.
         */
        void CreateState(const std::string& name);

        /**
         * @brief Find state pointer by id.
         * @param graph The animation graph to search.
         * @param id The ID of the state to find.
         * @return Shared pointer to the found AnimationStateNode, or nullptr if not found.
         */
        std::shared_ptr<AnimationStateNode> FindStateById(const std::shared_ptr<AnimationGraph>& graph, int id);

        /**
         * @brief Draws the node editor for the animation graph.
         * @param graph The animation graph being edited.
         * @param animator The animator associated with the entity.
         */
        void DrawNodeEditor(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator);

        /**
         * @brief Draws a single animation state node.
         * @param n The animation state node to draw.
         * @param graph The animation graph being edited.
         * @param animator The animator associated with the entity.
         */
        void DrawNode(AnimationStateNode& n, const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator);

        /**
         * @brief Draws the information inspector panel.
         * @param graph The animation graph being edited.
         * @param animator The animator associated with the entity.
         */
        void DrawInformationInspector(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator);

        /**
         * @brief Draws the state inspector panel.
         * @param graph The animation graph being edited.
         * @param animator The animator associated with the entity.
         */
        void DrawStateInspector(const std::shared_ptr<AnimationGraph>& graph, const std::shared_ptr<graphics::Animator>& animator);

        /**
         * @brief Draws the parameter inspector panel.
         * @param graph The animation graph being edited.
         */
        void DrawParameterInspector(const std::shared_ptr<AnimationGraph>& graph);

        /**
         * @brief Draws the transition inspector panel.
         * @param graph The animation graph being edited.
         */
        void DrawTransitionInspector(const std::shared_ptr<AnimationGraph>& graph);
    };
}
