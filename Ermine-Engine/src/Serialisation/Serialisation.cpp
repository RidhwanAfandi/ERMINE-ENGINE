/* Start Header ************************************************************************/
/*!
\file       Serialisation.cpp
\author     WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu
\date       Jan 31, 2026
\brief      Serialisation functions for Config and Scene

Copyright (C) 2026 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Serialisation.h"
#include "Entity.h"
#include "Components.h"
#include "Renderer.h"
#include "MeshTypes.h"
#include "AssetManager.h"

#include <document.h>
#include <writer.h>
#include <prettywriter.h>
#include <stringbuffer.h>
#include <ostreamwrapper.h>
#include <istreamwrapper.h>
#include "GeometryFactory.h"
#include "Physics.h"
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include "NavMesh.h"

namespace {
    std::string BuildMaterialSignature(const Ermine::graphics::Material& material,
        std::string_view customFragmentShader,
        std::string_view meshName = {})
    {
        auto canonicalName = [](const std::string& name) -> std::string {
            if (name == "material.albedo") return "materialAlbedo";
            if (name == "material.albedoMap") return "materialAlbedoMap";
            if (name == "material.normalMap") return "materialNormalMap";
            if (name == "material.metallic") return "materialMetallic";
            if (name == "material.roughness") return "materialRoughness";
            if (name == "material.emissive") return "materialEmissive";
            if (name == "material.emissiveIntensity") return "materialEmissiveIntensity";
            if (name == "material.ao") return "materialAo";
            if (name == "material.normalStrength") return "materialNormalStrength";
            if (name == "material.hasNormalMap") return "materialHasNormalMap";
            if (name == "material.metallicMap") return "materialMetallicMap";
            if (name == "materialAlbedoMap") return "materialAlbedoMap";
            if (name == "materialNormalMap") return "materialNormalMap";
            if (name == "materialMetallicMap") return "materialMetallicMap";
            if (name == "materialRoughnessMap") return "materialRoughnessMap";
            if (name == "materialAoMap") return "materialAoMap";
            if (name == "materialEmissiveMap") return "materialEmissiveMap";
            if (name == "materialAlpha" || name == "materialTransparency")
                return {};
            return name;
        };

        auto normalizePath = [](std::string path) -> std::string {
            for (char& c : path) {
                if (c == '\\') c = '/';
                else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return path;
        };

        std::map<std::string, Ermine::graphics::MaterialParam> normalized;
        for (const auto& [name, param] : material.GetParameters()) {
            std::string key = canonicalName(name);
            if (key.empty())
                continue;
            if (normalized.find(key) == normalized.end())
                normalized.emplace(key, param);
        }

        std::ostringstream oss;
        oss << std::setprecision(6) << std::fixed;
        oss << "mesh=" << meshName << ";";
        oss << "frag=" << customFragmentShader << ";";

        Ermine::Vec2 uvScale = material.GetUVScale();
        Ermine::Vec2 uvOffset = material.GetUVOffset();
        oss << "uvs=" << uvScale.x << "," << uvScale.y << ";";
        oss << "uvo=" << uvOffset.x << "," << uvOffset.y << ";";

        for (const auto& [name, param] : normalized) {
            oss << "p:" << name << "|";
            switch (param.type) {
            case Ermine::graphics::MaterialParamType::FLOAT:
                oss << "f:" << (param.floatValues.empty() ? 0.0f : param.floatValues[0]);
                break;
            case Ermine::graphics::MaterialParamType::VEC2:
                oss << "v2:";
                if (param.floatValues.size() >= 2)
                    oss << param.floatValues[0] << "," << param.floatValues[1];
                break;
            case Ermine::graphics::MaterialParamType::VEC3:
                oss << "v3:";
                if (param.floatValues.size() >= 3)
                    oss << param.floatValues[0] << "," << param.floatValues[1] << "," << param.floatValues[2];
                break;
            case Ermine::graphics::MaterialParamType::VEC4:
                oss << "v4:";
                if (param.floatValues.size() >= 4)
                    oss << param.floatValues[0] << "," << param.floatValues[1] << "," << param.floatValues[2] << "," << param.floatValues[3];
                break;
            case Ermine::graphics::MaterialParamType::INT:
                oss << "i:" << param.intValue;
                break;
            case Ermine::graphics::MaterialParamType::BOOL:
                oss << "b:" << (param.boolValue ? 1 : 0);
                break;
            case Ermine::graphics::MaterialParamType::TEXTURE_2D:
                oss << "t:";
                if (param.texture) {
                    std::string path =
                        Ermine::AssetManager::GetInstance().ResolveTexturePathForMaterialWrite(param.texture);
                    oss << normalizePath(path);
                }
                break;
            }
            oss << ";";
        }

        return oss.str();
    }

    std::string ComputePreferredMaterialBaseName(const Ermine::ECS& ecs, Ermine::EntityID id)
    {
        std::string baseName = "Material";
        if (ecs.HasComponent<Ermine::ObjectMetaData>(id)) {
            const auto& meta = ecs.GetComponent<Ermine::ObjectMetaData>(id);
            if (!meta.name.empty())
                baseName = meta.name;
            if (baseName.rfind("Mesh_", 0) == 0 && baseName.size() > 5)
                baseName = baseName.substr(5);
        }
        return Ermine::AssetManager::SanitizeAssetName(baseName);
    }

    void CompileAllMaterialsForSceneSave(Ermine::ECS& ecs)
    {
        auto& assetManager = Ermine::AssetManager::GetInstance();
        assetManager.ScanMaterialAssets("../Resources/Materials/", false);

        struct ExistingMaterialEntry {
            Ermine::Guid guid;
            std::string name;
        };

        std::unordered_map<std::string, std::vector<ExistingMaterialEntry>> signatureIndex;
        signatureIndex.reserve(assetManager.GetMaterialPathsByGuid().size());
        std::unordered_map<Ermine::Guid, std::shared_ptr<Ermine::graphics::Material>> boundByGuid;
        std::vector<Ermine::EntityID> materialEntities;
        materialEntities.reserve(256);

        for (const auto& [guid, path] : assetManager.GetMaterialPathsByGuid()) {
            auto existing = assetManager.LoadMaterialAsset(path, false);
            if (!existing)
                continue;
            std::string frag;
            if (const std::string* stored = assetManager.GetMaterialCustomFragmentShader(guid))
                frag = *stored;
            const std::string sig = BuildMaterialSignature(*existing, frag, {});
            signatureIndex[sig].push_back({ guid, std::filesystem::path(path).stem().string() });
            boundByGuid.try_emplace(guid, existing);
        }

        const std::filesystem::path materialsDir = std::filesystem::absolute("../Resources/Materials");

        for (Ermine::EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
            if (!ecs.IsEntityValid(id) || !ecs.HasComponent<Ermine::Material>(id))
                continue;

            materialEntities.push_back(id);
            auto& comp = ecs.GetComponent<Ermine::Material>(id);
            auto matShared = comp.GetSharedMaterial();
            if (!matShared)
                continue;

            const std::string preferredName = ComputePreferredMaterialBaseName(ecs, id);
            const std::string currentSig = BuildMaterialSignature(*matShared, comp.customFragmentShader, {});

            Ermine::Guid resolvedGuid{};
            std::string resolvedName;
            auto sigIt = signatureIndex.find(currentSig);
            if (sigIt != signatureIndex.end() && !sigIt->second.empty()) {
                auto matchIt = std::find_if(sigIt->second.begin(), sigIt->second.end(),
                    [&](const ExistingMaterialEntry& e) { return e.name == preferredName; });
                if (matchIt != sigIt->second.end()) {
                    resolvedGuid = matchIt->guid;
                    resolvedName = matchIt->name;
                }
                else {
                    resolvedGuid = sigIt->second.front().guid;
                    resolvedName = sigIt->second.front().name;
                }

                // Force recompilation overwrite even when signature/name already matches.
                resolvedGuid = assetManager.SaveMaterialAsset(resolvedName, *matShared, true, comp.customFragmentShader);
            }
            else {
                std::string saveName = preferredName;
                std::filesystem::path targetPath = materialsDir / (saveName + ".mat");
                if (std::filesystem::exists(targetPath)) {
                    int suffix = 1;
                    do {
                        saveName = preferredName + "_Copy" + std::to_string(suffix++);
                        targetPath = materialsDir / (saveName + ".mat");
                    } while (std::filesystem::exists(targetPath));
                }

                resolvedName = saveName;
                resolvedGuid = assetManager.SaveMaterialAsset(resolvedName, *matShared, true, comp.customFragmentShader);
                if (resolvedGuid.IsValid()) {
                    signatureIndex[currentSig].push_back({ resolvedGuid, resolvedName });
                }
            }

            if (!resolvedGuid.IsValid())
                continue;

            // Force-write/refresh .meta alongside recompiled .mat.
            const std::string resolvedPath = assetManager.GetMaterialPathByGuid(resolvedGuid);
            if (!resolvedPath.empty()) {
                if (resolvedName.empty())
                    resolvedName = std::filesystem::path(resolvedPath).stem().string();

                std::filesystem::path metaPath = resolvedPath;
                metaPath += ".meta";
                SaveAssetMetaGuid(metaPath, resolvedGuid, "Material", 1, true);
            }

            auto& bucket = signatureIndex[currentSig];
            const bool alreadyIndexed = std::any_of(bucket.begin(), bucket.end(),
                [&](const ExistingMaterialEntry& e) { return e.guid == resolvedGuid; });
            if (!alreadyIndexed)
                bucket.push_back({ resolvedGuid, resolvedName });

            auto boundIt = boundByGuid.find(resolvedGuid);
            if (boundIt != boundByGuid.end() && boundIt->second) {
                comp.SetMaterial(boundIt->second, resolvedGuid);
                continue;
            }

            auto shared = assetManager.GetMaterialByGuid(resolvedGuid);
            if (!shared)
                shared = matShared;
            comp.SetMaterial(shared, resolvedGuid);
            boundByGuid[resolvedGuid] = shared;
        }

        // Ensure all entities sharing a GUID also share the same material instance.
        for (Ermine::EntityID id : materialEntities) {
            auto& comp = ecs.GetComponent<Ermine::Material>(id);
            if (!comp.materialGuid.IsValid())
                continue;
            auto it = boundByGuid.find(comp.materialGuid);
            if (it != boundByGuid.end() && it->second)
                comp.SetMaterial(it->second, comp.materialGuid);
        }
    }
}


using namespace rapidjson;

using Ermine::graphics::GeometryFactory; // make intent explicit

void Ermine::Mesh::RebuildPrimitive() {
    if (primitive.type == "Cube") {
        *this = GeometryFactory::CreateCube(primitive.size.x, primitive.size.y, primitive.size.z);
        kind = MeshKind::Primitive;
    }
    else if (primitive.type == "Sphere") {
        *this = GeometryFactory::CreateSphere(primitive.size.x); // adapt to your API
        kind = MeshKind::Primitive;
    }
    else if (primitive.type == "Quad") {
        *this = GeometryFactory::CreateQuad(primitive.size.x, primitive.size.y); // adapt to your API
        kind = MeshKind::Primitive;
    }
    else if (primitive.type == "Cone") {
        // primitive.size.x stores diameter (radius * 2), so divide by 2 to get actual radius
        // primitive.size.y = height
        *this = GeometryFactory::CreateCone(primitive.size.x / 2.0f, primitive.size.y);
        kind = MeshKind::Primitive;
    }
    else {
        EE_CORE_WARN("Unknown primitive type: {}", primitive.type);
        kind = MeshKind::None;
    }
    // TODO: other primitives...
}

namespace Ermine {
    static inline EntityID FindByGuid(const Guid& g) {
        return ECS::GetInstance().GetGuidRegistry().FindEntity(g);
    }

    void ResolveHierarchyGuids(Ermine::ECS& ecs)
    {
        // 1) Clear runtime links
        for (EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
            if (!ecs.IsEntityValid(id) || !ecs.HasComponent<HierarchyComponent>(id)) continue;
            auto& h = ecs.GetComponent<HierarchyComponent>(id);
            h.parent = HierarchyComponent::INVALID_PARENT;
            h.children.clear();
        }

        // 2) Rebuild from GUIDs
        for (EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
            if (!ecs.IsEntityValid(id) || !ecs.HasComponent<HierarchyComponent>(id)) continue;
            auto& h = ecs.GetComponent<HierarchyComponent>(id);

            if (h.parentGuid.IsValid()) {
                const EntityID p = FindByGuid(h.parentGuid);
                if (ecs.IsEntityValid(p) && ecs.HasComponent<HierarchyComponent>(p)) {
                    h.parent = p;
                    ecs.GetComponent<HierarchyComponent>(p).children.push_back(id);
                }
                else {
                    h.parent = HierarchyComponent::INVALID_PARENT; // dangling -> root
                }
            }

            // Optional symmetry (not strictly required if parent is authoritative)
            for (const Guid& cg : h.childrenGuids) {
                const EntityID cid = FindByGuid(cg);
                if (ecs.IsEntityValid(cid) && ecs.HasComponent<HierarchyComponent>(cid)) {
                    // ensure child has me as parent
                    ecs.GetComponent<HierarchyComponent>(cid).parent = id;
                    h.children.push_back(cid);
                }
            }
        }

        // 3) Recompute depth + dirties
        auto computeDepth = [&](EntityID n) {
            int d = 0;
            std::unordered_set<EntityID> seen;
            EntityID cur = n;
            while (ecs.IsEntityValid(cur)) {
                const auto& h = ecs.GetComponent<HierarchyComponent>(cur);
                if (h.parent == HierarchyComponent::INVALID_PARENT) break;
                if (!seen.insert(cur).second) break; // cycle guard
                cur = h.parent; ++d;
            }
            return d;
            };

        for (EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
            if (!ecs.IsEntityValid(id) || !ecs.HasComponent<HierarchyComponent>(id)) continue;
            auto& h = ecs.GetComponent<HierarchyComponent>(id);
            h.depth = computeDepth(id);
            h.isDirty = true;
            h.worldTransformDirty = true;
        }
    }
}

static void CollectSubtree(const Ermine::ECS& ecs, Ermine::EntityID root, std::vector<Ermine::EntityID>& out)
{
    if (!ecs.IsEntityValid(root)) return;
    std::vector<Ermine::EntityID> stack{ root };
    while (!stack.empty()) {
        Ermine::EntityID e = stack.back(); stack.pop_back();
        if (!ecs.IsEntityValid(e)) continue;
        out.push_back(e);
        if (ecs.HasComponent<Ermine::HierarchyComponent>(e)) {
            const auto& h = ecs.GetComponent<Ermine::HierarchyComponent>(e);
            for (Ermine::EntityID c : h.children) stack.push_back(c);
        }
    }
}

// --- ImGui Windows Serialization Helpers ---
static rapidjson::Value SerializeImGuiWindows(const std::unordered_map<std::string, bool>& windows, rapidjson::Document::AllocatorType& allocator)
{
    // Create a JSON object to hold the window states
    rapidjson::Value obj(rapidjson::kObjectType);

    // Serialize each window's open state
    for (const auto& [name, open] : windows)
        obj.AddMember(rapidjson::Value(name.c_str(), allocator), rapidjson::Value(open), allocator);

    return obj;
}
static std::unordered_map<std::string, bool> DeserializeImGuiWindows(const rapidjson::Value& v)
{
    std::unordered_map<std::string, bool> out;

    // Ensure the value is an object
    if (!v.IsObject()) return out;

    // Deserialize each window's open state
    for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
    {
        // Check that the name is a string and the value is a boolean
        if (it->name.IsString() && it->value.IsBool())
            out[it->name.GetString()] = it->value.GetBool();
    }

    return out;
}

std::string SerializeConfig(const Config& config) {
    Document d;
    d.SetObject();
    auto& allocator = d.GetAllocator();

    d.AddMember("windowWidth", config.windowWidth, allocator);
    d.AddMember("windowHeight", config.windowHeight, allocator);
    d.AddMember("fullscreen", config.fullscreen, allocator);
    d.AddMember("maximized", config.maximized, allocator);
    d.AddMember("title", Value(config.title.c_str(), allocator), allocator);
    d.AddMember("imguiWindows", SerializeImGuiWindows(config.imguiWindows, allocator), allocator);
	d.AddMember("fontSize", config.fontSize, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);

    return buffer.GetString();
}

Config DeserializeConfig(const std::string& jsonStr) {
    Document d;
    d.Parse(jsonStr.c_str());

    if (d.HasParseError() || !d.IsObject()) {
        throw std::runtime_error("Invalid JSON string");
    }

    Config config{};
    if (d.HasMember("windowWidth") && d["windowWidth"].IsInt())
        config.windowWidth = d["windowWidth"].GetInt();
    if (d.HasMember("windowHeight") && d["windowHeight"].IsInt())
        config.windowHeight = d["windowHeight"].GetInt();
    if (d.HasMember("fullscreen") && d["fullscreen"].IsBool())
        config.fullscreen = d["fullscreen"].GetBool();
    if (d.HasMember("maximized") && d["maximized"].IsBool())
        config.maximized = d["maximized"].GetBool();
    if (d.HasMember("title") && d["title"].IsString())
        config.title = d["title"].GetString();
    if (d.HasMember("imguiWindows") && d["imguiWindows"].IsObject())
        config.imguiWindows = DeserializeImGuiWindows(d["imguiWindows"]);
	if (d.HasMember("fontSize") && d["fontSize"].IsNumber())
		config.fontSize = d["fontSize"].GetFloat();

    return config;
}

void SaveConfigToFile(const Config& config, const std::filesystem::path& path, bool pretty)
{

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + path.parent_path().string());
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Could not open file for writing: " + path.string());
    }

    rapidjson::OStreamWrapper osw(ofs);
    if (pretty) {
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        rapidjson::Document d; d.SetObject(); auto& a = d.GetAllocator();

        if (!config.maximized)
        {
            d.AddMember("windowWidth", config.windowWidth, a);
            d.AddMember("windowHeight", config.windowHeight, a);
        }
        else
        {
            d.AddMember("windowWidth", 1920, a);
            d.AddMember("windowHeight", 1080, a);
        }

        d.AddMember("fullscreen", config.fullscreen, a);
        d.AddMember("maximized", config.maximized, a);
        d.AddMember("title", rapidjson::Value(config.title.c_str(), a), a);
        d.AddMember("imguiWindows", SerializeImGuiWindows(config.imguiWindows, a), a);
		d.AddMember("fontSize", config.fontSize, a);
        d.Accept(writer);
    }
    else {
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        rapidjson::Document d; d.SetObject(); auto& a = d.GetAllocator();
        if (!config.maximized)
        {
            d.AddMember("windowWidth", config.windowWidth, a);
            d.AddMember("windowHeight", config.windowHeight, a);
        }
        else
        {
            d.AddMember("windowWidth", 1920, a);
            d.AddMember("windowHeight", 1080, a);
        }
        d.AddMember("fullscreen", config.fullscreen, a);
        d.AddMember("maximized", config.maximized, a);
        d.AddMember("title", rapidjson::Value(config.title.c_str(), a), a);
        d.AddMember("imguiWindows", SerializeImGuiWindows(config.imguiWindows, a), a);
		d.AddMember("fontSize", config.fontSize, a);
		d.AddMember("baseFontSize", config.baseFontSize, a);
		d.AddMember("themeMode", config.themeMode, a);
        d.Accept(writer);
    }
}

Config LoadConfigFromFile(const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Could not open file for reading: " + path.string());
    }

    IStreamWrapper isw(ifs);
    Document d;
    d.ParseStream(isw);

    if (d.HasParseError() || !d.IsObject()) {
        throw std::runtime_error("Invalid JSON file: " + path.string());
    }

    Config config{};
    if (d.HasMember("windowWidth") && d["windowWidth"].IsInt())
        config.windowWidth = d["windowWidth"].GetInt();
    if (d.HasMember("windowHeight") && d["windowHeight"].IsInt())
        config.windowHeight = d["windowHeight"].GetInt();
    if (d.HasMember("fullscreen") && d["fullscreen"].IsBool())
        config.fullscreen = d["fullscreen"].GetBool();
    if (d.HasMember("maximized") && d["maximized"].IsBool())
        config.maximized = d["maximized"].GetBool();
    if (d.HasMember("title") && d["title"].IsString())
        config.title = d["title"].GetString();
	if (d.HasMember("imguiWindows") && d["imguiWindows"].IsObject())
		config.imguiWindows = DeserializeImGuiWindows(d["imguiWindows"]);
	if (d.HasMember("fontSize") && d["fontSize"].IsNumber())
		config.fontSize = d["fontSize"].GetFloat();
	if (d.HasMember("baseFontSize") && d["baseFontSize"].IsNumber())
		config.baseFontSize = d["baseFontSize"].GetFloat();
    if (d.HasMember("themeMode") && d["themeMode"].IsInt())
		config.themeMode = d["themeMode"].GetInt();

    return config;
}


void SaveSceneToFile(const Ermine::ECS& ecs, const std::filesystem::path& path, bool pretty) {
    auto& mutableEcs = const_cast<Ermine::ECS&>(ecs);

    // Sync all Material components from their internal materials before saving
    for (Ermine::EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
        if (ecs.IsEntityValid(id) && ecs.HasComponent<Ermine::Material>(id)) {
            auto& matComp = mutableEcs.GetComponent<Ermine::Material>(id);
            matComp.SyncFromMaterial();
        }
    }

    // Ensure .mat/.meta assets are compiled and refreshed before scene serialization.
    CompileAllMaterialsForSceneSave(mutableEcs);

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + path.parent_path().string());
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("Could not open file for writing: " + path.string());
    OStreamWrapper osw(ofs);

    Document d; d.SetObject();
    auto& a = d.GetAllocator();

    Value entities(kArrayType);

    for (Ermine::EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
        if (!ecs.IsEntityValid(id)) continue;

        Value e(kObjectType);
        e.AddMember("id", id, a);

        Value comps(kObjectType);

        if (ecs.HasComponent<Ermine::IDComponent>(id)) {
            rapidjson::Value idPayload(rapidjson::kObjectType);
            const auto& c = ecs.GetComponent<Ermine::IDComponent>(id);

            const std::string guid_str = c.guid.ToString();
            idPayload.AddMember(
                rapidjson::Value("guid", a),
                rapidjson::Value(guid_str.c_str(), (rapidjson::SizeType)guid_str.size(), a),
                a
            );

            comps.AddMember(rapidjson::Value("IDComponent", a), idPayload, a);
        }

        for (const std::string& name : ecs.GetComponentNames(id)) {
            const auto* desc = ecs.GetDescriptor(name);

            rapidjson::Value payload(rapidjson::kObjectType);
            bool wrote = false;

            // Prefer the generic serializer if present
            if (desc && desc->serialize)
            {
                desc->serialize(id, payload, a);
                wrote = true;
            }
            else
            {
                if (name == "ScriptsComponent" && ecs.HasComponent<Ermine::ScriptsComponent>(id))
                {
                    auto& scs = ecs.GetComponent<Ermine::ScriptsComponent>(id);
                    scs.Serialize(payload, a);   // <-- this already writes fields
                    wrote = true;
                }
            }

            if (wrote)
                comps.AddMember(rapidjson::Value(name.c_str(), a), payload, a);
        }

        e.AddMember("components", comps, a);
        entities.PushBack(e, a);
    }

    d.AddMember("entities", entities, a);


    if (auto renderer = ecs.GetSystem<Ermine::graphics::Renderer>()) {
        renderer->SyncToGlobalGraphics();

        rapidjson::Value ggJson(rapidjson::kObjectType);
        renderer->m_GlobalGraphics.Serialize(ggJson, a);
        d.AddMember("globalGraphics", ggJson, a);
    }

    if (pretty) { PrettyWriter<OStreamWrapper> w(osw); w.SetIndent(' ', 2); d.Accept(w); }
    else { Writer<OStreamWrapper> w(osw); d.Accept(w); }
}


void LoadSceneFromFile(Ermine::ECS& ecs, const std::filesystem::path& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("Could not open file for reading: " + path.string());

    IStreamWrapper isw(ifs);
    Document d; d.ParseStream(isw);
    if (d.HasParseError() || !d.IsObject())
        throw std::runtime_error("Invalid JSON file: " + path.string());

    // Defensive: ensure we have an "entities" array
    if (!d.HasMember("entities") || !d["entities"].IsArray())
        throw std::runtime_error("Invalid scene JSON (missing 'entities'): " + path.string());

    // Clear MeshManager for new scene
    auto renderer = ecs.GetSystem<Ermine::graphics::Renderer>();
    if (renderer) {
        renderer->m_MeshManager.Clear();
    }

    Ermine::AssetManager::GetInstance().ClearModelCache();

    // Pre-scan materials so GUID -> path is resolved before component deserialization
    Ermine::AssetManager::GetInstance().ScanMaterialAssets("../Resources/Materials/", false);

    for (auto& e : d["entities"].GetArray()) {
        if (!e.IsObject()) continue;

        Ermine::EntityID id = ecs.CreateEntity();

        if (!e.HasMember("components") || !e["components"].IsObject())
            continue;

        const auto& comps = e["components"];

        if (comps.HasMember("IDComponent") && comps["IDComponent"].IsObject()) {
            const auto& payload = comps["IDComponent"];

            Ermine::Guid g =
                (payload.HasMember("guid") && payload["guid"].IsString())
                ? Ermine::Guid::FromString(payload["guid"].GetString())
                : Ermine::Guid::New(); // fallback for old files

            if (ecs.HasComponent<Ermine::IDComponent>(id)) {
                auto& c = ecs.GetComponent<Ermine::IDComponent>(id);
                c.guid = g;
            }
            else {
                ecs.AddComponent<Ermine::IDComponent>(id, Ermine::IDComponent{ g });
            }
            ecs.GetGuidRegistry().Register(id, g);
        }

        for (auto it = comps.MemberBegin(); it != comps.MemberEnd(); ++it) {
            if (!it->value.IsObject()) continue;

            const std::string compName = it->name.GetString();
            const rapidjson::Value& payload = it->value;

            // IDComponent is handled above; avoid warning spam.
            if (compName == "IDComponent")
                continue;

            // Look up the component descriptor and call its type-erased deserializer
            const auto* desc = ecs.GetDescriptor(compName);
            bool handled = false;

            // Prefer the generic deserializer if present
            if (desc && desc->deserialize)
            {
                desc->deserialize(id, payload);
                handled = true;
            }
            else
            {
                // --- Custom fallbacks ---
                if (compName == "ScriptsComponent")
                {
                    // Ensure component exists
                    if (!ecs.HasComponent<Ermine::ScriptsComponent>(id))
                        ecs.AddComponent<Ermine::ScriptsComponent>(id, Ermine::ScriptsComponent{});

                    auto& scs = ecs.GetComponent<Ermine::ScriptsComponent>(id);

                    // Let the component handle scripts + fields
                    scs.Deserialize(payload);

                    // NOTE: Do NOT call AttachAll here if you have a ScriptSystem that does it later.
                    // If you prefer immediate instances after load, you *can* do:
                    scs.AttachAll(id);

                    handled = true;
                }
                else if (compName == "Script")
                {
                    // Optional legacy single-Script -> ScriptsComponent upgrade
                    if (payload.HasMember("class") && payload["class"].IsString())
                    {
                        const std::string cls = payload["class"].GetString();
                        if (!ecs.HasComponent<Ermine::ScriptsComponent>(id))
                            ecs.AddComponent<Ermine::ScriptsComponent>(id, Ermine::ScriptsComponent{});
                        ecs.GetComponent<Ermine::ScriptsComponent>(id).Add(cls, id);
                        handled = true;
                    }
                }
            }


            if (!handled)
            {
                EE_CORE_WARN("Unknown or non-deserializable component '{}' on entity {}; skipping.",
                    compName.c_str(), static_cast<uint64_t>(id));
            }
        }
    }

    // Backward-compat: bind legacy materials (no GUID) in linear passes with signature dedupe.
    {
        auto& assets = Ermine::AssetManager::GetInstance();
        const std::filesystem::path defaultMaterialPath =
            std::filesystem::absolute("../Resources/Materials/Default_White.mat");
        std::filesystem::path defaultMetaPath = defaultMaterialPath;
        defaultMetaPath += ".meta";
        Ermine::Guid defaultMetaGuid{};
        const bool hasDefaultMeta = LoadAssetMetaGuid(defaultMetaPath, defaultMetaGuid) && defaultMetaGuid.IsValid();

        std::shared_ptr<Ermine::graphics::Material> defaultMaterial;
        Ermine::Guid defaultMaterialGuid{};
        if (hasDefaultMeta && std::filesystem::exists(defaultMaterialPath)) {
            defaultMaterial = assets.LoadMaterialAsset(defaultMaterialPath.string(), false);
            if (defaultMaterial) {
                defaultMaterialGuid = assets.GetMaterialGuidForPath(defaultMaterialPath.string());
                if (!defaultMaterialGuid.IsValid())
                    defaultMaterialGuid = defaultMetaGuid;
            }
        }
        else {
            // Fallback when default asset metadata is missing: use in-memory template material.
            auto fallback = std::make_shared<Ermine::graphics::Material>();
            fallback->LoadTemplate(Ermine::graphics::MaterialTemplates::PBR_WHITE());
            auto defaultShader = assets.LoadShader(
                "../Resources/Shaders/vertex.glsl",
                "../Resources/Shaders/fragment_enhanced.glsl"
            );
            if (defaultShader && defaultShader->IsValid())
                fallback->SetShader(defaultShader);
            defaultMaterial = fallback;
        }
        if (!defaultMaterial) {
            EE_CORE_WARN("Default material not found or failed to load: {}", defaultMaterialPath.string());
        }

        std::vector<Ermine::EntityID> materialEntities;
        materialEntities.reserve(256);

        std::vector<Ermine::EntityID> legacyMaterialEntities;
        std::unordered_map<Ermine::Guid, std::shared_ptr<Ermine::graphics::Material>> boundByGuid;

        for (Ermine::EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
            if (!ecs.IsEntityValid(id) || !ecs.HasComponent<Ermine::Material>(id))
                continue;

            materialEntities.push_back(id);
            auto& matComp = ecs.GetComponent<Ermine::Material>(id);
            if (!matComp.materialGuid.IsValid()) {
                legacyMaterialEntities.push_back(id);
                continue;
            }

            auto shared = assets.GetMaterialByGuid(matComp.materialGuid);
            if (!shared)
                shared = matComp.GetSharedMaterial();
            if (shared) {
                matComp.SetMaterial(shared, matComp.materialGuid);
                boundByGuid.try_emplace(matComp.materialGuid, shared);
            }
            else if (defaultMaterial) {
                matComp.SetMaterial(defaultMaterial, defaultMaterialGuid);
                if (defaultMaterialGuid.IsValid())
                    boundByGuid.try_emplace(defaultMaterialGuid, defaultMaterial);
            }
        }

        if (!legacyMaterialEntities.empty()) {
            struct ExistingMaterialEntry {
                Ermine::Guid guid;
                std::string name;
            };

            std::unordered_map<std::string, std::vector<ExistingMaterialEntry>> signatureIndex;
            signatureIndex.reserve(assets.GetMaterialPathsByGuid().size());

            for (const auto& [guid, path] : assets.GetMaterialPathsByGuid()) {
                auto existing = assets.LoadMaterialAsset(path, false);
                if (!existing)
                    continue;
                std::string frag;
                if (const std::string* stored = assets.GetMaterialCustomFragmentShader(guid))
                    frag = *stored;
                const std::string sig = BuildMaterialSignature(*existing, frag, {});
                signatureIndex[sig].push_back({ guid, std::filesystem::path(path).stem().string() });
                boundByGuid.try_emplace(guid, existing);
            }

            const std::filesystem::path materialsDir = std::filesystem::absolute("../Resources/Materials");

            for (Ermine::EntityID id : legacyMaterialEntities) {
                auto& matComp = ecs.GetComponent<Ermine::Material>(id);
                auto matShared = matComp.GetSharedMaterial();
                if (!matShared) {
                    if (defaultMaterial) {
                        matComp.SetMaterial(defaultMaterial, defaultMaterialGuid);
                        if (defaultMaterialGuid.IsValid())
                            boundByGuid.try_emplace(defaultMaterialGuid, defaultMaterial);
                    }
                    continue;
                }

                const std::string preferredName = ComputePreferredMaterialBaseName(ecs, id);
                const std::string sig = BuildMaterialSignature(*matShared, matComp.customFragmentShader, {});

                Ermine::Guid resolvedGuid{};
                auto sigIt = signatureIndex.find(sig);
                if (sigIt != signatureIndex.end() && !sigIt->second.empty()) {
                    auto matchIt = std::find_if(sigIt->second.begin(), sigIt->second.end(),
                        [&](const ExistingMaterialEntry& e) { return e.name == preferredName; });
                    resolvedGuid = (matchIt != sigIt->second.end()) ? matchIt->guid : sigIt->second.front().guid;
                }
                else {
                    std::string saveName = preferredName;
                    std::filesystem::path targetPath = materialsDir / (saveName + ".mat");
                    if (std::filesystem::exists(targetPath)) {
                        int suffix = 1;
                        do {
                            saveName = preferredName + "_Copy" + std::to_string(suffix++);
                            targetPath = materialsDir / (saveName + ".mat");
                        } while (std::filesystem::exists(targetPath));
                    }

                    resolvedGuid = assets.SaveMaterialAsset(saveName, *matShared, false, matComp.customFragmentShader);
                    if (resolvedGuid.IsValid()) {
                        signatureIndex[sig].push_back({ resolvedGuid, saveName });
                    }
                }

                if (!resolvedGuid.IsValid())
                {
                    if (defaultMaterial) {
                        matComp.SetMaterial(defaultMaterial, defaultMaterialGuid);
                        if (defaultMaterialGuid.IsValid())
                            boundByGuid.try_emplace(defaultMaterialGuid, defaultMaterial);
                    }
                    continue;
                }

                auto boundIt = boundByGuid.find(resolvedGuid);
                if (boundIt != boundByGuid.end() && boundIt->second) {
                    matComp.SetMaterial(boundIt->second, resolvedGuid);
                }
                else {
                    auto shared = assets.GetMaterialByGuid(resolvedGuid);
                    if (!shared && defaultMaterial) {
                        shared = defaultMaterial;
                        resolvedGuid = defaultMaterialGuid;
                    }
                    else if (!shared) {
                        shared = matShared;
                    }
                    matComp.SetMaterial(shared, resolvedGuid);
                    if (resolvedGuid.IsValid())
                        boundByGuid[resolvedGuid] = shared;
                }
            }
        }

        // Ensure all entities sharing a GUID also share the same material instance.
        for (Ermine::EntityID id : materialEntities) {
            auto& matComp = ecs.GetComponent<Ermine::Material>(id);
            if (!matComp.materialGuid.IsValid())
                continue;
            auto it = boundByGuid.find(matComp.materialGuid);
            if (it != boundByGuid.end() && it->second)
                matComp.SetMaterial(it->second, matComp.materialGuid);
        }
    }

    ecs.ResyncAllSignaturesFromStorage();

    Ermine::ResolveHierarchyGuids(ecs);

    if (auto navSys = ecs.GetSystem<Ermine::NavMeshSystem>())
    {
        navSys->WarmStartBakedNavMeshes();
    }

    // Upload all registered meshes to GPU and build indirect draw commands
    if (renderer) {
        renderer->m_MeshManager.UploadAndBuild();
        EE_CORE_INFO("Scene loaded: MeshManager populated with {} meshes",
            renderer->m_MeshManager.GetMeshCount());

        if (d.HasMember("globalGraphics") && d["globalGraphics"].IsObject()) {
            renderer->m_GlobalGraphics.Deserialize(d["globalGraphics"]);
            renderer->ApplyFromGlobalGraphics();
        }
    }
}

void SaveCurrentScene(const std::string& sceneName)
{
    // "Level01" = Resources/Scenes/Level01.scene
    filesystem::path scenePath = filesystem::path("Resources") / "Scenes" / (sceneName + ".scene");

    SaveSceneToFile(Ermine::ECS::GetInstance(), scenePath, true);
}

void LoadScene(const std::string& sceneName)
{
    filesystem::path scenePath = filesystem::path("Resources") / "Scenes" / (sceneName + ".scene");

    LoadSceneFromFile(Ermine::ECS::GetInstance(), scenePath);
    Ermine::ECS::GetInstance().GetSystem<Ermine::Physics>()->UpdatePhysicList();
    Ermine::ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>()->MarkDrawDataForRebuild();
}

void SavePrefabToFile(const Ermine::ECS& ecs, Ermine::EntityID root, const std::filesystem::path& path)
{
    // Sync all Material components in the prefab subtree
    std::vector<Ermine::EntityID> toSync{ root };
    while (!toSync.empty()) {
        Ermine::EntityID e = toSync.back();
        toSync.pop_back();
        if (!ecs.IsEntityValid(e)) continue;
        
        if (ecs.HasComponent<Ermine::Material>(e)) {
            auto& matComp = const_cast<Ermine::ECS&>(ecs).GetComponent<Ermine::Material>(e);
            matComp.SyncFromMaterial();
        }

        if (ecs.HasComponent<Ermine::HierarchyComponent>(e)) {
            const auto& h = ecs.GetComponent<Ermine::HierarchyComponent>(e);
            for (auto c : h.children)
                toSync.push_back(c);
        }
    }

    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + path.parent_path().string());
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("Could not open file for writing: " + path.string());
    OStreamWrapper osw(ofs);

    Document d; d.SetObject();
    auto& a = d.GetAllocator();
    Value entities(kArrayType);

    // collect subtree
    std::vector<Ermine::EntityID> toProcess{ root };
    std::vector<Ermine::EntityID> all;
    while (!toProcess.empty()) {
        Ermine::EntityID e = toProcess.back();
        toProcess.pop_back();
        if (!ecs.IsEntityValid(e)) continue;
        all.push_back(e);

        if (ecs.HasComponent<Ermine::HierarchyComponent>(e)) {
            const auto& h = ecs.GetComponent<Ermine::HierarchyComponent>(e);
            for (auto c : h.children)
                toProcess.push_back(c);
        }
    }

    for (Ermine::EntityID id : all) {
        Value e(kObjectType);
        Value comps(kObjectType);

        // always write IDComponent
        if (ecs.HasComponent<Ermine::IDComponent>(id)) {
            const auto& c = ecs.GetComponent<Ermine::IDComponent>(id);
            std::string guid_str = c.guid.ToString();
            Value idPayload(kObjectType);
            idPayload.AddMember("guid", Value(guid_str.c_str(), (rapidjson::SizeType)guid_str.size(), a), a);
            comps.AddMember("IDComponent", idPayload, a);
        }

        // write all other components like the scene
        for (const std::string& name : ecs.GetComponentNames(id)) {
            // ... inside: for (const std::string& name : ecs.GetComponentNames(id)) {
            const auto* desc = ecs.GetDescriptor(name);
            rapidjson::Value payload(rapidjson::kObjectType);
            bool wrote = false;

            // Prefer generic serializer
            if (desc && desc->serialize) {
                desc->serialize(id, payload, a);
                wrote = true;
            }

            // --- Prefab fallback: ScriptsComponent (array of classes)
            if (!wrote && name == "ScriptsComponent" && ecs.HasComponent<Ermine::ScriptsComponent>(id)) {
                const auto& scs = ecs.GetComponent<Ermine::ScriptsComponent>(id);
                rapidjson::Value arr(rapidjson::kArrayType);
                for (const auto& sc : scs.scripts) {
                    rapidjson::Value obj(rapidjson::kObjectType);
                    obj.AddMember(rapidjson::Value("class", a),
                        rapidjson::Value(sc.m_className.c_str(), a), a);
                    arr.PushBack(obj, a);
                }
                payload.AddMember(rapidjson::Value("scripts", a), arr, a);
                wrote = true;
            }

            // --- Prefab fallback: single Script
            if (!wrote && name == "Script" && ecs.HasComponent<Ermine::Script>(id)) {
                const auto& s = ecs.GetComponent<Ermine::Script>(id);
                payload.AddMember(rapidjson::Value("class", a),
                    rapidjson::Value(s.m_className.c_str(), a), a);
                wrote = true;
            }

            if (wrote) {
                comps.AddMember(rapidjson::Value(name.c_str(), a), payload, a);
            }
        }

        e.AddMember("components", comps, a);
        entities.PushBack(e, a);
    }

    d.AddMember("entities", entities, a);

    PrettyWriter<OStreamWrapper> w(osw);
    w.SetIndent(' ', 2);
    d.Accept(w);
}

Ermine::EntityID LoadPrefabFromFile(Ermine::ECS& ecs, const std::filesystem::path& path)
{
    if (!path.has_extension() || path.extension() != ".prefab") {
        EE_CORE_ERROR("LoadPrefabFromFile rejected non-prefab file: {}", path.string());
        return {};
    }

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("Could not open file for reading: " + path.string());

    IStreamWrapper isw(ifs);
    Document d; d.ParseStream(isw);
    if (d.HasParseError() || !d.IsObject())
        throw std::runtime_error("Invalid JSON file: " + path.string());
    if (!d.HasMember("entities") || !d["entities"].IsArray())
        throw std::runtime_error("Invalid prefab JSON (missing 'entities'): " + path.string());

    const auto& ents = d["entities"];

    // oldGuidStr -> new EntityID
    std::unordered_map<std::string, Ermine::EntityID> oldGuidToNew;
    oldGuidToNew.reserve(ents.Size());

    // ---------- Pass 1: create entities, ensure single IDComponent, assign NEW GUIDs ----------
    for (auto& e : ents.GetArray()) {
        if (!e.IsObject()) continue;
        if (!e.HasMember("components") || !e["components"].IsObject()) continue;
        const auto& comps = e["components"];

        std::string oldGuid;
        if (comps.HasMember("IDComponent")) {
            const auto& idc = comps["IDComponent"];
            if (idc.IsObject() && idc.HasMember("guid") && idc["guid"].IsString())
                oldGuid = idc["guid"].GetString();
        }

        Ermine::EntityID newEntity = ecs.CreateEntity();

        // Avoid double-adding IDComponent (some engines add it in CreateEntity)
        Ermine::Guid newGuid = Ermine::Guid::New();
        if (ecs.HasComponent<Ermine::IDComponent>(newEntity)) {
            ecs.GetComponent<Ermine::IDComponent>(newEntity).guid = newGuid;
        }
        else {
            ecs.AddComponent<Ermine::IDComponent>(newEntity, Ermine::IDComponent{ newGuid });
        }
        ecs.GetGuidRegistry().Unregister(newEntity); // safe even if absent
        ecs.GetGuidRegistry().Register(newEntity, newGuid);

        if (!oldGuid.empty())
            oldGuidToNew.emplace(oldGuid, newEntity);
    }

    auto remapEntityByOldGuid = [&](const std::string& old) -> Ermine::EntityID {
        auto it = oldGuidToNew.find(old);
        return it != oldGuidToNew.end() ? it->second : Ermine::HierarchyComponent::INVALID_PARENT;
        };

    Ermine::EntityID rootEntity = 0;

    // ---------- Pass 2: deserialize everything else; remap Hierarchy parentGuid ----------
    for (auto& e : ents.GetArray()) {
        if (!e.IsObject()) continue;
        if (!e.HasMember("components") || !e["components"].IsObject()) continue;
        const auto& comps = e["components"];

        // resolve our new entity from saved old guid
        std::string myOldGuid;
        if (comps.HasMember("IDComponent")) {
            const auto& idc = comps["IDComponent"];
            if (idc.IsObject() && idc.HasMember("guid") && idc["guid"].IsString())
                myOldGuid = idc["guid"].GetString();
        }
        Ermine::EntityID entity = remapEntityByOldGuid(myOldGuid);
        if (!ecs.IsEntityValid(entity)) continue;

        for (auto it = comps.MemberBegin(); it != comps.MemberEnd(); ++it) {
            if (!it->value.IsObject()) continue;
            const std::string compName = it->name.GetString();
            const auto& payload = it->value;

            if (compName == "IDComponent") continue; // already handled in pass 1

            // HierarchyComponent: set parentGuid by remapping old parent guid to the new entity's guid
            if (compName == "HierarchyComponent") {
                Ermine::Guid parentGuid{};
                if (payload.HasMember("parentGuid") && payload["parentGuid"].IsString()) {
                    const std::string oldParentGuid = payload["parentGuid"].GetString();
                    Ermine::EntityID newParent = remapEntityByOldGuid(oldParentGuid);
                    if (newParent != Ermine::HierarchyComponent::INVALID_PARENT &&
                        ecs.HasComponent<Ermine::IDComponent>(newParent)) {
                        parentGuid = ecs.GetComponent<Ermine::IDComponent>(newParent).guid;
                    }
                }

                // Avoid duplicate add
                if (ecs.HasComponent<Ermine::HierarchyComponent>(entity)) {
                    auto& h = ecs.GetComponent<Ermine::HierarchyComponent>(entity);
                    h.parentGuid = parentGuid;
                    h.parent = Ermine::HierarchyComponent::INVALID_PARENT;
                    h.children.clear();
                    h.depth = 0;
                    h.isDirty = true;
                    h.worldTransformDirty = true;
                }
                else {
                    Ermine::HierarchyComponent h;
                    h.parentGuid = parentGuid;
                    ecs.AddComponent<Ermine::HierarchyComponent>(entity, h);
                }

                if (!parentGuid.IsValid() && rootEntity == 0) rootEntity = entity;
                continue;
            }

            // ScriptsComponent (array)
            if (compName == "ScriptsComponent") {
                std::vector<std::string> classNames;
                if (payload.HasMember("scripts") && payload["scripts"].IsArray()) {
                    const auto arr = payload["scripts"].GetArray();
                    classNames.reserve(arr.Size());
                    for (const auto& v : arr) {
                        if (!v.IsObject()) continue;
                        const auto m = v.FindMember("class");
                        if (m != v.MemberEnd() && m->value.IsString())
                            classNames.emplace_back(m->value.GetString());
                    }
                }
                else if (payload.HasMember("class") && payload["class"].IsString()) {
                    // legacy single-class form
                    classNames.emplace_back(payload["class"].GetString());
                }

                if (!ecs.HasComponent<Ermine::ScriptsComponent>(entity))
                    ecs.AddComponent<Ermine::ScriptsComponent>(entity, Ermine::ScriptsComponent{});
                auto& scs = ecs.GetComponent<Ermine::ScriptsComponent>(entity);
                for (const auto& cls : classNames) scs.Add(cls, entity);
                continue;
            }

            // Single Script (legacy)
            if (compName == "Script") {
                if (payload.HasMember("class") && payload["class"].IsString()) {
                    const std::string cls = payload["class"].GetString();
                    // Prefer canonical ScriptsComponent container:
                    if (!ecs.HasComponent<Ermine::ScriptsComponent>(entity))
                        ecs.AddComponent<Ermine::ScriptsComponent>(entity, Ermine::ScriptsComponent{});
                    ecs.GetComponent<Ermine::ScriptsComponent>(entity).Add(cls, entity);
                }
                continue;
            }

            // Generic path
            if (const auto* desc = ecs.GetDescriptor(compName); desc && desc->deserialize) {
                desc->deserialize(entity, payload);
            }
            else {
                EE_CORE_WARN("Prefab component '{}' has no deserializer; skipping.", compName.c_str());
            }
        }
    }

    // finalize
    ecs.ResyncAllSignaturesFromStorage();
    Ermine::ResolveHierarchyGuids(ecs);

    //ecs.GetSystem<Ermine::HierarchySystem>()->ForceUpdateAllTransforms();
    ecs.GetSystem<Ermine::Physics>()->UpdatePhysicList();

    // Return detected root; fallback to first created if none marked as root
    if (rootEntity != 0) return rootEntity;
    if (!oldGuidToNew.empty()) return oldGuidToNew.begin()->second;
    return {};
}

namespace Ermine
{
    static std::string SanitizeGuidNoHyphen(std::string_view s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
        {
            if (c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
                continue;
            if (c >= 'A' && c <= 'F') c = char(c - 'A' + 'a');
            out.push_back(c);
        }
        return out;
    }

    static bool Is32HexLower(std::string_view s)
    {
        if (s.size() != 32) return false;
        for (char c : s)
        {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return false;
        }
        return true;
    }
}

bool LoadAssetMetaGuid(const std::filesystem::path& metaPath, Ermine::Guid& outGuid)
{
    std::ifstream ifs(metaPath, std::ios::binary);
    if (!ifs) return false;

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document d;
    d.ParseStream(isw);

    if (d.HasParseError() || !d.IsObject()) return false;

    auto it = d.FindMember("guid");
    if (it == d.MemberEnd() || !it->value.IsString()) return false;

    std::string cleaned = Ermine::SanitizeGuidNoHyphen(it->value.GetString());
    if (!Ermine::Is32HexLower(cleaned)) return false;

    Ermine::Guid g = Ermine::Guid::FromString(cleaned);   // your format: no hyphen
    if (!g.IsValid()) return false;

    outGuid = g;
    return true;
}

bool SaveAssetMetaGuid(const std::filesystem::path& metaPath,
    const Ermine::Guid& guid,
    std::string_view type,
    int metaVersion,
    bool pretty)
{
    if (metaPath.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(metaPath.parent_path(), ec);
    }

    std::ofstream ofs(metaPath, std::ios::binary | std::ios::trunc);
    if (!ofs) return false;

    rapidjson::OStreamWrapper osw(ofs);

    rapidjson::Document d;
    d.SetObject();
    auto& a = d.GetAllocator();

    std::string guidStr = guid.ToString(); // no hyphen
    d.AddMember("guid",
        rapidjson::Value(guidStr.c_str(), (rapidjson::SizeType)guidStr.size(), a),
        a);

    d.AddMember("metaVersion", metaVersion, a);

    if (!type.empty())
    {
        d.AddMember("type",
            rapidjson::Value(type.data(), (rapidjson::SizeType)type.size(), a),
            a);
    }

    if (pretty)
    {
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        d.Accept(writer);
    }
    else
    {
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        d.Accept(writer);
    }

    return true;
}

Ermine::Guid EnsureMetaForSource(const std::filesystem::path& sourcePath,
    std::string_view type,
    bool pretty)
{
    std::filesystem::path src = sourcePath;
    if (!src.is_absolute())
        src = std::filesystem::absolute(src);

    std::filesystem::path metaPath = src;
    metaPath += ".meta";

    Ermine::Guid existing{};
    if (LoadAssetMetaGuid(metaPath, existing))
        return existing;

    Ermine::Guid created = Ermine::Guid::New();
    SaveAssetMetaGuid(metaPath, created, type, /*metaVersion*/1, pretty);
    return created;
}

