/* Start Header ************************************************************************/
/*!
\file       AnimationClip.h
\author     Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       27/09/2025
\brief      This file contains the declaration of animation-related data
            structures used for skeletal animation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Ermine::graphics
{
    /**
     * @struct Keyframe
     * @brief Represents a single keyframe of animation data for a bone.
     *
     * A keyframe stores the transform of a bone at a given time,
     * including position, rotation, and scale. Keyframes are interpolated
     * during playback to produce smooth bone animation.
     */
    struct Keyframe {
        double time;        // Time of the keyframe, in animation ticks.
        glm::vec3 position; // Translation of the bone at this keyframe.
        glm::quat rotation; // Orientation of the bone at this keyframe.
        glm::vec3 scale;    // Scale of the bone at this keyframe.
    };

    /**
     * @struct BoneAnimation
     * @brief Stores the animation channel for a single bone.
     *
     * Each bone animation contains a sequence of keyframes sorted by time.
     * During evaluation, the system interpolates between these keyframes
     * to compute the current bone transform.
     */
    struct BoneAnimation {
        std::string boneName;       // Name of the bone this animation affects.
        std::vector<Keyframe> keys; // Keyframes sorted by ascending time.
    };

    /**
     * @struct AnimationClip
     * @brief Represents a complete skeletal animation clip.
     *
     * An animation clip contains a collection of bone animations, along
     * with timing information such as duration and ticks-per-second.
     * Clips are typically loaded from assets (e.g., FBX files) and can
     * be sampled at any time to animate a skeleton.
     */
    struct AnimationClip {
        std::string name;             // Name of the animation clip.
        double duration = 0.0;        // Duration of the clip, in ticks.
        double ticksPerSecond = 30.0; // Conversion rate from ticks to seconds.

        // Mapping of bone names to their animations
        std::unordered_map<std::string, BoneAnimation> boneAnimations;
    };
}
