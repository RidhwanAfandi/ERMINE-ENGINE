/* Start Header ************************************************************************/
/*!
\file       HierarchyInspector.cpp
\author     Edwin Lee Zirui, edwinzirui.lee, 2301299, edwinzirui.lee\@digipen.edu (25%)
\co-authors WEE HONG RU Curtis, h.wee, 2301266, h.wee\@digipen.edu (65%)
\co-authors Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu (10%)
\date       29/01/2026
\brief      Inspector panel for viewing and editing entity properties

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "HierarchyInspector.h"
#include "HierarchySystem.h"
#include "Components.h"
#include "ECS.h"
#include "GeometryFactory.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Physics.h"
#include "FiniteStateMachine.h"
#include "FSMEditor.h"
#include <EditorGUI.h>
#include "NavMesh.h"
#include "Particles.h"
#include "GPUParticles.h"
#include "AnimationGUI.h"
#include "CommandHistory.h"
#include "GISystem.h"
#include "Renderer.h"
#include "Serialisation.h"

#include "xcore/my_properties.h"
#include "xproperty.h"
#include "sprop/property_sprop.h"
#include "sprop/property_sprop_getset.h"
#include <algorithm> // for std::transform
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace Ermine::editor {
	// --- small helpers ---
	static std::string PrettyLabelFromPath(const std::string& path) {
		// Take the last segment after '/'
		auto slash = path.find_last_of('/');
		std::string s = (slash == std::string::npos) ? path : path.substr(slash + 1);

		std::string out;
		out.reserve(s.size() + 8);
		bool first = true;

		for (size_t i = 0; i < s.size(); ++i) {
			char c = s[i];
			char prev = (out.empty() ? 0 : out.back());

			// Insert space before uppercase if:
			// - not the first character
			// - previous is NOT uppercase
			// - previous is NOT a digit
			if (!first && std::isupper((unsigned char)c) &&
				!(std::isupper((unsigned char)prev) || std::isdigit((unsigned char)prev))) {
				out.push_back(' ');
			}

			// Uppercase first letter
			out.push_back(first ? (char)std::toupper((unsigned char)c) : c);
			first = false;
		}

		return out;
	}

	static bool FieldAppliesToType(const std::string& key, LightType t) {
		if (key == "innerAngle" || key == "outerAngle") return t == LightType::SPOT;
		if (key == "castsShadows") return t != LightType::POINT;
		if (key == "castsRays") return t == LightType::SPOT; // Only show for spotlights
		if (key == "radius") return t == LightType::SPOT || t == LightType::POINT;
		// color, intensity, castsShadows, type are always shown
		return true;
	}

	static Ermine::Material BuildDefaultWhiteMaterialComponent()
	{
		auto& assets = AssetManager::GetInstance();
		const std::filesystem::path defaultMaterialPath =
			std::filesystem::absolute("../Resources/Materials/Default_White.mat");

		if (std::filesystem::exists(defaultMaterialPath)) {
			Guid guid = assets.GetMaterialGuidForPath(defaultMaterialPath.string());
			auto material = assets.LoadMaterialAsset(defaultMaterialPath.string(), false);
			if (material) {
				return Ermine::Material(material, guid);
			}
		}

		// Last-resort fallback if the default asset is missing or fails to load.
		auto fallback = std::make_shared<graphics::Material>();
		fallback->LoadTemplate(graphics::MaterialTemplates::PBR_WHITE());
		auto defaultShader = assets.LoadShader(
			"../Resources/Shaders/vertex.glsl",
			"../Resources/Shaders/fragment_enhanced.glsl"
		);
		if (defaultShader && defaultShader->IsValid())
			fallback->SetShader(defaultShader);

		return Ermine::Material(fallback, {});
	}

	static std::shared_ptr<graphics::Material> BuildMeshMaterialAsset(
		const std::string& meshName,
		const std::string& meshID,
		const aiMaterial* aiMat,
		bool useCacheDefaults,
		Guid& outGuid)
	{
		auto& assets = AssetManager::GetInstance();
		const std::string sanitizedMeshName = AssetManager::SanitizeAssetName(meshName);
		const std::string sanitizedMeshID = AssetManager::SanitizeAssetName(meshID);
		const bool hasMeshName = !sanitizedMeshName.empty();
		const std::string primaryName = hasMeshName ? sanitizedMeshName : sanitizedMeshID;
		const std::filesystem::path materialsDir = std::filesystem::absolute("../Resources/Materials/");

		if (hasMeshName) {
			const std::filesystem::path meshNamePath = materialsDir / (sanitizedMeshName + ".mat");
			if (std::filesystem::exists(meshNamePath)) {
				Guid existingGuid = assets.GetMaterialGuidForPath(meshNamePath.string());
				outGuid = existingGuid;
				auto existingMaterial = assets.LoadMaterialAsset(meshNamePath.string(), false);
				if (existingMaterial)
					return existingMaterial;
			}
		}

		if (!sanitizedMeshID.empty() && primaryName != sanitizedMeshID) {
			const std::filesystem::path meshIdPath = materialsDir / (sanitizedMeshID + ".mat");
			if (std::filesystem::exists(meshIdPath)) {
				Guid existingGuid = assets.GetMaterialGuidForPath(meshIdPath.string());
				outGuid = existingGuid;
				auto existingMaterial = assets.LoadMaterialAsset(meshIdPath.string(), false);
				if (existingMaterial)
					return existingMaterial;
			}
		}

		return assets.CreateMaterialAsset(
			primaryName,
			[&](graphics::Material& material) {
				auto tryLoadTexture = [&](aiTextureType type, const char* slot, const char* hasFlag) {
					aiString texPath;
					if (aiMat->GetTexture(type, 0, &texPath) != AI_SUCCESS)
						return false;

					std::string texPathStr = std::string(texPath.C_Str());
					std::replace(texPathStr.begin(), texPathStr.end(), '\\', '/');
					const auto lastSlash = texPathStr.find_last_of('/');
					const std::string filename = (lastSlash != std::string::npos) ? texPathStr.substr(lastSlash + 1) : texPathStr;
					auto texture = assets.LoadTexture("../Resources/Textures/" + filename);
					if (!texture || !texture->IsValid())
						return false;

					material.SetTexture(slot, texture);
					if (hasFlag)
						material.SetBool(hasFlag, true);
					return true;
				};

				if (!aiMat) {
					material.SetVec4("materialAlbedo", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
					material.SetFloat("materialRoughness", 0.5f);
					material.SetFloat("materialMetallic", 0.0f);
					material.SetFloat("materialAo", 1.0f);
					material.SetVec3("materialEmissive", Vec3(0.0f, 0.0f, 0.0f));
					material.SetFloat("materialEmissiveIntensity", 0.0f);
					material.SetFloat("materialAlpha", 1.0f);
					material.SetFloat("materialTransparency", 0.0f);
					material.SetUVScale(useCacheDefaults ? Vec2(1.0f, -1.0f) : Vec2(1.0f, 1.0f));
					material.SetUVOffset(useCacheDefaults ? Vec2(0.0f, 1.0f) : Vec2(0.0f, 0.0f));
					return;
				}

				// Texture maps
				{
					aiString texPath;
					if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
						aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
						std::string texPathStr = std::string(texPath.C_Str());
						std::replace(texPathStr.begin(), texPathStr.end(), '\\', '/');
						const auto lastSlash = texPathStr.find_last_of('/');
						const std::string filename = (lastSlash != std::string::npos) ? texPathStr.substr(lastSlash + 1) : texPathStr;
						auto albedoTex = assets.LoadTexture("../Resources/Textures/" + filename);
						if (albedoTex && albedoTex->IsValid()) {
							material.SetTexture("materialAlbedoMap", albedoTex);
							material.SetBool("materialHasAlbedoMap", true);
						}
					}
				}
				tryLoadTexture(aiTextureType_NORMALS, "materialNormalMap", "materialHasNormalMap");
				tryLoadTexture(aiTextureType_SHININESS, "materialRoughnessMap", "materialHasRoughnessMap");
				tryLoadTexture(aiTextureType_METALNESS, "materialMetallicMap", "materialHasMetallicMap");
				if (!tryLoadTexture(aiTextureType_AMBIENT_OCCLUSION, "materialAoMap", "materialHasAoMap")) {
					tryLoadTexture(aiTextureType_LIGHTMAP, "materialAoMap", "materialHasAoMap");
				}
				tryLoadTexture(aiTextureType_EMISSIVE, "materialEmissiveMap", "materialHasEmissiveMap");

				// Scalar/color properties
				{
					aiColor4D baseColor(1.f, 1.f, 1.f, 1.f);
					if (aiMat->Get(AI_MATKEY_BASE_COLOR, baseColor) != AI_SUCCESS) {
						aiColor3D diffuse(1.f, 1.f, 1.f);
						if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
							baseColor = aiColor4D(diffuse.r, diffuse.g, diffuse.b, 1.f);
						}
					}

					float opacity = 1.0f;
					if (aiMat->Get(AI_MATKEY_OPACITY, opacity) != AI_SUCCESS)
						opacity = baseColor.a;
					opacity = std::clamp(opacity, 0.0f, 1.0f);
					baseColor.a = opacity;

					material.SetVec4("materialAlbedo", Vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a));
					material.SetFloat("materialAlpha", baseColor.a);
					material.SetFloat("materialTransparency", 1.0f - baseColor.a);
				}

				float metallic = 0.0f;
				if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) != AI_SUCCESS)
					metallic = 0.0f;
				material.SetFloat("materialMetallic", std::clamp(metallic, 0.0f, 1.0f));

				float roughness = 0.5f;
				if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != AI_SUCCESS) {
					float shininess = 32.0f;
					if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
						shininess = std::max(shininess, 0.0f);
						roughness = std::sqrt(2.0f / (shininess + 2.0f));
					}
				}
				material.SetFloat("materialRoughness", std::clamp(roughness, 0.0f, 1.0f));

				float ao = 1.0f;
				aiColor3D ambient(1.f, 1.f, 1.f);
				if (aiMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient) == AI_SUCCESS) {
					ao = std::clamp((ambient.r + ambient.g + ambient.b) / 3.0f, 0.0f, 1.0f);
				}
				material.SetFloat("materialAo", ao);

				aiColor3D emissive(0.f, 0.f, 0.f);
				if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) != AI_SUCCESS)
					emissive = aiColor3D(0.f, 0.f, 0.f);
				material.SetVec3("materialEmissive", Vec3(emissive.r, emissive.g, emissive.b));
				const float emissiveIntensity = std::max({ emissive.r, emissive.g, emissive.b, 0.0f });
				material.SetFloat("materialEmissiveIntensity", emissiveIntensity);

				// UV transform
				aiUVTransform uvTransform;
				if (aiMat->Get(AI_MATKEY_UVTRANSFORM(aiTextureType_DIFFUSE, 0), uvTransform) == AI_SUCCESS) {
					material.SetUVScale(Vec2(uvTransform.mScaling.x, uvTransform.mScaling.y));
					material.SetUVOffset(Vec2(uvTransform.mTranslation.x, uvTransform.mTranslation.y));
				}
				else {
					material.SetUVScale(Vec2(1.0f, 1.0f));
					material.SetUVOffset(Vec2(0.0f, 0.0f));
				}
			},
			&outGuid);
	}

	static std::string BuildMaterialSignature(const graphics::Material& material,
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
				return {}; // redundant with albedo alpha
			return name;
		};

		auto normalizePath = [](std::string path) -> std::string {
			for (char& c : path) {
				if (c == '\\') c = '/';
				else c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return path;
		};

		std::map<std::string, graphics::MaterialParam> normalized;
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

		// UV transform
		Vec2 uvScale = material.GetUVScale();
		Vec2 uvOffset = material.GetUVOffset();
		oss << "uvs=" << uvScale.x << "," << uvScale.y << ";";
		oss << "uvo=" << uvOffset.x << "," << uvOffset.y << ";";

		for (const auto& [name, param] : normalized) {
			oss << "p:" << name << "|";
			switch (param.type) {
			case graphics::MaterialParamType::FLOAT:
				oss << "f:" << (param.floatValues.empty() ? 0.0f : param.floatValues[0]);
				break;
			case graphics::MaterialParamType::VEC2:
				oss << "v2:";
				if (param.floatValues.size() >= 2)
					oss << param.floatValues[0] << "," << param.floatValues[1];
				break;
			case graphics::MaterialParamType::VEC3:
				oss << "v3:";
				if (param.floatValues.size() >= 3)
					oss << param.floatValues[0] << "," << param.floatValues[1] << "," << param.floatValues[2];
				break;
			case graphics::MaterialParamType::VEC4:
				oss << "v4:";
				if (param.floatValues.size() >= 4)
					oss << param.floatValues[0] << "," << param.floatValues[1] << "," << param.floatValues[2] << "," << param.floatValues[3];
				break;
			case graphics::MaterialParamType::INT:
				oss << "i:" << param.intValue;
				break;
			case graphics::MaterialParamType::BOOL:
				oss << "b:" << (param.boolValue ? 1 : 0);
				break;
			case graphics::MaterialParamType::TEXTURE_2D:
				oss << "t:";
				if (param.texture) {
					std::string path =
						AssetManager::GetInstance().ResolveTexturePathForMaterialWrite(param.texture);
					oss << normalizePath(path);
				}
				break;
			}
			oss << ";";
		}

		return oss.str();
	}

	// Scan for available fragment shaders in the Resources/Shaders directory
	static std::vector<std::string> ScanFragmentShaders() {
		std::vector<std::string> shaders;
		const std::string shaderDir = "../Resources/Shaders/";

		try {
			namespace fs = std::filesystem;
			if (fs::exists(shaderDir) && fs::is_directory(shaderDir)) {
				for (const auto& entry : fs::directory_iterator(shaderDir)) {
					if (entry.is_regular_file()) {
						std::string filename = entry.path().filename().string();
						// Look for fragment shaders (contains "fragment" or ends with .frag)
						if (filename.find("fragment") != std::string::npos ||
							filename.find(".frag") != std::string::npos) {
							// Store the full path relative to Resources/Shaders/
							shaders.push_back(shaderDir + filename);
						}
					}
				}
			}
		}
		catch (const std::exception& e) {
			EE_CORE_WARN("Failed to scan fragment shaders: {0}", e.what());
		}

		// Sort alphabetically for consistent UI
		std::sort(shaders.begin(), shaders.end());
		return shaders;
	}

	static std::string ResolveCustomVertexShaderPath(const std::string& fragmentPath) {
		constexpr const char* kDefaultVertex = "../Resources/Shaders/vertex.glsl";
		if (fragmentPath.empty()) {
			return kDefaultVertex;
		}

		std::filesystem::path fragmentFile(fragmentPath);
		std::string fragmentName = fragmentFile.filename().string();
		std::string vertexName = fragmentName;

		const size_t fragmentPos = vertexName.find("fragment");
		if (fragmentPos != std::string::npos) {
			vertexName.replace(fragmentPos, std::string("fragment").size(), "vertex");
		}
		else if (fragmentFile.extension() == ".frag") {
			vertexName = fragmentFile.stem().string() + ".vert";
		}
		else {
			return kDefaultVertex;
		}

		std::filesystem::path vertexPath = fragmentFile.has_parent_path()
			? (fragmentFile.parent_path() / vertexName)
			: (std::filesystem::path("../Resources/Shaders") / vertexName);

		if (std::filesystem::exists(vertexPath)) {
			return vertexPath.generic_string();
		}
		return kDefaultVertex;
	}

	template<typename T>
	static bool ComponentHeaderWithRemove(const char* headerLabel, EntityID entity,
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
	{
		bool open = ImGui::CollapsingHeader(headerLabel, flags);

		// Open context menu when right-clicking the header row
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Remove Component")) {
				auto& ecs = Ermine::ECS::GetInstance();
				ecs.RemoveComponent<T>(entity);
				ImGui::EndPopup();
				return false;
			}
			ImGui::EndPopup();
		}

		if (!open) return false;
		return true;
	}

	// Component header with Remove, Copy, and Paste functionality
	template<typename T>
	static bool ComponentHeaderWithCopyPaste(const char* headerLabel, EntityID entity,
		std::optional<T>& clipboard, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
	{
		bool open = ImGui::CollapsingHeader(headerLabel, flags);

		// Open context menu when right-clicking the header row
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Copy Component")) {
				auto& ecs = Ermine::ECS::GetInstance();
				clipboard = ecs.GetComponent<T>(entity);
			}
			if (ImGui::MenuItem("Paste Component", nullptr, false, clipboard.has_value())) {
				auto& ecs = Ermine::ECS::GetInstance();
				ecs.GetComponent<T>(entity) = clipboard.value();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Remove Component")) {
				auto& ecs = Ermine::ECS::GetInstance();
				ecs.RemoveComponent<T>(entity);
				ImGui::EndPopup();
				return false;
			}
			ImGui::EndPopup();
		}

		if (!open) return false;
		return true;
	}

	static bool ComponentHeaderWithRemoveForScriptsComponent(const char* headerLabel, EntityID entity, const std::string& className,
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen)
	{
		bool open = ImGui::CollapsingHeader(headerLabel, flags);

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Remove Component"))
			{
				auto& scs = ECS::GetInstance().GetComponent<ScriptsComponent>(entity);
				scs.RemoveByClass(className);
				ImGui::EndPopup();
				return false;
			}
			ImGui::EndPopup();
		}

		if (!open) return false;
		return true;
	}

	enum class Vec3AxisMask : uint8_t
	{
		None = 0,
		X = 1 << 0,
		Y = 1 << 1,
		Z = 1 << 2
	};

	static Vec3AxisMask operator|(Vec3AxisMask a, Vec3AxisMask b)
	{
		return static_cast<Vec3AxisMask>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	static Vec3AxisMask& operator|=(Vec3AxisMask& a, Vec3AxisMask b)
	{
		a = a | b;
		return a;
	}

	// Unity-like XYZ control. Returns true if any component changed.
	// - Clicking X/Y/Z button resets that axis to resetValue.
	// - speed/min/max behave like regular DragFloat.
	static bool DrawVec3XYZ(const char* label, float v[3], Vec3AxisMask& outActivatedAxes, Vec3AxisMask& outCommittedAxes, float speed = 0.1f, float resetValue = 0.0f, float minVal = -FLT_MAX, float maxVal = FLT_MAX)
	{
		outActivatedAxes = Vec3AxisMask::None;
		outCommittedAxes = Vec3AxisMask::None;

		ImGui::PushID(label);

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, 140.0f);
		ImGui::TextUnformatted(label);
		ImGui::NextColumn();

		bool changed = false;

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

		const float lineH = ImGui::GetTextLineHeightWithSpacing();
		const ImVec2 btnSize(lineH, lineH);
		const float inner = ImGui::GetStyle().ItemInnerSpacing.x;

		auto axisWidget = [&](const char* id, const char* axisName, float axisColor[3], float& val, Vec3AxisMask axisBit)
			{
				ImGui::PushID(id);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(axisColor[0], axisColor[1], axisColor[2], 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(axisColor[0] + 0.1f, axisColor[1] + 0.1f, axisColor[2] + 0.1f, 1.f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(axisColor[0] + 0.2f, axisColor[1] + 0.2f, axisColor[2] + 0.2f, 1.f));
				if (ImGui::Button(axisName, btnSize)) { val = resetValue; changed = true; outCommittedAxes |= axisBit; }
				ImGui::PopStyleColor(3);

				ImGui::SameLine(0.0f, inner);
				changed |= ImGui::DragFloat("##v", &val, speed, minVal, maxVal, "%.3f");
				if (ImGui::IsItemActivated())
					outActivatedAxes |= axisBit;

				if (ImGui::IsItemDeactivatedAfterEdit())
					outCommittedAxes |= axisBit;
				ImGui::PopItemWidth();
				ImGui::PopID();

				ImGui::SameLine();
			};

		float colX[3] = { 0.85f, 0.35f, 0.35f }; // red-ish
		float colY[3] = { 0.40f, 0.80f, 0.40f }; // green-ish
		float colZ[3] = { 0.35f, 0.55f, 0.90f }; // blue-ish

		axisWidget("X", "X", colX, v[0], Vec3AxisMask::X);
		axisWidget("Y", "Y", colY, v[1], Vec3AxisMask::Y);
		// Last axis on the row: avoid trailing SameLine
		ImGui::PushID("Z");
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(colZ[0], colZ[1], colZ[2], 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(colZ[0] + 0.1f, colZ[1] + 0.1f, colZ[2] + 0.1f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(colZ[0] + 0.2f, colZ[1] + 0.2f, colZ[2] + 0.2f, 1.f));
		if (ImGui::Button("Z", btnSize)) { v[2] = resetValue; changed = true; outCommittedAxes |= Vec3AxisMask::Z; }
		ImGui::PopStyleColor(3);
		ImGui::SameLine(0.0f, inner);
		changed |= ImGui::DragFloat("##v", &v[2], speed, minVal, maxVal, "%.3f");
		if (ImGui::IsItemActivated())
			outActivatedAxes |= Vec3AxisMask::Z;
		if (ImGui::IsItemDeactivatedAfterEdit())
			outCommittedAxes |= Vec3AxisMask::Z;
		ImGui::PopItemWidth();
		ImGui::PopID();

		ImGui::Columns(1);
		ImGui::PopID();

		return changed;
	}

	void HierarchyInspector::OnImGuiRender() {
		if (!m_IsVisible) return;

		ImGui::Begin("Inspector");

		if (!m_ActiveScene) {
			ImGui::Text("No active scene");
			ImGui::End();
			return;
		}

		EntityID selected = m_ActiveScene->GetSelectedEntity(); // Use scene selection
		if (selected == 0) {
			ImGui::Text("No entity selected");
			ImGui::End();
			return;
		}

		// Entity header
		DrawEntityHeader(selected);

		// Draw components with unique IDs
		ImGui::PushID(static_cast<int>(selected));

		if (ECS::GetInstance().HasComponent<Transform>(selected)) {
			DrawTransformComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<Mesh>(selected)) {
			DrawMeshComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<Material>(selected)) {
			DrawMaterialComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<Light>(selected)) {
			DrawLightComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<LightProbeVolumeComponent>(selected)) {
			DrawLightProbeVolumeComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<HierarchyComponent>(selected)) {
			DrawHierarchyComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<PhysicComponent>(selected)) {
			DrawPhysicsComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<AudioComponent>(selected)) {
			DrawAudioComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<ScriptsComponent>(selected)) {
			DrawScriptComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<ModelComponent>(selected)) {
			DrawModelComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<AnimationComponent>(selected)) {
			DrawAnimationComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<StateMachine>(selected)) {
			DrawStateMachineComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<NavMeshComponent>(selected)) {
			DrawNavMeshComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<NavMeshAgent>(selected))
			DrawNavMeshAgentComponent(selected);

		if (ECS::GetInstance().HasComponent<NavJumpLink>(selected))
			DrawNavJumpComponent(selected);

		if (ECS::GetInstance().HasComponent<ParticleEmitter>(selected)) {
			DrawParticleEmitterComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<GPUParticleEmitter>(selected)) {
			DrawGPUParticleEmitterComponent(selected);
		}

		if (ECS::GetInstance().HasComponent<CameraComponent>(selected))
			DrawCameraComponent(selected);

		if (ECS::GetInstance().HasComponent<UIComponent>(selected))
			DrawUIComponent(selected);

		if (ECS::GetInstance().HasComponent<UIHealthbarComponent>(selected))
			DrawUIHealthbarComponent(selected);

		if (ECS::GetInstance().HasComponent<UICrosshairComponent>(selected))
			DrawUICrosshairComponent(selected);

		if (ECS::GetInstance().HasComponent<UISkillsComponent>(selected))
			DrawUISkillsComponent(selected);

		if (ECS::GetInstance().HasComponent<UIManaBarComponent>(selected))
			DrawUIManaBarComponent(selected);

		if (ECS::GetInstance().HasComponent<UIBookCounterComponent>(selected))
			DrawUIBookCounterComponent(selected);

		if (ECS::GetInstance().HasComponent<UIImageComponent>(selected))
			DrawUIImageComponent(selected);

		if (ECS::GetInstance().HasComponent<UIButtonComponent>(selected))
			DrawUIButtonComponent(selected);

		if (ECS::GetInstance().HasComponent<UISliderComponent>(selected))
			DrawUISliderComponent(selected);

		if (ECS::GetInstance().HasComponent<UITextComponent>(selected))
			DrawUITextComponent(selected);

		ImGui::PopID();

		ImGui::Separator();

		// Add Component button
		if (ImGui::Button("Add Component")) {
			ImGui::OpenPopup("AddComponent");
		}

		if (ImGui::BeginPopup("AddComponent")) {
			DrawAddComponentMenu(selected);
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void HierarchyInspector::DrawEntityHeader(EntityID entity) {
		if (!ECS::GetInstance().HasComponent<ObjectMetaData>(entity)) {
			ImGui::Text("Entity ID: %u", entity);
			ImGui::Text("Missing ObjectMetaData component");
			ImGui::Separator();
			return;
		}

		auto& metadata = ECS::GetInstance().GetComponent<ObjectMetaData>(entity);

		// Collect reflective properties
		xproperty::settings::context ctx{};
		xproperty::sprop::container  bag;
		xproperty::sprop::collector  collect(metadata, bag, ctx, /*forEditors=*/true);

		std::string err;

		for (auto& p : bag.m_Properties) {
			const auto guid = p.m_Value.getTypeGuid();
			const char* id = p.m_Path.c_str();                 // unique ID
			std::string label = PrettyLabelFromPath(p.m_Path);    // pretty label (no "ObjectMetaData/")

			ImGui::PushID(id);

			if (guid == xproperty::settings::var_type<std::string>::guid_v) {
				std::string s = p.m_Value.get<std::string>();
				char buf[256]; std::snprintf(buf, sizeof(buf), "%s", s.c_str());
				if (ImGui::InputText(label.c_str(), buf, IM_ARRAYSIZE(buf))) {
					p.m_Value.set<std::string>(buf);
					xproperty::sprop::setProperty(err, metadata, p, ctx);
				}
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v) {
				bool v = p.m_Value.get<bool>();
				if (ImGui::Checkbox(label.c_str(), &v)) {
					p.m_Value.set<bool>(v);
					xproperty::sprop::setProperty(err, metadata, p, ctx);
				}
			}

			ImGui::PopID();
		}

		// Entity ID (read-only)
		ImGui::Text("Entity ID: %u", entity);
		ImGui::Separator();

		if (!err.empty())
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", err.c_str());
	}

	void HierarchyInspector::DrawTransformComponent(EntityID entity) {
		if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& t = ECS::GetInstance().GetComponent<Transform>(entity);
		auto hierarchySystem = ECS::GetInstance().GetSystem<HierarchySystem>();

		xproperty::settings::context ctx{};
		xproperty::sprop::container bag;
		xproperty::sprop::collector collect(t, bag, ctx, /*forEditors=*/true);

		std::string err;

		for (auto& p : bag.m_Properties)
		{
			const auto guid = p.m_Value.getTypeGuid();
			const char* id = p.m_Path.c_str();                  // unique
			std::string label = PrettyLabelFromPath(p.m_Path);     // pretty, no "Transform/"

			ImGui::PushID(id);

			// Vec3 (position / scale)
			if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v) {
				Ermine::Vec3 v = p.m_Value.get<Ermine::Vec3>();

				static std::unordered_map<std::string, Transform> s_DragStart;
				static std::unordered_map<std::string, bool> s_Dragging;

				const std::string& key = p.m_Path;

				//const Transform before = t;
				Vec3AxisMask committedAxes{};
				Vec3AxisMask activatedAxes{};
				const bool changed = DrawVec3XYZ(label.c_str(), &v.x, activatedAxes, committedAxes);

				if (activatedAxes != Vec3AxisMask::None && !s_Dragging[key]) // Capture before on first activation frame
				{
					s_DragStart[key] = t;
					s_Dragging[key] = true;
				}

				if (changed) {
					p.m_Value.set<Ermine::Vec3>({ v.x, v.y, v.z });
					xproperty::sprop::setProperty(err, t, p, ctx);

					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
					hierarchySystem->MarkDirty(entity);
					ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}

				if (committedAxes != Vec3AxisMask::None && s_Dragging[key])
				{
					const Transform before = s_DragStart[key];
					const Transform after = t;
					CommandHistory::GetInstance().Execute(std::make_unique<TransformCommand>(entity, before, t));
					s_Dragging[key] = false;
					EE_CORE_WARN("POI");
				}
			}
			// Quaternion (rotation) – shown/edited as Euler degrees
			else if (guid == xproperty::settings::var_type<Ermine::Quaternion>::guid_v) {
				Ermine::Quaternion q = p.m_Value.get<Ermine::Quaternion>();

				static std::unordered_map<std::string, Transform> s_DragStart;
				static std::unordered_map<std::string, bool> s_Dragging;

				const std::string& key = p.m_Path;

				// Convert to Euler (degrees) for UI
				Ermine::Vec3 eulerDeg = QuaternionToEuler(q, /*degrees*/true);

				// If the field is named "rotation", make the label explicit
				const bool isRotation = (label == "Rotation");
				const char* rotLabel = isRotation ? "Rotation (Degrees)" : label.c_str();

				Vec3AxisMask committedAxes{};
				Vec3AxisMask activatedAxes{};
				const bool changed = DrawVec3XYZ(rotLabel, &eulerDeg.x, activatedAxes, committedAxes, 1.0f, 0.0f, -360.0f, 360.0f);

				if (activatedAxes != Vec3AxisMask::None && !s_Dragging[key]) // Capture before on first activation frame
				{
					s_DragStart[key] = t;
					s_Dragging[key] = true;
				}

				if (changed) {
					p.m_Value.set<Ermine::Quaternion>(FromEulerDegrees(eulerDeg));
					xproperty::sprop::setProperty(err, t, p, ctx);

					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
					hierarchySystem->MarkDirty(entity);
					ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}

				if (committedAxes != Vec3AxisMask::None && s_Dragging[key])
				{
					const Transform before = s_DragStart[key];
					const Transform after = t;
					CommandHistory::GetInstance().Execute(std::make_unique<TransformCommand>(entity, before, t));
					s_Dragging[key] = false;
					EE_CORE_WARN("POI");
				}
			}

			ImGui::PopID();
		}

		if (!err.empty())
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "xprop: %s", err.c_str());
	}

	void HierarchyInspector::DrawMeshComponent(EntityID entity) {
		if (!ComponentHeaderWithRemove<Mesh>("Mesh", entity))
			return;

		auto& mesh = ECS::GetInstance().GetComponent<Mesh>(entity);

		// Combo for mesh kind
		const char* kinds[] = { "None", "Primitive", /*"Asset"*/ };
		int currentKind = static_cast<int>(mesh.kind);
		if (ImGui::Combo("Kind", &currentKind, kinds, IM_ARRAYSIZE(kinds))) {
			mesh.kind = static_cast<MeshKind>(currentKind);
			if (mesh.kind == MeshKind::Primitive)
				mesh.RebuildPrimitive();
			// Mark renderer for full rebuild due to mesh kind change
			ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
		}

		// Primitive controls
		if (mesh.kind == MeshKind::Primitive) {
			// Shape type dropdown - UPDATED TO INCLUDE CONE
			const char* types[] = { "Cube", "Sphere", "Quad", "Cone" };
			int currentType = 0;
			if (mesh.primitive.type == "Sphere") currentType = 1;
			else if (mesh.primitive.type == "Quad") currentType = 2;
			else if (mesh.primitive.type == "Cone") currentType = 3;

			if (ImGui::Combo("Primitive Type", &currentType, types, IM_ARRAYSIZE(types))) {
				mesh.primitive.type = types[currentType];
				mesh.RebuildPrimitive();
				// Mark renderer for full rebuild due to primitive type change
				ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
			}

			// Size control - different for different primitives
			if (mesh.primitive.type == "Cone") {
				// For cone: size.x = diameter (radius * 2), size.y = height
				float diameter = mesh.primitive.size.x;
				float height = mesh.primitive.size.y;

				bool changed = false;
				changed |= ImGui::DragFloat("Diameter", &diameter, 0.1f, 0.01f, 100.f);
				changed |= ImGui::DragFloat("Height", &height, 0.1f, 0.01f, 100.f);

				if (changed) {
					mesh.primitive.size = { diameter, height, diameter };
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
					mesh.RebuildPrimitive();
					ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}
			}
			else if (mesh.primitive.type == "Sphere") {
				// For sphere: size.x = radius
				float radius = mesh.primitive.size.x;
				if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.01f, 100.f)) {
					mesh.primitive.size = { radius, radius, radius };
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
					mesh.RebuildPrimitive();
					ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}
			}
			else {
				// For cube/quad: size = width, height, depth
				float size[3] = { mesh.primitive.size.x, mesh.primitive.size.y, mesh.primitive.size.z };
				if (ImGui::DragFloat3("Size", size, 0.1f, 0.01f, 100.f)) {
					mesh.primitive.size = { size[0], size[1], size[2] };
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
					mesh.RebuildPrimitive();
					ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}
			}
		}

		// Asset controls (basic stub)
		//if (mesh.kind == MeshKind::Asset) {
		//	char buf[256];
		//	strcpy_s(buf, mesh.asset.meshName.c_str());
		//	if (ImGui::InputText("Mesh Name", buf, sizeof(buf))) {
		//		mesh.asset.meshName = buf;
		//		// Mark renderer for full rebuild due to mesh asset change
		//		ECS::GetInstance().GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
		//		// TODO: trigger asset reload here
		//	}
		//}
	}

	void HierarchyInspector::DrawMaterialComponent(EntityID entity) {
		if (!ComponentHeaderWithRemove<Material>("Material", entity))
			return;

		// --- Fetch component & underlying material safely ---
		auto& matComp = ECS::GetInstance().GetComponent<Material>(entity);
		auto& assetManager = AssetManager::GetInstance();

		// Resolve GUID from pointer if possible (legacy/other paths)
		if (!matComp.materialGuid.IsValid() && matComp.m_material) {
			matComp.materialGuid = assetManager.FindMaterialGuid(matComp.m_material.get());
		}

		// Ensure material pointer is loaded from GUID when available
		if (!matComp.m_material && matComp.materialGuid.IsValid()) {
			matComp.m_material = assetManager.GetMaterialByGuid(matComp.materialGuid);
		}

		struct MaterialAssetEntry {
			Guid guid;
			std::string path;
			std::string name;
		};

		static std::vector<MaterialAssetEntry> materialAssets;
		static bool materialAssetsScanned = false;

		auto refreshMaterialAssets = [&]() {
			assetManager.ScanMaterialAssets("../Resources/Materials/", false);
			materialAssets.clear();
			for (const auto& [guid, path] : assetManager.GetMaterialPathsByGuid()) {
				MaterialAssetEntry entry;
				entry.guid = guid;
				entry.path = path;
				entry.name = std::filesystem::path(path).stem().string();
				materialAssets.push_back(std::move(entry));
			}
			std::sort(materialAssets.begin(), materialAssets.end(),
				[](const MaterialAssetEntry& a, const MaterialAssetEntry& b) { return a.name < b.name; });
		};

		if (!materialAssetsScanned) {
			refreshMaterialAssets();
			materialAssetsScanned = true;
		}

		if (matComp.materialGuid.IsValid()) {
			if (const std::string* frag = assetManager.GetMaterialCustomFragmentShader(matComp.materialGuid)) {
				matComp.customFragmentShader = *frag;
			}
		}

		ImGui::SeparatorText("Material Asset");

		std::string currentMaterialLabel = "None";
		if (matComp.materialGuid.IsValid()) {
			auto it = std::find_if(materialAssets.begin(), materialAssets.end(),
				[&](const MaterialAssetEntry& e) { return e.guid == matComp.materialGuid; });
			if (it != materialAssets.end()) {
				currentMaterialLabel = it->name;
			}
			else {
				currentMaterialLabel = "Missing (GUID)";
			}
		}

		if (ImGui::BeginCombo("Material##MaterialAsset", currentMaterialLabel.c_str())) {
			for (const auto& entry : materialAssets) {
				bool isSelected = (entry.guid == matComp.materialGuid);
				if (ImGui::Selectable(entry.name.c_str(), isSelected)) {
					auto newMat = assetManager.GetMaterialByGuid(entry.guid);
					matComp.SetMaterial(newMat, entry.guid);
					if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>()) {
						renderer->MarkMaterialsDirty();
					}
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// Drag & Drop target for Material (.mat files)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE"))
			{
				const char* droppedPathCStr = static_cast<const char*>(payload->Data);
				std::filesystem::path droppedPath = droppedPathCStr;

				// Only accept .mat files
				if (droppedPath.extension() == ".mat")
				{
					auto& assetManager = AssetManager::GetInstance();

					// Normalize / absolute path if needed
					std::string matPath = droppedPath.string();

					// Load or fetch material
					auto material = assetManager.LoadMaterialAsset(matPath, true);
					if (material)
					{
						Guid matGuid = assetManager.GetMaterialGuidForPath(matPath);

						matComp.SetMaterial(material, matGuid);

						// Sync custom fragment shader (if any)
						if (const std::string* frag = assetManager.GetMaterialCustomFragmentShader(matGuid))
							matComp.customFragmentShader = *frag;

						// Mark renderer dirty
						if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>())
							renderer->MarkMaterialsDirty();

						EE_CORE_INFO("Applied dropped material: {}", matPath);
					}
					else
						EE_CORE_WARN("Failed to load dropped material: {}", matPath);
				}
				else
					EE_CORE_INFO("Ignored drop '{}': not a .mat file", droppedPath.string().c_str());
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		if (ImGui::Button("Refresh##MaterialAsset")) {
			refreshMaterialAssets();
		}

		bool flickerEmissive = matComp.flickerEmissive;
		if (ImGui::Checkbox("Flicker Emissive", &flickerEmissive)) {
			matComp.flickerEmissive = flickerEmissive;
			if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>()) {
				renderer->MarkDrawDataForRebuild();
			}
		}

		static char newMaterialName[128] = "NewMaterial";
		ImGui::InputText("New Material##MaterialAsset", newMaterialName, IM_ARRAYSIZE(newMaterialName));
		if (ImGui::Button("Create Material##MaterialAsset")) {
			Guid newGuid{};
			auto newMat = assetManager.CreateMaterialAsset(newMaterialName, nullptr, &newGuid);
			if (newMat) {
				matComp.SetMaterial(newMat, newGuid);
				refreshMaterialAssets();
				if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>()) {
					renderer->MarkMaterialsDirty();
				}
			}
		}

		auto makeUniqueMaterialName = [&](const std::string& base) {
			std::string safeBase = base.empty() ? "Material" : base;
			auto exists = [&](const std::string& name) {
				return std::any_of(materialAssets.begin(), materialAssets.end(),
					[&](const MaterialAssetEntry& e) { return e.name == name; });
			};

			if (!exists(safeBase))
				return safeBase;

			for (int i = 1; i < 1000; ++i) {
				std::string candidate = safeBase + "_Copy";
				if (i > 1)
					candidate += std::to_string(i);
				if (!exists(candidate))
					return candidate;
			}
			return safeBase + "_Copy";
		};

		ImGui::SameLine();
		if (ImGui::Button("Duplicate Material##MaterialAsset")) {
			if (auto gm = matComp.GetMaterial()) {
				std::string baseName = currentMaterialLabel == "None" ? "Material" : currentMaterialLabel;
				std::string newName = makeUniqueMaterialName(baseName);
				Guid newGuid = assetManager.SaveMaterialAsset(newName, *gm, false, matComp.customFragmentShader);
				auto newMat = assetManager.GetMaterialByGuid(newGuid);
				if (newMat) {
					matComp.SetMaterial(newMat, newGuid);
					refreshMaterialAssets();
					if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>()) {
						renderer->MarkMaterialsDirty();
					}
				}
			}
		}

		if (ImGui::Button("Compile All Materials##MaterialAsset")) {
			auto& ecs = ECS::GetInstance();
			assetManager.ScanMaterialAssets("../Resources/Materials/", false);

			struct ExistingMaterialEntry {
				Guid guid;
				std::string name;
			};

			std::unordered_map<std::string, std::vector<ExistingMaterialEntry>> signatureIndex;
			signatureIndex.reserve(assetManager.GetMaterialPathsByGuid().size());
			std::unordered_map<Guid, std::shared_ptr<graphics::Material>> boundByGuid;
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

			auto preferredMaterialName = [&](Ermine::EntityID id) {
				std::string baseName = "Material";
				if (ecs.HasComponent<ObjectMetaData>(id)) {
					const auto& meta = ecs.GetComponent<ObjectMetaData>(id);
					if (!meta.name.empty())
						baseName = meta.name;
					if (baseName.rfind("Mesh_", 0) == 0 && baseName.size() > 5)
						baseName = baseName.substr(5);
				}
				return AssetManager::SanitizeAssetName(baseName);
			};

			for (Ermine::EntityID id = 0; id < Ermine::MAX_ENTITIES; ++id) {
				if (!ecs.IsEntityValid(id) || !ecs.HasComponent<Ermine::Material>(id))
					continue;

				materialEntities.push_back(id);
				auto& comp = ecs.GetComponent<Ermine::Material>(id);
				auto matShared = comp.GetSharedMaterial();
				if (!matShared)
					continue;

				const std::string preferredName = preferredMaterialName(id);
				const std::string currentSig = BuildMaterialSignature(*matShared, comp.customFragmentShader, {});

				Guid resolvedGuid{};
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

			for (Ermine::EntityID id : materialEntities) {
				auto& comp = ecs.GetComponent<Ermine::Material>(id);
				if (!comp.materialGuid.IsValid())
					continue;
				auto it = boundByGuid.find(comp.materialGuid);
				if (it != boundByGuid.end() && it->second)
					comp.SetMaterial(it->second, comp.materialGuid);
			}

			refreshMaterialAssets();
			if (auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>()) {
				renderer->MarkMaterialsDirty();
			}
		}

		graphics::Material* gm = matComp.GetMaterial();
		if (!gm) {
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No material assigned.");
			return;
		}

		// --- Load custom shader if path is set (for scene deserialization) ---
		if (!matComp.customFragmentShader.empty() &&
			(!gm->GetShader() || gm->GetShader() == nullptr)) {
			auto& assetManager = AssetManager::GetInstance();
			const std::string vertexPath = ResolveCustomVertexShaderPath(matComp.customFragmentShader);
			auto customShader = assetManager.LoadShader(
				vertexPath,
				matComp.customFragmentShader
			);
			if (customShader && customShader->IsValid()) {
				gm->SetShader(customShader);
				EE_CORE_INFO("Restored custom shader pair: vertex='{}', fragment='{}'", vertexPath, matComp.customFragmentShader);
			}
			else {
				EE_CORE_WARN("Failed to restore custom fragment shader: {0}", matComp.customFragmentShader);
			}
		}

		// --- Sync cacheCastsShadows to graphics::Material (for scene deserialization) ---
		// Ensure the material's castsShadows parameter matches the serialized component value
		if (auto param = gm->GetParameter("materialCastsShadows"); !param || param->boolValue != matComp.cacheCastsShadows) {
			gm->SetBool("materialCastsShadows", matComp.cacheCastsShadows);
		}

		// --- Safe param accessors (no nullptrs, with fallbacks) ---
		auto getFloat = [&](const char* a, const char* b, float fb) -> float {
			if (auto p = gm->GetParameter(a); p && !p->floatValues.empty()) return p->floatValues[0];
			if (auto q = gm->GetParameter(b); q && !q->floatValues.empty()) return q->floatValues[0];
			return fb;
			};
		auto getVec3 = [&](const char* a, const char* b, const Vec3& fb) -> Vec3 {
			if (auto p = gm->GetParameter(a); p && p->floatValues.size() >= 3)
				return Vec3(p->floatValues[0], p->floatValues[1], p->floatValues[2]);
			if (auto q = gm->GetParameter(b); q && q->floatValues.size() >= 3)
				return Vec3(q->floatValues[0], q->floatValues[1], q->floatValues[2]);
			return fb;
			};
		auto getVec2 = [&](const char* a, const char* b, const Vec2& fb) -> Vec2 {
			if (auto p = gm->GetParameter(a); p && p->floatValues.size() >= 2)
				return Vec2(p->floatValues[0], p->floatValues[1]);
			if (auto q = gm->GetParameter(b); q && q->floatValues.size() >= 2)
				return Vec2(q->floatValues[0], q->floatValues[1]);
			return fb;
			};
		auto getBool = [&](const char* a, const char* b, bool fb) -> bool {
			if (auto p = gm->GetParameter(a)) return p->boolValue;
			if (b) { if (auto q = gm->GetParameter(b)) return q->boolValue; }
			return fb;
			};

		auto setFloatBoth = [&](const char* a, const char* b, float v) {
			gm->SetFloat(a, v); gm->SetFloat(b, v);
			};
		auto setVec2Both = [&](const char* a, const char* b, const Vec2& v) {
			gm->SetVec2(a, v); gm->SetVec2(b, v);
			};
		auto setVec3Both = [&](const char* a, const char* b, const Vec3& v) {
			gm->SetVec3(a, v); gm->SetVec3(b, v);
			};
		auto setBoolBoth = [&](const char* a, const char* b, bool v) {
			gm->SetBool(a, v); if (b) gm->SetBool(b, v);
			};

		auto getAlbedoRGBA = [&]() -> Vec4 {
			// Try main param as vec4
			if (auto p = gm->GetParameter("materialAlbedo"); p && p->floatValues.size() >= 4)
				return Vec4(p->floatValues[0], p->floatValues[1], p->floatValues[2], p->floatValues[3]);

			// Try alias as vec4
			if (auto q = gm->GetParameter("material.albedo"); q && q->floatValues.size() >= 4)
				return Vec4(q->floatValues[0], q->floatValues[1], q->floatValues[2], q->floatValues[3]);

			// Fallback: RGB + (alpha from materialAlpha or 1 - transparency)
			Vec3 rgb = getVec3("materialAlbedo", "material.albedo", matComp.cacheAlbedo);
			float a = 1.0f;
			if (auto aP = gm->GetParameter("materialAlpha"); aP && !aP->floatValues.empty())
				a = aP->floatValues[0];
			else if (auto tP = gm->GetParameter("materialTransparency"); tP && !tP->floatValues.empty())
				a = 1.0f - tP->floatValues[0];

			return Vec4(rgb.x, rgb.y, rgb.z, std::clamp(a, 0.0f, 1.0f));
			};

		auto setAlbedoRGBA = [&](const Vec4& rgba) {
			Vec4 clamped{ std::clamp(rgba.x,0.f,1.f),
						  std::clamp(rgba.y,0.f,1.f),
						  std::clamp(rgba.z,0.f,1.f),
						  std::clamp(rgba.w,0.f,1.f) };
			gm->SetVec4("materialAlbedo", clamped);
			gm->SetVec4("material.albedo", clamped);       // keep alias in lockstep
			gm->SetFloat("materialAlpha", clamped.w);      // mirrors for compat
			gm->SetFloat("materialTransparency", 1.0f - clamped.w);

			// keep component caches updated (Vec3 only)
			matComp.hasAlbedo = true;
			matComp.cacheAlbedo = Vec3(clamped.x, clamped.y, clamped.z);
			};

		// --- Albedo (RGBA) ---
		{
			Vec4 rgba = getAlbedoRGBA();
			float col4[4] = { rgba.x, rgba.y, rgba.z, rgba.w };

			// Edit RGBA (with alpha bar/preview)
			if (ImGui::ColorEdit4("Albedo", col4,
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
				setAlbedoRGBA(Vec4(col4[0], col4[1], col4[2], col4[3]));
			}

			// Optional: separate Transparency (1 - alpha) control for clarity
			float transparency = 1.0f - col4[3];
			if (ImGui::SliderFloat("Transparency", &transparency, 0.0f, 1.0f)) {
				setAlbedoRGBA(Vec4(col4[0], col4[1], col4[2], 1.0f - transparency));
			}
		}

		// --- Metallic ---
		{
			float m = getFloat("materialMetallic", "material.metallic", matComp.cacheMetallic);
			if (ImGui::SliderFloat("Metallic", &m, 0.0f, 1.0f)) {
				matComp.hasMetal = true;
				matComp.cacheMetallic = m;
				setFloatBoth("materialMetallic", "material.metallic", m);
			}
		}

		// --- Roughness ---
		{
			float r = getFloat("materialRoughness", "material.roughness", matComp.cacheRoughness);
			if (ImGui::SliderFloat("Roughness", &r, 0.0f, 1.0f)) {
				matComp.hasRough = true;
				matComp.cacheRoughness = r;
				setFloatBoth("materialRoughness", "material.roughness", r);
			}
		}

		ImGui::SeparatorText("Emissive");

		// --- Emissive color & intensity ---
		{
			Vec3 e = getVec3("materialEmissive", "material.emissive", matComp.cacheEmissive);
			float col[3] = { e.x, e.y, e.z };
			float I = getFloat("materialEmissiveIntensity", "material.emissiveIntensity", matComp.cacheEmissiveIntensity);

			bool chC = ImGui::ColorEdit3("Emissive Color", col);
			bool chI = ImGui::SliderFloat("Emissive Intensity", &I, 0.0f, 10.0f);

			if (chC || chI) {
				e = { col[0], col[1], col[2] };
				matComp.hasEmiss = true;
				matComp.cacheEmissive = e;
				matComp.cacheEmissiveIntensity = I;
				setVec3Both("materialEmissive", "material.emissive", e);
				setFloatBoth("materialEmissiveIntensity", "material.emissiveIntensity", I);
			}
		}

		ImGui::SeparatorText("UV Mapping");

		// --- UV Scale ---
		{
			Vec2 uvScale = gm->GetUVScale();
			float scale[2] = { uvScale.x, uvScale.y };
			if (ImGui::DragFloat2("UV Scale", scale, 0.01f, 0.001f, 100.0f)) {
				gm->SetUVScale(Vec2(scale[0], scale[1]));
			}
		}

		// --- UV Offset ---
		{
			Vec2 uvOffset = gm->GetUVOffset();
			float offset[2] = { uvOffset.x, uvOffset.y };
			if (ImGui::DragFloat2("UV Offset", offset, 0.01f, -10.0f, 10.0f)) {
				gm->SetUVOffset(Vec2(offset[0], offset[1]));
			}
		}

		ImGui::SeparatorText("Rendering");

        // --- Fill Amount / UV Axis ---
        {
            float fill = getFloat("materialFillAmount", nullptr, 1.0f);
            if (ImGui::SliderFloat("Fill", &fill, 0.0f, 1.0f)) {
                gm->SetFloat("materialFillAmount", std::clamp(fill, 0.0f, 1.0f));
            }

            Vec2 fillAxis = getVec2("materialFillUVAxis", nullptr, Vec2(0.0f, 1.0f));
            float axis[2] = { fillAxis.x, fillAxis.y };
            if (ImGui::DragFloat2("Fill UV Axis", axis, 0.01f, -1.0f, 1.0f)) {
                Vec2 uvAxis(axis[0], axis[1]);
                const float lenSq = uvAxis.x * uvAxis.x + uvAxis.y * uvAxis.y;
                if (lenSq < 1e-8f) {
                    uvAxis = Vec2(0.0f, 1.0f);
                }
                gm->SetVec2("materialFillUVAxis", uvAxis);
            }
        }

		// --- Casts Shadows ---
		{
			bool castsShadows = getBool("materialCastsShadows", nullptr, matComp.cacheCastsShadows);
			if (ImGui::Checkbox("Casts Shadows", &castsShadows)) {
				gm->SetBool("materialCastsShadows", castsShadows);
				matComp.cacheCastsShadows = castsShadows;  // Update component cache for serialization
			}
		}

		// --- Custom Fragment Shader ---
		{
			static std::vector<std::string> fragmentShaders; // Cache the shader list
			static bool shadersScanned = false;

			// Scan shaders once per session
			if (!shadersScanned) {
				fragmentShaders = ScanFragmentShaders();
				shadersScanned = true;
			}

			// Get current selection from component
			std::string currentShader = matComp.customFragmentShader;

			// Build display name for combo box
			std::string displayName = currentShader.empty() ? "None (Standard PBR)" : currentShader;
			if (!currentShader.empty()) {
				// Show just the filename for cleaner UI
				size_t lastSlash = currentShader.find_last_of('/');
				if (lastSlash != std::string::npos) {
					displayName = currentShader.substr(lastSlash + 1);
				}
			}

			ImGui::Text("Fragment Shader");
			if (ImGui::BeginCombo("##FragmentShaderCombo", displayName.c_str())) {
				// First option: None (use standard PBR)
				bool isSelected = currentShader.empty();
				if (ImGui::Selectable("None (Standard PBR)", isSelected)) {
					matComp.customFragmentShader = "";
					// Clear custom shader from material
					gm->SetShader(nullptr); // Will revert to standard in renderer
					if (matComp.materialGuid.IsValid()) {
						assetManager.SetMaterialCustomFragmentShader(matComp.materialGuid, {});
						std::string name = assetManager.GetMaterialNameByGuid(matComp.materialGuid);
						if (name.empty())
							name = "Material_" + matComp.materialGuid.ToString();
						assetManager.SaveMaterialAsset(name, *gm, true, {});
					}
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}

				// List all available fragment shaders
				for (const auto& shaderPath : fragmentShaders) {
					// Extract filename for display
					std::string filename = shaderPath;
					size_t lastSlash = shaderPath.find_last_of('/');
					if (lastSlash != std::string::npos) {
						filename = shaderPath.substr(lastSlash + 1);
					}

					bool isShaderSelected = (currentShader == shaderPath);
					if (ImGui::Selectable(filename.c_str(), isShaderSelected)) {
						// Update component cache
						matComp.customFragmentShader = shaderPath;

						// Load and set the custom shader on the material
						auto& assetManager = AssetManager::GetInstance();
						const std::string vertexPath = ResolveCustomVertexShaderPath(shaderPath);
						auto customShader = assetManager.LoadShader(
							vertexPath,
							shaderPath
						);
						if (customShader && customShader->IsValid()) {
							gm->SetShader(customShader);
							EE_CORE_INFO("Loaded custom shader pair: vertex='{}', fragment='{}'", vertexPath, shaderPath);
						}
						else {
							EE_CORE_WARN("Failed to load custom fragment shader: {0}", shaderPath);
						}
						if (matComp.materialGuid.IsValid()) {
							assetManager.SetMaterialCustomFragmentShader(matComp.materialGuid, shaderPath);
							std::string name = assetManager.GetMaterialNameByGuid(matComp.materialGuid);
							if (name.empty())
								name = "Material_" + matComp.materialGuid.ToString();
							assetManager.SaveMaterialAsset(name, *gm, true, shaderPath);
						}
					}
					if (isShaderSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			// Drag & Drop for shader files
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
					const char* droppedPath = static_cast<const char*>(payload->Data);
					std::filesystem::path shaderPath = std::filesystem::path(droppedPath).filename().string();

					if (shaderPath.extension() == ".glsl")
					{
						matComp.customFragmentShader = shaderPath.string();

						auto& assetManager = AssetManager::GetInstance();
						const std::string vertexPath = ResolveCustomVertexShaderPath(matComp.customFragmentShader);
						auto shader = assetManager.LoadShader(
							vertexPath,
							matComp.customFragmentShader
						);

						if (shader && shader->IsValid()) {
							gm->SetShader(shader);
							EE_CORE_INFO("Applied dropped fragment shader: {}", shaderPath.string());
						}
						if (matComp.materialGuid.IsValid()) {
							assetManager.SetMaterialCustomFragmentShader(matComp.materialGuid, matComp.customFragmentShader);
							std::string name = assetManager.GetMaterialNameByGuid(matComp.materialGuid);
							if (name.empty())
								name = "Material_" + matComp.materialGuid.ToString();
							assetManager.SaveMaterialAsset(name, *gm, true, matComp.customFragmentShader);
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::SeparatorText("Maps");

		// --- Presence flags (use same keys as (de)serialize) ---
		bool hasAlbMap = getBool("materialHasAlbedoMap", nullptr, false);
		bool hasNorm = getBool("materialHasNormalMap", "material.hasNormalMap", false);
		bool hasRghMap = getBool("materialHasRoughnessMap", nullptr, false);
		bool hasMetMap = getBool("materialHasMetallicMap", nullptr, false);
		bool hasAoMap = getBool("materialHasAoMap", nullptr, false);
		bool hasEmiMap = getBool("materialHasEmissiveMap", nullptr, false);

		if (ImGui::Checkbox("Albedo Map", &hasAlbMap)) {
			gm->SetBool("materialHasAlbedoMap", hasAlbMap);
		}
		if (ImGui::Checkbox("Normal Map", &hasNorm)) {
			setBoolBoth("materialHasNormalMap", "material.hasNormalMap", hasNorm);
		}
		if (ImGui::Checkbox("Roughness Map", &hasRghMap)) {
			gm->SetBool("materialHasRoughnessMap", hasRghMap);
		}
		if (ImGui::Checkbox("Metallic Map", &hasMetMap)) {
			gm->SetBool("materialHasMetallicMap", hasMetMap);
		}
		if (ImGui::Checkbox("AO Map", &hasAoMap)) {
			gm->SetBool("materialHasAoMap", hasAoMap);
		}
		if (ImGui::Checkbox("Emissive Map", &hasEmiMap)) {
			gm->SetBool("materialHasEmissiveMap", hasEmiMap);
		}

		ImGui::Separator();

		// --- Safe reset button ---
		if (ImGui::SmallButton("Reset to Defaults")) {
			Vec4 alb{ 1.f,1.f,1.f,1.f };
			setAlbedoRGBA(alb);

			setFloatBoth("materialMetallic", "material.metallic", 0.0f);
			setFloatBoth("materialRoughness", "material.roughness", 0.5f);
			Vec3 emi{ 0.f,0.f,0.f }; setVec3Both("materialEmissive", "material.emissive", emi);
			setFloatBoth("materialEmissiveIntensity", "material.emissiveIntensity", 1.0f);

			gm->SetUVScale(Vec2(1.0f, 1.0f));
			gm->SetUVOffset(Vec2(0.0f, 0.0f));

            gm->SetBool("materialCastsShadows", true);
            matComp.cacheCastsShadows = true;
            gm->SetFloat("materialFillAmount", 1.0f);
            gm->SetVec3("materialFillDirection", Vec3(0.0f, 1.0f, 0.0f));
            gm->SetVec2("materialFillUVAxis", Vec2(0.0f, 1.0f));

            gm->SetBool("materialHasAlbedoMap", false);
			setBoolBoth("materialHasNormalMap", "material.hasNormalMap", false);
			gm->SetBool("materialHasRoughnessMap", false);
			gm->SetBool("materialHasMetallicMap", false);
			gm->SetBool("materialHasAoMap", false);
			gm->SetBool("materialHasEmissiveMap", false);

			matComp.hasAlbedo = matComp.hasMetal = matComp.hasRough = matComp.hasEmiss = true;
			matComp.cacheMetallic = 0.0f;
			matComp.cacheRoughness = 0.5f;
			matComp.cacheEmissive = emi;
			matComp.cacheEmissiveIntensity = 1.0f;
		}

		ImGui::SeparatorText("Textures");

		struct SlotRow {
			const char* label;          // UI label
			const char* slot;           // primary slot name in Material
			const char* altSlot;        // optional alias slot (albedo uses this mirror)
			const char* hasFlag;        // "materialHasAlbedoMap", etc.
			const char* hasFlagAlias;   // alt presence flag (normal map alias)
		};

		SlotRow rows[] = {
			{ "Albedo",    "materialAlbedoMap",   "material.albedoMap",   "materialHasAlbedoMap",    nullptr },
			{ "Normal",    "materialNormalMap",   "material.normalMap",    "materialHasNormalMap",   "material.hasNormalMap" },
			{ "Roughness", "materialRoughnessMap",nullptr,                 "materialHasRoughnessMap",nullptr },
			{ "Metallic",  "materialMetallicMap", "material.metallicMap",  "materialHasMetallicMap", nullptr },
			{ "AO",        "materialAoMap",       nullptr,                 "materialHasAoMap",       nullptr },
			{ "Emissive",  "materialEmissiveMap", nullptr,                 "materialHasEmissiveMap", nullptr },
		};

		// Track if material was changed for GPU update
		bool materialChanged = false;

		// Helper to render ONE row with dropdown texture selection and drag-and-drop
		auto showTextureSlotDropTarget = [&](const SlotRow& r)
			{
				// 1. Resolve current texture bound in this slot
				std::shared_ptr<graphics::Texture> curTex = gm->GetTexture(r.slot);

				// 2. Build display name (stable std::string so ImGui can safely use c_str())
				std::string slotLabelStr;
				if (curTex && curTex->IsValid()) {
					std::string fullPath = curTex->GetFilePath(); // e.g. "../Resources/Textures/Wood.png"
					std::filesystem::path p(fullPath);
					slotLabelStr = p.filename().string();         // "Wood.png"
					if (slotLabelStr.empty())
						slotLabelStr = fullPath;
				}
				else {
					slotLabelStr = "<None>";
				}

				ImGui::PushID(r.slot);

				// Label (e.g. "Albedo")
				ImGui::TextUnformatted(r.label);
				ImGui::SameLine();

				// DROPDOWN SELECTION: Build texture list from AssetManager
				auto& assetManager = AssetManager::GetInstance();
				const auto& loadedTextures = assetManager.GetLoadedTextures();

				std::vector<std::pair<std::string, std::shared_ptr<graphics::Texture>>> textureList;
				textureList.emplace_back("<None>", nullptr);

				for (const auto& [path, texture] : loadedTextures) {
					if (texture && texture->IsValid()) {
						std::filesystem::path p(path);
						std::string displayName = p.filename().string();
						if (displayName.empty()) displayName = path;
						textureList.emplace_back(displayName, texture);
					}
				}

				// Combo box for texture selection
				std::string comboLabel = "##TextureCombo_" + std::string(r.slot);
				if (ImGui::BeginCombo(comboLabel.c_str(), slotLabelStr.c_str(), ImGuiComboFlags_None)) {
					for (size_t i = 0; i < textureList.size(); i++) {
						const auto& [name, tex] = textureList[i];
						bool isSelected = false;

						if (i == 0) {
							// "<None>" is selected if current texture is null
							isSelected = (curTex == nullptr || !curTex->IsValid());
						}
						else {
							// Match by texture pointer or filepath
							isSelected = (curTex == tex) ||
								(curTex && tex && curTex->GetFilePath() == tex->GetFilePath());
						}

						if (ImGui::Selectable(name.c_str(), isSelected)) {
							if (tex && tex->IsValid()) {
								// Set texture
								gm->SetTexture(r.slot, tex);
								if (r.altSlot) gm->SetTexture(r.altSlot, tex);

								// Set flags
								if (r.hasFlag) gm->SetBool(r.hasFlag, true);
								if (r.hasFlagAlias) gm->SetBool(r.hasFlagAlias, true);

								// Register texture with renderer and get array index
								auto renderer = ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>();
								int arrayIndex = renderer->RegisterTexture(tex);
								gm->SetTextureArrayIndex(r.slot, arrayIndex);
								if (r.altSlot) gm->SetTextureArrayIndex(r.altSlot, arrayIndex);

								// Mark for GPU update
								renderer->BuildTextureArray();
								materialChanged = true;
							}
							else {
								// Clear texture ("<None>" selected)
								gm->SetTexture(r.slot, nullptr);
								if (r.altSlot) gm->SetTexture(r.altSlot, nullptr);
								if (r.hasFlag) gm->SetBool(r.hasFlag, false);
								if (r.hasFlagAlias) gm->SetBool(r.hasFlagAlias, false);
								gm->SetTextureArrayIndex(r.slot, -1);
								if (r.altSlot) gm->SetTextureArrayIndex(r.altSlot, -1);
								materialChanged = true;
							}
						}

						if (isSelected) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}

				// Hover tooltip with full path
				if (ImGui::IsItemHovered() && curTex && curTex->IsValid()) {
					ImGui::BeginTooltip();
					ImGui::Text("File: %s", curTex->GetFilePath().c_str());
					int arrayIdx = gm->GetTextureArrayIndex(r.slot);
					if (arrayIdx >= 0) {
						ImGui::Text("Array Index: %d", arrayIdx);
					}
					ImGui::EndTooltip();
				}

				// Right-click popup menu to Clear this texture
				if (ImGui::BeginPopupContextItem("TexSlotContext")) {
					if (ImGui::MenuItem("Clear Texture")) {
						gm->SetTexture(r.slot, nullptr);
						if (r.altSlot) gm->SetTexture(r.altSlot, nullptr);
						if (r.hasFlag) gm->SetBool(r.hasFlag, false);
						if (r.hasFlagAlias) gm->SetBool(r.hasFlagAlias, false);
						gm->SetTextureArrayIndex(r.slot, -1);
						if (r.altSlot) gm->SetTextureArrayIndex(r.altSlot, -1);
					}
					ImGui::EndPopup();
				}

				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
						const char* droppedPathCStr = static_cast<const char*>(payload->Data);
						std::filesystem::path droppedPath = droppedPathCStr;

						// Only accept texture-ish files
						std::string ext = droppedPath.extension().string();
						for (auto& c : ext) c = (char)tolower(c);

						bool isTextureFile =
							(ext == ".png" ||
								ext == ".jpg" || ext == ".jpeg" ||
								ext == ".tga" ||
								ext == ".bmp" ||
								ext == ".dds" ||
								ext == ".ktx" ||
								ext == ".hdr");

						if (isTextureFile) {
							std::shared_ptr<graphics::Texture> newTex =
								AssetManager::GetInstance().LoadTexture(droppedPath.string());

							if (newTex && newTex->IsValid()) {
								// Assign to this slot
								gm->SetTexture(r.slot, newTex);
								if (r.altSlot) gm->SetTexture(r.altSlot, newTex);

								// Flip presence flags true
								if (r.hasFlag) gm->SetBool(r.hasFlag, true);
								if (r.hasFlagAlias) gm->SetBool(r.hasFlagAlias, true);

								// Register texture with renderer and get array index
								auto renderer = ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>();
								int arrayIndex = renderer->RegisterTexture(newTex);
								gm->SetTextureArrayIndex(r.slot, arrayIndex);
								if (r.altSlot) gm->SetTextureArrayIndex(r.altSlot, arrayIndex);

								// Mark for GPU update
								renderer->BuildTextureArray();
								materialChanged = true;
							}
							else {
								EE_CORE_WARN("Failed to load dropped texture: {}", droppedPath.string());
							}
						}
						else {
							// Ignore non-texture drops so we don't crash
							EE_CORE_INFO("Ignored drop '%s': not a supported texture format",
								droppedPath.string().c_str());
						}
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::PopID();
			};

		// Draw all rows
		for (const auto& row : rows) {
			showTextureSlotDropTarget(row);
		}

		// Update material SSBO on GPU if textures were changed
		if (materialChanged) {
			auto renderer = ECS::GetInstance().GetSystem<Ermine::graphics::Renderer>();
			if (renderer) {
				uint32_t materialIndex = renderer->GetMaterialIndex(entity);
				auto ssboData = gm->GetSSBOData();
				renderer->UpdateMaterialSSBO(ssboData, materialIndex);
			}
		}
	}

	void HierarchyInspector::DrawLightComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<Light>("Light", entity))
			return;

		auto& light = ECS::GetInstance().GetComponent<Light>(entity);

		xproperty::settings::context ctx{};
		xproperty::sprop::container  bag;
		xproperty::sprop::collector  collect(light, bag, ctx, /*forEditors=*/true);

		std::string err;

		// Optional: section headers
		ImGui::TextDisabled("Common");
		ImGui::Separator();

		for (auto& p : bag.m_Properties)
		{
			const auto guid = p.m_Value.getTypeGuid();
			const std::string key = [&] {
				const auto slash = p.m_Path.find_last_of('/');
				return (slash == std::string::npos) ? p.m_Path : p.m_Path.substr(slash + 1);
				}();

			if (!FieldAppliesToType(key, light.type))
				continue; // hide irrelevant fields

			const std::string labelStr = PrettyLabelFromPath(p.m_Path);
			const char* label = labelStr.c_str();

			ImGui::PushID(p.m_Path.c_str());

			// --- strings (none in Light, but generic kept for future fields) ---
			if (guid == xproperty::settings::var_type<std::string>::guid_v) {
				std::string s = p.m_Value.get<std::string>();
				char buf[256]; std::snprintf(buf, sizeof(buf), "%s", s.c_str());
				if (ImGui::InputText(label, buf, IM_ARRAYSIZE(buf))) {
					p.m_Value.set<std::string>(buf);
					xproperty::sprop::setProperty(err, light, p, ctx);
				}
			}
			// --- booleans ---
			else if (guid == xproperty::settings::var_type<bool>::guid_v) {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox(label, &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, light, p, ctx);
				}
			}
			// --- floats ---
			else if (guid == xproperty::settings::var_type<float>::guid_v) {
				float v = p.m_Value.get<float>();
				if (key == "intensity") {
					if (ImGui::SliderFloat("Intensity", &v, 0.0f, 10.0f)) {
						p.m_Value.set<float>(v);
						xproperty::sprop::setProperty(err, light, p, ctx);
					}
				}
				else if (key == "innerAngle") {
					if (ImGui::DragFloat("Inner Angle (deg)", &v, 0.1f, 0.0f, 180.0f)) {
						p.m_Value.set<float>(v);
						xproperty::sprop::setProperty(err, light, p, ctx);
					}
				}
				else if (key == "outerAngle") {
					if (ImGui::DragFloat("Outer Angle (deg)", &v, 0.1f, 0.0f, 180.0f)) {
						p.m_Value.set<float>(v);
						xproperty::sprop::setProperty(err, light, p, ctx);
					}
				}
				else if (key == "radius") {
					if (ImGui::DragFloat("Radius", &v, 0.01f, 0.0f)) {
						p.m_Value.set<float>(v);
						xproperty::sprop::setProperty(err, light, p, ctx);
					}
				}
				else {
					if (ImGui::DragFloat(label, &v, 0.01f)) {
						p.m_Value.set<float>(v);
						xproperty::sprop::setProperty(err, light, p, ctx);
					}
				}
			}
			// --- Vec3 (color) ---
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v/* || guid == xproperty::settings::var_type<Vec3>::guid_v*/) {
				auto v = (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v)
					? p.m_Value.get<Ermine::Vec3>()
					: Ermine::Vec3{ p.m_Value.get<Vec3>().x, p.m_Value.get<Vec3>().y, p.m_Value.get<Vec3>().z };

				float a[3] = { v.x, v.y, v.z };
				if (key == "color" ? ImGui::ColorEdit3("Color", a) : ImGui::DragFloat3(label, a, 0.01f)) {
					Ermine::Vec3 nv{ a[0], a[1], a[2] };
					p.m_Value.set<Ermine::Vec3>(nv); // set canonical type
					xproperty::sprop::setProperty(err, light, p, ctx);
				}
			}
			// --- enum: LightType ---
			else if (guid == xproperty::settings::var_type<LightType>::guid_v) {
				int idx = static_cast<int>(p.m_Value.get<LightType>());
				const char* names[] = { "Point", "Directional", "Spot" };
				if (ImGui::Combo("Type", &idx, names, IM_ARRAYSIZE(names))) {
					p.m_Value.set<LightType>(static_cast<LightType>(idx));
					xproperty::sprop::setProperty(err, light, p, ctx);
				}
			}
			else {
				// Unknown type: show a read-only stub so you see it's there
				ImGui::TextDisabled("%s (unhandled type)", label);
			}

			ImGui::PopID();
		}

		if (!err.empty())
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "xprop: %s", err.c_str());
	}

	void HierarchyInspector::DrawHierarchyComponent(EntityID entity) {
		if (ImGui::CollapsingHeader("Hierarchy")) {
			auto& ecs = ECS::GetInstance();
			auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);

			if (hierarchy.parent != 0 && hierarchy.parent != HierarchyComponent::INVALID_PARENT) {
				if (ecs.IsEntityValid(hierarchy.parent) && ecs.HasComponent<ObjectMetaData>(hierarchy.parent)) {
					auto& metadata = ecs.GetComponent<ObjectMetaData>(hierarchy.parent);
					ImGui::Text("Parent: %s", metadata.name.c_str());
				}
				else {
					ImGui::Text("Parent: (Invalid Entity %u)", hierarchy.parent);
				}
			}
			else {
				ImGui::Text("Parent: None");
			}

			if (!hierarchy.children.empty()) {
				if (ImGui::TreeNode("Children")) {
					for (auto child : hierarchy.children) {
						if (ecs.IsEntityValid(child) && ecs.HasComponent<ObjectMetaData>(child)) {
							auto& metadata = ecs.GetComponent<ObjectMetaData>(child);
							ImGui::BulletText("%s", metadata.name.c_str());
						}
						else {
							ImGui::BulletText("(Invalid Entity %u)", child);
						}
					}
					ImGui::TreePop();
				}
			}
		}
	}

	void HierarchyInspector::DrawLightProbeVolumeComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<LightProbeVolumeComponent>("Light Probe Volume", entity))
			return;

		auto& ecs = ECS::GetInstance();
		auto& volume = ecs.GetComponent<LightProbeVolumeComponent>(entity);

		ImGui::Checkbox("Active", &volume.isActive);
		ImGui::SliderInt("Capture Resolution", &volume.captureResolution, 16, 512);
		if (volume.captureResolution < 1) volume.captureResolution = 1;
		ImGui::SliderInt("Voxel Resolution", &volume.voxelResolution, 16, 256);
		if (volume.voxelResolution < 1) volume.voxelResolution = 1;
		ImGui::InputInt("Priority", &volume.priority);
		ImGui::DragFloat3("Bounds Min", &volume.boundsMin.x, 0.1f);
		ImGui::DragFloat3("Bounds Max", &volume.boundsMax.x, 0.1f);
		ImGui::Checkbox("Show Gizmos", &volume.showGizmos);
		ImGui::Text("Probe Index: %d", volume.probeIndex);

		if (ImGui::TreeNode("Bake Settings")) {
			auto renderer = ecs.GetSystem<graphics::Renderer>();
			if (renderer) {
				ImGui::SliderInt("GI Bounces", &renderer->m_GIBakeBounces, 1, 8);
				ImGui::SliderFloat("Energy Loss", &renderer->m_GIBakeEnergyLoss, 0.0f, 1.0f);
			}
			ImGui::TreePop();
		}

		if (ImGui::Button("Bake Probe")) {
			auto renderer = ecs.GetSystem<graphics::Renderer>();
			if (renderer) {
				renderer->CaptureLightProbe(entity);
			}
		}
	}

	void HierarchyInspector::DrawPhysicsComponent(EntityID entity)
	{
		bool open = ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen);
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Remove Component")) {
				auto& ecs = ECS::GetInstance();
				ecs.RemoveComponent<PhysicComponent>(entity);
				ecs.GetSystem<Physics>()->UpdatePhysicList();  // keep physics in sync
				ImGui::EndPopup();
				return;
			}
			ImGui::EndPopup();
		}
		if (!open) return;

		auto& pc = ECS::GetInstance().GetComponent<PhysicComponent>(entity);

		xproperty::settings::context ctx{};
		xproperty::sprop::container  bag;
		xproperty::sprop::collector  collect(pc, bag, ctx, /*forEditors=*/true);

		std::string err;

		// Draw reflected scalars/enums only
		for (auto& p : bag.m_Properties)
		{
			const auto guid = p.m_Value.getTypeGuid();
			const char* id = p.m_Path.c_str();
			std::string label = PrettyLabelFromPath(p.m_Path);

			ImGui::PushID(id);

			// Mass (float, clamp >= 0)
			if (guid == xproperty::settings::var_type<float>::guid_v && label == "Mass") {
				float m = p.m_Value.get<float>();
				if (ImGui::DragFloat("Mass", &m, 0.01f, 0.0f)) {
					if (m < 0.0f) m = 0.0f;
					p.m_Value.set<float>(m);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			// PhysicsBodyType
			else if (guid == xproperty::settings::var_type<PhysicsBodyType>::guid_v) {
				int idx = static_cast<int>(p.m_Value.get<PhysicsBodyType>());
				const char* names[] = { "Rigidbody", "Trigger" };
				if (ImGui::Combo("Physics Body Type", &idx, names, IM_ARRAYSIZE(names))) {
					p.m_Value.set<PhysicsBodyType>(static_cast<PhysicsBodyType>(idx));
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			// JPH::EMotionType
			else if (guid == xproperty::settings::var_type<JPH::EMotionType>::guid_v) {
				int idx = static_cast<int>(p.m_Value.get<JPH::EMotionType>());
				const char* names[] = { "Static", "Kinematic", "Dynamic" };
				if (ImGui::Combo("Motion Type", &idx, names, IM_ARRAYSIZE(names))) {
					p.m_Value.set<JPH::EMotionType>(static_cast<JPH::EMotionType>(idx));
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			// ShapeType
			else if (guid == xproperty::settings::var_type<ShapeType>::guid_v) {
				int idx = static_cast<int>(p.m_Value.get<ShapeType>());
				const char* names[] = { "Box", "Sphere", "Capsule", "CustomMesh" };
				if (ImGui::Combo("Shape Type", &idx, names, (int)ShapeType::Total)) {
					p.m_Value.set<ShapeType>(static_cast<ShapeType>(idx));
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v && label == "Colliderpivot")
			{
				Ermine::Vec3 v = p.m_Value.get<Ermine::Vec3>();
				float arr[3] = { v.x, v.y, v.z };

				if (ImGui::DragFloat3("Collider Pos", arr, 0.05f, -FLT_MAX, FLT_MAX))
				{
					v.x = arr[0];
					v.y = arr[1];
					v.z = arr[2];
					p.m_Value.set<Ermine::Vec3>(v);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					// Rebuild physics body with updated size
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v && label == "Colliderrot")
			{
				Ermine::Vec3 v = p.m_Value.get<Ermine::Vec3>();
				float arr[3] = { v.x, v.y, v.z };

				if (ImGui::DragFloat3("Collider Rot", arr, 0.05f, -FLT_MAX, FLT_MAX))
				{
					v.x = arr[0];
					v.y = arr[1];
					v.z = arr[2];
					p.m_Value.set<Ermine::Vec3>(v);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					// Rebuild physics body with updated size
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			else if (guid == xproperty::settings::var_type<Ermine::Vec3>::guid_v && label == "Collidersize")
			{
				Ermine::Vec3 v = p.m_Value.get<Ermine::Vec3>();
				float arr[3] = { v.x, v.y, v.z };

				if (ImGui::DragFloat3("Collider Size", arr, 0.05f, 0.0f, 100.0f))
				{
					v.x = arr[0];
					v.y = arr[1];
					v.z = arr[2];
					p.m_Value.set<Ermine::Vec3>(v);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					// Rebuild physics body with updated size
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Posx") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Freeze Pos X", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
				ImGui::SameLine();
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Posy") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Y", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
				ImGui::SameLine();
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Posz") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Z", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Rotx") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Freeze Rot X", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
				ImGui::SameLine();
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Roty") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Y", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}
				ImGui::SameLine();
			}
			else if (guid == xproperty::settings::var_type<bool>::guid_v && label == "Rotz") {
				bool b = p.m_Value.get<bool>();
				if (ImGui::Checkbox("Z", &b)) {
					p.m_Value.set<bool>(b);
					xproperty::sprop::setProperty(err, pc, p, ctx);
					pc.update = true;
					ECS::GetInstance().GetSystem<Physics>()->UpdatePhysicList();
				}

			}

			ImGui::PopID();
		}

		if (!err.empty())
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "xprop: %s", err.c_str());

		ImGui::Separator();
	}

	void HierarchyInspector::DrawAudioComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<AudioComponent>("Audio", entity))
			return;

		auto& audio = ECS::GetInstance().GetComponent<AudioComponent>(entity);

		// Collect reflective properties
		xproperty::settings::context ctx{};
		xproperty::sprop::container  bag;
		xproperty::sprop::collector  collect(audio, bag, ctx, true);

		std::string err;

		for (auto& p : bag.m_Properties)
		{
			const auto guid = p.m_Value.getTypeGuid();
			const char* id = p.m_Path.c_str();
			std::string label = PrettyLabelFromPath(p.m_Path);

			ImGui::PushID(id);

			// string fields (USED FOR AUDIO PATH)
			if (guid == xproperty::settings::var_type<std::string>::guid_v) {
				std::string s = p.m_Value.get<std::string>();
				char buf[256];
				std::snprintf(buf, sizeof(buf), "%s", s.c_str());

				ImGui::InputText(label.c_str(), buf, IM_ARRAYSIZE(buf));

				// --- Drag & Drop audio ---
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
						const char* droppedPath = static_cast<const char*>(payload->Data);
						p.m_Value.set<std::string>(droppedPath);
						xproperty::sprop::setProperty(err, audio, p, ctx);
					}
					ImGui::EndDragDropTarget();
				}

				if (strcmp(buf, s.c_str()) != 0) {
					p.m_Value.set<std::string>(buf);
					xproperty::sprop::setProperty(err, audio, p, ctx);
				}
			}
			// bool fields
			else if (guid == xproperty::settings::var_type<bool>::guid_v) {
				bool v = p.m_Value.get<bool>();
				if (ImGui::Checkbox(label.c_str(), &v)) {
					p.m_Value.set<bool>(v);
					xproperty::sprop::setProperty(err, audio, p, ctx);
				}
			}
			// float fields
			else if (guid == xproperty::settings::var_type<float>::guid_v) {
				float v = p.m_Value.get<float>();
				if (ImGui::DragFloat(label.c_str(), &v, 0.01f, 0.0f, 1.0f)) {
					p.m_Value.set<float>(v);
					xproperty::sprop::setProperty(err, audio, p, ctx);
				}
			}
			// int fields
			else if (guid == xproperty::settings::var_type<int>::guid_v) {
				int v = p.m_Value.get<int>();
				if (ImGui::DragInt(label.c_str(), &v)) {
					p.m_Value.set<int>(v);
					xproperty::sprop::setProperty(err, audio, p, ctx);
				}
			}

			ImGui::PopID();
		}

		ImGui::Separator();

		// Optional quick preview buttons
		if (ImGui::Button("Play")) {
			// TODO: AudioSystem::Get().Play(audio.soundName, entity);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			// TODO: AudioSystem::Get().Stop(entity);
		}

		if (!err.empty())
			ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Error: %s", err.c_str());
	}

	void HierarchyInspector::DrawScriptComponent(EntityID entity)
	{
		/*auto& script = ECS::GetInstance().GetComponent<Script>(entity);*/
		auto& scs = ECS::GetInstance().GetComponent<ScriptsComponent>(entity);

		// Helper: property row with label on the left and widget on the right
		auto PropertyRow = [&](const char* label, auto&& widgetFn)
			{
				ImGui::Columns(2, nullptr, false);
				ImGui::SetColumnWidth(0, 140.0f);
				ImGui::TextUnformatted(label);
				ImGui::NextColumn();
				widgetFn();
				ImGui::Columns(1);
			};

		if (scs.scripts.empty())
			return;

		for (size_t i = 0; i < scs.scripts.size(); ++i)
		{
			auto& script = scs.scripts[i];

			ImGui::PushID(static_cast<int>(i));

			// Unique header label per script (also doubles as unique ImGui ID)
			std::string visibleName = script.m_className.empty() ? std::string("<Unset>") : script.m_className;
			std::string headerLabel = "Script: " + visibleName + "##ScriptHeader_" + std::to_string(i);

			// Collapsing header with context popup that removes THIS script by class name
			if (!ComponentHeaderWithRemoveForScriptsComponent(headerLabel.c_str(), entity, script.m_className))
			{
				ImGui::PopID();
				continue; // removed or closed
			}

			ImGui::Checkbox("Enabled", &script.m_enabled);

			// Editable class name (label left)
			// TODO: Change to a drop-down of available scripts in project
			{
				char buf[256];
				strcpy_s(buf, script.m_className.c_str());
				PropertyRow("Class", [&] {
					if (ImGui::InputText("##Class", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) { // TODO: validate classname?
						script = Script{ std::string(buf), entity }; // Reconstruct a new state to initialize the Script
						scs.AttachAll(entity);
					}
					});
			}

			// Display fields
			std::unordered_map<std::string, ScriptFieldValue> fields;
			if (script.m_instance && script.m_instance->GetManaged())
				scripting::ScriptEngine::PullManagedFieldsToCache(script.m_instance->GetManaged(), fields);

			// Draw each exposed field
			for (auto& [name, val] : fields)
			{
				ImGui::PushID(name.c_str());
				bool changed = false;

				switch (val.kind)
				{
				case ScriptFieldValue::Kind::Float:
				{
					PropertyRow(name.c_str(), [&] {
						ImGui::InputFloat("##v", &std::get<float>(val.value));
						changed = true;
						});
					break;
				}
				case ScriptFieldValue::Kind::Int:
				{
					PropertyRow(name.c_str(), [&] {
						ImGui::InputInt("##v", &std::get<int>(val.value));
						changed = true;
						});
					break;
				}
				case ScriptFieldValue::Kind::Bool:
				{
					PropertyRow(name.c_str(), [&] {
						ImGui::Checkbox("##v", &std::get<bool>(val.value));
						changed = true;
						});
					break;
				}
				case ScriptFieldValue::Kind::Vector3:
				{
					// Uses internal two-column layout with label left
					Vec3AxisMask committedAxis{  };
					Vec3AxisMask activatedAxis{  };
					if (changed == DrawVec3XYZ(name.c_str(), &std::get<Vec3>(val.value).x, activatedAxis, committedAxis))
					{
					}
					break;
				}
				case ScriptFieldValue::Kind::Quaternion:
				{
					// Uses internal two-column layout with label left
					Vec3 euler = QuaternionToEuler(std::get<Quaternion>(val.value), true);
					Vec3AxisMask committedAxis{};
					Vec3AxisMask activatedAxis{  };
					if (changed == DrawVec3XYZ(name.c_str(), &euler.x, activatedAxis, committedAxis))
					{
					}
					val.value = FromEulerDegrees(euler);
					break;
				}
				case ScriptFieldValue::Kind::String:
				{
					char innerBuff[256];
					strcpy_s(innerBuff, std::get<std::string>(val.value).c_str());
					PropertyRow(name.c_str(), [&] {
						if (ImGui::InputText("##v", innerBuff, sizeof(innerBuff), ImGuiInputTextFlags_EnterReturnsTrue))
						{
							// nothing else to do here; value is set when pushing back to managed fields
							changed = true;
						}
						});
					val.value = innerBuff;
					break;
				}
				default:
					EE_CORE_WARN("CURTIS SAYS WHY WASTE TIME SAY LOT WORD WHEN FEW WORD DO TRICKS {}.", name);
					break;
				}

				ImGui::PopID();

				if (changed && script.m_instance && script.m_instance->GetManaged())
				{
					ECS::GetInstance().GetSystem<scripting::ScriptSystem>()->m_ScriptEngine->PushSingleField(script.m_instance->GetManaged(), name, val);
					// Mark dirty for scene save?
				}
			}
			// Push change to managed object
			if (script.m_instance && script.m_instance->GetManaged())
			{
				scripting::ScriptEngine::PushCacheToManagedFields(script.m_instance->GetManaged(), fields);
				scripting::ScriptEngine::PullManagedFieldsToCache(script.m_instance->GetManaged(), script.m_fields);
			}

			ImGui::PopID();
		}
	}

	void HierarchyInspector::DrawModelComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<ModelComponent>("Model", entity))
			return;

		auto& modelComp = ECS::GetInstance().GetComponent<ModelComponent>(entity);

		// --- Scan available models from ../Resources/Models/ ---
		static std::vector<std::string> availableModels;
		static bool initialized = false;
		static const std::string modelsDir = "../Resources/Models/";

		if (!initialized) {
			availableModels.clear();
			for (auto& entry : std::filesystem::recursive_directory_iterator(modelsDir)) {
				if (entry.is_regular_file()) {
					std::string ext = entry.path().extension().string();
					// Added .skin and .mesh to the filter
					if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
						ext == ".skin" || ext == ".mesh") {
						// Store relative path from modelsDir with forward slashes
						std::string relativePath = std::filesystem::relative(entry.path(), modelsDir).string();
						// Normalize path separators to forward slashes
						std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
						availableModels.push_back(relativePath);
					}
				}
			}
			std::sort(availableModels.begin(), availableModels.end());
			initialized = true;
		}

		// --- Current model name ---
		std::string currentName = (modelComp.m_model ? modelComp.m_model->GetName() : "<None>");
		if (currentName.empty()) currentName = "<None>";

		// --- Dropdown menu ---
		static int selectedModel = -1;
		if (modelComp.m_model) {
			auto it = std::find(availableModels.begin(), availableModels.end(), modelComp.m_model->GetName());
			if (it != availableModels.end())
				selectedModel = (int)std::distance(availableModels.begin(), it);
		}

		if (ImGui::BeginCombo("Model File", currentName.c_str())) {
			for (int i = 0; i < (int)availableModels.size(); ++i) {
				bool isSelected = (i == selectedModel);
				if (ImGui::Selectable(availableModels[i].c_str(), isSelected)) {
					selectedModel = i;
					std::string fullPath = modelsDir + availableModels[i];

					auto& ecs = ECS::GetInstance();
					auto hierarchySystem = ecs.GetSystem<HierarchySystem>();

					// Store existing children to reuse them
					std::vector<EntityID> existingChildren;
					if (hierarchySystem && ecs.HasComponent<HierarchyComponent>(entity)) {
						auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
						existingChildren = hierarchy.children;
						EE_CORE_INFO("Reusing {} existing child entities for reload", existingChildren.size());
					}

					// Remove cached version (forces reload from disk)
					auto& manager = AssetManager::GetInstance();
					manager.UnloadModel(fullPath);

					// Load fresh copy from disk
					auto reloaded = manager.LoadModel(fullPath);
					if (reloaded) {
						modelComp.m_model = reloaded;

						// Reuse or create child entities for each mesh with a material
						auto renderer = ecs.GetSystem<graphics::Renderer>();
						const aiScene* scene = reloaded->GetAssimpScene(); // May be nullptr for cache files

						if (hierarchySystem && renderer && !reloaded->GetMeshes().empty()) {
							const auto& meshes = reloaded->GetMeshes();

							for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
								const auto& meshData = meshes[meshIndex];
								const std::string& meshID = meshData.meshID;

								// Get material index from aiScene (if available)
								uint32_t matIndex = 0;
								bool hasSceneMaterial = false;
								std::string meshName = meshID;

								if (scene && meshIndex < scene->mNumMeshes) {
									aiMesh* aiMsh = scene->mMeshes[meshIndex];
									if (aiMsh) {
										if (aiMsh->mName.length > 0) {
											meshName = aiMsh->mName.C_Str();
										}
										if (aiMsh->mMaterialIndex < scene->mNumMaterials) {
											matIndex = aiMsh->mMaterialIndex;
											hasSceneMaterial = true;
										}
									}
								}

								// Reuse existing child entity if available, otherwise create new one
								EntityID childEntity;
								if (meshIndex < existingChildren.size()) {
									childEntity = existingChildren[meshIndex];
									EE_CORE_INFO("Reusing child entity {} for mesh {}", childEntity, meshID);
								}
								else {
									childEntity = ecs.CreateEntity();
									EE_CORE_INFO("Creating new child entity {} for mesh {}", childEntity, meshID);
								}

								// Reset/add required components
								if (!ecs.HasComponent<HierarchyComponent>(childEntity)) {
									ecs.AddComponent<HierarchyComponent>(childEntity, HierarchyComponent());
								}
								else {
									auto& hc = ecs.GetComponent<HierarchyComponent>(childEntity);
									hc.parent = entity;
									hc.children.clear();
									hc.depth = 1;
									hc.isDirty = true;
								}

								if (!ecs.HasComponent<Transform>(childEntity))
									ecs.AddComponent<Transform>(childEntity, Transform());
								else {
									ecs.GetComponent<Transform>(childEntity) = Transform();
								}

								if (!ecs.HasComponent<ObjectMetaData>(childEntity)) {
									ecs.AddComponent<ObjectMetaData>(childEntity,
										ObjectMetaData("Mesh_" + meshID, "Mesh", true));
								}
								else {
									ecs.GetComponent<ObjectMetaData>(childEntity) =
										ObjectMetaData("Mesh_" + meshID, "Mesh", true);
								}

								// Only set parent if this is a newly created child
								if (meshIndex >= existingChildren.size()) {
									hierarchySystem->SetParent(childEntity, entity, true);
								}

								// Create or load a shared material asset (mesh-name based)
								Guid materialGuid{};
								auto materialPtr = BuildMeshMaterialAsset(
									meshName,
									meshID,
									hasSceneMaterial ? scene->mMaterials[matIndex] : nullptr,
									!hasSceneMaterial,
									materialGuid
								);
								if (!materialPtr) {
									EE_CORE_WARN("Failed to create/load material asset for mesh '{}'", meshID);
									continue;
								}

								// Reset or add material component
								if (ecs.HasComponent<Ermine::Material>(childEntity)) {
									auto& matComp = ecs.GetComponent<Ermine::Material>(childEntity);
									matComp = Ermine::Material(materialPtr, materialGuid);
									EE_CORE_INFO("Reset material on child entity {}", childEntity);
								}
								else {
									ecs.AddComponent<Ermine::Material>(childEntity, Ermine::Material(materialPtr, materialGuid));
									EE_CORE_INFO("Added material to child entity {}", childEntity);
								}
							}

							// Delete any excess children that are no longer needed
							if (meshes.size() < existingChildren.size()) {
								EE_CORE_INFO("Deleting {} excess child entities", existingChildren.size() - meshes.size());
								for (size_t i = meshes.size(); i < existingChildren.size(); ++i) {
									EntityID excessChild = existingChildren[i];
									EE_CORE_INFO("Deleting excess child entity {}", excessChild);
									hierarchySystem->UnsetParent(excessChild);
									ecs.DestroyEntity(excessChild);
								}
							}
						}

						// Refresh animator (only if scene has animations)
						if (ecs.HasComponent<AnimationComponent>(entity)) {
							auto& animComp = ecs.GetComponent<AnimationComponent>(entity);
							if (scene && scene->mNumAnimations > 0)
								animComp.m_animator = std::make_shared<graphics::Animator>(reloaded);
							else
								animComp.m_animator.reset();
						}

						// Mark renderer for full rebuild due to model reload
						ecs.GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();

						EE_CORE_INFO("Model reloaded successfully");
					}
					else {
						EE_CORE_ERROR("Failed to reload model from: {}", fullPath);
					}
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// --- Drag & Drop Target ---
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
			
				// Handle dropped model file
				const char* modelPath = (const char*)payload->Data;
				std::string filename = std::filesystem::path(modelPath).filename().string();
				std::string fullPath = modelsDir + filename;

				auto& ecs = ECS::GetInstance();
				auto hierarchySystem = ecs.GetSystem<HierarchySystem>();

				// Store existing children to reuse them
				std::vector<EntityID> existingChildren;
				if (hierarchySystem && ecs.HasComponent<HierarchyComponent>(entity)) {
					auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
					existingChildren = hierarchy.children;
				}

				// Remove cached version (forces reload from disk)
				auto& manager = AssetManager::GetInstance();
				manager.UnloadModel(fullPath);

				// Load fresh copy from disk
				auto reloaded = manager.LoadModel(fullPath);
				if (reloaded) {
					modelComp.m_model = reloaded;

					// Update dropdown selection
					auto it = std::find(availableModels.begin(), availableModels.end(), modelComp.m_model->GetName());
					selectedModel = (it != availableModels.end()) ? (int)std::distance(availableModels.begin(), it) : -1;

					// Reuse or create child entities for each mesh with a material
					auto renderer = ecs.GetSystem<graphics::Renderer>();
					const aiScene* scene = reloaded->GetAssimpScene(); // May be nullptr for cache files

					if (hierarchySystem && renderer && !reloaded->GetMeshes().empty()) {
						const auto& meshes = reloaded->GetMeshes();

						for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
							const auto& meshData = meshes[meshIndex];
							const std::string& meshID = meshData.meshID;

							// Get material index from aiScene (if available)
							uint32_t matIndex = 0;
							bool hasSceneMaterial = false;
							std::string meshName = meshID;

							if (scene && meshIndex < scene->mNumMeshes) {
								aiMesh* aiMsh = scene->mMeshes[meshIndex];
								if (aiMsh) {
									if (aiMsh->mName.length > 0) {
										meshName = aiMsh->mName.C_Str();
									}
									if (aiMsh->mMaterialIndex < scene->mNumMaterials) {
										matIndex = aiMsh->mMaterialIndex;
										hasSceneMaterial = true;
									}
								}
							}

							// Reuse existing child entity if available, otherwise create new one
							EntityID childEntity;
							if (meshIndex < existingChildren.size()) {
								childEntity = existingChildren[meshIndex];
								EE_CORE_INFO("Reusing child entity {} for mesh {}", childEntity, meshID);
							}
							else {
								childEntity = ecs.CreateEntity();
								EE_CORE_INFO("Creating new child entity {} for mesh {}", childEntity, meshID);
							}

							// Reset/add required components
							if (!ecs.HasComponent<HierarchyComponent>(childEntity)) {
								ecs.AddComponent<HierarchyComponent>(childEntity, HierarchyComponent());
							}
							else {
								auto& hc = ecs.GetComponent<HierarchyComponent>(childEntity);
								hc.parent = entity;
								hc.children.clear();
								hc.depth = 1;
								hc.isDirty = true;
							}

							if (!ecs.HasComponent<Transform>(childEntity))
								ecs.AddComponent<Transform>(childEntity, Transform());
							else {
								ecs.GetComponent<Transform>(childEntity) = Transform();
							}

							if (!ecs.HasComponent<ObjectMetaData>(childEntity)) {
								ecs.AddComponent<ObjectMetaData>(childEntity,
									ObjectMetaData("Mesh_" + meshID, "Mesh", true));
							}
							else {
								ecs.GetComponent<ObjectMetaData>(childEntity) =
									ObjectMetaData("Mesh_" + meshID, "Mesh", true);
							}

							// Only set parent if this is a newly created child
							if (meshIndex >= existingChildren.size()) {
								hierarchySystem->SetParent(childEntity, entity, true);
							}

							// Create or load a shared material asset (mesh-name based)
							Guid materialGuid{};
							auto materialPtr = BuildMeshMaterialAsset(
								meshName,
								meshID,
								hasSceneMaterial ? scene->mMaterials[matIndex] : nullptr,
								!hasSceneMaterial,
								materialGuid
							);
							if (!materialPtr) {
								EE_CORE_WARN("Failed to create/load material asset for mesh '{}'", meshID);
								continue;
							}

							// Reset or add material component
							if (ecs.HasComponent<Ermine::Material>(childEntity)) {
								auto& matComp = ecs.GetComponent<Ermine::Material>(childEntity);
								matComp = Ermine::Material(materialPtr, materialGuid);
								EE_CORE_INFO("Reset material on child entity {}", childEntity);
							}
							else {
								ecs.AddComponent<Ermine::Material>(childEntity, Ermine::Material(materialPtr, materialGuid));
								EE_CORE_INFO("Added material to child entity {}", childEntity);
							}
						}

						// Delete any excess children that are no longer needed
						if (meshes.size() < existingChildren.size()) {
							EE_CORE_INFO("Deleting {} excess child entities", existingChildren.size() - meshes.size());
							for (size_t i = meshes.size(); i < existingChildren.size(); ++i) {
								EntityID excessChild = existingChildren[i];
								EE_CORE_INFO("Deleting excess child entity {}", excessChild);
								hierarchySystem->UnsetParent(excessChild);
								ecs.DestroyEntity(excessChild);
							}
						}
					}

					// Refresh animator (only if scene has animations)
					if (ecs.HasComponent<AnimationComponent>(entity)) {
						auto& animComp = ecs.GetComponent<AnimationComponent>(entity);
						if (scene && scene->mNumAnimations > 0)
							animComp.m_animator = std::make_shared<graphics::Animator>(reloaded);
						else
							animComp.m_animator.reset();
					}

					ecs.GetSystem<graphics::Renderer>()->MarkDrawDataForRebuild();
				}
			}
			ImGui::EndDragDropTarget();
		}

		// --- Info about the model ---
		if (modelComp.m_model) {
			auto& model = modelComp.m_model;
			ImGui::Text("Name: %s", model->GetName().c_str());
			ImGui::Text("Meshes: %d", (int)model->GetMeshes().size());
			ImGui::Text("Bones: %d", model->GetBoneCount());
		}
	}

	void HierarchyInspector::DrawAnimationComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<AnimationComponent>("Animation", entity))
			return;

		auto& animComp = ECS::GetInstance().GetComponent<AnimationComponent>(entity);

		if (!animComp.m_animator) {
			ImGui::TextUnformatted("No animations available (model has no animations).");
			return;
		}

		auto& animator = animComp.m_animator;
		const auto& clips = animator->GetClips();

		if (!clips.empty()) {
			static int selectedClip = 0;
			std::vector<const char*> names;
			names.reserve(clips.size());
			for (auto& c : clips) names.push_back(c.name.c_str());

			if (ImGui::Combo("Active Clip", &selectedClip, names.data(), (int)names.size())) {
				animator->PlayAnimation(selectedClip, animator->IsLooping());
			}

			if (ImGui::Button("Play")) animator->PlayAnimation(selectedClip, animator->IsLooping());
			ImGui::SameLine();
			if (ImGui::Button("Pause")) animator->PauseAnimation();
			ImGui::SameLine();
			if (ImGui::Button("Resume")) animator->ResumeAnimation();
			ImGui::SameLine();
			if (ImGui::Button("Stop")) animator->StopAnimation();
			ImGui::SameLine();
			bool looping = animator->IsLooping();
			if (ImGui::Checkbox("Looping", &looping)) animator->IsLooping() = looping;
		}
		else
			ImGui::TextUnformatted("No animation clips found in this model.");

		// Reload Animator Button
		if (ImGui::Button("Reload Animation")) {
			auto model = animator->GetModel();
			if (model) {
				const aiScene* scene = model->GetAssimpScene();
				if (scene && scene->mNumAnimations > 0) {
					animComp.m_animator = std::make_shared<graphics::Animator>(model);
					ImGui::TextUnformatted("Animation reloaded successfully.");
				}
				else {
					animComp.m_animator.reset();
					ImGui::TextUnformatted("No animations found in this model.");
				}
			}
		}

		// Open Animation Editor Button
		ImGui::SameLine();
		if (ImGui::Button("Open Editor")) {
			auto animationWindow = editor::EditorGUI::GetWindow<AnimationEditorImGUI>();
			if (animationWindow) {
				animationWindow->SetSelectedEntity(entity);
				editor::EditorGUI::FocusWindow("Animation Editor");
			}
		}
	}

	void HierarchyInspector::DrawStateMachineComponent(EntityID entity)
	{
		//if (!ImGui::CollapsingHeader("State Machine", ImGuiTreeNodeFlags_DefaultOpen))
		//	return;

		if (!ComponentHeaderWithRemove<StateMachine>("State Machine", entity))
			return;

		auto& fsmComp = ECS::GetInstance().GetComponent<StateMachine>(entity);

		ImGui::Text("Current Node: %s",
			(fsmComp.m_CurrentScript ? fsmComp.m_CurrentScript->name.c_str() : "(none)"));

		ImGui::Text("Attached Script: %s",
			(fsmComp.m_CurrentScript && fsmComp.m_CurrentScript->isAttached && !fsmComp.m_CurrentScript->scriptClassName.empty())
			? fsmComp.m_CurrentScript->scriptClassName.c_str()
			: "(none)");

		ImGui::Separator();
		if (ImGui::Button("Edit State Machine"))
		{
			auto fsmWindow = editor::EditorGUI::GetWindow<FSMEditorImGUI>();
			if (fsmWindow)
			{
				fsmWindow->SetSelectedEntity(entity);
				editor::EditorGUI::FocusWindow("FSM Editor");
			}
		}
	}

	void HierarchyInspector::DrawNavMeshComponent(EntityID entity)
	{
		if (!ImGui::CollapsingHeader("NavMesh", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		auto& nav = ECS::GetInstance().GetComponent<NavMeshComponent>(entity);

		ImGui::TextUnformatted("Recast Build Settings");
		ImGui::DragFloat("Cell Size", &nav.cellSize, 0.01f, 0.01f, 2.0f);
		ImGui::DragFloat("Cell Height", &nav.cellHeight, 0.01f, 0.01f, 2.0f);
		//ImGui::DragFloat("Agent Height", &nav.agentHeight, 0.01f, 0.1f, 5.0f);
		//ImGui::DragFloat("Agent Radius", &nav.agentRadius, 0.01f, 0.05f, 2.0f);
		ImGui::DragFloat("Max Climb", &nav.agentMaxClimb, 0.01f, 0.0f, 2.0f);
		ImGui::DragFloat("Max Slope", &nav.agentMaxSlope, 0.1f, 0.0f, 89.0f);

		ImGui::Checkbox("Bake CustomMesh Shape", &nav.bakeUsingCustomMesh);
		ImGui::Separator();

		if (ImGui::Button("Bake Nav Mesh"))
		{
			if (auto sys = ECS::GetInstance().GetSystem<NavMeshSystem>())
				sys->BakeNavMesh(entity);
		}

		ImGui::Separator();
		ImGui::Checkbox("Draw Walkable", &nav.drawWalkable);
		//ImGui::Checkbox("Draw NavMesh", &nav.drawNavMesh);
	}

	void HierarchyInspector::DrawNavMeshAgentComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<NavMeshAgent>("NavMesh Agent", entity))
			return;

		auto& ecs = ECS::GetInstance();
		auto& agent = ecs.GetComponent<NavMeshAgent>(entity);

		auto AutoFitFromPhysicsCollider = [&](NavMeshAgent& a) -> bool
		{
				if (!ecs.HasComponent<Transform>(entity) || !ecs.HasComponent<PhysicComponent>(entity))
					return false;

				auto& t = ecs.GetComponent<Transform>(entity);
				auto& p = ecs.GetComponent<PhysicComponent>(entity);

				// Match Physics.cpp: primitive.size if there's a Mesh, else 1.0
				float primX = 1.0f, primY = 1.0f, primZ = 1.0f;
				if (ecs.HasComponent<Mesh>(entity))
				{
					auto& m = ecs.GetComponent<Mesh>(entity);
					primX = m.primitive.size.x;
					primY = m.primitive.size.y;
					primZ = m.primitive.size.z;
				}

				// Defaults if something goes weird
				const float minVal = 0.01f;

				switch (p.shapeType)
				{
				case ShapeType::Box:
				{
					// Physics.cpp:
					// halfExtent = scale * 0.5 * primitive.size * colliderSize
					float hx = t.scale.x * 0.5f * primX * p.colliderSize.x;
					float hy = t.scale.y * 0.5f * primY * p.colliderSize.y;
					float hz = t.scale.z * 0.5f * primZ * p.colliderSize.z;

					hx = std::max(hx, minVal);
					hy = std::max(hy, minVal);
					hz = std::max(hz, minVal);

					a.radius = std::max(hx, hz);
					a.height = 2.0f * hy;
					a.centerYOffset = hy;
					break;
				}

				case ShapeType::Sphere:
				{
					// Physics.cpp:
					// radius = scale.x * primitive.size.x * colliderSize.x
					float r = t.scale.x * primX * p.colliderSize.x;
					r = (r > 0.0f && std::isfinite(r)) ? r : minVal;

					a.radius = r;
					a.height = 2.0f * r; // reasonable nav height for a sphere
					a.centerYOffset = a.radius;
					break;
				}

				case ShapeType::Capsule:
				{
					// Physics.cpp:
					// halfHeight = scale.y * 0.5 * primY * colliderSize.y
					// capRadius  = scale.x * 0.5 * primX * colliderSize.x
					float halfH = t.scale.y * 0.5f * primY * p.colliderSize.y;
					float r = t.scale.x * 0.5f * primX * p.colliderSize.x;

					halfH = (halfH > 0.0f && std::isfinite(halfH)) ? halfH : minVal;
					r = (r > 0.0f && std::isfinite(r)) ? r : minVal;

					a.radius = r;
					// Total capsule height = cylinder(2*halfH) + two hemispheres(2*r)
					a.height = 2.0f * halfH;
					a.centerYOffset = halfH;
					break;
				}

				case ShapeType::CustomMesh:
				{
					// Fallback: use AABB of whatever vertices are available (scaled)
					// Prefer PhysicComponent.customMeshVertices (Physics fills this for custom meshes)
					const auto& verts = p.customMeshVertices;
					if (verts.empty())
						return false;

					glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
					for (auto& v : verts)
					{
						glm::vec3 s(v.x * t.scale.x, v.y * t.scale.y, v.z * t.scale.z);
						mn = glm::min(mn, s);
						mx = glm::max(mx, s);
					}

					glm::vec3 size = mx - mn;
					a.radius = 0.5f * std::max(size.x, size.z);
					a.height = std::max(size.y, minVal);
					a.radius = std::max(a.radius, minVal);
					a.centerYOffset = a.height * 0.5f;
					break;
				}

				default:
					return false;
				}

				// Corner cutting help
				if (a.stoppingDistance < a.radius)
					a.stoppingDistance = a.radius;

				agent.radius = std::clamp(agent.radius, 0.05f, 2.0f);
				agent.height = std::clamp(agent.height, 0.2f, 6.0f);
				agent.centerYOffset = std::clamp(agent.centerYOffset, 0.0f, agent.height);

				return true;
		};

		if (agent.autoFitFromCollider && !agent.didAutoFit)
		{
			if (AutoFitFromPhysicsCollider(agent))
				agent.didAutoFit = true;
		}

		// Editable fields
		//ImGui::DragFloat("Speed", &agent.speed, 0.1f, 0.0f, 100.0f);
		//ImGui::DragFloat("Acceleration", &agent.acceleration, 0.1f, 0.0f, 100.0f);
		//ImGui::DragFloat("Stopping Distance", &agent.stoppingDistance, 0.01f, 0.0f, 10.0f);
		//ImGui::Checkbox("Auto Rotate", &agent.autoRotate);

#if defined(EE_EDITOR)
		ImGui::SeparatorText("Debug");
		ImGui::Checkbox("Show Path", &agent.debugDrawPath);
#endif

		ImGui::Checkbox("Auto Fit From Collider", &agent.autoFitFromCollider);
		ImGui::DragFloat("Radius", &agent.radius, 0.01f, 0.01f, 10.0f);
		ImGui::DragFloat("Height", &agent.height, 0.01f, 0.01f, 20.0f);
		agent.centerYOffset = std::max(agent.radius, agent.height * 0.5f);

		if (ImGui::Button("Refit From Collider"))
			agent.didAutoFit = false;
	}

	void HierarchyInspector::DrawNavJumpComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<NavJumpLink>("Nav Jump Link", entity))
			return;

		auto& navJ = ECS::GetInstance().GetComponent<NavJumpLink>(entity);

		ImGui::SeparatorText("Jump Settings");

		ImGui::DragFloat3("Landing Position", &navJ.landingPosition.x, 0.1f);
		ImGui::DragFloat("Jump Duration", &navJ.jumpDuration, 0.01f, 0.05f, 3.0f);
		ImGui::DragFloat("Jump Height", &navJ.jumpHeight, 0.05f, 0.0f, 10.0f);
	}

	void HierarchyInspector::DrawParticleEmitterComponent(EntityID entity)
	{
		//if (!ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen))
		//	return;

		if (!ComponentHeaderWithRemove<ParticleEmitter>("Particle Emitter", entity))
			return;

		auto& emitter = ECS::GetInstance().GetComponent<ParticleEmitter>(entity);

		auto particleSystem = ECS::GetInstance().GetSystem<ParticleSystem>();
		if (!particleSystem)
		{
			ImGui::Text("Particle System not found.");
			return;
		}

		// count alive particles for this emitter
		int alive = particleSystem->GetParticleCount();

		// Display info
		ImGui::Text("Emitter Active: %s", emitter.active ? "Yes" : "No");
		ImGui::Text("Particles Alive: %d", alive);
	}

	void HierarchyInspector::DrawGPUParticleEmitterComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<GPUParticleEmitter>("Particle Emitter (GPU)", entity))
			return;

		auto& emitter = ECS::GetInstance().GetComponent<GPUParticleEmitter>(entity);

		ImGui::Checkbox("Active", &emitter.active);
		ImGui::SameLine();
		ImGui::Checkbox("Show Debug Bounds", &emitter.showDebugBounds);

		// Quick manipulation section
		if (ImGui::CollapsingHeader("Quick Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
			
			// Overall Scale
			ImGui::Text("Overall Size");
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderFloat("##OverallScale", &emitter.overallScale, 0.1f, 10.0f, "%.2fx")) {
				emitter.overallScale = std::max(0.01f, emitter.overallScale);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Scales all size/radius/area parameters\nAffects: particle size, spawn area, bounds");
			}

			// Quick size buttons
			ImGui::Text("Quick Sizes:");
			if (ImGui::Button("Tiny##Size")) {
				emitter.overallScale = 0.25f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Small##Size")) {
				emitter.overallScale = 0.5f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Normal##Size")) {
				emitter.overallScale = 1.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Large##Size")) {
				emitter.overallScale = 2.0f;
			}
			ImGui::SameLine();
			if (ImGui::Button("Huge##Size")) {
				emitter.overallScale = 4.0f;
			}

			ImGui::Separator();
			ImGui::Text("Particle Offset (relative to parent)");
			ImGui::SetNextItemWidth(-1);
			
			// Local position offset controls
			float offset[3] = { emitter.localPositionOffset.x, emitter.localPositionOffset.y, emitter.localPositionOffset.z };
			if (ImGui::DragFloat3("Local Position", offset, 0.01f)) {
				emitter.localPositionOffset = Vec3(offset[0], offset[1], offset[2]);
			}
			
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Offset from parent object's position\nX=Right, Y=Up, Z=Forward in local space");
			}

			// Helper buttons for common offsets
			ImGui::Text("Quick Offsets:");
			if (ImGui::Button("Center")) {
				emitter.localPositionOffset = Vec3(0.0f, 0.0f, 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Top")) {
				emitter.localPositionOffset = Vec3(0.0f, 1.0f, 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Front")) {
				emitter.localPositionOffset = Vec3(0.0f, 0.0f, 1.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Right")) {
				emitter.localPositionOffset = Vec3(1.0f, 0.0f, 0.0f);
			}

			// Quick preset buttons
			ImGui::Separator();
			ImGui::Text("Quick Presets");
			
			if (ImGui::Button("Electric Door Lock")) {
				// Gate (1) settings
				emitter.maxParticles = 512;
				emitter.emissionShape = 1;
				emitter.overallScale = 1.0f;
				emitter.spawnBoxExtents = Vec3(1.0f, 1.0f, 1.0f);
				emitter.spawnRadius = 150.0f;
				emitter.spawnRadiusInner = 50.0f;
				emitter.spawnRate = 20.0f;
				emitter.burstCountMin = 20;
				emitter.burstCountMax = 40;
				emitter.burstInterval = 1.5f;
				emitter.burstOnStart = true;
				emitter.directionMode = 0;
				emitter.direction = Vec3(0.0f, 0.0f, 1.0f);
				emitter.coneAngle = 25.0f;
				emitter.coneInnerAngle = 0.0f;
				emitter.speedMin = 0.5f;
				emitter.speedMax = 3.0f;
				emitter.gravity = Vec3(0.0f, 0.5f, 0.0f);
				emitter.drag = 0.0f;
				emitter.turbulenceStrength = 0.0f;
				emitter.turbulenceScale = 1.0f;
				emitter.boundsMode = 1;
				emitter.boundsShape = 0;
				emitter.boundsBoxExtents = Vec3(1.0f, 1.0f, 1.0f);
				emitter.boundsRadius = 2.0f;
				emitter.boundsRadiusInner = 0.0f;
				emitter.renderMode = 2; // Electric
				emitter.smokeOpacity = 0.6f;
				emitter.smokeSoftness = 0.5f;
				emitter.smokeNoiseScale = 0.15f;
				emitter.smokeDistortScale = 0.25f;
				emitter.smokeDistortStrength = 0.35f;
				emitter.smokePuffScale = 0.35f;
				emitter.smokePuffStrength = 0.6f;
				emitter.smokeStretch = 0.5f;
				emitter.smokeUpBias = 0.2f;
				emitter.smokeDepthFade = 6.0f;
				emitter.electricIntensity = 5.0f;
				emitter.electricFrequency = 15.0f;
				emitter.electricBoltThickness = 0.08f;
				emitter.electricBoltVariation = 1.5f;
				emitter.electricGlow = 2.0f;
				emitter.electricBoltCount = 1;
				emitter.colorStart = Vec3(0.3f, 0.6f, 1.0f);
				emitter.colorEnd = Vec3(0.1f, 0.2f, 0.5f);
				emitter.alphaStart = 1.0f;
				emitter.alphaEnd = 0.0f;
				emitter.sizeStartMin = 0.5f;
				emitter.sizeStartMax = 1.0f;
				emitter.sizeEndMin = 0.15f;
				emitter.sizeEndMax = 2.0f;
				emitter.lifetimeMin = 0.3f;
				emitter.lifetimeMax = 0.6f;
				emitter.sparkleShape = 0;
			}
			ImGui::SameLine();
			if (ImGui::Button("Electric Hazard")) {
				emitter.renderMode = 2;
				emitter.electricIntensity = 1.5f;
				emitter.electricFrequency = 25.0f;
				emitter.electricBoltCount = 2;
				emitter.electricBoltThickness = 0.06f;
				emitter.electricBoltVariation = 2.5f;
				emitter.electricGlow = 0.3f;
				emitter.colorStart = Vec3(1.0f, 0.9f, 0.2f);
				emitter.colorEnd = Vec3(1.0f, 0.5f, 0.0f);
				emitter.burstOnStart = true;
				emitter.burstInterval = 1.0f;
				emitter.burstCountMin = 20;
				emitter.burstCountMax = 40;
			}
			
			if (ImGui::Button("Magic Glow")) {
				emitter.renderMode = 0; // Glow
				emitter.sparkleShape = 1; // Star
				emitter.colorStart = Vec3(0.8f, 0.2f, 1.0f);
				emitter.colorEnd = Vec3(0.2f, 0.1f, 0.5f);
				emitter.spawnRate = 30.0f;
				emitter.lifetimeMin = 0.8f;
				emitter.lifetimeMax = 1.5f;
				emitter.gravity = Vec3(0.0f, 0.5f, 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Smoke Puff")) {
				emitter.renderMode = 1; // Smoke
				emitter.smokeOpacity = 0.6f;
				emitter.smokeSoftness = 0.5f;
				emitter.colorStart = Vec3(0.3f, 0.3f, 0.3f);
				emitter.colorEnd = Vec3(0.1f, 0.1f, 0.1f);
				emitter.burstOnStart = true;
				emitter.burstCountMin = 50;
				emitter.burstCountMax = 100;
				emitter.lifetimeMin = 1.5f;
				emitter.lifetimeMax = 3.0f;
			}
		}

		ImGui::Separator();
		ImGui::Text("Particle Settings");

		int maxParticles = emitter.maxParticles;
		if (ImGui::DragInt("Max Particles", &maxParticles, 1.0f, 16, 4096)) {
			emitter.maxParticles = maxParticles;
			emitter.initialized = false; // Force reinit
		}

		ImGui::Separator();
		ImGui::Text("Emission");

		const char* emissionShapes[] = { "Point", "Sphere", "Box", "Disc (XZ)" };
		ImGui::Combo("Emission Shape", &emitter.emissionShape, emissionShapes, 4);
		ImGui::DragFloat("Spawn Radius", &emitter.spawnRadius, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Spawn Inner Radius", &emitter.spawnRadiusInner, 0.01f, 0.0f, emitter.spawnRadius);
		ImGui::DragFloat3("Spawn Box Extents", &emitter.spawnBoxExtents.x, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Spawn Rate (per sec)", &emitter.spawnRate, 0.1f, 0.0f, 500.0f);
		ImGui::DragInt("Burst Count Min", &emitter.burstCountMin, 1.0f, 0, 1024);
		ImGui::DragInt("Burst Count Max", &emitter.burstCountMax, 1.0f, 0, 1024);
		ImGui::DragFloat("Burst Interval (sec)", &emitter.burstInterval, 0.01f, 0.0f, 10.0f);
		ImGui::Checkbox("Burst On Start", &emitter.burstOnStart);

		ImGui::Separator();
		ImGui::Text("Direction");

		const char* directionModes[] = { "Fixed", "Cone", "From Spawn", "Random Sphere" };
		ImGui::Combo("Direction Mode", &emitter.directionMode, directionModes, 4);
		ImGui::DragFloat3("Direction", &emitter.direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::DragFloat("Cone Angle", &emitter.coneAngle, 0.1f, 0.0f, 180.0f);
		ImGui::DragFloat("Cone Inner Angle", &emitter.coneInnerAngle, 0.1f, 0.0f, emitter.coneAngle);

		ImGui::Separator();
		ImGui::Text("Bounds");

		const char* boundsModes[] = { "None", "Kill", "Clamp", "Bounce" };
		const char* boundsShapes[] = { "Sphere", "Box", "Disc (XZ)" };
		ImGui::Combo("Bounds Mode", &emitter.boundsMode, boundsModes, 4);
		ImGui::Combo("Bounds Shape", &emitter.boundsShape, boundsShapes, 3);
		ImGui::DragFloat("Bounds Radius", &emitter.boundsRadius, 0.01f, 0.0f, 100.0f);
		ImGui::DragFloat("Bounds Inner Radius", &emitter.boundsRadiusInner, 0.01f, 0.0f, emitter.boundsRadius);
		ImGui::DragFloat3("Bounds Box Extents", &emitter.boundsBoxExtents.x, 0.01f, 0.0f, 100.0f);

		ImGui::Separator();
		ImGui::Text("Forces");

		ImGui::DragFloat("Speed Min", &emitter.speedMin, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat("Speed Max", &emitter.speedMax, 0.01f, 0.0f, 50.0f);
		ImGui::DragFloat3("Gravity", &emitter.gravity.x, 0.01f, -50.0f, 50.0f);
		ImGui::DragFloat("Drag", &emitter.drag, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Turbulence Strength", &emitter.turbulenceStrength, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Turbulence Scale", &emitter.turbulenceScale, 0.01f, 0.01f, 10.0f);

		ImGui::Separator();
		ImGui::Text("Appearance");

		const char* renderModes[] = { "Glow", "Smoke", "Electric" };
		ImGui::Combo("Render Mode", &emitter.renderMode, renderModes, 3);
		if (emitter.renderMode == 1) {
			ImGui::DragFloat("Smoke Opacity", &emitter.smokeOpacity, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("Smoke Softness", &emitter.smokeSoftness, 0.01f, 0.01f, 2.0f);
			ImGui::DragFloat("Smoke Noise Scale", &emitter.smokeNoiseScale, 0.01f, 0.01f, 5.0f);
			ImGui::DragFloat("Smoke Distort Scale", &emitter.smokeDistortScale, 0.01f, 0.01f, 5.0f);
			ImGui::DragFloat("Smoke Distort Strength", &emitter.smokeDistortStrength, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("Smoke Puff Scale", &emitter.smokePuffScale, 0.01f, 0.01f, 5.0f);
			ImGui::DragFloat("Smoke Puff Strength", &emitter.smokePuffStrength, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Smoke Stretch", &emitter.smokeStretch, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Smoke Up Bias", &emitter.smokeUpBias, 0.01f, 0.0f, 2.0f);
			ImGui::DragFloat("Smoke Depth Fade", &emitter.smokeDepthFade, 0.1f, 0.0f, 20.0f);
		}
		else if (emitter.renderMode == 2) { // Electric mode
			ImGui::DragFloat("Electric Intensity", &emitter.electricIntensity, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Electric Frequency", &emitter.electricFrequency, 0.1f, 1.0f, 50.0f);
			ImGui::DragInt("Bolt Count", &emitter.electricBoltCount, 0.1f, 1, 10);
			ImGui::DragFloat("Bolt Thickness", &emitter.electricBoltThickness, 0.01f, 0.01f, 0.5f);
			ImGui::DragFloat("Bolt Variation", &emitter.electricBoltVariation, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("Electric Glow", &emitter.electricGlow, 0.01f, 0.0f, 2.0f);
		}

		float colorStart[3] = { emitter.colorStart.x, emitter.colorStart.y, emitter.colorStart.z };
		if (ImGui::ColorEdit3("Color Start", colorStart)) {
			emitter.colorStart = Vec3(colorStart[0], colorStart[1], colorStart[2]);
		}
		float colorEnd[3] = { emitter.colorEnd.x, emitter.colorEnd.y, emitter.colorEnd.z };
		if (ImGui::ColorEdit3("Color End", colorEnd)) {
			emitter.colorEnd = Vec3(colorEnd[0], colorEnd[1], colorEnd[2]);
		}
		ImGui::DragFloat("Alpha Start", &emitter.alphaStart, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Alpha End", &emitter.alphaEnd, 0.01f, 0.0f, 1.0f);

		ImGui::DragFloat("Size Start Min", &emitter.sizeStartMin, 0.001f, 0.001f, 5.0f);
		ImGui::DragFloat("Size Start Max", &emitter.sizeStartMax, 0.001f, 0.001f, 5.0f);
		ImGui::DragFloat("Size End Min", &emitter.sizeEndMin, 0.001f, 0.001f, 5.0f);
		ImGui::DragFloat("Size End Max", &emitter.sizeEndMax, 0.001f, 0.001f, 5.0f);

		ImGui::Separator();
		ImGui::Text("Lifetime");

		ImGui::DragFloat("Lifetime Min", &emitter.lifetimeMin, 0.1f, 0.1f, 10.0f);
		ImGui::DragFloat("Lifetime Max", &emitter.lifetimeMax, 0.1f, 0.1f, 10.0f);

		ImGui::Separator();
		ImGui::Text("Sparkle Shape");

		const char* shapes[] = { "Soft Circle", "Star", "Diamond" };
		ImGui::Combo("Shape", &emitter.sparkleShape, shapes, 3);

		// Status
		ImGui::Separator();
		ImGui::Text("Status: %s", emitter.initialized ? "Initialized" : "Not Initialized");
		ImGui::Text("Particles: %d", emitter.maxParticles);
	}

	void HierarchyInspector::DrawCameraComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<CameraComponent>("Camera Component", entity))
			return;

		auto& cameraComp = ECS::GetInstance().GetComponent<CameraComponent>(entity);

		ImGui::SliderFloat("Field of View", &cameraComp.fov, 40, 120);

		ImGui::Text("Clipping Planes");
		if (ImGui::BeginTable("clipPlanesTable", 2))
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Near:");

			ImGui::TableNextColumn();

			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputFloat("##Near", &cameraComp.nearPlane, 0.0f, 0.0f, "%.1f");

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Far");

			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputFloat("##Far", &cameraComp.farPlane, 0.0f, 0.0f, "%.1f");

			ImGui::EndTable();
		}
	}

	void HierarchyInspector::DrawUIComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<UIComponent>("UI Component (HUD)", entity))
			return;

		auto& ui = ECS::GetInstance().GetComponent<UIComponent>(entity);

		// === HEALTHBAR SETTINGS ===
		if (ImGui::CollapsingHeader("Healthbar Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Healthbar", &ui.showHealthbar);
			ImGui::ColorEdit3("Health Color", &ui.healthbarColor.x);
			ImGui::ColorEdit3("Background Color", &ui.healthbarBgColor.x);
			ImGui::DragFloat("Width", &ui.healthbarWidth, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Height", &ui.healthbarHeight, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat3("Position", &ui.healthbarPosition.x, 0.01f, 0.0f, 1.0f);

			char bgTexBuffer[256];
			strncpy_s(bgTexBuffer, ui.healthbarBgTexture.c_str(), sizeof(bgTexBuffer) - 1);
			bgTexBuffer[sizeof(bgTexBuffer) - 1] = '\0';
			if (ImGui::InputText("BG Texture", bgTexBuffer, sizeof(bgTexBuffer)))
				ui.healthbarBgTexture = bgTexBuffer;

			char fillTexBuffer[256];
			strncpy_s(fillTexBuffer, ui.healthbarFillTexture.c_str(), sizeof(fillTexBuffer) - 1);
			fillTexBuffer[sizeof(fillTexBuffer) - 1] = '\0';
			if (ImGui::InputText("Fill Texture", fillTexBuffer, sizeof(fillTexBuffer)))
				ui.healthbarFillTexture = fillTexBuffer;

			char frameTexBuffer[256];
			strncpy_s(frameTexBuffer, ui.healthbarFrameTexture.c_str(), sizeof(frameTexBuffer) - 1);
			frameTexBuffer[sizeof(frameTexBuffer) - 1] = '\0';
			if (ImGui::InputText("Frame Texture", frameTexBuffer, sizeof(frameTexBuffer)))
				ui.healthbarFrameTexture = frameTexBuffer;
		}

		// === HEALTH SYSTEM ===
		if (ImGui::CollapsingHeader("Health System"))
		{
			ImGui::DragFloat("Current Health", &ui.currentHealth, 1.0f, 0.0f, ui.maxHealth);
			ImGui::DragFloat("Max Health", &ui.maxHealth, 1.0f, 1.0f, 1000.0f);
			ImGui::DragFloat("Regen Rate", &ui.healthRegenRate, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("Regen Delay", &ui.healthRegenDelay, 0.1f, 0.0f, 10.0f);
		}

		// === SKILL SLOTS ===
		if (ImGui::CollapsingHeader("Skill Slots Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Skills", &ui.showSkills);
			ImGui::DragFloat("Slot Size", &ui.skillSlotSize, 0.001f, 0.01f, 0.2f);
			ImGui::DragFloat("Slot Spacing", &ui.skillSlotSpacing, 0.001f, 0.0f, 0.1f);
			ImGui::DragFloat3("Skills Position", &ui.skillsPosition.x, 0.01f, 0.0f, 1.0f);
		}

		// === INDIVIDUAL SKILLS ===
		if (ImGui::CollapsingHeader("Individual Skills (4 Slots)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Skills only render if Skill Name is set!");
			ImGui::Separator();

			for (int i = 0; i < 4; ++i)
			{
				ImGui::PushID(i);
				if (ImGui::TreeNode(("Skill Slot " + std::to_string(i + 1)).c_str()))
				{
					auto& skill = ui.skills[i];

					// Skill Name (REQUIRED for skill to render!)
					char nameBuffer[128];
					strncpy_s(nameBuffer, skill.skillName.c_str(), sizeof(nameBuffer) - 1);
					nameBuffer[sizeof(nameBuffer) - 1] = '\0';
					if (ImGui::InputText("Skill Name", nameBuffer, sizeof(nameBuffer)))
						skill.skillName = nameBuffer;
					if (skill.skillName.empty())
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "WARNING: Empty name - skill won't render!");

					// Keybind
					char keybindBuffer[32];
					strncpy_s(keybindBuffer, skill.keyBinding.c_str(), sizeof(keybindBuffer) - 1);
					keybindBuffer[sizeof(keybindBuffer) - 1] = '\0';
					if (ImGui::InputText("Key Binding", keybindBuffer, sizeof(keybindBuffer)))
						skill.keyBinding = keybindBuffer;

					// Description
					char descBuffer[256];
					strncpy_s(descBuffer, skill.description.c_str(), sizeof(descBuffer) - 1);
					descBuffer[sizeof(descBuffer) - 1] = '\0';
					if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer)))
						skill.description = descBuffer;

					// Icon Texture Path
					char iconBuffer[256];
					strncpy_s(iconBuffer, skill.iconTexturePath.c_str(), sizeof(iconBuffer) - 1);
					iconBuffer[sizeof(iconBuffer) - 1] = '\0';
					if (ImGui::InputText("Icon Texture Path", iconBuffer, sizeof(iconBuffer)))
						skill.iconTexturePath = iconBuffer;

					// Cooldown and Cost
					ImGui::DragFloat("Max Cooldown (sec)", &skill.maxCooldown, 0.1f, 0.0f, 60.0f);
					ImGui::DragFloat("Health Cost", &skill.manaCost, 1.0f, 0.0f, 100.0f);

					ImGui::TreePop();
				}
				ImGui::PopID();
				ImGui::Separator();
			}
		}

		// === SKILL APPEARANCE ===
		if (ImGui::CollapsingHeader("Skill Appearance"))
		{
			ImGui::Text("Ready State:");
			ImGui::ColorEdit3("Ready Tint", &ui.skillReadyTint.x);
			ImGui::DragFloat("Ready Alpha", &ui.skillReadyAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Cooldown State:");
			ImGui::ColorEdit3("Cooldown Tint", &ui.skillCooldownTint.x);
			ImGui::DragFloat("Cooldown Alpha", &ui.skillCooldownAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Low Health State:");
			ImGui::ColorEdit3("Low Health Tint", &ui.skillLowHealthTint.x);
			ImGui::DragFloat("Low Health Alpha", &ui.skillLowHealthAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Fallback (No Texture):");
			ImGui::ColorEdit3("Fallback Color", &ui.skillFallbackColor.x);
			ImGui::DragFloat("Fallback Alpha", &ui.skillFallbackAlpha, 0.01f, 0.0f, 1.0f);
		}

		// === SKILL EFFECTS ===
		if (ImGui::CollapsingHeader("Skill Effects"))
		{
			ImGui::Text("Cooldown Overlay:");
			ImGui::ColorEdit3("Overlay Color", &ui.skillCooldownOverlayColor.x);
			ImGui::DragFloat("Overlay Alpha", &ui.skillCooldownOverlayAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Activation Flash:");
			ImGui::DragFloat("Flash Duration", &ui.skillFlashDuration, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Flash Color", &ui.skillFlashColor.x);
			ImGui::DragFloat("Flash Glow Size", &ui.skillFlashGlowSize, 0.001f, 0.0f, 0.1f);
		}

		// === SKILL KEYBIND LABELS ===
		if (ImGui::CollapsingHeader("Skill Keybind Labels"))
		{
			ImGui::DragFloat("Text Scale", &ui.skillKeybindTextScale, 0.01f, 0.1f, 2.0f);
			ImGui::DragFloat("Offset Below Slot", &ui.skillKeybindOffsetY, 0.001f, 0.0f, 0.2f);
			ImGui::ColorEdit3("Label Color", &ui.skillKeybindColor.x);
			ImGui::DragFloat("Alpha (Ready)", &ui.skillKeybindAlphaReady, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Alpha (Not Ready)", &ui.skillKeybindAlphaNotReady, 0.01f, 0.0f, 1.0f);
		}

		// === CROSSHAIR ===
		if (ImGui::CollapsingHeader("Crosshair"))
		{
			ImGui::Checkbox("Show Crosshair", &ui.showCrosshair);
			ImGui::DragFloat("Size", &ui.crosshairSize, 0.001f, 0.01f, 0.2f);
			ImGui::DragInt("Style", &ui.crosshairStyle, 1, 0, 2);
		}
	}

	void HierarchyInspector::DrawUIHealthbarComponent(EntityID entity)
	{
		if (!ComponentHeaderWithCopyPaste<UIHealthbarComponent>("UI Healthbar", entity, s_ClipboardUIHealthbar))
			return;

		auto& healthbar = ECS::GetInstance().GetComponent<UIHealthbarComponent>(entity);

		ImGui::Checkbox("Show Healthbar", &healthbar.showHealthbar);

		ImGui::Separator();
		ImGui::Text("Colors (based on health %)");
		ImGui::ColorEdit3("Normal Color", &healthbar.healthbarColor.x);
		ImGui::ColorEdit3("Low Health Color (< 50%)", &healthbar.healthbarLowColor.x);
		ImGui::ColorEdit3("Critical Color (< 25%)", &healthbar.healthbarCriticalColor.x);
		ImGui::ColorEdit3("Background Color", &healthbar.healthbarBgColor.x);

		ImGui::Separator();
		ImGui::Text("Shine Effect (no texture only)");
		ImGui::ColorEdit3("Shine Color", &healthbar.healthbarShineColor.x);
		ImGui::DragFloat("Shine Alpha", &healthbar.healthbarShineAlpha, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Size & Position");
		ImGui::DragFloat("Width", &healthbar.healthbarWidth, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Height", &healthbar.healthbarHeight, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat3("Position", &healthbar.healthbarPosition.x, 0.01f, 0.0f, 1.0f);

		char bgTexBuffer[256];
		strncpy_s(bgTexBuffer, healthbar.healthbarBgTexture.c_str(), sizeof(bgTexBuffer) - 1);
		bgTexBuffer[sizeof(bgTexBuffer) - 1] = '\0';
		if (ImGui::InputText("BG Texture", bgTexBuffer, sizeof(bgTexBuffer)))
			healthbar.healthbarBgTexture = bgTexBuffer;

		char fillTexBuffer[256];
		strncpy_s(fillTexBuffer, healthbar.healthbarFillTexture.c_str(), sizeof(fillTexBuffer) - 1);
		fillTexBuffer[sizeof(fillTexBuffer) - 1] = '\0';
		if (ImGui::InputText("Fill Texture", fillTexBuffer, sizeof(fillTexBuffer)))
			healthbar.healthbarFillTexture = fillTexBuffer;

		char frameTexBuffer[256];
		strncpy_s(frameTexBuffer, healthbar.healthbarFrameTexture.c_str(), sizeof(frameTexBuffer) - 1);
		frameTexBuffer[sizeof(frameTexBuffer) - 1] = '\0';
		if (ImGui::InputText("Frame Texture", frameTexBuffer, sizeof(frameTexBuffer)))
			healthbar.healthbarFrameTexture = frameTexBuffer;

		ImGui::Separator();
		ImGui::Text("Health System");
		ImGui::DragFloat("Current Health", &healthbar.currentHealth, 1.0f, 0.0f, healthbar.maxHealth);
		ImGui::DragFloat("Max Health", &healthbar.maxHealth, 1.0f, 1.0f, 1000.0f);
		ImGui::DragFloat("Regen Rate", &healthbar.healthRegenRate, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Regen Delay", &healthbar.healthRegenDelay, 0.1f, 0.0f, 10.0f);
	}

	void HierarchyInspector::DrawUICrosshairComponent(EntityID entity)
	{
		if (!ComponentHeaderWithCopyPaste<UICrosshairComponent>("UI Crosshair", entity, s_ClipboardUICrosshair))
			return;

		auto& crosshair = ECS::GetInstance().GetComponent<UICrosshairComponent>(entity);

		ImGui::Checkbox("Show Crosshair", &crosshair.showCrosshair);

		// Texture Path (editable!)
		char texPathBuffer[256];
		strncpy_s(texPathBuffer, crosshair.crosshairTexturePath.c_str(), sizeof(texPathBuffer) - 1);
		texPathBuffer[sizeof(texPathBuffer) - 1] = '\0';
		if (ImGui::InputText("Texture Path", texPathBuffer, sizeof(texPathBuffer)))
			crosshair.crosshairTexturePath = texPathBuffer;

		ImGui::ColorEdit3("Crosshair Color", &crosshair.crosshairColor.x);
		ImGui::DragFloat("Size", &crosshair.crosshairSize, 0.001f, 0.01f, 0.2f);
		ImGui::DragFloat("Thickness", &crosshair.crosshairThickness, 0.0001f, 0.0001f, 0.01f);
		ImGui::DragInt("Style", &crosshair.crosshairStyle, 1, 0, 2);
		ImGui::DragFloat("Gap", &crosshair.crosshairGap, 0.001f, 0.0f, 0.1f);
	}

	void HierarchyInspector::DrawUISkillsComponent(EntityID entity)
	{
		if (!ComponentHeaderWithCopyPaste<UISkillsComponent>("UI Skills", entity, s_ClipboardUISkills))
			return;

		auto& skills = ECS::GetInstance().GetComponent<UISkillsComponent>(entity);

		ImGui::Checkbox("Show Skills", &skills.showSkills);
		ImGui::DragFloat("Default Slot Size", &skills.skillSlotSize, 0.001f, 0.01f, 0.2f);
		ImGui::TextDisabled("(Individual slots can override this)");

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Skill Slots", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Skills only render if Skill Name is set!");
			ImGui::TextDisabled("Each slot has its own position - arrange them anywhere!");

			// Add/Remove buttons
			if (ImGui::Button("+ Add Skill Slot"))
			{
				UISkillsComponent::SkillSlot newSlot;
				newSlot.position = { 0.5f, 0.1f, 0.0f };  // Default center bottom
				skills.skills.push_back(newSlot);
			}
			ImGui::SameLine();
			if (ImGui::Button("- Remove Last Slot") && !skills.skills.empty())
			{
				skills.skills.pop_back();
			}
			ImGui::Text("Total Slots: %zu", skills.skills.size());

			ImGui::Separator();

			for (size_t i = 0; i < skills.skills.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));

				// Use fixed header ID to prevent tree node from closing when name changes
				std::string headerId = "skill_slot_" + std::to_string(i);
				std::string displayName = "Skill Slot " + std::to_string(i + 1);
				if (!skills.skills[i].skillName.empty())
					displayName += " (" + skills.skills[i].skillName + ")";

				if (ImGui::TreeNodeEx(headerId.c_str(), ImGuiTreeNodeFlags_None, "%s", displayName.c_str()))
				{
					auto& skill = skills.skills[i];

					// Position and size (MOST IMPORTANT - at top!)
					ImGui::DragFloat3("Position", &skill.position.x, 0.01f, 0.0f, 1.0f);
					ImGui::DragFloat("Size Override", &skill.size, 0.001f, 0.0f, 0.2f);
					ImGui::TextDisabled("(0 = use default size)");

					ImGui::Separator();

					char nameBuffer[128];
					strncpy_s(nameBuffer, skill.skillName.c_str(), sizeof(nameBuffer) - 1);
					nameBuffer[sizeof(nameBuffer) - 1] = '\0';
					if (ImGui::InputText("Skill Name", nameBuffer, sizeof(nameBuffer)))
						skill.skillName = nameBuffer;
					if (skill.skillName.empty())
						ImGui::TextColored(ImVec4(1, 0, 0, 1), "WARNING: Empty name - skill won't render!");

					char keybindBuffer[32];
					strncpy_s(keybindBuffer, skill.keyBinding.c_str(), sizeof(keybindBuffer) - 1);
					keybindBuffer[sizeof(keybindBuffer) - 1] = '\0';
					if (ImGui::InputText("Key Binding", keybindBuffer, sizeof(keybindBuffer)))
						skill.keyBinding = keybindBuffer;

					char descBuffer[256];
					strncpy_s(descBuffer, skill.description.c_str(), sizeof(descBuffer) - 1);
					descBuffer[sizeof(descBuffer) - 1] = '\0';
					if (ImGui::InputText("Description", descBuffer, sizeof(descBuffer)))
						skill.description = descBuffer;

					char iconBuffer[256];
					strncpy_s(iconBuffer, skill.iconTexturePath.c_str(), sizeof(iconBuffer) - 1);
					iconBuffer[sizeof(iconBuffer) - 1] = '\0';
					if (ImGui::InputText("Icon Texture Path", iconBuffer, sizeof(iconBuffer)))
						skill.iconTexturePath = iconBuffer;
					ImGui::TextDisabled("(Default icon - used if selected/unselected not set)");

					ImGui::Separator();
					ImGui::Text("Selection State Icons:");

					char selectedIconBuffer[256];
					strncpy_s(selectedIconBuffer, skill.selectedIconPath.c_str(), sizeof(selectedIconBuffer) - 1);
					selectedIconBuffer[sizeof(selectedIconBuffer) - 1] = '\0';
					if (ImGui::InputText("Selected Icon Path", selectedIconBuffer, sizeof(selectedIconBuffer)))
						skill.selectedIconPath = selectedIconBuffer;

					char unselectedIconBuffer[256];
					strncpy_s(unselectedIconBuffer, skill.unselectedIconPath.c_str(), sizeof(unselectedIconBuffer) - 1);
					unselectedIconBuffer[sizeof(unselectedIconBuffer) - 1] = '\0';
					if (ImGui::InputText("Unselected Icon Path", unselectedIconBuffer, sizeof(unselectedIconBuffer)))
						skill.unselectedIconPath = unselectedIconBuffer;

					ImGui::Checkbox("Is Selected", &skill.isSelected);
					ImGui::TextDisabled("(Runtime state - set by game logic)");

					ImGui::Separator();
					ImGui::DragFloat("Max Cooldown (sec)", &skill.maxCooldown, 0.1f, 0.0f, 60.0f);
					ImGui::DragFloat("Health Cost", &skill.manaCost, 1.0f, 0.0f, 100.0f);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Skill Appearance"))
		{
			ImGui::Text("Ready State:");
			ImGui::ColorEdit3("Ready Tint", &skills.skillReadyTint.x);
			ImGui::DragFloat("Ready Alpha", &skills.skillReadyAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Cooldown State:");
			ImGui::ColorEdit3("Cooldown Tint", &skills.skillCooldownTint.x);
			ImGui::DragFloat("Cooldown Alpha", &skills.skillCooldownAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Low Health State:");
			ImGui::ColorEdit3("Low Health Tint", &skills.skillLowHealthTint.x);
			ImGui::DragFloat("Low Health Alpha", &skills.skillLowHealthAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Fallback (No Texture):");
			ImGui::ColorEdit3("Fallback Color", &skills.skillFallbackColor.x);
			ImGui::DragFloat("Fallback Alpha", &skills.skillFallbackAlpha, 0.01f, 0.0f, 1.0f);
		}

		if (ImGui::CollapsingHeader("Skill Effects"))
		{
			ImGui::Text("Cooldown Overlay:");
			ImGui::ColorEdit3("Overlay Color", &skills.skillCooldownOverlayColor.x);
			ImGui::DragFloat("Overlay Alpha", &skills.skillCooldownOverlayAlpha, 0.01f, 0.0f, 1.0f);

			ImGui::Separator();
			ImGui::Text("Activation Flash:");
			ImGui::DragFloat("Flash Duration", &skills.skillFlashDuration, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit3("Flash Color", &skills.skillFlashColor.x);
			ImGui::DragFloat("Flash Glow Size", &skills.skillFlashGlowSize, 0.001f, 0.0f, 0.1f);
		}

		if (ImGui::CollapsingHeader("Skill Keybind Labels"))
		{
			ImGui::DragFloat("Text Scale", &skills.skillKeybindTextScale, 0.01f, 0.1f, 2.0f);
			ImGui::DragFloat("Offset Below Slot", &skills.skillKeybindOffsetY, 0.001f, 0.0f, 0.2f);
			ImGui::ColorEdit3("Label Color", &skills.skillKeybindColor.x);
			ImGui::DragFloat("Alpha (Ready)", &skills.skillKeybindAlphaReady, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Alpha (Not Ready)", &skills.skillKeybindAlphaNotReady, 0.01f, 0.0f, 1.0f);
		}
	}

	void HierarchyInspector::DrawUIManaBarComponent(EntityID entity)
	{
		if (!ComponentHeaderWithCopyPaste<UIManaBarComponent>("UI Mana Bar", entity, s_ClipboardUIManaBar))
			return;

		auto& manaBar = ECS::GetInstance().GetComponent<UIManaBarComponent>(entity);

		ImGui::Checkbox("Show Mana Bar", &manaBar.showManaBar);
		ImGui::ColorEdit3("Mana Color", &manaBar.manaBarColor.x);
		ImGui::ColorEdit3("Background Color", &manaBar.manaBarBgColor.x);
		ImGui::DragFloat("Width", &manaBar.manaBarWidth, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Height", &manaBar.manaBarHeight, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat3("Position", &manaBar.manaBarPosition.x, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Mana System");
		ImGui::DragFloat("Current Mana", &manaBar.currentMana, 1.0f, 0.0f, manaBar.maxMana);
		ImGui::DragFloat("Max Mana", &manaBar.maxMana, 1.0f, 1.0f, 1000.0f);
		ImGui::DragFloat("Regen Rate", &manaBar.manaRegenRate, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Regen Delay", &manaBar.manaRegenDelay, 0.1f, 0.0f, 10.0f);
	}

	void HierarchyInspector::DrawUIBookCounterComponent(EntityID entity)
	{
		if (!ComponentHeaderWithCopyPaste<UIBookCounterComponent>("UI Book Counter", entity, s_ClipboardUIBookCounter))
			return;

		auto& bookCounter = ECS::GetInstance().GetComponent<UIBookCounterComponent>(entity);

		ImGui::Checkbox("Show Book Counter", &bookCounter.showBookCounter);
		ImGui::DragInt("Books Collected", &bookCounter.booksCollected, 1, 0, bookCounter.totalBooks);
		ImGui::DragInt("Total Books", &bookCounter.totalBooks, 1, 1, 100);
		ImGui::DragFloat3("Position", &bookCounter.bookCounterPosition.x, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Text Appearance");
		ImGui::ColorEdit3("Text Color", &bookCounter.textColor.x);
		ImGui::DragFloat("Text Scale", &bookCounter.textScale, 0.1f, 0.1f, 5.0f);
		ImGui::DragFloat("Text Alpha", &bookCounter.textAlpha, 0.01f, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Optional Textures");

		// Book Icon Texture
		char iconTexBuffer[256];
		strncpy_s(iconTexBuffer, bookCounter.bookIconTexture.c_str(), sizeof(iconTexBuffer) - 1);
		iconTexBuffer[sizeof(iconTexBuffer) - 1] = '\0';
		if (ImGui::InputText("Book Icon Texture", iconTexBuffer, sizeof(iconTexBuffer)))
			bookCounter.bookIconTexture = iconTexBuffer;
		ImGui::DragFloat("Icon Size", &bookCounter.bookIconSize, 0.001f, 0.01f, 0.2f);
		ImGui::DragFloat("Icon Offset X", &bookCounter.bookIconOffsetX, 0.01f, -0.5f, 0.5f);

		// Background Texture
		char bgTexBuffer[256];
		strncpy_s(bgTexBuffer, bookCounter.backgroundTexture.c_str(), sizeof(bgTexBuffer) - 1);
		bgTexBuffer[sizeof(bgTexBuffer) - 1] = '\0';
		if (ImGui::InputText("Background Texture", bgTexBuffer, sizeof(bgTexBuffer)))
			bookCounter.backgroundTexture = bgTexBuffer;
		ImGui::DragFloat2("Background Size", &bookCounter.backgroundSize.x, 0.01f, 0.01f, 1.0f);
	}

	void HierarchyInspector::DrawUIImageComponent(EntityID entity)
	{
		if (!ComponentHeaderWithRemove<UIImageComponent>("UI Image Component", entity))
			return;

		auto& imageComp = ECS::GetInstance().GetComponent<UIImageComponent>(entity);

		// Image Path
		char pathBuffer[256];
		strncpy_s(pathBuffer, imageComp.imagePath.c_str(), sizeof(pathBuffer) - 1);
		pathBuffer[sizeof(pathBuffer) - 1] = '\0';
		if (ImGui::InputText("Image Path", pathBuffer, sizeof(pathBuffer))) {
			imageComp.imagePath = pathBuffer;
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_FILE")) {
				const char* droppedPathCStr = static_cast<const char*>(payload->Data);
				std::filesystem::path droppedPath = droppedPathCStr;

				// Only accept texture-ish files
				std::string ext = droppedPath.extension().string();
				for (auto& c : ext) c = (char)tolower(c);

				bool isTextureFile =
					(ext == ".png" ||
						ext == ".jpg" || ext == ".jpeg" ||
						ext == ".tga" ||
						ext == ".bmp" ||
						ext == ".dds" ||
						ext == ".ktx" ||
						ext == ".hdr");

				if (isTextureFile) {
					std::shared_ptr<graphics::Texture> newTex =
						AssetManager::GetInstance().LoadTexture(droppedPath.string());

					if (newTex && newTex->IsValid()) 
					{
						imageComp.imagePath = droppedPath.string();
					}
					else 
					{
						EE_CORE_WARN("Failed to load dropped texture: {}", droppedPath.string());
					}
				}
				else {
					// Ignore non-texture drops so we don't crash
					EE_CORE_INFO("Ignored drop '%s': not a supported texture format",
						droppedPath.string().c_str());
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Fullscreen toggle
		ImGui::Checkbox("Fullscreen", &imageComp.fullscreen);

		// Position (only relevant when not fullscreen)
		if (!imageComp.fullscreen) {
			ImGui::DragFloat3("Position", &imageComp.position.x, 0.01f);
			ImGui::DragFloat("Width", &imageComp.width, 0.01f);
			ImGui::DragFloat("Height", &imageComp.height, 0.01f);
			ImGui::Checkbox("Maintain Aspect Ratio", &imageComp.maintainAspectRatio);
		}

		// Tint Color
		ImGui::ColorEdit3("Tint Color", &imageComp.tintColor.x);

		// Alpha
		ImGui::SliderFloat("Alpha", &imageComp.alpha, 0.0f, 1.0f);

		ImGui::Separator();
		ImGui::Text("Caption Settings");

		// Show Caption toggle
		ImGui::Checkbox("Show Caption", &imageComp.showCaption);

		if (imageComp.showCaption) {
			// Caption text (multiline)
			char captionBuffer[512];
			strncpy_s(captionBuffer, imageComp.caption.c_str(), sizeof(captionBuffer) - 1);
			captionBuffer[sizeof(captionBuffer) - 1] = '\0';
			if (ImGui::InputTextMultiline("Caption Text", captionBuffer, sizeof(captionBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3))) {
				imageComp.caption = captionBuffer;
			}

			// Caption color
			ImGui::ColorEdit3("Caption Color", &imageComp.captionColor.x);

			// Font size
			ImGui::DragFloat("Font Size", &imageComp.captionFontSize, 1.0f, 8.0f, 72.0f);

			// Caption position
			ImGui::DragFloat2("Caption Position", &imageComp.captionPosition.x, 0.01f, 0.0f, 1.0f);
		}

		ImGui::Separator();
		ImGui::DragInt("Render Order", &imageComp.renderOrder, 1, -100, 100);
		ImGui::TextDisabled("Lower = behind, Higher = on top");
	}

	void HierarchyInspector::DrawAddComponentMenu(EntityID entity)
	{
		auto& ecs = Ermine::ECS::GetInstance();

		// =========================
		// Search Bar
		// =========================
		static char searchBuffer[128] = "";
		ImGui::InputTextWithHint("##componentSearch", "Search components...", searchBuffer, sizeof(searchBuffer));

		// Convert string to lowercase
		auto toLower = [](const std::string& str) {
			std::string out = str;
			std::transform(out.begin(), out.end(), out.begin(), ::tolower);
			return out;
			};

		std::string searchLower = toLower(searchBuffer);

		auto matchSearch = [&](const char* name) {
			if (searchLower.empty()) return true;
			std::string nameLower = toLower(name);
			return nameLower.find(searchLower) != std::string::npos;
			};

		auto shouldShowMenu = [&](const char* menuName, const std::vector<const char*>& items) {
			if (matchSearch(menuName)) return true;
			for (auto& item : items)
				if (matchSearch(item)) return true;
			return false;
			};

		bool itemSelected = false; // Tracks if a component was added

		// =========================
		// Transform
		// =========================
		if (shouldShowMenu("Transform", { "Transform" }) && ImGui::BeginMenu("Transform"))
		{
			if (matchSearch("Transform") && ImGui::MenuItem("Transform") && !ecs.HasComponent<Transform>(entity))
			{
				ecs.AddComponent(entity, Transform());
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// Rendering
		// =========================
		if (shouldShowMenu("Rendering", { "Mesh","Material","Model","Animation","Light","Camera","Light Probe Volume" }) && ImGui::BeginMenu("Rendering"))
		{
			if (matchSearch("Mesh") && ImGui::MenuItem("Mesh") && !ecs.HasComponent<Mesh>(entity))
			{
				ecs.AddComponent(entity, graphics::GeometryFactory::CreateCube());
				ecs.AddComponent(entity, BuildDefaultWhiteMaterialComponent());
				itemSelected = true;
			}
			if (matchSearch("Material") && ImGui::MenuItem("Material") && !ecs.HasComponent<Material>(entity))
			{
				ecs.AddComponent(entity, BuildDefaultWhiteMaterialComponent());
				itemSelected = true;
			}
			if (matchSearch("Model") && ImGui::MenuItem("Model") && !ecs.HasComponent<ModelComponent>(entity))
			{
				ecs.AddComponent(entity, ModelComponent());
				itemSelected = true;
			}
			if (matchSearch("Animation") && ImGui::MenuItem("Animation") && !ecs.HasComponent<AnimationComponent>(entity))
			{
				ecs.AddComponent(entity, AnimationComponent());
				itemSelected = true;
			}
			if (matchSearch("Light") && ImGui::MenuItem("Light") && !ecs.HasComponent<Light>(entity))
			{
				ecs.AddComponent(entity, Light());
				itemSelected = true;
			}
			if (matchSearch("Camera") && ImGui::MenuItem("Camera") && !ecs.HasComponent<CameraComponent>(entity))
			{
				CameraComponent tempCam{};
				tempCam.fov = 45;
				tempCam.nearPlane = 5.0f;
				ecs.AddComponent(entity, tempCam);
				itemSelected = true;
			}
			if (matchSearch("Light Probe Volume") && ImGui::MenuItem("Light Probe Volume") && !ecs.HasComponent<LightProbeVolumeComponent>(entity))
			{
				ecs.AddComponent(entity, LightProbeVolumeComponent());
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// Physics
		// =========================
		if (shouldShowMenu("Physics", { "Physics" }) && ImGui::BeginMenu("Physics"))
		{
			if (matchSearch("Physics") && ImGui::MenuItem("Physics") && !ecs.HasComponent<PhysicComponent>(entity))
			{
				ecs.AddComponent(entity, PhysicComponent());
				ecs.GetSystem<Physics>()->UpdatePhysicList();
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// Audio
		// =========================
		if (shouldShowMenu("Audio", { "Audio" }) && ImGui::BeginMenu("Audio"))
		{
			if (matchSearch("Audio") && ImGui::MenuItem("Audio") && !ecs.HasComponent<AudioComponent>(entity))
			{
				ecs.AddComponent(entity, AudioComponent());
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// Gameplay
		// =========================
		if (shouldShowMenu("Gameplay", { "State Machine","Script" }) && ImGui::BeginMenu("Gameplay"))
		{
			if (matchSearch("State Machine") && ImGui::MenuItem("State Machine") && !ecs.HasComponent<StateMachine>(entity))
			{
				ecs.AddComponent(entity, StateMachine());
				auto& fsmComp = ecs.GetComponent<StateMachine>(entity);
				if (fsmComp.m_Nodes.empty())
				{
					auto defaultNode = std::make_shared<ScriptNode>();
					defaultNode->id = 0;
					defaultNode->name = "Start";
					defaultNode->isAttached = false;
					defaultNode->scriptClassName = "";
					defaultNode->isStartNode = true;
					fsmComp.m_Nodes.push_back(defaultNode);
				}
				fsmComp.Init(entity);
				itemSelected = true;
			}
			if (matchSearch("Script") && ImGui::MenuItem("Script"))
			{
				if (!ecs.HasComponent<ScriptsComponent>(entity))
					ecs.AddComponent(entity, ScriptsComponent{});
				ecs.GetComponent<ScriptsComponent>(entity).AddEmpty();
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// AI & Navigation
		// =========================
		if (shouldShowMenu("AI & Navigation", { "Nav Mesh","Nav Mesh Agent","Nav Jump Link" }) && ImGui::BeginMenu("AI & Navigation"))
		{
			if (matchSearch("Nav Mesh") && ImGui::MenuItem("Nav Mesh") && !ecs.HasComponent<NavMeshComponent>(entity))
			{
				ecs.AddComponent(entity, NavMeshComponent());
				itemSelected = true;
			}
			if (matchSearch("Nav Mesh Agent") && ImGui::MenuItem("Nav Mesh Agent") && !ecs.HasComponent<NavMeshAgent>(entity))
			{
				ecs.AddComponent(entity, NavMeshAgent());
				itemSelected = true;
			}
			if (matchSearch("Nav Jump Link") && ImGui::MenuItem("Nav Jump Link") && !ecs.HasComponent<NavJumpLink>(entity))
			{
				ecs.AddComponent(entity, NavJumpLink());
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// Particles
		// =========================
		if (shouldShowMenu("Particles", { "Particle Emitter", "GPU Particle Emitter" }) && ImGui::BeginMenu("Particles"))
		{
			if (matchSearch("Particle Emitter") && ImGui::MenuItem("Particle Emitter") && !ecs.HasComponent<ParticleEmitter>(entity))
			{
				ecs.AddComponent(entity, ParticleEmitter());
				itemSelected = true;
			}
			if (matchSearch("GPU Particle Emitter") && ImGui::MenuItem("Particle Emitter (GPU)") && !ecs.HasComponent<GPUParticleEmitter>(entity))
			{
				ecs.AddComponent(entity, GPUParticleEmitter());
				itemSelected = true;
			}
			ImGui::EndMenu();
		}

		// =========================
		// UI
		// =========================
		const std::vector<std::pair<const char*, std::function<void(EntityID)>>> uiComponents = {
			{"UI Component (Legacy)", [&](EntityID e) { ecs.AddComponent(e, UIComponent{}); }},
			{"UI Healthbar",         [&](EntityID e) { ecs.AddComponent(e, UIHealthbarComponent{}); }},
			{"UI Crosshair",         [&](EntityID e) { ecs.AddComponent(e, UICrosshairComponent{}); }},
			{"UI Skills",            [&](EntityID e) { ecs.AddComponent(e, UISkillsComponent{}); }},
			{"UI Mana Bar",          [&](EntityID e) { ecs.AddComponent(e, UIManaBarComponent{}); }},
			{"UI Book Counter",      [&](EntityID e) { ecs.AddComponent(e, UIBookCounterComponent{}); }},
			{"UI Image",             [&](EntityID e) { ecs.AddComponent(e, UIImageComponent{}); }},
			{"UI Button",            [&](EntityID e) { ecs.AddComponent(e, UIButtonComponent{}); }},
			{"UI Slider",            [&](EntityID e) { ecs.AddComponent(e, UISliderComponent{}); }},
			{"UI Text",              [&](EntityID e) { ecs.AddComponent(e, UITextComponent{}); }}
		};

		if (shouldShowMenu("UI", { "UI Component (Legacy)", "UI Healthbar","UI Crosshair",
								   "UI Skills","UI Mana Bar","UI Book Counter","UI Image","UI Button","UI Slider","UI Text" }) &&
			ImGui::BeginMenu("UI"))
		{
			for (auto& [name, addFunc] : uiComponents)
			{
				if (matchSearch(name) && ImGui::MenuItem(name))
				{
					addFunc(entity);
					itemSelected = true;
				}
			}
			ImGui::EndMenu();
		}

		// Reset search buffer if item was added
		if (itemSelected) searchBuffer[0] = '\0';

		ecs.ResyncAllSignaturesFromStorage();
	}

	void HierarchyInspector::DrawUIButtonComponent(EntityID entity)
{
	if (!ComponentHeaderWithRemove<UIButtonComponent>("UI Button Component", entity))
		return;

	auto& button = ECS::GetInstance().GetComponent<UIButtonComponent>(entity);

	// Button text
	char textBuffer[256];
	strncpy_s(textBuffer, button.text.c_str(), sizeof(textBuffer) - 1);
	textBuffer[sizeof(textBuffer) - 1] = '\0';
	if (ImGui::InputText("Button Text", textBuffer, sizeof(textBuffer))) {
		button.text = textBuffer;
	}

	// Position and size (UI is 2D, only X and Y needed)
	ImGui::DragFloat2("Position (X, Y)", &button.position.x, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat2("Size (Width, Height)", &button.size.x, 0.01f, 0.01f, 1.0f);

	ImGui::Separator();
	ImGui::Text("Colors");

	// Colors
	ImGui::ColorEdit3("Normal Color", &button.normalColor.x);
	ImGui::ColorEdit3("Hover Color", &button.hoverColor.x);
	ImGui::ColorEdit3("Pressed Color", &button.pressedColor.x);
	ImGui::ColorEdit3("Text Color", &button.textColor.x);
	ImGui::DragFloat("Text Scale", &button.textScale, 0.1f, 0.1f, 5.0f);
	ImGui::SliderFloat("Background Alpha", &button.backgroundAlpha, 0.0f, 1.0f);

	ImGui::Separator();
	ImGui::Text("Images (Optional)");
	ImGui::TextDisabled("If set, images override color-based rendering");

	char normalImageBuffer[256];
	strncpy_s(normalImageBuffer, button.normalImage.c_str(), sizeof(normalImageBuffer) - 1);
	normalImageBuffer[sizeof(normalImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Normal Image", normalImageBuffer, sizeof(normalImageBuffer))) {
		button.normalImage = normalImageBuffer;
	}

	char hoverImageBuffer[256];
	strncpy_s(hoverImageBuffer, button.hoverImage.c_str(), sizeof(hoverImageBuffer) - 1);
	hoverImageBuffer[sizeof(hoverImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Hover Image", hoverImageBuffer, sizeof(hoverImageBuffer))) {
		button.hoverImage = hoverImageBuffer;
	}

	char pressedImageBuffer[256];
	strncpy_s(pressedImageBuffer, button.pressedImage.c_str(), sizeof(pressedImageBuffer) - 1);
	pressedImageBuffer[sizeof(pressedImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Pressed Image", pressedImageBuffer, sizeof(pressedImageBuffer))) {
		button.pressedImage = pressedImageBuffer;
	}
	ImGui::TextDisabled("Example: ../Resources/Textures/UI/button.png");

	ImGui::Separator();
	ImGui::Text("Action");

	// Button action dropdown
	const char* actionNames[] = { "None", "Load Scene", "Quit", "Custom" };
	int currentAction = static_cast<int>(button.action);
	if (ImGui::Combo("Action", &currentAction, actionNames, IM_ARRAYSIZE(actionNames))) {
		button.action = static_cast<UIButtonComponent::ButtonAction>(currentAction);
	}

	// Action data (scene path or custom event)
	if (button.action == UIButtonComponent::ButtonAction::LoadScene)
	{
		char actionDataBuffer[256];
		strncpy_s(actionDataBuffer, button.actionData.c_str(), sizeof(actionDataBuffer) - 1);
		actionDataBuffer[sizeof(actionDataBuffer) - 1] = '\0';

		if (ImGui::InputText("Scene Path", actionDataBuffer, sizeof(actionDataBuffer))) {
			button.actionData = actionDataBuffer;
		}
		ImGui::TextDisabled("Example: ../Resources/Scenes/level.scene");
	}
	else if (button.action == UIButtonComponent::ButtonAction::Custom)
	{
		// Dropdown for known custom actions
		static const char* customActions[] = {
			"OpenControls",
			"CloseControlsScreen",
			"OpenSettings",
			"CloseSettings",
			"OpenSettingsPage",
			"CloseSettingsPage",
			"OpenSettingsAudio",
			"OpenSettingsControls",
			"OpenSettingsVideo",
			"BackToSettingsPage",
			"Resume",
			"ShowTeleportInfo",
			"ShowShootingInfo",
			"ShowReturnInfo",
			"ShowLightDInfo"
		};
		static const int numCustomActions = IM_ARRAYSIZE(customActions);

		// Find current selection index
		int currentCustomAction = -1;
		for (int i = 0; i < numCustomActions; ++i)
		{
			if (button.actionData == customActions[i])
			{
				currentCustomAction = i;
				break;
			}
		}

		// Show dropdown
		if (ImGui::Combo("Custom Action", &currentCustomAction, customActions, numCustomActions))
		{
			button.actionData = customActions[currentCustomAction];
		}

		// Also allow manual input for custom events not in the list
		char actionDataBuffer[256];
		strncpy_s(actionDataBuffer, button.actionData.c_str(), sizeof(actionDataBuffer) - 1);
		actionDataBuffer[sizeof(actionDataBuffer) - 1] = '\0';
		if (ImGui::InputText("Action (Manual)", actionDataBuffer, sizeof(actionDataBuffer))) {
			button.actionData = actionDataBuffer;
		}
		ImGui::TextDisabled("Use dropdown above or type a custom event name");
	}

	// Audio settings
	ImGui::Separator();
	ImGui::Text("Audio");

	char hoverSoundBuffer[256];
	strncpy_s(hoverSoundBuffer, button.hoverSoundName.c_str(), sizeof(hoverSoundBuffer) - 1);
	hoverSoundBuffer[sizeof(hoverSoundBuffer) - 1] = '\0';
	if (ImGui::InputText("Hover Sound", hoverSoundBuffer, sizeof(hoverSoundBuffer))) {
		button.hoverSoundName = hoverSoundBuffer;
	}
	ImGui::TextDisabled("Example: click.wav");

	char clickSoundBuffer[256];
	strncpy_s(clickSoundBuffer, button.clickSoundName.c_str(), sizeof(clickSoundBuffer) - 1);
	clickSoundBuffer[sizeof(clickSoundBuffer) - 1] = '\0';
	if (ImGui::InputText("Click Sound", clickSoundBuffer, sizeof(clickSoundBuffer))) {
		button.clickSoundName = clickSoundBuffer;
	}
	ImGui::TextDisabled("Example: button_click.wav");

	ImGui::SliderFloat("Sound Volume", &button.soundVolume, 0.0f, 1.0f);

	ImGui::Separator();
	ImGui::DragInt("Render Order", &button.renderOrder, 1, -100, 100);
	ImGui::TextDisabled("Lower = behind, Higher = on top");

	// Show button state (read-only)
	ImGui::Separator();
	ImGui::Text("State (Read-Only)");
	ImGui::Checkbox("Is Hovered", &button.isHovered);
	ImGui::SameLine();
	ImGui::Checkbox("Is Pressed", &button.isPressed);
}

void HierarchyInspector::DrawUISliderComponent(EntityID entity)
{
	if (!ComponentHeaderWithRemove<UISliderComponent>("UI Slider Component", entity))
		return;

	auto& slider = ECS::GetInstance().GetComponent<UISliderComponent>(entity);

	// Position and size
	ImGui::DragFloat2("Position (X, Y)", &slider.position.x, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat2("Size (Width, Height)", &slider.size.x, 0.01f, 0.01f, 1.0f);

	ImGui::Separator();
	ImGui::Text("Target");

	// Slider target dropdown
	const char* targetNames[] = { "None", "Master Volume", "Music Volume", "SFX Volume", "Ambience Volume", "Custom" };
	int currentTarget = static_cast<int>(slider.target);
	if (ImGui::Combo("Target", &currentTarget, targetNames, IM_ARRAYSIZE(targetNames))) {
		slider.target = static_cast<UISliderComponent::SliderTarget>(currentTarget);
	}

	// Custom target field
	if (slider.target == UISliderComponent::SliderTarget::Custom)
	{
		char customTargetBuffer[256];
		strncpy_s(customTargetBuffer, slider.customTarget.c_str(), sizeof(customTargetBuffer) - 1);
		customTargetBuffer[sizeof(customTargetBuffer) - 1] = '\0';
		if (ImGui::InputText("Custom Target", customTargetBuffer, sizeof(customTargetBuffer))) {
			slider.customTarget = customTargetBuffer;
		}
	}

	ImGui::Separator();
	ImGui::Text("Value");

	ImGui::DragFloat("Min Value", &slider.minValue, 0.01f, 0.0f, 1.0f);
	ImGui::DragFloat("Max Value", &slider.maxValue, 0.01f, 0.0f, 1.0f);
	ImGui::SliderFloat("Current Value", &slider.value, slider.minValue, slider.maxValue);

	ImGui::Separator();
	ImGui::Text("Colors");

	ImGui::ColorEdit3("Track Color", &slider.trackColor.x);
	ImGui::ColorEdit3("Fill Color", &slider.fillColor.x);
	ImGui::ColorEdit3("Handle Color", &slider.handleColor.x);
	ImGui::ColorEdit3("Handle Hover Color", &slider.handleHoverColor.x);
	ImGui::SliderFloat("Track Alpha", &slider.trackAlpha, 0.0f, 1.0f);
	ImGui::DragFloat("Handle Size", &slider.handleSize, 0.005f, 0.01f, 0.1f);

	ImGui::Separator();
	ImGui::Text("Images (Optional)");
	ImGui::TextDisabled("If set, images override color-based rendering");

	char trackImageBuffer[256];
	strncpy_s(trackImageBuffer, slider.trackImage.c_str(), sizeof(trackImageBuffer) - 1);
	trackImageBuffer[sizeof(trackImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Track Image", trackImageBuffer, sizeof(trackImageBuffer))) {
		slider.trackImage = trackImageBuffer;
	}

	char fillImageBuffer[256];
	strncpy_s(fillImageBuffer, slider.fillImage.c_str(), sizeof(fillImageBuffer) - 1);
	fillImageBuffer[sizeof(fillImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Fill Image", fillImageBuffer, sizeof(fillImageBuffer))) {
		slider.fillImage = fillImageBuffer;
	}

	char handleImageBuffer[256];
	strncpy_s(handleImageBuffer, slider.handleImage.c_str(), sizeof(handleImageBuffer) - 1);
	handleImageBuffer[sizeof(handleImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Handle Image", handleImageBuffer, sizeof(handleImageBuffer))) {
		slider.handleImage = handleImageBuffer;
	}
	ImGui::TextDisabled("Example: ../Resources/Textures/UI/slider_track.png");

	ImGui::Separator();
	ImGui::Text("Label");

	char labelBuffer[256];
	strncpy_s(labelBuffer, slider.label.c_str(), sizeof(labelBuffer) - 1);
	labelBuffer[sizeof(labelBuffer) - 1] = '\0';
	if (ImGui::InputText("Label Text", labelBuffer, sizeof(labelBuffer))) {
		slider.label = labelBuffer;
	}

	ImGui::ColorEdit3("Label Color", &slider.labelColor.x);
	ImGui::DragFloat("Label Scale", &slider.labelScale, 0.1f, 0.1f, 3.0f);
	ImGui::DragFloat2("Label Offset", &slider.labelOffset.x, 0.01f, -0.5f, 0.5f);

	// Label images (unselected/selected)
	char labelImageBuffer[256];
	strncpy_s(labelImageBuffer, slider.labelImagePath.c_str(), sizeof(labelImageBuffer) - 1);
	labelImageBuffer[sizeof(labelImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Label Image (Normal)", labelImageBuffer, sizeof(labelImageBuffer))) {
		slider.labelImagePath = labelImageBuffer;
	}

	char labelActiveImageBuffer[256];
	strncpy_s(labelActiveImageBuffer, slider.labelActiveImagePath.c_str(), sizeof(labelActiveImageBuffer) - 1);
	labelActiveImageBuffer[sizeof(labelActiveImageBuffer) - 1] = '\0';
	if (ImGui::InputText("Label Image (Active)", labelActiveImageBuffer, sizeof(labelActiveImageBuffer))) {
		slider.labelActiveImagePath = labelActiveImageBuffer;
	}

	// Value display settings
	ImGui::Separator();
	ImGui::Text("Value Display");

	ImGui::Checkbox("Show Value", &slider.showValue);
	ImGui::Checkbox("As Percentage", &slider.valueAsPercentage);
	ImGui::ColorEdit3("Value Color", &slider.valueColor.x);
	ImGui::DragFloat("Value Scale", &slider.valueScale, 0.1f, 0.1f, 3.0f);
	ImGui::DragFloat2("Value Offset", &slider.valueOffset.x, 0.01f, -1.0f, 1.0f);

	ImGui::Separator();
	ImGui::DragInt("Render Order", &slider.renderOrder, 1, -100, 100);
	ImGui::TextDisabled("Lower = behind, Higher = on top");

	// Show slider state (read-only)
	ImGui::Separator();
	ImGui::Text("State (Read-Only)");
	ImGui::Checkbox("Is Hovered", &slider.isHovered);
	ImGui::SameLine();
	ImGui::Checkbox("Is Dragging", &slider.isDragging);
}

void HierarchyInspector::DrawUITextComponent(EntityID entity)
{
	if (!ComponentHeaderWithRemove<UITextComponent>("UI Text Component", entity))
		return;

	auto& textComp = ECS::GetInstance().GetComponent<UITextComponent>(entity);

	// Text (multiline)
	char textBuffer[1024];
	strncpy_s(textBuffer, textComp.text.c_str(), sizeof(textBuffer) - 1);
	textBuffer[sizeof(textBuffer) - 1] = '\0';
	if (ImGui::InputTextMultiline("Text", textBuffer, sizeof(textBuffer), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4))) {
		textComp.text = textBuffer;
	}

	// Position
	ImGui::DragFloat2("Position (X, Y)", &textComp.position.x, 0.01f, 0.0f, 1.0f);

	// Font size
	ImGui::DragFloat("Font Size", &textComp.fontSize, 0.1f, 0.1f, 10.0f);

	// Color
	ImGui::ColorEdit3("Color", &textComp.color.x);

	// Alpha
	ImGui::SliderFloat("Alpha", &textComp.alpha, 0.0f, 1.0f);

	// Alignment
	const char* alignmentNames[] = { "Left", "Center", "Right" };
	ImGui::Combo("Alignment", &textComp.alignment, alignmentNames, IM_ARRAYSIZE(alignmentNames));

	ImGui::Separator();
	ImGui::DragInt("Render Order", &textComp.renderOrder, 1, -100, 100);
	ImGui::TextDisabled("Lower = behind, Higher = on top");
}

} // namespace Ermine::editor
