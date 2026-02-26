/* Start Header ************************************************************************/
/*!
\file       Engine.cpp
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\date       09/03/2025
\brief      This file contains the declaration of the Engine system.
			Expose key engine functions to be called from the editor's main loop
			Helps managed subsystems of the engine

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "PreCompile.h"
#include "Engine.h"
// ECS and Components
#include "ECS.h"
#include "Components.h"
// Internal Engine Systems
#include "Input.h"
#include "Logger.h"
#include "AssetManager.h"
#include "FrameController.h"
#include "GeometryFactory.h"
#include "JobSystem.h"
#include "Serialisation.h"
#include "Window.h"
// Engine Systems
#include "Renderer.h"
#include "ScriptEngine.h"
#include "AudioSystem.h"
#include "Particles.h"
#include "GPUParticles.h"
#include "Physics.h"
#include "FiniteStateMachine.h"
#include "Skybox.h"
#include "Cubemap.h"
#include "ScriptSystem.h"
#include "AnimationManager.h"
#include "ConsoleGUI.h"
#include "SettingsGUI.h"
#include "GuidRegistry.h"
#include "Scene.h"
#include "HierarchySystem.h"
#include "CameraSystem.h"
#include "UIRenderSystem.h"
#include "UIButtonSystem.h"
#include "VideoManager.h"
#include "NavMesh.h"	 
#include "NavMeshAgentSystem.h"
#include "GISystem.h"
#include "EditorGUI.h"

#if defined(EE_EDITOR)
#include "GraphicsDebugGUI.h"
#include "AssetBrowser.h"
#include "EditorCamera.h"
#include "ViewPortGUI.h"
#include "AudioImGUI.h"
#include "SceneManager.h"
#include "FSMEditor.h"
#include "AnimationGUI.h"
#include "ResourcePipe.h"
#include "VideoImGUI.h"

#endif

using namespace Ermine;

#define EE_AUTO_REGISTER_COMPONENT(Type, Name) \
	static bool _##Type##_autoreg = [](){ Ermine::ECS::GetInstance().RegisterComponent<Type>(Name); return true; }();

namespace
{
	bool s_isInitialized = false;

	// For Skybox/Environment mapping
	static std::unique_ptr<Ermine::graphics::Skybox> skybox;
	static std::shared_ptr<Ermine::graphics::Cubemap> environmentCubemap;

	void EnableMemoryLeakChecking(int breakAlloc = -1)
	{
		int tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
		_CrtSetDbgFlag(tmpDbgFlag);
		if (breakAlloc != -1)
			_CrtSetBreakAlloc(breakAlloc);
	}

	EntityID fbxEntity = 0;

	// /*!***********************************************************************
	// \brief
	// Creates a main menu scene dynamically with camera and menu script
	// \return
	// Shared pointer to the created scene
	// *************************************************************************/
	// std::shared_ptr<Ermine::Scene> CreateMainMenuScene()
	// {
	// 	auto scene = std::make_shared<Ermine::Scene>("Main Menu");

	// 	// Create main camera entity
	// 	Ermine::EntityID cameraEntity = scene->CreateEntity("MainCamera", true, true);
	// 	ECS::GetInstance().AddComponent(cameraEntity, CameraComponent(45.0f, 16.0f / 9.0f, 0.1f, 100.0f, true, false));
	// 	EE_CORE_INFO("Created main menu camera entity: {}", cameraEntity);

	// 	// Create menu script entity
	// 	Ermine::EntityID menuScriptEntity = scene->CreateEntity("MenuController", true, true);
	// 	ScriptsComponent scriptsComp;
	// 	scriptsComp.Add("MainMenu", menuScriptEntity, true);
	// 	ECS::GetInstance().AddComponent<ScriptsComponent>(menuScriptEntity, scriptsComp);
	// 	EE_CORE_INFO("Created menu script entity: {}", menuScriptEntity);

	// 	// Create UI entity for menu buttons/text - THIS IS THE MAIN MENU UI
	// 	Ermine::EntityID uiEntity = scene->CreateEntity("MenuUI", false, false);
	// 	UIComponent uiComp;

	// 	// ===== CONFIGURE UI FOR MAIN MENU =====
	// 	// Enable all UI rendering
	// 	uiComp.showHealthbar = false;      // No health bar for main menu
	// 	uiComp.showManaBar = false;        // No mana bar for main menu
	// 	uiComp.showSkills = false;         // No skill slots for main menu
	// 	uiComp.showCrosshair = false;      // No crosshair for main menu
	// 	uiComp.showBookCounter = false;    // No book counter for main menu

	// 	// Set initial health to render menu background/title
	// 	uiComp.currentHealth = uiComp.maxHealth;
	// 	uiComp.currentMana = uiComp.maxMana;

	// 	// Position menu elements at center of screen
	// 	uiComp.healthbarPosition = Ermine::Vec3(0.35f, 0.6f, 0.0f);
	// 	uiComp.healthbarWidth = 0.3f;
	// 	uiComp.healthbarHeight = 0.15f;

	// 	// Configure health bar as a background panel (solid color)
	// 	uiComp.healthbarBgColor = Ermine::Vec3(0.1f, 0.1f, 0.1f);  // Dark background
	// 	uiComp.healthbarColor = Ermine::Vec3(0.2f, 0.8f, 0.3f);    // Green (menu ready indicator)

	// 	// Add dummy skills to render menu buttons
	// 	// Skill 0: Play Button
	// 	uiComp.skills[0].skillName = "Play Game";
	// 	uiComp.skills[0].keyBinding = "SPACE";
	// 	uiComp.skills[0].iconTexturePath = "../Resources/Textures/UI/Skills/play_icon.png";
	// 	uiComp.skills[0].description = "Start Game";

	// 	// Skill 1: Settings Button
	// 	uiComp.skills[1].skillName = "Settings";
	// 	uiComp.skills[1].keyBinding = "S";
	// 	uiComp.skills[1].iconTexturePath = "../Resources/Textures/UI/Skills/settings_icon.png";
	// 	uiComp.skills[1].description = "Game Settings";

	// 	// Skill 2: Credits Button
	// 	uiComp.skills[2].skillName = "Credits";
	// 	uiComp.skills[2].keyBinding = "C";
	// 	uiComp.skills[2].iconTexturePath = "../Resources/Textures/UI/Skills/credits_icon.png";
	// 	uiComp.skills[2].description = "View Credits";

	// 	// Skill 3: Quit Button
	// 	uiComp.skills[3].skillName = "Quit";
	// 	uiComp.skills[3].keyBinding = "Q";
	// 	uiComp.skills[3].iconTexturePath = "../Resources/Textures/UI/Skills/quit_icon.png";
	// 	uiComp.skills[3].description = "Exit Game";

	// 	// Position skill slots (menu buttons)
	// 	uiComp.skillsPosition = Ermine::Vec3(0.5f, 0.4f, 0.0f);
	// 	uiComp.skillSlotSize = 0.12f;
	// 	uiComp.skillSlotSpacing = 0.02f;

	// 	// Show the health bar as background and skills as buttons
	// 	uiComp.showHealthbar = true;
	// 	uiComp.showSkills = true;

	// 	ECS::GetInstance().AddComponent<UIComponent>(uiEntity, uiComp);
	// 	EE_CORE_INFO("Created menu UI entity with buttons: {}", uiEntity);

	// 	// Create a main light
	// 	Ermine::EntityID lightEntity = scene->CreateEntity("MainLight", true, true);
	// 	ECS::GetInstance().AddComponent(
	// 		lightEntity,
	// 		Transform(
	// 			Ermine::Vec3(0, 5, 0),
	// 			FromEulerDegrees(50.0f, -30.0f, 0.0f),
	// 			Ermine::Vec3(1, 1, 1)));
	// 	ECS::GetInstance().AddComponent(lightEntity, ObjectMetaData("Main Light", "Light", true));
	// 	ECS::GetInstance().AddComponent(lightEntity, Light(Ermine::Vec3(1, 1, 1), 1.0f, LightType::DIRECTIONAL, true));
	// 	EE_CORE_INFO("Created menu light entity: {}", lightEntity);

	// 	EE_CORE_INFO("Main Menu scene created successfully with {} entities", scene->GetEntityCount());
	// 	return scene;
	// }

}

