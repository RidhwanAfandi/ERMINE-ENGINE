/* Start Header ************************************************************************/
/*!
\file       VulkanTypes.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Shared Vulkan types, constants, and utility macros for the ERMINE-ENGINE
            Vulkan renderer backend.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once

// ─── Vulkan SDK ────────────────────────────────────────────────────────────────
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#include <vulkan/vulkan.h>

// ─── GLM (column-major, Vulkan clip space) ─────────────────────────────────────
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan uses [0,1] depth instead of [-1,1]
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

// ─── Standard includes used throughout Vulkan backend ──────────────────────────
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <functional>
#include <memory>
#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <span>

namespace Ermine::Vulkan
{

// ─── Frame-in-flight count (triple buffering) ──────────────────────────────────
static constexpr uint32_t FRAMES_IN_FLIGHT = 3;

// ─── G-Buffer attachment count (matches OpenGL GBuffer) ────────────────────────
static constexpr uint32_t GBUFFER_ATTACHMENT_COUNT = 4;

// ─── Descriptor set layout indices ─────────────────────────────────────────────
//  Set 0 – Per-frame: camera UBO, lights UBO, probes UBO
//  Set 1 – Per-pass : G-buffer textures, depth, shadow map array
//  Set 2 – Per-draw : material SSBO, texture array, skeletal SSBO
//  Set 3 – Global   : draw commands SSBO, draw info SSBO (indirect)
static constexpr uint32_t DESC_SET_PER_FRAME  = 0;
static constexpr uint32_t DESC_SET_PER_PASS   = 1;
static constexpr uint32_t DESC_SET_PER_DRAW   = 2;
static constexpr uint32_t DESC_SET_GLOBAL     = 3;

// ─── Binding points (mirror SSBO_Bindings.h for cross-API parity) ──────────────
// Note: Vulkan descriptor bindings are per-set, so these are within each set.
namespace Bindings
{
    // Set 0 (per-frame)
    static constexpr uint32_t CAMERA_UBO        = 0;
    static constexpr uint32_t LIGHTS_UBO        = 1;
    static constexpr uint32_t PROBES_UBO        = 2;
    static constexpr uint32_t SHADOW_VIEWS_SSBO = 3;

    // Set 1 (per-pass – G-buffer read)
    static constexpr uint32_t GBUFFER_POS       = 0;
    static constexpr uint32_t GBUFFER_NORMAL    = 1;
    static constexpr uint32_t GBUFFER_MATERIAL  = 2;
    static constexpr uint32_t GBUFFER_EXTRA     = 3;
    static constexpr uint32_t GBUFFER_DEPTH     = 4;
    static constexpr uint32_t SHADOW_MAP_ARRAY  = 5;

    // Set 2 (per-draw)
    static constexpr uint32_t MATERIAL_SSBO     = 0;
    static constexpr uint32_t TEXTURE_SSBO      = 1;
    static constexpr uint32_t SKELETAL_SSBO     = 2;

    // Set 3 (global / indirect)
    static constexpr uint32_t DRAW_COMMANDS     = 0;
    static constexpr uint32_t DRAW_INFO         = 1;
    static constexpr uint32_t VERTEX_SSBO       = 2;
    static constexpr uint32_t INDEX_SSBO        = 3;
} // namespace Bindings

// ─── Push constant layout ──────────────────────────────────────────────────────
// 128 bytes total (guaranteed by Vulkan spec minimum of 128 bytes)
struct PushConstants
{
    uint32_t  drawIndex;        // Index into DrawInfo SSBO
    uint32_t  materialIndex;    // Index into Material SSBO
    uint32_t  skeletalIndex;    // Index into Skeletal SSBO (0 if static)
    uint32_t  flags;            // Bitfield: bit0=skinned, bit1=transparent, bit2=custom
    glm::mat4 modelMatrix;      // Per-draw model matrix (if not using indirect SSBO)
};
static_assert(sizeof(PushConstants) <= 128, "Push constants exceed 128 bytes");

// ─── Per-frame camera UBO ──────────────────────────────────────────────────────
// std140 layout — matches the GLSL uniform block
struct CameraUBO
{
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::mat4 invView;
    glm::mat4 invProjection;
    glm::mat4 prevViewProjection; // For motion blur
    glm::vec4 cameraPosition;     // xyz = pos, w = near plane
    glm::vec4 cameraDirection;    // xyz = dir, w = far plane
    glm::vec2 resolution;         // Framebuffer size
    glm::vec2 invResolution;
    float     elapsedTime;
    float     deltaTime;
    uint32_t  frameIndex;
    float     _pad;
};

// ─── Queue families ────────────────────────────────────────────────────────────
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;  // Dedicated transfer queue

    bool IsComplete() const
    {
        return graphicsFamily.has_value() &&
               presentFamily.has_value()  &&
               computeFamily.has_value();
    }

    bool HasDedicatedTransfer() const { return transferFamily.has_value(); }
};

// ─── Swapchain support details ─────────────────────────────────────────────────
struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;

    bool IsAdequate() const
    {
        return !formats.empty() && !presentModes.empty();
    }

    bool SupportsMailbox() const
    {
        for (const auto& mode : presentModes)
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return true;
        return false;
    }
};

// ─── Render pass identifiers ──────────────────────────────────────────────────
enum class RenderPassID : uint8_t
{
    DepthPrePass    = 0,
    ShadowMap       = 1,
    Geometry        = 2,    // G-buffer fill
    Lighting        = 3,    // Deferred lighting
    ForwardOpaque   = 4,    // Custom shader opaque pass
    ForwardTransp   = 5,    // Transparent objects
    PostProcess     = 6,    // Bloom, FXAA, tone map, etc.
    UI              = 7,
    Picking         = 8,
    OutlineMask     = 9,
    COUNT
};

// ─── Image layout helpers ──────────────────────────────────────────────────────
struct ImageBarrier
{
    VkImage              image;
    VkImageLayout        oldLayout;
    VkImageLayout        newLayout;
    VkAccessFlags        srcAccess;
    VkAccessFlags        dstAccess;
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    VkImageAspectFlags   aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t             baseMip    = 0;
    uint32_t             mipCount   = 1;
    uint32_t             baseLayer  = 0;
    uint32_t             layerCount = 1;
};

// ─── Error handling ────────────────────────────────────────────────────────────

#ifdef EE_DEBUG
#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            char _buf[256];                                                     \
            ::snprintf(_buf, sizeof(_buf),                                      \
                "[VK_CHECK] %s:%d  expr='%s'  result=%d",                      \
                __FILE__, __LINE__, #expr, static_cast<int>(_vkr));             \
            throw std::runtime_error(_buf);                                     \
        }                                                                       \
    } while(0)
#else
#define VK_CHECK(expr)                                                          \
    do {                                                                        \
        VkResult _vkr = (expr);                                                 \
        if (_vkr != VK_SUCCESS) {                                               \
            throw std::runtime_error("Vulkan call failed: " #expr);             \
        }                                                                       \
    } while(0)
#endif

// Soft check – logs and returns false instead of throwing
#define VK_CHECK_SOFT(expr, ...) \
    ([&]() -> bool { \
        VkResult _vkr = (expr); \
        if (_vkr != VK_SUCCESS) { \
            EE_CORE_ERROR("Vulkan: " #expr " failed ({})", static_cast<int>(_vkr)); \
            __VA_ARGS__; \
            return false; \
        } \
        return true; \
    }())

// ─── Utility: byte size helpers ─────────────────────────────────────────────────
constexpr VkDeviceSize KB(uint64_t n) { return n * 1024ULL; }
constexpr VkDeviceSize MB(uint64_t n) { return n * 1024ULL * 1024ULL; }
constexpr VkDeviceSize GB(uint64_t n) { return n * 1024ULL * 1024ULL * 1024ULL; }

// ─── Utility: round up to alignment ────────────────────────────────────────────
constexpr VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace Ermine::Vulkan
