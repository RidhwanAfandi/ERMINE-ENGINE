/* Start Header ************************************************************************/
/*!
\file       Renderer.h
\author     WONG JUN YU, Kean, junyukean.wong, 2301234, junyukean.wong\@digipen.edu
\co-author  Jeremy Lim Ting Jie, jeremytingjie.lim, 2301370, jeremytingjie.lim\@digipen.edu
\co-author  Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b\@digipen.edu
\co-author  Lum Ko Sand, kosand.lum, 2301263, kosand.lum\@digipen.edu
\date       27/09/2025
\brief      This file contains the declaration of the Renderer system.
            This file is used to render the game objects to the screen.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VertexArray.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "Systems.h"
#include "GPUProfiler.h"
#include "Material.h"
#include "Components.h"
#include "EditorCamera.h"
#include "shadow_config.h"
#include "MeshManager.h"
#include "HierarchySystem.h"
#include <algorithm>
#include <cmath>

namespace Ermine::graphics
{

    // Transparency rendering support
    struct TransparentObject {
        EntityID entity;
        float distanceToCamera;
        glm::mat4 modelMatrix;

        bool operator<(const TransparentObject& other) const {
            return distanceToCamera > other.distanceToCamera; // Sort back-to-front
        }
    };

    // Frustum culling support
    struct Frustum {
        glm::vec4 planes[6]; // Left, Right, Bottom, Top, Near, Far

        /**
         * @brief Extract frustum planes from view-projection matrix
         * @param viewProj Combined view-projection matrix
         */
        void ExtractFromViewProjection(const glm::mat4& viewProj) {
            // Extract using Gribb & Hartmann method
            // Matrix is accessed as mat[col][row] in GLM (column-major)
            // We want row vectors, so we transpose the access pattern

            glm::vec4 row0(viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]);
            glm::vec4 row1(viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]);
            glm::vec4 row2(viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]);
            glm::vec4 row3(viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]);

            // Left plane
            planes[0] = row3 + row0;
            // Right plane
            planes[1] = row3 - row0;
            // Bottom plane
            planes[2] = row3 + row1;
            // Top plane
            planes[3] = row3 - row1;
            // Near plane
            planes[4] = row3 + row2;
            // Far plane
            planes[5] = row3 - row2;

            // Normalize planes
            for (int i = 0; i < 6; i++) {
                float length = glm::length(glm::vec3(planes[i]));
                if (length > 0.0001f) {
                    planes[i] /= length;
                }
            }
        }


        /**
         * @brief Get the 8 frustum corners in world space
         * @param invViewProj Inverse of the view-projection matrix
         * @return Array of 8 corners [nearBL, nearBR, nearTR, nearTL, farBL, farBR, farTR, farTL]
         */
        std::array<glm::vec3, 8> GetCorners(const glm::mat4& invViewProj) const {
            std::array<glm::vec3, 8> corners;

            // NDC corners of the frustum (normalized device coordinates)
            // Near plane: z = -1, Far plane: z = 1
            glm::vec4 ndcCorners[8] = {
                glm::vec4(-1, -1, -1, 1), // near bottom-left
                glm::vec4( 1, -1, -1, 1), // near bottom-right
                glm::vec4( 1,  1, -1, 1), // near top-right
                glm::vec4(-1,  1, -1, 1), // near top-left
                glm::vec4(-1, -1,  1, 1), // far bottom-left
                glm::vec4( 1, -1,  1, 1), // far bottom-right
                glm::vec4( 1,  1,  1, 1), // far top-right
                glm::vec4(-1,  1,  1, 1)  // far top-left
            };

            for (int i = 0; i < 8; i++) {
                glm::vec4 worldCorner = invViewProj * ndcCorners[i];
                corners[i] = glm::vec3(worldCorner) / worldCorner.w; // Perspective divide
            }

            return corners;
        }

        /**
         * @brief Test if AABB is inside or intersecting frustum
         * @param aabbMin AABB minimum in world space
         * @param aabbMax AABB maximum in world space
         * @return true if visible (inside or intersecting), false if completely outside
         */
        bool TestAABB(const glm::vec3& aabbMin, const glm::vec3& aabbMax) const {
            // Test AABB against all 6 frustum planes
            for (int i = 0; i < 6; i++) {
                // Get positive vertex (furthest point in plane normal direction)
                glm::vec3 positiveVertex;
                positiveVertex.x = (planes[i].x >= 0.0f) ? aabbMax.x : aabbMin.x;
                positiveVertex.y = (planes[i].y >= 0.0f) ? aabbMax.y : aabbMin.y;
                positiveVertex.z = (planes[i].z >= 0.0f) ? aabbMax.z : aabbMin.z;

                // If positive vertex is outside, AABB is completely outside
                if (glm::dot(glm::vec3(planes[i]), positiveVertex) + planes[i].w < 0.0f) {
                    return false; // Outside this plane
                }
            }
            return true; // Inside or intersecting
        }
    };

    // Lights

    /*!***********************************************************************
    \brief Light System. Contains all light entities in the scene.
    *************************************************************************/
    class LightSystem : public System {};

    /*!***********************************************************************
    \brief Light System. Contains all light entities in the scene.
    *************************************************************************/
    class ModelSystem : public System {};

    /*!***********************************************************************
    \brief Light System. Contains all light entities in the scene.
    *************************************************************************/
    class MaterialSystem : public System {};



    /*!***********************************************************************
    \brief Light GPU structure
    *************************************************************************/
    struct LightGPU
    {
        glm::vec4 position_type;    // xyz = position (view space), w = light type
        glm::vec4 color_intensity;  // xyz = color, w = intensity
        glm::vec4 direction_range;  // xyz = direction (view space), w = range
		glm::vec4 spot_angles_castshadows_startOffset; // x = inner angle (cos), y = outer angle (cos), z = flags bitfield, w = shadow base layer or -1 if no shadows
        glm::mat4 lightSpaceMatrix[NUM_CASCADES];
        glm::mat4 pointLightMatrices[6];
		glm::vec4 splitDepths[(NUM_CASCADES+3)/4]; // split depths for cascaded shadow maps xyzw
    };

    struct ShadowViewGPU
    {
        glm::mat4 lightSpaceMatrix;
        glm::uvec4 data; // x = shadow layer index
    };

	/**
	 * @brief GPU representation of a light probe for UBO transmission
	 * Stores spherical harmonics coefficients for indirect lighting
	 */
	struct LightProbeGPU
	{
		glm::vec4 position_radius;      // xyz = world position, w = unused
		glm::vec4 shCoefficients[9];    // SH L2 coefficients (vec4 for std140 alignment, only xyz used)
		glm::vec4 boundsMin;            // xyz = world bounds min, w = padding
		glm::vec4 boundsMax;            // xyz = world bounds max, w = padding
		glm::vec4 flags;                // x = isActive (1.0 or 0.0), y = priority, zw = padding
	};

	/**
	 * @brief Volume of light probes stored on GPU
	 */
	struct LightProbeVolumeGPU
	{
		glm::vec4 boundsMin_spacing;    // xyz = boundsMin, w = unused
		glm::vec4 boundsMax_padding;    // xyz = boundsMax, w = unused
		glm::vec4 gridDimensions;       // xyz = probe count per axis, w = total probe count
		glm::vec4 probeSpacing;         // xyz = spacing between probes, w = unused
	};



    // Forward declarations
    struct MaterialSSBO;
    class Skybox;

    //physic wireframe
    struct DebugVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    /**
     * @brief The Renderer class is responsible for rendering the game objects to the screen.
     */
    class Renderer : public System
    {
    public:
		// Mesh Manager
		MeshManager m_MeshManager;

        size_t culledMeshes = 0;

        // Debug visualization toggles
        bool m_DebugDrawAABBs = false;
        bool m_DebugDrawFrustum = false;
        bool m_DebugDrawBoneAABBs = false;

        // Lighting Pass Parameters
        // SSAO parameters
        bool m_SSAOEnabled = false;
		int  m_SSAOSamples = 16;
		float m_SSAORadius = 10.0f;
		float m_SSAOBias = 0.01f;
		float m_SSAOIntensity = 1.0f;
		float m_SSAOFadeout = 0.1f;
		float m_SSAOMaxDistance = 1000.0f;

        // Fog parameters
        bool m_FogEnabled = false;
        int m_FogMode = 0;  // 0 = linear, 1 = exponential, 2 = exponential squared
        glm::vec3 m_FogColor = glm::vec3(0.5f, 0.6f, 0.7f);
        float m_FogDensity = 0.02f;   // For exponential fog modes
        float m_FogStart = 50.0f;     // For linear fog
        float m_FogEnd = 200.0f;      // For linear fog
		float m_FogHeightCoefficient = 0.1f; // For height-based fog
		float m_FogHeightFalloff = 10.0f;      // For height-based fog

        // Ambient lighting parameters
        glm::vec3 m_AmbientColor = glm::vec3(1.0f, 1.0f, 1.0f);  // RGB color of ambient light
        float m_AmbientIntensity = 0.08f;  // Intensity multiplier for ambient light

		// GI bake parameters
		int m_GIBakeBounces = 2;
		float m_GIBakeEnergyLoss = 0.6f;
        
        // Post-processing uniforms - toggles
        bool m_VignetteEnabled = false;
        bool m_FXAAEnabled = true;
        bool m_ToneMappingEnabled = true;
        bool m_GammaCorrectionEnabled = true;
        bool m_BloomEnabled = true;
        bool m_SkyBoxisHDR = false;
        bool m_ShowSkybox = true;
        bool m_FilmGrainEnabled = false;
        bool m_ChromaticAberrationEnabled = false;
        bool m_RadialBlurEnabled = false;

        // Post-processing uniforms - parameters
        float m_Exposure = 1.0f;
        float m_Contrast = 1.0f;
        float m_Saturation = 1.0f;
        float m_Gamma = 2.2f;
        float m_VignetteIntensity = 0.3f;
        float m_VignetteRadius = 0.8f;
        float m_VignetteCoverage = 0.0f;
        float m_VignetteFalloff = 0.2f;
        float m_VignetteMapStrength = 1.0f;
        std::string m_VignetteMapPath{};
        glm::vec3 m_VignetteMapRGBModifier = glm::vec3(0.0f, 0.0f, 0.0f);
        float m_BloomStrength = 0.04f;
        float m_GrainIntensity = 0.015f;
        float m_GrainScale = 1.5f;
        float m_ChromaticAmount = 0.003f;
        float m_RadialBlurStrength = 0.0f;
        int m_RadialBlurSamples = 12;
        glm::vec2 m_RadialBlurCenter = glm::vec2(0.5f, 0.5f);

        // FXAA parameters
        float m_FXAASpanMax = 8.0f;
        float m_FXAAReduceMin = 1.0f / 128.0f;
        float m_FXAAReduceMul = 1.0f / 8.0f;

        // Bloom pass parameters
        float m_BloomThreshold = 1.0f;
        float m_BloomIntensity = 2.0f;
        float m_BloomRadius = 1.0f;

		// Spotlight ray parameters
        bool m_SpotlightRaysEnabled = true;
        float m_SpotlightRayIntensity = 0.3f;
        float m_SpotlightRayFalloff = 2.0f;

        // Motion blur parameters
        bool m_MotionBlurEnabled = false;
        float m_MotionBlurStrength = 1.0f;
        int m_MotionBlurSamples = 8;

        // Maximum bone array size expected in shader
        static constexpr int MAX_BONE_UNIFORMS = 128;

        GlobalGraphics m_GlobalGraphics;

        // Outline (selection) parameters
        bool m_OutlineEnabled = true;
        glm::vec3 m_OutlineColor = glm::vec3(1.0, 0.4f, 0.0f);
        float m_OutlineThickness = 2.5f;
        float m_OutlineIntensity = 1.5f;

        // Sync helpers
        void SyncToGlobalGraphics();    // copy class -> m_GlobalGraphics
        void ApplyFromGlobalGraphics(); // copy m_GlobalGraphics -> class

        void SetVignetteMapTexture(const std::shared_ptr<Texture>& texture, const std::string& path)
        {
            m_VignetteMapTexture = texture;
            m_VignetteMapPath = path;
        }

        void ClearVignetteMapTexture()
        {
            m_VignetteMapTexture.reset();
            m_VignetteMapPath.clear();
        }

        bool HasVignetteMapTexture() const
        {
            return m_VignetteMapTexture && m_VignetteMapTexture->IsValid();
        }

        /**
         * @brief To run the pass and read GL_STENCIL_INDEX at a pixel
         * @param x The x coordinate from the framebuffer
         * @param y The y coordinate from the framebuffer
         * @param view camera view matrix
         * @param projection camera projection matrix
         * @return
         */
        std::pair<bool, EntityID> PickEntityAt(const int& x, const int& y, const Mtx44& view, const Mtx44& projection);

        /**
		 * @brief Update the shadow maps for all lights that cast shadows.
		 */
        void InitializeShadowMapResources();
        /**
         * @brief Initialize the renderer with the screen width and height.
         * @param screenWidth The width of the screen
         * @param screenHeight The height of the screen
         */
        void Init(const int& screenWidth, const int& screenHeight);

        /**
         * @brief Marks draw data for full rebuild on next frame.
         * Call this when entities, materials, meshes, transforms, or hierarchy changes
         * to ensure all rendering data is properly updated.
         */
        void MarkDrawDataForRebuild() { m_DrawDataNeedsFullRebuild = true; }

        //PHYSICS
        /**
         * @brief Submits a debug line to be rendered in the scene.
         * @param from Starting point of the line in world space.
         * @param to Ending point of the line in world space.
         * @param color RGB color of the line.
         */
        void SubmitDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);

        /**
         * @brief Renders all submitted debug lines using the provided view and projection matrices.
         * @param view View matrix for the camera.
         * @param proj Projection matrix for the camera.
         */
        void RenderDebugLines(const glm::mat4& view, const glm::mat4& proj);

        /**
         * @brief Renders all submitted debug lines using the provided view and projection matrices.
         * @param view View matrix for the camera (custom Mtx44 type).
         * @param proj Projection matrix for the camera (custom Mtx44 type).
         */
        void RenderDebugLines(const Mtx44& view, const Mtx44& proj);

        /*!***********************************************************************
        \brief
         Adds a filled debug triangle to the renderer�s internal vertex list for
         visualization purposes. The triangle will be rendered during the next call
         to RenderDebugTriangles().
        \param[in] a
         The first vertex position of the triangle in world space.
        \param[in] b
         The second vertex position of the triangle in world space.
        \param[in] c
         The third vertex position of the triangle in world space.
        \param[in] color
         The RGB color of the triangle to render.
        \return
         None.
        *************************************************************************/
        void SubmitDebugTriangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& color);
        /*!***********************************************************************
        \brief
         Renders all submitted debug triangles in 3D space using the provided
         view and projection matrices. This is typically used for visualizing
         geometry such as collision shapes, navigation meshes, or debug overlays.
        \param[in] view
         The view matrix representing the current camera orientation and position.
        \param[in] proj
         The projection matrix defining the camera�s perspective or orthographic view.
        \return
         None.
        *************************************************************************/
        void RenderDebugTriangles(const Mtx44& view, const Mtx44& proj);

        /**
         * @brief Submit an AABB wireframe for debug visualization
         * @param min AABB minimum corner in world space
         * @param max AABB maximum corner in world space
         * @param color Color of the wireframe
         */
        void SubmitDebugAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color);

        /**
         * @brief Submit frustum wireframe for debug visualization
         * @param frustum The frustum to visualize
         * @param invViewProj Inverse view-projection matrix for corner calculation
         * @param color Color of the wireframe
         */
        void SubmitDebugFrustum(const Frustum& frustum, const glm::mat4& invViewProj, const glm::vec3& color);

        /**
         * @brief Offscreen buffer structure for rendering to texture
         */
        struct OffscreenBuffer
        {
            unsigned int FBO; // Frame Buffer Object
            unsigned int ColorTexture;
            unsigned int RBO; // Render Buffer Object

            // Viewport size
            int width;
            int height;
        };


        //struct InstanceData {
        //    glm::mat4 model; // per-entity transform
        //    glm::mat3 normalMat; // per-entity normal matrix
        //    //glm::vec4 colour; // optional tint
        //};

        //// group by mesh pointer, shader, texture
        //struct BatchKey {
        //    //Mesh* k_mesh;
        //    const graphics::VertexArray* k_vao;
        //    const graphics::IndexBuffer* k_ibo;

        //    std::shared_ptr<Shader> k_shader;
        //    std::shared_ptr<Texture> k_texture;

        //    bool operator<(const BatchKey& other) const {
        //        if (k_vao != other.k_vao) return k_vao < other.k_vao;
        //        if (k_ibo != other.k_ibo) return k_ibo < other.k_ibo;
        //        if (k_shader != other.k_shader) return k_shader < other.k_shader;
        //        return k_texture < other.k_texture;
        //    }
        //};

        /**
        * @brief G buffer structure for rendering to Lighting pass
        */
        //~Renderer();
        struct GBuffer
        {
            unsigned int FBO = 0;
            unsigned int DepthTexture = 0;

            // Multiple Render Targets (MRTs)

            uint64_t HandlePackedTexture0 = 0;
            uint64_t HandlePackedTexture1 = 0;
            uint64_t HandlePackedTexture2 = 0;
            uint64_t HandlePackedTexture3 = 0;
            uint64_t HandlePackedTexture4 = 0;  // Velocity buffer
            uint64_t HandleDepthTexture = 0;

            unsigned int PackedTexture0 = 0;
            unsigned int PackedTexture1 = 0;
            unsigned int PackedTexture2 = 0;
            unsigned int PackedTexture3 = 0;
            unsigned int PackedTexture4 = 0;    // Velocity buffer



            int width = 0;
            int height = 0;
        };


        /**
        * @brief PostProcessing buffer structure for each post-processing effect
        */
        struct PostProcessBuffer
        {
			unsigned int FBO = 0;
			unsigned int ColorTexture = 0;

			int width = 0;
			int height = 0;
        };


        /**
         * @brief Destructor - cleans up allocated resources
         */
        ~Renderer();


        /**
         * @brief Create an offscreen buffer for viewport/scene rendering
         * @param width The width of the offscreen buffer
         * @param height The height of the offscreen buffer
         * @return OffscreenBuffer The offscreen buffer
         */
        OffscreenBuffer CreateOffscreenBuffer(const int& width, const int& height);

        /**
         * @brief Resize the offscreen buffer to new dimensions without recreating the FBO
         * @param width New width
         * @param height New height
         */
        void ResizeOffscreenBuffer(const int& width, const int& height);

        /**
         * @brief Create  g-buffer for deferred rendering
         * @param width The width of the g-buffer
         * @param height The height of the g-buffer
         */
        void CreateGBuffer(const int& width, const int& height);


        /**
         * @brief Create post-processing buffer
         * @param width The width of the post-processing buffer
         * @param height The height of the post-processing buffer
         */
        void CreatePostProcessBuffer(const int& width, const int& height);

        /**
         * @brief Resize the g-buffer to new dimensions
         * @param width New width
         * @param height New height
         */
        void ResizeGBuffer(const int& width, const int& height);

        /**
         * @brief Begin geometry pass for deferred rendering
         * Clear g-buffer and set up for geometry rendering
         */
        void BeginGeometryPass();

        /**
         * @brief End geometry pass and prepare for lighting pass
         */
        void EndGeometryPass();

        /**
         * @brief Begin lighting pass for deferred rendering
         * Bind g-buffer textures and set up for lighting calculations
         */
        void BeginLightingPass();

        /**
         * @brief End lighting pass and finalize frame
         */
        void EndLightingPass();

        /**
         * @brief Render depth pre-pass to eliminate overdraw in geometry pass.
         * Renders all geometry depth-only, so expensive G-buffer fragment shader only runs for visible pixels.
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderDepthPrePass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render a geometry pass for deferred rendering
         * This function handles the CPU-side setup and issues draw calls
         * The actual g-buffer writing happens in the geometry fragment shader
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderGeometryPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render lighting pass for deferred rendering
         * Reads from g-buffer textures and performs lighting calculations on GPU
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderLightingPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render Post-processing effects using the lighting pass output
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderPostProcessPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Complete deferred rendering pipeline
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderDeferredPipeline(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Bind g-buffer textures to specified texture units
         * @param startingTextureUnit The first texture unit to bind to (default: 0)
         */
        void BindGBufferTextures();

        std::shared_ptr<OffscreenBuffer> GetOffscreenBuffer() const { return m_OffscreenBuffer; }
        std::shared_ptr<GBuffer> GetGBuffer() const { return m_GBuffer; }
        std::shared_ptr<PostProcessBuffer> GetPostProcessBuffer() const { return m_PostProcessBuffer; }
        std::shared_ptr<PostProcessBuffer> GetMotionBlurBuffer() const { return m_MotionBlurBuffer; }

        /**
        * @brief Cleanup g-buffer resources
        */
        void CleanupGBuffer();

        /**
        * @brief Cleanup postprocess buffer resources
        */
        void CleanupPostProcessBuffer();

        /**
         * @brief Update the game objects to the screen.
         */
        void Update(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Draw the game objects to the screen.
         */
        void Draw(const std::shared_ptr<VertexArray>& vao, const std::shared_ptr<IndexBuffer>& ibo) const;

        /**
         * @brief Draw the game objects to the screen using instanced rendering.
         */
        void DrawInstanced(const std::shared_ptr<VertexArray>& vao, const std::shared_ptr<IndexBuffer>& ibo, int instanceCount) const;

        /**
         * @brief Clear the screen.
         */
        void Clear() const;

        /**
         * @brief Get the current performance metrics
         * @return CurrentGPU performance metrics
         */
        const GPUProfiler::PerformanceMetrics& GetPerformanceMetrics() const;
        /**
         * @brief Set the shading mode for rendering.
         * @param isBlinnPhong If true, enables Blinn-Phong shading; if false, uses PBR shading.
         */
        void SetShadingMode(bool isBlinnPhong) { m_IsBlinnPhong = isBlinnPhong; }
        /**
         * @brief Get the current shading mode.
         * @return True if Blinn-Phong shading is enabled, false if PBR shading is used.
         */
        bool GetShadingMode() const { return m_IsBlinnPhong; }
        /**
         * @brief Updates the lights' shader SSBO with the current light and transform data from all living entities.
         * @param view Current output-frame view matrix.
         * @param projection Current output-frame projection matrix.
         */
        void UpdateLightsSSBO(const Mtx44& view, const Mtx44& projection);
        size_t GetUploadedLightCount() const { return m_LastUploadedLightCount; }
        size_t GetLightSSBOCapacity() const { return m_LightsSSBOCapacity; }
        
		/**
		 * @brief Updates the light probes UBO with current probe data from all probe entities.
		 * Uploads probe positions, SH coefficients, bounds, and priority to GPU.
		 */
		void UpdateLightProbesUBO();

		/**
		 * @brief Initializes light probe capture resources (cubemap FBO, textures).
		 * Must be called before capturing any probes.
		 */
		void InitializeProbeCaptureResources();

		/**
		 * @brief Captures environment lighting at a probe's position into SH coefficients.
		 * Renders scene to cubemap, then projects to spherical harmonics.
		 * @param probeEntity The entity containing the LightProbeVolumeComponent to update.
		 */
		void CaptureLightProbe(EntityID probeEntity);

		/**
		 * @brief Projects a cubemap texture to spherical harmonics (L2, 9 coefficients).
		 * @param cubemapID OpenGL texture ID of the cubemap to project.
		 * @param outCoefficients Array to store 9 vec3 SH coefficients (27 floats total).
		 */
		void ProjectCubemapToSH(GLuint cubemapID, glm::vec3 outCoefficients[9]);

		/**
		 * @brief Projects a cubemap array layer (6 faces) to spherical harmonics (L2, 9 coefficients).
		 * @param probeIndex Index of the probe in the cubemap array.
		 * @param outCoefficients Array to store 9 vec3 SH coefficients (27 floats total).
		 */
		void ProjectCubemapArrayToSH(int probeIndex, glm::vec3 outCoefficients[9]);

        /**
         * @brief Updates the material's shader storage buffer object (SSBO) at a specific index.
         * Used for dynamic material updates after initial compilation.
         * @param materialData The material data to be uploaded to the SSBO.
         * @param materialIndex The index in the material array to update.
         */
        void UpdateMaterialSSBO(const graphics::MaterialSSBO& materialData, uint32_t materialIndex);

        /**
         * @brief Updates the material's SSBO with the specified material data (legacy version).
         * Updates index 0 by default. Prefer using the indexed version.
         * @param materialData The material data to be uploaded to the SSBO.
         */
        void UpdateMaterialSSBO(const graphics::MaterialSSBO& materialData);

        /**
         * @brief Update multiple entities' material albedo color and upload to GPU.
         * @param entities Vector of entity IDs whose materials will be updated.
         * @param color The new albedo color to set for each material.
		 */ 
        void UpdateMultipleEntitiesMaterials(const std::vector<EntityID>& entities,
            const Vec3& color)
        {
            auto& ecs = ECS::GetInstance();
            auto renderer = ecs.GetSystem<Renderer>();

            for (EntityID entity : entities) {
                if (!ecs.HasComponent<Ermine::Material>(entity)) continue;

                auto& materialComp = ecs.GetComponent<Ermine::Material>(entity);
                auto* material = materialComp.GetMaterial();

                if (!material) continue;

                // Update material
                material->SetVec3("materialAlbedo", color);

                // Upload to GPU
                auto ssboData = material->GetSSBOData();
                uint32_t materialIndex = renderer->GetMaterialIndex(entity);
                renderer->UpdateMaterialSSBO(ssboData, materialIndex);
            }
        }

        /**
         * @brief Update an entity's material properties and upload to GPU.
         * @param entity The entity whose material will be updated.
         * @param albedo The new albedo color.
         * @param roughness The new roughness value.
         * @param metallic The new metallic value.
         * @param emissive The new emissive color.
		 */
        void UpdateMaterialColor(EntityID entity,
            const Vec3& albedo,
            float roughness,
            float metallic,
            const Vec3& emissive);

        /**
		* @brief Retrieves the material index for the specified entity.
		* @param entity The entity whose material index to retrieve.
        */
        uint32_t GetMaterialIndex(EntityID entity) const;

        /**
         * @brief Sets the u_MaterialIndex uniform for the entity's material.
         * Call this before each draw call to tell the shader which material to use.
         * @param entity The entity whose material index to set.
         * @param shader The shader program to set the uniform on.
         */
        void SetMaterialIndex(EntityID entity, const std::shared_ptr<Shader>& shader);

        
        /**
         * @brief Compiles all materials from entities with Material and Model components into a single SSBO.
         * This function collects material data from all entities, uploads it to GPU memory, and assigns
         * material indices to each entity for shader access. Should be called once after scene load or
         * when materials are added/removed.
         */
        void CompileMaterials();

        /**
         * @brief Checks if any materials have been modified and marks for recompilation.
         * This is called every frame to detect ImGui or runtime material changes.
         */
        void CheckMaterialUpdates();

        /**
         * @brief Marks materials as dirty, triggering recompilation on next frame.
         * Call this when materials are added, removed, or modified.
         * Also clears draw data cache to force full rebuild (prevents stale cache on scene reload).
         */
        void MarkMaterialsDirty() {
            m_MaterialsDirty = true;
            m_DrawDataNeedsFullRebuild = true;
            m_CachedDrawItems.clear();
            m_LastEntityListHash = 0;
        }

        /**
         * @brief Forces a full rebuild of draw data on the next frame.
         * Useful for debugging or when manual refresh is needed.
         */
        void ForceDrawDataRebuild() {
            m_DrawDataNeedsFullRebuild = true;
        }

        /**
         * @brief Registers a texture in the global texture array.
         * @param texture Shared pointer to the texture.
         * @return The index of the texture in the array, or -1 if registration failed.
         */
        int RegisterTexture(std::shared_ptr<Texture> texture);

        /**
         * @brief Gets the texture array index for a given texture ID.
         * @param textureID The OpenGL texture ID.
         * @return The array index, or -1 if not found.
         */
        int GetTextureArrayIndex(GLuint textureID) const;

        /**
         * @brief Builds the bindless texture array SSBO.
         * This should be called after all textures are registered and before rendering.
         */
        void BuildTextureArray();

        /**
         * @brief Compiles draw commands and draw info for all passes.
         * Routes opaque meshes to geometry/shadow passes, transparent/custom shader meshes to forward pass.
         * Iterates through all entities once and builds DrawElementsIndirectCommand + DrawInfo for all rendering.
         * Should be called every frame.
         */
        void CompileDrawData();

        /**
         * @brief Performs full rebuild of all draw data (called when entities change).
         * Clears and rebuilds entire draw command lists from scratch.
         * Called by CompileDrawData() when m_DrawDataNeedsFullRebuild is true.
         */
        void RebuildDrawData();

        /**
         * @brief Fast per-frame update of draw data (called when only transforms change).
         * Updates frustum, model matrices, and culling results without rebuilding structures.
         * Called by CompileDrawData() when only transforms changed (optimization path).
         */
        void UpdateDrawData();

        /**
         * @brief Helper functions for draw data optimization and dirty tracking.
         */
        uint64_t CalculateEntityListHash() const;
        uint64_t CalculateEntityTransformHash(EntityID entity) const;
        bool HasEntityListChanged() const;
        bool HasEntityTransformChanged(EntityID entity) const;

        /**
         * @brief Binds the MaterialBlock shader storage buffer to the specified shader program if it has not been bound before.
         * @param shader The shader program to which the material block should be bound.
         */
        void BindMaterialBlockIfPresent(const std::shared_ptr<Shader>& shader);
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
        void ToggleDeferredRendering();
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
        void RenderModelDeferred(const Model& model, graphics::Material* material, const Mtx44& view, const Mtx44& projection, const glm::mat4& rootTransform);
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
        void RenderModelForward(const Model& model, graphics::Material* material, const Mtx44& view, const Mtx44& projection, const glm::mat4& rootTransform);

        /**
         * @brief Set the skybox to be rendered
         * @param skybox Pointer to the skybox to render
         */
        void SetSkybox(graphics::Skybox* skybox) { m_skybox = skybox; }


#pragma region ShadowMapMemberFunctions
        /**
         * @brief Initializes the shadow map framebuffer object (FBO).
         * Creates and binds the FBO for shadow mapping. If a depth texture array exists, attaches it.
         * Does not validate completeness unless a depth attachment is present.
         * @return True if the FBO was successfully created, false otherwise.
         */
        bool InitializeShadowMap();
        /**
         * @brief Creates a shadow map texture array for cascaded shadow mapping.
         * Attempts to allocate a depth texture array with as many layers as possible, falling back if allocation fails.
         * Attaches the texture array to the shadow map FBO and sets up bindless texture handle.
         * @return True if the texture array was successfully created and attached, false otherwise.
         */
        bool CreateShadowMapArray();
        /**
         * @brief Calculates light-space matrices for all shadow-casting lights.
         * Computes cascade splits and shadow matrices for directional and spot lights based on the current frame view and projection.
         * Updates each light's shadow matrix and split depth for use in shadow mapping.
         * @param view Current output-frame view matrix.
         * @param projection Current output-frame projection matrix.
         */
        void CalculateLightMatrix(const Mtx44& view, const Mtx44& projection);
        /**
         * @brief Syncs shadow view state against the current output frame before rendering.
         * Rebuilds shadow layer allocation, updates light-space matrices, and re-uploads the light SSBO.
         */
        void SyncShadowViewsForOutputFrame(const Mtx44& view, const Mtx44& projection);
        /**
         * @brief Renders the shadow map for all shadow-casting lights and cascades.
         * Reuses pre-skinned positions from geometry pass to avoid redundant bone calculations.
         * Restores previous OpenGL state after rendering.
         */
		void RenderShadowMapInstanced();
        /**
         * @brief Executes the full shadow pass for all shadow-casting lights.
         * Calculates light-space matrices and renders the shadow map using instanced rendering.
         */
        void RenderShadowPass();
        /**
         * @brief Computes the eight frustum corners in world space for a given cascade split.
         * Unprojects normalized device coordinates (NDC) to world space using the inverse projection-view matrix.
         * @param invPV Inverse projection-view matrix.
         * @param nearSplit NDC Z value for the near plane of the cascade.
         * @param farSplit NDC Z value for the far plane of the cascade.
         * @return Array of eight world-space frustum corners.
         */
        std::array<glm::vec3, 8> createCascadeFrustum(const glm::mat4& invPV, float nearSplit, float farSplit);
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
        bool testSpotlightFrustumIntersection(const glm::vec3& lightPos, const glm::vec3& spotDir,
            float outerAngleRad, float lightRadius,
            const std::array<glm::vec3, 8>& frustumCorners);
        /**
         * @brief Calculates the shadow matrix for a spotlight.
         * Computes a view and orthographic projection matrix that tightly fits the cascade frustum in light space.
         * Applies texel snapping and margin adjustments for stable shadows.
         * @param lightPos Position of the spotlight.
         * @param spotDir Direction vector of the spotlight.
         * @param outerAngleRad Outer angle of the spotlight cone in radians.
         * @param lightRadius Maximum range of the spotlight.
        */
        glm::mat4 calculateSpotlightShadowMatrix(const glm::vec3& lightPos,
            const glm::vec3& spotDir,
            float outerAngleRad,
            float lightRadius);
        void calculatePointLightShadowMatrices(const glm::vec3& lightPos,
            float lightRadius,
            glm::mat4 outMatrices[6]);
#pragma endregion

        /**
         * @brief Render opaque objects with custom shaders before transparent pass
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderOpaqueCustomShaders(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render transparent objects with custom shaders
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderTransparentCustomShaders(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render transparent objects using forward rendering
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderForwardPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Render camera-attached mask for forward-rendered objects (used by motion blur)
         * @param view The view matrix
         * @param projection The projection matrix
         */
        void RenderMotionBlurMask(const Mtx44& view, const Mtx44& projection);
        void RenderMotionBlurPass();

        /**
         * @brief Sort opaque custom shader objects by shader pointer (for batching)
         * Opaque objects don't need distance sorting, only shader batching
         */
        void SortOpaqueCustomShadersByShader();

        /**
         * @brief Sort transparent objects by distance from camera
         * @param cameraPos Camera position in world space
         * @param fullRebuild If true, sort by shader then distance. If false, sort only by distance within shader groups.
         */
        void SortTransparentObjects(const Vec3& cameraPos, bool fullRebuild = true);

        /**
         * @brief Check if material is transparent based on transparency value
         * @param material The material to check
         * @return true if material should be rendered in transparent pass
         */
        bool IsTransparentMaterial(const Ermine::graphics::Material* material) const;

        /**
         * @brief Check if material casts shadows
         * @param material Material to check
         * @return true if material casts shadows
         */
        bool CastsShadows(const Ermine::graphics::Material* material) const;

        /**
         * @brief Check if material uses a custom shader (not standard deferred pipeline)
         * @param material Material to check
         * @return true if material has custom shader
         */
        bool HasCustomShader(const Ermine::graphics::Material* material) const;

        /**
         * @brief Handle window resize events to adjust buffers and viewports
         * @param width New window width
         * @param height New window height
		 */
		void OnWindowResize(const int& width, const int& height);

    protected:
		/**
		 * @brief Called when an entity is added to this system
		 * @param entity The entity that was added
		 */
		

    private:
        void EnsureLightsSSBO(size_t requiredLightCount);
        void BindLightsSSBO() const;

        // Texture Array Management (Bindless Texture System)
        struct TextureArrayEntry
        {
            GLuint textureID = 0;
            std::string filePath;
            int arrayIndex = -1;
        };

        std::vector<GLuint> m_TextureArray;                           // All textures in the array
        std::unordered_map<std::string, int> m_TexturePathToIndex;    // Map file path to array index
        std::unordered_map<GLuint, int> m_TextureIDToIndex;           // Map texture ID to array index
        GLuint m_TextureArraySSBO = 0;                                // SSBO containing texture handles
        size_t m_TextureArraySSBOCapacity = 0;                        // Track buffer capacity to avoid orphaning
        bool m_TextureArrayDirty = true;                              // Flag to trigger texture array rebuild

        // Renderer state
		uint8_t frameCounter = 0;
		float m_ElapsedTime = 0.0f;  // Accumulated time for shader effects

        // Motion blur - previous frame matrices
        glm::mat4 m_PreviousViewProjectionMatrix = glm::mat4(1.0f);
        bool m_FirstFrame = true; // Skip motion blur on first frame

		// Light System
		std::shared_ptr<LightSystem> m_LightSystem = nullptr;
		// Model System
		std::shared_ptr<ModelSystem> m_ModelSystem = nullptr;
		// Material System
		std::shared_ptr<MaterialSystem> m_MaterialSystem = nullptr;
        std::shared_ptr<OffscreenBuffer> m_OffscreenBuffer;

        // Lighting SSBO
        GLuint m_LightsSSBO = 0;
        size_t m_LightsSSBOCapacity = 0;
        size_t m_LastUploadedLightCount = 0;
        bool m_IsBlinnPhong = false; // Default to PBR shading

		// Light Probe UBO
		GLuint m_LightProbesUBO = 0;
		static constexpr GLuint ProbesBindingPoint = 5; // UBO binding point for probes
		static constexpr int MAX_PROBES = 128; // Maximum probes in UBO at once
		std::unordered_set<GLuint> m_ProbeBlockBoundPrograms;
		bool m_LightProbesEnabled = true; // Toggle probe contribution

		// Light Probe Capture (indirect cubemap array)
		GLuint m_ProbeIndirectCubemapArray = 0; // GL_TEXTURE_CUBE_MAP_ARRAY
		GLuint m_ProbeIndirectDepthArray = 0;   // Depth cubemap array
		GLuint m_ProbeVoxelAlbedoTexture = 0;   // 3D voxel albedo texture (RGBA8)
		GLuint m_ProbeVoxelEmissiveTexture = 0; // 3D voxel emissive texture (RGBA8)
		GLuint m_ProbeVoxelNormalTexture = 0;   // 3D voxel normal texture (RGBA8)
		int m_ProbeVoxelResolution = 0;

		// GI bake compute shader
		std::shared_ptr<Shader> m_ProbeBakeComputeShader;
		std::shared_ptr<Shader> m_ProbeVoxelizeComputeShader;
		std::shared_ptr<Shader> m_ProbeLightInjectComputeShader;

        // Material SSBO
        GLuint m_MaterialSSBO = 0;
        size_t m_MaterialSSBOCapacity = 0; // Track buffer capacity to avoid orphaning
        std::unordered_set<GLuint> m_MaterialBlockBoundPrograms;
        std::unordered_map<EntityID, uint32_t> m_EntityMaterialIndices; // Maps entity to material index in SSBO
        
        // Material compilation system - upload all materials at load time
        std::vector<MaterialSSBO> m_CompiledMaterials; // All materials compiled into a single vector
        bool m_MaterialsDirty = true; // Flag to trigger recompilation when materials change

        // Outline mask pass resources
		GLuint m_OutlineMaskFBO = 0;
		GLuint m_OutlineMaskTexture = 0;

        int m_ViewportWidth = 0, m_ViewportHeight = 0;

        std::shared_ptr<Shader> m_OutlineMaskIndirectShader;
        std::shared_ptr<Shader> m_OutlineMaskIndirectSkinnedShader;

        void CreateOutlineMaskBuffer(const int& width, const int& height);
        void DestroyOutlineMaskBuffer();
        void RenderOutlineMaskPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Uploads all compiled materials to the GPU SSBO at once.
         * This should be called once after CompileMaterials() during load time.
         */
        void UploadMaterialsToGPU();

        // ========================================================================
        // CUSTOM SHADER DRAW ITEM - Bundles GPU data with CPU metadata
        // ========================================================================

        /**
         * @brief Combines GPU draw data with CPU-side metadata for custom shaders.
         * This eliminates parallel array alignment issues - shader travels with its data.
         */
        struct CustomShaderDrawItem {
            DrawElementsIndirectCommand command;  // GPU indirect draw command
            DrawInfo info;                        // GPU draw info (model matrix, material index, etc.)
            std::shared_ptr<graphics::Shader> shader;  // CPU-only: for sorting and batching
            bool castsShadows;                    // Shadow casting flag (for shadow pass routing)
        };

        /**
         * @brief Combines GPU draw data with CPU-side metadata for default shader passes.
         * Used for geometry and transparent default passes.
         */
        struct DefaultShaderDrawItem {
            DrawElementsIndirectCommand command;  // GPU indirect draw command
            DrawInfo info;                        // GPU draw info (model matrix, material index, etc.)
            bool castsShadows;                    // Shadow casting flag (for shadow pass routing)
        };

        // ========================================================================
        // RENDER PASS CONTAINERS - Organized by pass, never cleared in fast path
        // ========================================================================

        // DEPTH PREPASS - Opaque geometry only (for early-z rejection)
        // Transparent objects excluded to prevent depth conflicts with objects behind them
        std::vector<DrawElementsIndirectCommand> m_DepthPrepassStandardCommands;
        std::vector<DrawInfo> m_DepthPrepassStandardInfos;
        std::vector<DrawElementsIndirectCommand> m_DepthPrepassSkinnedCommands;
        std::vector<DrawInfo> m_DepthPrepassSkinnedInfos;

        // PICKING PASS - ALL geometry (opaque + transparent, for object selection)
        std::vector<DrawElementsIndirectCommand> m_PickingStandardCommands;
        std::vector<DrawInfo> m_PickingStandardInfos;
        std::vector<DrawElementsIndirectCommand> m_PickingSkinnedCommands;
        std::vector<DrawInfo> m_PickingSkinnedInfos;

        // GEOMETRY PASS - Opaque default shader only (for deferred lighting)
        std::vector<DefaultShaderDrawItem> m_GeometryStandardItems;
        std::vector<DefaultShaderDrawItem> m_GeometrySkinnedItems;

        // SHADOW PASS - ALL geometry with castsShadows=true
        std::vector<DrawElementsIndirectCommand> m_ShadowStandardCommands;
        std::vector<DrawInfo> m_ShadowStandardInfos;
        std::vector<DrawElementsIndirectCommand> m_ShadowSkinnedCommands;
        std::vector<DrawInfo> m_ShadowSkinnedInfos;

        // FORWARD PASS - Opaque Custom Shaders (rendered after geometry pass, before transparent)
        std::vector<CustomShaderDrawItem> m_ForwardOpaqueCustomStandardItems;
        std::vector<CustomShaderDrawItem> m_ForwardOpaqueCustomSkinnedItems;

        // FORWARD PASS - Transparent Default Shaders (rendered in sorted order)
        std::vector<DefaultShaderDrawItem> m_ForwardTransparentDefaultStandardItems;
        std::vector<DefaultShaderDrawItem> m_ForwardTransparentDefaultSkinnedItems;

        // FORWARD PASS - Transparent Custom Shaders (rendered in sorted order)
        std::vector<CustomShaderDrawItem> m_ForwardTransparentCustomStandardItems;
        std::vector<CustomShaderDrawItem> m_ForwardTransparentCustomSkinnedItems;

        // ========================================================================
        // GPU BUFFERS AND CAPACITY TRACKING - For custom shader passes only
        // (Standard passes use MeshManager's buffers)
        // ========================================================================

        // Opaque custom shader GPU buffers
        GLuint m_ForwardOpaqueCustomStandardCmdBuffer = 0;
        GLuint m_ForwardOpaqueCustomStandardInfoBuffer = 0;
        GLuint m_ForwardOpaqueCustomSkinnedCmdBuffer = 0;
        GLuint m_ForwardOpaqueCustomSkinnedInfoBuffer = 0;
        size_t m_ForwardOpaqueCustomStandardCmdBufferCapacity = 0;
        size_t m_ForwardOpaqueCustomStandardInfoBufferCapacity = 0;
        size_t m_ForwardOpaqueCustomSkinnedCmdBufferCapacity = 0;
        size_t m_ForwardOpaqueCustomSkinnedInfoBufferCapacity = 0;

        // Transparent custom shader GPU buffers
        GLuint m_ForwardTransparentCustomStandardCmdBuffer = 0;
        GLuint m_ForwardTransparentCustomStandardInfoBuffer = 0;
        GLuint m_ForwardTransparentCustomSkinnedCmdBuffer = 0;
        GLuint m_ForwardTransparentCustomSkinnedInfoBuffer = 0;
        size_t m_ForwardTransparentCustomStandardCmdBufferCapacity = 0;
        size_t m_ForwardTransparentCustomStandardInfoBufferCapacity = 0;
        size_t m_ForwardTransparentCustomSkinnedCmdBufferCapacity = 0;
        size_t m_ForwardTransparentCustomSkinnedInfoBufferCapacity = 0;

        // Custom shader uploaded count tracking (for render loops)
        size_t m_ForwardOpaqueCustomStandardUploadedCount = 0;
        size_t m_ForwardOpaqueCustomSkinnedUploadedCount = 0;
        size_t m_ForwardTransparentCustomStandardUploadedCount = 0;
        size_t m_ForwardTransparentCustomSkinnedUploadedCount = 0;

        // Geometry and forward pass vertex/index count tracking (for GPU profiler)
        size_t m_GeometryStandardVertexCount = 0;
        size_t m_GeometryStandardIndexCount = 0;
        size_t m_GeometrySkinnedVertexCount = 0;
        size_t m_GeometrySkinnedIndexCount = 0;
        size_t m_ForwardPassDrawCommandsVertexCount = 0;
        size_t m_ForwardPassDrawCommandsIndexCount = 0;

        // Draw data optimization - Dirty tracking infrastructure
        bool m_DrawDataNeedsFullRebuild = true;               // Force full rebuild (entity add/remove/major change)
        bool m_NeedsTransparentSort = true;                   // Trigger transparent object sorting (set during full rebuild)
        uint64_t m_LastEntityListHash = 0;                    // Hash of entity list to detect add/remove

        // Draw data optimization - Cache for fast path (avoids expensive lookups)
        struct CachedDrawItem {
            EntityID entity;               // Parent entity ID (model root)
            EntityID childMaterialEntity;  // Child entity that was checked for material (0 if none)
            EntityID materialEntity;       // Entity that actually provided the material (child or parent)
            bool hadParentMaterial;        // Whether parent had valid material when cached
            bool hadChildMaterial;         // Whether child had valid material when cached
            MeshHandle meshHandle;         // Cached mesh handle (avoids hash map lookup)
            const MeshSubset* meshData;    // Cached mesh data pointer
            const MeshData* modelMeshData; // Cached model mesh data (for skinned CPU bounds)
            uint32_t materialIndex;        // Cached material index
            glm::vec3 aabbMin;             // Object-space AABB min
            glm::vec3 aabbMax;             // Object-space AABB max
            bool isTransparent;            // Transparency flag (affects pass routing)
            bool castsShadows;             // Shadow casting flag (affects shadow pass routing)
            bool hasCustomShader;          // Custom shader flag (affects pass routing)
            bool useSkinning;              // Skinning flag (affects VAO selection)
            bool hasSkinningData;          // Mesh has valid bone influences
            bool isCameraAttached;         // Camera-attached flag (no motion blur)
            bool flickerEmissive;          // Enable emissive flicker in the g-buffer pass
            uint32_t boneOffset;           // Bone transform offset (skinned only)
        };
        std::vector<CachedDrawItem> m_CachedDrawItems; // Cached draw items for fast updates

        // Debug/validation flags
        #ifdef EE_DEBUG
        bool m_ForceFullRebuildEveryFrame = false;            // Disable optimization for testing
        bool m_ValidateDrawDataEveryFrame = false;            // Compare optimized vs full rebuild
        #endif

        // Deferred rendering buffers
        bool m_UseDeferredRendering = true;
        Ermine::Mesh m_QuadMesh;
        std::shared_ptr<GBuffer> m_GBuffer;
        std::shared_ptr<Shader> m_DepthPrePassShader = nullptr; // Shader for depth pre-pass (eliminates overdraw)
        std::shared_ptr<Shader> m_GBufferShader = 0; // Shader for executing g-buffer pass
        std::shared_ptr<Shader> m_LightPassShader = 0; // Shader for lighting pass


        // Post-processing buffer
        std::shared_ptr<PostProcessBuffer> m_PostProcessBuffer;
        std::shared_ptr<PostProcessBuffer> m_BloomExtractBuffer;
        std::shared_ptr<PostProcessBuffer> m_BloomBlurBuffer1;
        std::shared_ptr<PostProcessBuffer> m_BloomBlurBuffer2;
        std::shared_ptr<PostProcessBuffer> m_AntiAliasingBuffer;
        std::shared_ptr<PostProcessBuffer> m_MotionBlurBuffer;
        std::shared_ptr<PostProcessBuffer> m_MotionBlurMaskBuffer;
        GLuint m_NoiseTexture = 0; // Film grain noise texture
        std::shared_ptr<Texture> m_VignetteMapTexture = nullptr; // Optional vignette map texture
        std::shared_ptr<Shader> m_BloomShader = 0; // Shader for bloom effect
        std::shared_ptr<Shader> m_PostProcessShader = 0; // Shader for post-processing effects
		std::shared_ptr<Shader> m_AAShader = 0; // Shader for anti-aliasing
        std::shared_ptr<Shader> m_MotionBlurShader = 0; // Shader for motion blur effect
        std::shared_ptr<Shader> m_MotionBlurMaskShader = 0; // Shader for motion blur mask rendering

        // Skybox
        Skybox* m_skybox = nullptr;

        // Shadow mapping
        std::shared_ptr<Shader> m_ShadowMapInstancedShader = nullptr;
        GLuint m_ShadowMapCube = 0;
        uint64_t m_ShadowMapArrayHandle = 0;
        GLuint m_ShadowMapFBO = 0;
        GLuint m_ShadowMapArray = 0;

		// Light Probe Capture Resources
		GLuint m_ProbeCubemapFBO = 0;       // Framebuffer for capturing probe cubemaps
		GLuint m_ProbeCubemap = 0;          // Cubemap texture for capture
		GLuint m_ProbeDepthCubemap = 0;     // Depth buffer for cubemap capture
		int m_ProbeCaptureResolution = 64;  // Default capture resolution per face

        // Noise textures
        GLuint m_IGNTexture = 0;
        GLuint64 m_IGNTextureHandle = 0;
		glm::vec2 m_IGNTextureSize = glm::vec2(128.0f, 128.0f);

        /**
         * @brief Generates an Interleaved Gradient Noise texture for dithering and sampling.
		 */
		void GenerateIGNTexture();

        unsigned int m_TotalShadowLayers = 0; // Total layers used by all shadow-casting lights
        int m_TotalShadowInstances = 0; // Total instances for shadow rendering (one per shadow layer)
        GLuint m_ShadowViewSSBO = 0; // SSBO containing per-layer shadow view matrices
        size_t m_ShadowViewSSBOCapacity = 0;
        std::vector<ShadowViewGPU> m_ShadowViews;
        std::vector<EntityID> m_VisibleLights;
        std::vector<EntityID> m_ShadowCastingLights;

        // Forward rendering shader for transparent objects
        std::shared_ptr<Shader> m_ForwardShader = nullptr;
        std::vector<TransparentObject> m_transparentObjects;

        //Physics
        std::vector<DebugVertex> m_DebugLines;
        unsigned int m_DebugVAO = 0, m_DebugVBO = 0;
        std::shared_ptr<Shader> debugShader = nullptr;

        void BindMaterialTextures(Ermine::graphics::Material* material);

        // Picking (stencil) helpers
        struct PickingBuffer
        {
            GLuint FBO = 0;
            GLuint ColorID = 0; // GL_R32UI
            GLuint Depth = 0; // GL_DEPTH24
            int width = 0;
            int height = 0;
        };

        std::shared_ptr<PickingBuffer> m_PickingBuffer;
        std::shared_ptr<Shader> m_PickingShader = nullptr; // Legacy picking shader (unused)

        void BuildVisibleLightSet(const Mtx44& view, const Mtx44& projection);
        void UploadLightsSSBOFromPreparedState();
        std::shared_ptr<Shader> m_PickingIndirectShader = nullptr; // Indirect rendering picking (standard meshes)
        std::shared_ptr<Shader> m_PickingIndirectSkinnedShader = nullptr; // Indirect rendering picking (skinned meshes)

        // NavMesh
        std::vector<DebugVertex> m_DebugTriangleVertices;

        /**
         * @brief Create an offscreen buffer for entity picking using stencil buffer
         * @param width The width of the picking buffer
         * @param height The height of the picking buffer
         */
        void CreatePickingBuffer(const int& width, const int& height);
        /**
         * @brief Resize the picking buffer to new dimensions without recreating the FBO
         * @param width New width
         * @param height New height
         */
        void ResizePickingBuffer(const int& width, const int& height);
        /**
         * @brief Render entities into the offscreen FBO's stencil buffer using camera VP and G-Buffer depth.
         * @param view the camera view matrix
         * @param projection the camera projection matrix
         */
        void RenderPickingPass(const Mtx44& view, const Mtx44& projection);

        /**
         * @brief Converts an Ermine::Mtx44 matrix to a glm::mat4 matrix.
         *
         * This function takes a 4x4 matrix of type Ermine::Mtx44 and converts it into
         * a glm::mat4 by directly mapping each element from row-major to the glm matrix.
         *
         * @param m The source 4x4 matrix in Ermine::Mtx44 format.
         * @return glm::mat4 A glm 4x4 matrix containing the same values as @p m.
         */
        inline glm::mat4 ToGlm(const Ermine::Mtx44& m) {
            return glm::mat4(
                m.m00, m.m01, m.m02, m.m03,
                m.m10, m.m11, m.m12, m.m13,
                m.m20, m.m21, m.m22, m.m23,
                m.m30, m.m31, m.m32, m.m33
            );
        }
        /**
         * @brief Gets the world transform matrix for an entity using GlobalTransform component
         * @param entity The entity to get the world matrix for
         * @return glm::mat4 The world transform matrix
         */
        glm::mat4 GetEntityWorldMatrix(EntityID entity) const;
    };



    /*!***********************************************************************
    \brief
     Helper function to setup mesh child entities for a model.

     Creates child entities for each mesh to track per-mesh materials.
     Each child has: MaterialComponent, HierarchyComponent, Transform.
     Children are named "Mesh_<meshID>" for reliable matching during reloads.

     \param parentEntity The entity with the ModelComponent
     \param model The loaded model whose meshes need child entities
    *************************************************************************/
    inline void SetupModelMeshChildren(EntityID parentEntity, std::shared_ptr<graphics::Model> model)
    {
        if (!model) return;

        auto& ecs = ECS::GetInstance();
        auto hierarchySystem = ecs.GetSystem<HierarchySystem>();
        auto renderer = ecs.GetSystem<graphics::Renderer>();

        if (!hierarchySystem || !renderer) return;

        // Ensure parent has HierarchyComponent
        if (!ecs.HasComponent<HierarchyComponent>(parentEntity)) {
            ecs.AddComponent<HierarchyComponent>(parentEntity, HierarchyComponent());
        }

        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(parentEntity);
        const aiScene* scene = model->GetAssimpScene();

        // Track which children we've matched to meshes
        std::unordered_set<EntityID> matchedChildren;

        // Process each mesh in the model
        const auto& meshes = model->GetMeshes();
        for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex) {
            const auto& meshData = meshes[meshIndex];
            const std::string& meshID = meshData.meshID;
            const std::string expectedChildName = "Mesh_" + meshID;

            // Try to find existing child with matching name
            EntityID childEntity = 0;
            for (EntityID child : hierarchy.children) {
                if (ecs.HasComponent<ObjectMetaData>(child)) {
                    auto& metadata = ecs.GetComponent<ObjectMetaData>(child);
                    if (metadata.name == expectedChildName) {
                        childEntity = child;
                        matchedChildren.insert(child);
                        break;
                    }
                }
            }

            // If no matching child found, create one
            if (childEntity == 0) {
                childEntity = ecs.CreateEntity();

                // Add required components
                ecs.AddComponent<HierarchyComponent>(childEntity, HierarchyComponent());
                ecs.AddComponent<Transform>(childEntity, Transform());
                ecs.AddComponent<ObjectMetaData>(childEntity, ObjectMetaData(expectedChildName, "Mesh", true));

                // Set parent-child relationship
                hierarchySystem->SetParent(childEntity, parentEntity, true);
            }

            // Get material index and mesh name from aiScene using mesh index
            uint32_t materialIndex = UINT32_MAX;
            std::string meshName = meshID;
            if (scene && meshIndex < scene->mNumMeshes) {
                aiMesh* aiMsh = scene->mMeshes[meshIndex];
                if (aiMsh) {
                    if (aiMsh->mName.length > 0) {
                        meshName = aiMsh->mName.C_Str();
                    }
                    materialIndex = aiMsh->mMaterialIndex;
                }
            }

            aiMaterial* aiMat = nullptr;
            if (scene && materialIndex != UINT32_MAX && materialIndex < scene->mNumMaterials) {
                aiMat = scene->mMaterials[materialIndex];
            }

            // Create or load a shared material asset for this mesh (mesh-name first, meshID fallback)
            Guid materialGuid{};
            auto& assets = AssetManager::GetInstance();
            const std::string sanitizedMeshName = AssetManager::SanitizeAssetName(meshName);
            const std::string sanitizedMeshID = AssetManager::SanitizeAssetName(meshID);
            const bool hasMeshName = !sanitizedMeshName.empty();
            const std::string primaryMaterialName = hasMeshName ? sanitizedMeshName : sanitizedMeshID;
            const std::filesystem::path materialsDir = std::filesystem::absolute("../Resources/Materials/");

            std::shared_ptr<graphics::Material> materialPtr;
            if (hasMeshName) {
                const std::filesystem::path meshNamePath = materialsDir / (sanitizedMeshName + ".mat");
                if (std::filesystem::exists(meshNamePath)) {
                    materialGuid = assets.GetMaterialGuidForPath(meshNamePath.string());
                    materialPtr = assets.LoadMaterialAsset(meshNamePath.string(), false);
                }
            }

            if (!materialPtr && !sanitizedMeshID.empty() && primaryMaterialName != sanitizedMeshID) {
                const std::filesystem::path meshIDPath = materialsDir / (sanitizedMeshID + ".mat");
                if (std::filesystem::exists(meshIDPath)) {
                    materialGuid = assets.GetMaterialGuidForPath(meshIDPath.string());
                    materialPtr = assets.LoadMaterialAsset(meshIDPath.string(), false);
                }
            }

            if (!materialPtr) {
                materialPtr = AssetManager::GetInstance().CreateMaterialAsset(
                    primaryMaterialName,
                    [&](graphics::Material& material) {
                    auto tryLoadTexture = [&](aiTextureType type, const char* slot, const char* hasFlag) {
                        if (!aiMat)
                            return false;

                        aiString texPath;
                        if (aiMat->GetTexture(type, 0, &texPath) != AI_SUCCESS)
                            return false;

                        std::string texPathStr = std::string(texPath.C_Str());
                        std::replace(texPathStr.begin(), texPathStr.end(), '\\', '/');
                        const auto lastSlash = texPathStr.find_last_of('/');
                        const std::string filename = (lastSlash != std::string::npos) ? texPathStr.substr(lastSlash + 1) : texPathStr;
                        auto tex = assets.LoadTexture("../Resources/Textures/" + filename);
                        if (!tex || !tex->IsValid())
                            return false;

                        material.SetTexture(slot, tex);
                        if (hasFlag)
                            material.SetBool(hasFlag, true);
                        return true;
                    };

                    if (!aiMat) {
                        material.SetVec4("materialAlbedo", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
                        material.SetFloat("materialMetallic", 0.0f);
                        material.SetFloat("materialRoughness", 0.5f);
                        material.SetFloat("materialAo", 1.0f);
                        material.SetVec3("materialEmissive", Vec3(0.0f, 0.0f, 0.0f));
                        material.SetFloat("materialEmissiveIntensity", 0.0f);
                        material.SetFloat("materialAlpha", 1.0f);
                        material.SetFloat("materialTransparency", 0.0f);
                        material.SetUVScale(Vec2(1.0f, 1.0f));
                        material.SetUVOffset(Vec2(0.0f, 0.0f));
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

                    // Scalar/color values
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

                    // Fetch UV transform (UVs are already flipped at import)
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
                    &materialGuid
                );
            }
            if (!materialPtr) {
                EE_CORE_WARN("Failed to create/load material asset for mesh '{}'", meshID);
                continue;
            }

            // Add or update material component on child entity
            if (ecs.HasComponent<Ermine::Material>(childEntity)) {
                // Update existing material
                auto& matComp = ecs.GetComponent<Ermine::Material>(childEntity);
                matComp = Ermine::Material(materialPtr, materialGuid);
            }
            else {
                // Add new material component
                ecs.AddComponent<Ermine::Material>(childEntity, Ermine::Material(materialPtr, materialGuid));
            }
        }

        // Delete orphaned children (children with "Mesh_" prefix that don't match any current mesh)
        std::vector<EntityID> childrenToDelete;
        for (EntityID child : hierarchy.children) {
            if (matchedChildren.find(child) == matchedChildren.end()) {
                // This child wasn't matched, check if it's a mesh child
                if (ecs.HasComponent<ObjectMetaData>(child)) {
                    auto& metadata = ecs.GetComponent<ObjectMetaData>(child);
                    // Only delete if it follows the "Mesh_" naming convention
                    if (metadata.name.rfind("Mesh_", 0) == 0) {
                        childrenToDelete.push_back(child);
                    }
                }
            }
        }

        // Delete orphaned mesh children
        for (EntityID child : childrenToDelete) {
            ecs.DestroyEntity(child);
        }
    }

}