bool engine::Init(GLFWwindow* windowContext)
{
	if (s_isInitialized) // Already initialized
		return true;

	(void)CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	std::string databasePath = "../Ermine-Game.lion_rcdbase";  // Adjust path as needed
	std::string projectGuid = "";  // Leave empty to auto-detect, or put your actual project GUID

	if (!AssetManager::GetInstance().Initialize(databasePath, projectGuid)) {
		EE_CORE_WARN("AssetManager database initialization failed, falling back to direct loading");
	}

	EnableMemoryLeakChecking();

	Input::Init(windowContext);

	FrameController::Init(120.f, 60.f);

	graphics::GPUProfiler::Init(150); // Track last 150 frames

	job::Initialize();

	ECS::GetInstance().Init();

	AudioSystem::Init();

	// TODO: Register all components here, limit of 255 components
	EE_AUTO_REGISTER_COMPONENT(Transform, "Transform")
	EE_AUTO_REGISTER_COMPONENT(Rigidbody3D, "Rigidbody3D")
	EE_AUTO_REGISTER_COMPONENT(Mesh, "Mesh")
	EE_AUTO_REGISTER_COMPONENT(Material, "Material")
	EE_AUTO_REGISTER_COMPONENT(ObjectMetaData, "ObjectMetaData")
	EE_AUTO_REGISTER_COMPONENT(Light, "Light")
	EE_AUTO_REGISTER_COMPONENT(LightProbeVolumeComponent, "LightProbeVolumeComponent")
	EE_AUTO_REGISTER_COMPONENT(AudioComponent, "AudioComponent")
	EE_AUTO_REGISTER_COMPONENT(GlobalAudioComponent, "GlobalAudioComponent")
	EE_AUTO_REGISTER_COMPONENT(PhysicComponent, "PhysicComponent")
	EE_AUTO_REGISTER_COMPONENT(ModelComponent, "ModelComponent")
	EE_AUTO_REGISTER_COMPONENT(AnimationComponent, "AnimationComponent")
	EE_AUTO_REGISTER_COMPONENT(HierarchyComponent, "HierarchyComponent");
	EE_AUTO_REGISTER_COMPONENT(StateMachine, "StateMachine")
	EE_AUTO_REGISTER_COMPONENT(NavMeshComponent, "NavMesh")
	EE_AUTO_REGISTER_COMPONENT(NavMeshAgent, "NavMeshAgent")
	EE_AUTO_REGISTER_COMPONENT(NavJumpLink, "NavJumpLink")
	EE_AUTO_REGISTER_COMPONENT(GlobalTransform, "GlobalTransform")
	EE_AUTO_REGISTER_COMPONENT(ParticleEmitter, "ParticleEmitter")
	EE_AUTO_REGISTER_COMPONENT(GPUParticleEmitter, "GPUParticleEmitter")
	EE_AUTO_REGISTER_COMPONENT(CameraComponent, "CameraComponent");
	EE_AUTO_REGISTER_COMPONENT(UIComponent, "UIComponent");
	EE_AUTO_REGISTER_COMPONENT(UIHealthbarComponent, "UIHealthbarComponent");
	EE_AUTO_REGISTER_COMPONENT(UICrosshairComponent, "UICrosshairComponent");
	EE_AUTO_REGISTER_COMPONENT(UISkillsComponent, "UISkillsComponent");
	EE_AUTO_REGISTER_COMPONENT(UIManaBarComponent, "UIManaBarComponent");
	EE_AUTO_REGISTER_COMPONENT(UIBookCounterComponent, "UIBookCounterComponent");
	EE_AUTO_REGISTER_COMPONENT(UIButtonComponent, "UIButtonComponent");
	EE_AUTO_REGISTER_COMPONENT(UISliderComponent, "UISliderComponent");
	EE_AUTO_REGISTER_COMPONENT(UIImageComponent, "UIImageComponent");

	// NOTE : THESE ARE SPECIAL CASES DUE TO THE FACT THAT THEIR COMPONENTS ARE UNIQUE AND WOULDN'T WORK BY SHALLOW COPIED OR DEEP COPIED
	// THE CLONING FUNCTIONALITY HAVE BEEN CONSIDERED INTO ECS ITSELF. UNSURE, ASK.
	// Special case for Script component, ctor a new script with same class name
	ECS::GetInstance().RegisterComponent<Script>("Script",
		[](ComponentManager& cm, EntityID src, EntityID dst)
		{
			if (!cm.HasComponent<Script>(src)) return;
			auto& srcScript = cm.GetComponent<Script>(src);
			cm.AddComponent<Script>(dst, Script(srcScript.m_className, dst));
		});

	ECS::GetInstance().RegisterComponent<ScriptsComponent>("ScriptsComponent",
		[](ComponentManager& cm, EntityID src, EntityID dst)
		{
			if (!cm.HasComponent<ScriptsComponent>(src)) return;
			auto& srcScriptsComp = cm.GetComponent<ScriptsComponent>(src);
			ScriptsComponent dstScriptsComp;
			for (const auto& srcScript : srcScriptsComp.scripts)
			{
				Script newScript(srcScript.m_className, dst);
				newScript.m_enabled = srcScript.m_enabled;
				newScript.m_fields = srcScript.m_fields; // Copy fields
				dstScriptsComp.scripts.emplace_back(std::move(newScript));
			}
			cm.AddComponent<ScriptsComponent>(dst, std::move(dstScriptsComp));
		});

	// Special case for IDComponent with custom clone to force new GUID, as IDs should be unique
	ECS::GetInstance().RegisterComponent<IDComponent>("IDComponent",
		[](ComponentManager& cm, [[maybe_unused]] EntityID src, EntityID dst)
		{
			auto g = Guid::New();
			cm.AddComponent<IDComponent>(dst, IDComponent{ g });
			ECS::GetInstance().GetGuidRegistry().Register(dst, g);
		});

	// Register all systems
	ECS::GetInstance().RegisterSystem<graphics::Renderer>();
	ECS::GetInstance().RegisterSystem<graphics::ModelSystem>();
	ECS::GetInstance().RegisterSystem<graphics::MaterialSystem>();
	ECS::GetInstance().RegisterSystem<scripting::ScriptSystem>();
	ECS::GetInstance().RegisterSystem<AudioSystem>();
	ECS::GetInstance().RegisterSystem<ParticleSystem>();
	ECS::GetInstance().RegisterSystem<GPUParticleSystem>();
	ECS::GetInstance().RegisterSystem<graphics::LightSystem>();
	ECS::GetInstance().RegisterSystem<graphics::AnimationManager>();
	ECS::GetInstance().RegisterSystem<HierarchySystem>();
	ECS::GetInstance().RegisterSystem<StateManager>();
	ECS::GetInstance().RegisterSystem<NavMeshSystem>();
	ECS::GetInstance().RegisterSystem<NavMeshAgentSystem>();
	ECS::GetInstance().RegisterSystem<graphics::CameraSystem>();
	ECS::GetInstance().RegisterSystem<UIRenderSystem>();
	ECS::GetInstance().RegisterSystem<UIButtonSystem>();
	ECS::GetInstance().RegisterSystem<VideoManager>();
	ECS::GetInstance().RegisterSystem<GISystem>();

	//Register JPH::TempAllocatorImpl for Physcis
	RegisterDefaultAllocator();
	ECS::GetInstance().RegisterSystem<Physics>();
	ECS::GetInstance().GetSystem<Physics>()->Init();
	ECS::GetInstance().GetSystem<Physics>()->AttachDebugRenderer(std::make_shared<MyDebugRenderer>());

	// Set system signatures
	SignatureID sig;

	// For Renderer system
	sig.set(ECS::GetInstance().GetComponentType<Mesh>());
	ECS::GetInstance().SetSystemSignature<graphics::Renderer>(sig);

	// For Model system
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<ModelComponent>());
	ECS::GetInstance().SetSystemSignature<graphics::ModelSystem>(sig);

	// For Material system
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<Material>());
	ECS::GetInstance().SetSystemSignature<graphics::MaterialSystem>(sig);

	// For CameraSystem system
	// CameraSystem doesn't require any components to exist (it's a singleton system) ???? that is not how it works!
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<CameraComponent>());
	ECS::GetInstance().SetSystemSignature<graphics::CameraSystem>(sig);

	// For Script system
	sig.reset();
	//sig.set(ECS::GetInstance().GetComponentType<Script>());
	sig.set(ECS::GetInstance().GetComponentType<ScriptsComponent>());
	ECS::GetInstance().SetSystemSignature<scripting::ScriptSystem>(sig);

	// For Audio system
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<AudioComponent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<AudioSystem>(sig);

	// For Particles
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	sig.set(ECS::GetInstance().GetComponentType<ParticleEmitter>());
	ECS::GetInstance().SetSystemSignature<ParticleSystem>(sig);

	// For GPU Orb Particles
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	sig.set(ECS::GetInstance().GetComponentType<GPUParticleEmitter>());
	ECS::GetInstance().SetSystemSignature<GPUParticleSystem>(sig);

	// For Physics
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<PhysicComponent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<Physics>(sig);

	// Lights
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<Light>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<graphics::LightSystem>(sig);

	// For Animation system
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<AnimationComponent>());
	ECS::GetInstance().SetSystemSignature<graphics::AnimationManager>(sig);

	// For Hierarchy System
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<HierarchyComponent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<HierarchySystem>(sig);

	// For FSM
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<StateMachine>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<StateManager>(sig);

	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<NavMeshComponent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<NavMeshSystem>(sig);

	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<NavMeshAgent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<NavMeshAgentSystem>(sig);

	// For UI Rendering System
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<UIComponent>());
	ECS::GetInstance().SetSystemSignature<UIRenderSystem>(sig);

	// For UI Button System (only requires UIButtonComponent)
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<UIButtonComponent>());
	ECS::GetInstance().SetSystemSignature<UIButtonSystem>(sig);

	// Video Manager (no component requirements)
	sig.reset();
	ECS::GetInstance().SetSystemSignature<VideoManager>(sig);

	// GI System (light probe volumes)
	sig.reset();
	sig.set(ECS::GetInstance().GetComponentType<LightProbeVolumeComponent>());
	sig.set(ECS::GetInstance().GetComponentType<Transform>());
	ECS::GetInstance().SetSystemSignature<GISystem>(sig);

	glfwSetFramebufferSizeCallback(windowContext, []([[maybe_unused]] GLFWwindow* window, int width, int height)
		{
#if defined(EE_EDITOR)
			editor::EditorCamera::GetInstance().SetViewportSize(static_cast<float>(width), static_cast<float>(height));
			ECS::GetInstance().GetSystem<graphics::CameraSystem>()->SetViewportSize(static_cast<float>(width), static_cast<float>(height));
#else
			glViewport(0, 0, width, height);
			auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
			if (renderer && width > 0 && height > 0)
				renderer->OnWindowResize(width, height);

			auto uiSystem = ECS::GetInstance().GetSystem<UIRenderSystem>();
			if (uiSystem && width > 0 && height > 0)
				uiSystem->OnScreenResize(width, height);

		auto buttonSystem = ECS::GetInstance().GetSystem<UIButtonSystem>();
		if (buttonSystem && width > 0 && height > 0)
			buttonSystem->OnScreenResize(width, height);

		auto videoSystem = ECS::GetInstance().GetSystem<VideoManager>();
		if (videoSystem && width > 0 && height > 0)
			videoSystem->OnScreenResize(width, height);
#endif
		});

	// Create graphics resources
	auto shader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/gBuffer_vertex.glsl", "../Resources/Shaders/gBuffer_fragment.glsl");
	auto texture = AssetManager::GetInstance().LoadTexture("../Resources/Textures/greybox_light_grid.png");

	// Load skybox shader and create a simple test cubemap
	auto skyboxShader = AssetManager::GetInstance().LoadShader("../Resources/Shaders/skybox_vertex.glsl", "../Resources/Shaders/skybox_fragment.glsl");

	std::array<std::string, 6> cubemapFaces = {
		"../Resources/Textures/Skybox/right.jpg",   // +X (right)
		"../Resources/Textures/Skybox/left.jpg",    // -X (left)
		"../Resources/Textures/Skybox/bottom.jpg",  // +Y (top)
		"../Resources/Textures/Skybox/top.jpg",     // -Y (bottom)
		"../Resources/Textures/Skybox/front.jpg",   // +Z (front)
		"../Resources/Textures/Skybox/back.jpg"     // -Z (back)
	};
	environmentCubemap = AssetManager::GetInstance().LoadCubemap(cubemapFaces, "default_skybox");

	// Create the skybox if cubemap loaded successfully
	if (environmentCubemap && environmentCubemap->IsValid() && skyboxShader && skyboxShader->IsValid()) {
		skybox = std::make_unique<graphics::Skybox>(environmentCubemap, skyboxShader);
		EE_CORE_INFO("Skybox created successfully");
		// Register skybox with renderer so it actually gets rendered
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		if (renderer)
			renderer->SetSkybox(skybox.get());
	}
	else {
		EE_CORE_WARN("Failed to create skybox - cubemap or shader invalid");
	}

	// Create shared materials for common use cases
	std::shared_ptr<graphics::Material> basicWhiteMaterial = AssetManager::GetInstance().CreateMaterial("basic_white", shader, "PBR_WHITE");
	std::shared_ptr<graphics::Material> metalMaterial = AssetManager::GetInstance().CreateMaterial("shiny_metal", shader, "PBR_METAL");
	std::shared_ptr<graphics::Material> emissiveWhiteMaterial = AssetManager::GetInstance().CreateMaterial("light_emissive", shader, "EMISSIVE_WHITE");
	std::shared_ptr<graphics::Material> glassMaterial = AssetManager::GetInstance().CreateMaterial("clear_glass", shader, "PBR_GLASS");
	std::shared_ptr<graphics::Material> waterMaterial = AssetManager::GetInstance().CreateMaterial("water_surface", shader, "PBR_WATER");

	EE_CORE_INFO("Created shared materials with proper texture assignment control");

	// Initialize game camera
	int windowWidth, windowHeight;
	glfwGetFramebufferSize(windowContext, &windowWidth, &windowHeight);

	ECS::GetInstance().GetSystem<NavMeshSystem>()->Init();
	// initialize particles emitter
	ECS::GetInstance().GetSystem<ParticleSystem>()->Init(shader);
	ECS::GetInstance().GetSystem<GPUParticleSystem>()->Init();

	EE_CORE_INFO("Total living entities after creation: {0}", ECS::GetInstance().GetLivingEntityCount());

	//EE_CORE_INFO("FSM Test Cube created with ID: {}", s_FSMCube);
	if (windowWidth > 0 && windowHeight > 0)
		ECS::GetInstance().GetSystem<graphics::Renderer>()->Init(windowWidth, windowHeight);
	else
		ECS::GetInstance().GetSystem<graphics::Renderer>()->Init(1920, 1080); // Fallback to default size

	// Initialize UI Render System
	if (windowWidth > 0 && windowHeight > 0)
		ECS::GetInstance().GetSystem<UIRenderSystem>()->Init(windowWidth, windowHeight);
	else
		ECS::GetInstance().GetSystem<UIRenderSystem>()->Init(1920, 1080);

	// Initialize UI Button System with same dimensions
	if (windowWidth > 0 && windowHeight > 0)
		ECS::GetInstance().GetSystem<UIButtonSystem>()->Init(windowWidth, windowHeight);
	else
		ECS::GetInstance().GetSystem<UIButtonSystem>()->Init(1920, 1080);

	// Initialize Video Manager with same dimensions
	if (windowWidth > 0 && windowHeight > 0)
		ECS::GetInstance().GetSystem<VideoManager>()->Init(windowWidth, windowHeight);
	else
		ECS::GetInstance().GetSystem<VideoManager>()->Init(1920, 1080);

	// Editor windows
