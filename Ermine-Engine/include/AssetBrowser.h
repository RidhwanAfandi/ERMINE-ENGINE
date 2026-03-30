/* Start Header ************************************************************************/
/*!
\file       AssetBrowser.h
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (30%)
\co-author  Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (70%)
\date       26/01/2026
\brief      This file contains the declaration of the ImGui-based Asset Browser system.
            It provides UI functionality for browsing, previewing, and managing
            project assets such as textures, audio, and shaders. It includes a
            folder tree view, search filtering, context menus, and file
            management features.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#define NOMINMAX
#include "PreCompile.h"
#include "imgui.h"
#include "ImGuiUIWindow.h"

#ifndef IMGUI_CDECL
#ifdef _MSC_VER
#define IMGUI_CDECL __cdecl
#else
#define IMGUI_CDECL
#endif
#endif

namespace Ermine {
	class ResourcePipeline;
	struct TextureImportSettings;
	struct MeshImportSettings;
}

namespace Ermine::ImguiUI
{
	/**
	 * @struct Asset
	 * @brief Represents an asset in the asset browser.
	 * An asset can be of various types such as audio, texture, or font.
	 * It contains metadata including a unique ID, type, name, selection state, icon, and real file name.
	 */
	struct Asset
	{
		ImGuiID ID;			  // Unique identifier for the asset
		int Type;			  // 0 = Audio, 1 = Texture, 2 = Font
		std::string Name;	  // Display name of the asset
		bool IsSelected;	  // Selection state
		ImTextureID Icon;	  // Icon representing the asset
		std::string realName; // Actual file name of the asset


		bool needsReimport = false;      // Source file modified
		bool isProcessedAsset = false;   // Is .dds, .mesh, .skin
		std::string sourceFile;          // Original source if processed

		/**
		 * @brief Construct an asset with all fields.
		 * @param id Unique identifier for the asset.
		 * @param type Type of the asset (0 = Audio, 1 = Texture, 2 = Font).
		 * @param name Display name of the asset.
		 * @param select Selection state of the asset.
		 * @param icon Icon representing the asset.
		 * @param _realName Actual file name of the asset.
		 * @return An instance of the Asset struct.
		 */
		Asset(ImGuiID id, int type, std::string name, bool select, ImTextureID icon, std::string realName)
			: ID(id), Type(type), Name(std::move(name)), IsSelected(select), Icon(icon), realName(std::move(realName)) {}
	};

	/**
	 * @struct Browser
	 * @brief Manages the state and functionality of the asset browser.
	 * It handles directory navigation, item management, UI layout, and rendering.
	 * The browser supports searching, displaying icons, and interacting with assets.
	 */
	struct Browser
	{
		// --- Paths and State ---
		std::filesystem::path projectRoot;		// Root path of the project
		std::filesystem::path currentDirectory; // Currently viewed directory
		std::vector<Asset> Items;				// List of assets in the current directory
		bool pendingRefresh = false;			// Flag to check to refresh assets

		// --- UI State ---
		std::string searchQuery;				// Current search query
		float iconSize;							// Size of the icons
		float iconSpacing;						// Spacing between icons

		// --- Icons ---
		ImTextureID folderIcon = 0;				// Icon for folders
		ImTextureID fileIcon = 0;				// Icon for generic files
		ImTextureID refreshIcon = 0;			// Icon for refresh action
		bool iconsInitialized = false;			// Flag to check if icons are initialized

		// --- Rename modal state ---
		bool renamePending = false;				// Flag to indicate if a rename operation is pending
		std::string renameFrom;					// Original name before renaming
		std::string renameTo;					// New name after renaming
		char renameBuffer[256] = { 0 };			// Buffer for rename input

		Ermine::ResourcePipeline* m_Pipeline = nullptr; // Pointer to the resource pipeline

		std::atomic<bool> preloadInProgress{ false };
		std::thread preloadThread;

		/**
		 * @brief Default constructor that initializes the asset browser state.
		 */
		Browser();

		/**
		 * @brief Initializes the asset browser with the given resource pipeline.
		 * @param pipeline Pointer to the resource pipeline for asset management.
		 */
		void InitWithPipeline(Ermine::ResourcePipeline* pipeline);

		/**
		 * @brief Loads all required icons for folders, files, and refresh buttons.
		 */
		void InitIcons();

		/**
		 * @brief Retrieves or generates an icon preview for the given file.
		 * @param path Filesystem path to the target file.
		 * @return ImTextureID handle for the appropriate preview icon.
		 */
		ImTextureID GetPreviewIconForFile(const std::filesystem::path& path);

		/**
		 * @brief Refreshes the current directory contents.
		 * This function reloads assets and updates their display icons
		 * and metadata, typically called after changes to the filesystem.
		 */
		void Refresh();

		/**
		 * @brief Loads all files and subfolders from the specified directory.
		 * @param dir Path to the directory to be loaded.
		 */
		void LoadDirectoryContents(const std::filesystem::path& dir);

		/**
		 * @brief Handles files dropped into the asset browser window.
		 * @param filePaths Vector of paths representing dropped files.
		 */
		void HandleDroppedFiles(const std::vector<std::string>& filePaths);

		/**
		 * @brief Copies an external file into the asset folder.
		 * @param sourceFilePath Path to the file being imported.
		 * @return True if the file was successfully copied, false otherwise.
		 */
		bool CopyFileToAssets(const std::string& sourceFilePath);

		/**
		 * @brief Displays and processes right-click context menu for a file.
		 * @param filePath Path to the target file.
		 */
		void HandleFileContextMenu(const std::filesystem::path& filePath);

		/**
		 * @brief Draws the hierarchical folder tree on the left panel.
		 * @param rootPath Root path for the folder tree traversal.
		 */
		void DrawFolderTree(const std::filesystem::path& rootPath);

		/**
		 * @brief Draws the grid of files and folders in the right panel.
		 * Handles file selection, double-click navigation, and contextual
		 * interactions like new folder creation and refresh.
		 */
		void DrawFileGrid();

		/**
		 * @brief Renders the main asset browser window, including both
		 * the folder tree and the file grid.
		 * @param title Title of the ImGui window.
		 */
		void Draw(const char* title);

		// Reference to functions for creating Game Objects with components using Asset Browser //
		/*bool CreateObjectWithAsset(ImGuiID id) {
			GameObject* newObj;
			std::string name = "GameObject_" + std::to_string(GAMEOBJECTFACTORY.GetGameObjects().size());
			newObj = GAMEOBJECTFACTORY.CreateGameObject(name);

			if (!newObj)
				return false;

			if (Items[id].Type == 0) {
				newObj->AddComponent(Component::ComponentType::Audio);
				BS::Component::AudioComponent* audio = newObj->GetComponent<BS::Component::AudioComponent>();

				if (!audio)
					return false;

				audio->SetAudio(GetItemNames()[id]);
				return true;
			}
			if (Items[id].Type == 1) {
				newObj->AddComponent(Component::ComponentType::Renderer);
				BS::Component::RendererComponent* renderer = newObj->GetComponent<BS::Component::RendererComponent>();

				if (!renderer)
					return false;

				renderer->SetTextureName(GetItemNames()[id]);
				return true;
			}
			if (Items[id].Type == 2) {
				newObj->AddComponent(Component::ComponentType::Text);
				BS::Component::TextComponent* text = newObj->GetComponent<BS::Component::TextComponent>();

				if (!text)
					return false;

				text->SetFontType(GetItemNames()[id]);
				return true;
			}

			return false;
		}*/
		/*bool CreateComponentWithAsset(ImGuiID id, IObject* obj) {
			if (!obj)
				return false;

			if (Items[id].Type == 0) {
				obj->AddComponent(Component::ComponentType::Audio);
				BS::Component::AudioComponent* audio = obj->GetComponent<BS::Component::AudioComponent>();

				if (!audio)
					return false;

				audio->SetAudio(GetItemNames()[id]);
				return true;
			}
			if (Items[id].Type == 1) {
				obj->AddComponent(Component::ComponentType::Renderer);
				BS::Component::RendererComponent* renderer = obj->GetComponent<BS::Component::RendererComponent>();

				if (!renderer)
					return false;

				renderer->SetTextureName(GetItemNames()[id]);
				return true;
			}
			if (Items[id].Type == 2) {
				obj->AddComponent(Component::ComponentType::Text);
				BS::Component::TextComponent* text = obj->GetComponent<BS::Component::TextComponent>();

				if (!text)
					return false;

				text->SetFontType(GetItemNames()[id]);
				return true;
			}

			return false;
		}*/

	private:
		/**
		 * @brief Preloads all texture assets from the specified directory.
		 * @param dir Path to the directory containing texture assets.
		 */
		void PreloadAllTextureAssets(const std::filesystem::path& dir);

		/**
		 * @brief Preloads all texture assets from the specified directory asynchronously.
		 * @param dir Path to the directory containing texture assets.
		 */
		void PreloadAllTextureAssetsAsync(const std::filesystem::path& dir);

		/**
		 * @brief Checks and updates the import status of the given asset.
		 * @param asset Reference to the asset to check.
		 */
		void CheckImportStatus(Asset& asset);

		/**
		 * @brief Displays and processes right-click context menu for importing files.
		 * @param filePath Path to the target file.
		 */
		void HandleImportContextMenu(const std::filesystem::path& filePath);
	};

	/**
	 * @class AssetBrowser
	 * @brief ImGUI window for browsing and managing assets.
	 * The AssetBrowser class provides a user interface for navigating directories,
	 * viewing assets, and performing actions such as searching, selecting, and importing assets.
	 */
	class AssetBrowser : public ImGUIWindow
	{
	public:
		/**
		 * @brief Construct an AssetBrowser window with a default title.
		 * The window allows users to browse and manage assets.
		 */
		AssetBrowser() : ImGUIWindow("Asset Browser") {}

		/**
		 * @brief Initialize the AssetBrowser with the given resource pipeline.
		 * @param pipeline Pointer to the resource pipeline for asset management.
		 */
		void InitWithPipeline(Ermine::ResourcePipeline* pipeline) { assets_browser.InitWithPipeline(pipeline); }

		/**
		 * @brief Render the AssetBrowser window.
		 * This function is responsible for drawing the asset browser UI,
		 * including the directory tree, asset grid, and context menus.
		 */
		void Render() override;

		/**
		 * @brief Static callback for handling external files dropped into the asset browser.
		 * @param filePaths Vector of paths representing dropped files.
		 */
		static void OnExternalFilesDropped(const std::vector<std::string>& filePaths);

		/**
		 * @brief Get a reference to the underlying Browser instance.
		 * @return Reference to the Browser instance.
		 */
		Browser& GetBrowser() { return assets_browser; }

	private:
		Browser assets_browser; // Access the underlying Browser instance.
	};
}