// --- Material Serialization ---

#include "Material.h"

void SaveMaterialToFile(const Ermine::graphics::Material& material,
    const std::filesystem::path& path,
    bool pretty,
    std::string_view customFragmentShader)
{
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + path.parent_path().string());
        }
    }

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("Could not open file for writing: " + path.string());
    }

    rapidjson::Document d;
    d.SetObject();
    auto& a = d.GetAllocator();

    // Save all material parameters
    rapidjson::Value paramsObj(rapidjson::kObjectType);

    // Helper lambda to save a parameter if it exists
    auto saveParam = [&](const std::string& name) {
        if (const auto* param = material.GetParameter(name)) {
            rapidjson::Value paramObj(rapidjson::kObjectType);
            
            switch (param->type) {
                case Ermine::graphics::MaterialParamType::FLOAT:
                    paramObj.AddMember("type", "float", a);
                    if (!param->floatValues.empty()) {
                        paramObj.AddMember("value", param->floatValues[0], a);
                    }
                    break;
                    
                case Ermine::graphics::MaterialParamType::VEC2:
                    paramObj.AddMember("type", "vec2", a);
                    if (param->floatValues.size() >= 2) {
                        rapidjson::Value arr(rapidjson::kArrayType);
                        arr.PushBack(param->floatValues[0], a);
                        arr.PushBack(param->floatValues[1], a);
                        paramObj.AddMember("value", arr, a);
                    }
                    break;
                    
                case Ermine::graphics::MaterialParamType::VEC3:
                    paramObj.AddMember("type", "vec3", a);
                    if (param->floatValues.size() >= 3) {
                        rapidjson::Value arr(rapidjson::kArrayType);
                        arr.PushBack(param->floatValues[0], a);
                        arr.PushBack(param->floatValues[1], a);
                        arr.PushBack(param->floatValues[2], a);
                        paramObj.AddMember("value", arr, a);
                    }
                    break;
                    
                case Ermine::graphics::MaterialParamType::VEC4:
                    paramObj.AddMember("type", "vec4", a);
                    if (param->floatValues.size() >= 4) {
                        rapidjson::Value arr(rapidjson::kArrayType);
                        arr.PushBack(param->floatValues[0], a);
                        arr.PushBack(param->floatValues[1], a);
                        arr.PushBack(param->floatValues[2], a);
                        arr.PushBack(param->floatValues[3], a);
                        paramObj.AddMember("value", arr, a);
                    }
                    break;
                    
                case Ermine::graphics::MaterialParamType::INT:
                    paramObj.AddMember("type", "int", a);
                    paramObj.AddMember("value", param->intValue, a);
                    break;
                    
                case Ermine::graphics::MaterialParamType::BOOL:
                    paramObj.AddMember("type", "bool", a);
                    paramObj.AddMember("value", param->boolValue, a);
                    break;
                    
                case Ermine::graphics::MaterialParamType::TEXTURE_2D:
                    paramObj.AddMember("type", "texture2d", a);
                    if (param->texture) {
                        std::string texPath =
                            Ermine::AssetManager::GetInstance().ResolveTexturePathForMaterialWrite(param->texture);
                        paramObj.AddMember("value", rapidjson::Value(texPath.c_str(), a), a);
                    }
                    break;
            }
            
            paramsObj.AddMember(rapidjson::Value(name.c_str(), a), paramObj, a);
        }
    };

    // Save all common material parameters
    saveParam("materialAlbedo");
    saveParam("materialMetallic");
    saveParam("materialRoughness");
    saveParam("materialAo");
    saveParam("materialEmissive");
    saveParam("materialEmissiveIntensity");
    saveParam("materialNormalStrength");
    saveParam("materialShadingModel");
    saveParam("materialCastsShadows");
    saveParam("materialHasAlbedoMap");
    saveParam("materialHasNormalMap");
    saveParam("materialHasRoughnessMap");
    saveParam("materialHasMetallicMap");
    saveParam("materialHasAoMap");
    saveParam("materialHasEmissiveMap");
    saveParam("materialAlbedoMap");
    saveParam("materialNormalMap");
    saveParam("materialRoughnessMap");
    saveParam("materialMetallicMap");
    saveParam("materialAoMap");
    saveParam("materialEmissiveMap");

    d.AddMember("parameters", paramsObj, a);

    // Save UV transform
    rapidjson::Value uvObj(rapidjson::kObjectType);
    auto uvScale = material.GetUVScale();
    auto uvOffset = material.GetUVOffset();
    
    rapidjson::Value scaleArr(rapidjson::kArrayType);
    scaleArr.PushBack(uvScale.x, a);
    scaleArr.PushBack(uvScale.y, a);
    uvObj.AddMember("scale", scaleArr, a);
    
    rapidjson::Value offsetArr(rapidjson::kArrayType);
    offsetArr.PushBack(uvOffset.x, a);
    offsetArr.PushBack(uvOffset.y, a);
    uvObj.AddMember("offset", offsetArr, a);
    
    d.AddMember("uvTransform", uvObj, a);

    if (!customFragmentShader.empty()) {
        rapidjson::Value fragPath;
        fragPath.SetString(customFragmentShader.data(),
            static_cast<rapidjson::SizeType>(customFragmentShader.size()),
            a);
        d.AddMember("customFragmentShader", fragPath, a);
    }

    // Write to file
    rapidjson::OStreamWrapper osw(ofs);
    if (pretty) {
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        d.Accept(writer);
    }
    else {
        rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
        d.Accept(writer);
    }
}

