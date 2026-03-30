/* Start Header ************************************************************************/
/*!
\file       SceneManager.h
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Sep 10, 2025
\brief      Scene management including new, open, save, and file dialogs

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "PreCompile.h"
#include <string>
#include <optional>
#include "Scene.h"

class SceneManager
{
public:
    /*!
    \brief Singleton for SceneManager
    */
    static SceneManager& GetInstance();

    // File operations
    /*!
    \brief Clears the scene and creates a new one with Main light
    */
    void NewScene();

    /*!
    \brief Clears the scene
    */
    void ClearScene();

    /*!
    \brief Open new scene dialog pop up
    */
    void OpenSceneDialog();

    /*!
    \brief Save scene
    */
    void SaveScene();

    /*!
    \brief Save temp scene on play
    */
    void SaveTemp();

    /*!
    \brief Load temp scene on stop
    */
    void LoadTemp();

    /*!
    \brief Removes temp scene
    */
    void RemoveTemp();

    /*!
    \brief Save scene dialog pop up
    */
    void SaveSceneAsDialog();

    // Direct path-based API
    /*!
    \brief Open Scene from path
    */
    void OpenScene(const std::string& path);

    /*!
    \brief Save Scene to path
    */
    void SaveSceneTo(const std::string& path);

    // Accessors
    /*!
    \brief Get current scene's file path
    */
    std::optional<std::string> GetCurrentScenePath() const { return m_CurrentScenePath; }

    /*!
    \brief Get is dirty (IF scene is modified)
    */
    bool IsDirty() const { return m_Dirty; }

    /*!
    \brief Mark is dirty (when scene is modified)
    */
    void MarkDirty(bool dirty = true) { m_Dirty = dirty; }

    /*!
    \brief Show save dialog pop up
    */
    static std::optional<std::string> ShowSaveDialog(
        const wchar_t* defaultFileName = L"untitled.scene",
        HWND owner = nullptr);

    /*!
    \brief Show open dialog pop up
    */
    static std::optional<std::string> ShowOpenDialog(HWND owner = nullptr);

    /*!
    \brief Set active scene
    */
    void SetActiveScene(const std::shared_ptr<Ermine::Scene>& s) { m_ActiveScene = s; }

    /*!
    \brief Get active scene
    */
    std::shared_ptr<Ermine::Scene> GetActiveScene() const { return m_ActiveScene; }

    /*!
    \brief Ensures active scene
    */
    Ermine::Scene& EnsureActiveScene() {
        if (!m_ActiveScene) m_ActiveScene = std::make_shared<Ermine::Scene>("Untitled Scene");
        return *m_ActiveScene;
    }

    /*!
    \brief Creates the persistent HUD entity with UIComponent
    */
    void CreateHUDEntity();

    // temporary reference to health bar, to be removed
    static Ermine::EntityID healthBar;
    static Ermine::EntityID GetHealthBar();
    /*!
    \brief Checks if a HUD entity with UIComponent exists in the current scene
    \return True if HUD entity exists, false otherwise
    */
    bool HasHUDEntity() const;

    /*!
    \brief Ensures a HUD entity exists, creating one if needed
    \details This is the recommended way to guarantee HUD presence.
             Safe to call multiple times - only creates if missing.
    */
    void EnsureHUDExists();

    /*!
    \brief Removes any existing HUD entity from the scene
    \details Useful before saving if you want clean scene files without HUD data
    */
    void RemoveHUDEntity();

    // Defer scene loading to a safe point (start of next frame)
    void RequestOpenScene(const std::string& path);
    bool HasPendingSceneRequest() const { return m_PendingSceneRequest.has_value(); }
    void FlushPendingSceneRequest();

    /*!
    \brief Apply cursor state based on scene type
    \details E.g., game scenes may hide the cursor, while editor scenes show it
    \param scenePath The path of the scene to determine cursor state
    */
    void ApplySceneCursorState(const std::string& scenePath);

private:
    SceneManager() = default;

    std::optional<std::string> m_CurrentScenePath; // full file path
    bool m_Dirty = false; // set true when scene modified

    std::shared_ptr<Ermine::Scene> m_ActiveScene;

    std::optional<std::string> m_PendingSceneRequest;
    std::optional<std::string> m_TempSceneRestorePath;
};