#if defined(EE_EDITOR)
	SceneManager::GetInstance().NewScene();

	EE_CORE_INFO("Material system now supports efficient sharing between entities using shared_ptr");
	EE_CORE_INFO("Systems and components registered successfully, Engine Initialized");

	editor::EditorGUI::CreateImGUIWindow<ParticlesImGUI>();
	editor::EditorGUI::CreateImGUIWindow<AudioImGUI>();
	editor::EditorGUI::CreateImGUIWindow<editor::GraphicsDebugGUI>();
	editor::EditorGUI::CreateImGUIWindow<ViewPortGUI>();
	editor::EditorGUI::CreateImGUIWindow<FSMEditorImGUI>();
	editor::EditorGUI::CreateImGUIWindow<AnimationEditorImGUI>();
	editor::EditorGUI::CreateImGUIWindow<ConsoleGUI>();
	editor::EditorGUI::CreateImGUIWindow<ImguiUI::AssetBrowser>();
	editor::EditorGUI::CreateImGUIWindow<SettingsGUI>();
	editor::EditorGUI::CreateImGUIWindow<VideoImGUI>();

	// Legacy ImGui menu windows removed - replaced with scene-based UI:
	// - Main menu: Open Resources/Scenes/mainmenu.scene, edit MenuBackground/GameTitle entities
	// - Cutscene: Open Resources/Scenes/cutscene_intro.scene, edit Slide1/2/3 entities
	// - Scripts MenuController.cs and CutscenePlayer.cs handle the logic
	// editor::EditorGUI::CreateImGUIWindow<editor::MainMenuGUI>();
	// auto* cutsceneGUI = editor::EditorGUI::CreateImGUIWindow<editor::CutsceneGUI>();
	// cutsceneGUI->LoadSlideshow(...);
	// cutsceneGUI->SetNextScene(...);

	{
		static Ermine::ResourcePipeline pipeline;
		if (pipeline.Initialize("../Resources")) {
			EE_CORE_INFO("ResourcePipeline initialized successfully");

			auto* assetBrowser = editor::EditorGUI::GetWindow<ImguiUI::AssetBrowser>();
			if (assetBrowser) {
				assetBrowser->InitWithPipeline(&pipeline);
				EE_CORE_INFO("AssetBrowser connected to ResourcePipeline");
			}
			else {
				EE_CORE_ERROR("Failed to get AssetBrowser window");
			}
		}
		else {
			EE_CORE_ERROR("Failed to initialize ResourcePipeline");
		}
	}

	// Load main menu scene on startup (scene-based approach)
	EE_CORE_INFO("Loading main menu scene...");
	SceneManager::GetInstance().OpenScene("../Resources/Scenes/mainmenu_video_bg.scene");
	EE_CORE_INFO("Main menu scene loaded");
