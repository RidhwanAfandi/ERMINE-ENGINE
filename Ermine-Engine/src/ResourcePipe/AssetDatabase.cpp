/* Start Header ************************************************************************/
/*!
\file       AssetDatabase.cpp
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

#include <PreCompile.h>
#include "AssetDatabase.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace Ermine {

    //=============================================================================
    // AssetEntry Implementation
    //=============================================================================
    bool AssetEntry::IsOutOfDate() const {
        if (!std::filesystem::exists(sourcePath)) {
            return false; // Source file deleted
        }

        auto currentModTime = std::filesystem::last_write_time(sourcePath);
        return currentModTime > lastModified;
    }

    //=============================================================================
    // AssetDatabase Implementation
    //=============================================================================
    AssetDatabase::AssetDatabase() {
    }

    AssetDatabase::~AssetDatabase() {
        // Auto-save on destruction (optional)
        // Save();
    }

    bool AssetDatabase::Initialize(const std::string& projectPath) {
        m_ProjectPath = projectPath;

        // Create .ermine directory if it doesn't exist
        std::filesystem::path ermineDir = std::filesystem::path(projectPath) / ".ermine";
        std::filesystem::create_directories(ermineDir);

        m_DatabasePath = (ermineDir / "AssetDatabase.db").string();

        std::cout << "[AssetDatabase] Initialized at: " << m_DatabasePath << std::endl;

        // Try to load existing database
        return Load();
    }

    bool AssetDatabase::Load() {
        if (!std::filesystem::exists(m_DatabasePath)) {
            std::cout << "[AssetDatabase] No existing database found, starting fresh." << std::endl;
            return true; // Not an error, just empty
        }

        std::ifstream file(m_DatabasePath);
        if (!file.is_open()) {
            std::cerr << "[AssetDatabase] ERROR: Failed to open database file." << std::endl;
            return false;
        }

        m_Assets.clear();

        std::string line;
        AssetEntry currentEntry;
        bool readingEntry = false;

        while (std::getline(file, line)) {
            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') {
                continue; // Skip empty lines and comments
            }

            if (line == "ASSET_START") {
                readingEntry = true;
                currentEntry = AssetEntry{};
            }
            else if (line == "ASSET_END") {
                if (readingEntry) {
                    m_Assets.push_back(currentEntry);
                    readingEntry = false;
                }
            }
            else if (readingEntry) {
                size_t splitPos = line.find('=');
                if (splitPos == std::string::npos) continue;

                std::string key = line.substr(0, splitPos);
                std::string value = line.substr(splitPos + 1);

                if (key == "InstanceGUID") {
                    currentEntry.guid.m_Instance.m_Value = std::stoull(value, nullptr, 16);
                }
                else if (key == "TypeGUID") {
                    currentEntry.guid.m_Type.m_Value = std::stoull(value, nullptr, 16);
                }
                else if (key == "SourcePath") {
                    currentEntry.sourcePath = value;
                }
                else if (key == "CachedPath") {
                    currentEntry.cachedPath = value;
                }
                else if (key == "LastModified") {
                    auto timeVal = std::stoull(value);
                    currentEntry.lastModified = std::filesystem::file_time_type(
                        std::chrono::duration<uint64_t>(timeVal));
                }
                else if (key == "ResourceType") {
                    currentEntry.resourceType = value;
                }
            }
        }

        file.close();

        // Rebuild lookup maps for fast searching
        RebuildLookupMaps();

        std::cout << "[AssetDatabase] Loaded " << m_Assets.size() << " assets from database." << std::endl;
        return true;
    }

    bool AssetDatabase::Save() const {
        std::ofstream file(m_DatabasePath);
        if (!file.is_open()) {
            std::cerr << "[AssetDatabase] ERROR: Failed to save database file." << std::endl;
            return false;
        }

        file << "# Ermine Asset Database\n";
        file << "# Auto-generated - Do not edit manually\n\n";

        for (const auto& asset : m_Assets) {
            file << "ASSET_START\n";
            file << "InstanceGUID=" << std::hex << asset.guid.m_Instance.m_Value << std::dec << "\n";
            file << "TypeGUID=" << std::hex << asset.guid.m_Type.m_Value << std::dec << "\n";
            file << "SourcePath=" << asset.sourcePath << "\n";
            file << "CachedPath=" << asset.cachedPath << "\n";
            file << "LastModified=" << asset.lastModified.time_since_epoch().count() << "\n";
            file << "ResourceType=" << asset.resourceType << "\n";
            file << "ASSET_END\n\n";
        }

        file.close();
        std::cout << "[AssetDatabase] Saved " << m_Assets.size() << " assets to database." << std::endl;
        return true;
    }

    AssetEntry* AssetDatabase::FindBySourcePath(const std::string& sourcePath) {
        auto it = m_SourcePathToIndex.find(sourcePath);
        if (it != m_SourcePathToIndex.end()) {
            return &m_Assets[it->second];
        }
        return nullptr;
    }

    const AssetEntry* AssetDatabase::FindBySourcePath(const std::string& sourcePath) const {
        auto it = m_SourcePathToIndex.find(sourcePath);
        if (it != m_SourcePathToIndex.end()) {
            return &m_Assets[it->second];
        }
        return nullptr;
    }

    AssetEntry* AssetDatabase::FindByGUID(const xresource::full_guid& guid) {
        uint64_t key = GUIDToKey(guid);
        auto it = m_GUIDToIndex.find(key);
        if (it != m_GUIDToIndex.end()) {
            return &m_Assets[it->second];
        }
        return nullptr;
    }

    const AssetEntry* AssetDatabase::FindByGUID(const xresource::full_guid& guid) const {
        uint64_t key = GUIDToKey(guid);
        auto it = m_GUIDToIndex.find(key);
        if (it != m_GUIDToIndex.end()) {
            return &m_Assets[it->second];
        }
        return nullptr;
    }

    bool AssetDatabase::RegisterAsset(const AssetEntry& entry) {
        // Check if asset already exists
        if (FindBySourcePath(entry.sourcePath) != nullptr) {
            std::cerr << "[AssetDatabase] ERROR: Asset already registered: " << entry.sourcePath << std::endl;
            return false;
        }

        m_Assets.push_back(entry);

        // Update lookup maps
        size_t index = m_Assets.size() - 1;
        m_SourcePathToIndex[entry.sourcePath] = index;
        m_GUIDToIndex[GUIDToKey(entry.guid)] = index;

        std::cout << "[AssetDatabase] Registered: " << entry.sourcePath
            << " (GUID: 0x" << std::hex << entry.guid.m_Instance.m_Value << std::dec << ")" << std::endl;

        return true;
    }

    bool AssetDatabase::UpdateAsset(const std::string& sourcePath, const AssetEntry& entry) {
        auto* existing = FindBySourcePath(sourcePath);
        if (!existing) {
            std::cerr << "[AssetDatabase] ERROR: Asset not found: " << sourcePath << std::endl;
            return false;
        }

        // Update the entry
        *existing = entry;
        RebuildLookupMaps();
        std::cout << "[AssetDatabase] Updated: " << sourcePath << std::endl;
        return true;
    }

    bool AssetDatabase::RemoveAsset(const std::string& sourcePath) {
        auto it = m_SourcePathToIndex.find(sourcePath);
        if (it == m_SourcePathToIndex.end()) {
            return false;
        }

        size_t index = it->second;
        m_Assets.erase(m_Assets.begin() + index);

        // Rebuild lookup maps since indices changed
        RebuildLookupMaps();

        std::cout << "[AssetDatabase] Removed: " << sourcePath << std::endl;
        return true;
    }

    std::vector<AssetEntry*> AssetDatabase::GetAssetsByType(const std::string& resourceType) {
        std::vector<AssetEntry*> results;

        for (auto& asset : m_Assets) {
            if (asset.resourceType == resourceType) {
                results.push_back(&asset);
            }
        }

        return results;
    }

    bool AssetDatabase::NeedsReimport(const std::string& sourcePath) const {
        const auto* entry = FindBySourcePath(sourcePath);
        if (!entry) {
            return true; // New asset, needs import
        }

        return entry->IsOutOfDate();
    }

    void AssetDatabase::Clear() {
        m_Assets.clear();
        m_SourcePathToIndex.clear();
        m_GUIDToIndex.clear();
    }

    void AssetDatabase::RebuildLookupMaps() {
        m_SourcePathToIndex.clear();
        m_GUIDToIndex.clear();

        for (size_t i = 0; i < m_Assets.size(); ++i) {
            m_SourcePathToIndex[m_Assets[i].sourcePath] = i;
            m_GUIDToIndex[GUIDToKey(m_Assets[i].guid)] = i;
        }
    }

} // namespace Ermine