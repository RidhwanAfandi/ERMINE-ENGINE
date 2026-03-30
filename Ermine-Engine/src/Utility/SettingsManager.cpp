/* Start Header ************************************************************************/
/*!
\file       SettingsManager.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       Mar 18, 2026
\brief      Implementation of persistent user settings manager.
            Loads/saves audio and gamma settings to a JSON file so they persist
            across scene changes and game restarts.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "SettingsManager.h"
#include "ECS.h"
#include "Components.h"
#include "Renderer.h"
#include "Logger.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <fstream>
#include <sstream>

namespace Ermine
{
    SettingsManager& SettingsManager::GetInstance()
    {
        static SettingsManager instance;
        return instance;
    }

    void SettingsManager::Load(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            EE_CORE_INFO("No user_settings.json found, using defaults");
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string jsonStr = buffer.str();
        file.close();

        rapidjson::Document doc;
        doc.Parse(jsonStr.c_str());

        if (doc.HasParseError() || !doc.IsObject())
        {
            EE_CORE_WARN("Failed to parse user_settings.json, using defaults");
            return;
        }

        if (doc.HasMember("masterVolume") && doc["masterVolume"].IsFloat())
            masterVolume = doc["masterVolume"].GetFloat();
        if (doc.HasMember("musicVolume") && doc["musicVolume"].IsFloat())
            musicVolume = doc["musicVolume"].GetFloat();
        if (doc.HasMember("sfxVolume") && doc["sfxVolume"].IsFloat())
            sfxVolume = doc["sfxVolume"].GetFloat();
        if (doc.HasMember("ambienceVolume") && doc["ambienceVolume"].IsFloat())
            ambienceVolume = doc["ambienceVolume"].GetFloat();
        if (doc.HasMember("gammaSliderValue") && doc["gammaSliderValue"].IsFloat())
            gammaSliderValue = doc["gammaSliderValue"].GetFloat();

        EE_CORE_INFO("User settings loaded: master={}, music={}, sfx={}, ambience={}, gamma={}",
            masterVolume, musicVolume, sfxVolume, ambienceVolume, gammaSliderValue);
    }

    void SettingsManager::Save(const std::filesystem::path& path)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& alloc = doc.GetAllocator();

        doc.AddMember("masterVolume", masterVolume, alloc);
        doc.AddMember("musicVolume", musicVolume, alloc);
        doc.AddMember("sfxVolume", sfxVolume, alloc);
        doc.AddMember("ambienceVolume", ambienceVolume, alloc);
        doc.AddMember("gammaSliderValue", gammaSliderValue, alloc);

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::ofstream file(path);
        if (!file.is_open())
        {
            EE_CORE_ERROR("Failed to save user_settings.json");
            return;
        }

        file << buffer.GetString();
        file.close();

        EE_CORE_INFO("User settings saved");
    }

    void SettingsManager::ApplyToSystems()
    {
        auto& ecs = ECS::GetInstance();

        // Apply gamma to renderer
        float gamma = 2.8f - (gammaSliderValue * 1.2f);
        if (auto renderer = ecs.GetSystem<graphics::Renderer>())
        {
            renderer->m_Gamma = gamma;
            EE_CORE_INFO("Applied gamma setting: slider={}, gamma={}", gammaSliderValue, gamma);
        }

        // Apply audio volumes to GlobalAudioComponent
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<GlobalAudioComponent>(e)) continue;

            auto& globalAudio = ecs.GetComponent<GlobalAudioComponent>(e);
            globalAudio.masterVolume = masterVolume;
            globalAudio.SetMusicVolume(musicVolume);
            globalAudio.SetSFXVolume(sfxVolume);
            globalAudio.SetAmbienceVolume(ambienceVolume);

            EE_CORE_INFO("Applied audio settings: master={}, music={}, sfx={}, ambience={}",
                masterVolume, musicVolume, sfxVolume, ambienceVolume);
            break; // Only one GlobalAudioComponent should exist
        }

        // Sync slider UI positions to match saved settings
        for (EntityID e = 0; e < MAX_ENTITIES; ++e)
        {
            if (!ecs.IsEntityValid(e)) continue;
            if (!ecs.HasComponent<UISliderComponent>(e)) continue;

            auto& slider = ecs.GetComponent<UISliderComponent>(e);

            switch (slider.target)
            {
            case UISliderComponent::SliderTarget::MasterVolume:
                slider.value = masterVolume;
                break;
            case UISliderComponent::SliderTarget::MusicVolume:
                slider.value = musicVolume;
                break;
            case UISliderComponent::SliderTarget::SFXVolume:
                slider.value = sfxVolume;
                break;
            case UISliderComponent::SliderTarget::AmbienceVolume:
                slider.value = ambienceVolume;
                break;
            case UISliderComponent::SliderTarget::Custom:
                if (slider.customTarget == "Gamma")
                    slider.value = gammaSliderValue;
                break;
            default:
                break;
            }
        }
    }
}