Ermine::graphics::Material LoadMaterialFromFile(const std::filesystem::path& path,
    std::string* outCustomFragmentShader)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        throw std::runtime_error("Could not open file for reading: " + path.string());
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document d;
    d.ParseStream(isw);

    if (d.HasParseError() || !d.IsObject()) {
        throw std::runtime_error("Invalid JSON file: " + path.string());
    }

    Ermine::graphics::Material material;

    // Load parameters
    if (d.HasMember("parameters") && d["parameters"].IsObject()) {
        const auto& paramsObj = d["parameters"];
        
        for (auto it = paramsObj.MemberBegin(); it != paramsObj.MemberEnd(); ++it) {
            std::string paramName = it->name.GetString();
            const auto& paramData = it->value;
            
            if (!paramData.IsObject() || !paramData.HasMember("type")) continue;
            
            std::string type = paramData["type"].GetString();
            
            if (type == "float" && paramData.HasMember("value") && paramData["value"].IsNumber()) {
                material.SetFloat(paramName, paramData["value"].GetFloat());
            }
            else if (type == "vec2" && paramData.HasMember("value") && paramData["value"].IsArray()) {
                const auto& arr = paramData["value"].GetArray();
                if (arr.Size() >= 2) {
                    material.SetVec2(paramName, Ermine::Vec2(arr[0].GetFloat(), arr[1].GetFloat()));
                }
            }
            else if (type == "vec3" && paramData.HasMember("value") && paramData["value"].IsArray()) {
                const auto& arr = paramData["value"].GetArray();
                if (arr.Size() >= 3) {
                    material.SetVec3(paramName, Ermine::Vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat()));
                }
            }
            else if (type == "vec4" && paramData.HasMember("value") && paramData["value"].IsArray()) {
                const auto& arr = paramData["value"].GetArray();
                if (arr.Size() >= 4) {
                    material.SetVec4(paramName, Ermine::Vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat()));
                }
            }
            else if (type == "int" && paramData.HasMember("value") && paramData["value"].IsInt()) {
                material.SetInt(paramName, paramData["value"].GetInt());
            }
            else if (type == "bool" && paramData.HasMember("value") && paramData["value"].IsBool()) {
                material.SetBool(paramName, paramData["value"].GetBool());
            }
            else if (type == "texture2d" && paramData.HasMember("value") && paramData["value"].IsString()) {
                std::string texPath = paramData["value"].GetString();
                auto tex = Ermine::AssetManager::GetInstance().LoadTexture(texPath);
                if (tex && tex->IsValid()) {
                    material.SetTexture(paramName, tex);
                }
            }
        }
    }

    // Load UV transform
    if (d.HasMember("uvTransform") && d["uvTransform"].IsObject()) {
        const auto& uvObj = d["uvTransform"];
        
        if (uvObj.HasMember("scale") && uvObj["scale"].IsArray()) {
            const auto& scaleArr = uvObj["scale"].GetArray();
            if (scaleArr.Size() >= 2) {
                material.SetUVScale(Ermine::Vec2(scaleArr[0].GetFloat(), scaleArr[1].GetFloat()));
            }
        }
        
        if (uvObj.HasMember("offset") && uvObj["offset"].IsArray()) {
            const auto& offsetArr = uvObj["offset"].GetArray();
            if (offsetArr.Size() >= 2) {
                material.SetUVOffset(Ermine::Vec2(offsetArr[0].GetFloat(), offsetArr[1].GetFloat()));
            }
        }
    }

    if (d.HasMember("customFragmentShader") && d["customFragmentShader"].IsString()) {
        std::string fragPath = d["customFragmentShader"].GetString();
        if (outCustomFragmentShader)
            *outCustomFragmentShader = fragPath;

        if (!fragPath.empty()) {
            auto shader = Ermine::AssetManager::GetInstance().LoadShader(
                "../Resources/Shaders/vertex.glsl",
                fragPath
            );
            if (shader && shader->IsValid()) {
                material.SetShader(shader);
            }
            else {
                EE_CORE_WARN("Failed to load custom fragment shader '{}' from material file", fragPath);
            }
        }
    }

    return material;
}

