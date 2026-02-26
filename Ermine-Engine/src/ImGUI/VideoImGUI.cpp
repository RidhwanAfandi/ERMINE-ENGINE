/* Start Header ************************************************************************/
/*!
\file       VideoImGUI.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\date       01/29/2026
\brief      ImGui window for video playback controls and render settings.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "VideoImGUI.h"

#include "VideoManager.h"
#include "ECS.h"

#include "imgui.h"

namespace Ermine
{
    /**
     * @brief Construct the video UI window with default labels.
     */
    VideoImGUI::VideoImGUI()
        : ImGUIWindow("Video Player")
    {
    }

    /**
     * @brief Build the ImGui controls for video playback and settings.
     */
    void VideoImGUI::Update()
    {
        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
        {
            ImGui::End();
            return;
        }

        auto videoSystem = ECS::GetInstance().GetSystem<VideoManager>();
        if (!videoSystem)
        {
            ImGui::TextUnformatted("VideoManager not registered.");
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted("Load Video");
        ImGui::InputText("Name", m_videoName, sizeof(m_videoName));
        ImGui::InputText("File Path", m_videoPath, sizeof(m_videoPath));
        ImGui::Checkbox("Loop", &m_loop);

        if (ImGui::Button("Load"))
        {
            videoSystem->LoadVideo(m_videoName, m_videoPath, m_loop);
        }
        ImGui::SameLine();
        if (ImGui::Button("Set Current"))
        {
            videoSystem->SetCurrentVideo(m_videoName);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Playback");

        bool playing = videoSystem->IsVideoPlaying();
        if (ImGui::Button(playing ? "Pause" : "Play"))
        {
            if (playing)
                videoSystem->Pause();
            else
                videoSystem->Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            videoSystem->Stop();
        bool renderEnabled = videoSystem->IsRenderEnabled();
        if (ImGui::Checkbox("Render Video", &renderEnabled))
            videoSystem->SetRenderEnabled(renderEnabled);

        const char* fitModes[] = { "Aspect-Fit", "Stretch-To-Fill" };
        int currentIndex = static_cast<int>(videoSystem->GetFitMode());
        if (m_fitModeIndex != currentIndex)
            m_fitModeIndex = currentIndex;
        if (ImGui::Combo("Fit Mode", &m_fitModeIndex, fitModes, IM_ARRAYSIZE(fitModes)))
        {
            auto mode = (m_fitModeIndex == 1)
                ? VideoManager::VideoFitMode::StretchToFill
                : VideoManager::VideoFitMode::AspectFit;
            videoSystem->SetFitMode(mode);
        }

        const auto& current = videoSystem->GetCurrentVideo();
        ImGui::Text("Current: %s", current.empty() ? "(none)" : current.c_str());

        auto it = videoSystem->GetVideos().find(current);
        if (it != videoSystem->GetVideos().end() && it->second)
        {
            const auto& data = *it->second;
            ImGui::Text("Frame: %d / %d", data.currentFrameIndex + 1, data.totalFrames);
            ImGui::Text("Size: %u x %u", data.width, data.height);
            ImGui::Text("Loaded: %s", data.loaded ? "Yes" : "No");
            ImGui::Text("Done: %s", data.done ? "Yes" : "No");
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Loaded Videos");
        for (const auto& entry : videoSystem->GetVideos())
        {
            const bool selected = (entry.first == current);
            if (ImGui::Selectable(entry.first.c_str(), selected))
            {
                videoSystem->SetCurrentVideo(entry.first);
            }
        }

        if (!current.empty())
        {
            if (ImGui::Button("Unload Current"))
            {
                videoSystem->FreeVideo(current);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload All"))
        {
            videoSystem->CleanupAllVideos();
        }

        ImGui::End();
    }

    /**
     * @brief Render hook (handled in Update).
     */
    void VideoImGUI::Render()
    {
    }
}
