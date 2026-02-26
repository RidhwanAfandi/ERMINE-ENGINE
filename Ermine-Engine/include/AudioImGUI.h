/* Start Header ************************************************************************/
/*!
\file       AudioImGUI.h
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       15/9/2025
\brief      This file contains the declaration of AudioImGUI for managing audio through ImGUI.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include "ImGuiUIWindow.h"
#include "imgui.h"
#include "AudioSystem.h"
#include "ECS.h"
#include "Components.h"

namespace Ermine
{

    struct AudioFileInfo
    {
        std::string filename;
        std::string fullPath;
    };

    class AudioImGUI : public ImGUIWindow
    {
    public:
        AudioImGUI();
        ~AudioImGUI();

        void Update() override;
        void Render() override;

    private:
        // GUI state variables
        char m_SoundPath[256] = "../Resources/Audio/";
        float m_Volume = 0.5f;
        bool m_Is3D = false;
        bool m_IsLooping = false;
        bool m_IsStreaming = false;
        float m_Position[3] = { 0.0f, 0.0f, 0.0f };

        EntityID m_SelectedEntity = 0;
        bool m_ShowEntityList = true;

        // Status messages
        std::string m_StatusMessage;
        float m_StatusTimer = 0.0f;

        // Audio management functions
        void RenderEntityAudioControls();
        void RenderGlobalAudioControls();
        void RenderAudioTester();
        void SetStatusMessage(const std::string& message, float duration = 3.0f);
        void UpdateStatus();
        void ScanForExistingGlobalAudio();

        // Helper functions
        std::vector<EntityID> GetEntitiesWithAudioComponent();
        int PlayTestAudio(); // Returns channel ID
        void StopAllAudio();
        void StopTestAudio();


        int m_TestChannelId = -1;  // Track the test audio channel
        float m_PreviousTestVolume = -1.0f;  // Track volume changes

        std::vector<AudioFileInfo> m_AudioFiles;
        int m_SelectedAudioIndex = -1;
        std::string m_SelectedAudioFile;
        int m_BrowserTestChannelId = -1;

        // Popup control flags
        bool m_ShowEntitySoundBrowser = false;
        bool m_ShowTesterSoundBrowser = false;
        bool m_ShowGlobalSFXBrowser = false;
        bool m_ShowGlobalMusicBrowser = false;
        bool m_ShowGlobalAmbienceBrowser = false;
        bool m_ShowEditAmbienceBrowser = false;

        int m_EditingMusicIndex = -1;
        int m_EditingSFXIndex = -1;
        char m_EditMusicName[256] = "";
        char m_EditMusicPath[256] = "";
        char m_EditSFXName[256] = "";
        char m_EditSFXPath[256] = "";
        char m_EditAmbienceName[256] = "";
        char m_EditAmbiencePath[256] = "";
        bool m_ShowEditMusicBrowser = false;
        bool m_ShowEditSFXBrowser = false;
        bool m_ShowDeleteConfirmation = false;
        int m_DeleteTargetIndex = -1;
        int m_EditingAmbienceIndex = -1;
        bool m_DeletingMusic = true;
        int m_DeleteTargetType = 0; // 0 = Music, 1 = SFX, 2 = Ambience

        // Add these private methods to your AudioImGUI class:
        void RenderAudioBrowser();
        bool RenderAudioFileSelector();
        void RefreshAudioFiles();


        EntityID m_GlobalAudioEntity = 0;
        bool m_HasGlobalAudio = false;
        char m_GlobalMusicPath[256] = "../Resources/Audio/";
        char m_GlobalSFXPath[256] = "../Resources/Audio/";
        char m_GlobalAmbienceName[256] = "";
        char m_GlobalAmbiencePath[512] = "";
        char m_GlobalMusicName[128] = "test_music";
        char m_GlobalSFXName[128] = "test_sfx";
        void CreateTestGlobalAudioEntity();
        void RenderGlobalAudioTester();

        // Entity audio management
        bool m_ShowDeleteAudioConfirmation = false;
        EntityID m_DeleteAudioEntity = 0;

        // You may also want to add these function declarations to the public/private sections:
        std::vector<EntityID> GetAllEntities();
        void AddAudioComponentToEntity(EntityID entity);
        void RemoveAudioComponentFromEntity(EntityID entity);
        void RenderDeleteAudioConfirmationPopup();

    };
}