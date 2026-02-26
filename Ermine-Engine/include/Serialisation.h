/* Start Header ************************************************************************/
/*!
\file       Serialisation.h
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Jan 31, 2026
\brief      Serialisation functions for Config and Scene

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#ifndef SERIALISATION_H
#define SERIALISATION_H

#include "PreCompile.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include "ECS.h"


struct Config {
    int windowWidth{};
    int windowHeight{};
    bool fullscreen{};
    bool maximized{};
    std::string title;
    std::unordered_map<std::string, bool> imguiWindows; // Key: Window Name, Value: Is Open
	float fontSize = 16.0f;
	float baseFontSize = 1.0f;
	int themeMode = -1; // 0: Light, 1: Dark, 2: Pink, 3: Cyberpunk, 4: Overwatch(Dark), 5: Overwatch(Light)
};

namespace Ermine { struct Guid; }

/**
 * @brief Serialise config file
 * @param config struct
 */
std::string SerializeConfig(const Config& config);

/**
 * @brief Deserialise config files
 * @param string file name
 */
Config DeserializeConfig(const std::string& jsonStr);

/**
 * @brief Save config to json file
 * @param config struct
 * @param file path
 * @param prettywriter bool
 */
void SaveConfigToFile(const Config& config, const std::filesystem::path& path, bool pretty = true);

/**
 * @brief Load config from json file
 * @param file path
 */
Config LoadConfigFromFile(const std::filesystem::path& path);

/**
 * @brief Save scene to json file
 * @param ecs reference
 * @param file path
 * @param prettywriter bool
 */
void SaveSceneToFile(const Ermine::ECS& ecs, const std::filesystem::path& path, bool pretty);

/**
 * @brief Load scene from json file
 * @param ecs reference
 * @param file path
 */
void LoadSceneFromFile(Ermine::ECS& ecs, const std::filesystem::path& path);

/**
 * @brief Save current scene calls SaveSceneToFile
 * @param string scene name
 */
void SaveCurrentScene(const std::string& sceneName);

/**
 * @brief Load current scene calls LoadSceneFromFile
 * @param string scene name
 */
void LoadScene(const std::string& sceneName);

/**
 * @brief Save prefab to json file
 * @param ecs reference
 * @param EntityID
 * @param file path
 */
void SavePrefabToFile(const Ermine::ECS& ecs, Ermine::EntityID id, const std::filesystem::path& path);

/**
 * @brief Load prefab from json file
 * @param ecs reference
 * @param file path
 */
Ermine::EntityID LoadPrefabFromFile(Ermine::ECS& ecs, const std::filesystem::path& path);

bool LoadAssetMetaGuid(const std::filesystem::path& metaPath, Ermine::Guid& outGuid);
bool SaveAssetMetaGuid(const std::filesystem::path& metaPath,
    const Ermine::Guid& guid,
    std::string_view type = {},
    int metaVersion = 1,
    bool pretty = true);

Ermine::Guid EnsureMetaForSource(const std::filesystem::path& sourcePath,
    std::string_view type = {},
    bool pretty = true);

// Forward declarations for material serialization
namespace Ermine::graphics { class Material; }

/**
 * @brief Save material to .mat file
 * @param material Material to save
 * @param path File path for the .mat file
 * @param pretty Use pretty formatting (default: true)
 */
void SaveMaterialToFile(const Ermine::graphics::Material& material,
    const std::filesystem::path& path,
    bool pretty = true,
    std::string_view customFragmentShader = {});

/**
 * @brief Load material from .mat file
 * @param path File path of the .mat file
 * @return Loaded material
 */
Ermine::graphics::Material LoadMaterialFromFile(const std::filesystem::path& path,
    std::string* outCustomFragmentShader = nullptr);

#endif // SERIALISATION_H
