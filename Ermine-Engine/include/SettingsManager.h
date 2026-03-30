/* Start Header ************************************************************************/
/*!
\file       SettingsManager.h
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee@digipen.edu
\date       Mar 18, 2026
\brief      Persistent user settings manager that survives scene changes and game restarts.
            Stores audio volumes and gamma settings, serialized to a JSON file.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <string>
#include <filesystem>

namespace Ermine
{
    class SettingsManager
    {
    public:
        static SettingsManager& GetInstance();

        // Load settings from disk. Call once at engine startup.
        void Load(const std::filesystem::path& path = "user_settings.json");

        // Save current settings to disk.
        void Save(const std::filesystem::path& path = "user_settings.json");

        // Audio volumes (slider values 0.0 - 1.0)
        // Defaults match scene audio mix (m4-test_copy_copy.scene, Level2Wcol.scene)
        float masterVolume   = 1.0f;
        float musicVolume    = 0.24f;  // Low music volume to not overpower gameplay
        float sfxVolume      = 1.0f;
        float ambienceVolume = 0.1f;   // Subtle background ambience

        // Gamma (slider value 0.0 - 1.0, mapped to actual gamma in the UI system)
        float gammaSliderValue = 0.5f; // 0.5 maps to default gamma 2.2

        // Apply stored settings to the Renderer and GlobalAudioComponent after a scene load.
        void ApplyToSystems();

    private:
        SettingsManager() = default;
        SettingsManager(const SettingsManager&) = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;
    };
}

#endif // SETTINGS_MANAGER_H
