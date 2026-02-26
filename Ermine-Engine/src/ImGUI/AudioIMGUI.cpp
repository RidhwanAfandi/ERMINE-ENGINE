/* Start Header ************************************************************************/
/*!
\file       AudioImGUI.cpp
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       26/01/2026
\brief      This file contains the implementation of AudioImGUI for managing audio through ImGUI.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AudioImGUI.h"
#include "AudioSystem.h"
#include "FrameController.h"
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;


namespace Ermine
{
    AudioImGUI::AudioImGUI() : ImGUIWindow("Audio Manager")
    {
        SetStatusMessage("Audio Manager Initialized", 2.0f);
    }

    AudioImGUI::~AudioImGUI() {}

    void AudioImGUI::Update()
    {
        UpdateStatus();
    }

    void AudioImGUI::Render()
    {
        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
        {
            ImGui::End();
            return;
        }

        // Status message display
        if (m_StatusTimer > 0.0f)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", m_StatusMessage.c_str());
            ImGui::Separator();
        }

        // Tabs for different audio management sections
        if (ImGui::BeginTabBar("AudioTabs"))
        {
            if (ImGui::BeginTabItem("Entity Audio"))
            {
                RenderEntityAudioControls();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Audio Tester"))
            {
                RenderAudioTester();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Audio Browser"))
            {
                RenderAudioBrowser();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Global Audio"))
            {
                RenderGlobalAudioControls();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void AudioImGUI::RenderEntityAudioControls()
    {
        ImGui::Text("Entity Audio Management");
        ImGui::Separator();

        // Get ALL entities (both with and without audio components)
        auto allEntities = GetAllEntities();

        if (allEntities.empty())
        {
            ImGui::Text("No entities found in the scene.");
            return;
        }

        // Entity selection - showing ALL entities
        ImGui::Text("Select Entity:");
        static int currentEntityIndex = 0;

        // Ensure currentEntityIndex is valid
        if (currentEntityIndex >= static_cast<int>(allEntities.size()))
        {
            currentEntityIndex = 0;
        }

        std::vector<std::string> entityNames;
        std::vector<bool> hasAudioComponent;

        for (size_t i = 0; i < allEntities.size(); ++i)
        {
            auto& ecs = ECS::GetInstance();
            EntityID entityId = allEntities[i];
            std::string name = "Entity " + std::to_string(entityId);
            bool hasAudio = ecs.HasComponent<AudioComponent>(entityId);

            // Try to get object metadata for a better name
            if (ecs.HasComponent<ObjectMetaData>(entityId))
            {
                auto& metadata = ecs.GetComponent<ObjectMetaData>(entityId);
                if (!metadata.name.empty())
                {
                    name = metadata.name + " (" + std::to_string(entityId) + ")";
                }
            }

            // Add indicator for audio component status
            if (hasAudio)
            {
                name += " [Audio]";
            }
            else
            {
                name += " [No Audio]";
            }

            entityNames.push_back(name);
            hasAudioComponent.push_back(hasAudio);
        }

        if (ImGui::BeginCombo("##EntitySelect", entityNames[currentEntityIndex].c_str()))
        {
            for (size_t i = 0; i < entityNames.size(); ++i)
            {
                bool isSelected = (currentEntityIndex == static_cast<int>(i));

                // Color code based on audio component status
                if (hasAudioComponent[i])
                {
                    // Green tint for entities with audio
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                }
                else
                {
                    // Yellow tint for entities without audio
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                }

                if (ImGui::Selectable(entityNames[i].c_str(), isSelected))
                {
                    currentEntityIndex = static_cast<int>(i);
                    m_SelectedEntity = allEntities[i];
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();

                ImGui::PopStyleColor();
            }
            ImGui::EndCombo();
        }

        // Legend
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Green");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "= Has Audio, ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Yellow");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "= No Audio)");

        if (currentEntityIndex < allEntities.size())
        {
            m_SelectedEntity = allEntities[currentEntityIndex];
            auto& ecs = ECS::GetInstance();

            // Validate entity still exists
            if (!ecs.IsEntityValid(m_SelectedEntity))
            {
                SetStatusMessage("Selected entity is no longer valid, refreshing list...");
                return;
            }

            bool hasAudio = ecs.HasComponent<AudioComponent>(m_SelectedEntity);

            ImGui::Separator();
            ImGui::Text("Entity ID: %u", m_SelectedEntity);

            // Show entity name if available
            if (ecs.HasComponent<ObjectMetaData>(m_SelectedEntity))
            {
                auto& metadata = ecs.GetComponent<ObjectMetaData>(m_SelectedEntity);
                if (!metadata.name.empty())
                {
                    ImGui::Text("Entity Name: %s", metadata.name.c_str());
                }
            }

            // Audio component management
            if (hasAudio)
            {
                // Entity HAS audio component - show controls and delete option
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Has AudioComponent");
                ImGui::SameLine();
                if (ImGui::Button("Remove Audio Component"))
                {
                    m_ShowDeleteAudioConfirmation = true;
                    m_DeleteAudioEntity = m_SelectedEntity;
                }

                auto& audioComp = ecs.GetComponent<AudioComponent>(m_SelectedEntity);

                ImGui::Separator();

                ImGui::Text("Audio Mode:");
                static const char* modes[] = { "Single Sound", "Random Variations" };
                int currentMode = audioComp.useRandomVariation ? 1 : 0;

                if (ImGui::Combo("##AudioMode", &currentMode, modes, 2))
                {
                    audioComp.useRandomVariation = (currentMode == 1);

                    // *** CLEAR THE OTHER MODE'S DATA ***
                    if (audioComp.useRandomVariation)
                    {
                        // Switched to variation mode - clear single sound
                        audioComp.soundName.clear();
                    }
                    else
                    {
                        // Switched to single mode - clear variations
                        audioComp.soundVariations.clear();
                    }
                }

                ImGui::Separator();

                // === Single Sound Mode ===
                if (!audioComp.useRandomVariation)
                {
                    ImGui::Text("Sound Path:");
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##EntityBrowse"))
                    {
                        m_ShowEntitySoundBrowser = true;
                    }

                    char soundBuffer[256];
                    strncpy_s(soundBuffer, sizeof(soundBuffer), audioComp.soundName.c_str(), sizeof(soundBuffer) - 1);
                    soundBuffer[sizeof(soundBuffer) - 1] = '\0';

                    if (ImGui::InputText("##EntitySoundPath", soundBuffer, sizeof(soundBuffer)))
                    {
                        audioComp.soundName = std::string(soundBuffer);
                    }

                    // Audio file browser popup for entity
                    if (m_ShowEntitySoundBrowser)
                    {
                        ImGui::OpenPopup("Select Audio File##Entity");
                    }

                    if (ImGui::BeginPopupModal("Select Audio File##Entity", &m_ShowEntitySoundBrowser, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        if (RenderAudioFileSelector())
                        {
                            audioComp.soundName = m_SelectedAudioFile;
                            m_ShowEntitySoundBrowser = false;
                            ImGui::CloseCurrentPopup();
                        }

                        if (ImGui::Button("Cancel"))
                        {
                            m_ShowEntitySoundBrowser = false;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }
                }
                // === Variation Mode ===
                else
                {
                    ImGui::Text("Sound Variations:");
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Each play will randomly pick one variation");

                    // Display existing variations
                    for (size_t i = 0; i < audioComp.soundVariations.size(); ++i)
                    {
                        ImGui::PushID(static_cast<int>(i));

                        ImGui::Text("%d.", static_cast<int>(i + 1));
                        ImGui::SameLine();

                        // Show filename only, not full path
                        std::string filename = audioComp.soundVariations[i];
                        size_t lastSlash = filename.find_last_of("/\\");
                        if (lastSlash != std::string::npos)
                        {
                            filename = filename.substr(lastSlash + 1);
                        }
                        ImGui::Text("%s", filename.c_str());

                        ImGui::SameLine();
                        if (ImGui::Button("Remove"))
                        {
                            audioComp.soundVariations.erase(audioComp.soundVariations.begin() + i);
                            ImGui::PopID();
                            break; // Exit loop after modifying vector
                        }

                        ImGui::PopID();
                    }

                    // Add new variation
                    ImGui::Separator();
                    static char variationBuffer[256] = "";
                    ImGui::InputText("New Variation Path", variationBuffer, sizeof(variationBuffer));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse##AddVariation"))
                    {
                        m_ShowEntitySoundBrowser = true;
                    }

                    if (ImGui::Button("Add Variation"))
                    {
                        if (strlen(variationBuffer) > 0)
                        {
                            audioComp.soundVariations.push_back(std::string(variationBuffer));
                            memset(variationBuffer, 0, sizeof(variationBuffer));
                            SetStatusMessage("Added sound variation");
                        }
                    }

                    // Audio file browser for variations
                    if (m_ShowEntitySoundBrowser)
                    {
                        ImGui::OpenPopup("Select Audio File##Variation");
                    }

                    if (ImGui::BeginPopupModal("Select Audio File##Variation", &m_ShowEntitySoundBrowser, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        if (RenderAudioFileSelector())
                        {
                            strncpy_s(variationBuffer, sizeof(variationBuffer), m_SelectedAudioFile.c_str(), sizeof(variationBuffer) - 1);
                            variationBuffer[sizeof(variationBuffer) - 1] = '\0';
                            m_ShowEntitySoundBrowser = false;
                            ImGui::CloseCurrentPopup();
                        }

                        if (ImGui::Button("Cancel"))
                        {
                            m_ShowEntitySoundBrowser = false;
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }

                    if (audioComp.soundVariations.empty())
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: No variations added yet!");
                    }
                }

                ImGui::Separator();

                // Common audio settings (work for both modes)
                ImGui::SliderFloat("Volume", &audioComp.volume, 0.0f, 1.0f);
                ImGui::Checkbox("3D Audio", &audioComp.is3D);
                ImGui::Checkbox("Looping", &audioComp.isLooping);
                ImGui::Checkbox("Streaming", &audioComp.isStreaming);
                ImGui::Checkbox("Follow Transform", &audioComp.followTransform);

                ImGui::Separator();

                // Audio control buttons
                if (ImGui::Button("Play Audio"))
                {
                    audioComp.shouldPlay = true;
                    audioComp.playOnStart = true;
                    SetStatusMessage("Playing audio for Entity " + std::to_string(m_SelectedEntity));
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop Audio"))
                {
                    audioComp.shouldStop = true;
                    audioComp.playOnStart = false;
                    SetStatusMessage("Stopping audio for Entity " + std::to_string(m_SelectedEntity));
                }

                // Display current status
                ImGui::Separator();
                ImGui::Text("Status:");
                ImGui::Text("Playing: %s", audioComp.isPlaying ? "Yes" : "No");
                ImGui::Text("Channel ID: %d", audioComp.channelId);

                if (audioComp.useRandomVariation)
                {
                    ImGui::Text("Variations: %d", static_cast<int>(audioComp.soundVariations.size()));
                }
            }
            else
            {
                // Entity DOESN'T have audio component - show add option
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "No AudioComponent");

                ImGui::Separator();

                if (ImGui::Button("Add Audio Component"))
                {
                    AddAudioComponentToEntity(m_SelectedEntity);
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Click to add audio capabilities to this entity");

                ImGui::Separator();
                ImGui::TextWrapped("This entity currently has no audio component. Add one to enable audio functionality.");
            }
        }

        // Render deletion confirmation popup
        RenderDeleteAudioConfirmationPopup();
    }

    std::vector<EntityID> AudioImGUI::GetAllEntities()
    {
        auto& ecs = ECS::GetInstance();
        std::vector<EntityID> allEntities;

        for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
        {
            if (ecs.IsEntityValid(entity))
            {
                // Skip entities with GlobalAudioComponent - they belong in the Global Audio tab
                if (ecs.HasComponent<GlobalAudioComponent>(entity))
                    continue;

                allEntities.push_back(entity);
            }
        }

        return allEntities;
    }

    void AudioImGUI::AddAudioComponentToEntity(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.IsEntityValid(entity) || ecs.HasComponent<AudioComponent>(entity))
        {
            SetStatusMessage("Cannot add AudioComponent: Entity invalid or already has audio");
            return;
        }

        // Create default AudioComponent
        AudioComponent audioComp;
        audioComp.soundName = ""; // Empty by default
        audioComp.volume = 1.0f;
        audioComp.is3D = true;  // Default to 3D for entity audio
        audioComp.isLooping = false;
        audioComp.isStreaming = false;
        audioComp.followTransform = true;  // Default to following transform
        audioComp.shouldPlay = false;
        audioComp.shouldStop = false;
        audioComp.isPlaying = false;
        audioComp.channelId = -1;

        ecs.AddComponent(entity, audioComp);

        std::string entityName = "Entity " + std::to_string(entity);
        if (ecs.HasComponent<ObjectMetaData>(entity))
        {
            auto& metadata = ecs.GetComponent<ObjectMetaData>(entity);
            if (!metadata.name.empty())
            {
                entityName = metadata.name;
            }
        }

        SetStatusMessage("Added AudioComponent to " + entityName);
    }

    void AudioImGUI::RenderDeleteAudioConfirmationPopup()
    {
        if (m_ShowDeleteAudioConfirmation)
        {
            ImGui::OpenPopup("Confirm Remove Audio Component");
        }

        if (ImGui::BeginPopupModal("Confirm Remove Audio Component", &m_ShowDeleteAudioConfirmation, ImGuiWindowFlags_AlwaysAutoResize))
        {
            auto& ecs = ECS::GetInstance();

            ImGui::Text("Are you sure you want to remove the AudioComponent from:");
            ImGui::Text("Entity ID: %u", m_DeleteAudioEntity);

            // Try to show entity name if available
            if (ecs.IsEntityValid(m_DeleteAudioEntity) && ecs.HasComponent<ObjectMetaData>(m_DeleteAudioEntity))
            {
                auto& metadata = ecs.GetComponent<ObjectMetaData>(m_DeleteAudioEntity);
                if (!metadata.name.empty())
                {
                    ImGui::Text("Entity Name: %s", metadata.name.c_str());
                }
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning: This will stop any currently playing audio!");

            ImGui::Separator();

            if (ImGui::Button("Yes, Remove"))
            {
                RemoveAudioComponentFromEntity(m_DeleteAudioEntity);
                m_ShowDeleteAudioConfirmation = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_ShowDeleteAudioConfirmation = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void AudioImGUI::RemoveAudioComponentFromEntity(EntityID entity)
    {
        auto& ecs = ECS::GetInstance();

        if (!ecs.IsEntityValid(entity) || !ecs.HasComponent<AudioComponent>(entity))
        {
            SetStatusMessage("Cannot remove AudioComponent: Entity invalid or no audio component");
            return;
        }

        // Stop any playing audio first
        auto& audioComp = ecs.GetComponent<AudioComponent>(entity);
        if (audioComp.isPlaying && audioComp.channelId != -1)
        {
            CAudioEngine::StopChannel(audioComp.channelId);
        }

        // Remove the component
        ecs.RemoveComponent<AudioComponent>(entity);

        std::string entityName = "Entity " + std::to_string(entity);
        if (ecs.HasComponent<ObjectMetaData>(entity))
        {
            auto& metadata = ecs.GetComponent<ObjectMetaData>(entity);
            if (!metadata.name.empty())
            {
                entityName = metadata.name;
            }
        }

        SetStatusMessage("Removed AudioComponent from " + entityName);
    }

    void AudioImGUI::RenderAudioTester()
    {
        ImGui::Text("Audio Tester - Quick Sound Testing");
        ImGui::Separator();

        // Sound path input with browse button
        ImGui::Text("Sound File:");
        ImGui::SameLine();
        if (ImGui::Button("Browse##TesterBrowse"))
        {
            m_ShowTesterSoundBrowser = true;
        }

        ImGui::InputText("##TesterSoundPath", m_SoundPath, sizeof(m_SoundPath));

        // Audio file browser popup for tester
        if (m_ShowTesterSoundBrowser)
        {
            ImGui::OpenPopup("Select Audio File##Tester");
        }

        if (ImGui::BeginPopupModal("Select Audio File##Tester", &m_ShowTesterSoundBrowser, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (RenderAudioFileSelector())
            {
				strncpy_s(m_SoundPath, sizeof(m_SoundPath), m_SelectedAudioFile.c_str(), sizeof(m_SoundPath) - 1);
                m_SoundPath[sizeof(m_SoundPath) - 1] = '\0';
                m_ShowTesterSoundBrowser = false;
                ImGui::CloseCurrentPopup();
            }

            if (ImGui::Button("Cancel"))
            {
                m_ShowTesterSoundBrowser = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Audio settings - track if they changed
        bool settingsChanged = false;

        ImGui::SliderFloat("Test Volume", &m_Volume, 0.0f, 1.0f);
        // Note: Volume can be updated in real-time (handled separately)

        if (ImGui::Checkbox("3D Audio##Test", &m_Is3D))
        {
            settingsChanged = true;
        }
        if (ImGui::Checkbox("Looping##Test", &m_IsLooping))
        {
            settingsChanged = true;
        }
        if (ImGui::Checkbox("Streaming##Test", &m_IsStreaming))
        {
            settingsChanged = true;
        }

        if (m_Is3D)
        {
            if (ImGui::InputFloat3("Position", m_Position))
            {
                // 3D position can be updated in real-time
                if (m_TestChannelId != -1 && CAudioEngine::IsPlaying(m_TestChannelId))
                {
                    Vec3 position{ m_Position[0], m_Position[1], m_Position[2] };
                    CAudioEngine::SetChannel3dPosition(m_TestChannelId, position);
                }
            }
        }

        // Show warning if settings changed while playing
        bool isTestPlaying = (m_TestChannelId != -1 && CAudioEngine::IsPlaying(m_TestChannelId));

        if (settingsChanged && isTestPlaying)
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                "Settings changed! Click 'Replay' to apply new settings.");
        }

        ImGui::Separator();

        // Play/Replay button
        const char* buttonText = isTestPlaying ? "Replay" : "Play";
        if (ImGui::Button(buttonText))
        {
            // Stop current audio if playing
            if (m_TestChannelId != -1 && CAudioEngine::IsPlaying(m_TestChannelId))
            {
                CAudioEngine::StopChannel(m_TestChannelId);
                m_TestChannelId = -1;
            }

            // Play with current settings
            m_TestChannelId = PlayTestAudio();
        }

        ImGui::SameLine();

        // Stop button
        if (isTestPlaying)
        {
            if (ImGui::Button("Stop"))
            {
                StopTestAudio();
            }
        }
        else
        {
            ImGui::BeginDisabled();
            ImGui::Button("Stop");
            ImGui::EndDisabled();
        }

        // Status display
        ImGui::Separator();
        ImGui::Text("Test Audio Status:");
        ImGui::Text("Playing: %s", isTestPlaying ? "Yes" : "No");
        if (isTestPlaying)
        {
            ImGui::Text("Channel ID: %d", m_TestChannelId);
            ImGui::Text("Volume: %.2f", m_Volume);
            ImGui::Text("3D Audio: %s", m_Is3D ? "Yes" : "No");
            ImGui::Text("Looping: %s", m_IsLooping ? "Yes" : "No");
            ImGui::Text("Streaming: %s", m_IsStreaming ? "Yes" : "No");
        }

        ImGui::Separator();
        ImGui::Text("Common Audio Files:");
        if (ImGui::Button("test.wav"))
        {
			strncpy_s(m_SoundPath, sizeof(m_SoundPath), "../Resources/Audio/test.wav", sizeof(m_SoundPath) - 1);
        }
    }

    void AudioImGUI::RenderAudioBrowser()
    {
        ImGui::Text("Audio File Browser");
        ImGui::Separator();

        if (ImGui::Button("Refresh Audio Files"))
        {
            RefreshAudioFiles();
            SetStatusMessage("Audio files refreshed");
        }

        ImGui::Separator();

        // Display audio files in a list
        if (ImGui::BeginListBox("Available Audio Files", ImVec2(-1, 300)))
        {
            for (size_t i = 0; i < m_AudioFiles.size(); ++i)
            {
                bool isSelected = (m_SelectedAudioIndex == static_cast<int>(i));
                if (ImGui::Selectable(m_AudioFiles[i].filename.c_str(), isSelected))
                {
                    m_SelectedAudioIndex = static_cast<int>(i);
                    m_SelectedAudioFile = m_AudioFiles[i].fullPath;
                }

                // Show tooltip with full path
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", m_AudioFiles[i].fullPath.c_str());
                }
            }
            ImGui::EndListBox();
        }

        if (m_SelectedAudioIndex >= 0 && m_SelectedAudioIndex < m_AudioFiles.size())
        {
            ImGui::Separator();
            ImGui::Text("Selected: %s", m_AudioFiles[m_SelectedAudioIndex].filename.c_str());
            ImGui::Text("Path: %s", m_AudioFiles[m_SelectedAudioIndex].fullPath.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("Tip: Use the 'Audio Tester' tab to test playback with different settings.");
        }
    }

    void AudioImGUI::ScanForExistingGlobalAudio()
    {
        auto& ecs = ECS::GetInstance();

        // Scan through all entities to find existing GlobalAudioComponent
        for (EntityID entity = 1; entity <= MAX_ENTITIES; ++entity)
        {
            if (ecs.IsEntityValid(entity) && ecs.HasComponent<GlobalAudioComponent>(entity))
            {
                m_GlobalAudioEntity = entity;
                m_HasGlobalAudio = true;
                SetStatusMessage("Found existing Global Audio Entity (ID: " + std::to_string(entity) + ")");
                return; // Found it, no need to continue
            }
        }

        // If we get here, no GlobalAudioComponent was found
        m_HasGlobalAudio = false;
    }

    void AudioImGUI::CreateTestGlobalAudioEntity()
    {
        if (m_HasGlobalAudio) return; // Already created

        auto& ecs = ECS::GetInstance();

        // Create entity for global audio
        m_GlobalAudioEntity = ecs.CreateEntity();
        ecs.AddComponent(m_GlobalAudioEntity, Transform());
        ecs.AddComponent(m_GlobalAudioEntity, ObjectMetaData("GlobalAudio", "Audio", false));
        ecs.AddComponent(m_GlobalAudioEntity, HierarchyComponent());
        ecs.AddComponent(m_GlobalAudioEntity, GlobalTransform());

        // Create and setup GlobalAudioComponent
        GlobalAudioComponent globalAudio;

        // Set default volumes
        globalAudio.SetMusicVolume(0.7f);
        globalAudio.SetSFXVolume(0.8f);
        globalAudio.masterVolume = 1.0f;

        ecs.AddComponent(m_GlobalAudioEntity, globalAudio);

        m_HasGlobalAudio = true;
        SetStatusMessage("Global Audio Entity Created with ID: " + std::to_string(m_GlobalAudioEntity));
    }

    void AudioImGUI::RenderGlobalAudioControls()
    {
        ImGui::Text("Global Audio System");
        ImGui::Separator();

        if (m_HasGlobalAudio)
        {
            auto& ecs = ECS::GetInstance();
            // Check if the entity we think exists is still valid
            if (!ecs.IsEntityValid(m_GlobalAudioEntity) ||
                !ecs.HasComponent<GlobalAudioComponent>(m_GlobalAudioEntity))
            {
                // Our cached entity is invalid, rescan the scene
                m_HasGlobalAudio = false;
                m_GlobalAudioEntity = 0;
            }
        }

        // If we don't have a valid global audio reference, scan for one
        if (!m_HasGlobalAudio)
        {
            ScanForExistingGlobalAudio();
        }

        // Now render based on the current state
        if (!m_HasGlobalAudio)
        {
            if (ImGui::Button("Create Test Global Audio Entity"))
            {
                CreateTestGlobalAudioEntity();
            }
            ImGui::TextWrapped("Click above to create a test GlobalAudioComponent entity.");
            return;
        }

        // Check if entity still exists
        auto& ecs = ECS::GetInstance();
        if (!ecs.IsEntityValid(m_GlobalAudioEntity) || !ecs.HasComponent<GlobalAudioComponent>(m_GlobalAudioEntity))
        {
            m_HasGlobalAudio = false;
            SetStatusMessage("Global Audio Entity was destroyed, please recreate.");
            return;
        }

        auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(m_GlobalAudioEntity);
		UNREFERENCED_PARAMETER(globalAudio);
        ImGui::Text("Entity ID: %u", m_GlobalAudioEntity);
        ImGui::Separator();

        // Render the actual global audio tester
        RenderGlobalAudioTester();
    }

    void AudioImGUI::RenderGlobalAudioTester()
    {
        auto& ecs = ECS::GetInstance();
        auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(m_GlobalAudioEntity);

        // === MUSIC SECTION ===
        if (ImGui::CollapsingHeader("Background Music", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Music Tracks: %zu", globalAudio.music.size());

            // Add new music track
            ImGui::InputText("Music Name##GlobalMusic", m_GlobalMusicName, sizeof(m_GlobalMusicName));

            ImGui::Text("Music Path:");
            ImGui::SameLine();
            if (ImGui::Button("Browse##GlobalMusicBrowse"))
            {
                m_ShowGlobalMusicBrowser = true;
            }
            ImGui::InputText("##GlobalMusicPath", m_GlobalMusicPath, sizeof(m_GlobalMusicPath));

            // Audio file browser popup for global music
            if (m_ShowGlobalMusicBrowser)
            {
                ImGui::OpenPopup("Select Audio File##GlobalMusic");
            }

            if (ImGui::BeginPopupModal("Select Audio File##GlobalMusic", &m_ShowGlobalMusicBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
					strncpy_s(m_GlobalMusicPath, sizeof(m_GlobalMusicPath), m_SelectedAudioFile.c_str(), sizeof(m_GlobalMusicPath) - 1);
                    m_GlobalMusicPath[sizeof(m_GlobalMusicPath) - 1] = '\0';
                    m_ShowGlobalMusicBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowGlobalMusicBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::Button("Add Music Track"))
            {
                if (strlen(m_GlobalMusicName) > 0 && strlen(m_GlobalMusicPath) > 0)
                {
                    globalAudio.AddMusicSource(m_GlobalMusicName, m_GlobalMusicPath);
                    SetStatusMessage("Added music track: " + std::string(m_GlobalMusicName));

                    // Clear input fields after adding
                    memset(m_GlobalMusicName, 0, sizeof(m_GlobalMusicName));
                    memset(m_GlobalMusicPath, 0, sizeof(m_GlobalMusicPath));
                }
            }

            ImGui::Separator();

            // Music controls
            if (ImGui::SliderFloat("Music Volume", &globalAudio.musicVolume, 0.0f, 1.0f))
            {
                globalAudio.SetMusicVolume(globalAudio.musicVolume);
            }

            // List existing music tracks with edit/delete options
            if (!globalAudio.music.empty())
            {
                ImGui::Text("Existing Music Tracks:");

                for (size_t i = 0; i < globalAudio.music.size(); ++i)
                {
                    const auto& music = globalAudio.music[i];
                    ImGui::PushID(("music_" + std::to_string(i)).c_str());

                    // Track info with editing capability
                    if (m_EditingMusicIndex == static_cast<int>(i))
                    {
                        // Edit mode
                        ImGui::Text("Editing Track %zu:", i);
                        ImGui::InputText("Name##EditMusic", m_EditMusicName, sizeof(m_EditMusicName));

                        ImGui::Text("Path:");
                        ImGui::SameLine();
                        if (ImGui::Button("Browse##EditMusicBrowse"))
                        {
                            m_ShowEditMusicBrowser = true;
                        }
                        ImGui::InputText("##EditMusicPath", m_EditMusicPath, sizeof(m_EditMusicPath));

                        // Save/Cancel buttons
                        if (ImGui::Button("Save"))
                        {
                            if (strlen(m_EditMusicName) > 0 && strlen(m_EditMusicPath) > 0)
                            {
                                // Update the music track (you'll need to add this method to GlobalAudioComponent)
                                globalAudio.UpdateMusicSource(i, m_EditMusicName, m_EditMusicPath);
                                SetStatusMessage("Updated music track: " + std::string(m_EditMusicName));
                                m_EditingMusicIndex = -1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                        {
                            m_EditingMusicIndex = -1;
                        }
                    }
                    else
                    {
                        // Display mode
                        ImGui::Text("Track %zu: %s", i, music.audioName.c_str());
                        ImGui::SameLine();

                        // Control buttons
                        if (ImGui::Button("Play"))
                        {
                            globalAudio.PlayMusic(static_cast<int>(i));
                            SetStatusMessage("Playing music: " + music.audioName);
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Stop"))
                        {
                            globalAudio.StopMusic();
                            SetStatusMessage("Stopped music");
                        }
                        ImGui::SameLine();

                        // Edit button
                        if (ImGui::Button("Edit"))
                        {
                            m_EditingMusicIndex = static_cast<int>(i);
							strncpy_s(m_EditMusicName, sizeof(m_EditMusicName), music.audioName.c_str(), sizeof(m_EditMusicName) - 1);
							strncpy_s(m_EditMusicPath, sizeof(m_EditMusicPath), music.audioPath.c_str(), sizeof(m_EditMusicPath) - 1);
                            m_EditMusicName[sizeof(m_EditMusicName) - 1] = '\0';
                            m_EditMusicPath[sizeof(m_EditMusicPath) - 1] = '\0';
                        }
                        ImGui::SameLine();

                        // Delete button
                        if (ImGui::Button("Delete"))
                        {
                            m_ShowDeleteConfirmation = true;
                            m_DeleteTargetIndex = static_cast<int>(i);
                            m_DeletingMusic = true;
                        }

                        // Show tooltip with full path
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Path: %s", music.audioPath.c_str());
                        }
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }
            }

            // Edit music browser popup
            if (m_ShowEditMusicBrowser)
            {
                ImGui::OpenPopup("Select Audio File##EditMusic");
            }

            if (ImGui::BeginPopupModal("Select Audio File##EditMusic", &m_ShowEditMusicBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
					strncpy_s(m_EditMusicPath, sizeof(m_EditMusicPath), m_SelectedAudioFile.c_str(), sizeof(m_EditMusicPath) - 1);
                    m_EditMusicPath[sizeof(m_EditMusicPath) - 1] = '\0';
                    m_ShowEditMusicBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowEditMusicBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // Current music status
            ImGui::Separator();
            ImGui::Text("Current Music Index: %d", globalAudio.currentMusicIndex);
            ImGui::Text("Current Music Channel: %d", globalAudio.currentMusicChannelId);
            bool isMusicPlaying = (globalAudio.currentMusicChannelId != -1 &&
                CAudioEngine::IsPlaying(globalAudio.currentMusicChannelId));
            ImGui::Text("Music Playing: %s", isMusicPlaying ? "Yes" : "No");
        }

        // === SFX SECTION === (similar implementation for SFX)
        if (ImGui::CollapsingHeader("Sound Effects", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("SFX Tracks: %zu", globalAudio.sfx.size());

            // Add new SFX track
            ImGui::InputText("SFX Name##GlobalSFX", m_GlobalSFXName, sizeof(m_GlobalSFXName));

            ImGui::Text("SFX Path:");
            ImGui::SameLine();
            if (ImGui::Button("Browse##GlobalSFXBrowse"))
            {
                m_ShowGlobalSFXBrowser = true;
            }
            ImGui::InputText("##GlobalSFXPath", m_GlobalSFXPath, sizeof(m_GlobalSFXPath));

            // Audio file browser popup for global SFX
            if (m_ShowGlobalSFXBrowser)
            {
                ImGui::OpenPopup("Select Audio File##GlobalSFX");
            }

            if (ImGui::BeginPopupModal("Select Audio File##GlobalSFX", &m_ShowGlobalSFXBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
					strncpy_s(m_GlobalSFXPath, sizeof(m_GlobalSFXPath), m_SelectedAudioFile.c_str(), sizeof(m_GlobalSFXPath) - 1);
                    m_GlobalSFXPath[sizeof(m_GlobalSFXPath) - 1] = '\0';
                    m_ShowGlobalSFXBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowGlobalSFXBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::Button("Add SFX Track"))
            {
                if (strlen(m_GlobalSFXName) > 0 && strlen(m_GlobalSFXPath) > 0)
                {
                    globalAudio.AddSFXSource(m_GlobalSFXName, m_GlobalSFXPath);
                    SetStatusMessage("Added SFX track: " + std::string(m_GlobalSFXName));

                    // Clear input fields after adding
                    memset(m_GlobalSFXName, 0, sizeof(m_GlobalSFXName));
                    memset(m_GlobalSFXPath, 0, sizeof(m_GlobalSFXPath));
                }
            }

            ImGui::Separator();

            // SFX volume control
            if (ImGui::SliderFloat("SFX Volume", &globalAudio.sfxVolume, 0.0f, 1.0f))
            {
                globalAudio.SetSFXVolume(globalAudio.sfxVolume);
            }

            // List existing SFX tracks with edit/delete options
            if (!globalAudio.sfx.empty())
            {
                ImGui::Text("Existing SFX Tracks:");

                for (size_t i = 0; i < globalAudio.sfx.size(); ++i)
                {
                    auto& sfx = globalAudio.sfx[i];  // Changed to non-const so we can modify volume
                    ImGui::PushID(("sfx_" + std::to_string(i)).c_str());

                    // Track info with editing capability
                    if (m_EditingSFXIndex == static_cast<int>(i))
                    {
                        // Edit mode
                        ImGui::Text("Editing SFX %zu:", i);
                        ImGui::InputText("Name##EditSFX", m_EditSFXName, sizeof(m_EditSFXName));

                        ImGui::Text("Path:");
                        ImGui::SameLine();
                        if (ImGui::Button("Browse##EditSFXBrowse"))
                        {
                            m_ShowEditSFXBrowser = true;
                        }
                        ImGui::InputText("##EditSFXPath", m_EditSFXPath, sizeof(m_EditSFXPath));

                        // Save/Cancel buttons
                        if (ImGui::Button("Save"))
                        {
                            if (strlen(m_EditSFXName) > 0 && strlen(m_EditSFXPath) > 0)
                            {
                                // Update the SFX track
                                globalAudio.UpdateSFXSource(i, m_EditSFXName, m_EditSFXPath);
                                SetStatusMessage("Updated SFX track: " + std::string(m_EditSFXName));
                                m_EditingSFXIndex = -1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                        {
                            m_EditingSFXIndex = -1;
                        }
                    }
                    else
                    {
                        // Display mode
                        ImGui::Text("SFX %zu: %s", i, sfx.audioName.c_str());

                        // *** NEW: Individual SFX Volume Slider ***
                        ImGui::PushItemWidth(150.0f);
                        if (ImGui::SliderFloat(("Volume##SFX" + std::to_string(i)).c_str(),
                            &sfx.volume, 0.0f, 1.0f, "%.2f"))
                        {
                            SetStatusMessage("Changed " + sfx.audioName + " volume to " +
                                std::to_string(sfx.volume));
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine();
                        if (ImGui::Button("Play by Index"))
                        {
                            globalAudio.PlaySFX(static_cast<int>(i));
                            SetStatusMessage("Playing SFX: " + sfx.audioName);
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Play by Name"))
                        {
                            globalAudio.PlaySFX(sfx.audioName);
                            SetStatusMessage("Playing SFX by name: " + sfx.audioName);
                        }
                        ImGui::SameLine();

                        // Edit button
                        if (ImGui::Button("Edit"))
                        {
                            m_EditingSFXIndex = static_cast<int>(i);
                            strncpy_s(m_EditSFXName, sizeof(m_EditSFXName),
                                sfx.audioName.c_str(), sizeof(m_EditSFXName) - 1);
                            strncpy_s(m_EditSFXPath, sizeof(m_EditSFXPath),
                                sfx.audioPath.c_str(), sizeof(m_EditSFXPath) - 1);
                            m_EditSFXName[sizeof(m_EditSFXName) - 1] = '\0';
                            m_EditSFXPath[sizeof(m_EditSFXPath) - 1] = '\0';
                        }
                        ImGui::SameLine();

                        // Delete button
                        if (ImGui::Button("Delete"))
                        {
                            m_ShowDeleteConfirmation = true;
                            m_DeleteTargetIndex = static_cast<int>(i);
                            m_DeletingMusic = false;
                        }

                        // Show tooltip with full path and volume info
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Path: %s\nVolume: %.2f",
                                sfx.audioPath.c_str(), sfx.volume);
                        }
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }
            }

            // Edit SFX browser popup
            if (m_ShowEditSFXBrowser)
            {
                ImGui::OpenPopup("Select Audio File##EditSFX");
            }

            if (ImGui::BeginPopupModal("Select Audio File##EditSFX", &m_ShowEditSFXBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
					strncpy_s(m_EditSFXPath, sizeof(m_EditSFXPath), m_SelectedAudioFile.c_str(), sizeof(m_EditSFXPath) - 1);
                    m_EditSFXPath[sizeof(m_EditSFXPath) - 1] = '\0';
                    m_ShowEditSFXBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowEditSFXBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        // === AMBIENCE SECTION ===
        if (ImGui::CollapsingHeader("Ambient Sounds", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Ambience Tracks: %zu", globalAudio.ambience.size());

            // Add new ambience track
            ImGui::InputText("Ambience Name##GlobalAmbience", m_GlobalAmbienceName, sizeof(m_GlobalAmbienceName));

            ImGui::Text("Ambience Path:");
            ImGui::SameLine();
            if (ImGui::Button("Browse##GlobalAmbienceBrowse"))
            {
                m_ShowGlobalAmbienceBrowser = true;
            }
            ImGui::InputText("##GlobalAmbiencePath", m_GlobalAmbiencePath, sizeof(m_GlobalAmbiencePath));

            // Audio file browser popup for global ambience
            if (m_ShowGlobalAmbienceBrowser)
            {
                ImGui::OpenPopup("Select Audio File##GlobalAmbience");
            }

            if (ImGui::BeginPopupModal("Select Audio File##GlobalAmbience", &m_ShowGlobalAmbienceBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
                    strncpy_s(m_GlobalAmbiencePath, sizeof(m_GlobalAmbiencePath), m_SelectedAudioFile.c_str(), sizeof(m_GlobalAmbiencePath) - 1);
                    m_GlobalAmbiencePath[sizeof(m_GlobalAmbiencePath) - 1] = '\0';
                    m_ShowGlobalAmbienceBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowGlobalAmbienceBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            if (ImGui::Button("Add Ambience Track"))
            {
                if (strlen(m_GlobalAmbienceName) > 0 && strlen(m_GlobalAmbiencePath) > 0)
                {
                    globalAudio.AddAmbienceSource(m_GlobalAmbienceName, m_GlobalAmbiencePath);
                    SetStatusMessage("Added ambience track: " + std::string(m_GlobalAmbienceName));

                    memset(m_GlobalAmbienceName, 0, sizeof(m_GlobalAmbienceName));
                    memset(m_GlobalAmbiencePath, 0, sizeof(m_GlobalAmbiencePath));
                }
            }

            ImGui::Separator();

            // Ambience volume control
            if (ImGui::SliderFloat("Ambience Volume", &globalAudio.ambienceVolume, 0.0f, 1.0f))
            {
                globalAudio.SetAmbienceVolume(globalAudio.ambienceVolume);
            }

            // List existing ambience tracks
            if (!globalAudio.ambience.empty())
            {
                ImGui::Text("Existing Ambience Tracks:");

                for (size_t i = 0; i < globalAudio.ambience.size(); ++i)
                {
                    const auto& ambience = globalAudio.ambience[i];
                    ImGui::PushID(("ambience_" + std::to_string(i)).c_str());

                    if (m_EditingAmbienceIndex == static_cast<int>(i))
                    {
                        // Edit mode
                        ImGui::Text("Editing Ambience %zu:", i);
                        ImGui::InputText("Name##EditAmbience", m_EditAmbienceName, sizeof(m_EditAmbienceName));

                        ImGui::Text("Path:");
                        ImGui::SameLine();
                        if (ImGui::Button("Browse##EditAmbienceBrowse"))
                        {
                            m_ShowEditAmbienceBrowser = true;
                        }
                        ImGui::InputText("##EditAmbiencePath", m_EditAmbiencePath, sizeof(m_EditAmbiencePath));

                        if (ImGui::Button("Save"))
                        {
                            if (strlen(m_EditAmbienceName) > 0 && strlen(m_EditAmbiencePath) > 0)
                            {
                                globalAudio.UpdateAmbienceSource(i, m_EditAmbienceName, m_EditAmbiencePath);
                                SetStatusMessage("Updated ambience track: " + std::string(m_EditAmbienceName));
                                m_EditingAmbienceIndex = -1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                        {
                            m_EditingAmbienceIndex = -1;
                        }
                    }
                    else
                    {
                        // Display mode
                        ImGui::Text("Ambience %zu: %s", i, ambience.audioName.c_str());
                        ImGui::SameLine();

                        if (ImGui::Button("Play by Index"))
                        {
                            globalAudio.PlayAmbience(static_cast<int>(i));
                            SetStatusMessage("Playing ambience: " + ambience.audioName);
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Play by Name"))
                        {
                            globalAudio.PlayAmbience(ambience.audioName);
                            SetStatusMessage("Playing ambience by name: " + ambience.audioName);
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Stop"))
                        {
                            globalAudio.StopAmbience();
                            SetStatusMessage("Stopped ambience");
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Edit"))
                        {
                            m_EditingAmbienceIndex = static_cast<int>(i);
                            strncpy_s(m_EditAmbienceName, sizeof(m_EditAmbienceName), ambience.audioName.c_str(), sizeof(m_EditAmbienceName) - 1);
                            strncpy_s(m_EditAmbiencePath, sizeof(m_EditAmbiencePath), ambience.audioPath.c_str(), sizeof(m_EditAmbiencePath) - 1);
                            m_EditAmbienceName[sizeof(m_EditAmbienceName) - 1] = '\0';
                            m_EditAmbiencePath[sizeof(m_EditAmbiencePath) - 1] = '\0';
                        }
                        ImGui::SameLine();

                        if (ImGui::Button("Delete"))
                        {
                            m_ShowDeleteConfirmation = true;
                            m_DeleteTargetIndex = static_cast<int>(i);
                            m_DeleteTargetType = 2; // Ambience
                        }

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Path: %s", ambience.audioPath.c_str());
                        }
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }
            }

            // Edit ambience browser popup
            if (m_ShowEditAmbienceBrowser)
            {
                ImGui::OpenPopup("Select Audio File##EditAmbience");
            }

            if (ImGui::BeginPopupModal("Select Audio File##EditAmbience", &m_ShowEditAmbienceBrowser))
            {
                RenderAudioBrowser();

                if (ImGui::Button("Select") && !m_SelectedAudioFile.empty())
                {
                    strncpy_s(m_EditAmbiencePath, sizeof(m_EditAmbiencePath), m_SelectedAudioFile.c_str(), sizeof(m_EditAmbiencePath) - 1);
                    m_EditAmbiencePath[sizeof(m_EditAmbiencePath) - 1] = '\0';
                    m_ShowEditAmbienceBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    m_ShowEditAmbienceBrowser = false;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            // Current ambience status
            ImGui::Separator();
            ImGui::Text("Current Ambience Index: %d", globalAudio.currentAmbienceIndex);
            ImGui::Text("Current Ambience Channel: %d", globalAudio.currentAmbienceChannelId);
            bool isAmbiencePlaying = (globalAudio.currentAmbienceChannelId != -1 &&
                CAudioEngine::IsPlaying(globalAudio.currentAmbienceChannelId));
            ImGui::Text("Ambience Playing: %s", isAmbiencePlaying ? "Yes" : "No");
        }

        // === DELETE CONFIRMATION POPUP ===
        if (m_ShowDeleteConfirmation)
        {
            ImGui::OpenPopup("Confirm Delete");
        }

        if (ImGui::BeginPopupModal("Confirm Delete", &m_ShowDeleteConfirmation, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const char* itemType = (m_DeleteTargetType == 0) ? "music track" :
                (m_DeleteTargetType == 1) ? "SFX track" : "ambience track";

            const auto& targetItem = (m_DeleteTargetType == 0) ? globalAudio.music[m_DeleteTargetIndex] :
                (m_DeleteTargetType == 1) ? globalAudio.sfx[m_DeleteTargetIndex] :
                globalAudio.ambience[m_DeleteTargetIndex];

            ImGui::Text("Are you sure you want to delete this %s?", itemType);
            ImGui::Text("Name: %s", targetItem.audioName.c_str());
            ImGui::Text("Path: %s", targetItem.audioPath.c_str());
            ImGui::Separator();

            if (ImGui::Button("Yes, Delete"))
            {
                if (m_DeleteTargetType == 0) // Music
                {
                    if (globalAudio.currentMusicIndex == m_DeleteTargetIndex)
                        globalAudio.StopMusic();

                    globalAudio.RemoveMusicSource(m_DeleteTargetIndex);
                    SetStatusMessage("Deleted music track: " + targetItem.audioName);

                    if (m_EditingMusicIndex == m_DeleteTargetIndex)
                        m_EditingMusicIndex = -1;
                    else if (m_EditingMusicIndex > m_DeleteTargetIndex)
                        m_EditingMusicIndex--;
                }
                else if (m_DeleteTargetType == 1) // SFX
                {
                    globalAudio.RemoveSFXSource(m_DeleteTargetIndex);
                    SetStatusMessage("Deleted SFX track: " + targetItem.audioName);

                    if (m_EditingSFXIndex == m_DeleteTargetIndex)
                        m_EditingSFXIndex = -1;
                    else if (m_EditingSFXIndex > m_DeleteTargetIndex)
                        m_EditingSFXIndex--;
                }
                else // Ambience
                {
                    if (globalAudio.currentAmbienceIndex == m_DeleteTargetIndex)
                        globalAudio.StopAmbience();

                    globalAudio.RemoveAmbienceSource(m_DeleteTargetIndex);
                    SetStatusMessage("Deleted ambience track: " + targetItem.audioName);

                    if (m_EditingAmbienceIndex == m_DeleteTargetIndex)
                        m_EditingAmbienceIndex = -1;
                    else if (m_EditingAmbienceIndex > m_DeleteTargetIndex)
                        m_EditingAmbienceIndex--;
                }

                m_ShowDeleteConfirmation = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                m_ShowDeleteConfirmation = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // === MASTER CONTROLS ===
        if (ImGui::CollapsingHeader("Master Controls", ImGuiTreeNodeFlags_DefaultOpen))
        {

            if (ImGui::Button("Reload Audio System"))
            {
                AudioSystem::Shutdown();
                AudioSystem::Init();
                SetStatusMessage("Audio System Reloaded");
            }

            if (ImGui::SliderFloat("Master Volume", &globalAudio.masterVolume, 0.0f, 1.0f))
            {
                // Update currently playing music with new master volume
                globalAudio.SetMusicVolume(globalAudio.musicVolume);
            }

            ImGui::Separator();

            // Quick test buttons
            if (ImGui::Button("Test Music Switch"))
            {
                if (globalAudio.music.size() >= 2)
                {
                    int nextTrack = (globalAudio.currentMusicIndex + 1) % static_cast<int>(globalAudio.music.size());
                    globalAudio.PlayMusic(nextTrack);
                    SetStatusMessage("Switched to music track " + std::to_string(nextTrack));
                }
                else
                {
                    SetStatusMessage("Need at least 2 music tracks to test switching");
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Stop All Music"))
            {
                globalAudio.StopMusic();
                SetStatusMessage("Stopped all music");
            }

            if (ImGui::Button("Play Random SFX"))
            {
                if (!globalAudio.sfx.empty())
                {
                    int randomIndex = rand() % static_cast<int>(globalAudio.sfx.size());
                    globalAudio.PlaySFX(randomIndex);
                    SetStatusMessage("Playing random SFX: " + globalAudio.sfx[randomIndex].audioName);
                }
                else
                {
                    SetStatusMessage("No SFX tracks available");
                }
            }

            // Debug info
            ImGui::Separator();
            ImGui::Text("=== Debug Info ===");
            ImGui::Text("Music Tracks: %zu", globalAudio.music.size());
            ImGui::Text("SFX Tracks: %zu", globalAudio.sfx.size());
            ImGui::Text("Master Volume: %.2f", globalAudio.masterVolume);
            ImGui::Text("Music Volume: %.2f", globalAudio.musicVolume);
            ImGui::Text("SFX Volume: %.2f", globalAudio.sfxVolume);

            // Update global audio component
            AudioSystem::UpdateGlobalAudio(globalAudio);
        }

        // Keyboard shortcuts info
        ImGui::Separator();
        ImGui::TextWrapped("Tip: You can also test GlobalAudioComponent functionality by creating "
            "keyboard shortcuts in your main game loop using the member functions like "
            "globalAudio.PlayMusic(index) and globalAudio.PlaySFX(\"name\").");
    }

    bool AudioImGUI::RenderAudioFileSelector()
    {
        ImGui::Text("Select an audio file:");
        ImGui::Separator();

        if (ImGui::Button("Refresh"))
        {
            RefreshAudioFiles();
        }

        bool fileSelected = false;

        if (ImGui::BeginListBox("##AudioFileList", ImVec2(400, 200)))
        {
            for (size_t i = 0; i < m_AudioFiles.size(); ++i)
            {
                bool isSelected = (m_SelectedAudioIndex == static_cast<int>(i));
                if (ImGui::Selectable(m_AudioFiles[i].filename.c_str(), isSelected))
                {
                    m_SelectedAudioIndex = static_cast<int>(i);
                    m_SelectedAudioFile = m_AudioFiles[i].fullPath;
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", m_AudioFiles[i].fullPath.c_str());
                }

                // Double-click to select
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                {
                    m_SelectedAudioIndex = static_cast<int>(i);
                    m_SelectedAudioFile = m_AudioFiles[i].fullPath;
                    fileSelected = true;
                }
            }
            ImGui::EndListBox();
        }

        if (m_SelectedAudioIndex >= 0 && m_SelectedAudioIndex < m_AudioFiles.size())
        {
            ImGui::Separator();
            ImGui::Text("Selected: %s", m_AudioFiles[m_SelectedAudioIndex].filename.c_str());

            if (ImGui::Button("Select"))
            {
                fileSelected = true;
            }
        }

        return fileSelected;
    }

    void AudioImGUI::RefreshAudioFiles()
    {
        m_AudioFiles.clear();
        m_SelectedAudioIndex = -1;
        m_SelectedAudioFile.clear();

        // Scan for audio files in the Resources/Audio directory
        std::string audioDir = "../Resources/Audio/";

        if (fs::exists(audioDir) && fs::is_directory(audioDir))
        {
            for (const auto& entry : fs::directory_iterator(audioDir))
            {
                if (entry.is_regular_file())
                {
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

                    // Check for audio file extensions
                    if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
                    {
                        AudioFileInfo info;
                        info.filename = entry.path().filename().string();
                        info.fullPath = entry.path().string();
                        m_AudioFiles.push_back(info);
                    }
                }
            }
        }

        // Sort files alphabetically
        std::sort(m_AudioFiles.begin(), m_AudioFiles.end(),
            [](const AudioFileInfo& a, const AudioFileInfo& b) {
                return a.filename < b.filename;
            });
    }

    void AudioImGUI::SetStatusMessage(const std::string& message, float duration)
    {
        m_StatusMessage = message;
        m_StatusTimer = duration;
    }

    void AudioImGUI::UpdateStatus()
    {
        if (m_StatusTimer > 0.0f)
        {
            m_StatusTimer -= FrameController::GetDeltaTime();
            if (m_StatusTimer <= 0.0f)
            {
                m_StatusMessage.clear();
            }
        }
    }

    std::vector<EntityID> AudioImGUI::GetEntitiesWithAudioComponent()
    {
        std::vector<EntityID> audioEntities;
        auto& ecs = ECS::GetInstance();

        // Get the audio system to access its tracked entities
        auto audioSystem = ecs.GetSystem<AudioSystem>();
        if (audioSystem)
        {
            // Access the entities tracked by the AudioSystem
            for (EntityID entity : audioSystem->GetEntities())
            {
                if (ecs.IsEntityValid(entity) && ecs.HasComponent<AudioComponent>(entity))
                {
                    audioEntities.push_back(entity);
                }
            }
        }

        return audioEntities;
    }

    int AudioImGUI::PlayTestAudio()
    {
        if (strlen(m_SoundPath) == 0)
        {
            SetStatusMessage("Please enter a sound file path");
            return -1; // Return -1 to indicate failure
        }

        try
        {
            // Load the sound
            CAudioEngine::LoadSound(m_SoundPath, m_Is3D, m_IsLooping, m_IsStreaming);

            // Play the sound
            Vec3 position = m_Is3D ? Vec3{ m_Position[0], m_Position[1], m_Position[2] } : Vec3{ 0, 0, 0 };
            int channelId = CAudioEngine::PlaySounds(m_SoundPath, position, m_Volume);

            if (channelId != -1)
            {
                SetStatusMessage("Playing test audio: " + std::string(m_SoundPath));
                return channelId; // Return the successful channel ID
            }
            else
            {
                SetStatusMessage("Failed to play audio");
                return -1; // Return -1 to indicate failure
            }
        }
        catch (const std::exception& e)
        {
            SetStatusMessage("Error playing audio: " + std::string(e.what()));
            return -1; // Return -1 to indicate failure
        }
    }

    void AudioImGUI::StopAllAudio()
    {
        CAudioEngine::StopAllChannels();
        SetStatusMessage("All audio channels stopped");
    }

    void AudioImGUI::StopTestAudio()
    {
        if (m_TestChannelId != -1)
        {
            CAudioEngine::StopChannel(m_TestChannelId);
            m_TestChannelId = -1;
            SetStatusMessage("Test audio stopped");
        }
        else
        {
            SetStatusMessage("No test audio playing");
        }
    }
}