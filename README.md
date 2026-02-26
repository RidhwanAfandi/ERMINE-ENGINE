# **ErmineEngine by ErmineWorks**

The Ermine Engine made by third year students of DigiPen Singapore.

> :exclamation: Warning: Project is still in development!

## Key Features

### Core Systems

- Entity-Component-System (ECS) architecture
- Hierarchical entity parenting
- Scene serialization (Save / Load)
- Job system for multi-threaded tasks
- Asset database with pipeline integration

### Rendering

- Physically-Based Rendering (PBR)
- Image-Based Lighting (IBL)
- HDR + Bloom post-processing
- Tone Mapping and Gamma Correction
- Skybox & Environment Cubemap support
- GPU Profiler (frame time graph & performance metrics)
- Forward + Deferred shading modes
- Screen-space Ambient Occlusion (SSAO) toggle

### Animation & Models

- Skeletal animation via Assimp FBX import
- Soft skinning with GPU bone blending
- Model import with material & texture binding

### Physics

- Integrated with Jolt Physics (JPH)
- Rigid body & trigger types
- Box, Sphere, Capsule, and Mesh collider support
- Dynamic, static, and kinematic motion types

### Audio

- 3D positional audio
- Global audio and ambient sounds

### Editor Tools

- Real-time editor built with ImGui
- Asset Browser (models, textures, shaders)
- Scene Hierarchy & Inspector windows
- Inspector Components (Transform, Mesh, Material, Light, Physics, Audio, Script, Model, Animation)
- Material Editor with shader parameter controls
- Particle Editor with live preview
- Audio Debug ImGui panel
- 3D Gizmos for Translate / Rotate / Scale
- Performance Graphs (CPU/GPU timing, FPS tracking)
- Script reloading and live assembly updates
- Creation and deletion of entities
- Parent and child Hierarchy

## Editor Controls

Left Mouse         - Select an object
Right Mouse + WASD - Move the editor camera forward and sideways
Right Mouse + QE   - Move the editor camera up and down
Left SHIFT         - Speed up camera movement speed
Mouse Scroll       - Zoom in and out camera
CTRL + P           - Play mode
CTRL + SHIFT + P   - Stop mode
Q                  - Toggle Local / World translate
W                  - Switch to Translate mode
E                  - Switch to Rotate mode
R                  - Switch to Scale mode
Y                  - Toggle Pivot / Center mode (where gizmo appears)
1 / 2              - Toggle PBR / Blinn-Phong shading
3                  - Toggle Deferred Rendering
4                  - Toggle SSAO

## **Build Information**

1. Please run MakeProject.bat to setup vsproj configuration.
2. Open the solution in Visual Studio 2022.
3. Build both Engine and Script-Assembly projects.
4. Launch the editor to begin creating or testing scenes.

## **FAQ Issues**

1. Missing/Mismatch Script-Assembly.dll error
This can be solved by building Script-Assembly project from VS2022 as it can be missed when recompiling project.
2. Missing Assets or Invalid Paths
Ensure that the Ermine-ResourcePipeline and .lion_rcdbase asset database are located in the same directory as the engine executable.

## Team Roaster

### Programmers

(IMGD)

- Kean (Product Manager)
- Ko Sand (Tech Lead)
- Brian (Assets)
- Si Han (Physics)
- Curtis (Serialization)
(RTIS)
- Jeremy (Graphics)
- Kai Rui (Audio)
- Ridhwan (Graphics Lead)
- Edwin (Scene)

### Designers

(UXGD)

- Klive (Design Lead)
- Brandon (Audio Lead)
- Fabio (Mechanics Design)

### Artist

(BFA)

- Lera (Art Lead)
- Naomi
- Wei Jia

## License

© 2025 DigiPen Institute of Technology.
Reproduction or distribution of this project or its components without prior written consent is prohibited.