#else
	SceneManager::GetInstance().OpenScene("../Resources/Scenes/mainmenu_video_bg.scene"); // Load mainmenu scene
	editor::EditorGUI::s_state = editor::EditorGUI::SimState::playing;			 // Set to playing state
#endif

	s_isInitialized = true;

	return true;
}

void engine::Shutdown()
{
	if (!s_isInitialized)
		return;

	//const std::filesystem::path scenePath = "Ermine-Engine.scene";
	//SaveSceneToFile("Ermine-Engine", scenePath);
	//SaveCurrentScene("Level01");

	Config cfg{};
	int width, height;
	glfwGetWindowSize(glfwGetCurrentContext(), &width, &height);
	cfg.windowWidth = width;
	cfg.windowHeight = height;
	cfg.fullscreen = (glfwGetWindowMonitor(glfwGetCurrentContext()) != nullptr);
	cfg.maximized = (glfwGetWindowAttrib(glfwGetCurrentContext(), GLFW_MAXIMIZED) == GLFW_TRUE);
	cfg.imguiWindows = ImGUIWindow::GetAllWindowStates();
	cfg.fontSize = SettingsGUI::GetFontSizeS();
	cfg.baseFontSize = SettingsGUI::GetBaseFontSize();
	cfg.themeMode = SettingsGUI::GetMode();

#if defined(EE_EDITOR)
	cfg.title = "Ermine Editor 0.4";
#else
	cfg.title = "Machina";
#endif

	SaveConfigToFile(cfg, "Ermine-Engine.config", false);

	skybox.reset();           // Destroy skybox before cubemap
	environmentCubemap.reset(); // Destroy cubemap before AssetManager cleanup

	CoUninitialize();

	AssetManager::GetInstance().Clear();
	ECS::GetInstance().GetSystem<Physics>()->Shutdown();
	ECS::GetInstance().GetSystem<NavMeshSystem>()->Shutdown();
	ECS::GetInstance().GetSystem<VideoManager>()->Shutdown();

	graphics::GPUProfiler::Shutdown();

#if defined(EE_EDITOR)
	auto scriptSys = ECS::GetInstance().GetSystem<scripting::ScriptSystem>();
	if (scriptSys && scriptSys->m_ScriptEngine)
	{
		scriptSys->m_ScriptEngine->StopWatchingScriptSources();
		scriptSys->m_ScriptEngine->StopWatchingGameAssembly();
	}
#endif

	if (auto scriptSys = ECS::GetInstance().GetSystem<scripting::ScriptSystem>())
		scriptSys->CleanupAllScripts();

	job::Shutdown();

	AssetManager::GetInstance().Clear();
	ECS::GetInstance().GetSystem<Physics>()->Shutdown();
	ECS::GetInstance().GetSystem<NavMeshSystem>()->Shutdown();
	ECS::GetInstance().GetSystem<VideoManager>()->Shutdown();
	graphics::GPUProfiler::Shutdown();

	ECS::GetInstance().GetSystem<scripting::ScriptSystem>()->m_ScriptEngine->Shutdown();
	AudioSystem::Shutdown();

	ECS::GetInstance().Shutdown();

	s_isInitialized = false;
}

