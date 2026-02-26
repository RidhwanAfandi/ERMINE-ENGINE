/* Start Header ************************************************************************/
/*!
\file       Animator.cpp
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\co-author  Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       28/10/2025
\brief      This file contains the definition of the animator class. It is responsible for
            loading animation clips from an Assimp scene, managing playback state,
            updating bone transforms per frame, providing final matrices for GPU skinning
            in the renderer.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Animator.h"
#include "Logger.h"
#include "Components.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm> // for std::upper_bound
#include <glm/gtx/quaternion.hpp>

namespace Ermine::graphics
{
    /**
     * @brief Construct an Animator for a given model.
     * @param model The skinned model to animate.
     */
    Animator::Animator(std::shared_ptr<Model> model) : m_Model(std::move(model))
    {
        EE_CORE_WARN("Animator::Constructor - Model ptr: {}, Model name: '{}'",
                     (void*)m_Model.get(), m_Model->GetName());

        m_Scene = m_Model->GetAssimpScene(); // cache scene for hierarchy traversal

        EE_CORE_WARN("Animator::Constructor - m_Scene: {}, mRootNode: {}",
                     (void*)m_Scene, (m_Scene ? (void*)m_Scene->mRootNode : nullptr));

        m_FinalBoneMatrices.resize(m_Model->GetBoneCount(), glm::mat4(1.0f));

        LoadAnimations(); // Load all animation clips
    }

    /**
     * @brief Load all animations from the model's Assimp scene.
     *
     * Converts each Assimp animation in the scene into an @ref AnimationClip
     * and stores them internally for playback.
     */
    void Animator::LoadAnimations()
    {
        m_Clips.clear();
        if (!m_Scene) return;

        for (unsigned int i = 0; i < m_Scene->mNumAnimations; ++i)
        {
            m_Clips.push_back(LoadAnimation(m_Scene->mAnimations[i]));
            EE_CORE_INFO("Animation clip loaded: {0}", m_Scene->mAnimations[i]->mName.C_Str());
        }
    }

    /**
     * @brief Convert a single Assimp animation into an AnimationClip.
     *
     * @param anim Pointer to the Assimp animation to convert.
     * @return A populated AnimationClip containing channels, keyframes, and metadata.
     */
    AnimationClip Animator::LoadAnimation(const aiAnimation* anim)
    {
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = anim->mDuration;
        clip.ticksPerSecond = (anim->mTicksPerSecond != 0.0)
            ? anim->mTicksPerSecond : 25.0;

        for (unsigned int i = 0; i < anim->mNumChannels; ++i)
        {
            aiNodeAnim* channel = anim->mChannels[i];
            BoneAnimation boneAnim;
            boneAnim.boneName = channel->mNodeName.C_Str();

            // Collect all unique times
            std::vector<double> times;
            for (unsigned int j = 0; j < channel->mNumPositionKeys; ++j)
                times.push_back(channel->mPositionKeys[j].mTime);
            for (unsigned int j = 0; j < channel->mNumRotationKeys; ++j)
                times.push_back(channel->mRotationKeys[j].mTime);
            for (unsigned int j = 0; j < channel->mNumScalingKeys; ++j)
                times.push_back(channel->mScalingKeys[j].mTime);

            std::sort(times.begin(), times.end());
            times.erase(std::unique(times.begin(), times.end()), times.end());

            // Build merged keyframes
            for (double t : times)
            {
                Keyframe kf;
                kf.time = t;

                // Position interpolation using binary search
                if (channel->mNumPositionKeys > 0) {
                    // Use upper_bound to find the next keyframe after time t
                    auto it = std::upper_bound(
                        channel->mPositionKeys,
                        channel->mPositionKeys + channel->mNumPositionKeys,
                        t,
                        [](double time, const aiVectorKey& key) { return time < key.mTime; }
                    );

                    if (it == channel->mPositionKeys) {
                        // t is before first keyframe, use first keyframe
                        kf.position = glm::vec3(
                            channel->mPositionKeys[0].mValue.x,
                            channel->mPositionKeys[0].mValue.y,
                            channel->mPositionKeys[0].mValue.z
                        );
                    }
                    else if (it == channel->mPositionKeys + channel->mNumPositionKeys) {
                        // t is after last keyframe, use last keyframe
                        auto& lastKey = channel->mPositionKeys[channel->mNumPositionKeys - 1];
                        kf.position = glm::vec3(lastKey.mValue.x, lastKey.mValue.y, lastKey.mValue.z);
                    }
                    else {
                        // Interpolate between previous and current keyframe
                        auto& nextKey = *it;
                        auto& prevKey = *(it - 1);

                        double dt = nextKey.mTime - prevKey.mTime;
                        double factor = (dt > 0.0) ? (t - prevKey.mTime) / dt : 0.0;

                        aiVector3D pos = prevKey.mValue + static_cast<float>(factor) * (nextKey.mValue - prevKey.mValue);
                        kf.position = glm::vec3(pos.x, pos.y, pos.z);
                    }
                }
                else {
                    kf.position = glm::vec3(0.0f);
                }

                // Rotation interpolation using binary search
                if (channel->mNumRotationKeys > 0) {
                    auto it = std::upper_bound(
                        channel->mRotationKeys,
                        channel->mRotationKeys + channel->mNumRotationKeys,
                        t,
                        [](double time, const aiQuatKey& key) { return time < key.mTime; }
                    );

                    if (it == channel->mRotationKeys) {
                        auto& firstKey = channel->mRotationKeys[0];
                        kf.rotation = glm::quat(firstKey.mValue.w, firstKey.mValue.x, firstKey.mValue.y, firstKey.mValue.z);
                    }
                    else if (it == channel->mRotationKeys + channel->mNumRotationKeys) {
                        auto& lastKey = channel->mRotationKeys[channel->mNumRotationKeys - 1];
                        kf.rotation = glm::quat(lastKey.mValue.w, lastKey.mValue.x, lastKey.mValue.y, lastKey.mValue.z);
                    }
                    else {
                        auto& nextKey = *it;
                        auto& prevKey = *(it - 1);

                        double dt = nextKey.mTime - prevKey.mTime;
                        double factor = (dt > 0.0) ? (t - prevKey.mTime) / dt : 0.0;

                        aiQuaternion result;
                        aiQuaternion::Interpolate(result, prevKey.mValue, nextKey.mValue, static_cast<float>(factor));
                        result.Normalize();
                        kf.rotation = glm::quat(result.w, result.x, result.y, result.z);
                    }
                }
                else {
                    kf.rotation = glm::quat(1, 0, 0, 0);
                }

                // Scaling interpolation using binary search
                if (channel->mNumScalingKeys > 0) {
                    auto it = std::upper_bound(
                        channel->mScalingKeys,
                        channel->mScalingKeys + channel->mNumScalingKeys,
                        t,
                        [](double time, const aiVectorKey& key) { return time < key.mTime; }
                    );

                    if (it == channel->mScalingKeys) {
                        auto& firstKey = channel->mScalingKeys[0];
                        kf.scale = glm::vec3(firstKey.mValue.x, firstKey.mValue.y, firstKey.mValue.z);
                    }
                    else if (it == channel->mScalingKeys + channel->mNumScalingKeys) {
                        auto& lastKey = channel->mScalingKeys[channel->mNumScalingKeys - 1];
                        kf.scale = glm::vec3(lastKey.mValue.x, lastKey.mValue.y, lastKey.mValue.z);
                    }
                    else {
                        auto& nextKey = *it;
                        auto& prevKey = *(it - 1);

                        double dt = nextKey.mTime - prevKey.mTime;
                        double factor = (dt > 0.0) ? (t - prevKey.mTime) / dt : 0.0;

                        aiVector3D scl = prevKey.mValue + static_cast<float>(factor) * (nextKey.mValue - prevKey.mValue);
                        kf.scale = glm::vec3(scl.x, scl.y, scl.z);
                    }
                }
                else {
                    kf.scale = glm::vec3(1.0f);
                }

                boneAnim.keys.push_back(std::move(kf));
            }

            clip.boneAnimations[boneAnim.boneName] = std::move(boneAnim);
        }

        return clip;
    }

    /**
     * @brief Clear all loaded animations.
     *
     * Removes all cached animation clips, current clip reference, and resets state.
     */
    void Animator::ClearAnimations()
    {
        m_Clips.clear();
        m_CurrentClip = nullptr;
        m_CurrentTime = 0.0;
        m_Paused = false;
    }

    /**
     * @brief Play an animation clip by index.
     *
     * Resets the playback time and sets the current animation to the selected clip.
     *
     * @param index The index of the clip in the internal clips list.
     * @param loop Whether the animation should loop after finishing.
     */
    void Animator::PlayAnimation(size_t index, bool loop)
    {
        if (index >= m_Clips.size()) return;
        m_CurrentClip = &m_Clips[index];
        m_CurrentTime = 0.0;
        m_Loop = loop;
        EE_CORE_INFO("Animation clip played: {0}", m_CurrentClip->name);
    }

    /**
     * @brief Play an animation clip by name.
     *
     * Searches for a clip with the given name and begins playback if found.
     *
     * @param name The name of the animation clip.
     * @param loop Whether the animation should loop after finishing.
     */
    void Animator::PlayAnimation(const std::string& name, bool loop)
    {
        for (auto& clip : m_Clips) {
            if (clip.name == name) {
                m_CurrentClip = &clip;
                m_CurrentTime = 0.0;
                m_Loop = loop;
                EE_CORE_INFO("Animation clip played: {0}", clip.name);
                return;
            }
        }
    }

    /**
     * @brief Stop the current animation.
     *
     * Clears the current clip, resets playback time to 0, and disables updates.
     */
    void Animator::StopAnimation()
    {
        m_CurrentClip = nullptr;
        m_CurrentTime = 0.0;
        m_Paused = false;
        EE_CORE_INFO("Animation clip stopped");
    }

    /**
     * @brief Pause the current animation.
     *
     * Freezes playback at the current time without resetting state.
     * Does nothing if no animation is playing.
     */
    void Animator::PauseAnimation()
    {
        if (m_CurrentClip)
        {
            m_Paused = true;
            EE_CORE_INFO("Animation clip paused");
        }
    }

    /**
     * @brief Resume playback of the current animation if paused.
     *
     * Does nothing if the animator is not paused or no animation is active.
     */
    void Animator::ResumeAnimation()
    {
        if (m_CurrentClip && m_Paused)
        {
            m_Paused = false;
            EE_CORE_INFO("Animation clip resumed");
        }
    }

    /**
     * @brief Seek the current animation to a specific time position (in seconds).
     *
     * Recalculates all bone transforms at that time without advancing playback.
     *
     * @param timeInSeconds Target playback time in seconds.
     */
    void Animator::Seek(double timeInSeconds)
    {
        // Ensure there is a current clip and model
        if (!m_CurrentClip || !m_Model)
            return;

        // Convert seconds to ticks based on the clip's ticks-per-second
        double ticksPerSecond = m_CurrentClip->ticksPerSecond != 0.0
            ? m_CurrentClip->ticksPerSecond
            : 25.0;
        m_CurrentTime = fmod(timeInSeconds * ticksPerSecond, m_CurrentClip->duration);

        // Recalculate bone transforms at this exact time
        m_FinalBoneMatrices.assign(m_Model->GetBoneCount(), glm::mat4(1.0f));

        if (const aiNode* rootNode = m_Model->GetAssimpScene()->mRootNode)
            CalculateBoneTransform(rootNode, glm::mat4(1.0f));
    }

    /**
     * @brief Evaluate and process animation transitions in the graph.
     *
     * Checks for any transition conditions in the linked AnimationGraph
     * and switches animations accordingly.
     */
    void Animator::EvaluateTransitions(EntityID entity)
    {
        if (!ECS::GetInstance().HasComponent<AnimationComponent>(entity)) return;

        auto& graph = ECS::GetInstance().GetComponent<AnimationComponent>(entity).m_animationGraph;

        // No graph or no current state
        if (!graph || !graph->current) return;

        for (auto& transition : graph->transitions) {
            // Match transition source node
            if (transition.fromNodeId != graph->current->id)
                continue;

            // Evaluate all conditions for this transition
            bool allTrue = true;
            for (auto& cond : transition.conditions) {
                auto it = std::find_if(graph->parameters.begin(), graph->parameters.end(),
                    [&](const AnimationParameter& p) { return p.name == cond.parameterName; });

                // Parameter missing
                if (it == graph->parameters.end()) {
                    allTrue = false;
                    break;
                }

                const auto& param = *it;
                bool result = false;

                // Evaluate based on parameter type and condition
                switch (param.type) {
                case AnimationParameter::Type::Bool:
                    if (cond.comparison == "==") result = param.boolValue == cond.boolValue;
                    else if (cond.comparison == "!=") result = param.boolValue != cond.boolValue;
                    break;
                case AnimationParameter::Type::Float:
                    if (cond.comparison == ">") result = param.floatValue > cond.threshold;
                    else if (cond.comparison == "<") result = param.floatValue < cond.threshold;
                    else if (cond.comparison == ">=") result = param.floatValue >= cond.threshold;
                    else if (cond.comparison == "<=") result = param.floatValue <= cond.threshold;
                    else if (cond.comparison == "==") result = param.floatValue == cond.threshold;
                    else if (cond.comparison == "!=") result = param.floatValue != cond.threshold;
                    break;
                case AnimationParameter::Type::Int:
                    if (cond.comparison == ">") result = param.intValue > static_cast<int>(cond.threshold);
                    else if (cond.comparison == "<") result = param.intValue < static_cast<int>(cond.threshold);
                    else if (cond.comparison == ">=") result = param.intValue >= static_cast<int>(cond.threshold);
                    else if (cond.comparison == "<=") result = param.intValue <= static_cast<int>(cond.threshold);
                    else if (cond.comparison == "==") result = param.intValue == static_cast<int>(cond.threshold);
                    else if (cond.comparison == "!=") result = param.intValue != static_cast<int>(cond.threshold);
                    break;
                case AnimationParameter::Type::Trigger:
                    result = param.triggerValue;
                    break;
                }

                // If any condition fails, break early
                if (!result) {
                    allTrue = false;
                    break;
                }
            }

            if (allTrue) {
                // Execute transistion
                auto nextNode = std::find_if(graph->states.begin(), graph->states.end(),
                    [&](auto& s) { return s->id == transition.toNodeId; });

                if (nextNode != graph->states.end() && (*nextNode)->isAttached) {
                    graph->current = *nextNode;
                    PlayAnimation(graph->current->clipName);

                    // Reset trigger parameters
                    for (auto& p : graph->parameters)
                        if (p.type == AnimationParameter::Type::Trigger)
                            p.triggerValue = false;

                    EE_CORE_INFO("Transitioned to state: {}", graph->current->name);
                    break;
                }
            }
        }
    }

    /**
     * @brief Advance animation playback and update bone transforms.
     *
     * Increments the playback timer by @p deltaTime and updates the skeleton's bone transforms
     * according to the current animation clip. Updates the final bone matrices for rendering.
     *
     * @param deltaTime Time step in seconds since last frame.
     */
    void Animator::Update(double deltaTime, EntityID entity)
    {
        // Ensure there is a current clip and valid scene
        if (!m_CurrentClip || !m_Scene || !m_Scene->mRootNode) return;
        if (m_Paused) return;

        if (ECS::GetInstance().HasComponent<AnimationComponent>(entity))
        {
            auto& graph = ECS::GetInstance().GetComponent<AnimationComponent>(entity).m_animationGraph;
            if (graph && !graph->playing) return;
        }

        // Handle playback speed from graph
        double playbackSpeed = 1.0;
        if (ECS::GetInstance().HasComponent<AnimationComponent>(entity)) {
            auto& graph = ECS::GetInstance().GetComponent<AnimationComponent>(entity).m_animationGraph;
            if (graph) playbackSpeed = graph->playbackSpeed;
        }

        // Advance animation time in ticks
        double ticksPerSecond = m_CurrentClip->ticksPerSecond != 0.0
            ? m_CurrentClip->ticksPerSecond : 30.0;
        m_CurrentTime += deltaTime * ticksPerSecond * playbackSpeed;

        // Check animation looping
        if (m_Loop)
            m_CurrentTime = fmod(m_CurrentTime, m_CurrentClip->duration);
        else if (m_CurrentTime > m_CurrentClip->duration)
            m_CurrentTime = m_CurrentClip->duration; // clamp

        // Reset bone matrices
        std::fill(m_FinalBoneMatrices.begin(), m_FinalBoneMatrices.end(), glm::mat4(1.0f));

        // Start recursion at root
        CalculateBoneTransform(m_Scene->mRootNode, glm::mat4(1.0f));

        // Push back results to the model
        m_Model->SetBoneTransforms(m_FinalBoneMatrices);

        // Evaluate any animation graph transitions
        EvaluateTransitions(entity);
    }

    /**
     * @brief Recursively calculate bone transforms by traversing Assimp node hierarchy.
     *
     * Applies animations to nodes and accumulates parent transformations.
     *
     * @param node Current node being processed.
     * @param parentTransform The accumulated transform of the parent node.
     */
    void Animator::CalculateBoneTransform(const aiNode* node, const glm::mat4& parentTransform)
    {
        std::string nodeName(node->mName.C_Str());
        glm::mat4 nodeTransform = m_Model->ToGlm(node->mTransformation);

        const BoneAnimation* boneAnim = nullptr;
        if (m_CurrentClip) {
            auto it = m_CurrentClip->boneAnimations.find(nodeName);
            if (it != m_CurrentClip->boneAnimations.end())
                boneAnim = &it->second;
        }

        if (boneAnim && !boneAnim->keys.empty())
        {
            // Find keyframes around current time
            const auto& keys = boneAnim->keys;
            auto it = std::upper_bound(keys.begin(), keys.end(), m_CurrentTime,
                [](double time, const Keyframe& kf) { return time < kf.time; });
            size_t i = (it == keys.begin()) ? 0 : std::distance(keys.begin(), it) - 1;

            const Keyframe& curr = keys[i];
            const Keyframe& next = (i + 1 < keys.size()) ? keys[i + 1] : curr;

            double delta = next.time - curr.time;
            double factor = (delta > 0.0) ? (m_CurrentTime - curr.time) / delta : 0.0;

            glm::vec3 translation = glm::mix(curr.position, next.position, static_cast<float>(factor));
            glm::quat rotation = glm::slerp(curr.rotation, next.rotation, static_cast<float>(factor));
            glm::vec3 scale = glm::mix(curr.scale, next.scale, static_cast<float>(factor));

            nodeTransform = glm::translate(glm::mat4(1.0f), translation)
                * glm::mat4_cast(rotation)
                * glm::scale(glm::mat4(1.0f), scale);
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;

        const auto& boneMap = m_Model->GetBoneMapping();
        auto it = boneMap.find(nodeName);
        if (it != boneMap.end())
        {
            int boneIndex = it->second;
            m_FinalBoneMatrices[boneIndex] = globalTransform * m_Model->GetBoneOffsets()[boneIndex];
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            CalculateBoneTransform(node->mChildren[i], globalTransform);
        }
    }
}
