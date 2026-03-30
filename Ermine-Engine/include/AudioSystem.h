/* Start Header ************************************************************************/
/*!
\file       AudioSystem.h
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       15/9/2025
\brief      AudioSystem as a proper ECS System that manages audio components.
            Integrates with the Ermine ECS architecture and handles both
            GlobalAudioComponent and individual AudioComponent updates.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

#include "Components.h"
//#include "AudioManager.h"
#include "Systems.h" // For the base System class

namespace Ermine
{
    /*!***********************************************************************
    \brief
     AudioSystem manages all audio components as a proper ECS System.
     It automatically tracks entities with AudioComponent + Transform through
     the ECS signature system.
    *************************************************************************/
    class AudioSystem : public System
    {
    private:
        // Static initialization flag
        static bool s_initialized;
        static EntityID s_globalAudioEntity;
        static bool s_isPaused;

    public:
        AudioSystem() = default;
        ~AudioSystem() = default;

        // Core system functions
        static void Init();
        static void Shutdown();

        // Main update function called by the engine
        void Update();

        static void PauseAll();
        static void ResumeAll();
        static bool IsPaused() { return s_isPaused; }

        // GlobalAudioComponent management (static functions for global access)
        static void UpdateGlobalAudio(GlobalAudioComponent& globalAudio);
        static void PlayGlobalMusic(GlobalAudioComponent& globalAudio, int index);
        static void PlayGlobalSFX(GlobalAudioComponent& globalAudio, int index);
        static void PlayGlobalSFX(GlobalAudioComponent& globalAudio, int index, bool useReverb, float wetLevel, float dryLevel, float decayTime, float earlyDelay = 0.001f, float lateDelay = 0.001f);
        static void PlayGlobalSFX(GlobalAudioComponent& globalAudio, const std::string& name);
        static void PlayGlobalSFX(GlobalAudioComponent& globalAudio, const std::string& name, bool useReverb, float wetLevel, float dryLevel, float decayTime, float earlyDelay = 0.001f, float lateDelay = 0.001f);
        static void StopGlobalSFX(GlobalAudioComponent& globalAudio, const std::string& name);
        static void StopGlobalMusic(GlobalAudioComponent& globalAudio);

        // *** NEW: Ambience playback ***
        static void PlayGlobalAmbience(GlobalAudioComponent& globalAudio, int index);
        static void PlayGlobalAmbience(GlobalAudioComponent& globalAudio, const std::string& name);
        static void StopGlobalAmbience(GlobalAudioComponent& globalAudio);

        // Voice playback
        static void PlayGlobalVoice(GlobalAudioComponent& globalAudio, int index);
        static void PlayGlobalVoice(GlobalAudioComponent& globalAudio, const std::string& name);
        static void StopGlobalVoice(GlobalAudioComponent& globalAudio);

        // Individual AudioComponent management (instance methods)
        void UpdateAudioComponents();
        void PlayEntityAudio(AudioComponent& audioComp, const Transform& transform);
        void StopEntityAudio(AudioComponent& audioComp);
        void UpdateEntityAudio3D(AudioComponent& audioComp, const Transform& transform);

        // Utility functions
        static float ConvertVolumeToFMOD(float volume01);
        static float ConvertVolumeFromFMOD(float volumeDB);

        const std::set<EntityID>& GetEntities() const { return m_Entities; }
        // Static method to check if audio system is initialized
        static bool IsInitialized() { return s_initialized; }   
    };
}