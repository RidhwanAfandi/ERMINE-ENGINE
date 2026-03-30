/* Start Header ************************************************************************/
/*!
\file       Renderer.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\co-author  Ridhwan Afandi, moahamedridhwan.b, 2301367, moahamedridhwan.b\@digipen.edu
\co-author  Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       09/27/2025
\brief      This file contains the definition of the Renderer system.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Renderer.h"
#include "Material.h"
#include "SSBO_Bindings.h"
#include "CameraSystem.h"
#include "EditorGUI.h"
#include "FrameController.h"

#include <algorithm>
#include <numeric> // For std::iota

#include "ECS.h"
#include "Logger.h"
#include "MathUtils.h"
#include "Matrix3x3.h"
#include "Components.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/glm.hpp"
#include "Input.h"
#include "GeometryFactory.h"
#include "AssetManager.h"
#include "Skybox.h"
#include "SceneManager.h"
#include <random>  
#include <filesystem>
#include <fstream>
#include "Physics.h"
#include "NavMesh.h"
#include "GISystem.h"
#include "GPUParticles.h"

namespace {
	constexpr uint32_t kProbeFileMagic = 0x49475245u; // 'ERGI'
	constexpr uint32_t kProbeFileVersion = 1;

	bool SaveProbeSHToFile(const std::filesystem::path& path, const glm::vec3 sh[9])
	{
		std::ofstream out(path, std::ios::binary);
		if (!out.is_open())
			return false;

		out.write(reinterpret_cast<const char*>(&kProbeFileMagic), sizeof(kProbeFileMagic));
		out.write(reinterpret_cast<const char*>(&kProbeFileVersion), sizeof(kProbeFileVersion));
		out.write(reinterpret_cast<const char*>(sh), sizeof(glm::vec3) * 9);
		return out.good();
	}

	bool LoadProbeSHFromFile(const std::filesystem::path& path, glm::vec3 sh[9])
	{
		std::ifstream in(path, std::ios::binary);
		if (!in.is_open())
			return false;

		uint32_t magic = 0;
		uint32_t version = 0;
		in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		in.read(reinterpret_cast<char*>(&version), sizeof(version));
		if (!in.good() || magic != kProbeFileMagic || version != kProbeFileVersion)
			return false;

		in.read(reinterpret_cast<char*>(sh), sizeof(glm::vec3) * 9);
		return in.good();
	}
} // namespace
#include "AnimationManager.h"
#include "DrawCommands.h"

#include <GLFW/glfw3.h>

#include "SceneManager.h"
#include "Selection.h"

using namespace Ermine::graphics;

unsigned int SHADOW_MAX_LAYERS = SHADOW_MAX_LAYERS_DESIRED;

/**
 * @brief Checks for OpenGL errors and logs them.
 * @param file Source file name.
 * @param line Line number.
 * @return GLenum The last OpenGL error code.
 */
GLenum glCheckError_(const char* file, int line)
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR)
	{
		std::string error;
		switch (errorCode)
		{
		case GL_INVALID_ENUM:                  error = "INVALID_ENUM";  break;
		case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
		}
		EE_CORE_ERROR("{0} | {1} ({2})", error, file, line);
		assert(false && "Check logs");
	}
	return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

namespace {
	inline bool IsFiniteVec3(const glm::vec3& v)
	{
		return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
	}

	inline std::string ResolveCustomVertexShaderPath(const std::string& fragmentPath)
	{
		constexpr const char* kDefaultVertex = "../Resources/Shaders/vertex.glsl";
		if (fragmentPath.empty()) {
			return kDefaultVertex;
		}

		std::filesystem::path fragmentFile(fragmentPath);
		std::string vertexName = fragmentFile.filename().string();

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

	inline glm::vec3 ExtractWorldPosition(const glm::mat4& worldMatrix)
	{
		return glm::vec3(worldMatrix[3]);
	}

	inline glm::vec3 ExtractWorldForward(const glm::mat4& worldMatrix)
	{
		const glm::vec3 forward = glm::vec3(worldMatrix[2]);
		const float lenSq = glm::dot(forward, forward);
		if (!IsFiniteVec3(forward) || lenSq <= 1e-8f) {
			return glm::vec3(0.0f, 0.0f, 1.0f);
		}
		return forward * glm::inversesqrt(lenSq);
	}

	inline glm::mat3 ExtractWorldRotationNoScale(const glm::mat4& worldMatrix)
	{
		glm::vec3 x = glm::vec3(worldMatrix[0]);
		glm::vec3 y = glm::vec3(worldMatrix[1]);
		glm::vec3 z = glm::vec3(worldMatrix[2]);

		if (!IsFiniteVec3(x) || !IsFiniteVec3(y) || !IsFiniteVec3(z)) {
			return glm::mat3(1.0f);
		}

		const float xLenSq = glm::dot(x, x);
		const float yLenSq = glm::dot(y, y);
		const float zLenSq = glm::dot(z, z);
		if (xLenSq <= 1e-8f || yLenSq <= 1e-8f || zLenSq <= 1e-8f) {
			return glm::mat3(1.0f);
		}

		x *= glm::inversesqrt(xLenSq);
		y = y - x * glm::dot(y, x);
		const float yOrthoLenSq = glm::dot(y, y);
		if (yOrthoLenSq <= 1e-8f) {
			return glm::mat3(1.0f);
		}
		y *= glm::inversesqrt(yOrthoLenSq);

		glm::vec3 zOrtho = glm::cross(x, y);
		const float zOrthoLenSq = glm::dot(zOrtho, zOrtho);
		if (zOrthoLenSq <= 1e-8f) {
			return glm::mat3(1.0f);
		}
		zOrtho *= glm::inversesqrt(zOrthoLenSq);

		if (glm::dot(zOrtho, z) < 0.0f) {
			zOrtho = -zOrtho;
		}

		return glm::mat3(x, y, zOrtho);
	}

	inline glm::mat4 BuildSkinnedModelMatrix(const glm::mat4& worldMatrix, const glm::vec3& localScale)
	{
		glm::vec3 safeScale = localScale;
		if (!IsFiniteVec3(safeScale)) {
			safeScale = glm::vec3(1.0f);
		}

		glm::mat4 model(1.0f);
		model = glm::translate(model, ExtractWorldPosition(worldMatrix));
		model *= glm::mat4(ExtractWorldRotationNoScale(worldMatrix));
		model = glm::scale(model, safeScale);
		return model;
	}

	bool ComputeSkinnedMeshAABB(const Ermine::graphics::MeshData& mesh,
		const std::vector<glm::mat4>& boneTransforms,
		glm::vec3& outMin,
		glm::vec3& outMax)
	{
		if (boneTransforms.empty()) {
			return false;
		}

		if (mesh.boneAabbMin.size() == boneTransforms.size()
			&& mesh.boneAabbMax.size() == boneTransforms.size()
			&& !mesh.boneAabbMin.empty()) {
			glm::vec3 aabbMin(FLT_MAX);
			glm::vec3 aabbMax(-FLT_MAX);
			bool any = false;

			for (size_t i = 0; i < boneTransforms.size(); ++i) {
				if (!mesh.boneAabbValid.empty() && mesh.boneAabbValid[i] == 0) {
					continue;
				}

				glm::vec3 localMin = mesh.boneAabbMin[i];
				glm::vec3 localMax = mesh.boneAabbMax[i];
				glm::vec3 center = (localMin + localMax) * 0.5f;
				glm::vec3 extent = (localMax - localMin) * 0.5f;

				const glm::mat4& boneTransform = boneTransforms[i];
				glm::vec3 worldCenter = glm::vec3(boneTransform * glm::vec4(center, 1.0f));
				glm::mat3 upperLeft = glm::mat3(boneTransform);
				glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
					+ glm::abs(upperLeft[1]) * extent.y
					+ glm::abs(upperLeft[2]) * extent.z;

				glm::vec3 boneMin = worldCenter - worldExtent;
				glm::vec3 boneMax = worldCenter + worldExtent;

				aabbMin = glm::min(aabbMin, boneMin);
				aabbMax = glm::max(aabbMax, boneMax);
				any = true;
			}

			if (any) {
				if (!IsFiniteVec3(aabbMin) || !IsFiniteVec3(aabbMax)) {
					return false;
				}
				outMin = aabbMin;
				outMax = aabbMax;
				return true;
			}
		}

		if (!mesh.vbo) {
			return false;
		}

		const auto* vertexData = reinterpret_cast<const Ermine::graphics::VertexData*>(mesh.vbo->GetDataPointer());
		if (!vertexData) {
			return false;
		}

		const unsigned int numVertices = mesh.vbo->GetSize() / sizeof(Ermine::graphics::VertexData);
		if (numVertices == 0) {
			return false;
		}

		glm::vec3 aabbMin(FLT_MAX);
		glm::vec3 aabbMax(-FLT_MAX);
		bool anyWeighted = false;

		for (unsigned int i = 0; i < numVertices; ++i) {
			const Ermine::graphics::VertexData& v = vertexData[i];
			glm::vec4 skinnedPos(0.0f);

			for (int j = 0; j < Ermine::graphics::MAX_BONE_INFLUENCE; ++j) {
				const int boneID = v.IDs[j];
				const float weight = v.Weights[j];

				if (weight <= 0.0f) {
					continue;
				}
				anyWeighted = true;

				if (boneID < 0 || boneID >= static_cast<int>(boneTransforms.size())) {
					continue;
				}

				skinnedPos += boneTransforms[boneID]
					* glm::vec4(v.position[0], v.position[1], v.position[2], 1.0f)
					* weight;
			}

			const glm::vec3 pos(skinnedPos);
			aabbMin = glm::min(aabbMin, pos);
			aabbMax = glm::max(aabbMax, pos);
		}

		if (!anyWeighted) {
			return false;
		}

		if (!IsFiniteVec3(aabbMin) || !IsFiniteVec3(aabbMax)) {
			return false;
		}

		outMin = aabbMin;
		outMax = aabbMax;
		return true;
	}
}

void Renderer::InitializeShadowMapResources()
{
	if (m_ShadowMapArrayHandle != 0) {
		glMakeTextureHandleNonResidentARB(m_ShadowMapArrayHandle);
		m_ShadowMapArrayHandle = 0;
	}

	if (m_ShadowMapArray != 0) {
		glDeleteTextures(1, &m_ShadowMapArray);
		m_ShadowMapArray = 0;
	}

	if (m_ShadowMapFBO != 0) {
		glDeleteFramebuffers(1, &m_ShadowMapFBO);
		m_ShadowMapFBO = 0;
	}

	// Clear any OpenGL errors from cleanup
	while (glGetError() != GL_NO_ERROR);

	InitializeShadowMap();
	CreateShadowMapArray();


	EE_CORE_INFO("Shadow map initialized: FBO={0}, Texture={1}, Layers={2}",
		m_ShadowMapFBO, m_ShadowMapArray, SHADOW_MAX_LAYERS);
}

/**
 * @brief Initializes the renderer and its resources.
 * @param screenWidth Width of the screen.
 * @param screenHeight Height of the screen.
 */
void Renderer::Init(const int& screenWidth, const int& screenHeight)
{
	// Check for ARB_bindless_texture support
	if (!glfwExtensionSupported("GL_ARB_bindless_texture") || !GL_ARB_bindless_texture)
	{
		EE_CORE_WARN("GL_ARB_bindless_texture not supported. Deferred rendering will be disabled.");
		m_UseDeferredRendering = false;
	}
	// Create a fullscreen quad for rendering the offscreen buffer to the screen

	// Initialize MeshManager for centralized mesh storage and indirect rendering
	m_MeshManager.Initialize();

	// Connect AnimationManager to SkeletalSSBO for efficient bone transform updates
	auto animationManager = Ermine::ECS::GetInstance().GetSystem<AnimationManager>();
	if (animationManager)
	{
		animationManager->SetSkeletalSSBO(&m_MeshManager.m_SkeletalSSBO);
	}

	// Add light system reference
	m_LightSystem = Ermine::ECS::GetInstance().GetSystem<LightSystem>();
	m_ModelSystem = Ermine::ECS::GetInstance().GetSystem<ModelSystem>();
	m_MaterialSystem = Ermine::ECS::GetInstance().GetSystem<MaterialSystem>();

	m_QuadMesh = GeometryFactory::CreateQuad(2.0f, 2.0f);

	// Load deferred shading shaders
	m_ShadowMapInstancedShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/shadowmap_instanced_vertex.glsl", "../Resources/Shaders/shadowmap_fragment.glsl");
	m_DepthPrePassShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/depth_prepass_vertex.glsl", "../Resources/Shaders/depth_prepass_fragment.glsl");
	m_GBufferShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/gBuffer_vertex.glsl", "../Resources/Shaders/gBuffer_fragment.glsl");
	m_LightPassShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/lighting_vertex.glsl", "../Resources/Shaders/lighting_fragment.glsl");
	m_BloomShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/bloom_vertex.glsl", "../Resources/Shaders/bloom_fragment.glsl");
	m_PostProcessShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/postprocess_vertex.glsl", "../Resources/Shaders/postprocess_fragment.glsl");
	m_AAShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/FXAA_vertex.glsl", "../Resources/Shaders/FXAA_fragment.glsl");
	m_MotionBlurShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/motionblur_vertex.glsl", "../Resources/Shaders/motionblur_fragment.glsl");
	m_ProbeBakeComputeShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/gi_probe_bake_compute.glsl");
	m_ProbeVoxelizeComputeShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/gi_probe_voxelize_compute.glsl");
	m_ProbeLightInjectComputeShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/gi_probe_light_inject_compute.glsl");
	// Load forward rendering shader for transparent objects
	m_ForwardShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/vertex.glsl", "../Resources/Shaders/fragment_enhanced.glsl");
	if (!m_ForwardShader || !m_ForwardShader->IsValid()) {
		EE_CORE_WARN("Forward shader for transparency not loaded, using enhanced fragment shader from assets");
		// Fallback to the existing enhanced fragment shader
		m_ForwardShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/vertex.glsl", "../Resources/Shaders/fragment_enhanced.glsl");
	}
	// Create initial g-buffer
	CreateGBuffer(screenWidth, screenHeight);
	CreatePostProcessBuffer(screenWidth, screenHeight);

	// Create shadow map FBO and texture
	InitializeShadowMapResources();

	// Create outline mask buffer for select outline
#ifdef EE_EDITOR
	CreateOutlineMaskBuffer(screenWidth, screenHeight);

	m_OutlineMaskIndirectShader = AssetManager::GetInstance().LoadShader(
		"../Resources/Shaders/outline_mask_indirect_vertex.glsl",
		"../Resources/Shaders/outline_mask_fragment.glsl");

	m_OutlineMaskIndirectSkinnedShader = AssetManager::GetInstance().LoadShader(
		"../Resources/Shaders/outline_mask_indirect_skinned_vertex.glsl",
		"../Resources/Shaders/outline_mask_fragment.glsl");
#endif

	// Setup shadow VAOs for shadow pass rendering
	m_MeshManager.SetupShadowVAOs();

	m_PickingShader = AssetManager::GetInstance().LoadShader(
		"../Resources/Shaders/picking_vertex.glsl",
		"../Resources/Shaders/picking_fragment_uint.glsl"
	);

	// Load indirect rendering picking shaders
	m_PickingIndirectShader = AssetManager::GetInstance().LoadShader(
		"../Resources/Shaders/picking_indirect_vertex.glsl",
		"../Resources/Shaders/picking_indirect_fragment.glsl"
	);

	m_PickingIndirectSkinnedShader = AssetManager::GetInstance().LoadShader(
		"../Resources/Shaders/picking_indirect_skinned_vertex.glsl",
		"../Resources/Shaders/picking_indirect_fragment.glsl"
	);

	CreatePickingBuffer(screenWidth, screenHeight);

	m_MaterialsDirty = true;

	GenerateIGNTexture();

	// Initialize light probe capture resources
	InitializeProbeCaptureResources();
}

/**
 * @brief Submits a debug line to be rendered in the scene.
 * @param from Starting point of the line in world space.
 * @param to Ending point of the line in world space.
 * @param color RGB color of the line.
 */
void Renderer::SubmitDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color)
{
	m_DebugLines.push_back({ from, color });
	m_DebugLines.push_back({ to, color });
}

void Renderer::SubmitDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color)
{
	// Draw 12 edges of the AABB
	// Bottom face (4 edges)
	SubmitDebugLine(glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z), color);
	SubmitDebugLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, min.y, max.z), color);
	SubmitDebugLine(glm::vec3(max.x, min.y, max.z), glm::vec3(min.x, min.y, max.z), color);
	SubmitDebugLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, min.y, min.z), color);

	// Top face (4 edges)
	SubmitDebugLine(glm::vec3(min.x, max.y, min.z), glm::vec3(max.x, max.y, min.z), color);
	SubmitDebugLine(glm::vec3(max.x, max.y, min.z), glm::vec3(max.x, max.y, max.z), color);
	SubmitDebugLine(glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z), color);
	SubmitDebugLine(glm::vec3(min.x, max.y, max.z), glm::vec3(min.x, max.y, min.z), color);

	// Vertical edges (4 edges)
	SubmitDebugLine(glm::vec3(min.x, min.y, min.z), glm::vec3(min.x, max.y, min.z), color);
	SubmitDebugLine(glm::vec3(max.x, min.y, min.z), glm::vec3(max.x, max.y, min.z), color);
	SubmitDebugLine(glm::vec3(max.x, min.y, max.z), glm::vec3(max.x, max.y, max.z), color);
	SubmitDebugLine(glm::vec3(min.x, min.y, max.z), glm::vec3(min.x, max.y, max.z), color);
}

void Renderer::SubmitDebugFrustum(const Frustum& frustum, const glm::mat4& invViewProj, const glm::vec3& color)
{
	// Get the 8 corners of the frustum directly from inverse view-projection
	// This is more reliable than computing plane intersections
	auto corners = frustum.GetCorners(invViewProj);

	// Corner indices: 0=nearBL, 1=nearBR, 2=nearTR, 3=nearTL, 4=farBL, 5=farBR, 6=farTR, 7=farTL

	// Draw near plane (4 edges)
	SubmitDebugLine(corners[0], corners[1], color); // bottom
	SubmitDebugLine(corners[1], corners[2], color); // right
	SubmitDebugLine(corners[2], corners[3], color); // top
	SubmitDebugLine(corners[3], corners[0], color); // left

	// Draw far plane (4 edges)
	SubmitDebugLine(corners[4], corners[5], color); // bottom
	SubmitDebugLine(corners[5], corners[6], color); // right
	SubmitDebugLine(corners[6], corners[7], color); // top
	SubmitDebugLine(corners[7], corners[4], color); // left

	// Draw connecting edges (4 edges from near to far)
	SubmitDebugLine(corners[0], corners[4], color); // bottom-left
	SubmitDebugLine(corners[1], corners[5], color); // bottom-right
	SubmitDebugLine(corners[2], corners[6], color); // top-right
	SubmitDebugLine(corners[3], corners[7], color); // top-left
}

/**
 * @brief Renders all submitted debug lines using the provided view and projection matrices.
 * @param view View matrix for the camera.
 * @param proj Projection matrix for the camera.
 */
void Renderer::RenderDebugLines(const glm::mat4& view, const glm::mat4& proj)
{
	if (m_DebugLines.empty()) return;

	// 1) Create VAO/VBO once
	if (m_DebugVAO == 0) {
		glGenVertexArrays(1, &m_DebugVAO);
		glGenBuffers(1, &m_DebugVBO);

		glBindVertexArray(m_DebugVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_DebugVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(DebugVertex) * 65536, nullptr, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0); // position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
			(void*)offsetof(DebugVertex, position));
		glEnableVertexAttribArray(1); // color
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
			(void*)offsetof(DebugVertex, color));
		glBindVertexArray(0);
	}

	// 2) Load shader once (make sure it�s valid)
	if (!debugShader) {
		debugShader = AssetManager::GetInstance().LoadShader(
			"../Resources/Shaders/debug_line_vert.glsl",
			"../Resources/Shaders/debug_line_frag.glsl"
		);
		if (!debugShader || !debugShader->IsValid()) {
			m_DebugLines.clear();
			return; // avoid Bind() on null
		}
	}

	// 3) Bind VAO/VBO and upload THIS FRAME�S data
	glBindVertexArray(m_DebugVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_DebugVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
		(GLsizeiptr)(m_DebugLines.size() * sizeof(DebugVertex)),
		m_DebugLines.data());

	// 4) Shader + uniforms
	debugShader->Bind();
	debugShader->SetUniformMatrix4fv("uView", view);
	debugShader->SetUniformMatrix4fv("uProj", proj);

	// 5) States (depth to taste)
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glLineWidth(2.0f);

	// 6) Draw and cleanup
	glDrawArrays(GL_LINES, 0, (GLsizei)m_DebugLines.size());
	glBindVertexArray(0);
	m_DebugLines.clear();
}

void Renderer::SubmitDebugTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color)
{
	m_DebugTriangleVertices.push_back({ a, color });
	m_DebugTriangleVertices.push_back({ b, color });
	m_DebugTriangleVertices.push_back({ c, color });
}

void Renderer::RenderDebugTriangles(const Mtx44& view, const Mtx44& proj)
{
	if (m_DebugTriangleVertices.empty()) return;

	if (!debugShader)
	{
		debugShader = AssetManager::GetInstance().LoadShader(
			"../Resources/Shaders/debug_line_vert.glsl",
			"../Resources/Shaders/debug_line_frag.glsl"
		);
		if (!debugShader || !debugShader->IsValid())
		{
			m_DebugTriangleVertices.clear();
			return;
		}
	}

	debugShader->Bind();
	debugShader->SetUniformMatrix4fv("uView", view);
	debugShader->SetUniformMatrix4fv("uProj", proj);
	debugShader->SetUniform1f("u_Alpha", 0.35f);

	if (m_DebugVAO == 0)
	{
		glGenVertexArrays(1, &m_DebugVAO);
		glGenBuffers(1, &m_DebugVBO);

		glBindVertexArray(m_DebugVAO);
		glBindBuffer(GL_ARRAY_BUFFER, m_DebugVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(DebugVertex) * 65536, nullptr, GL_DYNAMIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, position));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, color));
		glBindVertexArray(0);
	}

	glBindVertexArray(m_DebugVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_DebugVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
		(GLsizeiptr)(m_DebugTriangleVertices.size() * sizeof(DebugVertex)),
		m_DebugTriangleVertices.data());

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_DebugTriangleVertices.size());

	glBindVertexArray(0);
	debugShader->Unbind();

	m_DebugTriangleVertices.clear();

	glDisable(GL_BLEND);
}

/**
 * @brief Create an offscreen buffer for viewport/scene rendering
 * @param width The width of the offscreen buffer
 * @param height The height of the offscreen buffer
 * @return OffscreenBuffer The offscreen buffer
 */
void Renderer::EnsureLightsSSBO(size_t requiredLightCount)
{
	if (!m_LightsSSBO)
	{
		glGenBuffers(1, &m_LightsSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightsSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, LIGHT_SSBO_BINDING, m_LightsSSBO);
		EE_CORE_INFO("Created Lights SSBO at binding point {0}", LIGHT_SSBO_BINDING);
	}
	else
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightsSSBO);
	}

	BindLightsSSBO();

	if (requiredLightCount > m_LightsSSBOCapacity || m_LightsSSBOCapacity == 0)
	{
		const GLsizeiptr headerSize = static_cast<GLsizeiptr>(sizeof(glm::vec4));
		const GLsizeiptr bodySize = static_cast<GLsizeiptr>(requiredLightCount * sizeof(LightGPU));
		glBufferData(GL_SHADER_STORAGE_BUFFER, headerSize + bodySize, nullptr, GL_DYNAMIC_DRAW);
		m_LightsSSBOCapacity = requiredLightCount;
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glCheckError();
}

void Renderer::BindLightsSSBO() const
{
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, LIGHT_SSBO_BINDING, m_LightsSSBO);
}

Renderer::OffscreenBuffer Renderer::CreateOffscreenBuffer(const int& width, const int& height)
{
	OffscreenBuffer buffer{};

	// If an offscreen buffer already exists, delete its OpenGL resources before creating a new one.
	if (m_OffscreenBuffer)
	{
		glDeleteFramebuffers(1, &m_OffscreenBuffer->FBO);
		glDeleteTextures(1, &m_OffscreenBuffer->ColorTexture);
		glDeleteRenderbuffers(1, &m_OffscreenBuffer->RBO);
	}

	EnsureLightsSSBO(0);

	// Create FBO
	glGenFramebuffers(1, &buffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, buffer.FBO);

	// Check for errors
	glCheckError();

	// Create color texture
	glGenTextures(1, &buffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, buffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffer.ColorTexture, 0);

	// Check for errors
	glCheckError();

	// Create RBO for depth & stencil attachment
	glGenRenderbuffers(1, &buffer.RBO);
	glBindRenderbuffer(GL_RENDERBUFFER, buffer.RBO);

	// Making sure dimensions are non-zero
	if (width <= 0 || height <= 0)
	{
		EE_CORE_ERROR("ERROR: Invalid framebuffer dimensions: {0}x{1}", width, height);
	}
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);
	glBindRenderbuffer(GL_RENDERBUFFER, buffer.RBO);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, buffer.RBO);

	// Check for errors
	glCheckError();

	// Explicitly specify draw buffer
	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	// Check overall framebuffer completeness
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status)
		{
		case GL_FRAMEBUFFER_UNDEFINED:
			EE_CORE_ERROR("ERROR: Framebuffer is undefined!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete attachment!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			EE_CORE_ERROR("ERROR: Framebuffer missing attachment!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete draw buffer!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete read buffer!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			EE_CORE_ERROR("ERROR: Framebuffer unsupported!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete multisample!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete layer targets!");
			break;
		default:
			EE_CORE_ERROR("ERROR: Framebuffer unknown error!");
			break;
		}
		EE_CORE_FATAL("Framebuffer Failed!!!");
		assert(false && "Check logs");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	buffer.width = width;
	buffer.height = height;
	m_OffscreenBuffer = std::make_shared<OffscreenBuffer>(buffer);
	return buffer;
}

/**
 * @brief Resize the offscreen buffer to new dimensions without recreating the FBO
 * @param width New width
 * @param height New height
 */
void Renderer::ResizeOffscreenBuffer(const int& width, const int& height)
{
	if (!m_OffscreenBuffer)
	{
		CreateOffscreenBuffer(width, height);
		return;
	}

	if (m_OffscreenBuffer->width == width && m_OffscreenBuffer->height == height)
		return;

	// Resize color texture
	glBindTexture(GL_TEXTURE_2D, m_OffscreenBuffer->ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// Resize depth-stencil renderbuffer
	glBindRenderbuffer(GL_RENDERBUFFER, m_OffscreenBuffer->RBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);

	// Validate framebuffer completeness after resize
	glBindFramebuffer(GL_FRAMEBUFFER, m_OffscreenBuffer->FBO);
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		EE_CORE_ERROR("ERROR: Offscreen framebuffer not complete after resize! Status: {0}", status);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_OffscreenBuffer->width = width;
	m_OffscreenBuffer->height = height;

	glCheckError();

	ResizePickingBuffer(width, height);
}

/**
 * @brief Create optimized g-buffer for deferred rendering using scalar materials and emissive
 * RT0: RGBA16F (48 bits) - Albedo RGB
 * RT1: RGB16F (48 bits) - Normals XYZ
 * RT2: RGBA8 (32 bits) - Emissive RGB + Intensity
 * RT3: RGBA8 (32 bits) - Material properties (R: Roughness, G: Metallic, B: AO, A: Unused)
 * Total: 160 bits per pixel
 */
void Renderer::CreateGBuffer(const int& width, const int& height)
{
	// Clean up existing g-buffer if it exists
	CleanupGBuffer();

	GBuffer gBuffer{};
	gBuffer.width = width;
	gBuffer.height = height;

	// Validate dimensions
	if (width <= 0 || height <= 0)
	{
		EE_CORE_ERROR("ERROR: Invalid G-Buffer dimensions: {0}x{1}", width, height);
	}

	EnsureLightsSSBO(0);

	// Create framebuffer
	glGenFramebuffers(1, &gBuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer.FBO);

	// Create RT0 Texture: RGBA8 (32 bits) - Albedo RGB (albedo is always [0,1], 8bpc is sufficient)
	glGenTextures(1, &gBuffer.PackedTexture0);
	glBindTexture(GL_TEXTURE_2D, gBuffer.PackedTexture0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBuffer.PackedTexture0, 0);

	// Create RT1 Texture: RG16F (32 bits) - Normals oct-encoded (unit vector has 2 DOF, saves 16 bits vs RGB16F)
	glGenTextures(1, &gBuffer.PackedTexture1);
	glBindTexture(GL_TEXTURE_2D, gBuffer.PackedTexture1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gBuffer.PackedTexture1, 0);

	// Create RT2 Texture: RGBA8 (32 bits) - Emissive RGB + Intensity
	glGenTextures(1, &gBuffer.PackedTexture2);
	glBindTexture(GL_TEXTURE_2D, gBuffer.PackedTexture2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gBuffer.PackedTexture2, 0);

	// Create RT3 Texture: RGBA8 (32 bits) - Material properties (R: Roughness, G: Metallic, B: AO, A: Unused)
	glGenTextures(1, &gBuffer.PackedTexture3);
	glBindTexture(GL_TEXTURE_2D, gBuffer.PackedTexture3);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gBuffer.PackedTexture3, 0);

	// Create RT4 Texture: RG16F (32 bits) - Screen-space velocity XY
	// Savings from RT0 (48→32) + RT1 (48→32) = 32 bits freed, exactly covering this new buffer
	glGenTextures(1, &gBuffer.PackedTexture4);
	glBindTexture(GL_TEXTURE_2D, gBuffer.PackedTexture4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gBuffer.PackedTexture4, 0);

	// Create depth texture: 32-bit float required for near=0.1/far=1000 range (10000:1 ratio)
	glGenTextures(1, &gBuffer.DepthTexture);
	glBindTexture(GL_TEXTURE_2D, gBuffer.DepthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gBuffer.DepthTexture, 0);

	// Set up MRTs - 5 color attachments (RT0-RT3 + RT4 velocity)
	GLenum drawBuffers[5] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers(5, drawBuffers);

	// Check framebuffer completeness
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		EE_CORE_ERROR("ERROR: G-Buffer framebuffer not complete! Status: {0}", status);
		CleanupGBuffer();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCheckError();

	// Get the texture handles for the deferred lighting textures created above
	gBuffer.HandlePackedTexture0 = glGetTextureHandleARB(gBuffer.PackedTexture0);
	glMakeTextureHandleResidentARB(gBuffer.HandlePackedTexture0);

	gBuffer.HandlePackedTexture1 = glGetTextureHandleARB(gBuffer.PackedTexture1);
	glMakeTextureHandleResidentARB(gBuffer.HandlePackedTexture1);

	gBuffer.HandlePackedTexture2 = glGetTextureHandleARB(gBuffer.PackedTexture2);
	glMakeTextureHandleResidentARB(gBuffer.HandlePackedTexture2);

	gBuffer.HandlePackedTexture3 = glGetTextureHandleARB(gBuffer.PackedTexture3);
	glMakeTextureHandleResidentARB(gBuffer.HandlePackedTexture3);

	gBuffer.HandlePackedTexture4 = glGetTextureHandleARB(gBuffer.PackedTexture4);
	glMakeTextureHandleResidentARB(gBuffer.HandlePackedTexture4);

	gBuffer.HandleDepthTexture = glGetTextureHandleARB(gBuffer.DepthTexture);
	glMakeTextureHandleResidentARB(gBuffer.HandleDepthTexture);

	m_GBuffer = std::make_shared<GBuffer>(gBuffer);

	// RT0: RGBA8(32) + RT1: RG16F(32) + RT2: RGBA8(32) + RT3: RGBA8(32) + RT4 velocity: RG16F(32) + Depth: 32F(32) = 192 bits
	EE_CORE_INFO("Created G-Buffer: {0}x{1}, 192 bits per pixel (velocity buffer included at same cost as before)", width, height);
}

/**
 * @brief Create an offscreen buffer for viewport/scene rendering
 * @param width The width of the offscreen buffer
 * @param height The height of the offscreen buffer
 * @return OffscreenBuffer The offscreen buffer
 */
void Renderer::CreatePostProcessBuffer(const int& width, const int& height)
{
	PostProcessBuffer pPBuffer, bEBuffer, bBBuffer1, bBBuffer2, AABuffer, MBBuffer, MBMaskBuffer;


	// If an  buffer already exists, delete its OpenGL resources before creating a new one.
	if (m_PostProcessBuffer)
	{
		glDeleteFramebuffers(1, &m_PostProcessBuffer->FBO);
		glDeleteTextures(1, &m_PostProcessBuffer->ColorTexture);
		glDeleteFramebuffers(1, &m_BloomExtractBuffer->FBO);
		glDeleteTextures(1, &m_BloomExtractBuffer->ColorTexture);
		glDeleteFramebuffers(1, &m_BloomBlurBuffer1->FBO);
		glDeleteTextures(1, &m_BloomBlurBuffer1->ColorTexture);
		glDeleteFramebuffers(1, &m_BloomBlurBuffer2->FBO);
		glDeleteTextures(1, &m_BloomBlurBuffer2->ColorTexture);
		glDeleteFramebuffers(1, &m_MotionBlurBuffer->FBO);
		glDeleteTextures(1, &m_MotionBlurBuffer->ColorTexture);
		if (m_MotionBlurMaskBuffer)
		{
			glDeleteFramebuffers(1, &m_MotionBlurMaskBuffer->FBO);
			glDeleteTextures(1, &m_MotionBlurMaskBuffer->ColorTexture);
		}
		if (m_NoiseTexture != 0)
		{
			glDeleteTextures(1, &m_NoiseTexture);
			m_NoiseTexture = 0;
		}
	}

	// Create main post-process buffer with depth attachment for skybox rendering
	glGenFramebuffers(1, &pPBuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, pPBuffer.FBO);

	// Color texture
	glGenTextures(1, &pPBuffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, pPBuffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pPBuffer.ColorTexture, 0);

	// outline
	//glActiveTexture(GL_TEXTURE2);
	//glBindTexture(GL_TEXTURE_2D, m_OutlineMaskTexture);
	//m_PostProcessShader->SetUniform1i("u_OutlineMask", 2);
	//m_PostProcessShader->SetUniform1i("u_OutlineEnabled", m_OutlineEnabled ? 1 : 0);
	//m_PostProcessShader->SetUniform3f("u_OutlineColor", m_OutlineColor.r, m_OutlineColor.g, m_OutlineColor.b);
	//m_PostProcessShader->SetUniform1f("u_OutlineThickness", m_OutlineThickness);
	//m_PostProcessShader->SetUniform1f("u_OutlineIntensity", m_OutlineIntensity);

	// Share G-Buffer's depth texture instead of creating a separate one
	// The depth attachment will be added after G-Buffer creation
	// Note: We don't create a depth texture here - we'll attach the G-Buffer's depth texture later

	glCheckError();

	// Calculate half resolution for bloom buffers (god rays + bloom)
	int halfWidth = width / 2;
	int halfHeight = height / 2;

	// Create bloom extract buffer at half resolution
	glGenFramebuffers(1, &bEBuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, bEBuffer.FBO);
	glGenTextures(1, &bEBuffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, bEBuffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, halfWidth, halfHeight, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bEBuffer.ColorTexture, 0);
	glCheckError();

	// Create bloom blur buffer 1 at half resolution
	glGenFramebuffers(1, &bBBuffer1.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, bBBuffer1.FBO);
	glGenTextures(1, &bBBuffer1.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, bBBuffer1.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, halfWidth, halfHeight, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bBBuffer1.ColorTexture, 0);
	glCheckError();

	// Create bloom blur buffer 2 at half resolution
	glGenFramebuffers(1, &bBBuffer2.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, bBBuffer2.FBO);
	glGenTextures(1, &bBBuffer2.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, bBBuffer2.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, halfWidth, halfHeight, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bBBuffer2.ColorTexture, 0);
	glCheckError();

	glGenFramebuffers(1, &AABuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, AABuffer.FBO);
	glGenTextures(1, &AABuffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, AABuffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, AABuffer.ColorTexture, 0);
	glCheckError();

	// Create motion blur buffer at full resolution
	glGenFramebuffers(1, &MBBuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, MBBuffer.FBO);
	glGenTextures(1, &MBBuffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, MBBuffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, MBBuffer.ColorTexture, 0);
	glCheckError();

	// Create motion blur mask buffer at full resolution (R8)
	glGenFramebuffers(1, &MBMaskBuffer.FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, MBMaskBuffer.FBO);
	glGenTextures(1, &MBMaskBuffer.ColorTexture);
	glBindTexture(GL_TEXTURE_2D, MBMaskBuffer.ColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, MBMaskBuffer.ColorTexture, 0);

	// Explicitly specify draw buffer for mask FBO
	GLenum maskDrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, maskDrawBuffers);
	glCheckError();

	GLenum maskStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (maskStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		EE_CORE_ERROR("ERROR: Motion blur mask framebuffer not complete! Status: {0}", maskStatus);
	}

	//

	// Making sure dimensions are non-zero
	if (width <= 0 || height <= 0)
	{
		EE_CORE_ERROR("ERROR: Invalid framebuffer dimensions: {0}x{1}", width, height);
	}

	// Explicitly specify draw buffer
	GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	// Check overall framebuffer completeness
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status)
		{
		case GL_FRAMEBUFFER_UNDEFINED:
			EE_CORE_ERROR("ERROR: Framebuffer is undefined!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete attachment!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			EE_CORE_ERROR("ERROR: Framebuffer missing attachment!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete draw buffer!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete read buffer!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			EE_CORE_ERROR("ERROR: Framebuffer unsupported!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete multisample!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			EE_CORE_ERROR("ERROR: Framebuffer incomplete layer targets!");
			break;
		default:
			EE_CORE_ERROR("ERROR: Framebuffer unknown error!");
			break;
		}
		EE_CORE_FATAL("Framebuffer Failed!!!");
		assert(false && "Check logs");
	}

	pPBuffer.width = width;
	pPBuffer.height = height;
	m_PostProcessBuffer = std::make_shared<PostProcessBuffer>(pPBuffer);
	bEBuffer.width = halfWidth;
	bEBuffer.height = halfHeight;
	m_BloomExtractBuffer = std::make_shared<PostProcessBuffer>(bEBuffer);
	bBBuffer1.width = halfWidth;
	bBBuffer1.height = halfHeight;
	m_BloomBlurBuffer1 = std::make_shared<PostProcessBuffer>(bBBuffer1);
	bBBuffer2.width = halfWidth;
	bBBuffer2.height = halfHeight;
	m_BloomBlurBuffer2 = std::make_shared<PostProcessBuffer>(bBBuffer2);
	AABuffer.width = width;
	AABuffer.height = height;
	m_AntiAliasingBuffer = std::make_shared<PostProcessBuffer>(AABuffer);
	MBBuffer.width = width;
	MBBuffer.height = height;
	m_MotionBlurBuffer = std::make_shared<PostProcessBuffer>(MBBuffer);
	MBMaskBuffer.width = width;
	MBMaskBuffer.height = height;
	m_MotionBlurMaskBuffer = std::make_shared<PostProcessBuffer>(MBMaskBuffer);
	// Generate film grain noise texture (256x256, single channel)
	{
		constexpr int NOISE_SIZE = 256;
		std::vector<unsigned char> noiseData(NOISE_SIZE * NOISE_SIZE);

		// Use a better noise algorithm - blue noise approximation via void-and-cluster
		std::mt19937 rng(42); // Fixed seed for reproducibility
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);

		// Generate base white noise
		for (int i = 0; i < NOISE_SIZE * NOISE_SIZE; ++i)
		{
			noiseData[i] = static_cast<unsigned char>(dist(rng) * 255.0f);
		}

		// Apply simple low-pass filter to reduce harsh patterns (makes grain more filmic)
		std::vector<unsigned char> filtered(NOISE_SIZE * NOISE_SIZE);
		for (int y = 0; y < NOISE_SIZE; ++y)
		{
			for (int x = 0; x < NOISE_SIZE; ++x)
			{
				int sum = 0;
				int count = 0;
				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dx = -1; dx <= 1; ++dx)
					{
						int nx = (x + dx + NOISE_SIZE) % NOISE_SIZE;
						int ny = (y + dy + NOISE_SIZE) % NOISE_SIZE;
						sum += noiseData[ny * NOISE_SIZE + nx];
						++count;
					}
				}
				filtered[y * NOISE_SIZE + x] = static_cast<unsigned char>(sum / count);
			}
		}

		glGenTextures(1, &m_NoiseTexture);
		glBindTexture(GL_TEXTURE_2D, m_NoiseTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, NOISE_SIZE, NOISE_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, filtered.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glBindTexture(GL_TEXTURE_2D, 0);
		EE_CORE_INFO("Film grain noise texture created ({}x{})", NOISE_SIZE, NOISE_SIZE);
	}

	// Attach G-Buffer's depth texture to PostProcess FBO for shared depth testing
	// This must happen AFTER PostProcess buffer is created and AFTER G-Buffer exists
	if (m_PostProcessBuffer && m_GBuffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_GBuffer->DepthTexture, 0);

		status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			EE_CORE_ERROR("ERROR: PostProcess framebuffer not complete after depth attachment!");
			EE_CORE_ERROR("Status: {0}, G-Buffer DepthTexture: {1}, PostProcess FBO: {2}",
				status, m_GBuffer->DepthTexture, m_PostProcessBuffer->FBO);
		}
		else
		{
			EE_CORE_INFO("Successfully attached G-Buffer depth texture to PostProcess FBO");
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	if (m_MotionBlurMaskBuffer && m_GBuffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_MotionBlurMaskBuffer->FBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_GBuffer->DepthTexture, 0);

		GLenum maskDepthStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (maskDepthStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			EE_CORE_ERROR("ERROR: Motion blur mask framebuffer not complete after depth attachment!");
			EE_CORE_ERROR("Status: {0}, G-Buffer DepthTexture: {1}, Mask FBO: {2}",
				maskDepthStatus, m_GBuffer->DepthTexture, m_MotionBlurMaskBuffer->FBO);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

/**
 * @brief Creates the G-buffer for deferred rendering.
 * @param width Buffer width.
 * @param height Buffer height.
 */
void Renderer::ResizeGBuffer(const int& width, const int& height)
{
	if (m_GBuffer && m_GBuffer->width == width && m_GBuffer->height == height)
		return;

	CreateGBuffer(width, height);

	CreatePostProcessBuffer(width, height);

	CreateOutlineMaskBuffer(width, height);

	ResizePickingBuffer(width, height);
}

/**
 * @brief Render depth pre-pass to eliminate overdraw in geometry pass.
 * Renders all geometry depth-only, so expensive G-buffer fragment shader only runs for visible pixels.
 */
void Renderer::RenderDepthPrePass(const Mtx44& view, const Mtx44& projection)
{
	if (!m_GBuffer || !m_DepthPrePassShader) {
		EE_CORE_ERROR("G-Buffer or depth pre-pass shader not initialized!");
		return;
	}

	// Bind G-buffer FBO (write depth to same depth buffer that geometry pass will use)
	glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer->FBO);
	glViewport(0, 0, m_GBuffer->width, m_GBuffer->height);

	// Clear depth buffer only (color buffers will be written in geometry pass)
	glClear(GL_DEPTH_BUFFER_BIT);
	glClearDepth(1.0f);

	// Disable color writes (depth-only pass)
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Enable depth testing and writing
	// Use GL_LEQUAL to match geometry pass depth
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);

	// Enable backface culling for depth pre-pass
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// Bind depth pre-pass shader
	m_DepthPrePassShader->Bind();

	m_DepthPrePassShader->SetUniformMatrix4fv("view", &view.m2[0][0]);
	m_DepthPrePassShader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);

	// Render standard (non-skinned) meshes with StandardVAO
	if (!m_DepthPrepassStandardCommands.empty() && m_MeshManager.GetStandardVAO() != 0) {
		m_DepthPrePassShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_DepthPrepassStandardDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_DepthPrepassStandardDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_DepthPrepassStandardCommands.size()),
			0
		);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Render skinned (animated) meshes with SkinnedVAO
	if (!m_DepthPrepassSkinnedCommands.empty() && m_MeshManager.GetSkinnedVAO() != 0) {
		m_DepthPrePassShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_DepthPrepassSkinnedDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_DepthPrepassSkinnedDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_DepthPrepassSkinnedCommands.size()),
			0
		);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Re-enable color writes for subsequent passes
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	m_DepthPrePassShader->Unbind();

	// Note: G-buffer FBO remains bound for geometry pass
	// Depth state (GL_LEQUAL, glDepthMask) will be set by geometry pass

	glCheckError();
}

/**
 * @brief Begin geometry pass for deferred rendering
 */
void Renderer::BeginGeometryPass()
{
	if (!m_GBuffer)
	{
		EE_CORE_ERROR("G-Buffer not initialized!");
		return;
	}

	// Bind g-buffer framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, m_GBuffer->FBO);
	glViewport(0, 0, m_GBuffer->width, m_GBuffer->height);

	// Clear only color buffers (depth already cleared and written by depth pre-pass)
	glClear(GL_COLOR_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE); 

	// Disable blending for geometry pass
	glDisable(GL_BLEND);

	// Enable backface culling for geometry pass
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

/**
 * @brief End geometry pass and prepare for lighting pass
 */
void Renderer::EndGeometryPass()
{
	// Check for errors
	glCheckError();
}

/**
 * @brief Begin lighting pass for deferred rendering - render to texture for post-processing
 */
void Renderer::BeginLightingPass()
{
	// Always render lighting pass to post-process buffer for sampling
	if (!m_PostProcessBuffer)
	{
		EE_CORE_ERROR("Post-process buffer not initialized!");
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
	glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

	// Clear the lighting pass output
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up for lighting calculations
	glDisable(GL_DEPTH_TEST); // No depth testing needed for full-screen pass
	glDisable(GL_BLEND);       // No blending needed for lighting output
}

/**
 * @brief End lighting pass and finalize frame
 */
void Renderer::EndLightingPass()
{
	glDisable(GL_BLEND);
	glCheckError();
}

/**
 * @brief Binds material textures to the geometry shader.
 * @param material Pointer to the material.
 */
void Renderer::BindMaterialTextures(Ermine::graphics::Material* material)
{
	if (!material) return;

	int texUnit = 0;
	if (material->HasParameter("materialAlbedoMap")) {
		std::shared_ptr<Texture> albedo = material->GetParameter("materialAlbedoMap")->texture;
		if (albedo && albedo->IsValid()) {
			albedo->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialAlbedoMap", texUnit);
		}
	}
	texUnit = 1;
	if (material->HasParameter("materialNormalMap")) {
		std::shared_ptr<Texture> normal = material->GetParameter("materialNormalMap")->texture;
		if (normal && normal->IsValid()) {
			normal->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialNormalMap", texUnit);
		}
	}
	texUnit = 2;
	if (material->HasParameter("materialRoughnessMap")) {
		std::shared_ptr<Texture> roughness = material->GetParameter("materialRoughnessMap")->texture;
		if (roughness && roughness->IsValid()) {
			roughness->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialRoughnessMap", texUnit);
		}
	}
	texUnit = 3;
	if (material->HasParameter("materialMetallicMap")) {
		std::shared_ptr<Texture> metallic = material->GetParameter("materialMetallicMap")->texture;
		if (metallic && metallic->IsValid()) {
			metallic->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialMetallicMap", texUnit);
		}
	}
	texUnit = 4;
	if (material->HasParameter("materialAoMap")) {
		std::shared_ptr<Texture> ao = material->GetParameter("materialAoMap")->texture;
		if (ao && ao->IsValid()) {
			ao->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialAoMap", texUnit);
		}
	}
	texUnit = 5;
	if (material->HasParameter("materialEmissiveMap")) {
		std::shared_ptr<Texture> emissive = material->GetParameter("materialEmissiveMap")->texture;
		if (emissive && emissive->IsValid()) {
			emissive->Bind(texUnit);
			m_GBufferShader->SetUniform1i("materialEmissiveMap", texUnit);
		}
	}
}

/**
 * @brief Renders the geometry pass, collecting transparent objects.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderGeometryPass(const Mtx44& view, const Mtx44& projection)
{
	if (!m_GBuffer || !m_GBufferShader) {
		EE_CORE_ERROR("G-Buffer or geometry shader not initialized!");
		return;
	}

	// Explicitly bind geometry shader BEFORE setting up framebuffer state
	// This ensures clean shader state regardless of what was bound previously
	m_GBufferShader->Bind();

	BeginGeometryPass();

	// Re-bind to ensure it's still active after framebuffer setup
	m_GBufferShader->Bind();
	BindMaterialBlockIfPresent(m_GBufferShader);

	// Check if any materials have been modified via ImGui or code
	CheckMaterialUpdates();

	// Recompile materials if dirty
	if (m_MaterialsDirty) {
		CompileMaterials();
	}

	// Clear transparent objects from previous frame
	m_transparentObjects.clear();

	// Calculate camera position for transparent sorting
	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);
	glm::mat4 invView = glm::inverse(glmView);
	Vec3 cameraPos = Vec3(invView[3][0], invView[3][1], invView[3][2]);

	// Set view and projection uniforms (model matrix comes from DrawInfo SSBO per draw)
	m_GBufferShader->SetUniformMatrix4fv("view", &view.m2[0][0]);
	m_GBufferShader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);

	// Pass previous frame's view-projection for velocity buffer computation
	m_GBufferShader->SetUniformMatrix4fv("u_PreviousViewProjection", glm::value_ptr(m_PreviousViewProjectionMatrix));
	m_GBufferShader->SetUniform1f("u_Time", m_ElapsedTime);

	// Calculate and pass normal matrix for view (mat3 extracted from view matrix)
	// Reuse glmView calculated above for camera position
	// This avoids extracting mat3(view) per-vertex in the shader
	glm::mat3 normalView = glm::mat3(glmView);
	m_GBufferShader->SetUniformMatrix3fv("normalView", glm::value_ptr(normalView));

	glGetError(); // Clear any existing errors
	// Render standard (non-skinned) meshes with StandardVAO
	if (!m_GeometryStandardItems.empty() && m_MeshManager.GetStandardVAO() != 0) {
		// Set baseDrawID = 0 for standard batch
		m_GBufferShader->SetUniform1ui("baseDrawID", 0);

		// Bind StandardVAO and DrawInfo SSBO
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_GeometryStandardDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_GeometryStandardDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_GeometryStandardItems.size()),
			0
		);
		GPUProfiler::TrackDrawCall(m_GeometryStandardVertexCount, m_GeometryStandardIndexCount);

		// Unbind
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Render skinned (animated) meshes with SkinnedVAO
	if (!m_GeometrySkinnedItems.empty() && m_MeshManager.GetSkinnedVAO() != 0) {
		// Set baseDrawID = 0 for skinned batch
		m_GBufferShader->SetUniform1ui("baseDrawID", 0);

		// Bind SkinnedVAO and DrawInfo SSBO
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_GeometrySkinnedDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_GeometrySkinnedDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_GeometrySkinnedItems.size()),
			0
		);
		GPUProfiler::TrackDrawCall(m_GeometrySkinnedVertexCount, m_GeometrySkinnedIndexCount);

		// Unbind
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Sort transparent objects EVERY frame (camera position changes)
	// - Full rebuild: Sort by shader FIRST (creates stable batches), then distance
	// - Fast update: Sort by distance ONLY within existing shader groups
	SortTransparentObjects(cameraPos, m_NeedsTransparentSort);
	auto uploadSortedCustomTransparentBuffers = [&]() {
		if (!m_ForwardTransparentCustomStandardItems.empty() &&
			m_ForwardTransparentCustomStandardCmdBuffer != 0 &&
			m_ForwardTransparentCustomStandardInfoBuffer != 0) {
			std::vector<DrawElementsIndirectCommand> commands;
			std::vector<DrawInfo> infos;
			commands.reserve(m_ForwardTransparentCustomStandardItems.size());
			infos.reserve(m_ForwardTransparentCustomStandardItems.size());
			for (const auto& item : m_ForwardTransparentCustomStandardItems) {
				commands.push_back(item.command);
				infos.push_back(item.info);
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomStandardCmdBuffer);
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
				commands.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomStandardInfoBuffer);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				static_cast<GLsizeiptr>(infos.size() * sizeof(DrawInfo)),
				infos.data());
		}

		if (!m_ForwardTransparentCustomSkinnedItems.empty() &&
			m_ForwardTransparentCustomSkinnedCmdBuffer != 0 &&
			m_ForwardTransparentCustomSkinnedInfoBuffer != 0) {
			std::vector<DrawElementsIndirectCommand> commands;
			std::vector<DrawInfo> infos;
			commands.reserve(m_ForwardTransparentCustomSkinnedItems.size());
			infos.reserve(m_ForwardTransparentCustomSkinnedItems.size());
			for (const auto& item : m_ForwardTransparentCustomSkinnedItems) {
				commands.push_back(item.command);
				infos.push_back(item.info);
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomSkinnedCmdBuffer);
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
				commands.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomSkinnedInfoBuffer);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				static_cast<GLsizeiptr>(infos.size() * sizeof(DrawInfo)),
				infos.data());
		}

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	};
	uploadSortedCustomTransparentBuffers();
	m_NeedsTransparentSort = false;  // Reset flag after sorting

	// Save current VP for next frame's velocity computation
	glm::mat4 glmProjection = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);
	m_PreviousViewProjectionMatrix = glmProjection * glmView;

	EndGeometryPass();
}


/**
 * @brief Main entry point for draw data compilation. Chooses between full rebuild or fast update.
 *
 * Rendering pipeline routing:
 * - Geometry pass: Opaque materials only
 * - Forward pass: Transparent materials only
 * - Shadow pass: All objects except those with castsShadows=false
 */
void Renderer::CompileDrawData()
{
#ifdef EE_DEBUG
	// Debug mode: force full rebuild every frame if requested
	if (m_ForceFullRebuildEveryFrame) {
		m_DrawDataNeedsFullRebuild = true;
	}
#endif

	// Entity list size changed (entities added/removed)
	if (HasEntityListChanged()) {
		m_DrawDataNeedsFullRebuild = true;
	}

	// Cache is empty but entities exist (initial build needed)
	if (!m_DrawDataNeedsFullRebuild && m_CachedDrawItems.empty()) {
		if (!m_Entities.empty() || !m_ModelSystem->m_Entities.empty()) {
			m_DrawDataNeedsFullRebuild = true;
		}
	}

	// Decide: full rebuild or fast update?
	if (m_DrawDataNeedsFullRebuild) {
		// Full rebuild path (entities changed, first frame, etc.)
		RebuildDrawData();

		// Trigger transparent object sorting after rebuild (sorts by shader, then Z)
		m_NeedsTransparentSort = true;
	}
	else {
		// Fast update path (only transforms/camera changed)
		UpdateDrawData();
	}
}

/**
 * @brief Full rebuild of all draw data. Called when entities are added/removed or on first frame.
 */
void Renderer::RebuildDrawData()
{
	const auto& ecs = Ermine::ECS::GetInstance();

	// Clear previous frame's commands for all passes
	m_DepthPrepassStandardCommands.clear();
	m_DepthPrepassStandardInfos.clear();
	m_DepthPrepassSkinnedCommands.clear();
	m_DepthPrepassSkinnedInfos.clear();

	m_PickingStandardCommands.clear();
	m_PickingStandardInfos.clear();
	m_PickingSkinnedCommands.clear();
	m_PickingSkinnedInfos.clear();

	m_GeometryStandardItems.clear();
	m_GeometrySkinnedItems.clear();

	// Clear forward pass lists
	m_ForwardTransparentDefaultStandardItems.clear();
	m_ForwardTransparentDefaultSkinnedItems.clear();

	// Clear opaque custom shader lists
	m_ForwardOpaqueCustomStandardItems.clear();
	m_ForwardOpaqueCustomSkinnedItems.clear();

	// Clear transparent custom shader lists
	m_ForwardTransparentCustomStandardItems.clear();
	m_ForwardTransparentCustomSkinnedItems.clear();

	// Clear shadow-only lists
	m_ShadowStandardCommands.clear();
	m_ShadowStandardInfos.clear();
	m_ShadowSkinnedCommands.clear();
	m_ShadowSkinnedInfos.clear();

	// Clear cache for fast-path updates
	m_CachedDrawItems.clear();

	// Reset vertex/index counters
	m_GeometryStandardVertexCount = 0;
	m_GeometryStandardIndexCount = 0;
	m_GeometrySkinnedVertexCount = 0;
	m_GeometrySkinnedIndexCount = 0;
	m_ForwardPassDrawCommandsVertexCount = 0;
	m_ForwardPassDrawCommandsIndexCount = 0;

	// Reserve space for depth prepass (opaque visible geometry only)
	m_DepthPrepassStandardCommands.reserve(m_Entities.size());
	m_DepthPrepassStandardInfos.reserve(m_Entities.size());
	m_DepthPrepassSkinnedCommands.reserve(m_Entities.size() / 4);
	m_DepthPrepassSkinnedInfos.reserve(m_Entities.size() / 4);

	// Reserve space for picking pass (ALL visible geometry - opaque + transparent)
	m_PickingStandardCommands.reserve(m_Entities.size());
	m_PickingStandardInfos.reserve(m_Entities.size());
	m_PickingSkinnedCommands.reserve(m_Entities.size() / 4);
	m_PickingSkinnedInfos.reserve(m_Entities.size() / 4);

	// Reserve space for geometry pass (opaque default shader only)
	m_GeometryStandardItems.reserve(m_Entities.size());
	m_GeometrySkinnedItems.reserve(m_Entities.size() / 4);

	// Reserve space for forward pass lists
	m_ForwardTransparentDefaultStandardItems.reserve(m_Entities.size() / 4);
	m_ForwardTransparentDefaultSkinnedItems.reserve(m_Entities.size() / 8);

	// Reserve space for shadow-only lists (culled meshes)
	m_ShadowStandardCommands.reserve(m_Entities.size() / 4);
	m_ShadowStandardInfos.reserve(m_Entities.size() / 4);
	m_ShadowSkinnedCommands.reserve(m_Entities.size() / 8);
	m_ShadowSkinnedInfos.reserve(m_Entities.size() / 8);

	// Reserve space for draw item cache (estimate: primitives + avg 3 meshes per model)
	size_t estimatedDrawItems = m_Entities.size() + (m_ModelSystem->m_Entities.size() * 3);
	m_CachedDrawItems.reserve(estimatedDrawItems);

	// ========== FRUSTUM CULLING SETUP ==========
	// Get camera view and projection matrices
	// Use GameCamera if active (playing), otherwise use EditorCamera
	Mtx44 viewMtx, projMtx;

#if defined(EE_EDITOR)
	// In editor build, check if playing
	if (editor::EditorGUI::isPlaying)
	{
		auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
		if (gameCamera && gameCamera->HasValidCamera())
		{
			// Use player camera when in play mode
			viewMtx = gameCamera->GetViewMatrix();
			projMtx = gameCamera->GetProjectionMatrix();
		}
		else
		{
			// Fallback to editor camera if no valid game camera
			const auto& editorCamera = editor::EditorCamera::GetInstance();
			viewMtx = editorCamera.GetViewMatrix();
			projMtx = editorCamera.GetProjectionMatrix();
		}
	}
	else
	{
		// Use editor camera when not playing
		const auto& editorCamera = editor::EditorCamera::GetInstance();
		viewMtx = editorCamera.GetViewMatrix();
		projMtx = editorCamera.GetProjectionMatrix();
	}
#else
	// Standalone build - use game camera
	auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
	if (gameCamera && gameCamera->HasValidCamera())
	{
		viewMtx = gameCamera->GetViewMatrix();
		projMtx = gameCamera->GetProjectionMatrix();
	}
	else
	{
		// Fallback if no camera is available
		viewMtx = Mtx44(); // Identity matrix
		projMtx = Mtx44(); // Identity matrix
	}
#endif

	// Convert to glm for frustum extraction (use ToGlm helper)
	glm::mat4 viewGlm = ToGlm(viewMtx);
	glm::mat4 projGlm = ToGlm(projMtx);

	// Build frustum from view-projection matrix
	Frustum frustum;
	glm::mat4 viewProj = projGlm * viewGlm;
	frustum.ExtractFromViewProjection(viewProj);

	// Debug: Draw frustum if enabled
	if (m_DebugDrawFrustum) {
		glm::mat4 invViewProj = glm::inverse(viewProj);
		SubmitDebugFrustum(frustum, invViewProj, glm::vec3(0.0f, 1.0f, 1.0f)); // Cyan color
	}

	// Culling statistics
	culledMeshes = 0;

	auto FindChildEntityForMesh = [&](Ermine::EntityID parent, const std::string& meshID) -> Ermine::EntityID
		{
			auto& ecsLocal = ECS::GetInstance();
			if (!ecsLocal.HasComponent<Ermine::HierarchyComponent>(parent))
				return 0;

			const auto& h = ecsLocal.GetComponent<Ermine::HierarchyComponent>(parent);
			const std::string expected = "Mesh_" + meshID;

			// --- Prefer GUID-based children
			for (const Ermine::Guid& cg : h.childrenGuids) {
				Ermine::EntityID child = ecsLocal.GetGuidRegistry().FindEntity(cg); // 0 if not found
				if (child == 0) continue;
				if (!ecsLocal.HasComponent<Ermine::ObjectMetaData>(child)) continue;
				const auto& meta = ecsLocal.GetComponent<Ermine::ObjectMetaData>(child);
				if (meta.name == expected) return child;
			}

			// --- Fallback: legacy EntityID list (for old saves / transitional scenes)
			for (Ermine::EntityID child : h.children) {
				if (!ecsLocal.IsEntityValid(child)) continue;
				if (!ecsLocal.HasComponent<Ermine::ObjectMetaData>(child)) continue;
				const auto& meta = ecsLocal.GetComponent<Ermine::ObjectMetaData>(child);
				if (meta.name == expected) return child;
			}

			return 0;
		};


	// ========== MESHES ==========
	for (auto& entity : m_Entities) {
		if (ecs.HasComponent<Ermine::Material>(entity) && ecs.HasComponent<Ermine::Transform>(entity)) {
			auto& mesh = ecs.GetComponent<Mesh>(entity);
			auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);

			// Skip if no registered mesh ID
			if (mesh.registeredMeshID.empty()) continue;

			Ermine::graphics::Material* material = materialComponent.GetMaterial();
			if (!material) {
				continue;
			}

			glm::mat4 model = GetEntityWorldMatrix(entity);

			// Get mesh handle from MeshManager using the stored registered mesh ID
			MeshHandle meshHandle = m_MeshManager.GetMeshHandle(mesh.registeredMeshID);
			if (!meshHandle.isValid()) continue;

			const MeshSubset* meshData = m_MeshManager.GetMeshData(meshHandle);
			if (!meshData) continue;

			uint32_t materialIndex = material->GetMaterialIndex();

			// Determine which pass this mesh belongs to
			bool isTransparent = IsTransparentMaterial(material);
			bool castsShadows = CastsShadows(material);
			bool isCustomShader = HasCustomShader(material);

			// Build draw command
			DrawElementsIndirectCommand cmd;
			cmd.count = meshData->indexCount;
			cmd.instanceCount = 1;
			cmd.firstIndex = meshData->indexOffset;
			cmd.baseVertex = meshData->baseVertex;
			cmd.baseInstance = 0;

			// Build draw info
			DrawInfo info;
			info.modelMatrix = model;

			// Pre-calculate normal matrix and store as 3 separate columns (std430 mat3 has 16-byte stride!)
			glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
			info.normalMatrixCol0 = glm::vec4(normalMat[0], 0.0f);
			info.normalMatrixCol1 = glm::vec4(normalMat[1], 0.0f);
			info.normalMatrixCol2 = glm::vec4(normalMat[2], 0.0f);

			info.aabbMin = glm::vec3(mesh.aabbMin.x, mesh.aabbMin.y, mesh.aabbMin.z);
			info.materialIndex = materialIndex;
			info.aabbMax = glm::vec3(mesh.aabbMax.x, mesh.aabbMax.y, mesh.aabbMax.z);
			info.entityID = static_cast<uint32_t>(entity);
			info.flags = materialComponent.flickerEmissive ? FLAG_FLICKER_EMISSIVE : 0; // Primitives: no skinning, no camera attachment
			info.boneTransformOffset = 0;
			info._pad0 = 0;
			info._pad1 = 0;

			// Cache this draw item for fast path updates
			CachedDrawItem cacheItem;
			cacheItem.entity = entity;
			cacheItem.childMaterialEntity = 0;  // Primitives don't have child materials
			cacheItem.materialEntity = entity;  // Primitives always use their own material
			cacheItem.hadParentMaterial = true; // Primitive's material is valid (required to render)
			cacheItem.hadChildMaterial = false; // No child material
			cacheItem.meshHandle = meshHandle;
			cacheItem.meshData = meshData;
			cacheItem.modelMeshData = nullptr;
			cacheItem.materialIndex = materialIndex;
			cacheItem.aabbMin = glm::vec3(mesh.aabbMin.x, mesh.aabbMin.y, mesh.aabbMin.z);
			cacheItem.aabbMax = glm::vec3(mesh.aabbMax.x, mesh.aabbMax.y, mesh.aabbMax.z);
			cacheItem.isTransparent = isTransparent;
			cacheItem.castsShadows = castsShadows;
			cacheItem.hasCustomShader = isCustomShader;
			cacheItem.useSkinning = false; // Primitives never use skinning
			cacheItem.hasSkinningData = false;
			cacheItem.isCameraAttached = false;
			cacheItem.flickerEmissive = materialComponent.flickerEmissive;
			cacheItem.boneOffset = 0;
			m_CachedDrawItems.push_back(cacheItem);

			// PASS 1: PICKING PASS - ALL geometry (opaque + transparent, for object selection)
			m_PickingStandardCommands.push_back(cmd);
			m_PickingStandardInfos.push_back(info);

			// PASS 2: DEPTH PREPASS - Opaque geometry only (for early-z rejection)
			// Transparent objects excluded to prevent depth conflicts with objects behind them
			if (!isTransparent) {
				m_DepthPrepassStandardCommands.push_back(cmd);
				m_DepthPrepassStandardInfos.push_back(info);
			}

			// PASS 3: GEOMETRY/FORWARD - Route by shader type and transparency
			if (!isTransparent && !isCustomShader) {
				// Opaque default shader → Geometry pass (deferred lighting)
				DefaultShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.castsShadows = castsShadows;
				m_GeometryStandardItems.push_back(item);
			}
			else if (!isTransparent && isCustomShader) {
				// Opaque custom shader → Forward pass (rendered after geometry)
				CustomShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.shader = material->GetShader();
				item.castsShadows = castsShadows;
				m_ForwardOpaqueCustomStandardItems.push_back(item);
			}
			else if (isTransparent && !isCustomShader) {
				// Transparent default shader → Forward pass (sorted, back-to-front)
				DefaultShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.castsShadows = castsShadows;
				m_ForwardTransparentDefaultStandardItems.push_back(item);
			}
			else if (isTransparent && isCustomShader) {
				// Transparent custom shader → Forward pass (sorted, back-to-front)
				CustomShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.shader = material->GetShader();
				item.castsShadows = castsShadows;
				m_ForwardTransparentCustomStandardItems.push_back(item);
			}

			// PASS 4: SHADOW PASS - ALL geometry (visible OR culled) that casts shadows
			if (castsShadows) {
				m_ShadowStandardCommands.push_back(cmd);
				m_ShadowStandardInfos.push_back(info);
			}
		}
	}

	// ========== STATIC MODELS ==========
	for (auto& entity : m_ModelSystem->m_Entities) {
		if (!ecs.HasComponent<Ermine::Transform>(entity))
			continue;

		auto& modelComp = ecs.GetComponent<ModelComponent>(entity);

		if (!modelComp.m_model)
			continue;

		if (!ecs.HasComponent<AnimationComponent>(entity)) {
			// Build entity transform
			glm::mat4 entityModel = GetEntityWorldMatrix(entity);

			// Process each mesh in the model
			for (const auto& mesh : modelComp.m_model->GetMeshes()) {
				glm::mat4 meshModel = entityModel * mesh.localTransform;

				// Get mesh handle from MeshManager
				MeshHandle meshHandle = m_MeshManager.GetMeshHandle(mesh.meshID);
				if (!meshHandle.isValid()) continue;

				const MeshSubset* meshData = m_MeshManager.GetMeshData(meshHandle);
				if (!meshData) continue;

				// Get material from the mesh's child entity (if it has one), otherwise use parent entity
				// Track complete material structure for proper cache validation
				Ermine::graphics::Material* material = nullptr;
				Ermine::EntityID materialEntity = 0;
				Ermine::EntityID childEntity = 0;
				bool hadParentMaterial = false;
				bool hadChildMaterial = false;

				// Find child entity using GUID-based lookup (with fallback to legacy EntityID)
				childEntity = FindChildEntityForMesh(entity, meshData->meshID);

				// Check child material
				Ermine::graphics::Material* childMaterial = nullptr;
				if (childEntity != 0 && ecs.HasComponent<Ermine::Material>(childEntity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(childEntity);
					childMaterial = materialComponent.GetMaterial();
					hadChildMaterial = (childMaterial != nullptr);
					if (childMaterial) {
						material = childMaterial;
						materialEntity = childEntity;
					}
				}

				// Check parent material
				Ermine::graphics::Material* parentMaterial = nullptr;
				if (ecs.HasComponent<Ermine::Material>(entity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
					parentMaterial = materialComponent.GetMaterial();
					hadParentMaterial = (parentMaterial != nullptr);
					// Use parent if child didn't provide valid material
					if (!material && parentMaterial) {
						material = parentMaterial;
						materialEntity = entity;
					}
				}

				// Skip this mesh if no material found
				if (!material) continue;

				bool flickerEmissive = false;
				if (materialEntity != 0 && ecs.HasComponent<Ermine::Material>(materialEntity)) {
					flickerEmissive = ecs.GetComponent<Ermine::Material>(materialEntity).flickerEmissive;
				}

				uint32_t materialIndex = material->GetMaterialIndex();

				// Determine which pass this mesh belongs to
				bool isTransparent = IsTransparentMaterial(material);
				bool castsShadows = CastsShadows(material);
				bool isCustomShader = HasCustomShader(material);

				// Build draw command
				DrawElementsIndirectCommand cmd;
				cmd.count = meshData->indexCount;
				cmd.instanceCount = 1;
				cmd.firstIndex = meshData->indexOffset;
				cmd.baseVertex = meshData->baseVertex;
				cmd.baseInstance = 0;

				// Build draw info with AABB and model matrix
				DrawInfo info;
				info.modelMatrix = meshModel;
				

				// Pre-calculate normal matrix and store as 3 separate columns (std430 mat3 has 16-byte stride!)
				glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(meshModel)));
				info.normalMatrixCol0 = glm::vec4(normalMat[0], 0.0f);
				info.normalMatrixCol1 = glm::vec4(normalMat[1], 0.0f);
				info.normalMatrixCol2 = glm::vec4(normalMat[2], 0.0f);
				info.aabbMin = mesh.aabbMin;
				info.materialIndex = materialIndex;
				info.aabbMax = mesh.aabbMax;
				info.entityID = static_cast<uint32_t>(entity);

				// PLAYER OBJECT DETECTION
				bool isCameraAttached = false;

				// Case 1: Self check
				if (ecs.HasComponent<CameraComponent>(entity))
				{
					isCameraAttached = true;
				}
				// Case 2 & 3: Check hierarchy
				else if (ecs.HasComponent<HierarchyComponent>(entity))
				{
					const auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
					EntityID parentEntity = hierarchy.parent;

					// Recursive function to search entire tree for CameraComponent
					std::function<bool(EntityID)> hasCamera = [&](EntityID node) -> bool {
						if (ecs.HasComponent<CameraComponent>(node))
							return true;

						if (ecs.HasComponent<HierarchyComponent>(node))
						{
							const auto& nodeHierarchy = ecs.GetComponent<HierarchyComponent>(node);
							for (EntityID child : nodeHierarchy.children)
							{
								if (hasCamera(child))
									return true;
							}
						}
						return false;
					};

					if (parentEntity == 0)
					{
						// Case 2: This is a root node - search entire tree from here
						isCameraAttached = hasCamera(entity);
					}
					else
					{
						// Case 3: Has a parent - traverse to root, then search entire tree
						EntityID rootEntity = entity;
						while (ecs.HasComponent<HierarchyComponent>(rootEntity))
						{
							const auto& currentHierarchy = ecs.GetComponent<HierarchyComponent>(rootEntity);
							if (currentHierarchy.parent == 0)
								break;
							rootEntity = currentHierarchy.parent;
						}

						// Search entire tree from root
						isCameraAttached = hasCamera(rootEntity);
					}
				}

				// Build flags for this draw
				info.flags = 0;
				if (isCameraAttached) {
					info.flags |= FLAG_CAMERA_ATTACHED;
				}
				if (flickerEmissive) {
					info.flags |= FLAG_FLICKER_EMISSIVE;
				}
				info.boneTransformOffset = 0;
				info._pad0 = 0;
				info._pad1 = 0;

				// Cache this draw item for fast path updates - store complete material structure
				CachedDrawItem cacheItem;
				cacheItem.entity = entity;
				cacheItem.childMaterialEntity = childEntity;  // Child entity that was checked (or 0)
				cacheItem.materialEntity = materialEntity;    // Entity that actually provided the material
				cacheItem.hadParentMaterial = hadParentMaterial;  // Whether parent had valid material
				cacheItem.hadChildMaterial = hadChildMaterial;    // Whether child had valid material
				cacheItem.meshHandle = meshHandle;
				cacheItem.meshData = meshData;
				cacheItem.modelMeshData = &mesh;
				cacheItem.materialIndex = materialIndex;
				cacheItem.aabbMin = mesh.aabbMin;
				cacheItem.aabbMax = mesh.aabbMax;
				cacheItem.isTransparent = isTransparent;
				cacheItem.castsShadows = castsShadows;
				cacheItem.hasCustomShader = isCustomShader;
				cacheItem.useSkinning = false; // Static models never use skinning
				cacheItem.hasSkinningData = false;
				cacheItem.isCameraAttached = isCameraAttached; // Cache camera-attachment for fast path
				cacheItem.flickerEmissive = flickerEmissive;
				cacheItem.boneOffset = 0;
				m_CachedDrawItems.push_back(cacheItem);

				// PASS 1: PICKING PASS - ALL geometry (opaque + transparent, for object selection)
				m_PickingStandardCommands.push_back(cmd);
				m_PickingStandardInfos.push_back(info);

				// PASS 2: DEPTH PREPASS - Opaque geometry only (for early-z rejection)
				// Transparent objects excluded to prevent depth conflicts with objects behind them
				if (!isTransparent) {
					m_DepthPrepassStandardCommands.push_back(cmd);
					m_DepthPrepassStandardInfos.push_back(info);
				}

				// PASS 3: GEOMETRY/FORWARD - Route by shader type and transparency
				if (!isTransparent && !isCustomShader) {
					// Opaque default shader → Geometry pass (deferred lighting)
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = castsShadows;
					m_GeometryStandardItems.push_back(item);
					m_GeometryStandardVertexCount += meshData->vertexCount;
					m_GeometryStandardIndexCount += cmd.count;
				}
				else if (!isTransparent && isCustomShader) {
					// Opaque custom shader → Forward pass (rendered after geometry)
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = material->GetShader();
					item.castsShadows = castsShadows;
					m_ForwardOpaqueCustomStandardItems.push_back(item);
				}
				else if (isTransparent && !isCustomShader) {
					// Transparent default shader → Forward pass (sorted, back-to-front)
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = castsShadows;
					m_ForwardTransparentDefaultStandardItems.push_back(item);
					m_ForwardPassDrawCommandsVertexCount += meshData->vertexCount;
					m_ForwardPassDrawCommandsIndexCount += cmd.count;
				}
				else if (isTransparent && isCustomShader) {
					// Transparent custom shader → Forward pass (sorted, back-to-front)
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = material->GetShader();
					item.castsShadows = castsShadows;
					m_ForwardTransparentCustomStandardItems.push_back(item);
					m_ForwardPassDrawCommandsVertexCount += meshData->vertexCount;
					m_ForwardPassDrawCommandsIndexCount += cmd.count;
				}

				// PASS 4: SHADOW PASS - ALL geometry (visible OR culled) that casts shadows
				if (castsShadows) {
					m_ShadowStandardCommands.push_back(cmd);
					m_ShadowStandardInfos.push_back(info);
				}
			}
		}
		else {
			auto& animComp = ecs.GetComponent<AnimationComponent>(entity);

			if (animComp.boneTransformOffset < 0) continue; // Skip if no valid bone data

			// For skinned models: inherit parent world translation/rotation, but keep local scale only.
			const glm::mat4 worldMatrix = GetEntityWorldMatrix(entity);
			glm::vec3 localScale(1.0f);
			if (ecs.HasComponent<Transform>(entity)) {
				const auto& transform = ecs.GetComponent<Transform>(entity);
				localScale = glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z);
			}
			glm::mat4 modelMatrix = BuildSkinnedModelMatrix(worldMatrix, localScale);

			// Get bone transform offset
			uint32_t boneOffset = static_cast<uint32_t>(animComp.boneTransformOffset);

			// Process each mesh in the model
			const auto& meshes = modelComp.m_model->GetMeshes();

			for (const auto& mesh : meshes) {
				glm::mat4 meshModelMatrix = modelMatrix * mesh.localTransform;

				// Get mesh handle from MeshManager
				MeshHandle meshHandle = m_MeshManager.GetMeshHandle(mesh.meshID);
				if (!meshHandle.isValid()) {
					EE_CORE_WARN("Skinned entity {0}: Invalid mesh handle for meshID '{1}'", entity, mesh.meshID);
					continue;
				}

				const MeshSubset* meshData = m_MeshManager.GetMeshData(meshHandle);
				if (!meshData) {
					EE_CORE_WARN("Skinned entity {0}: No mesh data for meshID '{1}'", entity, mesh.meshID);
					continue;
				}

				// Get material from the mesh's child entity (if it has one), otherwise use parent
				// Track complete material structure for proper cache validation
				Ermine::graphics::Material* meshMaterial = nullptr;
				Ermine::EntityID materialEntity = 0;
				Ermine::EntityID childEntity = 0;
				bool hadParentMaterial = false;
				bool hadChildMaterial = false;

				// Find child entity using GUID-based lookup (with fallback to legacy EntityID)
				childEntity = FindChildEntityForMesh(entity, meshData->meshID);

				// Check child material
				Ermine::graphics::Material* childMaterial = nullptr;
				if (childEntity != 0 && ecs.HasComponent<Ermine::Material>(childEntity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(childEntity);
					childMaterial = materialComponent.GetMaterial();
					hadChildMaterial = (childMaterial != nullptr);
					if (childMaterial) {
						meshMaterial = childMaterial;
						materialEntity = childEntity;
					}
				}

				// Check parent material
				Ermine::graphics::Material* parentMaterial = nullptr;
				if (ecs.HasComponent<Ermine::Material>(entity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
					parentMaterial = materialComponent.GetMaterial();
					hadParentMaterial = (parentMaterial != nullptr);
					// Use parent if child didn't provide valid material
					if (!meshMaterial && parentMaterial) {
						meshMaterial = parentMaterial;
						materialEntity = entity;
					}
				}

				// Skip this mesh if no material found
				if (!meshMaterial) continue;

				bool flickerEmissive = false;
				if (materialEntity != 0 && ecs.HasComponent<Ermine::Material>(materialEntity)) {
					flickerEmissive = ecs.GetComponent<Ermine::Material>(materialEntity).flickerEmissive;
				}

				// Determine material properties for this mesh
				uint32_t materialIndex = meshMaterial->GetMaterialIndex();
				bool isTransparent = IsTransparentMaterial(meshMaterial);
				bool castsShadows = CastsShadows(meshMaterial);
				bool isCustomShader = HasCustomShader(meshMaterial);

				// Build draw command
				DrawElementsIndirectCommand cmd;
				cmd.count = meshData->indexCount;
				cmd.instanceCount = 1;
				cmd.firstIndex = meshData->indexOffset;
				cmd.baseVertex = meshData->baseVertex;
				cmd.baseInstance = 0;

				// Build draw info with AABB and model matrix
				DrawInfo info;
				info.modelMatrix = meshModelMatrix;
				

				// Pre-calculate normal matrix and store as 3 separate columns (std430 mat3 has 16-byte stride!)
				glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(meshModelMatrix)));
				info.normalMatrixCol0 = glm::vec4(normalMat[0], 0.0f);
				info.normalMatrixCol1 = glm::vec4(normalMat[1], 0.0f);
				info.normalMatrixCol2 = glm::vec4(normalMat[2], 0.0f);
				info.aabbMin = mesh.aabbMin;
				info.materialIndex = materialIndex;
				info.aabbMax = mesh.aabbMax;
				info.entityID = static_cast<uint32_t>(entity);

				// PLAYER OBJECT DETECTION
				bool isCameraAttached = false;

				// Case 1: Self check
				if (ecs.HasComponent<CameraComponent>(entity))
				{
					isCameraAttached = true;
				}
				// Case 2 & 3: Check hierarchy
				else if (ecs.HasComponent<HierarchyComponent>(entity))
				{
					const auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
					EntityID parentEntity = hierarchy.parent;

					// Recursive function to search entire tree for CameraComponent
					std::function<bool(EntityID)> hasCamera = [&](EntityID node) -> bool {
						if (ecs.HasComponent<CameraComponent>(node))
							return true;

						if (ecs.HasComponent<HierarchyComponent>(node))
						{
							const auto& nodeHierarchy = ecs.GetComponent<HierarchyComponent>(node);
							for (EntityID child : nodeHierarchy.children)
							{
								if (hasCamera(child))
									return true;
							}
						}
						return false;
					};

					if (parentEntity == 0)
					{
						// Case 2: This is a root node - search entire tree from here
						isCameraAttached = hasCamera(entity);
					}
					else
					{
						// Case 3: Has a parent - traverse to root, then search entire tree
						EntityID rootEntity = entity;
						while (ecs.HasComponent<HierarchyComponent>(rootEntity))
						{
							const auto& currentHierarchy = ecs.GetComponent<HierarchyComponent>(rootEntity);
							if (currentHierarchy.parent == 0)
								break;
							rootEntity = currentHierarchy.parent;
						}

						// Search entire tree from root
						isCameraAttached = hasCamera(rootEntity);
					}
				}

				// Build flags: skinning + camera attachment
				info.flags = FLAG_SKINNING;
				if (isCameraAttached)
				{
					info.flags |= FLAG_CAMERA_ATTACHED;
				}
				if (flickerEmissive)
				{
					info.flags |= FLAG_FLICKER_EMISSIVE;
				}

				info.boneTransformOffset = boneOffset;
				info._pad0 = 0;
				info._pad1 = 0;

				// Cache this draw item for fast path updates - store complete material structure
				CachedDrawItem cacheItem;
				cacheItem.entity = entity;
				cacheItem.childMaterialEntity = childEntity;  // Child entity that was checked (or 0)
				cacheItem.materialEntity = materialEntity;    // Entity that actually provided the material
				cacheItem.hadParentMaterial = hadParentMaterial;  // Whether parent had valid material
				cacheItem.hadChildMaterial = hadChildMaterial;    // Whether child had valid material
				cacheItem.meshHandle = meshHandle;
				cacheItem.meshData = meshData;
				cacheItem.modelMeshData = &mesh;
				cacheItem.materialIndex = materialIndex;
				cacheItem.aabbMin = mesh.aabbMin;
				cacheItem.aabbMax = mesh.aabbMax;
				cacheItem.isTransparent = isTransparent;
				cacheItem.castsShadows = castsShadows;
				cacheItem.hasCustomShader = isCustomShader;
				cacheItem.useSkinning = true; // Animated models use skinning
				cacheItem.hasSkinningData = std::any_of(
					mesh.boneAabbValid.begin(), mesh.boneAabbValid.end(),
					[](uint8_t v) { return v != 0; });
				cacheItem.isCameraAttached = isCameraAttached; // Cache camera-attachment for fast path
				cacheItem.flickerEmissive = flickerEmissive;
				cacheItem.boneOffset = boneOffset;
				m_CachedDrawItems.push_back(cacheItem);

				// PASS 1: PICKING PASS - ALL geometry (opaque + transparent, for object selection)
				m_PickingSkinnedCommands.push_back(cmd);
				m_PickingSkinnedInfos.push_back(info);

				// PASS 2: DEPTH PREPASS - Opaque geometry only (for early-z rejection)
				// Transparent objects excluded to prevent depth conflicts with objects behind them
				if (!isTransparent) {
					m_DepthPrepassSkinnedCommands.push_back(cmd);
					m_DepthPrepassSkinnedInfos.push_back(info);
				}

				// PASS 3: GEOMETRY/FORWARD - Route by shader type and transparency
				if (!isTransparent && !isCustomShader) {
					// Opaque default shader → Geometry pass (deferred lighting)
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = castsShadows;
					m_GeometrySkinnedItems.push_back(item);
					m_GeometrySkinnedVertexCount += meshData->vertexCount;
					m_GeometrySkinnedIndexCount += cmd.count;
				}
				else if (!isTransparent && isCustomShader) {
					// Opaque custom shader → Forward pass (rendered after geometry)
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = meshMaterial->GetShader();
					item.castsShadows = castsShadows;
					m_ForwardOpaqueCustomSkinnedItems.push_back(item);
				}
				else if (isTransparent && !isCustomShader) {
					// Transparent default shader → Forward pass (sorted, back-to-front)
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = castsShadows;
					m_ForwardTransparentDefaultSkinnedItems.push_back(item);
					m_ForwardPassDrawCommandsVertexCount += meshData->vertexCount;
					m_ForwardPassDrawCommandsIndexCount += cmd.count;
				}
				else if (isTransparent && isCustomShader) {
					// Transparent custom shader → Forward pass (sorted, back-to-front)
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = meshMaterial->GetShader();
					item.castsShadows = castsShadows;
					m_ForwardTransparentCustomSkinnedItems.push_back(item);
					m_ForwardPassDrawCommandsVertexCount += meshData->vertexCount;
					m_ForwardPassDrawCommandsIndexCount += cmd.count;
				}

				// PASS 4: SHADOW PASS - ALL geometry (visible OR culled) that casts shadows
				if (castsShadows) {
					m_ShadowSkinnedCommands.push_back(cmd);
					m_ShadowSkinnedInfos.push_back(info);
				}
			}
		}
	}

	// ========== UPDATE DIRTY TRACKING HASHES ==========
	// Update entity list hash for next frame's comparison
	m_LastEntityListHash = CalculateEntityListHash();

	// Clear full rebuild flag (will be set again if major change detected)
	m_DrawDataNeedsFullRebuild = false;

	// Re-run the stable fast path immediately so the rendered frame uses the same
	// packing/update path as subsequent frames.
	UpdateDrawData();
}

/**
 * @brief Fast incremental update of draw data. Only updates transforms/culling for existing entities.
 *
 * This is the optimized path that runs when only transforms/camera have changed.
 * Uses cached mesh handles and material indices to avoid expensive lookups.
 */
void Renderer::UpdateDrawData()
{
	const auto& ecs = Ermine::ECS::GetInstance();

	// Clear previous frame's commands (but NOT the cache!)
	m_DepthPrepassStandardCommands.clear();
	m_DepthPrepassStandardInfos.clear();
	m_DepthPrepassSkinnedCommands.clear();
	m_DepthPrepassSkinnedInfos.clear();

	m_PickingStandardCommands.clear();
	m_PickingStandardInfos.clear();
	m_PickingSkinnedCommands.clear();
	m_PickingSkinnedInfos.clear();

	m_GeometryStandardItems.clear();
	m_GeometrySkinnedItems.clear();

	m_ForwardTransparentDefaultStandardItems.clear();
	m_ForwardTransparentDefaultSkinnedItems.clear();

	// Clear custom shader item lists (rebuilt every frame with updated transforms)
	m_ForwardOpaqueCustomStandardItems.clear();
	m_ForwardOpaqueCustomSkinnedItems.clear();
	m_ForwardTransparentCustomStandardItems.clear();
	m_ForwardTransparentCustomSkinnedItems.clear();

	m_ShadowStandardCommands.clear();
	m_ShadowStandardInfos.clear();
	m_ShadowSkinnedCommands.clear();
	m_ShadowSkinnedInfos.clear();

	// Reset counters
	m_GeometryStandardVertexCount = 0;
	m_GeometryStandardIndexCount = 0;
	m_GeometrySkinnedVertexCount = 0;
	m_GeometrySkinnedIndexCount = 0;
	m_ForwardPassDrawCommandsVertexCount = 0;
	m_ForwardPassDrawCommandsIndexCount = 0;

	// ========== FRUSTUM CULLING SETUP ==========
	Mtx44 viewMtx, projMtx;

#if defined(EE_EDITOR)
	if (editor::EditorGUI::isPlaying)
	{
		auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
		if (gameCamera && gameCamera->HasValidCamera())
		{
			viewMtx = gameCamera->GetViewMatrix();
			projMtx = gameCamera->GetProjectionMatrix();
		}
		else
		{
			const auto& editorCamera = editor::EditorCamera::GetInstance();
			viewMtx = editorCamera.GetViewMatrix();
			projMtx = editorCamera.GetProjectionMatrix();
		}
	}
	else
	{
		const auto& editorCamera = editor::EditorCamera::GetInstance();
		viewMtx = editorCamera.GetViewMatrix();
		projMtx = editorCamera.GetProjectionMatrix();
	}
#else
	auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
	if (gameCamera && gameCamera->HasValidCamera())
	{
		viewMtx = gameCamera->GetViewMatrix();
		projMtx = gameCamera->GetProjectionMatrix();
	}
	else
	{
		viewMtx = Mtx44();
		projMtx = Mtx44();
	}
#endif

	// Build frustum
	glm::mat4 viewGlm = ToGlm(viewMtx);
	glm::mat4 projGlm = ToGlm(projMtx);
	Frustum frustum;
	glm::mat4 viewProj = projGlm * viewGlm;
	frustum.ExtractFromViewProjection(viewProj);

	// Debug: Draw frustum if enabled
	if (m_DebugDrawFrustum) {
		glm::mat4 invViewProj = glm::inverse(viewProj);
		SubmitDebugFrustum(frustum, invViewProj, glm::vec3(0.0f, 1.0f, 1.0f));
	}

	// Culling statistics
	culledMeshes = 0;

	// ========== FAST PATH: ITERATE CACHED DRAW ITEMS ==========
	// This avoids expensive mesh handle lookups and material lookups
	for (const auto& cachedItem : m_CachedDrawItems)
	{
		// EARLY OUT: Skip inactive entities (check selfActive flag)
		if (ecs.HasComponent<ObjectMetaData>(cachedItem.entity)) {
			const auto& meta = ecs.GetComponent<ObjectMetaData>(cachedItem.entity);
			if (!meta.selfActive) {
				continue;
			}
		}

		glm::mat4 model;
		if (cachedItem.useSkinning) {
			// Skinned entities should follow parent translation/rotation but not inherit parent scale.
			const glm::mat4 worldMatrix = GetEntityWorldMatrix(cachedItem.entity);
			glm::vec3 localScale(1.0f);
			if (ecs.HasComponent<Transform>(cachedItem.entity)) {
				const auto& transform = ecs.GetComponent<Transform>(cachedItem.entity);
				localScale = glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z);
			}
			model = BuildSkinnedModelMatrix(worldMatrix, localScale);
		}
		else {
			model = GetEntityWorldMatrix(cachedItem.entity);
		}
		if (cachedItem.modelMeshData) {
			model *= cachedItem.modelMeshData->localTransform;
		}

		glm::vec3 localAabbMin = cachedItem.aabbMin;
		glm::vec3 localAabbMax = cachedItem.aabbMax;
		if (cachedItem.useSkinning && cachedItem.hasSkinningData && cachedItem.modelMeshData
			&& ecs.HasComponent<AnimationComponent>(cachedItem.entity)) {
			const auto& animComp = ecs.GetComponent<AnimationComponent>(cachedItem.entity);
			if (animComp.m_animator) {
				ComputeSkinnedMeshAABB(*cachedItem.modelMeshData,
					animComp.m_animator->GetFinalBoneMatrices(),
					localAabbMin, localAabbMax);
			}
		}

		// Transform AABB to world space using center-extent method (faster than 8-corner transform)
		// Object-space center and half-extents
		glm::vec3 center = (localAabbMin + localAabbMax) * 0.5f;
		glm::vec3 extent = (localAabbMax - localAabbMin) * 0.5f;

		// Transform center to world space
		glm::vec3 worldCenter = glm::vec3(model * glm::vec4(center, 1.0f));

		// Compute maximum transformed extent (conservative bounds)
		// Extract the 3x3 rotation-scale portion of the model matrix
		glm::mat3 upperLeft = glm::mat3(model);
		glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
		                      + glm::abs(upperLeft[1]) * extent.y
		                      + glm::abs(upperLeft[2]) * extent.z;

		// Build world-space AABB
		glm::vec3 actualMin = worldCenter - worldExtent;
		glm::vec3 actualMax = worldCenter + worldExtent;

		// Test frustum culling.
		// Skinned bounds can be unstable frame-to-frame under animation, so keep them visible.
		bool isCulled = false;
		if (!cachedItem.useSkinning) {
			const bool hasFiniteAABB = IsFiniteVec3(actualMin) && IsFiniteVec3(actualMax);
			isCulled = hasFiniteAABB ? !frustum.TestAABB(actualMin, actualMax) : false;
		}

		// Debug: Draw AABB if enabled
		if (m_DebugDrawAABBs) {
			glm::vec3 aabbColor = isCulled ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
			SubmitDebugAABB(actualMin, actualMax, aabbColor);
		}
		if (m_DebugDrawBoneAABBs && cachedItem.useSkinning && cachedItem.modelMeshData
			&& ecs.HasComponent<AnimationComponent>(cachedItem.entity)) {
			const auto& animComp = ecs.GetComponent<AnimationComponent>(cachedItem.entity);
			if (animComp.m_animator) {
				const auto& boneTransforms = animComp.m_animator->GetFinalBoneMatrices();
				const auto& boneAabbMin = cachedItem.modelMeshData->boneAabbMin;
				const auto& boneAabbMax = cachedItem.modelMeshData->boneAabbMax;
				const auto& boneAabbValid = cachedItem.modelMeshData->boneAabbValid;
				if (boneAabbMin.size() == boneTransforms.size()
					&& boneAabbMax.size() == boneTransforms.size()) {
					for (size_t i = 0; i < boneTransforms.size(); ++i) {
						if (!boneAabbValid.empty() && boneAabbValid[i] == 0) {
							continue;
						}

						const glm::vec3 localMin = boneAabbMin[i];
						const glm::vec3 localMax = boneAabbMax[i];
						const glm::vec3 center = (localMin + localMax) * 0.5f;
						const glm::vec3 extent = (localMax - localMin) * 0.5f;
						const glm::mat4 boneMatrix = model * boneTransforms[i];

						const glm::vec3 worldCenter = glm::vec3(boneMatrix * glm::vec4(center, 1.0f));
						const glm::mat3 upperLeft = glm::mat3(boneMatrix);
						const glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
							+ glm::abs(upperLeft[1]) * extent.y
							+ glm::abs(upperLeft[2]) * extent.z;

						const glm::vec3 boneMin = worldCenter - worldExtent;
						const glm::vec3 boneMax = worldCenter + worldExtent;
						SubmitDebugAABB(boneMin, boneMax, glm::vec3(1.0f, 0.75f, 0.0f));
					}
				}
			}
		}

		// Build draw command (using cached mesh data - no lookups!)
		DrawElementsIndirectCommand cmd;
		cmd.count = cachedItem.meshData->indexCount;
		cmd.instanceCount = 1;
		cmd.firstIndex = cachedItem.meshData->indexOffset;
		cmd.baseVertex = cachedItem.meshData->baseVertex;
		cmd.baseInstance = 0;

		// Build draw info (using cached data + new transform)
		DrawInfo info;
		info.modelMatrix = model;

		// Pre-calculate normal matrix and store as 3 separate columns (std430 mat3 has 16-byte stride!)
		glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(model)));
		info.normalMatrixCol0 = glm::vec4(normalMat[0], 0.0f);
		info.normalMatrixCol1 = glm::vec4(normalMat[1], 0.0f);
		info.normalMatrixCol2 = glm::vec4(normalMat[2], 0.0f);
		info.aabbMin = localAabbMin;
		info.materialIndex = cachedItem.materialIndex;
		info.aabbMax = localAabbMax;
		info.entityID = static_cast<uint32_t>(cachedItem.entity);

		// Build flags from cached data (fast path - no hierarchy traversal)
		info.flags = 0;
		if (cachedItem.useSkinning)
		{
			info.flags |= FLAG_SKINNING;
		}
		if (cachedItem.isCameraAttached)
		{
			info.flags |= FLAG_CAMERA_ATTACHED;
		}
		if (cachedItem.flickerEmissive)
		{
			info.flags |= FLAG_FLICKER_EMISSIVE;
		}

		info.boneTransformOffset = cachedItem.boneOffset;
		info._pad0 = 0;
		info._pad1 = 0;

		// Route to appropriate buffers based on culling and transparency
		if (isCulled) {
			// Culled meshes - skip all visible passes (depth, picking, geometry, forward)
			culledMeshes++;
		}
		else {
			// ========== VISIBLE MESHES - Route to visible passes ==========

			// PASS 1: PICKING PASS - ALL visible geometry (opaque + transparent, for object selection)
			if (cachedItem.useSkinning) {
				m_PickingSkinnedCommands.push_back(cmd);
				m_PickingSkinnedInfos.push_back(info);
			}
			else {
				m_PickingStandardCommands.push_back(cmd);
				m_PickingStandardInfos.push_back(info);
			}

			// PASS 2: DEPTH PREPASS - Opaque visible geometry only (for early-z rejection)
			// Transparent objects excluded to prevent depth conflicts with objects behind them
			if (!cachedItem.isTransparent) {
				if (cachedItem.useSkinning) {
					m_DepthPrepassSkinnedCommands.push_back(cmd);
					m_DepthPrepassSkinnedInfos.push_back(info);
				}
				else {
					m_DepthPrepassStandardCommands.push_back(cmd);
					m_DepthPrepassStandardInfos.push_back(info);
				}
			}

			// PASS 3: GEOMETRY/FORWARD - Route by shader type and transparency
			if (!cachedItem.isTransparent && !cachedItem.hasCustomShader) {
				// Opaque default shader → Geometry pass (deferred lighting)
				DefaultShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.castsShadows = cachedItem.castsShadows;

				if (cachedItem.useSkinning) {
					m_GeometrySkinnedItems.push_back(item);
					m_GeometrySkinnedVertexCount += cachedItem.meshData->vertexCount;
					m_GeometrySkinnedIndexCount += cmd.count;
				}
				else {
					m_GeometryStandardItems.push_back(item);
					m_GeometryStandardVertexCount += cachedItem.meshData->vertexCount;
					m_GeometryStandardIndexCount += cmd.count;
				}
			}
			else if (!cachedItem.isTransparent && cachedItem.hasCustomShader) {
				// Opaque custom shader → Forward pass (rendered after geometry)
				// Verify it's truly a custom shader with updated logic
				Ermine::graphics::Material* meshMaterial = nullptr;
				if (ecs.HasComponent<Ermine::Material>(cachedItem.materialEntity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(cachedItem.materialEntity);
					meshMaterial = materialComponent.GetMaterial();
				}

				// Double-check with updated HasCustomShader logic
				if (meshMaterial && HasCustomShader(meshMaterial)) {
					// Opaque custom shader pass
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = meshMaterial->GetShader();
					item.castsShadows = cachedItem.castsShadows;

					if (cachedItem.useSkinning) {
						m_ForwardOpaqueCustomSkinnedItems.push_back(item);
					}
					else {
						m_ForwardOpaqueCustomStandardItems.push_back(item);
					}
				}
				else {
					// Was marked as custom but isn't anymore - treat as standard geometry pass
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = cachedItem.castsShadows;

					if (cachedItem.useSkinning) {
						m_GeometrySkinnedItems.push_back(item);
						m_GeometrySkinnedVertexCount += cachedItem.meshData->vertexCount;
						m_GeometrySkinnedIndexCount += cmd.count;
					}
					else {
						m_GeometryStandardItems.push_back(item);
						m_GeometryStandardVertexCount += cachedItem.meshData->vertexCount;
						m_GeometryStandardIndexCount += cmd.count;
					}
				}
			}
			else if (cachedItem.isTransparent && !cachedItem.hasCustomShader) {
				// Transparent default shader → Forward pass (sorted, back-to-front)
				DefaultShaderDrawItem item;
				item.command = cmd;
				item.info = info;
				item.castsShadows = cachedItem.castsShadows;

				if (cachedItem.useSkinning) {
					m_ForwardTransparentDefaultSkinnedItems.push_back(item);
				}
				else {
					m_ForwardTransparentDefaultStandardItems.push_back(item);
				}
				m_ForwardPassDrawCommandsVertexCount += cachedItem.meshData->vertexCount;
				m_ForwardPassDrawCommandsIndexCount += cmd.count;
			}
			else if (cachedItem.isTransparent && cachedItem.hasCustomShader) {
				// Transparent custom shader → Forward pass (sorted, back-to-front)
				// Check if transparent has custom shader
				Ermine::graphics::Material* meshMaterial = nullptr;
				if (ecs.HasComponent<Ermine::Material>(cachedItem.materialEntity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(cachedItem.materialEntity);
					meshMaterial = materialComponent.GetMaterial();
				}

				// Double-check with updated HasCustomShader logic
				if (meshMaterial && HasCustomShader(meshMaterial)) {
					// Transparent custom shader pass
					CustomShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.shader = meshMaterial->GetShader();
					item.castsShadows = cachedItem.castsShadows;

					if (cachedItem.useSkinning) {
						m_ForwardTransparentCustomSkinnedItems.push_back(item);
					}
					else {
						m_ForwardTransparentCustomStandardItems.push_back(item);
					}
				}
				else {
					// Was marked as custom but isn't anymore - treat as standard transparent
					DefaultShaderDrawItem item;
					item.command = cmd;
					item.info = info;
					item.castsShadows = cachedItem.castsShadows;

					if (cachedItem.useSkinning) {
						m_ForwardTransparentDefaultSkinnedItems.push_back(item);
					}
					else {
						m_ForwardTransparentDefaultStandardItems.push_back(item);
					}
				}
				m_ForwardPassDrawCommandsVertexCount += cachedItem.meshData->vertexCount;
				m_ForwardPassDrawCommandsIndexCount += cmd.count;
			}
		}

		// PASS 4: SHADOW PASS - ALL geometry (visible OR culled) that casts shadows
		if (cachedItem.castsShadows) {
			if (cachedItem.useSkinning) {
				m_ShadowSkinnedCommands.push_back(cmd);
				m_ShadowSkinnedInfos.push_back(info);
			}
			else {
				m_ShadowStandardCommands.push_back(cmd);
				m_ShadowStandardInfos.push_back(info);
			}
		}
	}

	GPUProfiler::SetCulledMeshesCount(culledMeshes);

	// ========== SORTING ==========
	// Transparent sorting happens later during RenderGeometryPass when view matrix is available
	// (Sorting requires camera position calculated from view matrix)
	// Fast path uses distance-only sorting within existing shader groups

	// ========== WRITE ALL DRAW DATA TO GPU BUFFERS ==========
	// PASS 1: PICKING PASS - ALL visible geometry (opaque + transparent, for object selection)
	m_MeshManager.m_PickingStandardDrawCommandBuffer.WriteCommands(m_PickingStandardCommands, 0);
	m_MeshManager.m_PickingStandardDrawInfoBuffer.WriteDrawInfos(m_PickingStandardInfos, 0);
	m_MeshManager.m_PickingSkinnedDrawCommandBuffer.WriteCommands(m_PickingSkinnedCommands, 0);
	m_MeshManager.m_PickingSkinnedDrawInfoBuffer.WriteDrawInfos(m_PickingSkinnedInfos, 0);

	// PASS 2: DEPTH PREPASS - Opaque visible geometry only (for early-z rejection)
	m_MeshManager.m_DepthPrepassStandardDrawCommandBuffer.WriteCommands(m_DepthPrepassStandardCommands, 0);
	m_MeshManager.m_DepthPrepassStandardDrawInfoBuffer.WriteDrawInfos(m_DepthPrepassStandardInfos, 0);
	m_MeshManager.m_DepthPrepassSkinnedDrawCommandBuffer.WriteCommands(m_DepthPrepassSkinnedCommands, 0);
	m_MeshManager.m_DepthPrepassSkinnedDrawInfoBuffer.WriteDrawInfos(m_DepthPrepassSkinnedInfos, 0);

	// Extract commands and infos from Items for geometry pass
	std::vector<DrawElementsIndirectCommand> geometryStandardCommands;
	std::vector<DrawInfo> geometryStandardInfos;
	geometryStandardCommands.reserve(m_GeometryStandardItems.size());
	geometryStandardInfos.reserve(m_GeometryStandardItems.size());
	for (const auto& item : m_GeometryStandardItems) {
		geometryStandardCommands.push_back(item.command);
		geometryStandardInfos.push_back(item.info);
	}

	std::vector<DrawElementsIndirectCommand> geometrySkinnedCommands;
	std::vector<DrawInfo> geometrySkinnedInfos;
	geometrySkinnedCommands.reserve(m_GeometrySkinnedItems.size());
	geometrySkinnedInfos.reserve(m_GeometrySkinnedItems.size());
	for (const auto& item : m_GeometrySkinnedItems) {
		geometrySkinnedCommands.push_back(item.command);
		geometrySkinnedInfos.push_back(item.info);
	}

	m_MeshManager.m_GeometryStandardDrawCommandBuffer.WriteCommands(geometryStandardCommands, 0);
	m_MeshManager.m_GeometryStandardDrawInfoBuffer.WriteDrawInfos(geometryStandardInfos, 0);
	m_MeshManager.m_GeometrySkinnedDrawCommandBuffer.WriteCommands(geometrySkinnedCommands, 0);
	m_MeshManager.m_GeometrySkinnedDrawInfoBuffer.WriteDrawInfos(geometrySkinnedInfos, 0);

	// Extract commands and infos from Items for forward transparent default pass
	std::vector<DrawElementsIndirectCommand> forwardTransparentDefaultStandardCommands;
	std::vector<DrawInfo> forwardTransparentDefaultStandardInfos;
	forwardTransparentDefaultStandardCommands.reserve(m_ForwardTransparentDefaultStandardItems.size());
	forwardTransparentDefaultStandardInfos.reserve(m_ForwardTransparentDefaultStandardItems.size());
	for (const auto& item : m_ForwardTransparentDefaultStandardItems) {
		forwardTransparentDefaultStandardCommands.push_back(item.command);
		forwardTransparentDefaultStandardInfos.push_back(item.info);
	}

	std::vector<DrawElementsIndirectCommand> forwardTransparentDefaultSkinnedCommands;
	std::vector<DrawInfo> forwardTransparentDefaultSkinnedInfos;
	forwardTransparentDefaultSkinnedCommands.reserve(m_ForwardTransparentDefaultSkinnedItems.size());
	forwardTransparentDefaultSkinnedInfos.reserve(m_ForwardTransparentDefaultSkinnedItems.size());
	for (const auto& item : m_ForwardTransparentDefaultSkinnedItems) {
		forwardTransparentDefaultSkinnedCommands.push_back(item.command);
		forwardTransparentDefaultSkinnedInfos.push_back(item.info);
	}

	m_MeshManager.m_ForwardStandardDrawCommandBuffer.WriteCommands(forwardTransparentDefaultStandardCommands, 0);
	m_MeshManager.m_ForwardStandardDrawInfoBuffer.WriteDrawInfos(forwardTransparentDefaultStandardInfos, 0);
	m_MeshManager.m_ForwardSkinnedDrawCommandBuffer.WriteCommands(forwardTransparentDefaultSkinnedCommands, 0);
	m_MeshManager.m_ForwardSkinnedDrawInfoBuffer.WriteDrawInfos(forwardTransparentDefaultSkinnedInfos, 0);

	// PASS 5: SHADOW PASS - ALL geometry with castsShadows=true (instanced for cascaded shadow maps)

	std::vector<DrawElementsIndirectCommand> shadowStandardCommands;
	shadowStandardCommands.reserve(m_ShadowStandardCommands.size());

	for (const auto& cmd : m_ShadowStandardCommands) {
		DrawElementsIndirectCommand shadowCmd = cmd;
		shadowCmd.instanceCount = m_TotalShadowInstances;
		shadowStandardCommands.push_back(shadowCmd);
	}

	std::vector<DrawElementsIndirectCommand> shadowSkinnedCommands;
	shadowSkinnedCommands.reserve(m_ShadowSkinnedCommands.size());

	for (const auto& cmd : m_ShadowSkinnedCommands) {
		DrawElementsIndirectCommand shadowCmd = cmd;
		shadowCmd.instanceCount = m_TotalShadowInstances;
		shadowSkinnedCommands.push_back(shadowCmd);
	}

	m_MeshManager.m_ShadowStandardDrawCommandBuffer.WriteCommands(shadowStandardCommands, 0);
	m_MeshManager.m_ShadowStandardDrawInfoBuffer.WriteDrawInfos(m_ShadowStandardInfos, 0);
	m_MeshManager.m_ShadowSkinnedDrawCommandBuffer.WriteCommands(shadowSkinnedCommands, 0);
	m_MeshManager.m_ShadowSkinnedDrawInfoBuffer.WriteDrawInfos(m_ShadowSkinnedInfos, 0);

	// ========== UPLOAD CUSTOM SHADER BUFFERS (FAST PATH - glBufferSubData) ==========
	// Extract and upload opaque custom shader standard items
	if (!m_ForwardOpaqueCustomStandardItems.empty()) {
		std::vector<DrawElementsIndirectCommand> commands;
		std::vector<DrawInfo> infos;
		commands.reserve(m_ForwardOpaqueCustomStandardItems.size());
		infos.reserve(m_ForwardOpaqueCustomStandardItems.size());

		for (const auto& item : m_ForwardOpaqueCustomStandardItems) {
			commands.push_back(item.command);
			infos.push_back(item.info);
		}

		if (m_ForwardOpaqueCustomStandardCmdBuffer == 0) {
			glGenBuffers(1, &m_ForwardOpaqueCustomStandardCmdBuffer);
			glGenBuffers(1, &m_ForwardOpaqueCustomStandardInfoBuffer);
		}

		size_t cmdSize = commands.size();
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomStandardCmdBuffer);
		if (cmdSize > m_ForwardOpaqueCustomStandardCmdBufferCapacity) {
			glBufferData(GL_DRAW_INDIRECT_BUFFER,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardOpaqueCustomStandardCmdBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardOpaqueCustomStandardInfoBuffer);
		if (cmdSize > m_ForwardOpaqueCustomStandardInfoBufferCapacity) {
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				cmdSize * sizeof(DrawInfo),
				infos.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardOpaqueCustomStandardInfoBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				cmdSize * sizeof(DrawInfo),
				infos.data());
		}

		m_ForwardOpaqueCustomStandardUploadedCount = cmdSize;
	}
	else {
		m_ForwardOpaqueCustomStandardUploadedCount = 0;
	}

	// Extract and upload opaque custom shader skinned items
	if (!m_ForwardOpaqueCustomSkinnedItems.empty()) {
		std::vector<DrawElementsIndirectCommand> commands;
		std::vector<DrawInfo> infos;
		commands.reserve(m_ForwardOpaqueCustomSkinnedItems.size());
		infos.reserve(m_ForwardOpaqueCustomSkinnedItems.size());

		for (const auto& item : m_ForwardOpaqueCustomSkinnedItems) {
			commands.push_back(item.command);
			infos.push_back(item.info);
		}

		if (m_ForwardOpaqueCustomSkinnedCmdBuffer == 0) {
			glGenBuffers(1, &m_ForwardOpaqueCustomSkinnedCmdBuffer);
			glGenBuffers(1, &m_ForwardOpaqueCustomSkinnedInfoBuffer);
		}

		size_t cmdSize = commands.size();
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomSkinnedCmdBuffer);
		if (cmdSize > m_ForwardOpaqueCustomSkinnedCmdBufferCapacity) {
			glBufferData(GL_DRAW_INDIRECT_BUFFER,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardOpaqueCustomSkinnedCmdBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardOpaqueCustomSkinnedInfoBuffer);
		if (cmdSize > m_ForwardOpaqueCustomSkinnedInfoBufferCapacity) {
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				cmdSize * sizeof(DrawInfo),
				infos.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardOpaqueCustomSkinnedInfoBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				cmdSize * sizeof(DrawInfo),
				infos.data());
		}

		m_ForwardOpaqueCustomSkinnedUploadedCount = cmdSize;
	}
	else {
		m_ForwardOpaqueCustomSkinnedUploadedCount = 0;
	}

	// Extract and upload transparent custom shader standard items
	if (!m_ForwardTransparentCustomStandardItems.empty()) {
		std::vector<DrawElementsIndirectCommand> commands;
		std::vector<DrawInfo> infos;
		commands.reserve(m_ForwardTransparentCustomStandardItems.size());
		infos.reserve(m_ForwardTransparentCustomStandardItems.size());

		for (const auto& item : m_ForwardTransparentCustomStandardItems) {
			commands.push_back(item.command);
			infos.push_back(item.info);
		}

		if (m_ForwardTransparentCustomStandardCmdBuffer == 0) {
			glGenBuffers(1, &m_ForwardTransparentCustomStandardCmdBuffer);
			glGenBuffers(1, &m_ForwardTransparentCustomStandardInfoBuffer);
		}

		size_t cmdSize = commands.size();
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomStandardCmdBuffer);
		if (cmdSize > m_ForwardTransparentCustomStandardCmdBufferCapacity) {
			glBufferData(GL_DRAW_INDIRECT_BUFFER,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardTransparentCustomStandardCmdBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomStandardInfoBuffer);
		if (cmdSize > m_ForwardTransparentCustomStandardInfoBufferCapacity) {
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				cmdSize * sizeof(DrawInfo),
				infos.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardTransparentCustomStandardInfoBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				cmdSize * sizeof(DrawInfo),
				infos.data());
		}

		m_ForwardTransparentCustomStandardUploadedCount = cmdSize;
	}
	else {
		m_ForwardTransparentCustomStandardUploadedCount = 0;
	}

	// Extract and upload transparent custom shader skinned items
	if (!m_ForwardTransparentCustomSkinnedItems.empty()) {
		std::vector<DrawElementsIndirectCommand> commands;
		std::vector<DrawInfo> infos;
		commands.reserve(m_ForwardTransparentCustomSkinnedItems.size());
		infos.reserve(m_ForwardTransparentCustomSkinnedItems.size());

		for (const auto& item : m_ForwardTransparentCustomSkinnedItems) {
			commands.push_back(item.command);
			infos.push_back(item.info);
		}

		if (m_ForwardTransparentCustomSkinnedCmdBuffer == 0) {
			glGenBuffers(1, &m_ForwardTransparentCustomSkinnedCmdBuffer);
			glGenBuffers(1, &m_ForwardTransparentCustomSkinnedInfoBuffer);
		}

		size_t cmdSize = commands.size();
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomSkinnedCmdBuffer);
		if (cmdSize > m_ForwardTransparentCustomSkinnedCmdBufferCapacity) {
			glBufferData(GL_DRAW_INDIRECT_BUFFER,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardTransparentCustomSkinnedCmdBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
				cmdSize * sizeof(DrawElementsIndirectCommand),
				commands.data());
		}

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomSkinnedInfoBuffer);
		if (cmdSize > m_ForwardTransparentCustomSkinnedInfoBufferCapacity) {
			glBufferData(GL_SHADER_STORAGE_BUFFER,
				cmdSize * sizeof(DrawInfo),
				infos.data(),
				GL_DYNAMIC_DRAW);
			m_ForwardTransparentCustomSkinnedInfoBufferCapacity = cmdSize;
		}
		else {
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
				cmdSize * sizeof(DrawInfo),
				infos.data());
		}

		m_ForwardTransparentCustomSkinnedUploadedCount = cmdSize;
	}
	else {
		m_ForwardTransparentCustomSkinnedUploadedCount = 0;
	}

	// Unbind buffers
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
 * @brief Calculate a hash of the entity list to detect add/remove operations.
 * @return Hash value representing current entity list state.
 */
uint64_t Renderer::CalculateEntityListHash() const
{
	uint64_t hash = 0;

	// Hash entity count from each source
	hash ^= std::hash<size_t>{}(m_Entities.size()) << 1;
	hash ^= std::hash<size_t>{}(m_ModelSystem->m_Entities.size()) << 2;

	// Simple hash of entity IDs (order matters)
	for (auto entity : m_Entities) {
		hash = hash * 31 + std::hash<EntityID>{}(entity);
	}
	for (auto entity : m_ModelSystem->m_Entities) {
		hash = hash * 31 + std::hash<EntityID>{}(entity);
	}

	return hash;
}

/**
 * @brief Calculate a hash of an entity's transform to detect movement.
 * @param entity The entity to hash.
 * @return Hash value representing entity's current transform state.
 */
uint64_t Renderer::CalculateEntityTransformHash(EntityID entity) const
{
	const auto& ecs = Ermine::ECS::GetInstance();

	if (!ecs.HasComponent<Transform>(entity)) {
		return 0;
	}

	const auto& trans = ecs.GetComponent<Transform>(entity);

	// Hash position, rotation, scale
	uint64_t hash = 0;
	hash ^= std::hash<float>{}(trans.position.x) << 1;
	hash ^= std::hash<float>{}(trans.position.y) << 2;
	hash ^= std::hash<float>{}(trans.position.z) << 3;
	hash ^= std::hash<float>{}(trans.rotation.x) << 4;
	hash ^= std::hash<float>{}(trans.rotation.y) << 5;
	hash ^= std::hash<float>{}(trans.rotation.z) << 6;
	hash ^= std::hash<float>{}(trans.rotation.w) << 7;
	hash ^= std::hash<float>{}(trans.scale.x) << 8;
	hash ^= std::hash<float>{}(trans.scale.y) << 9;
	hash ^= std::hash<float>{}(trans.scale.z) << 10;

	return hash;
}

/**
 * @brief Check if the entity list has changed since last frame.
 * @return True if entities were added/removed.
 */
bool Renderer::HasEntityListChanged() const
{
	uint64_t currentHash = CalculateEntityListHash();
	return currentHash != m_LastEntityListHash;
}

/**
 * @brief Renders the lighting pass for deferred rendering.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderLightingPass(const Mtx44& view, const Mtx44& projection)
{
	if (!m_GBuffer || !m_LightPassShader)
	{
		EE_CORE_ERROR("G-Buffer or lighting shader not initialized!");
		return;
	}

	// Explicitly bind lighting shader BEFORE setting up framebuffer state
	m_LightPassShader->Bind();

	BeginLightingPass();

	// Re-bind to ensure it's still active after framebuffer setup
	m_LightPassShader->Bind();

	// Bind g-buffer textures for reading
	BindGBufferTextures();

	// Calculate and set inverse matrices for world position reconstruction
	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);
	glm::mat4 glmProjection = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);

	glm::mat4 invView = glm::inverse(glmView);
	glm::mat4 invProjection = glm::inverse(glmProjection);
	m_LightPassShader->SetUniformMatrix4fv("view", glmView);
	m_LightPassShader->SetUniformMatrix4fv("invView", invView);
	m_LightPassShader->SetUniformMatrix4fv("invProjection", invProjection);
	m_LightPassShader->SetUniformMatrix4fv("projection", glmProjection);
	m_LightPassShader->SetUniform1i("u_SSAO", m_SSAOEnabled ? 1 : 0);
	m_LightPassShader->SetUniform1i("u_SSAOSamples", m_SSAOSamples);
	m_LightPassShader->SetUniform1f("u_SSAORadius", m_SSAORadius);
	m_LightPassShader->SetUniform1f("u_SSAOBias", m_SSAOBias);
	m_LightPassShader->SetUniform1f("u_SSAOIntensity", m_SSAOIntensity);
	m_LightPassShader->SetUniform1f("u_SSAOFadeout", m_SSAOFadeout);
	m_LightPassShader->SetUniform1f("u_SSAOMaxDistance", m_SSAOMaxDistance);

	// Bind IGN texture handle for jittering
	if (m_IGNTextureHandle != 0)
	{
		GLint locIGN = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_IGNHandle");
		if (locIGN != -1)
		{
			glUniform2ui(locIGN,
				static_cast<GLuint>(m_IGNTextureHandle),
				static_cast<GLuint>(m_IGNTextureHandle >> 32));
		}
	}

	// Send IGN texture resolution for proper UV scaling
	m_LightPassShader->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

	// Set fog parameters
	m_LightPassShader->SetUniform1i("u_FogEnabled", m_FogEnabled ? 1 : 0);
	m_LightPassShader->SetUniform1i("u_FogMode", m_FogMode);
	m_LightPassShader->SetUniform3f("u_FogColor", m_FogColor);
	m_LightPassShader->SetUniform1f("u_FogDensity", m_FogDensity);
	m_LightPassShader->SetUniform1f("u_FogStart", m_FogStart);
	m_LightPassShader->SetUniform1f("u_FogEnd", m_FogEnd);
	m_LightPassShader->SetUniform1f("u_FogHeightCoefficient", m_FogHeightCoefficient);
	m_LightPassShader->SetUniform1f("u_FogHeightFalloff", m_FogHeightFalloff);

	// Set ambient lighting parameters
	m_LightPassShader->SetUniform3f("u_AmbientColor", m_AmbientColor);
	m_LightPassShader->SetUniform1f("u_AmbientIntensity", m_AmbientIntensity);

	// Set light probe parameters
	m_LightPassShader->SetUniform1i("u_LightProbesEnabled", m_LightProbesEnabled ? 1 : 0);

	// Set shading mode
	m_LightPassShader->SetUniform1i("u_ShadingMode", m_IsBlinnPhong ? 1 : 0);

	// Render fullscreen quad
	if (m_QuadMesh.vertex_array && m_QuadMesh.index_buffer)
	{
		Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);
	}


	EndLightingPass();
}

/**
 * @brief Render motion blur using the per-pixel velocity GBuffer.
 *        Reads from m_PostProcessBuffer, writes to m_MotionBlurBuffer.
 *        Camera-attached objects have zero velocity (written in vertex shader) so they're never blurred.
 */
void Renderer::RenderMotionBlurPass()
{
	if (!m_MotionBlurEnabled || !m_MotionBlurShader || !m_MotionBlurShader->IsValid())
		return;
	if (!m_MotionBlurBuffer || !m_PostProcessBuffer || !m_GBuffer)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, m_MotionBlurBuffer->FBO);
	glViewport(0, 0, m_MotionBlurBuffer->width, m_MotionBlurBuffer->height);
	glDisable(GL_DEPTH_TEST);

	m_MotionBlurShader->Bind();

	// Input color from lighting/forward pass
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_PostProcessBuffer->ColorTexture);
	m_MotionBlurShader->SetUniform1i("u_ColorTexture", 0);

	// Bindless depth handle
	GLint locDepth = glGetUniformLocation(m_MotionBlurShader->GetRendererID(), "u_DepthHandle");
	if (locDepth != -1)
	{
		glUniform2ui(locDepth,
			static_cast<GLuint>(m_GBuffer->HandleDepthTexture),
			static_cast<GLuint>(m_GBuffer->HandleDepthTexture >> 32));
	}

	// Bindless velocity handle (RT4)
	GLint locVel = glGetUniformLocation(m_MotionBlurShader->GetRendererID(), "u_VelocityHandle");
	if (locVel != -1)
	{
		glUniform2ui(locVel,
			static_cast<GLuint>(m_GBuffer->HandlePackedTexture4),
			static_cast<GLuint>(m_GBuffer->HandlePackedTexture4 >> 32));
	}

	m_MotionBlurShader->SetUniform1f("u_MotionBlurStrength", m_MotionBlurStrength);
	m_MotionBlurShader->SetUniform1i("u_NumSamples", m_MotionBlurSamples);
	m_MotionBlurShader->SetUniform1i("u_FirstFrame", (frameCounter == 0) ? 1 : 0);

	// Camera near/far for depth linearization in depth-aware blur rejection
	float nearClip = 0.1f;
	float farClip = 1000.0f;
	auto& ecs = ECS::GetInstance();
#if defined(EE_EDITOR)
	if (editor::EditorGUI::isPlaying)
	{
		auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
		if (gameCamera && gameCamera->HasValidCamera())
		{
			nearClip = gameCamera->GetNearClip();
			farClip = gameCamera->GetFarClip();
		}
		else
		{
			const auto& editorCamera = editor::EditorCamera::GetInstance();
			nearClip = editorCamera.GetNearClip();
			farClip = editorCamera.GetFarClip();
		}
	}
	else
	{
		const auto& editorCamera = editor::EditorCamera::GetInstance();
		nearClip = editorCamera.GetNearClip();
		farClip = editorCamera.GetFarClip();
	}
#else
	auto gameCamera = ecs.GetSystem<graphics::CameraSystem>();
	if (gameCamera && gameCamera->HasValidCamera())
	{
		nearClip = gameCamera->GetNearClip();
		farClip = gameCamera->GetFarClip();
	}
#endif
	m_MotionBlurShader->SetUniform1f("u_NearClip", nearClip);
	m_MotionBlurShader->SetUniform1f("u_FarClip", farClip);

	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);
}

/**
 * @brief Render post-processing effects using the lighting pass output
 * @param view The view matrix
 * @param projection The projection matrix
 */
void Renderer::RenderPostProcessPass(const Mtx44& view, const Mtx44& projection)
{
	if (!m_PostProcessBuffer || !m_BloomShader || !m_PostProcessShader || !m_AAShader)
	{
		EE_CORE_ERROR("Post-process buffers or shaders not initialized!");
		return;
	}

#ifdef EE_EDITOR
	RenderOutlineMaskPass(view, projection);
#endif

	glDisable(GL_DEPTH_TEST);

	// Pass 1: Extract bright areas + volumetric god rays (at half resolution)
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomExtractBuffer->FBO);
	glViewport(0, 0, m_BloomExtractBuffer->width, m_BloomExtractBuffer->height);

	m_BloomShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_PostProcessBuffer->ColorTexture);
	m_BloomShader->SetUniform1i("u_LightingTexture", 0);
	m_BloomShader->SetUniform1i("u_Pass", 1);

	// Set bloom extraction parameters
	m_BloomShader->SetUniform1f("u_BloomThreshold", m_BloomThreshold);
	m_BloomShader->SetUniform1f("u_BloomRadius", m_BloomRadius);

	// Bind light SSBO for god rays
	BindLightsSSBO();

	// Bind bindless textures for god rays
	if (m_GBuffer)
	{
		// Depth texture handle
		GLint locDepth = glGetUniformLocation(m_BloomShader->GetRendererID(), "u_GBufferDepthHandle");
		if (locDepth != -1)
		{
			glUniform2ui(locDepth,
				static_cast<GLuint>(m_GBuffer->HandleDepthTexture),
				static_cast<GLuint>(m_GBuffer->HandleDepthTexture >> 32));
		}
	}

	// Shadow map array handle
	GLint locShadow = glGetUniformLocation(m_BloomShader->GetRendererID(), "u_ShadowMapArrayHandle");
	if (locShadow != -1 && m_ShadowMapArrayHandle != 0)
	{
		glUniform2ui(locShadow,
			static_cast<GLuint>(m_ShadowMapArrayHandle),
			static_cast<GLuint>(m_ShadowMapArrayHandle >> 32));
	}

	// IGN texture handle
	if (m_IGNTextureHandle != 0)
	{
		GLint locIGN = glGetUniformLocation(m_BloomShader->GetRendererID(), "u_IGNHandle");
		if (locIGN != -1)
		{
			glUniform2ui(locIGN,
				static_cast<GLuint>(m_IGNTextureHandle),
				static_cast<GLuint>(m_IGNTextureHandle >> 32));
		}
	}

	// IGN resolution
	m_BloomShader->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

	// Camera position and matrices - convert from custom Mtx44 to glm::mat4
	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);
	glm::mat4 glmProjection = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);

	glm::mat4 invView = glm::inverse(glmView);
	glm::mat4 invProjection = glm::inverse(glmProjection);
	glm::vec3 cameraPos = glm::vec3(invView[3]); // Extract camera position from inverse view matrix

	m_BloomShader->SetUniform3f("u_CameraPosition", cameraPos);
	m_BloomShader->SetUniformMatrix4fv("u_InvProjection", invProjection);
	m_BloomShader->SetUniformMatrix4fv("u_InvView", invView);

	// Spotlight ray parameters
	m_BloomShader->SetUniform1i("u_SpotlightRays", m_SpotlightRaysEnabled ? 1 : 0);
	m_BloomShader->SetUniform1f("u_SpotlightRayIntensity", m_SpotlightRayIntensity);

	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Pass 2: Horizontal blur (half resolution)
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomBlurBuffer1->FBO);
	glViewport(0, 0, m_BloomBlurBuffer1->width, m_BloomBlurBuffer1->height);

	m_BloomShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_BloomExtractBuffer->ColorTexture);
	m_BloomShader->SetUniform1i("u_LightingTexture", 0);
	m_BloomShader->SetUniform1i("u_Pass", 2);
	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Pass 3: Vertical blur (half resolution)
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomBlurBuffer2->FBO);
	glViewport(0, 0, m_BloomBlurBuffer2->width, m_BloomBlurBuffer2->height);

	m_BloomShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_BloomBlurBuffer1->ColorTexture);
	m_BloomShader->SetUniform1i("u_LightingTexture", 0);
	m_BloomShader->SetUniform1i("u_Pass", 3);
	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Pass 4: Second horizontal blur for creamy smooth result (half resolution, ping-pong back)
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomBlurBuffer1->FBO);
	glViewport(0, 0, m_BloomBlurBuffer1->width, m_BloomBlurBuffer1->height);

	m_BloomShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_BloomBlurBuffer2->ColorTexture);
	m_BloomShader->SetUniform1i("u_LightingTexture", 0);
	m_BloomShader->SetUniform1i("u_Pass", 2);
	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Pass 5: Second vertical blur for creamy smooth result (half resolution, ping-pong forward)
	glBindFramebuffer(GL_FRAMEBUFFER, m_BloomBlurBuffer2->FBO);
	glViewport(0, 0, m_BloomBlurBuffer2->width, m_BloomBlurBuffer2->height);

	m_BloomShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_BloomBlurBuffer1->ColorTexture);
	m_BloomShader->SetUniform1i("u_LightingTexture", 0);
	m_BloomShader->SetUniform1i("u_Pass", 3);
	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Pass 6: Post-processing (tone mapping, bloom combine, etc.) - restore full resolution
	glViewport(0, 0, m_AntiAliasingBuffer->width, m_AntiAliasingBuffer->height);
	glBindFramebuffer(GL_FRAMEBUFFER, m_AntiAliasingBuffer->FBO);
	glClear(GL_COLOR_BUFFER_BIT);

	m_PostProcessShader->Bind();

	// Bind main scene texture (already includes motion blur if enabled in deferred stage)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_PostProcessBuffer->ColorTexture);
	m_PostProcessShader->SetUniform1i("u_LightingTexture", 0);

	// Bind bloom texture
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BloomBlurBuffer2->ColorTexture);
	m_PostProcessShader->SetUniform1i("u_BloomTexture", 1);

#ifdef EE_EDITOR
	// Outline mask + params
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_OutlineMaskTexture);
	m_PostProcessShader->SetUniform1i("u_OutlineMask", 2);
	
	m_PostProcessShader->SetUniform1i("u_OutlineEnabled", m_OutlineEnabled);
	m_PostProcessShader->SetUniform3f("u_OutlineColor", m_OutlineColor);
	m_PostProcessShader->SetUniform1f("u_OutlineThickness", m_OutlineThickness);
	m_PostProcessShader->SetUniform1f("u_OutlineIntensity", m_OutlineIntensity);
#else
	m_PostProcessShader->SetUniform1i("u_OutlineEnabled", 0);
#endif

	// Pass bindless depth texture handle
	GLint locDepth = glGetUniformLocation(m_PostProcessShader->GetRendererID(), "u_GBufferDepthHandle");
	if (locDepth != -1 && m_GBuffer)
	{
		glUniform2ui(locDepth, static_cast<GLuint>(m_GBuffer->HandleDepthTexture),
			static_cast<GLuint>(m_GBuffer->HandleDepthTexture >> 32));
	}

	// Set post-processing toggle parameters
	m_PostProcessShader->SetUniform1i("u_Vignette", m_VignetteEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1i("u_ToneMapping", m_ToneMappingEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1i("u_GammaCorrection", m_GammaCorrectionEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1i("u_Bloom", m_BloomEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1i("u_SkyboxIsHDR", m_SkyBoxisHDR ? 1 : 0);

	// Set post-processing value parameters
	m_PostProcessShader->SetUniform1f("u_Exposure", m_Exposure);
	m_PostProcessShader->SetUniform1f("u_Contrast", m_Contrast);
	m_PostProcessShader->SetUniform1f("u_Saturation", m_Saturation);
	m_PostProcessShader->SetUniform1f("u_Gamma", m_Gamma);
	m_PostProcessShader->SetUniform1f("u_VignetteIntensity", m_VignetteIntensity);
	m_PostProcessShader->SetUniform1f("u_VignetteRadius", m_VignetteRadius);
	m_PostProcessShader->SetUniform1f("u_BloomStrength", m_BloomStrength);

	// Film grain and chromatic aberration
	m_PostProcessShader->SetUniform1i("u_FilmGrain", m_FilmGrainEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1f("u_GrainIntensity", m_GrainIntensity);
	m_PostProcessShader->SetUniform1f("u_GrainScale", m_GrainScale);
	m_PostProcessShader->SetUniform1i("u_ChromaticAberration", m_ChromaticAberrationEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1f("u_ChromaticAmount", m_ChromaticAmount);
	m_PostProcessShader->SetUniform1i("u_RadialBlur", m_RadialBlurEnabled ? 1 : 0);
	m_PostProcessShader->SetUniform1f("u_RadialBlurStrength", m_RadialBlurStrength);
	m_PostProcessShader->SetUniform1i("u_RadialBlurSamples", m_RadialBlurSamples);
	m_PostProcessShader->SetUniform2f("u_RadialBlurCenter", m_RadialBlurCenter.x, m_RadialBlurCenter.y);

	// Bind noise texture for film grain (use texture unit 3 to avoid conflict with outline mask on unit 2)
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, m_NoiseTexture);
	m_PostProcessShader->SetUniform1i("u_NoiseTexture", 3);
	// Animate noise by offsetting UV based on time
	m_PostProcessShader->SetUniform2f("u_NoiseOffset", std::fmod(m_ElapsedTime * 10.0f, 1.0f), std::fmod(m_ElapsedTime * 7.0f, 1.0f));

	// Bind optional vignette map texture on texture unit 4
	const bool hasVignetteMap = (m_VignetteMapTexture && m_VignetteMapTexture->IsValid());
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, hasVignetteMap ? m_VignetteMapTexture->GetRendererID() : 0);
	m_PostProcessShader->SetUniform1i("u_VignetteMap", 4);
	m_PostProcessShader->SetUniform1i("u_HasVignetteMap", hasVignetteMap ? 1 : 0);
	m_PostProcessShader->SetUniform1f("u_VignetteCoverage", m_VignetteCoverage);
	m_PostProcessShader->SetUniform1f("u_VignetteFalloff", m_VignetteFalloff);
	m_PostProcessShader->SetUniform1f("u_VignetteMapStrength", m_VignetteMapStrength);
	m_PostProcessShader->SetUniform3f("u_VignetteMapRGBModifier", m_VignetteMapRGBModifier);

	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	// Motion blur pass removed (disabled).

	// Final pass: FXAA
#if defined(EE_EDITOR)
	glBindFramebuffer(GL_FRAMEBUFFER, m_OffscreenBuffer->FBO);
	glViewport(0, 0, m_OffscreenBuffer->width, m_OffscreenBuffer->height);
#else
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

	glClear(GL_COLOR_BUFFER_BIT);

	m_AAShader->Bind();
	glActiveTexture(GL_TEXTURE0);
	// Motion blur output disabled; always use anti-aliasing buffer
	glBindTexture(GL_TEXTURE_2D, m_AntiAliasingBuffer->ColorTexture);
	m_AAShader->SetUniform1i("u_LightingTexture", 0);

	// Set FXAA parameters
	m_AAShader->SetUniform1i("u_FXAA", m_FXAAEnabled ? 1 : 0);
	m_AAShader->SetUniform1f("u_FXAASpanMax", m_FXAASpanMax);
	m_AAShader->SetUniform1f("u_FXAAReduceMin", m_FXAAReduceMin);
	m_AAShader->SetUniform1f("u_FXAAReduceMul", m_FXAAReduceMul);

	Draw(m_QuadMesh.vertex_array, m_QuadMesh.index_buffer);

	glEnable(GL_DEPTH_TEST);
}

/**
 * @brief Renders all submitted debug lines using the provided view and projection matrices.
 * @param view View matrix for the camera (custom Mtx44 type).
 * @param proj Projection matrix for the camera (custom Mtx44 type).
 */
void Renderer::RenderDebugLines(const Mtx44& view, const Mtx44& proj)
{
	RenderDebugLines(ToGlm(view), ToGlm(proj));
}

/**
 * @brief Complete deferred rendering pipeline
 * @param view The view matrix
 * @param projection The projection matrix
 */
void Renderer::RenderDeferredPipeline(const Mtx44& view, const Mtx44& projection)
{
	// Compile draw data for all passes (routes opaque to geometry, transparent to forward)
	// MUST be called first - populates draw command buffers for all subsequent passes
	CompileDrawData();

	// Ensure SSBO writes visible to all passes
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

	// Shadow pass - render scene from light's perspective (independent of G-buffer)
	// Runs BEFORE depth pre-pass to avoid GL state pollution from depth pre-pass
	if (frameCounter % SHADOW_MAP_REFRESH_INTERVAL_IN_FRAMES == 0)
	{
		RenderShadowPass();
	}

	// Depth pre-pass - render depth-only to eliminate fragment shader overdraw
	RenderDepthPrePass(view, projection);

	// Geometry pass - write opaque objects to g-buffer, collect transparent objects
	RenderGeometryPass(view, projection);

	// Lighting pass - read from g-buffer and perform lighting on opaque objects
	RenderLightingPass(view, projection);

	// Render skybox after lighting but before transparent objects
	// No depth blit needed - PostProcess FBO shares G-Buffer's depth texture
	if (m_ShowSkybox && m_skybox && m_skybox->IsValid() && m_PostProcessBuffer && m_GBuffer) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

		// Enable depth testing but render only where depth = 1.0 (background)
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);

		// Render skybox
		m_skybox->Render(view, projection);

		// Restore depth state
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);
	}
	else if (m_PostProcessBuffer && m_GBuffer) {
		// No skybox, but set up framebuffer and depth state for forward pass
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);

		// Set up depth state for subsequent opaque custom shader pass
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
	}

	// Motion blur pass - apply to deferred/skybox layer only.
	// Forward-rendered content (including camera-attached objects) is composited after this, unblurred.
	RenderMotionBlurPass();

	// When motion blur is enabled, copy blurred base back into post-process buffer before forward pass.
	if (m_MotionBlurEnabled && m_MotionBlurShader && m_MotionBlurShader->IsValid() &&
		m_MotionBlurBuffer && m_PostProcessBuffer)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_MotionBlurBuffer->FBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glBlitFramebuffer(
			0, 0, m_MotionBlurBuffer->width, m_MotionBlurBuffer->height,
			0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
	}

	// FORWARD PASS - render all custom shaders (opaque + transparent) and transparent standard
	// This handles: opaque custom shaders, transparent custom shaders, and transparent standard
	RenderForwardPass(view, projection);

#if defined(EE_EDITOR)

	if (m_PostProcessBuffer)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDisable(GL_CULL_FACE);

		if (auto physics = ECS::GetInstance().GetSystem<Physics>())
		{
			if (physics->wireframe)
				physics->DrawDebugPhysics();
		}
		// Light probe volume gizmos
		{
			auto& ecs = ECS::GetInstance();
			auto giSystem = ecs.GetSystem<GISystem>();
			if (giSystem) {
				for (EntityID entity : giSystem->GetProbeEntities()) {
					const auto& volume = ecs.GetComponent<LightProbeVolumeComponent>(entity);
					if (!volume.showGizmos) continue;

					glm::mat4 model = GetEntityWorldMatrix(entity);
					glm::vec3 localMin = volume.boundsMin;
					glm::vec3 localMax = volume.boundsMax;

					glm::vec3 center = (localMin + localMax) * 0.5f;
					glm::vec3 extent = (localMax - localMin) * 0.5f;

					glm::vec3 worldCenter = glm::vec3(model * glm::vec4(center, 1.0f));
					glm::mat3 upperLeft = glm::mat3(model);
					glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
						+ glm::abs(upperLeft[1]) * extent.y
						+ glm::abs(upperLeft[2]) * extent.z;

					glm::vec3 actualMin = worldCenter - worldExtent;
					glm::vec3 actualMax = worldCenter + worldExtent;

					glm::vec3 color = volume.isActive ? glm::vec3(0.2f, 0.8f, 1.0f) : glm::vec3(0.5f, 0.5f, 0.5f);
					SubmitDebugAABB(actualMin, actualMax, color);
				}
			}
		}

		if (auto navSys = ECS::GetInstance().GetSystem<NavMeshSystem>())
		{
			//EE_CORE_INFO("[Renderer] Drawing NavMesh debug lines");
			navSys->DebugDraw();
			navSys->DebugHighLight();
		}

		// Flush the navmesh debug lines to screen
		RenderDebugLines(view, projection);
		RenderDebugTriangles(view, projection);
	}

#endif

	// Post-processing pass - read final composited scene output
	RenderPostProcessPass(view, projection);
}

/**
 * @brief Bind g-buffer textures to specified texture units
 */
void Renderer::BindGBufferTextures()
{
	if (!m_GBuffer)
	{
		EE_CORE_WARN("G-Buffer not initialized, cannot bind textures");
		return;
	}

	if (m_LightPassShader)
	{
		GLint loc0 = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_GBuffer0Handle");
		GLint loc1 = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_GBuffer1Handle");
		GLint loc2 = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_GBuffer2Handle");
		GLint loc3 = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_GBuffer3Handle");
		GLint locD = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_GBufferDepthHandle");
		GLint locS = glGetUniformLocation(m_LightPassShader->GetRendererID(), "u_ShadowMapArrayHandle");

		// Bind bindless texture handles
		if (loc0 != -1)
		{
			// Convert 64-bit handle to two 32-bit unsigned integers
			glUniform2ui(loc0, static_cast<GLuint>(m_GBuffer->HandlePackedTexture0),
				static_cast<GLuint>(m_GBuffer->HandlePackedTexture0 >> 32));
		}
		if (loc1 != -1)
		{
			glUniform2ui(loc1, static_cast<GLuint>(m_GBuffer->HandlePackedTexture1),
				static_cast<GLuint>(m_GBuffer->HandlePackedTexture1 >> 32));
		}
		if (loc2 != -1)
		{
			glUniform2ui(loc2, static_cast<GLuint>(m_GBuffer->HandlePackedTexture2),
				static_cast<GLuint>(m_GBuffer->HandlePackedTexture2 >> 32));
		}
		if (loc3 != -1)
		{
			glUniform2ui(loc3, static_cast<GLuint>(m_GBuffer->HandlePackedTexture3),
				static_cast<GLuint>(m_GBuffer->HandlePackedTexture3 >> 32));
		}
		if (locD != -1)
		{
			glUniform2ui(locD, static_cast<GLuint>(m_GBuffer->HandleDepthTexture),
				static_cast<GLuint>(m_GBuffer->HandleDepthTexture >> 32));
		}
		if (locS != -1 && m_ShadowMapArray)
		{
			glUniform2ui(locS, static_cast<GLuint>(m_ShadowMapArrayHandle),
				static_cast<GLuint>(m_ShadowMapArrayHandle >> 32));
		}
	}
}

/**
 * @brief Cleanup g-buffer resources
 */
void Renderer::CleanupGBuffer()
{
	if (m_GBuffer)
	{
		// Make bindless handles non-resident BEFORE deleting textures
		if (m_GBuffer->HandlePackedTexture0 != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandlePackedTexture0);
			m_GBuffer->HandlePackedTexture0 = 0;
		}
		if (m_GBuffer->HandlePackedTexture1 != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandlePackedTexture1);
			m_GBuffer->HandlePackedTexture1 = 0;
		}
		if (m_GBuffer->HandlePackedTexture2 != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandlePackedTexture2);
			m_GBuffer->HandlePackedTexture2 = 0;
		}
		if (m_GBuffer->HandlePackedTexture3 != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandlePackedTexture3);
			m_GBuffer->HandlePackedTexture3 = 0;
		}
		if (m_GBuffer->HandlePackedTexture4 != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandlePackedTexture4);
			m_GBuffer->HandlePackedTexture4 = 0;
		}
		if (m_GBuffer->HandleDepthTexture != 0) {
			glMakeTextureHandleNonResidentARB(m_GBuffer->HandleDepthTexture);
			m_GBuffer->HandleDepthTexture = 0;
		}

		if (m_GBuffer->FBO != 0) {
			glDeleteFramebuffers(1, &m_GBuffer->FBO);
		}
		if (m_GBuffer->PackedTexture0 != 0)
		{
			glDeleteTextures(1, &m_GBuffer->PackedTexture0);
		}
		if (m_GBuffer->PackedTexture1 != 0)
		{
			glDeleteTextures(1, &m_GBuffer->PackedTexture1);
		}
		if (m_GBuffer->PackedTexture2 != 0)
		{
			glDeleteTextures(1, &m_GBuffer->PackedTexture2);
		}
		if (m_GBuffer->PackedTexture3 != 0)
		{
			glDeleteTextures(1, &m_GBuffer->PackedTexture3);
		}
		if (m_GBuffer->PackedTexture4 != 0)
		{
			glDeleteTextures(1, &m_GBuffer->PackedTexture4);
		}
		if (m_GBuffer->DepthTexture != 0)
		{
			glDeleteTextures(1, &m_GBuffer->DepthTexture);
		}
		m_GBuffer.reset();
	}
}

/**
 * @brief Cleanup post-processing buffer resources
 */
void Renderer::CleanupPostProcessBuffer()
{
	// Clean up main post-process buffer
	if (m_PostProcessBuffer)
	{
		if (m_PostProcessBuffer->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_PostProcessBuffer->FBO);
			m_PostProcessBuffer->FBO = 0;
		}
		if (m_PostProcessBuffer->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_PostProcessBuffer->ColorTexture);
			m_PostProcessBuffer->ColorTexture = 0;
		}
		// Note: Depth texture is shared with G-Buffer, don't delete it here
		m_PostProcessBuffer.reset();
	}

	// Clean up bloom extract buffer
	if (m_BloomExtractBuffer)
	{
		if (m_BloomExtractBuffer->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_BloomExtractBuffer->FBO);
			m_BloomExtractBuffer->FBO = 0;
		}
		if (m_BloomExtractBuffer->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_BloomExtractBuffer->ColorTexture);
			m_BloomExtractBuffer->ColorTexture = 0;
		}
		m_BloomExtractBuffer.reset();
	}

	// Clean up bloom blur buffer 1
	if (m_BloomBlurBuffer1)
	{
		if (m_BloomBlurBuffer1->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_BloomBlurBuffer1->FBO);
			m_BloomBlurBuffer1->FBO = 0;
		}
		if (m_BloomBlurBuffer1->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_BloomBlurBuffer1->ColorTexture);
			m_BloomBlurBuffer1->ColorTexture = 0;
		}
		m_BloomBlurBuffer1.reset();
	}

	// Clean up bloom blur buffer 2
	if (m_BloomBlurBuffer2)
	{
		if (m_BloomBlurBuffer2->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_BloomBlurBuffer2->FBO);
			m_BloomBlurBuffer2->FBO = 0;
		}
		if (m_BloomBlurBuffer2->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_BloomBlurBuffer2->ColorTexture);
			m_BloomBlurBuffer2->ColorTexture = 0;
		}
		m_BloomBlurBuffer2.reset();
	}
	// Clean up anti-aliasing buffer
	if (m_AntiAliasingBuffer)
	{
		if (m_AntiAliasingBuffer->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_AntiAliasingBuffer->FBO);
			m_AntiAliasingBuffer->FBO = 0;
		}
		if (m_AntiAliasingBuffer->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_AntiAliasingBuffer->ColorTexture);
			m_AntiAliasingBuffer->ColorTexture = 0;
		}
		m_AntiAliasingBuffer.reset();
	}

	// Clean up motion blur buffer
	if (m_MotionBlurBuffer)
	{
		if (m_MotionBlurBuffer->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_MotionBlurBuffer->FBO);
			m_MotionBlurBuffer->FBO = 0;
		}
		if (m_MotionBlurBuffer->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_MotionBlurBuffer->ColorTexture);
			m_MotionBlurBuffer->ColorTexture = 0;
		}
		m_MotionBlurBuffer.reset();
	}

	// Clean up motion blur mask buffer
	if (m_MotionBlurMaskBuffer)
	{
		if (m_MotionBlurMaskBuffer->FBO != 0)
		{
			glDeleteFramebuffers(1, &m_MotionBlurMaskBuffer->FBO);
			m_MotionBlurMaskBuffer->FBO = 0;
		}
		if (m_MotionBlurMaskBuffer->ColorTexture != 0)
		{
			glDeleteTextures(1, &m_MotionBlurMaskBuffer->ColorTexture);
			m_MotionBlurMaskBuffer->ColorTexture = 0;
		}
		m_MotionBlurMaskBuffer.reset();
	}

	// Clean up noise texture
	if (m_NoiseTexture != 0)
	{
		glDeleteTextures(1, &m_NoiseTexture);
		m_NoiseTexture = 0;
	}

}

/**
 * @brief Updates the lights' shader storage buffer object (SSBO) with current light data.
 * @param view Current output-frame view matrix.
 * @param projection Current output-frame projection matrix.
 */
void Renderer::UpdateLightsSSBO(const Mtx44& view, const Mtx44& projection)
{
	const auto& ecs = Ermine::ECS::GetInstance();
	if (!m_LightSystem) {
		m_VisibleLights.clear();
		m_ShadowCastingLights.clear();
		m_TotalShadowInstances = 0;
		m_LastUploadedLightCount = 0;
		return;
	}

	BuildVisibleLightSet(view, projection);
	UploadLightsSSBOFromPreparedState();
}

void Renderer::BuildVisibleLightSet(const Mtx44& view, const Mtx44& projection)
{
	const auto& ecs = Ermine::ECS::GetInstance();
	if (!m_LightSystem) {
		m_VisibleLights.clear();
		m_ShadowCastingLights.clear();
		m_TotalShadowInstances = 0;
		return;
	}

	// Convert current output-frame view/projection to glm for frustum extraction
	glm::mat4 viewGlm = ToGlm(view);
	glm::mat4 projGlm = ToGlm(projection);

	// Build frustum from view-projection matrix
	Frustum frustum;
	glm::mat4 viewProj = projGlm * viewGlm;
	frustum.ExtractFromViewProjection(viewProj);
	const glm::mat4 sortView = viewGlm;

	struct SortedLightCandidate
	{
		EntityID entity;
		float viewZ;
	};

	std::vector<SortedLightCandidate> visibleLights;
	visibleLights.reserve(m_LightSystem->m_Entities.size());

	// Clear and prepare shadow casting light list and layer allocator
	m_VisibleLights.clear();
	m_VisibleLights.reserve(visibleLights.capacity());
	m_ShadowCastingLights.clear();
	int currentLayer = 0;

	for (EntityID e : m_LightSystem->m_Entities)
	{
		if (ecs.HasComponent<ObjectMetaData>(e))
		{
			const auto& meta = ecs.GetComponent<ObjectMetaData>(e);
			if (!meta.selfActive)
				continue;
		}

		auto& light = ecs.GetComponent<Light>(e);
		const glm::mat4 lightWorld = GetEntityWorldMatrix(e);

		// Derive light transform from world matrix so parenting is respected.
		const glm::vec3 lightPos = ExtractWorldPosition(lightWorld);
		const glm::vec3 dirWorld = ExtractWorldForward(lightWorld);

		// ========== FRUSTUM CULLING TEST ==========
		bool isCulled = false;

		if (light.type == LightType::POINT || light.type == LightType::SPOT)
		{
			// Point light: test sphere against frustum
			// Create AABB from sphere bounds
			float radius = light.radius;
			glm::vec3 aabbMin = lightPos - glm::vec3(radius);
			glm::vec3 aabbMax = lightPos + glm::vec3(radius);

			isCulled = !frustum.TestAABB(aabbMin, aabbMax);
		}
		else if (light.type == LightType::DIRECTIONAL)
		{
			// Directional lights affect entire scene - never cull
			isCulled = false;
		}

		// Skip culled lights
		if (isCulled)
		{
			continue;
		}

		const glm::vec4 viewPosition = sortView * glm::vec4(lightPos, 1.0f);
		visibleLights.push_back({ e, viewPosition.z });
	}

	std::stable_sort(visibleLights.begin(), visibleLights.end(),
		[](const SortedLightCandidate& a, const SortedLightCandidate& b)
		{
			// View-space forward is -Z, so the light with the larger Z value is nearer.
			return a.viewZ > b.viewZ;
		});
	for (const SortedLightCandidate& candidate : visibleLights)
	{
		EntityID e = candidate.entity;
		if (ecs.HasComponent<ObjectMetaData>(e))
		{
			const auto& meta = ecs.GetComponent<ObjectMetaData>(e);
			if (!meta.selfActive)
				continue;
		}

		auto& light = ecs.GetComponent<Light>(e);
		const bool effectiveCastsShadows = light.castsShadows && light.type != LightType::POINT;

		// Allocate shadow layers for this light (if any)
		int shadowLayersNeeded = 0;
		if (effectiveCastsShadows) {
			if (light.type == LightType::DIRECTIONAL) {
				shadowLayersNeeded = NUM_CASCADES;
			} else if (light.type == LightType::SPOT) {
				shadowLayersNeeded = 1;
			}
		}

		if (shadowLayersNeeded > 0 && (currentLayer + shadowLayersNeeded) <= static_cast<int>(SHADOW_MAX_LAYERS)) {
			light.startOffset = currentLayer;
			currentLayer += shadowLayersNeeded;
			m_ShadowCastingLights.push_back(e);
		} else {
			light.startOffset = -1;
		}

		m_VisibleLights.push_back(e);
	}

	m_TotalShadowInstances = currentLayer;
}

void Renderer::UploadLightsSSBOFromPreparedState()
{
	const auto& ecs = Ermine::ECS::GetInstance();
	std::vector<LightGPU> lights;
	lights.reserve(m_VisibleLights.size());

	for (EntityID e : m_VisibleLights)
	{
		auto& light = ecs.GetComponent<Light>(e);
		const glm::mat4 lightWorld = GetEntityWorldMatrix(e);
		const bool effectiveCastsShadows = light.castsShadows && light.type != LightType::POINT;

		const glm::vec3 lightPos = ExtractWorldPosition(lightWorld);
		const glm::vec3 dirWorld = ExtractWorldForward(lightWorld);

		float innerCos = 1.0f, outerCos = 1.0f;
		if (light.type == LightType::SPOT) {
			float innerAngle = glm::radians(light.innerAngle);
			float outerAngle = glm::radians(light.outerAngle);
			innerCos = glm::cos(innerAngle);
			outerCos = glm::cos(outerAngle);
		}

		LightGPU gpu{};
		gpu.position_type = glm::vec4(lightPos.x, lightPos.y, lightPos.z, static_cast<float>(light.type));
		gpu.color_intensity = glm::vec4(light.color.x, light.color.y, light.color.z, light.intensity);
		gpu.direction_range = glm::vec4(dirWorld.x, dirWorld.y, dirWorld.z, light.radius);

		float flags = 0.0f;
		bool hasShadowLayers = effectiveCastsShadows && light.startOffset >= 0;
		if (hasShadowLayers) flags += 1.0f;
		if (light.castsRays) flags += 2.0f;

		gpu.spot_angles_castshadows_startOffset = glm::vec4(innerCos, outerCos, flags, static_cast<float>(light.startOffset));

		for (int i = 0; i < NUM_CASCADES; ++i) {
			gpu.lightSpaceMatrix[i] = light.lightSpaceMatrices[i];
			gpu.splitDepths[i / 4][i % 4] = light.splitDepths[i];
		}
		for (int i = 0; i < 6; ++i) {
			gpu.pointLightMatrices[i] = light.pointLightMatrices[i];
		}
		lights.emplace_back(gpu);
	}

	m_LastUploadedLightCount = lights.size();

	// Upload to SSBO
	EnsureLightsSSBO(lights.size());
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_LightsSSBO);

	glm::vec4 count(static_cast<float>(lights.size()), 0.0f, 0.0f, 0.0f);
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(glm::vec4), &count);

	if (!lights.empty())
	{
		const GLsizeiptr bodyOffset = static_cast<GLsizeiptr>(sizeof(glm::vec4));
		const GLsizeiptr bodySize = static_cast<GLsizeiptr>(lights.size() * sizeof(LightGPU));
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, bodyOffset, bodySize, lights.data());
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glCheckError();
}

/**
 * @brief Initializes light probe capture resources (cubemap FBO and textures).
 */
void Renderer::InitializeProbeCaptureResources()
{
	// Clean up existing resources if any
	if (m_ProbeCubemap != 0) {
		glDeleteTextures(1, &m_ProbeCubemap);
		m_ProbeCubemap = 0;
	}
	if (m_ProbeDepthCubemap != 0) {
		glDeleteTextures(1, &m_ProbeDepthCubemap);
		m_ProbeDepthCubemap = 0;
	}
	if (m_ProbeIndirectCubemapArray != 0) {
		glDeleteTextures(1, &m_ProbeIndirectCubemapArray);
		m_ProbeIndirectCubemapArray = 0;
	}
	if (m_ProbeIndirectDepthArray != 0) {
		glDeleteTextures(1, &m_ProbeIndirectDepthArray);
		m_ProbeIndirectDepthArray = 0;
	}
	if (m_ProbeVoxelAlbedoTexture != 0) {
		glDeleteTextures(1, &m_ProbeVoxelAlbedoTexture);
		m_ProbeVoxelAlbedoTexture = 0;
	}
	if (m_ProbeVoxelEmissiveTexture != 0) {
		glDeleteTextures(1, &m_ProbeVoxelEmissiveTexture);
		m_ProbeVoxelEmissiveTexture = 0;
	}
	if (m_ProbeVoxelNormalTexture != 0) {
		glDeleteTextures(1, &m_ProbeVoxelNormalTexture);
		m_ProbeVoxelNormalTexture = 0;
	}
	m_ProbeVoxelResolution = 0;
	if (m_ProbeCubemapFBO != 0) {
		glDeleteFramebuffers(1, &m_ProbeCubemapFBO);
		m_ProbeCubemapFBO = 0;
	}

	// Create cubemap array for indirect lighting (RGBA16F)
	glGenTextures(1, &m_ProbeIndirectCubemapArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_ProbeIndirectCubemapArray);
	glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA16F,
		m_ProbeCaptureResolution, m_ProbeCaptureResolution, 6 * MAX_PROBES);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// Create cubemap array depth texture
	glGenTextures(1, &m_ProbeIndirectDepthArray);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_ProbeIndirectDepthArray);
	glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_DEPTH_COMPONENT24,
		m_ProbeCaptureResolution, m_ProbeCaptureResolution, 6 * MAX_PROBES);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// Create FBO
	glGenFramebuffers(1, &m_ProbeCubemapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_ProbeCubemapFBO);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_ProbeIndirectCubemapArray, 0, 0);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ProbeIndirectDepthArray, 0, 0);

	// Check framebuffer status
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

	// Create Probes UBO
	if (m_LightProbesUBO == 0) {
		glGenBuffers(1, &m_LightProbesUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, m_LightProbesUBO);
		// Allocate for probe count (vec4) + MAX_PROBES * LightProbeGPU
		const size_t uboSize = sizeof(glm::vec4) + MAX_PROBES * sizeof(LightProbeGPU);
		glBufferData(GL_UNIFORM_BUFFER, uboSize, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, ProbesBindingPoint, m_LightProbesUBO);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	glCheckError();
}

/**
 * @brief Captures environment lighting at a probe's position into spherical harmonics.
 */
void Renderer::CaptureLightProbe(EntityID probeEntity)
{
	auto& ecs = Ermine::ECS::GetInstance();
	if (!ecs.HasComponent<LightProbeVolumeComponent>(probeEntity) || !ecs.HasComponent<Transform>(probeEntity)) {
		return;
	}

	auto& probe = ecs.GetComponent<LightProbeVolumeComponent>(probeEntity);
	const auto& trans = ecs.GetComponent<Transform>(probeEntity);
	glm::vec3 probePos(trans.position.x, trans.position.y, trans.position.z);

	// Ensure the probe has a unique index in the cubemap array
	std::vector<bool> usedIndices(MAX_PROBES, false);
	auto giSystem = ecs.GetSystem<GISystem>();
	if (giSystem) {
		for (EntityID other : giSystem->GetProbeEntities()) {
			if (other == probeEntity) continue;
			auto& otherProbe = ecs.GetComponent<LightProbeVolumeComponent>(other);
			if (!otherProbe.isActive) continue;
			if (otherProbe.probeIndex >= 0 && otherProbe.probeIndex < MAX_PROBES) {
				usedIndices[otherProbe.probeIndex] = true;
			}
		}
	}
	else {
		for (EntityID other = 0; other < MAX_ENTITIES; ++other) {
			if (!ecs.HasComponent<LightProbeVolumeComponent>(other)) continue;
			if (other == probeEntity) continue;
			auto& otherProbe = ecs.GetComponent<LightProbeVolumeComponent>(other);
			if (!otherProbe.isActive) continue;
			if (otherProbe.probeIndex >= 0 && otherProbe.probeIndex < MAX_PROBES) {
				usedIndices[otherProbe.probeIndex] = true;
			}
		}
	}
	const bool needsIndex = (probe.probeIndex < 0 || probe.probeIndex >= MAX_PROBES || usedIndices[probe.probeIndex]);
	if (needsIndex) {
		int freeIndex = -1;
		for (int i = 0; i < MAX_PROBES; ++i) {
			if (!usedIndices[i]) { freeIndex = i; break; }
		}
		if (freeIndex < 0) {
			return;
		}
		probe.probeIndex = freeIndex;
	}

	// Update capture resolution if changed
	if (probe.captureResolution != m_ProbeCaptureResolution) {
		if (probe.captureResolution < 1) {
			probe.captureResolution = 1;
		}
		m_ProbeCaptureResolution = probe.captureResolution;
		InitializeProbeCaptureResources(); // Recreate with new resolution
	}

	// Allocate voxel texture if needed
	if (probe.voxelResolution < 1) {
		probe.voxelResolution = 1;
	}
	if (probe.voxelResolution != m_ProbeVoxelResolution || m_ProbeVoxelAlbedoTexture == 0 || m_ProbeVoxelEmissiveTexture == 0 || m_ProbeVoxelNormalTexture == 0) {
		if (m_ProbeVoxelAlbedoTexture != 0) {
			glDeleteTextures(1, &m_ProbeVoxelAlbedoTexture);
			m_ProbeVoxelAlbedoTexture = 0;
		}
		if (m_ProbeVoxelEmissiveTexture != 0) {
			glDeleteTextures(1, &m_ProbeVoxelEmissiveTexture);
			m_ProbeVoxelEmissiveTexture = 0;
		}
		if (m_ProbeVoxelNormalTexture != 0) {
			glDeleteTextures(1, &m_ProbeVoxelNormalTexture);
			m_ProbeVoxelNormalTexture = 0;
		}
		m_ProbeVoxelResolution = probe.voxelResolution;
		glGenTextures(1, &m_ProbeVoxelAlbedoTexture);
		glBindTexture(GL_TEXTURE_3D, m_ProbeVoxelAlbedoTexture);
		glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8,
			m_ProbeVoxelResolution, m_ProbeVoxelResolution, m_ProbeVoxelResolution);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &m_ProbeVoxelEmissiveTexture);
		glBindTexture(GL_TEXTURE_3D, m_ProbeVoxelEmissiveTexture);
		glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8,
			m_ProbeVoxelResolution, m_ProbeVoxelResolution, m_ProbeVoxelResolution);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glGenTextures(1, &m_ProbeVoxelNormalTexture);
		glBindTexture(GL_TEXTURE_3D, m_ProbeVoxelNormalTexture);
		glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8,
			m_ProbeVoxelResolution, m_ProbeVoxelResolution, m_ProbeVoxelResolution);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_3D, 0);
	}

	// Clear voxel texture
	if (m_ProbeVoxelAlbedoTexture != 0) {
		const GLuint clearValue[4] = { 0, 0, 0, 0 };
		glClearTexImage(m_ProbeVoxelAlbedoTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearValue);
	}
	if (m_ProbeVoxelEmissiveTexture != 0) {
		const GLuint clearValue[4] = { 0, 0, 0, 0 };
		glClearTexImage(m_ProbeVoxelEmissiveTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearValue);
	}
	if (m_ProbeVoxelNormalTexture != 0) {
		const GLuint clearValue[4] = { 0, 0, 0, 0 };
		glClearTexImage(m_ProbeVoxelNormalTexture, 0, GL_RGBA, GL_UNSIGNED_BYTE, clearValue);
	}

	// Setup projection matrix (90 degree FOV for cubemap faces)
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

	// Cubemap view matrices (looking at +X, -X, +Y, -Y, +Z, -Z)
	glm::mat4 captureViews[6] = {
		glm::lookAt(probePos, probePos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +X
		glm::lookAt(probePos, probePos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), // -X
		glm::lookAt(probePos, probePos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),   // +Y
		glm::lookAt(probePos, probePos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),  // -Y
		glm::lookAt(probePos, probePos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +Z
		glm::lookAt(probePos, probePos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))  // -Z
	};

	GLint prevFBO = 0;
	GLint prevViewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	// Dispatch GI bake compute shader (indirect-only)
	// Compute world-space voxel bounds (probe bounds are local to entity)
	glm::mat4 volumeModel = GetEntityWorldMatrix(probeEntity);
	glm::vec3 localMin = probe.boundsMin;
	glm::vec3 localMax = probe.boundsMax;
	glm::vec3 localCenter = (localMin + localMax) * 0.5f;
	glm::vec3 localExtent = (localMax - localMin) * 0.5f;
	glm::vec3 worldCenter = glm::vec3(volumeModel * glm::vec4(localCenter, 1.0f));
	glm::mat3 upperLeft = glm::mat3(volumeModel);
	glm::vec3 worldExtent = glm::abs(upperLeft[0]) * localExtent.x
		+ glm::abs(upperLeft[1]) * localExtent.y
		+ glm::abs(upperLeft[2]) * localExtent.z;
	glm::vec3 worldBoundsMin = worldCenter - worldExtent;
	glm::vec3 worldBoundsMax = worldCenter + worldExtent;

	// Voxelize scene into probe-local volume (RGBA8: rgb=albedo+emissive, a=occupancy)
	if (m_ProbeVoxelizeComputeShader && m_ProbeVoxelizeComputeShader->IsValid()) {
		const GLuint program = m_ProbeVoxelizeComputeShader->GetRendererID();
		glUseProgram(program);

		GLint locVoxelMin = glGetUniformLocation(program, "u_VoxelBoundsMin");
		GLint locVoxelMax = glGetUniformLocation(program, "u_VoxelBoundsMax");
		GLint locVoxelRes = glGetUniformLocation(program, "u_VoxelResolution");
		GLint locIndexCount = glGetUniformLocation(program, "u_IndexCount");
		GLint locFirstIndex = glGetUniformLocation(program, "u_FirstIndex");
		GLint locBaseVertex = glGetUniformLocation(program, "u_BaseVertex");
		GLint locVertexStride = glGetUniformLocation(program, "u_VertexStride");
		GLint locVertexPosOffset = glGetUniformLocation(program, "u_VertexPositionOffset");
		GLint locVertexTexOffset = glGetUniformLocation(program, "u_VertexTexCoordOffset");
		GLint locModelMatrix = glGetUniformLocation(program, "u_ModelMatrix");
		GLint locMatAlbedo = glGetUniformLocation(program, "u_MaterialAlbedo");
		GLint locMatEmissive = glGetUniformLocation(program, "u_MaterialEmissive");
		GLint locMatEmissiveIntensity = glGetUniformLocation(program, "u_MaterialEmissiveIntensity");
		GLint locMatUVScale = glGetUniformLocation(program, "u_MaterialUVScale");
		GLint locMatUVOffset = glGetUniformLocation(program, "u_MaterialUVOffset");
		GLint locMatTexFlags = glGetUniformLocation(program, "u_MaterialTextureFlags");
		GLint locMatAlbedoMapIndex = glGetUniformLocation(program, "u_MaterialAlbedoMapIndex");

		if (locVoxelMin != -1) glUniform3f(locVoxelMin, worldBoundsMin.x, worldBoundsMin.y, worldBoundsMin.z);
		if (locVoxelMax != -1) glUniform3f(locVoxelMax, worldBoundsMax.x, worldBoundsMax.y, worldBoundsMax.z);
		if (locVoxelRes != -1) glUniform1i(locVoxelRes, m_ProbeVoxelResolution);
		if (locVertexStride != -1) {
			const int strideFloats = static_cast<int>(sizeof(graphics::Vertex) / sizeof(float));
			glUniform1i(locVertexStride, strideFloats);
		}
		if (locVertexPosOffset != -1) {
			const int positionOffset = static_cast<int>(offsetof(graphics::Vertex, position) / sizeof(float));
			glUniform1i(locVertexPosOffset, positionOffset);
		}
		if (locVertexTexOffset != -1) {
			const int texCoordOffset = static_cast<int>(offsetof(graphics::Vertex, texCoord) / sizeof(float));
			glUniform1i(locVertexTexOffset, texCoordOffset);
		}

		glBindImageTexture(0, m_ProbeVoxelAlbedoTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
		glBindImageTexture(1, m_ProbeVoxelEmissiveTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);
		glBindImageTexture(2, m_ProbeVoxelNormalTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);

		// Bind buffers for voxelization
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, INDEX_SSBO_BINDING, m_MeshManager.m_IndexSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, VERTEX_SSBO_BINDING, m_MeshManager.GetVertexVBO());

		// Read draw commands from GPU (geometry standard pass)
		size_t drawCount = m_MeshManager.m_GeometryStandardDrawCommandBuffer.GetCommandCount();
		GLuint cmdBuffer = m_MeshManager.m_GeometryStandardDrawCommandBuffer.GetBufferID();
		if (cmdBuffer == 0 || drawCount == 0) {
			glUseProgram(0);
		} else {
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, cmdBuffer);
			GLint bufferSize = 0;
			glGetBufferParameteriv(GL_DRAW_INDIRECT_BUFFER, GL_BUFFER_SIZE, &bufferSize);
			size_t maxCmds = bufferSize > 0 ? static_cast<size_t>(bufferSize) / sizeof(DrawElementsIndirectCommand) : 0;
			if (maxCmds == 0) {
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
				glUseProgram(0);
			} else {
				if (drawCount > maxCmds) {
					drawCount = maxCmds;
				}
				const size_t kMaxSafeDraws = 100000;
				if (drawCount > kMaxSafeDraws) {
					drawCount = kMaxSafeDraws;
				}
				std::vector<DrawElementsIndirectCommand> commands(drawCount);
				glGetBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
					static_cast<GLsizeiptr>(drawCount * sizeof(DrawElementsIndirectCommand)),
					commands.data());
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

				// Read draw infos to get model matrix + material index
				std::vector<DrawInfo> drawInfos(drawCount);
				GLuint infoBuffer = m_MeshManager.m_GeometryStandardDrawInfoBuffer.GetBufferID();
				if (infoBuffer != 0) {
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, infoBuffer);
					glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
						static_cast<GLsizeiptr>(drawCount * sizeof(DrawInfo)),
						drawInfos.data());
					glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
				}

				const GLuint groupSize = 64;
				size_t totalTris = 0;
				for (size_t i = 0; i < drawCount; ++i) {
					const auto& cmd = commands[i];
					if (cmd.count < 3) continue;

					if (locIndexCount != -1) glUniform1i(locIndexCount, static_cast<int>(cmd.count));
					if (locFirstIndex != -1) glUniform1i(locFirstIndex, static_cast<int>(cmd.firstIndex));
					if (locBaseVertex != -1) glUniform1i(locBaseVertex, static_cast<int>(cmd.baseVertex));
					if (locModelMatrix != -1 && i < drawInfos.size()) {
						glUniformMatrix4fv(locModelMatrix, 1, GL_FALSE, &drawInfos[i].modelMatrix[0][0]);
					}
					if (locMatAlbedo != -1 || locMatEmissive != -1 || locMatEmissiveIntensity != -1 ||
						locMatUVScale != -1 || locMatUVOffset != -1 || locMatTexFlags != -1 || locMatAlbedoMapIndex != -1) {
						glm::vec3 albedo(0.8f);
						glm::vec3 emissive(0.0f);
						float emissiveIntensity = 0.0f;
						glm::vec2 uvScale(1.0f);
						glm::vec2 uvOffset(0.0f);
						uint32_t textureFlags = 0;
						int albedoMapIndex = -1;
						if (i < drawInfos.size()) {
							const uint32_t matIndex = drawInfos[i].materialIndex;
							if (matIndex < m_CompiledMaterials.size()) {
								const auto& mat = m_CompiledMaterials[matIndex];
								albedo = glm::vec3(mat.albedo.x, mat.albedo.y, mat.albedo.z);
								emissive = glm::vec3(mat.emissive.x, mat.emissive.y, mat.emissive.z);
								emissiveIntensity = mat.emissiveIntensity;
								uvScale = glm::vec2(mat.uvScale.x, mat.uvScale.y);
								uvOffset = glm::vec2(mat.uvOffset.x, mat.uvOffset.y);
								textureFlags = mat.textureFlags;
								albedoMapIndex = mat.albedoMapIndex;
							}
						}
						if (locMatAlbedo != -1) glUniform3f(locMatAlbedo, albedo.x, albedo.y, albedo.z);
						if (locMatEmissive != -1) glUniform3f(locMatEmissive, emissive.x, emissive.y, emissive.z);
						if (locMatEmissiveIntensity != -1) glUniform1f(locMatEmissiveIntensity, emissiveIntensity);
						if (locMatUVScale != -1) glUniform2f(locMatUVScale, uvScale.x, uvScale.y);
						if (locMatUVOffset != -1) glUniform2f(locMatUVOffset, uvOffset.x, uvOffset.y);
						if (locMatTexFlags != -1) glUniform1ui(locMatTexFlags, textureFlags);
						if (locMatAlbedoMapIndex != -1) glUniform1i(locMatAlbedoMapIndex, albedoMapIndex);
					}

					const GLuint triCount = cmd.count / 3;
					totalTris += triCount;
					const GLuint groupsX = (triCount + groupSize - 1) / groupSize;
					glDispatchCompute(groupsX, 1, 1);
				}
			}
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(0);
	}

	// Inject direct lighting into voxel emissive using the light SSBO
	if (m_ProbeLightInjectComputeShader && m_ProbeLightInjectComputeShader->IsValid()) {
		const GLuint program = m_ProbeLightInjectComputeShader->GetRendererID();
		glUseProgram(program);

		GLint locVoxelMin = glGetUniformLocation(program, "u_VoxelBoundsMin");
		GLint locVoxelMax = glGetUniformLocation(program, "u_VoxelBoundsMax");
		GLint locVoxelRes = glGetUniformLocation(program, "u_VoxelResolution");
		if (locVoxelMin != -1) glUniform3f(locVoxelMin, worldBoundsMin.x, worldBoundsMin.y, worldBoundsMin.z);
		if (locVoxelMax != -1) glUniform3f(locVoxelMax, worldBoundsMax.x, worldBoundsMax.y, worldBoundsMax.z);
		if (locVoxelRes != -1) glUniform1i(locVoxelRes, m_ProbeVoxelResolution);

		BindLightsSSBO();
		glBindImageTexture(0, m_ProbeVoxelAlbedoTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
		glBindImageTexture(1, m_ProbeVoxelEmissiveTexture, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
		glBindImageTexture(2, m_ProbeVoxelNormalTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);

		const GLuint groupSize = 4;
		const GLuint groups = (m_ProbeVoxelResolution + groupSize - 1) / groupSize;
		glDispatchCompute(groups, groups, groups);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(0);
	}

	if (m_ProbeBakeComputeShader && m_ProbeBakeComputeShader->IsValid()) {
		const GLuint program = m_ProbeBakeComputeShader->GetRendererID();
		glUseProgram(program);

		GLint locBaseLayer = glGetUniformLocation(program, "u_ProbeBaseLayer");
		GLint locProbePos = glGetUniformLocation(program, "u_ProbePosition");
		GLint locResolution = glGetUniformLocation(program, "u_Resolution");
		GLint locBounces = glGetUniformLocation(program, "u_Bounces");
		GLint locEnergyLoss = glGetUniformLocation(program, "u_EnergyLoss");
		GLint locVoxelMin = glGetUniformLocation(program, "u_VoxelBoundsMin");
		GLint locVoxelMax = glGetUniformLocation(program, "u_VoxelBoundsMax");
		GLint locVoxelRes = glGetUniformLocation(program, "u_VoxelResolution");
		if (locBaseLayer != -1) glUniform1i(locBaseLayer, probe.probeIndex * 6);
		if (locProbePos != -1) glUniform3f(locProbePos, probePos.x, probePos.y, probePos.z);
		if (locResolution != -1) glUniform1i(locResolution, m_ProbeCaptureResolution);
		if (locBounces != -1) glUniform1i(locBounces, m_GIBakeBounces);
		if (locEnergyLoss != -1) glUniform1f(locEnergyLoss, m_GIBakeEnergyLoss);
		if (locVoxelMin != -1) glUniform3f(locVoxelMin, worldBoundsMin.x, worldBoundsMin.y, worldBoundsMin.z);
		if (locVoxelMax != -1) glUniform3f(locVoxelMax, worldBoundsMax.x, worldBoundsMax.y, worldBoundsMax.z);
		if (locVoxelRes != -1) glUniform1i(locVoxelRes, m_ProbeVoxelResolution);

		glBindImageTexture(0, m_ProbeIndirectCubemapArray, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		if (m_ProbeVoxelAlbedoTexture != 0) {
			glBindImageTexture(1, m_ProbeVoxelAlbedoTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
		}
		if (m_ProbeVoxelEmissiveTexture != 0) {
			glBindImageTexture(2, m_ProbeVoxelEmissiveTexture, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
		}

		const GLuint groupSize = 8;
		const GLuint groupsX = (m_ProbeCaptureResolution + groupSize - 1) / groupSize;
		const GLuint groupsY = (m_ProbeCaptureResolution + groupSize - 1) / groupSize;
		glDispatchCompute(groupsX, groupsY, 6);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glUseProgram(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

	// Project captured cubemap to SH coefficients
	ProjectCubemapArrayToSH(probe.probeIndex, probe.shCoefficients);

	// Save baked probe data to disk
	{
		std::string sceneName = "UnsavedScene";
		if (auto scenePathOpt = SceneManager::GetInstance().GetCurrentScenePath()) {
			std::filesystem::path scenePath(*scenePathOpt);
			sceneName = scenePath.stem().string();
		}
		std::filesystem::path outDir = std::filesystem::path("../Resources/Textures/GI") / sceneName;
		std::filesystem::create_directories(outDir);
		std::filesystem::path outPath = outDir / ("Probe_" + std::to_string(static_cast<uint64_t>(probeEntity)) + ".probe");

		if (SaveProbeSHToFile(outPath, probe.shCoefficients)) {
			probe.bakedProbePath = outPath.generic_string();
			probe.bakedDataLoaded = true;
		}
	}

	glCheckError();

}

/**
 * @brief Projects a cubemap to spherical harmonics (L2 - 9 coefficients).
 */
void Renderer::ProjectCubemapToSH(GLuint cubemapID, glm::vec3 outCoefficients[9])
{
	// Initialize coefficients to zero
	for (int i = 0; i < 9; ++i) {
		outCoefficients[i] = glm::vec3(0.0f);
	}

	// Read cubemap data from GPU
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);

	const int resolution = m_ProbeCaptureResolution;
	std::vector<glm::vec4> faceData(resolution * resolution);

	// SH basis function constants
	const float c0 = 0.282095f;  // 1 / (2 * sqrt(pi))
	const float c1 = 0.488603f;  // sqrt(3 / (4 * pi))
	const float c2 = 1.092548f;  // sqrt(15 / (4 * pi))
	const float c3 = 0.315392f;  // sqrt(5 / (16 * pi))
	const float c4 = 0.546274f;  // sqrt(15 / (16 * pi))

	float totalWeight = 0.0f;

	// Process each cubemap face
	for (int face = 0; face < 6; ++face) {
		glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB, GL_FLOAT, faceData.data());

		// Sample each pixel
		for (int y = 0; y < resolution; ++y) {
			for (int x = 0; x < resolution; ++x) {
				// Convert pixel coordinates to normalized [-1, 1] range
				float u = (x + 0.5f) / resolution * 2.0f - 1.0f;
				float v = (y + 0.5f) / resolution * 2.0f - 1.0f;

				// Calculate direction vector for this cubemap pixel
				glm::vec3 dir;
				switch (face) {
				case 0: dir = glm::normalize(glm::vec3(1.0f, -v, -u)); break;  // +X
				case 1: dir = glm::normalize(glm::vec3(-1.0f, -v, u)); break;  // -X
				case 2: dir = glm::normalize(glm::vec3(u, 1.0f, v)); break;    // +Y
				case 3: dir = glm::normalize(glm::vec3(u, -1.0f, -v)); break;  // -Y
				case 4: dir = glm::normalize(glm::vec3(u, -v, 1.0f)); break;   // +Z
				case 5: dir = glm::normalize(glm::vec3(-u, -v, -1.0f)); break; // -Z
				}

				// Get pixel color
				glm::vec3 color = faceData[y * resolution + x];

				// Solid angle weight (approximate)
				float temp = 1.0f + u * u + v * v;
				float weight = 4.0f / (sqrt(temp) * temp);
				totalWeight += weight;

				// Evaluate L2 SH basis functions
				float sh[9];
				sh[0] = c0;                            // Y(0,0)
				sh[1] = c1 * dir.y;                    // Y(1,-1)
				sh[2] = c1 * dir.z;                    // Y(1,0)
				sh[3] = c1 * dir.x;                    // Y(1,1)
				sh[4] = c2 * dir.x * dir.y;            // Y(2,-2)
				sh[5] = c2 * dir.y * dir.z;            // Y(2,-1)
				sh[6] = c3 * (3.0f * dir.z * dir.z - 1.0f); // Y(2,0)
				sh[7] = c2 * dir.x * dir.z;            // Y(2,1)
				sh[8] = c4 * (dir.x * dir.x - dir.y * dir.y); // Y(2,2)

				// Accumulate weighted SH coefficients
				for (int i = 0; i < 9; ++i) {
					outCoefficients[i] += color * sh[i] * weight;
				}
			}
		}
	}

	// Normalize by total weight
	for (int i = 0; i < 9; ++i) {
		outCoefficients[i] *= (4.0f * glm::pi<float>()) / totalWeight;
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glCheckError();
}

/**
 * @brief Projects a cubemap array layer to spherical harmonics (L2 - 9 coefficients).
 */
void Renderer::ProjectCubemapArrayToSH(int probeIndex, glm::vec3 outCoefficients[9])
{
	if (probeIndex < 0 || probeIndex >= MAX_PROBES) {
		return;
	}

	// Initialize coefficients to zero
	for (int i = 0; i < 9; ++i) {
		outCoefficients[i] = glm::vec3(0.0f);
	}

	const int resolution = m_ProbeCaptureResolution;
	std::vector<glm::vec4> faceData(resolution * resolution);

	// SH basis function constants
	const float c0 = 0.282095f;  // 1 / (2 * sqrt(pi))
	const float c1 = 0.488603f;  // sqrt(3 / (4 * pi))
	const float c2 = 1.092548f;  // sqrt(15 / (4 * pi))
	const float c3 = 0.315392f;  // sqrt(5 / (16 * pi))
	const float c4 = 0.546274f;  // sqrt(15 / (16 * pi))

	float totalWeight = 0.0f;

	GLint prevFBO = 0;
	GLint prevViewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	glBindFramebuffer(GL_FRAMEBUFFER, m_ProbeCubemapFBO);
	glViewport(0, 0, resolution, resolution);

	// Process each cubemap face from array layer
	for (int face = 0; face < 6; ++face) {
		const int layer = probeIndex * 6 + face;
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_ProbeIndirectCubemapArray, 0, layer);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			break;
		}

		glReadPixels(0, 0, resolution, resolution, GL_RGBA, GL_FLOAT, faceData.data());

		// Sample each pixel
		for (int y = 0; y < resolution; ++y) {
			for (int x = 0; x < resolution; ++x) {
				// Convert pixel coordinates to normalized [-1, 1] range
				float u = (x + 0.5f) / resolution * 2.0f - 1.0f;
				float v = (y + 0.5f) / resolution * 2.0f - 1.0f;

				// Calculate direction vector for this cubemap pixel
				glm::vec3 dir;
				switch (face) {
				case 0: dir = glm::normalize(glm::vec3(1.0f, -v, -u)); break;  // +X
				case 1: dir = glm::normalize(glm::vec3(-1.0f, -v, u)); break;  // -X
				case 2: dir = glm::normalize(glm::vec3(u, 1.0f, v)); break;    // +Y
				case 3: dir = glm::normalize(glm::vec3(u, -1.0f, -v)); break;  // -Y
				case 4: dir = glm::normalize(glm::vec3(u, -v, 1.0f)); break;   // +Z
				case 5: dir = glm::normalize(glm::vec3(-u, -v, -1.0f)); break; // -Z
				}

				// Get pixel color
				glm::vec3 color = glm::vec3(faceData[y * resolution + x]);

				// Solid angle weight (approximate)
				float temp = 1.0f + u * u + v * v;
				float weight = 4.0f / (sqrt(temp) * temp);
				totalWeight += weight;

				// Evaluate L2 SH basis functions
				float sh[9];
				sh[0] = c0;                            // Y(0,0)
				sh[1] = c1 * dir.y;                    // Y(1,-1)
				sh[2] = c1 * dir.z;                    // Y(1,0)
				sh[3] = c1 * dir.x;                    // Y(1,1)
				sh[4] = c2 * dir.x * dir.y;            // Y(2,-2)
				sh[5] = c2 * dir.y * dir.z;            // Y(2,-1)
				sh[6] = c3 * (3.0f * dir.z * dir.z - 1.0f); // Y(2,0)
				sh[7] = c2 * dir.x * dir.z;            // Y(2,1)
				sh[8] = c4 * (dir.x * dir.x - dir.y * dir.y); // Y(2,2)

				// Accumulate weighted SH coefficients
				for (int i = 0; i < 9; ++i) {
					outCoefficients[i] += color * sh[i] * weight;
				}
			}
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);

	// Normalize by total weight
	if (totalWeight > 0.0f) {
		for (int i = 0; i < 9; ++i) {
			outCoefficients[i] *= (4.0f * glm::pi<float>()) / totalWeight;
		}
	}

	glCheckError();
}

/**
 * @brief Updates the light probes UBO with current probe data.
 */
void Renderer::UpdateLightProbesUBO()
{
	if (!m_LightProbesEnabled || m_LightProbesUBO == 0) {
		return;
	}

	auto& ecs = Ermine::ECS::GetInstance();
	auto giSystem = ecs.GetSystem<GISystem>();
	
	std::vector<LightProbeGPU> probesGPU;
	probesGPU.reserve(MAX_PROBES);

	// Collect active probes
	if (giSystem) {
		for (EntityID entity : giSystem->GetProbeEntities()) {
			auto& probe = ecs.GetComponent<LightProbeVolumeComponent>(entity);
			const auto& trans = ecs.GetComponent<Transform>(entity);

			if (!probe.isActive) continue;
			if (probesGPU.size() >= MAX_PROBES) break; // Limit to MAX_PROBES

			if (!probe.bakedDataLoaded && !probe.bakedProbePath.empty()) {
				std::filesystem::path bakedPath(probe.bakedProbePath);
				if (LoadProbeSHFromFile(bakedPath, probe.shCoefficients)) {
					probe.bakedDataLoaded = true;
				}
			}

			LightProbeGPU gpuProbe;
			gpuProbe.position_radius = glm::vec4(trans.position.x, trans.position.y, trans.position.z, 0.0f);
			
			// Copy SH coefficients (convert vec3 array to vec4 for std140 alignment)
			for (int i = 0; i < 9; ++i) {
				gpuProbe.shCoefficients[i] = glm::vec4(probe.shCoefficients[i], 0.0f);
			}

			glm::mat4 model = GetEntityWorldMatrix(entity);
			glm::vec3 localMin = probe.boundsMin;
			glm::vec3 localMax = probe.boundsMax;

			glm::vec3 center = (localMin + localMax) * 0.5f;
			glm::vec3 extent = (localMax - localMin) * 0.5f;

			glm::vec3 worldCenter = glm::vec3(model * glm::vec4(center, 1.0f));
			glm::mat3 upperLeft = glm::mat3(model);
			glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
				+ glm::abs(upperLeft[1]) * extent.y
				+ glm::abs(upperLeft[2]) * extent.z;

			glm::vec3 actualMin = worldCenter - worldExtent;
			glm::vec3 actualMax = worldCenter + worldExtent;

			gpuProbe.boundsMin = glm::vec4(actualMin, 0.0f);
			gpuProbe.boundsMax = glm::vec4(actualMax, 0.0f);
			gpuProbe.flags = glm::vec4(1.0f, static_cast<float>(probe.priority), 0.0f, 0.0f);

			probesGPU.push_back(gpuProbe);
		}
	}
	else {
		for (EntityID entity = 0; entity < MAX_ENTITIES; ++entity) {
			if (!ecs.HasComponent<LightProbeVolumeComponent>(entity)) continue;
			if (!ecs.HasComponent<Transform>(entity)) continue;

			auto& probe = ecs.GetComponent<LightProbeVolumeComponent>(entity);
			const auto& trans = ecs.GetComponent<Transform>(entity);

			if (!probe.isActive) continue;
			if (probesGPU.size() >= MAX_PROBES) break; // Limit to MAX_PROBES

			if (!probe.bakedDataLoaded && !probe.bakedProbePath.empty()) {
				std::filesystem::path bakedPath(probe.bakedProbePath);
				if (LoadProbeSHFromFile(bakedPath, probe.shCoefficients)) {
					probe.bakedDataLoaded = true;
				}
			}

			LightProbeGPU gpuProbe;
			gpuProbe.position_radius = glm::vec4(trans.position.x, trans.position.y, trans.position.z, 0.0f);

			// Copy SH coefficients (convert vec3 array to vec4 for std140 alignment)
			for (int i = 0; i < 9; ++i) {
				gpuProbe.shCoefficients[i] = glm::vec4(probe.shCoefficients[i], 0.0f);
			}

			glm::mat4 model = GetEntityWorldMatrix(entity);
			glm::vec3 localMin = probe.boundsMin;
			glm::vec3 localMax = probe.boundsMax;

			glm::vec3 center = (localMin + localMax) * 0.5f;
			glm::vec3 extent = (localMax - localMin) * 0.5f;

			glm::vec3 worldCenter = glm::vec3(model * glm::vec4(center, 1.0f));
			glm::mat3 upperLeft = glm::mat3(model);
			glm::vec3 worldExtent = glm::abs(upperLeft[0]) * extent.x
				+ glm::abs(upperLeft[1]) * extent.y
				+ glm::abs(upperLeft[2]) * extent.z;

			glm::vec3 actualMin = worldCenter - worldExtent;
			glm::vec3 actualMax = worldCenter + worldExtent;

			gpuProbe.boundsMin = glm::vec4(actualMin, 0.0f);
			gpuProbe.boundsMax = glm::vec4(actualMax, 0.0f);
			gpuProbe.flags = glm::vec4(1.0f, static_cast<float>(probe.priority), 0.0f, 0.0f);

			probesGPU.push_back(gpuProbe);
		}
	}

	// Upload to UBO
	glBindBuffer(GL_UNIFORM_BUFFER, m_LightProbesUBO);

	// Upload probe count
	glm::vec4 probeCountVec(static_cast<float>(probesGPU.size()), 0.0f, 0.0f, 0.0f);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), &probeCountVec);

	// Upload probe data
	if (!probesGPU.empty()) {
		const GLsizeiptr probesSize = static_cast<GLsizeiptr>(probesGPU.size() * sizeof(LightProbeGPU));
		glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4), probesSize, probesGPU.data());
	}

	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	glCheckError();
}

/**
 * @brief Updates the material's SSBO with the specified material data.
 *
 * If the material SSBO does not exist, this function creates one. It then uploads the given material data
 * into the UBO, making it available to the shader for rendering.
 *
 * @param materialData The material data to be uploaded to the SSBO, including properties like color, texture, etc.
 */
void Renderer::UpdateMaterialSSBO(const graphics::MaterialSSBO& materialData, uint32_t materialIndex)
{
	if (!m_MaterialSSBO)
	{
		EE_CORE_ERROR("MaterialSSBO not initialized - call CompileMaterials() first");
		return;
	}

	if (materialIndex >= m_CompiledMaterials.size())
	{
		EE_CORE_WARN("UpdateMaterialSSBO: materialIndex {0} out of range (compiled={1}). Marking materials dirty.",
			materialIndex, m_CompiledMaterials.size());
		MarkMaterialsDirty();
		return;
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MATERIAL_SSBO_BINDING, m_MaterialSSBO);

	const size_t materialSize = sizeof(graphics::MaterialSSBO);
	const size_t offset = materialSize * materialIndex;

	// Keep CPU mirror coherent with the GPU update path.
	m_CompiledMaterials[materialIndex] = materialData;

	// Upload to specific index in the array
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, materialSize, &materialData);

	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		EE_CORE_ERROR("Failed to update material at index {0}, error: {1}",
			materialIndex, error);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/**
* @brief Updates the material's SSBO at index 0 with the specified material data.
 *
 * This is a convenience overload that defaults to updating the first material (index 0).
 *
 * @param materialData The material data to be uploaded to the SSBO, including properties like color, texture, etc.
 */
void Renderer::UpdateMaterialSSBO(const graphics::MaterialSSBO& materialData)
{
	// This version is kept for backward compatibility
	// It updates index 0 by default
	UpdateMaterialSSBO(materialData, 0);
}

/**
 * @brief Update material color properties and upload to SSBO
 * @param entity The entity whose material to update
 * @param albedo The new albedo color
 * @param roughness The new roughness value
 * @param metallic The new metallic value
 * @param emissive The new emissive color
 */
void Renderer::UpdateMaterialColor(EntityID entity,
	const Vec3& albedo,
	float roughness,
	float metallic,
	const Vec3& emissive)
{
	auto& ecs = ECS::GetInstance();
	auto renderer = ecs.GetSystem<Renderer>();
	auto& materialComp = ecs.GetComponent<Ermine::Material>(entity);
	auto* material = materialComp.GetMaterial();

	if (!material) return;

	// Update all properties
	material->SetVec3("materialAlbedo", albedo);
	material->SetFloat("materialRoughness", roughness);
	material->SetFloat("materialMetallic", metallic);
	material->SetVec3("materialEmissive", emissive);

	// Single upload for all changes
	auto ssboData = material->GetSSBOData();
	uint32_t materialIndex = renderer->GetMaterialIndex(entity);
	renderer->UpdateMaterialSSBO(ssboData, materialIndex);
}

/**
 * @brief Retrieves the material index associated with the given entity.
 * @param entity The entity whose material index is to be retrieved.
 * @return The material index for the specified entity, or 0 if not found.
 */
uint32_t Renderer::GetMaterialIndex(EntityID entity) const
{
	auto it = m_EntityMaterialIndices.find(entity);
	if (it != m_EntityMaterialIndices.end()) {
		return it->second;
	}
	return 0; // Default to first material
}

/**
 * @brief Sets the material index uniform before drawing
 * Call this before each draw call to tell shader which material to use
 */
void Renderer::SetMaterialIndex(EntityID entity, const std::shared_ptr<Shader>& shader)
{
	if (!shader || !shader->IsValid()) return;

	auto it = m_EntityMaterialIndices.find(entity);
	if (it != m_EntityMaterialIndices.end())
	{
		shader->SetUniform1i("u_MaterialIndex", static_cast<int>(it->second));
	}
	else
	{
		EE_CORE_WARN("Entity {0} has no material index, using default 0", entity);
		shader->SetUniform1i("u_MaterialIndex", 0);
	}
}

/**
 * @brief Binds the MaterialBlock shader storage block to the specified shader program if it has not been bound before.
 * @param shader The shader program to which the material SSBO block should be bound.
 */
void Renderer::BindMaterialBlockIfPresent(const std::shared_ptr<Shader>& shader)
{
	if (!shader || !shader->IsValid())
		return;

	const GLuint program = shader->GetRendererID();
	if (m_MaterialBlockBoundPrograms.find(program) != m_MaterialBlockBoundPrograms.end())
		return;

	// Resolve SSBO block index and bind it to the global material binding slot.
	GLuint blockIndex = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "MaterialBlock");
	if (blockIndex != GL_INVALID_INDEX)
	{
		glShaderStorageBlockBinding(program, blockIndex, MATERIAL_SSBO_BINDING);
		m_MaterialBlockBoundPrograms.insert(program);
	}
}

/**
 * @brief Update all mesh entities and draw them
 */
void Renderer::Update(const Mtx44& view, const Mtx44& projection)
{
	// Accumulate elapsed time for shader effects
	m_ElapsedTime += FrameController::GetDeltaTime();

	SyncShadowViewsForOutputFrame(view, projection);
	UpdateLightProbesUBO();


	// Check if new meshes have been registered and need uploading
	if (m_MeshManager.IsDirty() && m_MeshManager.HasStagedData())
	{
		m_MeshManager.UploadAndBuild();
		EE_CORE_INFO("MeshManager: Uploaded {} new meshes during runtime", m_MeshManager.GetMeshCount());
	}

	// Compile materials on first update when entities exist
	if (m_MaterialsDirty && !m_Entities.empty())
	{
		CompileMaterials();
	}

	// Rebind global SSBOs every frame so custom shaders always see valid data.
	if (m_MaterialSSBO != 0) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MATERIAL_SSBO_BINDING, m_MaterialSSBO);
	}
	if (m_TextureArraySSBO != 0) {
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TEXTURE_SSBO_BINDING, m_TextureArraySSBO);
	}

	if (m_UseDeferredRendering)
	{
		// Use deferred rendering pipeline (now includes transparency)
		RenderDeferredPipeline(view, projection);
	}
	else
	{
		// Forward rendering with transparency support
#if defined(EE_EDITOR)
		glBindFramebuffer(GL_FRAMEBUFFER, m_OffscreenBuffer->FBO);
		glViewport(0, 0, m_OffscreenBuffer->width, m_OffscreenBuffer->height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

		// Render skybox FIRST as the background
		if (m_ShowSkybox && m_skybox && m_skybox->IsValid()) {
			glDepthMask(GL_FALSE);
			m_skybox->Render(view, projection);
			glDepthMask(GL_TRUE);
		}

		// Clear transparent objects from previous frame
		m_transparentObjects.clear();

		// Calculate camera position for transparent sorting
		glm::mat4 glmView = glm::mat4(
			view.m00, view.m01, view.m02, view.m03,
			view.m10, view.m11, view.m12, view.m13,
			view.m20, view.m21, view.m22, view.m23,
			view.m30, view.m31, view.m32, view.m33
		);
		glm::mat4 invView = glm::inverse(glmView);
		Vec3 cameraPos = Vec3(invView[3][0], invView[3][1], invView[3][2]);

		auto& ecs = ECS::GetInstance();

		// First pass: Render opaque objects and collect transparent objects
		for (auto& entity : m_Entities)
		{
			if (ecs.HasComponent<ObjectMetaData>(entity))
			{
				const auto& meta = ecs.GetComponent<ObjectMetaData>(entity);
				if (!meta.selfActive)
					continue;
			}

			// Model pipeline
			if (ecs.HasComponent<ModelComponent>(entity) && ecs.HasComponent<Ermine::Material>(entity))
			{
				auto& trans = ecs.GetComponent<Transform>(entity);
				auto& modelComp = ecs.GetComponent<ModelComponent>(entity);
				//auto& materialComp = ecs.GetComponent<Ermine::Material>(entity);

				if (!modelComp.m_model) continue;

				// Build entity transform
				glm::mat4 entityModel = glm::mat4(1.0f);
				entityModel = glm::translate(entityModel, glm::vec3(trans.position.x, trans.position.y, trans.position.z));
				glm::quat rotQuat = glm::quat(trans.rotation.w, trans.rotation.x, trans.rotation.y, trans.rotation.z);
				rotQuat = glm::normalize(rotQuat);
				entityModel *= glm::mat4_cast(rotQuat);
				entityModel = glm::scale(entityModel, glm::vec3(trans.scale.x, trans.scale.y, trans.scale.z));

				// Check if entity has material for transparency check
				Ermine::graphics::Material* material = nullptr;
				if (ecs.HasComponent<Ermine::Material>(entity)) {
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
					material = materialComponent.GetMaterial();
				}

				// Check if transparent
				if (material && IsTransparentMaterial(material)) {
					TransparentObject transparentObj;
					transparentObj.entity = entity;
					transparentObj.modelMatrix = entityModel;
					transparentObj.distanceToCamera = 0.0f; // Will be calculated in SortTransparentObjects
					m_transparentObjects.push_back(transparentObj);
					continue; // Skip opaque rendering
				}

				// TODO: Implement indirect model rendering
				//RenderModelDeferred(*modelComp.m_model, materialComp.GetMaterial(), view, projection, entityModel);
			}
			// Mesh + material pipeline
			else if (ecs.HasComponent<Mesh>(entity) && ecs.HasComponent<Ermine::Material>(entity))
			{
				auto& trans = ecs.GetComponent<Transform>(entity);
				auto& mesh = ecs.GetComponent<Mesh>(entity);
				auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);

				if (!mesh.vertex_array || !mesh.index_buffer) continue;

				Ermine::graphics::Material* material = materialComponent.GetMaterial();
				if (!material) {
					EE_CORE_WARN("Entity {0} has null material", entity);
					continue;
				}

				// Build model matrix
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, glm::vec3(trans.position.x, trans.position.y, trans.position.z));
				glm::quat rotQuat = glm::quat(trans.rotation.w, trans.rotation.x, trans.rotation.y, trans.rotation.z);
				rotQuat = glm::normalize(rotQuat);
				model *= glm::mat4_cast(rotQuat);
				model = glm::scale(model, glm::vec3(trans.scale.x, trans.scale.y, trans.scale.z));

				// Check if material is transparent
				if (IsTransparentMaterial(material)) {
					TransparentObject transparentObj;
					transparentObj.entity = entity;
					transparentObj.modelMatrix = model;
					transparentObj.distanceToCamera = 0.0f; // Will be calculated in SortTransparentObjects
					m_transparentObjects.push_back(transparentObj);
					continue; // Skip opaque rendering in geometry pass
				}

				// Render opaque object
				auto shader = material->GetShader();
				if (!shader || !shader->IsValid()) {
					EE_CORE_WARN("Entity {0} has invalid shader", entity);
					continue;
				}


				// Bind material (this handles shader binding and texture binding)
				material->Bind();

				// Bind uniform blocks
				BindMaterialBlockIfPresent(shader);

				// Disable skinning for primitive meshes
				shader->SetUniform1i("u_UseSkinning", 0);

				// Set transformation matrices
				shader->SetUniformMatrix4fv("model", model);
				shader->SetUniformMatrix4fv("view", &view.m2[0][0]);
				shader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);

				glm::mat4 modelView = glmView * model;
				glm::mat3 normalMatrix = transpose(inverse(glm::mat3(modelView)));
				shader->SetUniformMatrix3fv("NormalMatrix", normalMatrix);

				// Set shading mode
				shader->SetUniform1i("isBlinnPhong", m_IsBlinnPhong ? 1 : 0);

				// Draw the mesh
				Draw(mesh.vertex_array, mesh.index_buffer);

				// Unbind material
				material->Unbind();
			}
		}

		// Sort transparent objects EVERY frame (camera position changes)
		// - Full rebuild: Sort by shader FIRST (creates stable batches), then distance
		// - Fast update: Sort by distance ONLY within existing shader groups
		SortTransparentObjects(cameraPos, m_NeedsTransparentSort);
		auto uploadSortedCustomTransparentBuffers = [&]() {
			if (!m_ForwardTransparentCustomStandardItems.empty() &&
				m_ForwardTransparentCustomStandardCmdBuffer != 0 &&
				m_ForwardTransparentCustomStandardInfoBuffer != 0) {
				std::vector<DrawElementsIndirectCommand> commands;
				std::vector<DrawInfo> infos;
				commands.reserve(m_ForwardTransparentCustomStandardItems.size());
				infos.reserve(m_ForwardTransparentCustomStandardItems.size());
				for (const auto& item : m_ForwardTransparentCustomStandardItems) {
					commands.push_back(item.command);
					infos.push_back(item.info);
				}

				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomStandardCmdBuffer);
				glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
					static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
					commands.data());

				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomStandardInfoBuffer);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					static_cast<GLsizeiptr>(infos.size() * sizeof(DrawInfo)),
					infos.data());
			}

			if (!m_ForwardTransparentCustomSkinnedItems.empty() &&
				m_ForwardTransparentCustomSkinnedCmdBuffer != 0 &&
				m_ForwardTransparentCustomSkinnedInfoBuffer != 0) {
				std::vector<DrawElementsIndirectCommand> commands;
				std::vector<DrawInfo> infos;
				commands.reserve(m_ForwardTransparentCustomSkinnedItems.size());
				infos.reserve(m_ForwardTransparentCustomSkinnedItems.size());
				for (const auto& item : m_ForwardTransparentCustomSkinnedItems) {
					commands.push_back(item.command);
					infos.push_back(item.info);
				}

				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomSkinnedCmdBuffer);
				glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
					static_cast<GLsizeiptr>(commands.size() * sizeof(DrawElementsIndirectCommand)),
					commands.data());

				glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ForwardTransparentCustomSkinnedInfoBuffer);
				glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
					static_cast<GLsizeiptr>(infos.size() * sizeof(DrawInfo)),
					infos.data());
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		};
		uploadSortedCustomTransparentBuffers();
		m_NeedsTransparentSort = false;  // Reset flag after sorting

		// Second pass: Render transparent objects in sorted order
		if (!m_transparentObjects.empty()) {
			// Enable alpha blending for transparency
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendEquation(GL_FUNC_ADD);

			// Enable depth testing but disable depth writing
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_FALSE);

			// Disable face culling for transparent objects
			glDisable(GL_CULL_FACE);

			// Render transparent objects back-to-front
			for (const auto& transparentObj : m_transparentObjects) {
				EntityID entity = transparentObj.entity;

				// Handle ModelComponent entities
				if (ecs.HasComponent<ModelComponent>(entity)) {
					auto& modelComp = ecs.GetComponent<ModelComponent>(entity);
					if (modelComp.m_model) {
						// Get material for proper transparency shader
						Ermine::graphics::Material* material = nullptr;
						if (ecs.HasComponent<Ermine::Material>(entity)) {
							auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
							material = materialComponent.GetMaterial();
						}
					}
				}
				// Handle Mesh entities
				else if (ecs.HasComponent<Mesh>(entity) && ecs.HasComponent<Ermine::Material>(entity)) {
					auto& mesh = ecs.GetComponent<Mesh>(entity);
					auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);

					if (!mesh.vertex_array || !mesh.index_buffer) continue;

					Ermine::graphics::Material* material = materialComponent.GetMaterial();
					if (!material) continue;

					auto shader = material->GetShader();
					if (!shader || !shader->IsValid()) continue;

					// Bind material
					material->Bind();

					// Bind uniform blocks
					BindMaterialBlockIfPresent(shader);

					// Set transformation matrices
					shader->SetUniformMatrix4fv("model", transparentObj.modelMatrix);
					shader->SetUniformMatrix4fv("view", &view.m2[0][0]);
					shader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);

					// Calculate normal matrix
					glmView = glm::mat4(
						view.m00, view.m01, view.m02, view.m03,
						view.m10, view.m11, view.m12, view.m13,
						view.m20, view.m21, view.m22, view.m23,
						view.m30, view.m31, view.m32, view.m33
					);

					glm::mat4 modelView = glmView * transparentObj.modelMatrix;
					glm::mat3 normalMatrix = transpose(inverse(glm::mat3(modelView)));
					shader->SetUniformMatrix3fv("NormalMatrix", normalMatrix);

					// Set shading mode
					shader->SetUniform1i("isBlinnPhong", m_IsBlinnPhong ? 1 : 0);

					// Bind textures
					int texUnit = 0;
					if (material->HasParameter("materialAlbedoMap")) {
						std::shared_ptr<Texture> albedo = material->GetParameter("materialAlbedoMap")->texture;
						if (albedo && albedo->IsValid()) {
							albedo->Bind(texUnit);
						}
					}
					texUnit++;

					if (material->HasParameter("materialNormalMap")) {
						std::shared_ptr<Texture> normal = material->GetParameter("materialNormalMap")->texture;
						if (normal && normal->IsValid()) {
							normal->Bind(texUnit);
							shader->SetUniform1i("materialNormalMap", texUnit);
						}
					}
					texUnit++;

					if (material->HasParameter("materialRoughnessMap")) {
						std::shared_ptr<Texture> roughness = material->GetParameter("materialRoughnessMap")->texture;
						if (roughness && roughness->IsValid()) {
							roughness->Bind(texUnit);
							shader->SetUniform1i("materialRoughnessMap", texUnit);
						}
					}
					texUnit++;

					if (material->HasParameter("materialMetallicMap")) {
						std::shared_ptr<Texture> metallic = material->GetParameter("materialMetallicMap")->texture;
						if (metallic && metallic->IsValid()) {
							metallic->Bind(texUnit);
							shader->SetUniform1i("materialMetallicMap", texUnit);
						}
					}
					texUnit++;

					// Draw the mesh
					Draw(mesh.vertex_array, mesh.index_buffer);

					// Unbind material
					material->Unbind();
				}
			}

			// Restore render state after transparent rendering
			glDepthMask(GL_TRUE);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glDisable(GL_BLEND);
		}

	}

	// Increment  frame counter at the end of the frame
	frameCounter++;
}

/**
 * @brief Draw the mesh
 * @param vao The vertex array object
 * @param ibo The index buffer object
 * @param shader The shader object
 */
void Renderer::Draw(const std::shared_ptr<VertexArray>& vao, const std::shared_ptr<IndexBuffer>& ibo) const
{
	vao->Bind();
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(ibo->GetCount()), GL_UNSIGNED_INT, 0);
	GPUProfiler::TrackDrawCall(
		static_cast<uint32_t>(vao->GetVertexCount()),
		ibo->GetCount()
	);
	vao->Unbind();
}

/**
 * @brief Draws a mesh using instanced rendering.
 * @param vao Vertex array object.
 * @param ibo Index buffer object.
 * @param instanceCount Number of instances.
 */
void Renderer::DrawInstanced(const std::shared_ptr<VertexArray>& vao, const std::shared_ptr<IndexBuffer>& ibo, int instanceCount) const
{
	vao->Bind();
	glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(ibo->GetCount()), GL_UNSIGNED_INT, 0, instanceCount);
	GPUProfiler::TrackDrawCall(
		static_cast<uint32_t>(vao->GetVertexCount()),
		ibo->GetCount()
	);
	vao->Unbind();
}

/**
 * @brief Clear the screen
 */
void Renderer::Clear() const
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * @brief Get the current performance metrics
 * @return CurrentGPU performance metrics
 */
const GPUProfiler::PerformanceMetrics& Renderer::GetPerformanceMetrics() const
{
	return GPUProfiler::GetMetrics();
}

/**
 * @brief Destructor for Renderer
 */
Renderer::~Renderer()
{
	// Check if we have a valid OpenGL context
	if (glfwGetCurrentContext() == nullptr)
	{
		// DO NOT LOG - logger may be destroyed during shutdown
		return;
	}

	// Make all bindless texture handles non-resident FIRST
	try
	{
		// Make texture array handles non-resident
		if (m_TextureArraySSBO != 0)
		{
			// Get all texture handles and make them non-resident
			for (GLuint textureID : m_TextureArray)
			{
				GLuint64 handle = glGetTextureHandleARB(textureID);
				if (handle != 0 && glIsTextureHandleResidentARB(handle))
				{
					glMakeTextureHandleNonResidentARB(handle);
				}
			}
			m_TextureArray.clear();
			m_TextureIDToIndex.clear();
			m_TexturePathToIndex.clear();
		}

		// Clean up shadow map handle
		if (m_ShadowMapArrayHandle != 0)
		{
			if (glIsTextureHandleResidentARB(m_ShadowMapArrayHandle))
			{
				glMakeTextureHandleNonResidentARB(m_ShadowMapArrayHandle);
			}
			m_ShadowMapArrayHandle = 0;
		}

		// Clean up IGN texture
		if (m_IGNTexture != 0)
		{
			if (m_IGNTextureHandle != 0 && glIsTextureHandleResidentARB(m_IGNTextureHandle))
			{
				glMakeTextureHandleNonResidentARB(m_IGNTextureHandle);
				m_IGNTextureHandle = 0;
			}
			glDeleteTextures(1, &m_IGNTexture);
			m_IGNTexture = 0;
		}

		// Clean up g-buffer handles
		CleanupGBuffer();

		// Clean up post-process buffers
		CleanupPostProcessBuffer();

		// Now delete buffers
		if (m_LightsSSBO)
		{
			glDeleteBuffers(1, &m_LightsSSBO);
			m_LightsSSBO = 0;
			m_LightsSSBOCapacity = 0;
			m_LastUploadedLightCount = 0;
		}

		if (m_MaterialSSBO)
		{
			glDeleteBuffers(1, &m_MaterialSSBO);
			m_MaterialSSBO = 0;
		}

		if (m_TextureArraySSBO)
		{
			glDeleteBuffers(1, &m_TextureArraySSBO);
			m_TextureArraySSBO = 0;
		}
		if (m_ShadowViewSSBO)
		{
			glDeleteBuffers(1, &m_ShadowViewSSBO);
			m_ShadowViewSSBO = 0;
			m_ShadowViewSSBOCapacity = 0;
		}

		// Clean up opaque custom shader buffers
		if (m_ForwardOpaqueCustomStandardCmdBuffer != 0)
		{
			glDeleteBuffers(1, &m_ForwardOpaqueCustomStandardCmdBuffer);
			m_ForwardOpaqueCustomStandardCmdBuffer = 0;
		}
		if (m_ForwardOpaqueCustomStandardInfoBuffer != 0)
		{
			glDeleteBuffers(1, &m_ForwardOpaqueCustomStandardInfoBuffer);
			m_ForwardOpaqueCustomStandardInfoBuffer = 0;
		}
		if (m_ForwardOpaqueCustomSkinnedCmdBuffer != 0)
		{
			glDeleteBuffers(1, &m_ForwardOpaqueCustomSkinnedCmdBuffer);
			m_ForwardOpaqueCustomSkinnedCmdBuffer = 0;
		}
		if (m_ForwardOpaqueCustomSkinnedInfoBuffer != 0)
		{
			glDeleteBuffers(1, &m_ForwardOpaqueCustomSkinnedInfoBuffer);
			m_ForwardOpaqueCustomSkinnedInfoBuffer = 0;
		}

		// Delete shadow map resources
		if (m_ShadowMapArray)
		{
			glDeleteTextures(1, &m_ShadowMapArray);
			m_ShadowMapArray = 0;
		}

		if (m_ShadowMapFBO)
		{
			glDeleteFramebuffers(1, &m_ShadowMapFBO);
			m_ShadowMapFBO = 0;
		}

		// Clean up offscreen buffer
		if (m_OffscreenBuffer)
		{
			if (m_OffscreenBuffer->FBO != 0)
			{
				glDeleteFramebuffers(1, &m_OffscreenBuffer->FBO);
			}
			if (m_OffscreenBuffer->ColorTexture != 0)
			{
				glDeleteTextures(1, &m_OffscreenBuffer->ColorTexture);
			}
			if (m_OffscreenBuffer->RBO != 0)
			{
				glDeleteRenderbuffers(1, &m_OffscreenBuffer->RBO);
			}
			m_OffscreenBuffer.reset();
		}

		// Clean up picking buffer
		if (m_PickingBuffer)
		{
			if (m_PickingBuffer->FBO != 0)
			{
				glDeleteFramebuffers(1, &m_PickingBuffer->FBO);
			}
			if (m_PickingBuffer->ColorID != 0)
			{
				glDeleteTextures(1, &m_PickingBuffer->ColorID);
			}
			if (m_PickingBuffer->Depth != 0)
			{
				glDeleteRenderbuffers(1, &m_PickingBuffer->Depth);
			}
			m_PickingBuffer.reset();
		}

		// Clean up debug rendering resources
		if (m_DebugVAO != 0)
		{
			glDeleteVertexArrays(1, &m_DebugVAO);
			m_DebugVAO = 0;
		}

		if (m_DebugVBO != 0)
		{
			glDeleteBuffers(1, &m_DebugVBO);
			m_DebugVBO = 0;
		}

		// Don't call glCheckError here as context might be shutting down
		// Instead, just clear any pending errors silently
		while (glGetError() != GL_NO_ERROR);
	}
	catch (...)
	{
	}
}

/**
 * @brief Toggles between forward and deferred rendering pipelines.
 *
 * This function flips the internal flag @c m_UseDeferredRendering. When enabled,
 * all rendering will go through the deferred pipeline using a G-buffer and lighting pass.
 * When disabled, rendering falls back to forward shading, where each object is drawn directly
 * with its material and lighting applied in a single pass.
 *
 * It also logs a message indicating the current rendering mode.
 */
void Renderer::ToggleDeferredRendering()
{
	m_UseDeferredRendering = !m_UseDeferredRendering;
	if (m_UseDeferredRendering)
		EE_CORE_INFO("Switched to Deferred Rendering");
	else
		EE_CORE_INFO("Switched to Forward Rendering");

}

/**
 * @brief Renders a model using the deferred rendering pipeline.
 *
 * In this mode, the function uses the shared G-buffer shader (@c m_GBufferShader) to
 * write geometry data (position, normals, material properties) into the G-buffer.
 * Per-entity materials are not bound as shaders, but their UBO data (albedo, metallic,
 * roughness, emissive, etc.) is uploaded to the GPU so the G-buffer can store them.
 *
 * @param model The model to render, containing mesh geometry and local transforms.
 * @param material Pointer to the material providing UBO data (albedo, metallic, etc.).
 * @param view The view matrix representing the camera transform.
 * @param projection The projection matrix (perspective or orthographic).
 * @param rootTransform Root transform matrix for the entity (translation, rotation, scale).
 */
void Renderer::RenderModelDeferred(const Model& model, graphics::Material* material, const Mtx44& view, const Mtx44& projection, const glm::mat4& rootTransform)
{
	const auto& meshes = model.GetMeshes();
	if (meshes.empty() || !material) return;
	if (!m_GBufferShader || !m_GBufferShader->IsValid()) return;

	// Bind shared g-buffer shader used to write geometry information
	m_GBufferShader->Bind();

	// convert the Mtx44 view/projection into glm mats for convenience
	glm::mat4 glmView = ToGlm(view);
	glm::mat4 glmProj = ToGlm(projection);

	// Get bone transforms (may be empty for static meshes)
	const auto& boneTransforms = model.GetBoneTransforms();
	const bool hasBones = !boneTransforms.empty();
	m_GBufferShader->SetUniform1i("u_UseSkinning", hasBones ? 1 : 0); // Enable skinning for model

	// Upload bone matrices if present
	if (hasBones)
	{
		GLsizei count = std::min((int)boneTransforms.size(), MAX_BONE_UNIFORMS);
		GLint loc = glGetUniformLocation(m_GBufferShader->GetRendererID(), "u_BoneMatrices");
		glUniformMatrix4fv(loc, count, GL_FALSE, glm::value_ptr(boneTransforms[0]));
	}

	for (const auto& mesh : meshes)
	{
		if (!mesh.vao || !mesh.ibo) continue;

		glm::mat4 modelMat = rootTransform * mesh.localTransform;
		m_GBufferShader->SetUniformMatrix4fv("model", modelMat);
		m_GBufferShader->SetUniformMatrix4fv("view", glmView);
		m_GBufferShader->SetUniformMatrix4fv("projection", glmProj);

		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMat)));
		m_GBufferShader->SetUniformMatrix3fv("NormalMatrix", normalMatrix);

		Draw(mesh.vao, mesh.ibo);
	}
}

/**
 * @brief Renders a model using the forward rendering pipeline.
 *
 * In this mode, the function binds the entity's own material and its shader, then
 * issues draw calls for each mesh in the model. Lighting and material shading are
 * evaluated directly during rasterization (per-fragment).
 *
 * @param model The model to render, containing mesh geometry and local transforms.
 * @param material Pointer to the material to bind, providing textures and shader.
 * @param view The view matrix representing the camera transform.
 * @param projection The projection matrix (perspective or orthographic).
 * @param rootTransform Root transform matrix for the entity (translation, rotation, scale).
 */
void Renderer::RenderModelForward(const Model& model, graphics::Material* material, const Mtx44& view, const Mtx44& projection, const glm::mat4& rootTransform)
{
	const auto& meshes = model.GetMeshes();
	if (meshes.empty() || !material) return;

	auto shader = material->GetShader();
	if (!shader || !shader->IsValid()) return;


	glm::mat4 glmView = ToGlm(view);
	glm::mat4 glmProj = ToGlm(projection);

	const auto& boneTransforms = model.GetBoneTransforms();
	const bool hasBones = !boneTransforms.empty();
	shader->SetUniform1i("u_UseSkinning", hasBones ? 1 : 0); // Enable skinning for model

	// bind material (textures, shader)
	material->Bind();
	BindMaterialBlockIfPresent(shader);

	// Upload bone matrices if present
	if (hasBones)
	{
		GLsizei count = std::min((int)boneTransforms.size(), MAX_BONE_UNIFORMS);
		GLint loc = glGetUniformLocation(shader->GetRendererID(), "u_BoneMatrices");
		glUniformMatrix4fv(loc, count, GL_FALSE, glm::value_ptr(boneTransforms[0]));
	}

	for (const auto& mesh : meshes)
	{
		if (!mesh.vao || !mesh.ibo) continue;

		glm::mat4 modelMat = rootTransform * mesh.localTransform;
		shader->SetUniformMatrix4fv("model", modelMat);
		shader->SetUniformMatrix4fv("view", glmView);
		shader->SetUniformMatrix4fv("projection", glmProj);

		glm::mat4 modelView = glmView * modelMat;
		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelView)));
		shader->SetUniformMatrix3fv("NormalMatrix", normalMatrix);
		Draw(mesh.vao, mesh.ibo);
	}

	material->Unbind();
}

/**
 * @brief Checks if a material is transparent.
 * @param material Material pointer.
 * @return true if transparent, false otherwise.
 */
bool Renderer::IsTransparentMaterial(const Ermine::graphics::Material* material) const
{
	if (!material) return false;

	// Check if material has albedo with alpha for transparency
	if (auto albedoParam = material->GetParameter("materialAlbedo")) {
		if (albedoParam->type == MaterialParamType::VEC4 &&
			albedoParam->floatValues.size() >= 4) {
			float alpha = albedoParam->floatValues[3];
			return alpha < 0.99f; // Consider transparent if alpha < 99%
		}
		// If albedo is Vec3, check if material has an albedo texture with alpha
		else if (albedoParam->type == MaterialParamType::VEC3) {
			// Check if there's an albedo texture that might have alpha
			if (auto albedoTexParam = material->GetParameter("materialAlbedoMap")) {
				if (albedoTexParam->type == MaterialParamType::TEXTURE_2D &&
					albedoTexParam->texture && albedoTexParam->texture->IsValid()) {
					// For texture-based materials, we can't easily check alpha without loading the texture
					// For now, assume opaque unless explicitly marked as transparent
					return false;
				}
			}
		}
	}

	// Default to opaque if no transparency information is found
	return false;
}

/**
 * @brief Checks if a material casts shadows.
 * @param material Material pointer.
 * @return true if casts shadows, false otherwise.
 */
bool Renderer::CastsShadows(const Ermine::graphics::Material* material) const
{
	if (!material) return true; // Default to true for missing materials

	// Check materialCastsShadows parameter
	if (auto param = material->GetParameter("materialCastsShadows")) {
		return param->boolValue;
	}

	// Default to true if parameter not set
	return true;
}

bool Renderer::HasCustomShader(const Ermine::graphics::Material* material) const
{
	if (!material) return false;

	// Get the shader from the material
	auto shader = material->GetShader();

	// If no shader is set, it's not a custom shader
	if (!shader) return false;

	// If standard shaders aren't initialized yet, we can't determine if this is custom
	// Safest assumption is to treat it as standard (return false) to avoid routing issues
	if (!m_GBufferShader || !m_ForwardShader) {
		return false;
	}

	// Exclude standard shaders: geometry shader (deferred), forward shader, or nullptr
	bool isStandardShader = (shader == m_GBufferShader) || (shader == m_ForwardShader) || (shader == nullptr);

	return !isStandardShader;
}

/**
 * @brief Sorts opaque custom shader objects by shader pointer to minimize state changes.
 * Opaque objects don't need distance sorting (no blending), only shader batching.
 */
void Renderer::SortOpaqueCustomShadersByShader()
{
	// ========== SORT OPAQUE CUSTOM STANDARD MESHES ==========
	// Sort by shader pointer only (no distance sorting needed for opaque)
	// Direct sort on items - shader travels with its data, no alignment issues!
	if (!m_ForwardOpaqueCustomStandardItems.empty()) {
		std::sort(m_ForwardOpaqueCustomStandardItems.begin(),
		          m_ForwardOpaqueCustomStandardItems.end(),
		          [](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
			          return a.shader.get() < b.shader.get(); // Group by shader
		          });
	}

	// ========== SORT OPAQUE CUSTOM SKINNED MESHES ==========
	// Sort by shader pointer only (no distance sorting needed for opaque)
	if (!m_ForwardOpaqueCustomSkinnedItems.empty()) {
		std::sort(m_ForwardOpaqueCustomSkinnedItems.begin(),
		          m_ForwardOpaqueCustomSkinnedItems.end(),
		          [](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
			          return a.shader.get() < b.shader.get(); // Group by shader
		          });
	}
}

/**
 * @brief Sorts transparent objects by shader first, then by distance to camera.
 * This minimizes shader state changes and improves alpha blending correctness.
 * @param cameraPos Camera position.
 */
void Renderer::SortTransparentObjects(const Vec3& cameraPos, bool fullRebuild)
{
	// Skip if no transparent objects at all
	bool hasTransparentStandard = !m_ForwardTransparentDefaultStandardItems.empty() || !m_ForwardTransparentDefaultSkinnedItems.empty();
	bool hasTransparentCustom = !m_ForwardTransparentCustomStandardItems.empty() || !m_ForwardTransparentCustomSkinnedItems.empty();

	if (!hasTransparentStandard && !hasTransparentCustom) return;

	glm::vec3 camPos = glm::vec3(cameraPos.x, cameraPos.y, cameraPos.z);

	// ========== SORT TRANSPARENT DEFAULT STANDARD MESHES (NON-SKINNED) ==========
	// These all use the same shader, so just sort by distance for proper blending
	if (!m_ForwardTransparentDefaultStandardItems.empty()) {
		// Sort back-to-front by distance for correct alpha blending
		std::sort(m_ForwardTransparentDefaultStandardItems.begin(),
		          m_ForwardTransparentDefaultStandardItems.end(),
		          [&camPos](const DefaultShaderDrawItem& a, const DefaultShaderDrawItem& b) {
			          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
			          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
			          float distA = glm::distance(posA, camPos);
			          float distB = glm::distance(posB, camPos);
			          return distA > distB; // Back-to-front
		          });
	}

	// ========== SORT TRANSPARENT DEFAULT SKINNED MESHES ==========
	// These all use the same shader, so just sort by distance for proper blending
	if (!m_ForwardTransparentDefaultSkinnedItems.empty()) {
		// Sort back-to-front by distance for correct alpha blending
		std::sort(m_ForwardTransparentDefaultSkinnedItems.begin(),
		          m_ForwardTransparentDefaultSkinnedItems.end(),
		          [&camPos](const DefaultShaderDrawItem& a, const DefaultShaderDrawItem& b) {
			          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
			          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
			          float distA = glm::distance(posA, camPos);
			          float distB = glm::distance(posB, camPos);
			          return distA > distB; // Back-to-front
		          });
	}

	// ========== SORT TRANSPARENT CUSTOM STANDARD MESHES (NON-SKINNED) ==========
	if (!m_ForwardTransparentCustomStandardItems.empty()) {
		if (fullRebuild) {
			// FULL REBUILD: Sort by SHADER first (batching), then by DISTANCE within each shader group
			std::sort(m_ForwardTransparentCustomStandardItems.begin(),
			          m_ForwardTransparentCustomStandardItems.end(),
			          [&camPos](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
				          // Primary sort: Shader pointer (creates batches)
				          if (a.shader.get() != b.shader.get()) {
					          return a.shader.get() < b.shader.get();
				          }

				          // Secondary sort: Distance back-to-front within each shader group
				          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
				          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
				          float distA = glm::distance(posA, camPos);
				          float distB = glm::distance(posB, camPos);
				          return distA > distB; // Back-to-front
			          });
		}
		else {
			// FAST REBUILD: Sort by DISTANCE only within each existing shader group
			// Find shader group boundaries and sort each group independently
			size_t groupStart = 0;
			for (size_t i = 1; i <= m_ForwardTransparentCustomStandardItems.size(); ++i) {
				// Check if we've reached a new shader or end of array
				bool newGroup = (i == m_ForwardTransparentCustomStandardItems.size()) ||
				                (m_ForwardTransparentCustomStandardItems[i].shader.get() !=
				                 m_ForwardTransparentCustomStandardItems[groupStart].shader.get());

				if (newGroup) {
					// Sort this shader group by distance
					std::sort(m_ForwardTransparentCustomStandardItems.begin() + groupStart,
					          m_ForwardTransparentCustomStandardItems.begin() + i,
					          [&camPos](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
						          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
						          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
						          float distA = glm::distance(posA, camPos);
						          float distB = glm::distance(posB, camPos);
						          return distA > distB; // Back-to-front
					          });
					groupStart = i;
				}
			}
		}
	}

	// ========== SORT TRANSPARENT CUSTOM SKINNED MESHES ==========
	if (!m_ForwardTransparentCustomSkinnedItems.empty()) {
		if (fullRebuild) {
			// FULL REBUILD: Sort by SHADER first (batching), then by DISTANCE within each shader group
			std::sort(m_ForwardTransparentCustomSkinnedItems.begin(),
			          m_ForwardTransparentCustomSkinnedItems.end(),
			          [&camPos](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
				          // Primary sort: Shader pointer (creates batches)
				          if (a.shader.get() != b.shader.get()) {
					          return a.shader.get() < b.shader.get();
				          }

				          // Secondary sort: Distance back-to-front within each shader group
				          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
				          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
				          float distA = glm::distance(posA, camPos);
				          float distB = glm::distance(posB, camPos);
				          return distA > distB; // Back-to-front
			          });
		}
		else {
			// FAST REBUILD: Sort by DISTANCE only within each existing shader group
			// Find shader group boundaries and sort each group independently
			size_t groupStart = 0;
			for (size_t i = 1; i <= m_ForwardTransparentCustomSkinnedItems.size(); ++i) {
				// Check if we've reached a new shader or end of array
				bool newGroup = (i == m_ForwardTransparentCustomSkinnedItems.size()) ||
				                (m_ForwardTransparentCustomSkinnedItems[i].shader.get() !=
				                 m_ForwardTransparentCustomSkinnedItems[groupStart].shader.get());

				if (newGroup) {
					// Sort this shader group by distance
					std::sort(m_ForwardTransparentCustomSkinnedItems.begin() + groupStart,
					          m_ForwardTransparentCustomSkinnedItems.begin() + i,
					          [&camPos](const CustomShaderDrawItem& a, const CustomShaderDrawItem& b) {
						          glm::vec3 posA = glm::vec3(a.info.modelMatrix[3]);
						          glm::vec3 posB = glm::vec3(b.info.modelMatrix[3]);
						          float distA = glm::distance(posA, camPos);
						          float distB = glm::distance(posB, camPos);
						          return distA > distB; // Back-to-front
					          });
					groupStart = i;
				}
			}
		}
	}
}

/**
 * @brief Renders opaque objects with custom shaders before the transparent pass.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderOpaqueCustomShaders(const Mtx44& view, const Mtx44& projection)
{
	static bool s_DisableOpaqueCustomMDI = false;
	auto drawCustomBatch = [&](const std::vector<CustomShaderDrawItem>& items,
		size_t batchStart,
		size_t batchEnd,
		const std::shared_ptr<graphics::Shader>& shaderToBind) {
		const size_t batchSize = batchEnd - batchStart;
		if (batchSize == 0 || !shaderToBind) {
			return;
		}

		const size_t byteOffset = batchStart * sizeof(DrawElementsIndirectCommand);
		if (!s_DisableOpaqueCustomMDI) {
			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				GL_UNSIGNED_INT,
				reinterpret_cast<const void*>(byteOffset),
				static_cast<GLsizei>(batchSize),
				0
			);
			if (glGetError() != GL_NO_ERROR) {
				s_DisableOpaqueCustomMDI = true;
			}
		}

		if (s_DisableOpaqueCustomMDI) {
			for (size_t drawIdx = batchStart; drawIdx < batchEnd; ++drawIdx) {
				const auto& cmd = items[drawIdx].command;
				shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(drawIdx));
				glDrawElementsInstancedBaseVertexBaseInstance(
					GL_TRIANGLES,
					static_cast<GLsizei>(cmd.count),
					GL_UNSIGNED_INT,
					reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.firstIndex) * sizeof(uint32_t)),
					static_cast<GLsizei>(cmd.instanceCount),
					static_cast<GLint>(cmd.baseVertex),
					cmd.baseInstance
				);
			}
		}
	};
	// Skip if no opaque custom shader meshes uploaded to GPU
	if (m_ForwardOpaqueCustomStandardUploadedCount == 0 && m_ForwardOpaqueCustomSkinnedUploadedCount == 0) {
		return;
	}

	// Bind the post-process buffer where the opaque scene is being rendered
	if (!m_PostProcessBuffer) {
		EE_CORE_ERROR("Post-process buffer not initialized for opaque custom shader pass!");
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
	glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

	// Render states for opaque custom shaders
	glDisable(GL_BLEND);              // NO alpha blending
	glEnable(GL_DEPTH_TEST);          // Enable depth testing
	glDepthFunc(GL_LEQUAL);           // Use LEQUAL to match geometry pass
	glDepthMask(GL_FALSE);            // DON'T write depth (depth pre-pass already wrote it)
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// ========== RENDER OPAQUE CUSTOM STANDARD MESHES (NON-SKINNED) ==========
	if (m_ForwardOpaqueCustomStandardUploadedCount > 0 && !m_ForwardOpaqueCustomStandardItems.empty()) {
		const auto& items = m_ForwardOpaqueCustomStandardItems;
		const size_t renderCount = std::min(m_ForwardOpaqueCustomStandardUploadedCount, items.size());
		GLuint standardVAO = m_MeshManager.GetStandardVAO();
		// Check that VAO and GPU buffers are valid before binding
		if (standardVAO != 0 && m_ForwardOpaqueCustomStandardInfoBuffer != 0 && m_ForwardOpaqueCustomStandardCmdBuffer != 0) {
			glBindVertexArray(standardVAO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardOpaqueCustomStandardInfoBuffer);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomStandardCmdBuffer);

			// Batch draws by shader
			size_t batchStart = 0;
			std::shared_ptr<graphics::Shader> currentShader = items[0].shader;

			for (size_t i = 0; i <= renderCount; ++i) {
				// Check if we've reached end or shader changed
				bool shaderChanged = (i < renderCount && items[i].shader != currentShader);
				bool isEnd = (i == renderCount);

				if (shaderChanged || isEnd) {
					// Bind shader
					std::shared_ptr<graphics::Shader> shaderToBind = currentShader;
					if (shaderToBind && shaderToBind->IsValid()) {
						shaderToBind->Bind();
						BindMaterialBlockIfPresent(shaderToBind);
						shaderToBind->SetUniformMatrix4fv("view", &view.m2[0][0]);
						shaderToBind->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
						shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(batchStart));
						shaderToBind->SetUniform1f("u_Time", m_ElapsedTime);

						// Bind IGN texture for volumetric effects
						if (m_IGNTextureHandle != 0) {
							GLint locIGN = glGetUniformLocation(shaderToBind->GetRendererID(), "u_IGNHandle");
							if (locIGN != -1) {
								glUniform2ui(locIGN,
									static_cast<GLuint>(m_IGNTextureHandle),
									static_cast<GLuint>(m_IGNTextureHandle >> 32));
							}
						}
						shaderToBind->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

						drawCustomBatch(items, batchStart, i, shaderToBind);
					}

					// Start new batch
					if (i < renderCount) {
						batchStart = i;
						currentShader = items[i].shader;
					}
				}
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			glBindVertexArray(0);
		}
	}

	// ========== RENDER OPAQUE CUSTOM SKINNED MESHES ==========
	if (m_ForwardOpaqueCustomSkinnedUploadedCount > 0 && !m_ForwardOpaqueCustomSkinnedItems.empty()) {
		const auto& items = m_ForwardOpaqueCustomSkinnedItems;
		const size_t renderCount = std::min(m_ForwardOpaqueCustomSkinnedUploadedCount, items.size());
		GLuint skinnedVAO = m_MeshManager.GetSkinnedVAO();
		// Check that VAO and GPU buffers are valid before binding
		if (skinnedVAO != 0 && m_ForwardOpaqueCustomSkinnedInfoBuffer != 0 && m_ForwardOpaqueCustomSkinnedCmdBuffer != 0) {
			glBindVertexArray(skinnedVAO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SKELETAL_SSBO_BINDING, m_MeshManager.m_SkeletalSSBO.GetBufferID());
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardOpaqueCustomSkinnedInfoBuffer);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomSkinnedCmdBuffer);

			// Batch draws by shader
			size_t batchStart = 0;
			std::shared_ptr<graphics::Shader> currentShader = items[0].shader;

			for (size_t i = 0; i <= renderCount; ++i) {
				// Check if we've reached end or shader changed
				bool shaderChanged = (i < renderCount && items[i].shader != currentShader);
				bool isEnd = (i == renderCount);

				if (shaderChanged || isEnd) {
					// Bind shader
					std::shared_ptr<graphics::Shader> shaderToBind = currentShader;
					if (shaderToBind && shaderToBind->IsValid()) {
						shaderToBind->Bind();
						BindMaterialBlockIfPresent(shaderToBind);
						shaderToBind->SetUniformMatrix4fv("view", &view.m2[0][0]);
						shaderToBind->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
						shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(batchStart));
						shaderToBind->SetUniform1f("u_Time", m_ElapsedTime);

						// Bind IGN texture for volumetric effects
						if (m_IGNTextureHandle != 0) {
							GLint locIGN = glGetUniformLocation(shaderToBind->GetRendererID(), "u_IGNHandle");
							if (locIGN != -1) {
								glUniform2ui(locIGN,
									static_cast<GLuint>(m_IGNTextureHandle),
									static_cast<GLuint>(m_IGNTextureHandle >> 32));
							}
						}
						shaderToBind->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

						drawCustomBatch(items, batchStart, i, shaderToBind);
					}

					// Start new batch
					if (i < renderCount) {
						batchStart = i;
						currentShader = items[i].shader;
					}
				}
			}

			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
			glBindVertexArray(0);
		}
	}

	// Unbind shader to prevent interference with subsequent passes
	// NOTE: SSBOs and indirect buffer remain bound - they're managed by each render pass
	glUseProgram(0);
	glBindVertexArray(0);
}

/**
 * @brief Renders transparent objects with custom shaders.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderTransparentCustomShaders(const Mtx44& view, const Mtx44& projection)
{
	static bool s_DisableTransparentCustomMDI = false;
	auto drawCustomBatch = [&](const std::vector<CustomShaderDrawItem>& items,
		size_t batchStart,
		size_t batchEnd,
		const std::shared_ptr<graphics::Shader>& shaderToBind) {
		const size_t batchSize = batchEnd - batchStart;
		if (batchSize == 0 || !shaderToBind) {
			return;
		}

		const size_t byteOffset = batchStart * sizeof(DrawElementsIndirectCommand);
		if (!s_DisableTransparentCustomMDI) {
			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				GL_UNSIGNED_INT,
				reinterpret_cast<const void*>(byteOffset),
				static_cast<GLsizei>(batchSize),
				0
			);
			if (glGetError() != GL_NO_ERROR) {
				s_DisableTransparentCustomMDI = true;
			}
		}

		if (s_DisableTransparentCustomMDI) {
			for (size_t drawIdx = batchStart; drawIdx < batchEnd; ++drawIdx) {
				const auto& cmd = items[drawIdx].command;
				shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(drawIdx));
				glDrawElementsInstancedBaseVertexBaseInstance(
					GL_TRIANGLES,
					static_cast<GLsizei>(cmd.count),
					GL_UNSIGNED_INT,
					reinterpret_cast<const void*>(static_cast<uintptr_t>(cmd.firstIndex) * sizeof(uint32_t)),
					static_cast<GLsizei>(cmd.instanceCount),
					static_cast<GLint>(cmd.baseVertex),
					cmd.baseInstance
				);
			}
		}
	};
	// Skip if no transparent custom shader meshes uploaded to GPU
	if (m_ForwardTransparentCustomStandardUploadedCount == 0 && m_ForwardTransparentCustomSkinnedUploadedCount == 0) {
		return;
	}

	// GL state is already set up by RenderForwardPass (blending, depth, etc.)
	// Buffers were already uploaded in CompileDrawData - just bind and draw

	// ========== RENDER TRANSPARENT CUSTOM STANDARD MESHES (NON-SKINNED) ==========
	if (m_ForwardTransparentCustomStandardUploadedCount > 0 && !m_ForwardTransparentCustomStandardItems.empty() && m_MeshManager.GetStandardVAO() != 0
		&& m_ForwardTransparentCustomStandardInfoBuffer != 0 && m_ForwardTransparentCustomStandardCmdBuffer != 0) {
		const auto& items = m_ForwardTransparentCustomStandardItems;
		const size_t renderCount = std::min(m_ForwardTransparentCustomStandardUploadedCount, items.size());
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardTransparentCustomStandardInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomStandardCmdBuffer);

		// Batch draws by shader (each unique shader gets its own draw call)
		size_t batchStart = 0;
		std::shared_ptr<graphics::Shader> currentShader = items[0].shader;

		for (size_t i = 0; i <= renderCount; ++i) {
			// Check if we've reached end or shader changed
			bool shaderChanged = (i < renderCount && items[i].shader != currentShader);
			bool isEnd = (i == renderCount);

			if (shaderChanged || isEnd) {
				// Bind shader
				std::shared_ptr<graphics::Shader> shaderToBind = currentShader;
				if (shaderToBind && shaderToBind->IsValid()) {
					shaderToBind->Bind();
					BindMaterialBlockIfPresent(shaderToBind);
					shaderToBind->SetUniformMatrix4fv("view", &view.m2[0][0]);
					shaderToBind->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
					shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(batchStart));
					shaderToBind->SetUniform1f("u_Time", m_ElapsedTime);

					// Bind IGN texture for volumetric effects
					if (m_IGNTextureHandle != 0) {
						GLint locIGN = glGetUniformLocation(shaderToBind->GetRendererID(), "u_IGNHandle");
						if (locIGN != -1) {
							glUniform2ui(locIGN,
								static_cast<GLuint>(m_IGNTextureHandle),
								static_cast<GLuint>(m_IGNTextureHandle >> 32));
						}
					}
					shaderToBind->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

					drawCustomBatch(items, batchStart, i, shaderToBind);
				}

				// Start new batch
				if (i < renderCount) {
					batchStart = i;
					currentShader = items[i].shader;
				}
			}
		}

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// ========== RENDER TRANSPARENT CUSTOM SKINNED MESHES ==========
	if (m_ForwardTransparentCustomSkinnedUploadedCount > 0 && !m_ForwardTransparentCustomSkinnedItems.empty() && m_MeshManager.GetSkinnedVAO() != 0
		&& m_ForwardTransparentCustomSkinnedInfoBuffer != 0 && m_ForwardTransparentCustomSkinnedCmdBuffer != 0) {
		const auto& items = m_ForwardTransparentCustomSkinnedItems;
		const size_t renderCount = std::min(m_ForwardTransparentCustomSkinnedUploadedCount, items.size());
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SKELETAL_SSBO_BINDING, m_MeshManager.m_SkeletalSSBO.GetBufferID());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardTransparentCustomSkinnedInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomSkinnedCmdBuffer);

		// Batch draws by shader
		size_t batchStart = 0;
		std::shared_ptr<graphics::Shader> currentShader = items[0].shader;

		for (size_t i = 0; i <= renderCount; ++i) {
			// Check if we've reached end or shader changed
			bool shaderChanged = (i < renderCount && items[i].shader != currentShader);
			bool isEnd = (i == renderCount);

			if (shaderChanged || isEnd) {
				// Bind shader
				std::shared_ptr<graphics::Shader> shaderToBind = currentShader;
				if (shaderToBind && shaderToBind->IsValid()) {
					shaderToBind->Bind();
					BindMaterialBlockIfPresent(shaderToBind);
					shaderToBind->SetUniformMatrix4fv("view", &view.m2[0][0]);
					shaderToBind->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
					shaderToBind->SetUniform1ui("baseDrawID", static_cast<uint32_t>(batchStart));
					shaderToBind->SetUniform1f("u_Time", m_ElapsedTime);

					// Bind IGN texture for volumetric effects
					if (m_IGNTextureHandle != 0) {
						GLint locIGN = glGetUniformLocation(shaderToBind->GetRendererID(), "u_IGNHandle");
						if (locIGN != -1) {
							glUniform2ui(locIGN,
								static_cast<GLuint>(m_IGNTextureHandle),
								static_cast<GLuint>(m_IGNTextureHandle >> 32));
						}
					}
					shaderToBind->SetUniform2f("u_IGNResolution", m_IGNTextureSize);

					drawCustomBatch(items, batchStart, i, shaderToBind);
				}

				// Start new batch
				if (i < renderCount) {
					batchStart = i;
					currentShader = items[i].shader;
				}
			}
		}

		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Unbind shader to prevent interference with subsequent standard transparent rendering
	glUseProgram(0);
	glBindVertexArray(0);
}

/**
 * @brief Renders all transparent objects using forward rendering.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderForwardPass(const Mtx44& view, const Mtx44& projection)
{
	// This function handles the COMPLETE forward rendering pipeline:
	// 1. Opaque custom shaders (with depth writing)
	// 2. Transparent custom shaders (with blending, no depth write)
	// 3. Transparent standard (with blending, no depth write, using m_ForwardShader)

	bool hasOpaqueCustom = !m_ForwardOpaqueCustomStandardItems.empty() || !m_ForwardOpaqueCustomSkinnedItems.empty();
	bool hasTransparentCustom = !m_ForwardTransparentCustomStandardItems.empty() || !m_ForwardTransparentCustomSkinnedItems.empty();
	bool hasTransparentStandard = !m_ForwardTransparentDefaultStandardItems.empty() || !m_ForwardTransparentDefaultSkinnedItems.empty();

	auto& ecs = ECS::GetInstance();
	auto gpuParticles = ecs.GetSystem<GPUParticleSystem>();
	bool hasGpuParticles = gpuParticles && gpuParticles->HasActiveEmitters();

	// Skip if nothing to render
	if (!hasOpaqueCustom && !hasTransparentCustom && !hasTransparentStandard && !hasGpuParticles) {
		return;
	}

	// Validate post-process buffer
	if (!m_PostProcessBuffer) {
		EE_CORE_ERROR("Post-process buffer not initialized for forward pass!");
		return;
	}

	glm::mat4 glmView = ToGlm(view);
	glm::mat4 invView = glm::inverse(glmView);
	Vec3 cameraPos = Vec3(invView[3][0], invView[3][1], invView[3][2]);

	// ========== STEP 1: RENDER OPAQUE CUSTOM SHADERS ==========
	// These render with depth writing ENABLED (before transparent objects)
	if (hasOpaqueCustom) {
		RenderOpaqueCustomShaders(view, projection);
	}

	// ========== STEP 2: RENDER TRANSPARENT CUSTOM SHADERS ==========
	// These use their own custom fragment shaders (e.g., volumetric orbs) with blending
	if (hasTransparentCustom) {
		// Set up GL state for transparent rendering
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE); // Don't write to depth buffer

		glDisable(GL_CULL_FACE);

		RenderTransparentCustomShaders(view, projection);
	}

	// ========== STEP 3: RENDER TRANSPARENT STANDARD ==========
	// These use the standard forward shader (m_ForwardShader) with blending
	if (hasTransparentStandard) {
		// Set up GL state for transparent rendering (in case we skipped custom)
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquation(GL_FUNC_ADD);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);

		glDisable(GL_CULL_FACE);
		if (!m_ForwardShader || !m_ForwardShader->IsValid()) {
			EE_CORE_ERROR("Forward shader not initialized!");
			glDepthMask(GL_TRUE);
			glEnable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			return;
		}

		// Bind default forward shader
		m_ForwardShader->Bind();

		// ========== RENDER TRANSPARENT STANDARD MESHES (NON-SKINNED) ==========
		if (!m_ForwardTransparentDefaultStandardItems.empty()) {
			GLuint standardVAO = m_MeshManager.GetStandardVAO();
			if (standardVAO != 0) {
				glBindVertexArray(standardVAO);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ForwardStandardDrawInfoBuffer.GetBufferID());
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ForwardStandardDrawCommandBuffer.GetBufferID());

				// Set uniforms for standard forward shader
				m_ForwardShader->SetUniformMatrix4fv("view", &view.m2[0][0]);
				m_ForwardShader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
				m_ForwardShader->SetUniform1ui("baseDrawID", 0);
				m_ForwardShader->SetUniform1f("u_Time", m_ElapsedTime);

				// Execute multi-draw for all standard transparent commands
				glMultiDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					nullptr,
					static_cast<GLsizei>(m_ForwardTransparentDefaultStandardItems.size()),
					0
				);

				GPUProfiler::TrackDrawCall(m_ForwardPassDrawCommandsVertexCount, m_ForwardPassDrawCommandsIndexCount);
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
				glBindVertexArray(0);
			}
		}

		// ========== RENDER TRANSPARENT STANDARD SKINNED MESHES ==========
		if (!m_ForwardTransparentDefaultSkinnedItems.empty()) {
			GLuint skinnedVAO = m_MeshManager.GetSkinnedVAO();
			if (skinnedVAO != 0) {
				glBindVertexArray(skinnedVAO);
				glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ForwardSkinnedDrawInfoBuffer.GetBufferID());
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ForwardSkinnedDrawCommandBuffer.GetBufferID());

				// Set uniforms for standard forward shader
				m_ForwardShader->SetUniformMatrix4fv("view", &view.m2[0][0]);
				m_ForwardShader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);
				m_ForwardShader->SetUniform1ui("baseDrawID", 0);
				m_ForwardShader->SetUniform1f("u_Time", m_ElapsedTime);

				// Execute multi-draw for all standard transparent skinned commands
				glMultiDrawElementsIndirect(
					GL_TRIANGLES,
					GL_UNSIGNED_INT,
					nullptr,
					static_cast<GLsizei>(m_ForwardTransparentDefaultSkinnedItems.size()),
					0
				);

				GPUProfiler::TrackDrawCall(m_ForwardPassDrawCommandsVertexCount, m_ForwardPassDrawCommandsIndexCount);
				glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
				glBindVertexArray(0);
			}
		}

	}

	if (hasGpuParticles) {
		glBindFramebuffer(GL_FRAMEBUFFER, m_PostProcessBuffer->FBO);
		glViewport(0, 0, m_PostProcessBuffer->width, m_PostProcessBuffer->height);
		gpuParticles->Render(view, projection, cameraPos);
		gpuParticles->RenderDebug(view, projection);
	}

	// Restore render state
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_BLEND);

	GPUProfiler::EndEvent();
}

void Renderer::RenderMotionBlurMask(const Mtx44& view, const Mtx44& projection)
{
	if (!m_MotionBlurMaskBuffer)
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_MotionBlurMaskBuffer->FBO);
	glViewport(0, 0, m_MotionBlurMaskBuffer->width, m_MotionBlurMaskBuffer->height);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (!m_MotionBlurEnabled || !m_MotionBlurMaskShader || !m_MotionBlurMaskShader->IsValid())
	{
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		return;
	}

	const bool hasForwardContent =
		!m_ForwardTransparentDefaultStandardItems.empty() ||
		!m_ForwardTransparentDefaultSkinnedItems.empty() ||
		(m_ForwardOpaqueCustomStandardUploadedCount > 0) ||
		(m_ForwardOpaqueCustomSkinnedUploadedCount > 0) ||
		(m_ForwardTransparentCustomStandardUploadedCount > 0) ||
		(m_ForwardTransparentCustomSkinnedUploadedCount > 0);

	if (!hasForwardContent)
	{
		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		return;
	}

	m_MotionBlurMaskShader->Bind();
	m_MotionBlurMaskShader->SetUniformMatrix4fv("view", &view.m2[0][0]);
	m_MotionBlurMaskShader->SetUniformMatrix4fv("projection", &projection.m2[0][0]);

	// Standard forward transparent (non-skinned)
	if (!m_ForwardTransparentDefaultStandardItems.empty() && m_MeshManager.GetStandardVAO() != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ForwardStandardDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ForwardStandardDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardTransparentDefaultStandardItems.size()),
			0
		);
	}

	// Standard forward transparent (skinned)
	if (!m_ForwardTransparentDefaultSkinnedItems.empty() && m_MeshManager.GetSkinnedVAO() != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ForwardSkinnedDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ForwardSkinnedDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardTransparentDefaultSkinnedItems.size()),
			0
		);
	}

	// Custom opaque (standard)
	if (m_ForwardOpaqueCustomStandardUploadedCount > 0 && m_MeshManager.GetStandardVAO() != 0 &&
		m_ForwardOpaqueCustomStandardInfoBuffer != 0 && m_ForwardOpaqueCustomStandardCmdBuffer != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardOpaqueCustomStandardInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomStandardCmdBuffer);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardOpaqueCustomStandardUploadedCount),
			0
		);
	}

	// Custom opaque (skinned)
	if (m_ForwardOpaqueCustomSkinnedUploadedCount > 0 && m_MeshManager.GetSkinnedVAO() != 0 &&
		m_ForwardOpaqueCustomSkinnedInfoBuffer != 0 && m_ForwardOpaqueCustomSkinnedCmdBuffer != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardOpaqueCustomSkinnedInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardOpaqueCustomSkinnedCmdBuffer);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardOpaqueCustomSkinnedUploadedCount),
			0
		);
	}

	// Custom transparent (standard)
	if (m_ForwardTransparentCustomStandardUploadedCount > 0 && m_MeshManager.GetStandardVAO() != 0 &&
		m_ForwardTransparentCustomStandardInfoBuffer != 0 && m_ForwardTransparentCustomStandardCmdBuffer != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetStandardVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardTransparentCustomStandardInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomStandardCmdBuffer);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardTransparentCustomStandardUploadedCount),
			0
		);
	}

	// Custom transparent (skinned)
	if (m_ForwardTransparentCustomSkinnedUploadedCount > 0 && m_MeshManager.GetSkinnedVAO() != 0 &&
		m_ForwardTransparentCustomSkinnedInfoBuffer != 0 && m_ForwardTransparentCustomSkinnedCmdBuffer != 0)
	{
		m_MotionBlurMaskShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetSkinnedVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_ForwardTransparentCustomSkinnedInfoBuffer);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_ForwardTransparentCustomSkinnedCmdBuffer);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			GL_UNSIGNED_INT,
			nullptr,
			static_cast<GLsizei>(m_ForwardTransparentCustomSkinnedUploadedCount),
			0
		);
	}

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
	glBindVertexArray(0);
	m_MotionBlurMaskShader->Unbind();

	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
}

#pragma region Shadow Mapping
/**
 * @brief Initializes the shadow map framebuffer object (FBO).
 * Creates and binds the FBO for shadow mapping. If a depth texture array exists, attaches it.
 * Does not validate completeness unless a depth attachment is present.
 * @return True if the FBO was successfully created, false otherwise.
 */
bool Renderer::InitializeShadowMap()
{
	if (m_ShadowMapFBO != 0)
	{
		glDeleteFramebuffers(1, &m_ShadowMapFBO);
		m_ShadowMapFBO = 0;
	}

	glGenFramebuffers(1, &m_ShadowMapFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowMapFBO);

	// If a depth texture already exists attach it. Otherwise we create the FBO now
	// and defer attachment until CreateShadowMap is called.
	if (m_ShadowMapArray != 0)
	{
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0);
	}

	// No color buffer is drawn
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	// Only check completeness if we already have a depth attachment.
	if (m_ShadowMapArray != 0)
	{
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			EE_CORE_ERROR("Shadow map FBO incomplete: {0}", status);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return false;
		}
	}
	else
	{
		EE_CORE_INFO("Initialized shadow FBO (no depth texture attached yet).");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return m_ShadowMapFBO != 0;
}

/**
 * @brief Creates a shadow map texture array for cascaded shadow mapping.
 * Attempts to allocate a depth texture array with as many layers as possible, falling back if allocation fails.
 * Attaches the texture array to the shadow map FBO and sets up bindless texture handle.
 * @return True if the texture array was successfully created and attached, false otherwise.
 */
bool Renderer::CreateShadowMapArray()
{
	// Query hardware limit for texture array layers
	GLint maxLayers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);

	// Clean up previous array if it exists
	if (m_ShadowMapArray)
	{
		if (m_ShadowMapArrayHandle != 0) {
			glMakeTextureHandleNonResidentARB(m_ShadowMapArrayHandle);
			m_ShadowMapArrayHandle = 0;
		}
		glDeleteTextures(1, &m_ShadowMapArray);
		m_ShadowMapArray = 0;
	}

	// Create depth texture array with fallback logic
	glGenTextures(1, &m_ShadowMapArray);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowMapArray);

	bool allocationSuccessful = false;
	unsigned int attemptedLayers = SHADOW_MAX_LAYERS;

	// Try allocating with progressively fewer layers until successful
	while (!allocationSuccessful && attemptedLayers >= 4) {
		glTexImage3D(
			GL_TEXTURE_2D_ARRAY,
			0,
			GL_DEPTH_COMPONENT16,
			SHADOW_MAP_RESOLUTION,
			SHADOW_MAP_RESOLUTION,
			attemptedLayers,
			0,
			GL_DEPTH_COMPONENT,
			GL_FLOAT,
			nullptr
		);

		GLenum error = glGetError();
		if (error == GL_NO_ERROR) {
			allocationSuccessful = true;
			SHADOW_MAX_LAYERS = attemptedLayers;
			EE_CORE_WARN("Successfully allocated {0} shadow map layers", SHADOW_MAX_LAYERS);
		}
		else {
			EE_CORE_WARN("Failed to allocate {0} layers (error: {1}), trying {2}",
				attemptedLayers, error, attemptedLayers / 2);
			attemptedLayers /= 2;
		}
	}

	if (!allocationSuccessful) {
		EE_CORE_ERROR("Failed to allocate shadow map array even with minimum layers");
		glDeleteTextures(1, &m_ShadowMapArray);
		m_ShadowMapArray = 0;
		return false;
	}

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

	// Create FBO if needed
	if (m_ShadowMapFBO == 0)
		glGenFramebuffers(1, &m_ShadowMapFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowMapFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_ShadowMapArray, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		EE_CORE_ERROR("CreateShadowMapArray: FBO incomplete after attaching depth array: {0}", status);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteTextures(1, &m_ShadowMapArray);
		m_ShadowMapArray = 0;
		return false;
	}

	// Bindless handle for array
	m_ShadowMapArrayHandle = glGetTextureHandleARB(m_ShadowMapArray);
	glMakeTextureHandleResidentARB(m_ShadowMapArrayHandle);

	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCheckError();

	return m_ShadowMapArray != 0;
}

/**
 * @brief Computes the eight frustum corners in world space for a given cascade split.
 * Unprojects normalized device coordinates (NDC) to world space using the inverse projection-view matrix.
 * @param invPV Inverse projection-view matrix.
 * @param nearSplit NDC Z value for the near plane of the cascade.
 * @param farSplit NDC Z value for the far plane of the cascade.
 * @return Array of eight world-space frustum corners.
 */
std::array<glm::vec3, 8> Renderer::createCascadeFrustum(const glm::mat4& invPV, float nearSplit, float farSplit) {
	std::array<glm::vec3, 8> frustumCorners;

	// Lambda function to unproject NDC to world space
	auto UnprojectNDC = [&](float ndcX, float ndcY, float ndcZ) -> glm::vec3 {
		glm::vec4 ndc(ndcX, ndcY, ndcZ, 1.0f);
		glm::vec4 world = invPV * ndc;
		if (world.w != 0.0f) world /= world.w;
		return glm::vec3(world);
		};

	const float xs[2] = { -1.0f, 1.0f };
	const float ys[2] = { -1.0f, 1.0f };

	int idx = 0;
	for (int iz = 0; iz < 2; ++iz) {
		float zndc = (iz == 0) ? nearSplit : farSplit;
		for (int iy = 0; iy < 2; ++iy) {
			for (int ix = 0; ix < 2; ++ix) {
				frustumCorners[idx++] = UnprojectNDC(xs[ix], ys[iy], zndc);
			}
		}
	}

	return frustumCorners;
}

/**
 * @brief Tests whether a spotlight's cone intersects a given frustum.
 * Checks if any frustum corner is inside the spotlight cone or if the cone intersects the frustum's AABB.
 * @param lightPos Position of the spotlight.
 * @param spotDir Direction vector of the spotlight.
 * @param outerAngleRad Outer angle of the spotlight cone in radians.
 * @param lightRadius Maximum range of the spotlight.
 * @param frustumCorners Array of eight frustum corners in world space.
 * @return True if the spotlight cone intersects the frustum, false otherwise.
 */
bool Renderer::testSpotlightFrustumIntersection(const glm::vec3& lightPos, const glm::vec3& spotDir,
	float outerAngleRad, float lightRadius,
	const std::array<glm::vec3, 8>& frustumCorners) {

	// Test if any frustum corner is inside spotlight cone. If so, they intersect.
	for (const auto& corner : frustumCorners) {
		glm::vec3 toCorner = corner - lightPos;
		float distanceToCorner = glm::length(toCorner);

		// Check if corner is within spotlight range
		if (distanceToCorner <= lightRadius && distanceToCorner > 0.01f) {
			glm::vec3 dirToCorner = toCorner / distanceToCorner;
			float angleToCorner = std::acos(glm::clamp(glm::dot(spotDir, dirToCorner), -1.0f, 1.0f));

			// Check if corner is within spotlight cone
			if (angleToCorner <= outerAngleRad) {
				return true;
			}
		}
	}

	// Test if spotlight cone intersects frustum planes
	// Create cone vertices at far plane
	const int numSamples = 8;
	float coneRadius = lightRadius * std::tan(outerAngleRad);

	// Create cone basis vectors
	glm::vec3 right = glm::normalize(glm::cross(spotDir, glm::vec3(0.0f, 1.0f, 0.0f)));
	if (glm::length(right) < 0.1f) {
		right = glm::normalize(glm::cross(spotDir, glm::vec3(1.0f, 0.0f, 0.0f)));
	}
	glm::vec3 up = glm::normalize(glm::cross(right, spotDir));

	// Test cone apex and circumference points
	glm::vec3 coneCenter = lightPos + spotDir * lightRadius;

	// Create AABB from frustum corners for quick rejection test
	glm::vec3 frustumMin = frustumCorners[0];
	glm::vec3 frustumMax = frustumCorners[0];
	for (const auto& corner : frustumCorners) {
		frustumMin = glm::min(frustumMin, corner);
		frustumMax = glm::max(frustumMax, corner);
	}
	frustumMin -= glm::vec3(0.1f);
	frustumMax += glm::vec3(0.1f);

	// Test cone apex
	if (lightPos.x >= frustumMin.x && lightPos.x <= frustumMax.x &&
		lightPos.y >= frustumMin.y && lightPos.y <= frustumMax.y &&
		lightPos.z >= frustumMin.z && lightPos.z <= frustumMax.z) {
		return true;
	}

	// Test points around cone circumference
	for (int i = 0; i < numSamples; ++i) {
		float angle = (2.0f * static_cast<float>(M_PI) * static_cast<float>(i)) / static_cast<float>(numSamples);
		glm::vec3 offset = right * (coneRadius * std::cos(angle)) + up * (coneRadius * std::sin(angle));
		glm::vec3 conePoint = coneCenter + offset;

		if (conePoint.x >= frustumMin.x && conePoint.x <= frustumMax.x &&
			conePoint.y >= frustumMin.y && conePoint.y <= frustumMax.y &&
			conePoint.z >= frustumMin.z && conePoint.z <= frustumMax.z) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Calculates the shadow matrix for a spotlight.
 * Computes a view and orthographic projection matrix that tightly fits the cascade frustum in light space.
 * Applies texel snapping and margin adjustments for stable shadows.
 * @param lightPos Position of the spotlight.
 * @param spotDir Direction vector of the spotlight.
 * @param outerAngleRad Outer angle of the spotlight cone in radians.
 * @param lightRadius Maximum range of the spotlight.
*/
glm::mat4 Renderer::calculateSpotlightShadowMatrix(const glm::vec3& lightPos,
	const glm::vec3& spotDir,
	float outerAngleRad,
	float lightRadius) {

	// View matrix
	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	if (glm::abs(glm::dot(spotDir, up)) > 0.999f)
		up = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::mat4 view = glm::lookAt(lightPos, lightPos + spotDir, up);

	// Perspective projection matching cone
	float fov = 2.0f * outerAngleRad;
	float nearPlane = 0.1f;
	float farPlane = lightRadius;
	glm::mat4 proj = glm::perspective(fov, 1.0f, nearPlane, farPlane);

	return proj * view;
}

void Renderer::calculatePointLightShadowMatrices(const glm::vec3& lightPos,
	float lightRadius,
	glm::mat4 outMatrices[6]) {

	float nearPlane = 0.1f;
	float farPlane = glm::max(lightRadius, nearPlane + 0.1f);
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

	outMatrices[0] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f));  // +X
	outMatrices[1] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // -X
	outMatrices[2] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));   // +Y
	outMatrices[3] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)); // -Y
	outMatrices[4] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f));  // +Z
	outMatrices[5] = proj * glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)); // -Z
}

/**
 * @brief Calculates light-space matrices for all shadow-casting lights.
 * Computes cascade splits and shadow matrices for directional and spot lights based on the camera's view and projection.
 * Updates each light's shadow matrix and split depth for use in shadow mapping.
 * @param view Current output-frame view matrix.
 * @param projection Current output-frame projection matrix.
 */
void Renderer::CalculateLightMatrix(const Mtx44& view, const Mtx44& projection)
{
	glm::mat4 glmProj = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);
	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);

	// Inverse PV used for unprojecting NDC to world
	glm::mat4 invPV = glm::inverse(glmProj * glmView);

	auto UnprojectNDC = [&](float ndcX, float ndcY, float ndcZ) -> glm::vec3 {
		glm::vec4 ndc(ndcX, ndcY, ndcZ, 1.0f);
		glm::vec4 world = invPV * ndc;
		if (world.w != 0.0f) world /= world.w;
		return glm::vec3(world);
		};

	// Derive near and far world positions along the view center ray
	glm::vec3 nearPos = UnprojectNDC(0.0f, 0.0f, -1.0f);
	glm::vec3 farPos = UnprojectNDC(0.0f, 0.0f, 1.0f);

	// Compute view-space distances for robust split calculation
	glm::vec4 nearPosView4 = glmView * glm::vec4(nearPos, 1.0f);
	glm::vec4 farPosView4 = glmView * glm::vec4(farPos, 1.0f);
	float nearDist = -nearPosView4.z; // positive distance from camera along view dir
	float farDist = -farPosView4.z;

	if (nearDist <= 1e-6f || farDist <= nearDist) {
		EE_CORE_WARN("calculatedirectionalmatrix: invalid camera near/far ({0},{1})", nearDist, farDist);
		return;
	}

	// Direction along camera center ray (world space)
	glm::vec3 viewDir = glm::normalize(farPos - nearPos);

	const auto& ecs = Ermine::ECS::GetInstance();
	m_ShadowViews.clear();
	if (m_TotalShadowInstances > 0) {
		m_ShadowViews.resize(static_cast<size_t>(m_TotalShadowInstances));
	}

	for (EntityID e : m_ShadowCastingLights) {
		if (!ecs.HasComponent<Light>(e)) continue;
		auto& light = ecs.GetComponent<Light>(e);
		if (!light.castsShadows || light.type == LightType::POINT) continue;
		int baseLayer = light.startOffset;
		if (baseLayer < 0) continue;

		// Use world transform so child lights track parent transforms.
		const glm::mat4 lightWorld = GetEntityWorldMatrix(e);
		const glm::vec3 lightPos = ExtractWorldPosition(lightWorld);
		const glm::vec3 lightForward = ExtractWorldForward(lightWorld);

		if (light.type == LightType::DIRECTIONAL) {
			// DIRECTIONAL LIGHT PROCESSING (existing code)
			if (baseLayer + NUM_CASCADES > m_TotalShadowInstances) {
				EE_CORE_WARN("Not enough layers in shadow map array for directional light entity {0}. Skipping.", e);
				continue;
			}

			// Get light direction
			glm::vec3 lightDir = glm::normalize(-lightForward); // from scene to light

			// Setup up vector
			glm::vec3 up(0.0f, 1.0f, 0.0f);
			if (glm::abs(glm::dot(up, lightDir)) > 0.999f)
				up = glm::vec3(1.0f, 0.0f, 0.0f);

			// Compute cascade splits
			std::vector<float> splits(NUM_CASCADES + 1);
			for (int i = 0; i <= NUM_CASCADES; ++i) {
				float si = static_cast<float>(i) / static_cast<float>(NUM_CASCADES);
				float logSplit = nearDist * std::pow(farDist / nearDist, si);
				float linSplit = nearDist + (farDist - nearDist) * si;
				splits[i] = SHADOW_MAP_ARRAY_LAMBDA * logSplit + (1.0f - SHADOW_MAP_ARRAY_LAMBDA) * linSplit;
			}

			// Get shadow map resolution
			int shadowRes = 1024;
			if (m_ShadowMapArray != 0) {
				glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowMapArray);
				GLint w = 0;
				glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &w);
				if (w > 0) shadowRes = w;
				glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
				glCheckError();
			}

			// Process each cascade
			for (int split = 0; split < NUM_CASCADES; ++split) {
				float splitNearDist = splits[split];
				float splitFarDist = splits[split + 1];

				// Compute world positions for cascade near/far
				glm::vec3 splitNearWorld = nearPos + viewDir * (splitNearDist - nearDist);
				glm::vec3 splitFarWorld = nearPos + viewDir * (splitFarDist - nearDist);

				// Compute depth for depth buffer
				glm::vec4 clipFar = glmProj * glmView * glm::vec4(splitFarWorld, 1.0f);
				float ndcZ_splitFar = (clipFar.w == 0.0f) ? 1.0f : (clipFar.z / clipFar.w);
				//float depthBufferFar = ndcZ_splitFar * 0.5f + 0.5f;

				// Build frustum corners
				std::array<glm::vec3, 8> frustumCornersWorld;
				const float xs[2] = { -1.0f, 1.0f };
				const float ys[2] = { -1.0f, 1.0f };
				float ndcZ_splitNear = (split == 0) ? -1.0f : (glmProj * glmView * glm::vec4(splitNearWorld, 1.0f)).z / (glmProj * glmView * glm::vec4(splitNearWorld, 1.0f)).w;

				int idx = 0;
				for (int iz = 0; iz < 2; ++iz) {
					float zndc = (iz == 0) ? ndcZ_splitNear : ndcZ_splitFar;
					for (int iy = 0; iy < 2; ++iy)
						for (int ix = 0; ix < 2; ++ix)
							frustumCornersWorld[idx++] = UnprojectNDC(xs[ix], ys[iy], zndc);
				}

				// Get AABB and center
				glm::vec3 minCorner = frustumCornersWorld[0];
				glm::vec3 maxCorner = frustumCornersWorld[0];
				for (const auto& c : frustumCornersWorld) {
					minCorner = glm::min(minCorner, c);
					maxCorner = glm::max(maxCorner, c);
				}
				glm::vec3 worldCenter = (minCorner + maxCorner) * 0.5f;

				// Place light far enough away with safety margin
				float diagonal = glm::length(maxCorner - minCorner);
				float lightDistance = glm::max(diagonal * 3.0f, (splitFarDist - splitNearDist) * 2.0f);
				glm::vec3 lightPosCalc = worldCenter - lightDir * lightDistance;

				// Create light view matrix
				glm::mat4 lightView = glm::lookAt(lightPosCalc, worldCenter, up);

				// Transform frustum to light space
				std::vector<glm::vec3> cornersLS;
				cornersLS.reserve(8);
				for (const auto& c : frustumCornersWorld) {
					glm::vec4 p = lightView * glm::vec4(c, 1.0f);
					cornersLS.emplace_back(glm::vec3(p));
				}

				// 2D PCA for optimal shadow map orientation
				glm::vec2 centroid2D(0.0f);
				for (const auto& p : cornersLS)
					centroid2D += glm::vec2(p.x, p.y);
				centroid2D /= static_cast<float>(cornersLS.size());

				// Compute 2x2 covariance matrix
				float cov_xx = 0.0f, cov_xy = 0.0f, cov_yy = 0.0f;
				for (const auto& p : cornersLS) {
					glm::vec2 d = glm::vec2(p.x, p.y) - centroid2D;
					cov_xx += d.x * d.x;
					cov_xy += d.x * d.y;
					cov_yy += d.y * d.y;
				}
				cov_xx /= static_cast<float>(cornersLS.size());
				cov_xy /= static_cast<float>(cornersLS.size());
				cov_yy /= static_cast<float>(cornersLS.size());

				// Find principal axis
				float trace = cov_xx + cov_yy;
				float det = cov_xx * cov_yy - cov_xy * cov_xy;
				float lambda1 = 0.5f * (trace + std::sqrt(trace * trace - 4.0f * det));

				glm::vec2 eigenVec;
				if (std::abs(cov_xy) > 1e-6f) {
					eigenVec = glm::normalize(glm::vec2(lambda1 - cov_yy, cov_xy));
				}
				else {
					eigenVec = (cov_xx > cov_yy) ? glm::vec2(1.0f, 0.0f) : glm::vec2(0.0f, 1.0f);
				}

				// Rotate around Z-axis only
				float angle = std::atan2(eigenVec.y, eigenVec.x);
				glm::mat4 zRotation = glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0.0f, 0.0f, 1.0f));
				glm::mat4 rotatedLightView = zRotation * lightView;

				// Find bounds in rotated space
				glm::vec3 lsMin(FLT_MAX), lsMax(-FLT_MAX);
				for (const auto& c : frustumCornersWorld) {
					glm::vec4 p = rotatedLightView * glm::vec4(c, 1.0f);
					glm::vec3 pp = glm::vec3(p);
					lsMin = glm::min(lsMin, pp);
					lsMax = glm::max(lsMax, pp);
				}

				// Add margins to prevent clipping
				const float xyMargin = 0.01f;
				const float zMargin = 0.01f;
				lsMin -= glm::vec3(xyMargin, xyMargin, zMargin);
				lsMax += glm::vec3(xyMargin, xyMargin, zMargin);

				// Compute ortho extents
				float nearPlane = -lsMax.z;
				float farPlane = -lsMin.z;
				glm::mat4 lightProj = glm::ortho(lsMin.x, lsMax.x, lsMin.y, lsMax.y, nearPlane, farPlane);

				// Store final matrix and split depth
				light.lightSpaceMatrices[split] = lightProj * rotatedLightView;
				light.splitDepths[split] = splitFarDist;

				ShadowViewGPU viewEntry{};
				viewEntry.lightSpaceMatrix = light.lightSpaceMatrices[split];
				viewEntry.data = glm::uvec4(static_cast<unsigned int>(baseLayer + split), 0u, 0u, 0u);
				m_ShadowViews[static_cast<size_t>(baseLayer + split)] = viewEntry;
			}
		}
		else if (light.type == LightType::SPOT) {
			if (baseLayer >= m_TotalShadowInstances) {
				continue;
			}

			// Get spotlight direction and parameters
			glm::vec3 spotDir = lightForward;
			float outerAngleRad = glm::radians(light.outerAngle);

			// Calculate single shadow matrix for the spotlight
			light.lightSpaceMatrices[0] = calculateSpotlightShadowMatrix(
				lightPos, spotDir, outerAngleRad, light.radius);

			ShadowViewGPU viewEntry{};
			viewEntry.lightSpaceMatrix = light.lightSpaceMatrices[0];
			viewEntry.data = glm::uvec4(static_cast<unsigned int>(baseLayer), 0u, 0u, 0u);
			m_ShadowViews[static_cast<size_t>(baseLayer)] = viewEntry;
		}
		else if (light.type == LightType::POINT) {
			if (baseLayer + 6 > m_TotalShadowInstances) {
				continue;
			}

			calculatePointLightShadowMatrices(lightPos, light.radius, light.pointLightMatrices);

			for (int face = 0; face < 6; ++face) {
				ShadowViewGPU viewEntry{};
				viewEntry.lightSpaceMatrix = light.pointLightMatrices[face];
				viewEntry.data = glm::uvec4(static_cast<unsigned int>(baseLayer + face), 0u, 0u, 0u);
				m_ShadowViews[static_cast<size_t>(baseLayer + face)] = viewEntry;
			}
		}
	}

	// Store total layers for instanced shadow rendering
	m_TotalShadowLayers = static_cast<unsigned int>(m_ShadowViews.size());

	// Upload shadow view SSBO
	if (!m_ShadowViewSSBO) {
		glGenBuffers(1, &m_ShadowViewSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ShadowViewSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SHADOW_VIEW_SSBO_BINDING, m_ShadowViewSSBO);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ShadowViewSSBO);
	size_t requiredSize = m_ShadowViews.size() * sizeof(ShadowViewGPU);
	if (requiredSize != m_ShadowViewSSBOCapacity) {
		glBufferData(GL_SHADER_STORAGE_BUFFER, requiredSize,
			m_ShadowViews.empty() ? nullptr : m_ShadowViews.data(),
			GL_DYNAMIC_DRAW);
		m_ShadowViewSSBOCapacity = requiredSize;
	} else if (!m_ShadowViews.empty()) {
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, requiredSize, m_ShadowViews.data());
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Renderer::SyncShadowViewsForOutputFrame(const Mtx44& view, const Mtx44& projection)
{
	BuildVisibleLightSet(view, projection);
	CalculateLightMatrix(view, projection);
	UploadLightsSSBOFromPreparedState();
}

/**
 * @brief Renders shadow map using indirect rendering and instancing across all shadow layers.
 *
 * Uses indirect rendering (glMultiDrawElementsIndirect) to render all meshes with a single draw call.
 * Reads pre-skinned positions from geometry pass (binding 8) to eliminate redundant bone calculations.
 * Each mesh is instanced across all shadow-casting light cascades for optimal performance.
 *
 * @note Requires geometry pass to have completed and written pre-skinned positions to SSBO.
 * @note Memory barrier is issued in RenderShadowPass() before calling this function.
 */
void Renderer::RenderShadowMapInstanced()
{
	// Validate resources
	if (!m_ShadowMapFBO || !m_ShadowMapArray || !m_ShadowMapInstancedShader)
	{
		EE_CORE_WARN("RenderShadowMapInstanced: missing shadow FBO/texture/shader");
		return;
	}
	if (m_TotalShadowInstances <= 0 || m_ShadowViews.empty() || !m_ShadowViewSSBO)
	{
		return;
	}

	// Query current viewport so we can restore it later
	GLint prevViewport[4];
	glGetIntegerv(GL_VIEWPORT, prevViewport);

	// Query shadow map resolution from the texture
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowMapArray);
	GLint shadowWidth = 1024, shadowHeight = 1024; // fallback
	glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &shadowWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &shadowHeight);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

	// Bind shadow FBO and set viewport to shadow resolution
	glBindFramebuffer(GL_FRAMEBUFFER, m_ShadowMapFBO);
	glViewport(0, 0, shadowWidth, shadowHeight);

	// Clear depth
	glClearDepth(1.0f);
	glClear(GL_DEPTH_BUFFER_BIT);

	// Setup render state for depth-only pass
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE); // Ensure depth writes are enabled (may be disabled from geometry pass)
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// Bind shadow shader
	m_ShadowMapInstancedShader->Bind();

	// Bind shadow view SSBO for per-layer matrices and layer indices
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SHADOW_VIEW_SSBO_BINDING, m_ShadowViewSSBO);

	// ========== RENDER STANDARD SHADOW MESHES ==========
	// Shadow buffers contain ALL geometry with castsShadows=true with correct instanceCount
	// Written in CompileDrawData() - just bind and draw
	if (!m_ShadowStandardCommands.empty() && m_MeshManager.GetStandardShadowVAO() != 0) {
		m_ShadowMapInstancedShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetStandardShadowVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ShadowStandardDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ShadowStandardDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
			static_cast<GLsizei>(m_ShadowStandardCommands.size()), 0);
		GPUProfiler::TrackDrawCall(m_GeometryStandardVertexCount, m_GeometryStandardIndexCount);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// ========== RENDER SKINNED SHADOW MESHES ==========
	if (!m_ShadowSkinnedCommands.empty() && m_MeshManager.GetSkinnedShadowVAO() != 0) {
		m_ShadowMapInstancedShader->SetUniform1ui("baseDrawID", 0);
		glBindVertexArray(m_MeshManager.GetSkinnedShadowVAO());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_ShadowSkinnedDrawInfoBuffer.GetBufferID());
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_ShadowSkinnedDrawCommandBuffer.GetBufferID());
		glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
			static_cast<GLsizei>(m_ShadowSkinnedCommands.size()), 0);
		GPUProfiler::TrackDrawCall(m_GeometrySkinnedVertexCount, m_GeometrySkinnedIndexCount);
		glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
		glBindVertexArray(0);
	}

	// Unbind framebuffer and restore viewport
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
	// Ensure shadow textures ready
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

	glCheckError();
}

/**
 * @brief Executes the full shadow pass for all shadow-casting lights.
 * Calculates cascade splits and shadow matrices for directional and spot lights based on the camera's view and projection.
 * Updates each light's shadow matrix and split depth for use in shadow mapping.
 */
void Renderer::RenderShadowPass()
{
	// Render shadows using instanced rendering
	RenderShadowMapInstanced();
}

#pragma endregion

/**
 * @brief Creates the picking buffer for entity selection.
 * @param width Buffer width.
 * @param height Buffer height.
 */
void Renderer::CreatePickingBuffer(const int& width, const int& height)
{
	if (m_PickingBuffer)
	{
		glDeleteFramebuffers(1, &m_PickingBuffer->FBO);
		glDeleteTextures(1, &m_PickingBuffer->ColorID);
		glDeleteRenderbuffers(1, &m_PickingBuffer->Depth);
		m_PickingBuffer.reset();
	}

	auto pb = std::make_shared<PickingBuffer>();
	pb->width = width;
	pb->height = height;

	glGenFramebuffers(1, &pb->FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, pb->FBO);

	// Color: 32-bit unsigned int
	glGenTextures(1, &pb->ColorID);
	glBindTexture(GL_TEXTURE_2D, pb->ColorID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pb->ColorID, 0);

	// Depth
	glGenRenderbuffers(1, &pb->Depth);
	glBindRenderbuffer(GL_RENDERBUFFER, pb->Depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pb->Depth);

	GLenum db[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, db);

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		EE_CORE_ERROR("Picking FBO incomplete: {0}", status);
		assert(false && "Picking FBO failed");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	m_PickingBuffer = pb;
	glCheckError();
}

/**
 * @brief Resizes the picking buffer.
 * @param width New width.
 * @param height New height.
 */
void Renderer::ResizePickingBuffer(const int& width, const int& height)
{
	if (!m_PickingBuffer)
	{
		CreatePickingBuffer(width, height);
		return;
	}
	if (m_PickingBuffer->width == width && m_PickingBuffer->height == height)
		return;

	glBindTexture(GL_TEXTURE_2D, m_PickingBuffer->ColorID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

	glBindRenderbuffer(GL_RENDERBUFFER, m_PickingBuffer->Depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, m_PickingBuffer->FBO);
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		EE_CORE_ERROR("Picking FBO not complete after resize! Status: {0}", status);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	m_PickingBuffer->width = width;
	m_PickingBuffer->height = height;
	glCheckError();
}

/**
 * @brief Renders the picking pass for entity selection.
 * @param view View matrix.
 * @param projection Projection matrix.
 */
void Renderer::RenderPickingPass(const Mtx44& view, const Mtx44& projection)
{
#if defined(EE_EDITOR)
	if (!m_PickingBuffer || !m_PickingIndirectShader || !m_PickingIndirectSkinnedShader)
		return;

	// 1) Prime depth: copy scene depth into picking FBO (source depends on path)
	//if (m_UseDeferredRendering && m_GBuffer)
	//{
	//	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBuffer->FBO);
	//}
	//else if (m_OffscreenBuffer)
	//{
	//	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_OffscreenBuffer->FBO);
	//}
	//else
	//{
	//	return;
	//}

	//glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_PickingBuffer->FBO);
	//glBlitFramebuffer(0, 0, m_PickingBuffer->width, m_PickingBuffer->height,
	//	0, 0, m_PickingBuffer->width, m_PickingBuffer->height,
	//	GL_DEPTH_BUFFER_BIT, GL_NEAREST);

	// 2) Render IDs using indirect rendering
	glBindFramebuffer(GL_FRAMEBUFFER, m_PickingBuffer->FBO);
	glViewport(0, 0, m_PickingBuffer->width, m_PickingBuffer->height);

	// Clear IDs to 0
	GLuint clearVal[1] = { 0u };
	glClearBufferuiv(GL_COLOR, 0, clearVal);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE); // Enable depth writes so first fragment wins (prevents double-picking)
	glDisable(GL_BLEND);

	// Build view-projection matrix
	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);
	glm::mat4 glmProjection = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);
	glm::mat4 vp = glmProjection * glmView;

	// ========== RENDER STANDARD MESHES (ALL visible geometry - opaque + transparent) ==========
	if (!m_PickingStandardCommands.empty())
	{
		m_PickingIndirectShader->Bind();
		m_PickingIndirectShader->SetUniformMatrix4fv("u_ViewProjection", vp);

		GLuint standardVAO = m_MeshManager.GetStandardVAO();
		if (standardVAO != 0)
		{
			glBindVertexArray(standardVAO);

			// Render all standard meshes from picking buffer (opaque + transparent)
			m_PickingIndirectShader->SetUniform1ui("baseDrawID", 0);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_PickingStandardDrawInfoBuffer.GetBufferID());
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_PickingStandardDrawCommandBuffer.GetBufferID());
			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				GL_UNSIGNED_INT,
				nullptr,
				static_cast<GLsizei>(m_PickingStandardCommands.size()),
				0
			);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

			glBindVertexArray(0);
		}

		m_PickingIndirectShader->Unbind();
	}

	// ========== RENDER SKINNED MESHES (ALL visible geometry - opaque + transparent) ==========
	if (!m_PickingSkinnedCommands.empty())
	{
		m_PickingIndirectSkinnedShader->Bind();
		m_PickingIndirectSkinnedShader->SetUniformMatrix4fv("u_ViewProjection", vp);

		GLuint skinnedVAO = m_MeshManager.GetSkinnedVAO();
		if (skinnedVAO != 0)
		{
			glBindVertexArray(skinnedVAO);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SKELETAL_SSBO_BINDING, m_MeshManager.m_SkeletalSSBO.GetBufferID());

			// Render all skinned meshes from picking buffer (opaque + transparent)
			m_PickingIndirectSkinnedShader->SetUniform1ui("baseDrawID", 0);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, DRAW_INFO_SSBO_BINDING, m_MeshManager.m_PickingSkinnedDrawInfoBuffer.GetBufferID());
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_MeshManager.m_PickingSkinnedDrawCommandBuffer.GetBufferID());
			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				GL_UNSIGNED_INT,
				nullptr,
				static_cast<GLsizei>(m_PickingSkinnedCommands.size()),
				0
			);
			glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

			glBindVertexArray(0);
		}

		m_PickingIndirectSkinnedShader->Unbind();
	}

	// Custom-shader meshes are already included in m_PickingStandardCommands /
	// m_PickingSkinnedCommands during draw-data compilation. Re-drawing them here is redundant
	// and can diverge from the generic picking buffers.

	// Restore default framebuffer after the picking pass.
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glCheckError();

#endif
}

/**
 * @brief Gets the world transform matrix for an entity using GlobalTransform component
 * @param entity The entity to get the world matrix for
 * @return glm::mat4 The world transform matrix
 */
glm::mat4 Renderer::GetEntityWorldMatrix(EntityID entity) const
{
	const auto& ecs = Ermine::ECS::GetInstance();

	if (ecs.HasComponent<GlobalTransform>(entity)) {
		auto& globalTransform = ecs.GetComponent<GlobalTransform>(entity);

		// CONVERT your custom matrix to GLM format
		Mtx44& m = globalTransform.worldMatrix;
		return glm::mat4(
			m.m00, m.m10, m.m20, m.m30,  // Column 0
			m.m01, m.m11, m.m21, m.m31,  // Column 1  
			m.m02, m.m12, m.m22, m.m32,  // Column 2
			m.m03, m.m13, m.m23, m.m33   // Column 3
		);
	}

	// Fallback to local transform using GLM
	if (ecs.HasComponent<Transform>(entity)) {
		auto& trans = ecs.GetComponent<Transform>(entity);
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(trans.position.x, trans.position.y, trans.position.z));
		glm::quat rotQuat(trans.rotation.w, trans.rotation.x, trans.rotation.y, trans.rotation.z);
		rotQuat = glm::normalize(rotQuat);
		model *= glm::mat4_cast(rotQuat);
		model = glm::scale(model, glm::vec3(trans.scale.x, trans.scale.y, trans.scale.z));
		return model;
	}

	return glm::mat4(1.0f);
}

void Renderer::SyncToGlobalGraphics()
{
	m_GlobalGraphics.ambientColor = Ermine::Vec3(m_AmbientColor.x, m_AmbientColor.y, m_AmbientColor.z);
	m_GlobalGraphics.ambientIntensity = m_AmbientIntensity;

	m_GlobalGraphics.ssaoEnabled = m_SSAOEnabled;
	m_GlobalGraphics.ssaoSamples = m_SSAOSamples;
	m_GlobalGraphics.ssaoRadius = m_SSAORadius;
	m_GlobalGraphics.ssaoBias = m_SSAOBias;
	m_GlobalGraphics.ssaoIntensity = m_SSAOIntensity;
	m_GlobalGraphics.ssaoFadeout = m_SSAOFadeout;
	m_GlobalGraphics.ssaoMaxDistance = m_SSAOMaxDistance;

	m_GlobalGraphics.fogEnabled = m_FogEnabled;
	m_GlobalGraphics.fogMode = m_FogMode;
	m_GlobalGraphics.fogColor = Ermine::Vec3(m_FogColor.x, m_FogColor.y, m_FogColor.z);
	m_GlobalGraphics.fogDensity = m_FogDensity;
	m_GlobalGraphics.fogStart = m_FogStart;
	m_GlobalGraphics.fogEnd = m_FogEnd;
	m_GlobalGraphics.fogHeightCoefficient = m_FogHeightCoefficient;
	m_GlobalGraphics.fogHeightFalloff = m_FogHeightFalloff;

	m_GlobalGraphics.vignetteEnabled = m_VignetteEnabled;
	m_GlobalGraphics.fxaaEnabled = m_FXAAEnabled;
	m_GlobalGraphics.toneMappingEnabled = m_ToneMappingEnabled;
	m_GlobalGraphics.gammaCorrectionEnabled = m_GammaCorrectionEnabled;
	m_GlobalGraphics.bloomEnabled = m_BloomEnabled;
	m_GlobalGraphics.skyboxIsHDR = m_SkyBoxisHDR;
	m_GlobalGraphics.showSkybox = m_ShowSkybox;

	m_GlobalGraphics.exposure = m_Exposure;
	m_GlobalGraphics.contrast = m_Contrast;
	m_GlobalGraphics.saturation = m_Saturation;
	m_GlobalGraphics.gamma = m_Gamma;
	m_GlobalGraphics.vignetteIntensity = m_VignetteIntensity;
	m_GlobalGraphics.vignetteRadius = m_VignetteRadius;
	m_GlobalGraphics.vignetteCoverage = m_VignetteCoverage;
	m_GlobalGraphics.vignetteFalloff = m_VignetteFalloff;
	m_GlobalGraphics.vignetteMapStrength = m_VignetteMapStrength;
	m_GlobalGraphics.vignetteMapRGBModifier = Ermine::Vec3(
		m_VignetteMapRGBModifier.r,
		m_VignetteMapRGBModifier.g,
		m_VignetteMapRGBModifier.b
	);
	m_GlobalGraphics.vignetteMapPath = m_VignetteMapPath;
	m_GlobalGraphics.bloomStrength = m_BloomStrength;

	m_GlobalGraphics.filmGrainEnabled = m_FilmGrainEnabled;
	m_GlobalGraphics.grainIntensity = m_GrainIntensity;
	m_GlobalGraphics.grainScale = m_GrainScale;
	m_GlobalGraphics.chromaticAberrationEnabled = m_ChromaticAberrationEnabled;
	m_GlobalGraphics.chromaticAmount = m_ChromaticAmount;
	m_GlobalGraphics.radialBlurEnabled = m_RadialBlurEnabled;
	m_GlobalGraphics.radialBlurStrength = m_RadialBlurStrength;
	m_GlobalGraphics.radialBlurSamples = m_RadialBlurSamples;
	m_GlobalGraphics.radialBlurCenterX = m_RadialBlurCenter.x;
	m_GlobalGraphics.radialBlurCenterY = m_RadialBlurCenter.y;

	m_GlobalGraphics.fxaaSpanMax = m_FXAASpanMax;
	m_GlobalGraphics.fxaaReduceMin = m_FXAAReduceMin;
	m_GlobalGraphics.fxaaReduceMul = m_FXAAReduceMul;

	m_GlobalGraphics.bloomThreshold = m_BloomThreshold;
	m_GlobalGraphics.bloomIntensity = m_BloomIntensity;
	m_GlobalGraphics.bloomRadius = m_BloomRadius;

	m_GlobalGraphics.motionBlurEnabled = m_MotionBlurEnabled;
	m_GlobalGraphics.motionBlurStrength = m_MotionBlurStrength;
	m_GlobalGraphics.motionBlurSamples = m_MotionBlurSamples;

	m_GlobalGraphics.spotlightRaysEnabled = m_SpotlightRaysEnabled;
	m_GlobalGraphics.spotlightRayIntensity = m_SpotlightRayIntensity;
	m_GlobalGraphics.spotlightRayFalloff = m_SpotlightRayFalloff;
}

void Renderer::ApplyFromGlobalGraphics()
{
	m_AmbientColor = glm::vec3(
		m_GlobalGraphics.ambientColor.x,
		m_GlobalGraphics.ambientColor.y,
		m_GlobalGraphics.ambientColor.z
	);
	m_AmbientIntensity = m_GlobalGraphics.ambientIntensity;

	m_SSAOEnabled = m_GlobalGraphics.ssaoEnabled;
	m_SSAOSamples = m_GlobalGraphics.ssaoSamples;
	m_SSAORadius = m_GlobalGraphics.ssaoRadius;
	m_SSAOBias = m_GlobalGraphics.ssaoBias;
	m_SSAOIntensity = m_GlobalGraphics.ssaoIntensity;
	m_SSAOFadeout = m_GlobalGraphics.ssaoFadeout;
	m_SSAOMaxDistance = m_GlobalGraphics.ssaoMaxDistance;

	m_FogEnabled = m_GlobalGraphics.fogEnabled;
	m_FogMode = m_GlobalGraphics.fogMode;
	m_FogColor = glm::vec3(
		m_GlobalGraphics.fogColor.x,
		m_GlobalGraphics.fogColor.y,
		m_GlobalGraphics.fogColor.z
	);
	m_FogDensity = m_GlobalGraphics.fogDensity;
	m_FogStart = m_GlobalGraphics.fogStart;
	m_FogEnd = m_GlobalGraphics.fogEnd;
	m_FogHeightCoefficient = m_GlobalGraphics.fogHeightCoefficient;
	m_FogHeightFalloff = m_GlobalGraphics.fogHeightFalloff;

	m_VignetteEnabled = m_GlobalGraphics.vignetteEnabled;
	m_FXAAEnabled = m_GlobalGraphics.fxaaEnabled;
	m_ToneMappingEnabled = m_GlobalGraphics.toneMappingEnabled;
	m_GammaCorrectionEnabled = m_GlobalGraphics.gammaCorrectionEnabled;
	m_BloomEnabled = m_GlobalGraphics.bloomEnabled;
	m_SkyBoxisHDR = m_GlobalGraphics.skyboxIsHDR;
	m_ShowSkybox = m_GlobalGraphics.showSkybox;

	m_Exposure = m_GlobalGraphics.exposure;
	m_Contrast = m_GlobalGraphics.contrast;
	m_Saturation = m_GlobalGraphics.saturation;
	m_Gamma = m_GlobalGraphics.gamma;
	m_VignetteIntensity = m_GlobalGraphics.vignetteIntensity;
	m_VignetteRadius = m_GlobalGraphics.vignetteRadius;
	m_VignetteCoverage = m_GlobalGraphics.vignetteCoverage;
	m_VignetteFalloff = m_GlobalGraphics.vignetteFalloff;
	m_VignetteMapStrength = m_GlobalGraphics.vignetteMapStrength;
	m_VignetteMapRGBModifier = glm::vec3(
		m_GlobalGraphics.vignetteMapRGBModifier.x,
		m_GlobalGraphics.vignetteMapRGBModifier.y,
		m_GlobalGraphics.vignetteMapRGBModifier.z
	);
	m_VignetteMapPath = m_GlobalGraphics.vignetteMapPath;
	if (!m_VignetteMapPath.empty()) {
		m_VignetteMapTexture = AssetManager::GetInstance().LoadTexture(m_VignetteMapPath);
		if (!m_VignetteMapTexture || !m_VignetteMapTexture->IsValid()) {
			EE_CORE_WARN("Failed to load vignette map texture: {}", m_VignetteMapPath);
			m_VignetteMapTexture.reset();
		}
	}
	else {
		m_VignetteMapTexture.reset();
	}
	m_BloomStrength = m_GlobalGraphics.bloomStrength;

	m_FilmGrainEnabled = m_GlobalGraphics.filmGrainEnabled;
	m_GrainIntensity = m_GlobalGraphics.grainIntensity;
	m_GrainScale = m_GlobalGraphics.grainScale;
	m_ChromaticAberrationEnabled = m_GlobalGraphics.chromaticAberrationEnabled;
	m_ChromaticAmount = m_GlobalGraphics.chromaticAmount;
	m_RadialBlurEnabled = m_GlobalGraphics.radialBlurEnabled;
	m_RadialBlurStrength = std::clamp(m_GlobalGraphics.radialBlurStrength, 0.0f, 0.35f);
	m_RadialBlurSamples = std::clamp(m_GlobalGraphics.radialBlurSamples, 4, 24);
	m_RadialBlurCenter = glm::vec2(
		std::clamp(m_GlobalGraphics.radialBlurCenterX, 0.0f, 1.0f),
		std::clamp(m_GlobalGraphics.radialBlurCenterY, 0.0f, 1.0f)
	);

	m_FXAASpanMax = m_GlobalGraphics.fxaaSpanMax;
	m_FXAAReduceMin = m_GlobalGraphics.fxaaReduceMin;
	m_FXAAReduceMul = m_GlobalGraphics.fxaaReduceMul;

	m_BloomThreshold = m_GlobalGraphics.bloomThreshold;
	m_BloomIntensity = m_GlobalGraphics.bloomIntensity;
	m_BloomRadius = m_GlobalGraphics.bloomRadius;

	m_MotionBlurEnabled = m_GlobalGraphics.motionBlurEnabled;
	m_MotionBlurStrength = m_GlobalGraphics.motionBlurStrength;
	m_MotionBlurSamples = m_GlobalGraphics.motionBlurSamples;

	m_SpotlightRaysEnabled = m_GlobalGraphics.spotlightRaysEnabled;
	m_SpotlightRayIntensity = m_GlobalGraphics.spotlightRayIntensity;
	m_SpotlightRayFalloff = m_GlobalGraphics.spotlightRayFalloff;
}

/**
 * @brief Picks the entity at the given screen coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param view View matrix.
 * @param projection Projection matrix.
 * @return Pair of (success, entity ID).
 */
std::pair<bool, Ermine::EntityID> Renderer::PickEntityAt(const int& x, const int& y, const Mtx44& view, const Mtx44& projection)
{

#if defined(EE_EDITOR)
	if (!m_PickingBuffer)
		return { false, EntityID{} };

	RenderPickingPass(view, projection);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_PickingBuffer->FBO);
	glReadBuffer(GL_COLOR_ATTACHMENT0); // color read buffer doesn't matter since we only read stencil
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	uint32_t id = 0u;
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &id);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	if (id == 0u) return { false, EntityID{} };

	EntityID picked = id - 1u;
	if (!ECS::GetInstance().IsEntityValid(picked))
		return { false , EntityID{} };

	return { true, picked };
#else
	return { false, EntityID{} };
#endif
}

/**
 * @brief Handle window resize events to adjust buffers and viewports
 * @param width New window width
 * @param height New window height
 */
void Renderer::OnWindowResize(const int& width, const int& height)
{
	if (m_UseDeferredRendering)
		ResizeGBuffer(width, height);
}

/**
 * @brief Compiles all materials from entities into a single SSBO.
 * This collects material data, uploads to GPU, and assigns indices.
 */
void Renderer::CheckMaterialUpdates()
{
	// Check if any materials have been modified (e.g., via ImGui)
	const auto& ecs = Ermine::ECS::GetInstance();

	for (auto entity : m_MaterialSystem->m_Entities)
	{
		if (!ecs.HasComponent<Ermine::Material>(entity)) continue;

		auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
		graphics::Material* material = materialComponent.GetMaterial();

		if (material && material->IsDirty())
		{
			// Material has been modified, trigger recompilation
			m_MaterialsDirty = true;

			// Invalidate draw data cache since material property changes affect routing
			m_DrawDataNeedsFullRebuild = true;
			return;
		}
	}
}

void Renderer::CompileMaterials()
{
	if (!m_MaterialsDirty) return;

	EE_CORE_INFO("Compiling materials for GPU upload...");

	// Clear previous compiled data
	m_CompiledMaterials.clear();
	m_EntityMaterialIndices.clear();

	// Map to track unique materials and avoid duplicates
	std::map<const graphics::Material*, uint32_t> materialToIndex;

	const auto& ecs = Ermine::ECS::GetInstance();

	// First pass: Collect unique materials
	for (auto entity : m_MaterialSystem->m_Entities)
	{
		if (!ecs.HasComponent<Ermine::Material>(entity)) continue;

		auto& materialComponent = ecs.GetComponent<Ermine::Material>(entity);
		graphics::Material* material = materialComponent.GetMaterial();

		if (!material) {
			EE_CORE_WARN("Entity {0} has null material", entity);
			continue;
		}

		// Resolve custom shader ownership deterministically before routing.
		auto& assets = AssetManager::GetInstance();
		std::string resolvedCustomFragmentShader;
		if (materialComponent.materialGuid.IsValid()) {
			if (const std::string* frag = assets.GetMaterialCustomFragmentShader(materialComponent.materialGuid)) {
				resolvedCustomFragmentShader = *frag;
			}
			materialComponent.customFragmentShader = resolvedCustomFragmentShader;
		}
		else {
			resolvedCustomFragmentShader = materialComponent.customFragmentShader;
		}

		if (resolvedCustomFragmentShader.empty()) {
			if (HasCustomShader(material)) {
				material->SetShader(nullptr);
			}
		}
		else {
			const std::string vertexPath = ResolveCustomVertexShaderPath(resolvedCustomFragmentShader);
			auto shader = assets.LoadShader(vertexPath, resolvedCustomFragmentShader);
			if (shader && shader->IsValid()) {
				if (material->GetShader() != shader) {
					material->SetShader(shader);
				}
			}
			else {
				EE_CORE_WARN("Failed to load custom shader pair: vertex='{}', fragment='{}'", vertexPath, resolvedCustomFragmentShader);
				if (HasCustomShader(material)) {
					material->SetShader(nullptr);
				}
			}
		}
		if (auto param = material->GetParameter("materialCastsShadows"); !param || param->boolValue != materialComponent.cacheCastsShadows) {
			material->SetBool("materialCastsShadows", materialComponent.cacheCastsShadows);
		}

		// Check if we've already seen this material
		if (materialToIndex.find(material) != materialToIndex.end()) {
			// Reuse existing index
			m_EntityMaterialIndices[entity] = materialToIndex[material];
			continue;
		}

		// New material - register textures and assign indices
		uint32_t materialIndex = static_cast<uint32_t>(m_CompiledMaterials.size());

		// Register all textures used by this material
		const std::vector<std::string> textureTypes = {
			"materialAlbedoMap",
			"materialNormalMap",
			"materialRoughnessMap",
			"materialMetallicMap",
			"materialAoMap",
			"materialEmissiveMap"
		};

		for (const auto& texName : textureTypes)
		{
			if (auto texture = material->GetTexture(texName))
			{
				int textureIndex = RegisterTexture(texture);
				if (textureIndex >= 0)
				{
					material->SetTextureArrayIndex(texName, textureIndex);
				}
			}
		}

		// Add material data to compiled list
		m_CompiledMaterials.push_back(material->GetSSBOData());

		// Store the mapping
		materialToIndex[material] = materialIndex;
		m_EntityMaterialIndices[entity] = materialIndex;

		// Update the material with its index
		material->SetMaterialIndex(static_cast<int>(materialIndex));
	}

	// Build the texture array
	BuildTextureArray();

	// Upload all materials to GPU
	UploadMaterialsToGPU();

	m_MaterialsDirty = false;

	EE_CORE_INFO("Compiled {0} unique materials for {1} entities",
		m_CompiledMaterials.size(), m_EntityMaterialIndices.size());
}


void Renderer::CreateOutlineMaskBuffer(const int& width, const int& height)
{
	DestroyOutlineMaskBuffer();

	glGenFramebuffers(1, &m_OutlineMaskFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, m_OutlineMaskFBO);

	// R8 is enough: we only need 0/1 coverage
	glGenTextures(1, &m_OutlineMaskTexture);
	glBindTexture(GL_TEXTURE_2D, m_OutlineMaskTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_OutlineMaskTexture, 0);

	//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_GBuffer->DepthTexture, 0);

	GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, drawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		EE_CORE_ERROR("OutlineMask FBO incomplete");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Update viewport sizes
	m_ViewportWidth = width;
	m_ViewportHeight = height;
}

void Renderer::DestroyOutlineMaskBuffer()
{
	if (m_OutlineMaskTexture != 0)
	{
		glDeleteTextures(1, &m_OutlineMaskTexture);
		m_OutlineMaskTexture = 0;
	}
	if (m_OutlineMaskFBO != 0)
	{
		glDeleteFramebuffers(1, &m_OutlineMaskFBO);
		m_OutlineMaskFBO = 0;
	}
}

void Renderer::RenderOutlineMaskPass(const Mtx44& view, const Mtx44& projection)
{
	if (!m_OutlineEnabled)
		return;

	const auto& selectedSet = Ermine::editor::Selection::All();
	EntityID primary = 0;

	auto sceneMgr = SceneManager::GetInstance();
	primary = sceneMgr.GetActiveScene()->GetSelectedEntity();

	glBindFramebuffer(GL_FRAMEBUFFER, m_OutlineMaskFBO);
	glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

	// Not selected
	glDisable(GL_BLEND);
	glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	//glEnable(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	auto isSelected = [&](EntityID id) -> bool
	{
		if (id == 0) return false;
		if (!selectedSet.empty())
			return selectedSet.contains(id);
		return id == primary;
	};

	glm::mat4 glmView = glm::mat4(
		view.m00, view.m01, view.m02, view.m03,
		view.m10, view.m11, view.m12, view.m13,
		view.m20, view.m21, view.m22, view.m23,
		view.m30, view.m31, view.m32, view.m33
	);
	glm::mat4 glmProjection = glm::mat4(
		projection.m00, projection.m01, projection.m02, projection.m03,
		projection.m10, projection.m11, projection.m12, projection.m13,
		projection.m20, projection.m21, projection.m22, projection.m23,
		projection.m30, projection.m31, projection.m32, projection.m33
	);
	glm::mat4 vp = glmProjection * glmView;

	m_MeshManager.RenderOutlineMaskIndirect(glmView, glmProjection, [&](const EntityID id) { return editor::Selection::IsSelected(id); }, *m_OutlineMaskIndirectShader, *m_OutlineMaskIndirectSkinnedShader);

	// Restore
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief Uploads all compiled materials to the GPU SSBO in one batch.
 */
void Renderer::UploadMaterialsToGPU()
{
	if (m_CompiledMaterials.empty()) {
		EE_CORE_WARN("No materials to upload to GPU");
		return;
	}

	// Create Material SSBO if it doesn't exist
	if (!m_MaterialSSBO)
	{
		glGenBuffers(1, &m_MaterialSSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MATERIAL_SSBO_BINDING, m_MaterialSSBO);
		EE_CORE_INFO("Created MaterialSSBO");
	}
	else
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_MaterialSSBO);
	}

	// Keep SSBO binding point 3 valid for all passes and custom shaders.
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, MATERIAL_SSBO_BINDING, m_MaterialSSBO);

	// Calculate total size needed
	const size_t materialSize = sizeof(graphics::MaterialSSBO);
	const size_t requiredSize = m_CompiledMaterials.size();
	const size_t uploadBytes = requiredSize * materialSize;

	// Only reallocate if size changed (prevents both orphaning and stale data)
	if (requiredSize != m_MaterialSSBOCapacity)
	{
		// Size changed - must reallocate to prevent stale data
		glBufferData(GL_SHADER_STORAGE_BUFFER, uploadBytes, m_CompiledMaterials.data(), GL_DYNAMIC_DRAW);
		m_MaterialSSBOCapacity = requiredSize;

		// Check for allocation errors
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			EE_CORE_ERROR("Failed to allocate MaterialSSBO: {0} bytes, error: {1}",
				uploadBytes, error);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			return;
		}
	}
	else
	{
		// Size unchanged - safe to use glBufferSubData (avoids orphaning)
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, uploadBytes, m_CompiledMaterials.data());
	}

	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		EE_CORE_ERROR("Failed to upload materials to GPU, error: {0}", error);
	}
	else
	{
		EE_CORE_INFO("Uploaded {0} materials ({1} bytes) to GPU",
			m_CompiledMaterials.size(), uploadBytes);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glCheckError();
}

/**
 * @brief Registers a texture in the global texture array.
 * @param texture Shared pointer to the texture.
 * @return The index of the texture in the array, or -1 if registration failed.
 */
int Renderer::RegisterTexture(std::shared_ptr<Texture> texture)
{
	if (!texture || !texture->IsValid())
	{
		return -1;
	}

	GLuint textureID = texture->GetRendererID();
	std::string filePath = texture->GetFilePath();

	// Check if texture is already registered by ID
	auto idIt = m_TextureIDToIndex.find(textureID);
	if (idIt != m_TextureIDToIndex.end())
	{
		return idIt->second;
	}

	// Check if texture is already registered by path
	auto pathIt = m_TexturePathToIndex.find(filePath);
	if (pathIt != m_TexturePathToIndex.end())
	{
		return pathIt->second;
	}

	// Register new texture
	int index = static_cast<int>(m_TextureArray.size());
	m_TextureArray.push_back(textureID);
	m_TextureIDToIndex[textureID] = index;
	if (!filePath.empty())
	{
		m_TexturePathToIndex[filePath] = index;
	}

	m_TextureArrayDirty = true;

	EE_CORE_INFO("Registered texture '{0}' at index {1}", filePath, index);
	return index;
}

/**
 * @brief Gets the texture array index for a given texture ID.
 * @param textureID The OpenGL texture ID.
 * @return The array index, or -1 if not found.
 */
int Renderer::GetTextureArrayIndex(GLuint textureID) const
{
	auto it = m_TextureIDToIndex.find(textureID);
	return it != m_TextureIDToIndex.end() ? it->second : -1;
}

/**
 * @brief Builds the bindless texture array SSBO.
 * This should be called after all textures are registered and before rendering.
 */
void Renderer::BuildTextureArray()
{
	if (!m_TextureArrayDirty || m_TextureArray.empty())
	{
		return;
	}

	EE_CORE_INFO("Building bindless texture array with {0} textures...", m_TextureArray.size());

	// Create texture array SSBO if it doesn't exist
	if (!m_TextureArraySSBO)
	{
		glGenBuffers(1, &m_TextureArraySSBO);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TextureArraySSBO);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TEXTURE_SSBO_BINDING, m_TextureArraySSBO);
		EE_CORE_INFO("Created Texture Array SSBO at binding point {0}", TEXTURE_SSBO_BINDING);
	}
	else
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_TextureArraySSBO);
	}

	// Keep SSBO binding point 5 valid for all passes and custom shaders.
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, TEXTURE_SSBO_BINDING, m_TextureArraySSBO);

	// Create bindless texture handles for all textures
	std::vector<GLuint64> textureHandles;
	textureHandles.reserve(m_TextureArray.size());

	for (GLuint textureID : m_TextureArray)
	{
		// Get bindless handle for this texture
		GLuint64 handle = glGetTextureHandleARB(textureID);
		if (handle == 0)
		{
			EE_CORE_ERROR("Failed to get bindless texture handle for texture ID {0}", textureID);
			textureHandles.push_back(0);
			continue;
		}

		// Make the handle resident (accessible in shaders)
		if (!glIsTextureHandleResidentARB(handle))
		{
			glMakeTextureHandleResidentARB(handle);
		}

		textureHandles.push_back(handle);
	}

	// Upload texture handles to SSBO - only reallocate if size changed
	const size_t requiredSize = textureHandles.size();
	const size_t uploadBytes = requiredSize * sizeof(GLuint64);

	// Only reallocate if size changed (prevents both orphaning and stale data)
	if (requiredSize != m_TextureArraySSBOCapacity)
	{
		// Size changed - must reallocate to prevent stale data
		glBufferData(GL_SHADER_STORAGE_BUFFER, uploadBytes, textureHandles.data(), GL_DYNAMIC_DRAW);
		m_TextureArraySSBOCapacity = requiredSize;

		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			EE_CORE_ERROR("Failed to allocate texture array SSBO: {0} bytes, error: {1}",
				uploadBytes, error);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			m_TextureArrayDirty = false;
			return;
		}
	}
	else
	{
		// Size unchanged - safe to use glBufferSubData (avoids orphaning)
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, uploadBytes, textureHandles.data());
	}

	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		EE_CORE_ERROR("Failed to upload texture array to GPU, error: {0}", error);
	}
	else
	{
		EE_CORE_INFO("Uploaded {0} texture handles ({1} bytes) to GPU",
			textureHandles.size(), uploadBytes);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	m_TextureArrayDirty = false;
}






/**
 * @brief Generates an Interleaved Gradient Noise (IGN) texture for jittering and dithering effects.
 * IGN produces spatially smooth but temporally stable noise patterns ideal for ray marching.
 */
void Renderer::GenerateIGNTexture()
{
	int width = static_cast<int>(m_IGNTextureSize.x);
	int height = static_cast<int>(m_IGNTextureSize.y);

	// Clean up existing texture if present
	if (m_IGNTexture != 0)
	{
		if (m_IGNTextureHandle != 0 && glIsTextureHandleResidentARB(m_IGNTextureHandle))
		{
			glMakeTextureHandleNonResidentARB(m_IGNTextureHandle);
		}
		glDeleteTextures(1, &m_IGNTexture);
		m_IGNTexture = 0;
		m_IGNTextureHandle = 0;
	}

	// Generate Interleaved Gradient Noise
	std::vector<float> noiseData(width * height);

	// IGN formula: fract(52.9829189 * fract(x * 0.06711056 + y * 0.00583715))
	const float magic1 = 0.06711056f;
	const float magic2 = 0.00583715f;
	const float magic3 = 52.9829189f;

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			// Use pixel center coordinates
			float fx = static_cast<float>(x) + 0.5f;
			float fy = static_cast<float>(y) + 0.5f;

			// Apply IGN formula
			float value = glm::fract(magic3 * glm::fract(fx * magic1 + fy * magic2));

			int idx = y * width + x;
			noiseData[idx] = value;
		}
	}

	// Create OpenGL texture
	glGenTextures(1, &m_IGNTexture);
	glBindTexture(GL_TEXTURE_2D, m_IGNTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, noiseData.data());

	// Texture parameters for tiling and filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Create bindless texture handle
	m_IGNTextureHandle = glGetTextureHandleARB(m_IGNTexture);
	if (m_IGNTextureHandle == 0)
	{
		EE_CORE_ERROR("Failed to create bindless texture handle for IGN texture");
		return;
	}

	// Make handle resident (accessible in shaders)
	glMakeTextureHandleResidentARB(m_IGNTextureHandle);

	EE_CORE_INFO("Generated IGN texture: {0}x{1}, Handle: {2}", width, height, m_IGNTextureHandle);
	glCheckError();
}