void engine::Update([[maybe_unused]] GLFWwindow* windowContext)
{
	if (!s_isInitialized)
		return;

	// Update FrameController
	FrameController::BeginFrame();

	// Profiler
	graphics::GPUProfiler::BeginFrame();

	// Handle shading mode toggle
	//HandleShadingToggle(windowContext);

	// Handle fullscreen toggle
	HandleFullscreenToggle(windowContext);

	// Update input states
	Input::Update();

	glfwPollEvents();

	auto& sm = SceneManager::GetInstance();
	if (sm.HasPendingSceneRequest())
		sm.FlushPendingSceneRequest();

	// ========== CHECK PAUSE STATE ==========
	bool isPaused = UIButtonSystem::IsGamePaused();  // CHANGE THIS LINE
	// =======================================

	// Only update game logic if not paused
	if (!isPaused)
	{
		// Game state update
		while (FrameController::ShouldUpdateFixed())
		{
			ECS::GetInstance().GetSystem<scripting::ScriptSystem>()->FixedUpdate();
			ECS::GetInstance().GetSystem<Physics>()->Update(FrameController::GetFixedDeltaTime());
		}

		// Other non-fixed logic
		ECS::GetInstance().GetSystem<scripting::ScriptSystem>()->Update();									// Game logic updates transforms, forces, etc
		ECS::GetInstance().GetSystem<HierarchySystem>()->UpdateHierarchy();									// Update hierarchy transforms first
		ECS::GetInstance().GetSystem<GISystem>()->Update();													// Assign GI probe indices
		ECS::GetInstance().GetSystem<StateManager>()->Update(FrameController::GetFixedDeltaTime());		// FSM update
		ECS::GetInstance().GetSystem<NavMeshAgentSystem>()->Update(FrameController::GetFixedDeltaTime());	// AI NavMesh Agent update
		ECS::GetInstance().GetSystem<graphics::AnimationManager>()->Update(FrameController::GetDeltaTime());// Animation Update
		ECS::GetInstance().GetSystem<ParticleSystem>()->Update(FrameController::GetDeltaTime());
		ECS::GetInstance().GetSystem<GPUParticleSystem>()->Update(FrameController::GetDeltaTime());

		// Update video playback
		ECS::GetInstance().GetSystem<VideoManager>()->Update(FrameController::GetDeltaTime());
	}

	// UI ALWAYS updates (even when paused, so pause menu works!)
	ECS::GetInstance().GetSystem<UIButtonSystem>()->Update(FrameController::GetDeltaTime());
	ECS::GetInstance().GetSystem<UIRenderSystem>()->Update(FrameController::GetDeltaTime());

	// Update camera
#if defined(EE_EDITOR)
	if (editor::EditorGUI::isPlaying)
	{
		auto gameCamera = ECS::GetInstance().GetSystem<graphics::CameraSystem>();
		gameCamera->Update();
	}
	else
	{
		editor::EditorCamera::GetInstance().Update();
	}
#else
	auto gameCamera = ECS::GetInstance().GetSystem<graphics::CameraSystem>();
	gameCamera->Update();
#endif

	// Audio always updates
	ECS::GetInstance().GetSystem<AudioSystem>()->Update();
}

