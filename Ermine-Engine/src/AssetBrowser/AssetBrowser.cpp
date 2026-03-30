/* Start Header ************************************************************************/
/*!
\file       AssetBrowser.cpp
\author     LEE Wen Jie, Brian, wenjiebrian.lee, 2301261, wenjiebrian.lee\@digipen.edu (30%)
\co-author  Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (70%)
\date       26/01/2026
\brief      This file contains the definition of the ImGui-based Asset Browser system.
            It provides UI functionality for browsing, previewing, and managing
            project assets such as textures, audio, and shaders. It includes a
            folder tree view, search filtering, context menus, and file
            management features.

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AssetBrowser.h"
#include "AssetManager.h" // For loading textures
#include "SceneManager.h" // For opening scenes
#include "PrefabManager.h" // For opening prefabs
#include "EditorGUI.h" // For forwarding dropped files to the asset browser
#include "Components.h"
#include "ResourcePipe.h"

namespace fs = std::filesystem;

namespace Ermine::ImguiUI
{
    /**
     * @brief Retrieves the lowercase file extension from a given file path.
     * This utility function extracts the extension portion of a file path
     * (e.g., ".PNG", ".txt"), converts it entirely to lowercase characters,
     * and removes the leading period for consistency.
     * @param path Full or relative file path.
     * @return The lowercase extension string without the leading dot.
     */
    static std::string GetExtensionLower(const std::string& path) {
        auto ext = fs::path(path).extension().string();
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
        return ext;
    }

    void Browser::InitWithPipeline(Ermine::ResourcePipeline* pipeline)
    {
        m_Pipeline = pipeline;
        if (m_Pipeline) {
            EE_CORE_INFO("[AssetBrowser] Connected to ResourcePipeline");
        }
    }

    void Browser::PreloadAllTextureAssets(const fs::path& dir)
    {
        // Check if directory exists
        if (!fs::exists(dir)) return;

        // Iterate through directory entries
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                PreloadAllTextureAssets(entry.path()); // Recurse into subfolders
            }
            else {
                // Load texture files
                std::string ext = GetExtensionLower(entry.path().string());
                if (ext == "png" || ext == "jpg" || ext == "jpeg") {
                    auto tex = AssetManager::GetInstance().LoadTexture(entry.path().string().c_str());
                    if (!tex || !tex->IsValid()) {
                        EE_CORE_WARN("Failed to preload texture: {}", entry.path().string());
                    }
                }
            }
        }
    }

    void Browser::PreloadAllTextureAssetsAsync(const std::filesystem::path& dir)
    {
        if (preloadInProgress) return; // Already running
        preloadInProgress = true;

        preloadThread = std::thread([this, dir]()
            {
                EE_CORE_INFO("Starting async preload of all textures...");

                try {
                    for (auto& entry : fs::recursive_directory_iterator(dir))
                    {
                        if (!entry.is_regular_file()) continue;

                        std::string ext = GetExtensionLower(entry.path().string());
                        if (ext == "png" || ext == "jpg" || ext == "jpeg")
                        {
                            auto tex = AssetManager::GetInstance().LoadTexture(entry.path().string().c_str());
                            if (!tex || !tex->IsValid())
                                EE_CORE_WARN("Failed to preload texture: {}", entry.path().string());
                        }
                    }
                }
                catch (const std::exception& e) {
                    EE_CORE_ERROR("Async preload failed: {}", e.what());
                }

                EE_CORE_INFO("Async texture preload completed.");
                preloadInProgress = false;
            });

        // Detach thread so it continues running independently
        preloadThread.detach();
    }

    void Browser::CheckImportStatus(Asset& asset)
    {
        if (!m_Pipeline) return;
        if (asset.Type == 1) return; // Skip folders

        namespace fs = std::filesystem;
        fs::path fullPath = asset.realName.empty() ?
            (currentDirectory / asset.Name) : fs::path(asset.realName);

        auto GetExtLower = [](const std::string& path) {
            auto ext = fs::path(path).extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
            return ext;
            };

        std::string ext = GetExtLower(fullPath.string());

        // Check if source asset
        bool isSourceAsset = (ext == "png" || ext == "jpg" || ext == "jpeg" ||
            ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb");

        if (isSourceAsset) {
            asset.needsReimport = m_Pipeline->NeedsReimport(fullPath.string());
        }

        // Check if processed asset
        asset.isProcessedAsset = (ext == "dds" || ext == "mesh" || ext == "skin");
    }

    void Browser::HandleImportContextMenu(const std::filesystem::path& filePath)
    {
        if (!m_Pipeline) return;

        namespace fs = std::filesystem;
        auto GetExtLower = [](const std::string& path) {
            auto ext = fs::path(path).extension().string();
            for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
            if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
            return ext;
            };

        std::string ext = GetExtLower(filePath.string());

        // Texture import
        if (ext == "png" || ext == "jpg" || ext == "jpeg")
        {
            if (ImGui::BeginMenu("Import as Texture"))
            {
                static Ermine::TextureImportSettings settings;
                static int selectedFormatIndex = 1; // Default to BGRA8

                ImGui::TextDisabled("Format:");
                ImGui::Separator();

                // Format dropdown
                auto formats = Ermine::ResourcePipeline::GetSupportedFormats();
                if (ImGui::BeginCombo("##Format", Ermine::ResourcePipeline::GetFormatName(settings.targetFormat))) {
                    for (int i = 0; i < formats.size(); i++) {
                        bool isSelected = (settings.targetFormat == formats[i]);
                        if (ImGui::Selectable(Ermine::ResourcePipeline::GetFormatName(formats[i]), isSelected)) {
                            settings.targetFormat = formats[i];
                            selectedFormatIndex = i;
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }

                        // Add helpful tooltips
                        if (ImGui::IsItemHovered()) {
                            switch (formats[i]) {
                            case DXGI_FORMAT_BC1_UNORM_SRGB:
                                ImGui::SetTooltip("BC1 sRGB (6:1 compression, no alpha)\nFor color/albedo textures without transparency");
                                break;
                            case DXGI_FORMAT_BC3_UNORM_SRGB:
                                ImGui::SetTooltip("BC3 sRGB (4:1 compression with alpha)\nFor color/albedo textures with transparency");
                                break;
                            case DXGI_FORMAT_BC7_UNORM_SRGB:
                                ImGui::SetTooltip("BC7 sRGB (High quality compression)\nBest quality for color textures, slower compression");
                                break;
                            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                                ImGui::SetTooltip("RGBA8 sRGB (Uncompressed)\nFor UI or textures requiring exact colors");
                                break;
                            case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
                                ImGui::SetTooltip("BGRA8 sRGB (Uncompressed)\nDefault uncompressed format for color textures");
                                break;

                                // Linear formats (for data textures)
                            case DXGI_FORMAT_BC5_UNORM:
                                ImGui::SetTooltip("BC5 Linear (2-channel compression)\n ONLY for normal maps! Stores XY direction vectors");
                                break;
                            case DXGI_FORMAT_BC4_UNORM:
                                ImGui::SetTooltip("BC4 Linear (Grayscale compression)\nFor roughness, metallic, AO, or height maps");
                                break;
                            case DXGI_FORMAT_BC1_UNORM:
                                ImGui::SetTooltip("BC1 Linear (6:1 compression, no alpha)\n For data textures only, NOT for colors!");
                                break;
                            case DXGI_FORMAT_BC3_UNORM:
                                ImGui::SetTooltip("BC3 Linear (4:1 compression with alpha)\n For data textures only, NOT for colors!");
                                break;
                            case DXGI_FORMAT_R8G8B8A8_UNORM:
                                ImGui::SetTooltip("RGBA8 Linear (Uncompressed)\nFor data that needs exact values (not display colors)");
                                break;
                            case DXGI_FORMAT_B8G8R8A8_UNORM:
                                ImGui::SetTooltip("BGRA8 Linear (Uncompressed)\n For data textures only, NOT for colors!");
                                break;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();
                ImGui::TextDisabled("Options:");
                ImGui::Checkbox("Generate Mipmaps", &settings.generateMipmaps);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Creates smaller versions for distant rendering");
                }

                ImGui::Separator();
                if (ImGui::Button("Import Now", ImVec2(120, 0)))
                {
                    EE_CORE_INFO("Importing texture: {} as {}",
                        filePath.string(),
                        Ermine::ResourcePipeline::GetFormatName(settings.targetFormat));

                    auto result = m_Pipeline->ImportTexture(filePath.string(), settings);

                    if (result.success) {
                        EE_CORE_INFO("✓ Import successful: {} ({}ms)",
                            result.outputPath, result.importTimeMs);
                        m_Pipeline->GetDatabase().Save();
                        Refresh();
                    }
                    else {
                        EE_CORE_ERROR("✗ Import failed: {}", result.errorMessage);
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndMenu();
            }
        }

        // Mesh import
        if (ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb")
        {
            if (ImGui::BeginMenu("Import as Mesh"))
            {
                static Ermine::MeshImportSettings settings;

                // Processing Options Section
                ImGui::TextDisabled("Processing Options:");
                ImGui::Separator();

                ImGui::Checkbox("Generate Normals", &settings.generateNormals);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Compute normals if missing or replace existing ones");
                }

                ImGui::Checkbox("Generate Tangents", &settings.generateTangents);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Required for normal mapping");
                }

                ImGui::Checkbox("Flip UVs", &settings.flipUVs);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Flip texture coordinates vertically (useful for DirectX assets)");
                }

                ImGui::Checkbox("Optimize Vertices", &settings.optimizeVertices);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Improve GPU cache performance");
                }

                ImGui::Spacing();

                // Pre-Transform Section
                ImGui::TextDisabled("Pre-Transform:");
                ImGui::Separator();

                ImGui::Checkbox("Apply Pre-Transform", &settings.applyPreTransform);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Bake transformations into the mesh data");
                }

                if (settings.applyPreTransform)
                {
                    ImGui::Indent();

                    // Scale
                    ImGui::TextDisabled("Scale:");
                    ImGui::SetNextItemWidth(200);
                    ImGui::DragFloat3("##Scale", settings.scale, 0.01f, 0.001f, 100.0f, "%.3f");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Uniform or non-uniform scaling");
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset##ScaleReset")) {
                        settings.scale[0] = settings.scale[1] = settings.scale[2] = 1.0f;
                    }

                    // Rotation
                    ImGui::TextDisabled("Rotation (Degrees):");
                    ImGui::SetNextItemWidth(200);
                    ImGui::DragFloat3("##Rotation", settings.rotation, 1.0f, -360.0f, 360.0f, "%.1f°");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Euler angles: X (Pitch), Y (Yaw), Z (Roll)");
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset##RotReset")) {
                        settings.rotation[0] = settings.rotation[1] = settings.rotation[2] = 0.0f;
                    }

                    // Translation
                    ImGui::TextDisabled("Translation:");
                    ImGui::SetNextItemWidth(200);
                    ImGui::DragFloat3("##Translation", settings.translation, 0.1f, -1000.0f, 1000.0f, "%.2f");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Offset the mesh position");
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reset##TransReset")) {
                        settings.translation[0] = settings.translation[1] = settings.translation[2] = 0.0f;
                    }

                    ImGui::Unindent();
                }

                ImGui::Spacing();
                ImGui::Separator();

                // Import button
                if (ImGui::Button("Import Now", ImVec2(120, 0)))
                {
                    EE_CORE_INFO("Importing mesh: {}", filePath.string());

                    if (settings.applyPreTransform) {
                        EE_CORE_INFO("  Pre-transform enabled:");
                        EE_CORE_INFO("    Scale: ({:.3f}, {:.3f}, {:.3f})",
                            settings.scale[0], settings.scale[1], settings.scale[2]);
                        EE_CORE_INFO("    Rotation: ({:.1f}°, {:.1f}°, {:.1f}°)",
                            settings.rotation[0], settings.rotation[1], settings.rotation[2]);
                        EE_CORE_INFO("    Translation: ({:.2f}, {:.2f}, {:.2f})",
                            settings.translation[0], settings.translation[1], settings.translation[2]);
                    }

                    auto result = m_Pipeline->ImportMesh(filePath.string(), settings);

                    if (result.success) {
                        EE_CORE_INFO("✓ Import successful: {} ({}ms)",
                            result.outputPath, result.importTimeMs);
                        m_Pipeline->GetDatabase().Save();
                        Refresh();
                    }
                    else {
                        EE_CORE_ERROR("✗ Import failed: {}", result.errorMessage);
                    }

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndMenu();
            }
        }

        // Quick reimport
        bool canReimport = (ext == "png" || ext == "jpg" || ext == "jpeg" ||
            ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb");

        if (canReimport && m_Pipeline->NeedsReimport(filePath.string()))
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Reimport (Modified)"))
            {
                EE_CORE_INFO("Reimporting: {}", filePath.string());
                auto result = m_Pipeline->ReimportAsset(filePath.string());

                if (result.success) {
                    EE_CORE_INFO("Reimport successful ({}ms)", result.importTimeMs);
                    m_Pipeline->GetDatabase().Save();
                    Refresh();
                }
                else {
                    EE_CORE_ERROR("Reimport failed: {}", result.errorMessage);
                }
            }
        }
    }



    /**
     * @brief Default constructor that initializes the asset browser state.
     */
    Browser::Browser()
    {
        // Default to Resources folder
        projectRoot = fs::current_path() / "../Resources";
        currentDirectory = projectRoot;

        // Default view settings
        iconSize = 60.0f;
        iconSpacing = 16.0f;
    }

    /**
     * @brief Loads all required icons for folders, files, and refresh buttons.
     */
    void Browser::InitIcons()
    {
        // Load only once
        if (iconsInitialized) return;

        // Load built-in icons
        auto folderTex = AssetManager::GetInstance().LoadTexture("../Resources/Textures/Icons/folder.png");
        auto fileTex = AssetManager::GetInstance().LoadTexture("../Resources/Textures/Icons/file.png");
        auto refreshTex = AssetManager::GetInstance().LoadTexture("../Resources/Textures/Icons/refresh.png");

        // Get ImGui texture IDs
        if (folderTex && folderTex->IsValid()) folderIcon = (ImTextureID)(intptr_t)folderTex->GetRendererID();
        if (fileTex && fileTex->IsValid()) fileIcon = (ImTextureID)(intptr_t)fileTex->GetRendererID();
        if (refreshTex && refreshTex->IsValid()) refreshIcon = (ImTextureID)(intptr_t)refreshTex->GetRendererID();

        // Preload all texture assets
        PreloadAllTextureAssets(projectRoot);
        //PreloadAllTextureAssetsAsync(projectRoot);

        // Initial load of directory contents
        LoadDirectoryContents(projectRoot);
        iconsInitialized = true;
    }

    /**
     * @brief Retrieves or generates an icon preview for the given file.
     * @param path Filesystem path to the target file.
     * @return ImTextureID handle for the appropriate preview icon.
     */
    ImTextureID Browser::GetPreviewIconForFile(const fs::path& path)
    {
        // Default icon
        ImTextureID icon = fileIcon;

        // Determine icon based on file type
        if (fs::is_directory(path)) icon = folderIcon;
        else {
            // Load image preview for common formats
            std::string ext = GetExtensionLower(path.string());
            if (ext == "png" || ext == "jpg" || ext == "jpeg") {
                auto tex = AssetManager::GetInstance().LoadTexture(path.string().c_str());
                if (tex && tex->IsValid()) icon = (ImTextureID)(intptr_t)tex->GetRendererID();
            }
        }
        return icon;
    }

    /**
     * @brief Refreshes the current directory contents.
     * This function reloads assets and updates their display icons
     * and metadata, typically called after changes to the filesystem.
     */
    void Browser::Refresh() { LoadDirectoryContents(currentDirectory); }

    /**
     * @brief Loads all files and subfolders from the specified directory.
     * @param dir Path to the directory to be loaded.
     */
    void Browser::LoadDirectoryContents(const std::filesystem::path& dir)
    {
        // Clear existing items
        Items.clear();

        // Load new items from directory
        if (!fs::exists(dir)) return;
        try {
            for (auto& entry : fs::directory_iterator(dir)) {

                // Skip Unity-style meta files
                if (entry.is_regular_file() && entry.path().extension() == ".meta") continue;

                // Create asset entry
                std::string name = entry.path().filename().string();
                ImTextureID icon = GetPreviewIconForFile(entry.path());
                int type = entry.is_directory() ? 1 : 0;
                std::string uniqueKey = entry.path().string();
                ImGuiID id = static_cast<ImGuiID>(std::hash<std::string>{}(uniqueKey));

                // Create asset and check import status
                Asset asset(id, type, name, false, icon, uniqueKey);
                CheckImportStatus(asset);

                // Add only once
                Items.emplace_back(asset);
            }
        }
        catch (std::exception& e) {
            EE_CORE_ERROR("Directory load error: {}", e.what());
        }
    }

    /**
     * @brief Handles files dropped into the asset browser window.
     * @param filePaths Vector of paths representing dropped files.
     */
    void Browser::HandleDroppedFiles(const std::vector<std::string>& filePaths)
    {
        // Accept plain file-system paths and copy them into the currently open folder under Resources
        for (const auto& s : filePaths) {
            if (!CopyFileToAssets(s)) {
                EE_CORE_ERROR("Failed to import: {}", s);
            }
        }
        Refresh();
    }

    /**
     * @brief Copies an external file into the asset folder.
     * @param sourceFilePath Path to the file being imported.
     * @return True if the file was successfully copied, false otherwise.
     */
    bool Browser::CopyFileToAssets(const std::string& sourceFilePath)
    {
        namespace fs = std::filesystem;
        fs::path src(sourceFilePath);
        if (!fs::exists(src)) return false;

        fs::path dest = currentDirectory / src.filename();

        try {
            fs::create_directories(currentDirectory);
            fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
            EE_CORE_INFO("Imported {} -> {}", src.string(), dest.string());

            // Auto-import
            if (m_Pipeline)
            {
                auto GetExtLower = [](const std::string& path) {
                    auto ext = fs::path(path).extension().string();
                    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
                    if (!ext.empty() && ext[0] == '.') ext.erase(0, 1);
                    return ext;
                    };

                std::string ext = GetExtLower(dest.string());
                bool shouldImport = (ext == "png" || ext == "jpg" || ext == "jpeg" ||
                    ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb");

                if (shouldImport)
                {
                    EE_CORE_INFO("Auto-importing dropped file...");
                    auto result = m_Pipeline->ReimportAsset(dest.string());

                    if (result.success) {
                        EE_CORE_INFO("Auto-import successful ({}ms)", result.importTimeMs);
                        m_Pipeline->GetDatabase().Save();
                    }
                    else {
                        EE_CORE_WARN("Auto-import failed: {}", result.errorMessage);
                    }
                }
            }

            return true;
        }
        catch (std::exception& e) {
            EE_CORE_ERROR("CopyFileToAssets failed: {}", e.what());
            return false;
        }
    }

    /**
     * @brief Displays and processes right-click context menu for a file.
     * @param filePath Path to the target file.
     */
    void Browser::HandleFileContextMenu(const std::filesystem::path& filePath)
    {
        HandleImportContextMenu(filePath);

        if (m_Pipeline) {
            ImGui::Separator();
        }

        // --- Context menu options for a file or folder ---
        // "Open" option
        if (ImGui::MenuItem("Open"))
            ShellExecuteA(NULL, "open", filePath.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);

        // "Show in Explorer" option
        if (ImGui::MenuItem("Show in Explorer"))
            ShellExecuteA(NULL, "open", filePath.parent_path().string().c_str(), NULL, NULL, SW_SHOWDEFAULT);

        // "New Folder" option
        if (ImGui::MenuItem("New Folder")) {
            fs::path newFolder = currentDirectory / "New Folder";
            int counter = 1;
            while (fs::exists(newFolder))
                newFolder = currentDirectory / ("New Folder " + std::to_string(counter++));
            fs::create_directory(newFolder);
            pendingRefresh = true;
        }

        // "Duplicate" option
        if (ImGui::MenuItem("Duplicate")) {
            fs::path dest = filePath.parent_path() / (filePath.stem().string() + "_copy" + filePath.extension().string());
            try { fs::copy_file(filePath, dest, fs::copy_options::overwrite_existing); }
            catch (...) { EE_CORE_ERROR("Failed to duplicate {}", filePath.string()); }
            pendingRefresh = true;
        }

        // "Delete" option
        if (ImGui::MenuItem("Delete")) {
            try { fs::remove(filePath); }
            catch (...) { EE_CORE_ERROR("Failed to delete {}", filePath.string()); }
            pendingRefresh = true;
        }

        // "Rename" option
        if (ImGui::MenuItem("Rename")) {
            renamePending = true;
            renameFrom = filePath.string();
            renameTo = filePath.filename().string();
            strncpy_s(renameBuffer, sizeof(renameBuffer), renameTo.c_str(), _TRUNCATE);
        }
    }

    /**
     * @brief Draws the hierarchical folder tree on the left panel.
     * @param rootPath Root path for the folder tree traversal.
     */
    void Browser::DrawFolderTree(const std::filesystem::path& rootPath)
    {
        // Iterate through subdirectories
        for (const auto& entry : fs::directory_iterator(rootPath))
        {
            // Only show directories
            if (!entry.is_directory()) continue;

            // Draw tree node for folder
            const std::string folderName = entry.path().filename().string();
            bool open = ImGui::TreeNodeEx(folderName.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);

            // Handle folder click to navigate
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
                currentDirectory = entry.path();
                LoadDirectoryContents(currentDirectory);
            }

            // Recursively draw subfolders
            if (open) { DrawFolderTree(entry.path()); ImGui::TreePop(); }
        }
    }

    /**
     * @brief Draws the grid of files and folders in the right panel.
     * Handles file selection, double-click navigation, and contextual
     * interactions like new folder creation and refresh.
     */
    void Browser::DrawFileGrid()
    {
        // Setup grid style
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(iconSpacing, iconSpacing));

        // Calculate grid layout parameters
        const float cellSize = iconSize + iconSpacing + ImGui::GetStyle().ItemSpacing.x;
        const float panelWidth = ImGui::GetContentRegionAvail().x;

        // Determine number of columns based on panel width
        int computedColumns = static_cast<int>(floor(panelWidth / cellSize));
        if (computedColumns < 1) computedColumns = 1;
        ImGui::Columns(computedColumns, nullptr, false); // Begin grid layout

        fs::path folderToOpen; // Track folder to open on double-click
        ImDrawList* dl = ImGui::GetWindowDrawList(); // Get draw list for highlights

        // Filter items based on search query
        std::vector<Asset*> visibleItems;
        if (searchQuery.empty())
            for (auto& a : Items) visibleItems.push_back(&a);
        else {
            std::string lowQ = searchQuery;
            std::transform(lowQ.begin(), lowQ.end(), lowQ.begin(), ::tolower);
            for (auto& a : Items) {
                std::string lowName = a.Name;
                std::transform(lowName.begin(), lowName.end(), lowName.begin(), ::tolower);
                if (lowName.find(lowQ) != std::string::npos) visibleItems.push_back(&a);
            }
        }

        // Draw each visible item
        for (auto* asset : visibleItems) {
            // Get cursor position for drawing highlights
            ImVec2 cursor = ImGui::GetCursorScreenPos();

            // Draw image button
            ImGui::PushID(asset->ID);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            bool clicked = ImGui::ImageButton(("##icon" + asset->Name).c_str(), asset->Icon, ImVec2(iconSize, iconSize), ImVec2(0, 0), ImVec2(1, 1));
            ImGui::PopStyleColor(3);

            if (asset->needsReimport) {
                ImVec2 badgePos = { cursor.x + iconSize - 20, cursor.y + 5 };
                dl->AddCircleFilled(badgePos, 8, IM_COL32(255, 165, 0, 255)); // Orange badge
                dl->AddText(ImVec2(badgePos.x - 3, badgePos.y - 7),
                    IM_COL32(255, 255, 255, 255), "!");
            }

            // Show tooltip on hover
            if (ImGui::IsItemHovered() && asset->needsReimport) {
                ImGui::SetTooltip("Source file modified - needs reimport");
            }

            // Draw selection highlight background
            if (asset->IsSelected) {
                dl->AddRectFilled(cursor, { cursor.x + iconSize, cursor.y + iconSize }, IM_COL32(80, 150, 255, 50));
                dl->AddRect(cursor, { cursor.x + iconSize, cursor.y + iconSize }, IM_COL32(80, 150, 255, 180), 0, 0, 4.0f);
            }

            // Hover highlight
            if (ImGui::IsItemHovered() && !asset->IsSelected)
                dl->AddRect(cursor, { cursor.x + iconSize, cursor.y + iconSize }, IM_COL32(80, 150, 255, 180), 0, 0, 4.0f);

            // --- Handle click behaviours ---
            // Single-click to select
            if (clicked) {
                for (auto& a : Items) a.IsSelected = false;
                asset->IsSelected = true;
            }

            // Double-click to open
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                fs::path full = asset->realName.empty() ? (currentDirectory / asset->Name) : fs::path(asset->realName);
                if (fs::is_directory(full))
                    folderToOpen = full;
                else {
                    std::string ext = GetExtensionLower(full.string());
                    if (ext == "scene") {
                        // Open scene file in editor
                        EE_CORE_INFO("Opening scene: {}", full.string());
                        SceneManager::GetInstance().ClearScene();
                        SceneManager::GetInstance().OpenScene(full.string().c_str());
                    }
                    else {
                        // Open file with default application
                        ShellExecuteA(NULL, "open", full.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
                    }
                }
            }

            // Drag-and-drop support
            if (ImGui::BeginDragDropSource()) {
                std::string fullPath = asset->realName.empty() ? (currentDirectory / asset->Name).string() : asset->realName;
                ImGui::SetDragDropPayload("ASSET_BROWSER_FILE", fullPath.c_str(), fullPath.size() + 1);
                ImGui::Text("%s", asset->Name.c_str());
                ImGui::EndDragDropSource();
            }

            // Right-click for context menu
            if (ImGui::BeginPopupContextItem()) {
                fs::path target = asset->realName.empty() ? (currentDirectory / asset->Name) : fs::path(asset->realName);
                HandleFileContextMenu(target);
                ImGui::EndPopup();
            }

            // Draw asset name label below the icon (with rename support)
            ImVec2 textSize = ImGui::CalcTextSize(asset->Name.c_str());
            float textOffset = (iconSize - textSize.x) * 0.5f;
            if (textOffset < 0.0f) textOffset = 0.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
            ImGui::PushID(("##label" + asset->Name).c_str());

            // Rename input box if renaming is active
            bool isRenamingThis = renamePending && (renameFrom == asset->realName);
            if (isRenamingThis) {
                ImGui::SetKeyboardFocusHere();
                ImGui::SetNextItemWidth(iconSize * 1.2f);

                if (ImGui::InputText("##RenameInput", renameBuffer, IM_ARRAYSIZE(renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                    try {
                        fs::path from(renameFrom);
                        fs::path to = from.parent_path() / std::string(renameBuffer);
                        fs::rename(from, to);
                        EE_CORE_INFO("Renamed {} -> {}", from.string(), to.string());
                        Refresh();
                    }
                    catch (std::exception& e) {
                        EE_CORE_ERROR("Rename failed: {}", e.what());
                    }
                    renamePending = false;
                }

                // Escape key cancels rename
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    renamePending = false;
            }
            else {
                // Display text label normally
                ImGui::TextWrapped(asset->Name.c_str());
            }
            ImGui::PopID(); // Pop label ID

            // Advance to next column
            ImGui::NextColumn();
            ImGui::PopID();
        }

        // Open folder if requested
        if (!folderToOpen.empty()) {
            currentDirectory = folderToOpen;
            LoadDirectoryContents(folderToOpen);
        }

        // Refresh assets if requested
        if (pendingRefresh) {
            pendingRefresh = false;
            Refresh();
        }

        // End grid layout
        ImGui::Columns(1);
        ImGui::PopStyleVar();

        // Keyboard delete for selected assets with confirmation
        static bool deletePopupOpen = false;
        static std::vector<std::filesystem::path> deleteTargets;

        // Detect delete key press
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            // Gather selected items for deletion
            deleteTargets.clear();
            for (auto& asset : Items)
                if (asset.IsSelected)
                    deleteTargets.push_back(asset.realName);

            // Open confirmation popup if there are targets
            if (!deleteTargets.empty()) {
                deletePopupOpen = true;
                ImGui::OpenPopup("Confirm Delete");
            }
        }

        // Delete confirmation popup
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete %d item(s)?", (int)deleteTargets.size());
            ImGui::Separator();
            ImGui::TextDisabled("This action cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                for (auto& path : deleteTargets) {
                    try {
                        if (std::filesystem::exists(path)) {
                            std::filesystem::remove_all(path);
                            EE_CORE_INFO("Deleted asset: {}", path.string());
                        }
                    }
                    catch (std::exception& e) {
                        EE_CORE_ERROR("Delete failed: {}", e.what());
                    }
                }
                deleteTargets.clear();
                deletePopupOpen = false;
                ImGui::CloseCurrentPopup();
                Refresh();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                deleteTargets.clear();
                deletePopupOpen = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup(); // End delete confirmation popup
        }
    }

    /**
     * @brief Renders the main asset browser window, including both
     * the folder tree and the file grid.
     * @param title Title of the ImGui window.
     */
    void Browser::Draw(const char* title)
    {
        // Set initial window size and begin ImGui window
        ImGui::SetNextWindowSize(ImVec2(iconSize * 12, iconSize * 7), ImGuiCond_FirstUseEver);

        // --- Top bar: search box, view scale, refresh button ---
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

        // Search box
        static char searchBuffer[256] = { 0 };
        strncpy_s(searchBuffer, sizeof(searchBuffer), searchQuery.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 300.f);
        if (ImGui::InputTextWithHint("##SearchAssets", "Search assets...", searchBuffer, IM_ARRAYSIZE(searchBuffer))) {
            searchQuery = std::string(searchBuffer);
            LoadDirectoryContents(currentDirectory);
        }

        // View scale
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text("View:"); ImGui::SameLine();
        ImGui::SetNextItemWidth(165);
        ImGui::SliderFloat("##ViewScaler", &iconSize, 48.0f, 128.0f, "%.0f");

        // Refresh button
        ImGui::SameLine();
        if (refreshIcon) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::ImageButton("##RefreshBtn", refreshIcon, ImVec2(30, 30), ImVec2(0, 1), ImVec2(1, 0))) Refresh();
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh Assets");
        }
        ImGui::EndGroup();
        ImGui::PopStyleVar(); // --- End top bar ---

        // --- Asset browser columns ---
        ImGui::Columns(2, "AssetBrowserColumns", false);
        ImGui::SetColumnWidth(0, 260);

        ImGui::BeginChild("Folders", ImVec2(0, 0), true); // Begin left folder tree panel
        if (fs::exists(projectRoot)) {
            bool rootOpen = ImGui::TreeNodeEx("Resources", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) { currentDirectory = projectRoot; LoadDirectoryContents(currentDirectory); }
            if (rootOpen) { DrawFolderTree(projectRoot); ImGui::TreePop(); }
        }
        ImGui::EndChild(); // End folder tree panel

        ImGui::NextColumn();
        ImGui::BeginChild("FilesPanel", ImVec2(0, 0), true); // Begin right files panel

        // Breadcrumb navigation
        {
            std::string rel = ".";
            try { rel = fs::relative(currentDirectory, projectRoot).string(); }
            catch (...) { rel = currentDirectory.string(); }
            std::replace(rel.begin(), rel.end(), '\\', '/');
            if (rel == "." || rel == "") rel = "";

            // "Resources" root button
            if (ImGui::SmallButton("Resources")) {
                currentDirectory = projectRoot;
                LoadDirectoryContents(currentDirectory);
            }

            // Breadcrumb buttons
            fs::path breadcrumb = projectRoot;
            std::stringstream ss(rel);
            std::string token;
            while (std::getline(ss, token, '/')) {
                if (token.empty()) continue;
                breadcrumb /= token;

                ImGui::SameLine(0, 5);
                ImGui::TextUnformatted(">");
                ImGui::SameLine(0, 5);
                if (ImGui::SmallButton(token.c_str())) {
                    currentDirectory = breadcrumb;
                    LoadDirectoryContents(currentDirectory);
                }
            }
        }

        // File grid
        ImGui::BeginChild("FileGrid", ImVec2(0, -40), true); // Begin file grid
        DrawFileGrid();
        ImGui::EndChild(); // End file grid

        // Drag & drop import: try to accept external file list or a raw path payload
        if (ImGui::BeginDragDropTarget()) { // Begin drag & drop target
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EXTERNAL_FILES")) {
                const auto* files = static_cast<const std::vector<std::string>*>(payload->Data);
                if (files) HandleDroppedFiles(*files);
            }
            if (const ImGuiPayload* p2 = ImGui::AcceptDragDropPayload("Path")) {
                const char* s = (const char*)p2->Data;
                if (s && *s) HandleDroppedFiles({ std::string(s) });
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                EntityID droppedEntity = *(EntityID*)payload->Data;

                //auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();

                auto& ecs = Ermine::ECS::GetInstance();

                std::string name = "NewPrefab";

                if (ecs.HasComponent<ObjectMetaData>(droppedEntity))
                    name = ecs.GetComponent<ObjectMetaData>(droppedEntity).name;

                PrefabManager::GetInstance().SavePrefab(droppedEntity, currentDirectory / (name + ".prefab"));
                EE_CORE_INFO("Saved entity {} as prefab in {}", droppedEntity, (currentDirectory / (name + ".prefab")).string());

                // Check if entity currently has a parent
                //EntityID currentParent = hierarchySystem->GetParent(droppedEntity);
                //if (currentParent != 0) {
                //    hierarchySystem->UnsetParent(droppedEntity);
                //    EE_CORE_INFO("Unparented entity {} - now a root entity", droppedEntity);
                //}
				Refresh();
            }
            ImGui::EndDragDropTarget(); // End drag & drop target
        }

        // Footer with selected path
        ImGui::BeginChild("Footer", ImVec2(0, 35), false); // Begin footer
        {
            // Show selected path below
            for (auto& asset : Items) {
                if (asset.IsSelected) {
                    fs::path rel = fs::relative(asset.realName, projectRoot);
                    std::string shortPath = "Resources/" + rel.string();
                    std::replace(shortPath.begin(), shortPath.end(), '\\', '/');
                    ImGui::TextDisabled("%s", shortPath.c_str());
                    ImGui::SameLine();
                    ImGui::SmallButton("Copy Path");
                    if (ImGui::IsItemClicked()) ImGui::SetClipboardText(shortPath.c_str());
                    break;
                }
            }
        }
        ImGui::EndChild(); // End footer

        ImGui::EndChild(); // End files panel

        ImGui::Columns(1); // End columns
    }

    /**
     * @brief Render the AssetBrowser window.
     * This function is responsible for drawing the asset browser UI,
     * including the directory tree, asset grid, and context menus.
     */
    void AssetBrowser::Render()
    {
        // Initialize icons on first render
        assets_browser.InitIcons();

        // Return if window is closed
        if (!IsOpen()) return;

        if (!ImGui::Begin(Name().c_str(), GetOpenPtr()))
        {
            ImGui::End();
            return;
        }

        // Draw the asset browser window
        assets_browser.Draw(m_name.c_str());

        ImGui::End(); // End ImGui window
    }

    /**
     * @brief Static callback for handling external files dropped into the asset browser.
     * @param filePaths Vector of paths representing dropped files.
     */
    void AssetBrowser::OnExternalFilesDropped(const std::vector<std::string>& filePaths)
    {
        // Find existing AssetBrowser window
        auto* browserWindow = Ermine::editor::EditorGUI::GetWindow<Ermine::ImguiUI::AssetBrowser>();
        if (!browserWindow)
        {
            EE_CORE_WARN("AssetBrowser window not found � cannot import dropped files.");
            return;
        }

        // Forward dropped files to the asset browser
        EE_CORE_INFO("Importing {} dropped files into Asset Browser...", filePaths.size());
        browserWindow->assets_browser.HandleDroppedFiles(filePaths);
    }
}
