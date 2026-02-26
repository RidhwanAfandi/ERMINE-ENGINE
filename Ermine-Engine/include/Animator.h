/* Start Header ************************************************************************/
/*!
\file       Animator.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       28/10/2025
\brief      This file contains the declaration of the animator class. It is responsible for
            loading animation clips from an Assimp scene, managing playback state,
            updating bone transforms per frame, providing final matrices for GPU skinning
            in the renderer.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "AnimationClip.h"   // For AnimationClip definition
#include "Model.h"           // For Model definition
#include "ECS.h"
#include <assimp/scene.h>    // For aiScene

namespace Ermine::graphics
{
    /**
     * @brief Controls playback of skeletal animations for a model.
     *
     * The Animator class manages animation clips loaded from Assimp,
     * updates playback state (play, pause, resume, stop),
     * calculates bone transformations each frame, and
     * provides final matrices to the renderer for GPU skinning.
     */
    class Animator {
    public:
        /**
         * @brief Construct an Animator for a given model.
         * @param model The skinned model to animate.
         */
        explicit Animator(std::shared_ptr<Model> model);

        /**
         * @brief Load all animations from the model's Assimp scene.
         *
         * Converts each Assimp animation in the scene into an @ref AnimationClip
         * and stores them internally for playback.
         */
        void LoadAnimations();

        /**
         * @brief Convert a single Assimp animation into an AnimationClip.
         *
         * @param anim Pointer to the Assimp animation to convert.
         * @return A populated AnimationClip containing channels, keyframes, and metadata.
         */
        static AnimationClip LoadAnimation(const aiAnimation* anim);
        
        /**
         * @brief Clear all loaded animations.
         *
         * Removes all cached animation clips, current clip reference, and resets state.
         */
        void ClearAnimations();

        /**
         * @brief Play an animation clip by index.
         *
         * Resets the playback time and sets the current animation to the selected clip.
         *
         * @param index The index of the clip in the internal clips list.
         * @param loop Whether the animation should loop after finishing.
         */
        void PlayAnimation(size_t index, bool loop = true);

        /**
         * @brief Play an animation clip by name.
         *
         * Searches for a clip with the given name and begins playback if found.
         *
         * @param name The name of the animation clip.
         * @param loop Whether the animation should loop after finishing.
         */
        void PlayAnimation(const std::string& name, bool loop = true);
        
        /**
         * @brief Stop the current animation.
         *
         * Clears the current clip, resets playback time to 0, and disables updates.
         */
        void StopAnimation();
        
        /**
         * @brief Pause the current animation.
         *
         * Freezes playback at the current time without resetting state.
         * Does nothing if no animation is playing.
         */
        void PauseAnimation();

        /**
         * @brief Resume playback of the current animation if paused.
         *
         * Does nothing if the animator is not paused or no animation is active.
         */
        void ResumeAnimation();

        /**
         * @brief Seek the current animation to a specific time position (in seconds).
         *
         * Recalculates all bone transforms at that time without advancing playback.
         *
         * @param timeInSeconds Target playback time in seconds.
         */
        void Seek(double timeInSeconds);

        /**
		 * @brief Evaluate and process animation transitions in the graph.
         *
		 * Checks for any transition conditions in the linked AnimationGraph
		 * and switches animations accordingly.
		 */
        void EvaluateTransitions(EntityID entity);

        /**
         * @brief Advance animation playback and update bone transforms.
         *
         * Increments the playback timer by @p deltaTime and updates the skeleton's bone transforms
         * according to the current animation clip. Updates the final bone matrices for rendering.
         *
         * @param deltaTime Time step in seconds since last frame.
         */
        void Update(double deltaTime, EntityID entity);

        /**
         * @brief Get the animated model.
         *
         * @return Shared pointer to the bound Model.
         */
        const std::shared_ptr<Model>& GetModel() const { return m_Model; }
        
        /**
         * @brief Get all loaded animation clips.
         *
         * @return Const reference to the vector of AnimationClips.
         */
        const std::vector<AnimationClip>& GetClips() const { return m_Clips; }

        /**
         * @brief Get the currently playing animation clip.
         *
         * @return Pointer to the current AnimationClip, or nullptr if none is playing.
         */
        const AnimationClip* GetCurrentClip() const { return m_CurrentClip; }

        /**
         * @brief Get or set the looping state of the current animation.
         *
         * @return Reference to the looping flag. True if looping is enabled.
         */
        bool& IsLooping() { return m_Loop; }

        /**
         * @brief Check if the current animation is paused.
         *
         * @return True if paused, false otherwise.
         */
        bool IsPaused() const { return m_Paused; }
        
        /**
         * @brief Get the final bone transformation matrices for GPU skinning.
         *
         * These matrices should be uploaded to the shader for skeletal animation rendering.
         *
         * @return Const reference to the vector of final bone transforms.
         */
        const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

    private:
        std::shared_ptr<Model> m_Model;               // The model to animate
        const aiScene* m_Scene = nullptr;             // Cached Assimp scene
        
        std::vector<AnimationClip> m_Clips;           // All animation clips
        const AnimationClip* m_CurrentClip = nullptr; // Currently playing clip

        double m_CurrentTime = 0.0;                   // Current time in ticks
        bool m_Loop = true;                           // Looping flag
        bool m_Paused = false;                        // Pausing flag

        std::vector<glm::mat4> m_FinalBoneMatrices;   // Final transforms per bone

        /**
         * @brief Recursively calculate bone transforms by traversing Assimp node hierarchy.
         *
         * Applies animations to nodes and accumulates parent transformations.
         *
         * @param node Current node being processed.
         * @param parentTransform The accumulated transform of the parent node.
         */
        void CalculateBoneTransform(const aiNode* node, const glm::mat4& parentTransform);
    };
}