void engine::Render(GLFWwindow* window)
{
	if (!s_isInitialized)
		return;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	glViewport(0, 0, width, height);

	Mtx44 view;
	Mtx44 proj;

#if defined(EE_EDITOR)
	// Use appropriate camera based on play state
	if (editor::EditorGUI::isPlaying)
	{
		auto gameCamera = ECS::GetInstance().GetSystem<graphics::CameraSystem>();
		if (gameCamera && gameCamera->HasValidCamera())
		{
			view = gameCamera->GetViewMatrix();
			proj = gameCamera->GetProjectionMatrix();
		}
		else
		{
			// Fallback to editor camera if no valid game camera
			view = editor::EditorCamera::GetInstance().GetViewMatrix();
			proj = editor::EditorCamera::GetInstance().GetProjectionMatrix();
		}
	}
	else
	{
		// Use editor camera when not playing
		view = editor::EditorCamera::GetInstance().GetViewMatrix();
		proj = editor::EditorCamera::GetInstance().GetProjectionMatrix();
	}
#else
	// Standalone build - use game camera
	auto gameCamera = ECS::GetInstance().GetSystem<graphics::CameraSystem>();
	if (gameCamera && gameCamera->HasValidCamera())
	{
		view = gameCamera->GetViewMatrix();
		proj = gameCamera->GetProjectionMatrix();
	}
	else
	{
		// Fallback if no camera is available
		view = Mtx44(); // Identity matrix
		proj = Mtx44(); // Identity matrix
	}
#endif

	// Start GPU timing for rendering
	graphics::GPUProfiler::BeginEvent("Frame");

	auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();

	// Clear buffers
	renderer->Clear();

	// Draw scene objects
	renderer->Update(view, proj);

	// Render video overlay (before UI and ImGui)
	auto videoSystem = ECS::GetInstance().GetSystem<VideoManager>();
	if (videoSystem)
		videoSystem->Render();

	// Stop GPU timing for rendering
	graphics::GPUProfiler::EndEvent();

	// Render UI overlays (health bars, mana, skills, crosshairs)
	// This renders to the offscreen framebuffer (before it's captured for the viewport)
	auto uiSystem = ECS::GetInstance().GetSystem<UIRenderSystem>();
	if (uiSystem)
		uiSystem->Render();

#if defined(EE_EDITOR)
	// Unbind framebuffer after UI rendering (so ImGui renders to the window)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

	// Render ImGui/Editor on top of everything
#if defined(EE_EDITOR)
	if (editor::EditorGUI::IsInit())
		editor::EditorGUI::Render();
#endif

	glfwSwapBuffers(window);

	graphics::GPUProfiler::EndFrame();
}

