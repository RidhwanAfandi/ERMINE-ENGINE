/* Start Header ************************************************************************/
/*!
\file       AudioSystem.cpp
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       15/9/2025
\brief      Implementation of AudioSystem as a proper ECS System.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AudioSystem.h"

#include "MathVector.h"
#include "AudioManager.h" // Include the CAudioEngine
#include "EditorGUI.h"

class ECS;

using namespace Ermine;

bool AudioSystem::s_initialized = false;
bool AudioSystem::s_isPaused = false;

void GlobalAudioComponent::PlayMusic(int index) {
    AudioSystem::PlayGlobalMusic(*this, index);
}

void GlobalAudioComponent::StopMusic() {
    AudioSystem::StopGlobalMusic(*this);
}

void GlobalAudioComponent::SetMusicVolume(float volume) {
    musicVolume = std::clamp(volume, 0.0f, 1.0f);

    // Update currently playing music volume if there's music playing
    if (currentMusicChannelId != -1 && currentMusicIndex >= 0 && currentMusicIndex < music.size()) {
        const auto& musicSource = music[currentMusicIndex];
        float finalVolume = AudioSystem::ConvertVolumeToFMOD(
            musicSource.volume * masterVolume * musicVolume
        );
        CAudioEngine::SetChannelVolume(currentMusicChannelId, finalVolume);
    }
}

void GlobalAudioComponent::PlaySFX(int index) {
    AudioSystem::PlayGlobalSFX(*this, index);
}

void GlobalAudioComponent::PlaySFX(const std::string& name) {
    AudioSystem::PlayGlobalSFX(*this, name);
}

void GlobalAudioComponent::SetSFXVolume(float volume) {
    sfxVolume = std::clamp(volume, 0.0f, 1.0f);
    // Note: SFX volumes will be applied to new SFX plays
    // Existing SFX can't be updated since we don't track their channel IDs
}

int GlobalAudioComponent::GetSFXIndex(const std::string& name) const {
    for (size_t i = 0; i < sfx.size(); ++i) {
        if (sfx[i].audioName == name) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found
}

int GlobalAudioComponent::GetMusicIndex(const std::string& name) const {
    for (size_t i = 0; i < music.size(); ++i) {
        if (music[i].audioName == name) {
            return static_cast<int>(i);
        }
    }
    return -1; // Not found
}

void GlobalAudioComponent::AddMusicSource(const std::string& name, const std::string& path) {
    // Check if music with this name already exists
    for (const auto& musicSource : music) {
        if (musicSource.audioName == name) {
            std::cout << "Music source '" << name << "' already exists!" << std::endl;
            return;
        }
    }

    // Add new music source
    AudioSource newMusic;
    newMusic.audioName = name;
    newMusic.audioPath = path;
    newMusic.volume = 1.0f; // Default volume
    music.push_back(newMusic);
}

void GlobalAudioComponent::AddSFXSource(const std::string& name, const std::string& path) {
    // Check if SFX with this name already exists
    for (const auto& sfxSource : sfx) {
        if (sfxSource.audioName == name) {
            std::cout << "SFX source '" << name << "' already exists!" << std::endl;
            return;
        }
    }

    // Add new SFX source
    AudioSource newSFX;
    newSFX.audioName = name;
    newSFX.audioPath = path;
    newSFX.volume = 1.0f; // Default volume
    sfx.push_back(newSFX);
}

void GlobalAudioComponent::PlayAmbience(int index) {
    AudioSystem::PlayGlobalAmbience(*this, index);
}

void GlobalAudioComponent::PlayAmbience(const std::string& name) {
    AudioSystem::PlayGlobalAmbience(*this, name);
}

void GlobalAudioComponent::StopAmbience() {
    AudioSystem::StopGlobalAmbience(*this);
}

void GlobalAudioComponent::SetAmbienceVolume(float volume) {
    ambienceVolume = std::clamp(volume, 0.0f, 1.0f);

    // Update currently playing ambience volume
    if (currentAmbienceChannelId != -1 &&
        currentAmbienceIndex >= 0 &&
        currentAmbienceIndex < ambience.size()) {
        const auto& ambienceSource = ambience[currentAmbienceIndex];
        float finalVolume = AudioSystem::ConvertVolumeToFMOD(
            ambienceSource.volume * masterVolume * ambienceVolume
        );
        CAudioEngine::SetChannelVolume(currentAmbienceChannelId, finalVolume);
    }
}

int GlobalAudioComponent::GetAmbienceIndex(const std::string& name) const {
    for (size_t i = 0; i < ambience.size(); ++i) {
        if (ambience[i].audioName == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GlobalAudioComponent::AddAmbienceSource(const std::string& name, const std::string& path) {
    // Check if ambience with this name already exists
    for (const auto& amb : ambience) {
        if (amb.audioName == name) {
            std::cout << "Ambience '" << name << "' already exists!" << std::endl;
            return;
        }
    }

    // Add new ambience source
    AudioSource newAmbience;
    newAmbience.audioName = name;
    newAmbience.audioPath = path;
    newAmbience.volume = 1.0f;
    ambience.push_back(newAmbience);
}

void GlobalAudioComponent::UpdateAmbienceSource(int index, const std::string& name, const std::string& path) {
    if (index >= 0 && index < static_cast<int>(ambience.size())) {
        // Stop current ambience if we're updating the currently playing track
        if (currentAmbienceIndex == index && currentAmbienceChannelId != -1) {
            CAudioEngine::StopChannel(currentAmbienceChannelId);
            currentAmbienceChannelId = -1;
        }

        // Update the ambience source
        ambience[index].audioName = name;
        ambience[index].audioPath = path;

        // Reload the sound with new path
        try {
            CAudioEngine::LoadSound(path, false, true, true); // Looped and streamed
        }
        catch (const std::exception& e) {
            (void)e;
            UNREFERENCED_PARAMETER(e);
        }
    }
}

void GlobalAudioComponent::RemoveAmbienceSource(int index) {
    if (index >= 0 && index < static_cast<int>(ambience.size())) {
        // Stop current ambience if we're removing the currently playing track
        if (currentAmbienceIndex == index && currentAmbienceChannelId != -1) {
            CAudioEngine::StopChannel(currentAmbienceChannelId);
            currentAmbienceChannelId = -1;
            currentAmbienceIndex = -1;
        }
        else if (currentAmbienceIndex > index) {
            currentAmbienceIndex--;
        }

        ambience.erase(ambience.begin() + index);
    }
}

const AudioSource* GlobalAudioComponent::GetAmbienceSource(int index) const {
    if (index >= 0 && index < static_cast<int>(ambience.size())) {
        return &ambience[index];
    }
    return nullptr;
}

int GlobalAudioComponent::FindAmbienceIndex(const std::string& name) const {
    for (size_t i = 0; i < ambience.size(); ++i) {
        if (ambience[i].audioName == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}


// ========== AudioSystem Static Methods ==========

void AudioSystem::PlayGlobalAmbience(GlobalAudioComponent& globalAudio, int index)
{
    if (index < 0 || index >= globalAudio.ambience.size())
        return;

    // Stop current ambience if playing
    StopGlobalAmbience(globalAudio);

    const auto& ambienceSource = globalAudio.ambience[index];

    // Load and play the ambience (2D, looping, streaming)
    CAudioEngine::LoadSound(ambienceSource.audioPath, false, true, true);

    // Convert volume from 0-1 to dB and apply master/ambience volume
    float finalVolume = ConvertVolumeToFMOD(
        ambienceSource.volume * globalAudio.masterVolume * globalAudio.ambienceVolume
    );

    // Play the ambience
    Vector3D position(0.0f, 0.0f, 0.0f);
    globalAudio.currentAmbienceChannelId = CAudioEngine::PlaySounds(
        ambienceSource.audioPath, position, finalVolume
    );
    globalAudio.currentAmbienceIndex = index;

    std::cout << "Playing ambience: " << ambienceSource.audioName << std::endl;
}

void AudioSystem::PlayGlobalAmbience(GlobalAudioComponent& globalAudio, const std::string& name)
{
    // Find the ambience by name
    for (size_t i = 0; i < globalAudio.ambience.size(); ++i)
    {
        if (globalAudio.ambience[i].audioName == name)
        {
            PlayGlobalAmbience(globalAudio, static_cast<int>(i));
            return;
        }
    }

    // Ambience not found
    std::cout << "Ambience '" << name << "' not found in GlobalAudioComponent" << std::endl;
}

void AudioSystem::StopGlobalAmbience(GlobalAudioComponent& globalAudio)
{
    if (globalAudio.currentAmbienceChannelId != -1)
    {
        CAudioEngine::StopChannel(globalAudio.currentAmbienceChannelId);
        globalAudio.currentAmbienceChannelId = -1;
        globalAudio.currentAmbienceIndex = -1;
        std::cout << "Stopped ambience" << std::endl;
    }
}

void AudioSystem::Init()
{
    if (!s_initialized)
    {
        // Initialize the underlying CAudioEngine
        CAudioEngine::Init();
        s_initialized = true;
    }
}

void AudioSystem::PauseAll()
{
    if (s_isPaused) return; // Already paused

    //std::cout << "=== PAUSING ALL AUDIO ===" << std::endl;
    CAudioEngine::PauseAllChannels(); // Pause instead of stop
    s_isPaused = true;
}

void AudioSystem::ResumeAll()
{
    if (!s_isPaused) return; // Not paused

    //std::cout << "=== RESUMING ALL AUDIO ===" << std::endl;
    CAudioEngine::ResumeAllChannels(); // Resume all paused channels
    s_isPaused = false;
}

void AudioSystem::Update()
{
    auto& ecs = ECS::GetInstance();
    static bool s_WasPlaying = false;
    static bool s_HasAutoPlayed = false;

    bool isPlaying = (editor::EditorGUI::s_state == editor::EditorGUI::SimState::playing);

    // Handle PAUSED state (different from STOPPED)
    if (editor::EditorGUI::s_state == editor::EditorGUI::SimState::paused)
    {
        if (!s_isPaused)
        {
            PauseAll(); // Pause all audio instead of stopping
        }

        CAudioEngine::Update(); // Still update FMOD (for pause state)
        return;
    }

    // Handle STOPPED state (complete stop)
    if (!isPlaying)
    {
        if (s_WasPlaying)
        {
            std::cout << "=== STOPPING ALL AUDIO ===" << std::endl;
            CAudioEngine::StopAllChannels(); // Complete stop

            // Clear entity audio states
            for (EntityID entity : m_Entities)
            {
                if (!ecs.IsEntityValid(entity)) continue;
                if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
                {
                    const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                    if (!meta.selfActive)
                        continue;
                }
                if (ecs.HasComponent<AudioComponent>(entity))
                {
                    auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
                    audioComp.shouldPlay = false;
                    audioComp.shouldStop = false;
                    audioComp.channelId = -1;
                    audioComp.isPlaying = false;
                }
            }

            // Clear global audio
            for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
            {
                if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
                {
                    auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
                    globalAudio.currentMusicChannelId = -1;
                    globalAudio.currentMusicIndex = -1;
                    globalAudio.currentAmbienceChannelId = -1;
                    globalAudio.currentAmbienceIndex = -1;
                    break;
                }
            }

            std::cout << "=== ALL AUDIO STOPPED ===" << std::endl;
        }

        s_WasPlaying = false;
        s_HasAutoPlayed = false;
        s_isPaused = false; // Reset pause state when stopped

        CAudioEngine::Update();
        UpdateAudioComponents();
        return;
    }

    // ===== PLAYING STATE =====

    // Resume if we were paused
    if (s_isPaused)
    {
        ResumeAll();
    }

    // Find global audio entity
    EntityID globalAudioEntity = 0;
    for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
    {
        if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
        {
            globalAudioEntity = entity;
            break;
        }
    }

    // Handle global audio
    if (globalAudioEntity != 0)
    {
        auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(globalAudioEntity);

        if (globalAudio.autoPlay &&
            globalAudio.currentMusicChannelId == -1 &&
            !globalAudio.music.empty())
        {
            PlayGlobalMusic(globalAudio, 0);
        }

        if (globalAudio.currentAmbienceChannelId == -1 && !globalAudio.ambience.empty())
        {
            PlayGlobalAmbience(globalAudio, 0);
        }

        UpdateGlobalAudio(globalAudio);
    }

    // Auto-play entities ONLY ONCE on first frame
    if (!s_HasAutoPlayed)
    {
        std::cout << "=== AUTO-PLAY CHECK ===" << std::endl;
        for (EntityID entity : m_Entities)
        {
            if (!ecs.IsEntityValid(entity)) continue;

            if (ECS::GetInstance().HasComponent<ObjectMetaData>(entity))
            {
                const auto& meta = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);
                if (!meta.selfActive)
                    continue;
            }

            if (ecs.HasComponent<AudioComponent>(entity))
            {
                auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
                if (audioComp.playOnStart && !audioComp.soundName.empty())
                {
                    std::cout << "Auto-playing entity " << entity
                        << " (" << audioComp.soundName << ")" << std::endl;
                    audioComp.shouldPlay = true;
                }
            }
        }
        s_HasAutoPlayed = true;
        std::cout << "=== AUTO-PLAY DONE ===" << std::endl;
    }

    CAudioEngine::Update();
    UpdateAudioComponents();

    s_WasPlaying = true;

	// Previous implementation kept for reference 
	// This version auto-plays sounds only once per play session
    //auto& ecs = ECS::GetInstance();
    //static bool s_WasPlaying = false; // Track previous state
    //static bool s_HasAutoPlayed = false; // Track if we've auto-played this session

    //bool isPlaying = (editor::EditorGUI::s_state == editor::EditorGUI::SimState::playing);

    //// Handle STOPPED/PAUSED state (ALWAYS, regardless of global audio)
    //if (!isPlaying)
    //{
    //    if (s_WasPlaying)
    //    {
    //        std::cout << "=== STOPPING ALL AUDIO ===" << std::endl;

    //        // FIRST: Stop all FMOD channels immediately
    //        CAudioEngine::StopAllChannels();

    //        // THEN: Clear all entity audio states
    //        for (EntityID entity : m_Entities)
    //        {
    //            if (!ecs.IsEntityValid(entity)) continue;
    //            if (ecs.HasComponent<AudioComponent>(entity))
    //            {
    //                auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
    //                audioComp.shouldPlay = false;
    //                audioComp.shouldStop = false;
    //                audioComp.channelId = -1;
    //                audioComp.isPlaying = false;
    //                std::cout << "Cleared entity " << entity << " audio state" << std::endl;
    //            }
    //        }

    //        // Clear global audio if it exists
    //        for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
    //        {
    //            if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
    //            {
    //                auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(entity);
    //                globalAudio.currentMusicChannelId = -1;
    //                globalAudio.currentMusicIndex = -1;
    //                break;
    //            }
    //        }

    //        std::cout << "=== ALL AUDIO STOPPED ===" << std::endl;
    //    }

    //    s_WasPlaying = false;
    //    s_HasAutoPlayed = false;
    //    CAudioEngine::Update();
    //    UpdateAudioComponents();
    //    return; // Exit early
    //}

    //// ===== PLAYING STATE =====

    //// Find the entity with GlobalAudioComponent (if it exists)
    //EntityID globalAudioEntity = 0;
    //for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
    //{
    //    if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
    //    {
    //        globalAudioEntity = entity;
    //        break;
    //    }
    //}

    //// Handle global audio if it exists
    //if (globalAudioEntity != 0)
    //{
    //    auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(globalAudioEntity);

    //    // Autoplay global music if needed
    //    if (globalAudio.autoPlay &&
    //        globalAudio.currentMusicChannelId == -1 &&
    //        !globalAudio.music.empty())
    //    {
    //        PlayGlobalMusic(globalAudio, 0);
    //    }

    //    UpdateGlobalAudio(globalAudio);
    //}

    //// Auto-play entities ONLY ONCE on first frame
    //if (!s_HasAutoPlayed)
    //{
    //    std::cout << "=== AUTO-PLAY CHECK ===" << std::endl;
    //    for (EntityID entity : m_Entities)
    //    {
    //        if (ecs.IsEntityValid(entity) && ecs.HasComponent<AudioComponent>(entity))
    //        {
    //            auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
    //            if (audioComp.playOnStart && !audioComp.soundName.empty())
    //            {
    //                std::cout << "Auto-playing entity " << entity
    //                    << " (" << audioComp.soundName << ")" << std::endl;
    //                audioComp.shouldPlay = true;
    //            }
    //        }
    //    }
    //    s_HasAutoPlayed = true;
    //    std::cout << "=== AUTO-PLAY DONE ===" << std::endl;
    //}

    //// Update FMOD and entity audio components
    //CAudioEngine::Update();
    //UpdateAudioComponents();

    //s_WasPlaying = true;


    // Update listener to follow main camera 
    //if (HasMainCamera()) {
    //    auto& camera = GetMainCamera();
    //    CAudioEngine::SetListenerAttributes(
    //        camera.GetPosition(),
    //        camera.GetVelocity(),    // Calculate from previous position
    //        camera.GetForward(),
    //        camera.GetUp()
    //    );
    //}

    // Update all entities with AudioComponents
    //UpdateAudioComponents();
}

void AudioSystem::UpdateAudioComponents()
{
    auto& ecs = ECS::GetInstance();

    // Iterate through all entities tracked by this system
    // m_Entities contains entities that match our signature (AudioComponent + Transform)
    for (EntityID entity : m_Entities)
    {
        if (!ecs.IsEntityValid(entity))
            continue;

        // Defensive check: ensure components still exist (in case of timing issues during scene transitions)
        if (!ecs.HasComponent<AudioComponent>(entity) || !ecs.HasComponent<Transform>(entity))
            continue;

        auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
        auto& transform = ecs.GetComponent<Transform>(entity);

        // Handle play requests
        if (audioComp.shouldPlay)
        {
            PlayEntityAudio(audioComp, transform);
            audioComp.shouldPlay = false;
        }

        // Handle stop requests
        if (audioComp.shouldStop)
        {
            StopEntityAudio(audioComp);
            audioComp.shouldStop = false;
        }

        // **NEW: Update volume if it changed while playing**
        if (audioComp.isPlaying && audioComp.channelId != -1)
        {
            float currentVolume = ConvertVolumeToFMOD(audioComp.volume);
            //std::cout << "Updating volume for channel " << audioComp.channelId
            //    << " to " << audioComp.volume << " (dB: " << currentVolume << ")" << std::endl;
            CAudioEngine::SetChannelVolume(audioComp.channelId, currentVolume);
        }

        // Update 3D position if following transform and currently playing
        if (audioComp.isPlaying && audioComp.followTransform && audioComp.channelId != -1)
        {
            UpdateEntityAudio3D(audioComp, transform);
        }

        // Check if audio is still playing (update isPlaying status)
        if (audioComp.channelId != -1)
        {
            audioComp.isPlaying = CAudioEngine::IsPlaying(audioComp.channelId);
        }
    }
}

void AudioSystem::UpdateGlobalAudio(GlobalAudioComponent& globalAudio)
{
    // Handle any global audio state updates here
    // Check if current music is still playing, update volumes, etc.

    if (globalAudio.currentMusicChannelId != -1)
    {
        // Check if music is still playing
        if (!CAudioEngine::IsPlaying(globalAudio.currentMusicChannelId))
        {
            globalAudio.currentMusicChannelId = -1;
            globalAudio.currentMusicIndex = -1;
        }
    }

    // *** NEW: Check if current ambience is still playing ***
    if (globalAudio.currentAmbienceChannelId != -1)
    {
        if (!CAudioEngine::IsPlaying(globalAudio.currentAmbienceChannelId))
        {
            globalAudio.currentAmbienceChannelId = -1;
            globalAudio.currentAmbienceIndex = -1;
        }
    }
}

void AudioSystem::PlayGlobalMusic(GlobalAudioComponent& globalAudio, int index)
{
    if (index < 0 || index >= globalAudio.music.size())
        return;

    // Stop current music if playing
    StopGlobalMusic(globalAudio);

    const auto& musicSource = globalAudio.music[index];

    // Load and play the music
    CAudioEngine::LoadSound(musicSource.audioPath, false, true, true); // 2D, looping, streaming

    // Convert volume from 0-1 to dB and apply master/music volume
    float finalVolume = ConvertVolumeToFMOD(musicSource.volume * globalAudio.masterVolume * globalAudio.musicVolume);

    // Convert Vec3 to Vector3D for CAudioEngine compatibility
    Vector3D position(0.0f, 0.0f, 0.0f);
    globalAudio.currentMusicChannelId = CAudioEngine::PlaySounds(musicSource.audioPath, position, finalVolume);
    globalAudio.currentMusicIndex = index;
}

void AudioSystem::PlayGlobalSFX(GlobalAudioComponent& globalAudio, int index)
{
    if (index < 0 || index >= globalAudio.sfx.size())
        return;

    const auto& sfxSource = globalAudio.sfx[index];

    // Load and play the SFX
    CAudioEngine::LoadSound(sfxSource.audioPath, false, false, false); // 2D, no loop, no stream

    // Convert volume from 0-1 to dB and apply master/SFX volume
    float finalVolume = ConvertVolumeToFMOD(sfxSource.volume * globalAudio.masterVolume * globalAudio.sfxVolume);

    // Convert Vec3 to Vector3D for CAudioEngine compatibility
    Vector3D position(0.0f, 0.0f, 0.0f);
    CAudioEngine::PlaySounds(sfxSource.audioPath, position, finalVolume);
}

void AudioSystem::PlayGlobalSFX(GlobalAudioComponent& globalAudio, const std::string& name)
{
    // Find the SFX by name
    for (size_t i = 0; i < globalAudio.sfx.size(); ++i)
    {
        if (globalAudio.sfx[i].audioName == name)
        {
            PlayGlobalSFX(globalAudio, static_cast<int>(i));
            return;
        }
    }

    // SFX not found
    std::cout << "SFX '" << name << "' not found in GlobalAudioComponent" << std::endl;
}

void AudioSystem::StopGlobalMusic(GlobalAudioComponent& globalAudio)
{
    if (globalAudio.currentMusicChannelId != -1)
    {
        CAudioEngine::StopChannel(globalAudio.currentMusicChannelId);
        globalAudio.currentMusicChannelId = -1;
        globalAudio.currentMusicIndex = -1;
    }
}

void AudioSystem::PlayEntityAudio(AudioComponent& audioComp, const Transform& transform)
{
    std::string soundToPlay;

    // Determine which sound to play
    if (audioComp.useRandomVariation && !audioComp.soundVariations.empty())
    {
        // Pick random variation (avoid playing the same one twice in a row)
        int randomIndex;

        if (audioComp.soundVariations.size() == 1)
        {
            randomIndex = 0;
        }
        else
        {
            do {
                randomIndex = rand() % audioComp.soundVariations.size();
            } while (randomIndex == audioComp.lastPlayedVariationIndex);
        }

        audioComp.lastPlayedVariationIndex = randomIndex;
        soundToPlay = audioComp.soundVariations[randomIndex];
        //std::cout << "Playing variation " << randomIndex << ": " << soundToPlay << std::endl;
    }
    else
    {
        // Use single sound
        soundToPlay = audioComp.soundName;
    }

    if (soundToPlay.empty() && audioComp.eventName.empty())
        return;

    if (!audioComp.eventName.empty())
    {
        // Handle FMOD Studio events
        CAudioEngine::LoadEvent(audioComp.eventName);
        CAudioEngine::PlayEvent(audioComp.eventName);
        audioComp.isPlaying = CAudioEngine::IsEventPlaying(audioComp.eventName);
        audioComp.channelId = -1; // Events don't use channel IDs
    }
    else if (!soundToPlay.empty())  // ← Changed from soundName to soundToPlay
    {
        // Handle regular sounds (including variations!)
        CAudioEngine::LoadSound(soundToPlay, audioComp.is3D, audioComp.isLooping, audioComp.isStreaming);  // ← Changed

        // Convert Vec3 to Vector3D for CAudioEngine compatibility
        Vector3D position(0.0f, 0.0f, 0.0f);
        if (audioComp.is3D)
        {
            position = Vector3D(transform.position.x, transform.position.y, transform.position.z);
        }

        // Convert 0-1 volume to dB
        float volumeDB = ConvertVolumeToFMOD(audioComp.volume);

        audioComp.channelId = CAudioEngine::PlaySounds(soundToPlay, position, volumeDB);  // ← Changed
        audioComp.isPlaying = (audioComp.channelId != -1);
    }
}

void AudioSystem::StopEntityAudio(AudioComponent& audioComp)
{
    if (!audioComp.eventName.empty())
    {
        CAudioEngine::StopEvent(audioComp.eventName);
    }
    else if (audioComp.channelId != -1)
    {
        CAudioEngine::StopChannel(audioComp.channelId);
        audioComp.channelId = -1;
    }

    audioComp.isPlaying = false;
}

void AudioSystem::UpdateEntityAudio3D(AudioComponent& audioComp, const Transform& transform)
{
    if (audioComp.channelId != -1 && audioComp.is3D)
    {
        // Convert Vec3 to Vector3D for CAudioEngine compatibility
        Vector3D position(transform.position.x, transform.position.y, transform.position.z);
        CAudioEngine::SetChannel3dPosition(audioComp.channelId, position);
    }
}

void AudioSystem::Shutdown()
{
    if (s_initialized)
    {
        CAudioEngine::Shutdown();
        s_initialized = false;
    }
}

// Utility functions for volume conversion
float AudioSystem::ConvertVolumeToFMOD(float volume01)
{
    // Convert 0-1 volume to FMOD dB range (-60 to 0)
    if (volume01 <= 0.0f) return -60.0f;
    if (volume01 >= 1.0f) return 0.0f;

    return CAudioEngine::VolumeTodB(volume01);
}

float AudioSystem::ConvertVolumeFromFMOD(float volumeDB)
{
    // Convert FMOD dB to 0-1 range
    return CAudioEngine::dbToVolume(volumeDB);
}

// Helper function is no longer needed since Vec3 and Vector3D are the same type