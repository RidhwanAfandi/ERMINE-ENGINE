/* Start Header ************************************************************************/
/*!
\file       AssetDatabase.h
\author     HURNG Kai Rui, h.kairui, 2301278, h.kairui@digipen.edu (100%)
\date       01/11/2025
\brief      This file implements the AssetDatabase system used by the engine to
            store and manage metadata for all imported assets. The database tracks
            information such as source file paths, cached binary paths, GUIDs,
            resource types, and last modification timestamps. It provides lookup,
            registration, updating, and removal of asset records, ensuring that
            each resource can be efficiently reimported or validated. The database
            is saved as a simple text file under the project’s .ermine directory
            and rebuilt automatically when needed.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
//#include "xresource_pipeline.h"
#include "../dependencies/xresource_guid/source/xresource_guid.h"

namespace Ermine {

    //=============================================================================
    // AssetEntry - Represents a single asset in the database
    //=============================================================================
    struct AssetEntry {
        xresource::full_guid guid;                      // Unique identifier
        std::string sourcePath;                         // Original file path (e.g., "Assets/Textures/Wood.png")
        std::string cachedPath;                         // Processed file path (e.g., "Cache/12345678.dds")
        std::filesystem::file_time_type lastModified;   // Last modification time
        std::string resourceType;                       // "TEXTURE", "STATIC_MESH", "SKINNED_MESH"

        // Helper to check if source file has been modified
        bool IsOutOfDate() const;
    };

    //=============================================================================
    // AssetDatabase - Manages all asset metadata and GUID mappings
    //=============================================================================
    class AssetDatabase {
    public:
        AssetDatabase();
        ~AssetDatabase();

        // Initialize database with project path
        bool Initialize(const std::string& projectPath);

        // Load database from disk
        bool Load();

        // Save database to disk
        bool Save() const;

        // Find asset by source path
        AssetEntry* FindBySourcePath(const std::string& sourcePath);
        const AssetEntry* FindBySourcePath(const std::string& sourcePath) const;

        // Find asset by GUID
        AssetEntry* FindByGUID(const xresource::full_guid& guid);
        const AssetEntry* FindByGUID(const xresource::full_guid& guid) const;

        // Register a new asset
        bool RegisterAsset(const AssetEntry& entry);

        // Update an existing asset entry
        bool UpdateAsset(const std::string& sourcePath, const AssetEntry& entry);

        // Remove an asset from database
        bool RemoveAsset(const std::string& sourcePath);

        // Get all assets of a specific type
        std::vector<AssetEntry*> GetAssetsByType(const std::string& resourceType);

        // Get all assets
        const std::vector<AssetEntry>& GetAllAssets() const { return m_Assets; }

        // Check if an asset needs reimporting (source file modified)
        bool NeedsReimport(const std::string& sourcePath) const;

        // Clear all entries
        void Clear();

        // Get database file path
        std::string GetDatabasePath() const { return m_DatabasePath; }

    private:
        std::string m_ProjectPath;                                      // Root project directory
        std::string m_DatabasePath;                                     // Path to .ermine/AssetDatabase.db
        std::vector<AssetEntry> m_Assets;                               // All registered assets

        // Quick lookup maps
        std::unordered_map<std::string, size_t> m_SourcePathToIndex;   // source path -> index in m_Assets
        std::unordered_map<uint64_t, size_t> m_GUIDToIndex;            // instance GUID -> index in m_Assets

        // Rebuild lookup maps after loading or modifications
        void RebuildLookupMaps();

        // Helper to convert GUID to uint64 for map key
        uint64_t GUIDToKey(const xresource::full_guid& guid) const {
            return guid.m_Instance.m_Value;
        }
    };

} // namespace Ermine