/**
 * @brief Handle shading mode toggle (keys 1-4)
 * @param windowContext The GLFW window context
 */
void engine::HandleShadingToggle(GLFWwindow* windowContext)
{
	static bool key1WasPressed = false;
	static bool key2WasPressed = false;
	static bool key3WasPressed = false;
	static bool key4WasPressed = false;

	bool key1IsPressed = glfwGetKey(windowContext, GLFW_KEY_1) == GLFW_PRESS;
	bool key2IsPressed = glfwGetKey(windowContext, GLFW_KEY_2) == GLFW_PRESS;
	bool key3IsPressed = glfwGetKey(windowContext, GLFW_KEY_3) == GLFW_PRESS;
	bool key4IsPressed = glfwGetKey(windowContext, GLFW_KEY_4) == GLFW_PRESS;

	// Toggle to PBR (key 1)
	if (key1IsPressed && !key1WasPressed) {
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		renderer->SetShadingMode(false);
		EE_CORE_INFO("Switched to PBR shading");
	}

	// Toggle to Blinn-Phong (key 2)
	if (key2IsPressed && !key2WasPressed) {
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		renderer->SetShadingMode(true);
		EE_CORE_INFO("Switched to Blinn-Phong shading");
	}

	// Toggle to Deferred (key 3)
	if (key3IsPressed && !key3WasPressed) {
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		renderer->ToggleDeferredRendering();
	}

	// Toggle SSAO (key 4)
	if (key4IsPressed && !key4WasPressed) {
		auto renderer = ECS::GetInstance().GetSystem<graphics::Renderer>();
		renderer->m_SSAOEnabled = !renderer->m_SSAOEnabled;
	}

	key1WasPressed = key1IsPressed;
	key2WasPressed = key2IsPressed;
	key3WasPressed = key3IsPressed;
	key4WasPressed = key4IsPressed;
}

/**
 * @brief Handle fullscreen toggle (F11 key)
 * @param windowContext The GLFW window context
 */
void Ermine::engine::HandleFullscreenToggle(GLFWwindow* windowContext)
{
	static bool f11WasPressed = false;
	bool f11IsPressed = glfwGetKey(windowContext, GLFW_KEY_F11) == GLFW_PRESS;
	if (f11IsPressed && !f11WasPressed) Ermine::Window::ToggleFullscreenWindow(windowContext);
	f11WasPressed = f11IsPressed;
}
