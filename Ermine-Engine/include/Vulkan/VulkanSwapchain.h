/* Start Header ************************************************************************/
/*!
\file       VulkanSwapchain.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Triple-buffered Vulkan swapchain with per-frame synchronization.

            Design:
            ┌─────────────────────────────────────────────────────────────────┐
            │  Triple buffering (FRAMES_IN_FLIGHT = 3)                        │
            │                                                                 │
            │  Frame 0 ──┐                                                    │
            │  Frame 1 ──┤── Swapchain images (typically 3 with MAILBOX)     │
            │  Frame 2 ──┘                                                    │
            │                                                                 │
            │  Per frame:                                                     │
            │    • CommandBuffer   – records this frame's draw calls          │
            │    • imageAvailable  – semaphore: swapchain image acquired      │
            │    • renderFinished  – semaphore: rendering complete            │
            │    • inFlightFence   – CPU waits here before reusing frame      │
            └─────────────────────────────────────────────────────────────────┘

            Present mode priority (checked at runtime via VulkanDeviceCapabilities):
              1. VK_PRESENT_MODE_MAILBOX_KHR   — triple buffer, low latency
              2. VK_PRESENT_MODE_FIFO_KHR      — vsync fallback (guaranteed)

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanContext.h"
#include <vector>
#include <array>

namespace Ermine::Vulkan
{

// ─── Per-frame synchronization objects ────────────────────────────────────────
struct FrameData
{
    VkCommandBuffer  commandBuffer   = VK_NULL_HANDLE;
    VkSemaphore      imageAvailable  = VK_NULL_HANDLE;  // GPU: swapchain image ready
    VkSemaphore      renderFinished  = VK_NULL_HANDLE;  // GPU: frame fully rendered
    VkFence          inFlightFence   = VK_NULL_HANDLE;  // CPU: frame resources free

    // Per-frame UBO staging (CPU-writable camera matrices, etc.)
    // Invalidated each frame; mapped persistently for zero-copy writes.
    VkBuffer         uniformBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory   uniformMemory   = VK_NULL_HANDLE;
    void*            uniformMapped   = nullptr;  // Persistent mapping

    bool IsValid() const { return commandBuffer != VK_NULL_HANDLE; }
};

// ─────────────────────────────────────────────────────────────────────────────
// VulkanSwapchain
// ─────────────────────────────────────────────────────────────────────────────

class VulkanSwapchain
{
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() { Shutdown(); }

    VulkanSwapchain(const VulkanSwapchain&)            = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Create the swapchain and all per-frame resources.
     * @param ctx       Initialized VulkanContext.
     * @param width     Initial framebuffer width.
     * @param height    Initial framebuffer height.
     */
    void Init(VulkanContext& ctx, uint32_t width, uint32_t height);

    /**
     * @brief Recreate the swapchain (called on window resize or VK_ERROR_OUT_OF_DATE_KHR).
     *        Waits for device idle, destroys old swapchain, then creates a new one.
     */
    void Recreate(uint32_t newWidth, uint32_t newHeight);

    /**
     * @brief Destroy all swapchain resources.
     */
    void Shutdown();

    // ── Per-frame interface ───────────────────────────────────────────────────

    /**
     * @brief Wait for the current frame's fence (CPU throttle) and acquire a
     *        swapchain image.
     *
     *        Returns false if the swapchain is out-of-date (triggers Recreate).
     *        Returns true when the image index is valid and recording can begin.
     *
     * @param[out] imageIndex  Index of the acquired swapchain image.
     */
    bool AcquireNextImage(uint32_t& imageIndex);

    /**
     * @brief Submit the current frame's command buffer and present.
     *
     *        Waits on imageAvailable, signals renderFinished.
     *        Returns false if swapchain is out-of-date (caller must Recreate).
     *
     * @param imageIndex   Image index from AcquireNextImage.
     */
    bool SubmitAndPresent(uint32_t imageIndex);

    // ── Frame data accessors ──────────────────────────────────────────────────

    FrameData&       GetCurrentFrame()       { return m_Frames[m_CurrentFrame]; }
    const FrameData& GetCurrentFrame() const { return m_Frames[m_CurrentFrame]; }

    uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }

    /**
     * @brief Advance to the next frame (call at the end of each frame).
     */
    void AdvanceFrame() { m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT; }

    // ── Swapchain image accessors ─────────────────────────────────────────────

    VkSwapchainKHR  GetSwapchain()    const { return m_Swapchain;    }
    VkFormat        GetImageFormat()  const { return m_ImageFormat;  }
    VkExtent2D      GetExtent()       const { return m_Extent;       }
    uint32_t        GetImageCount()   const { return static_cast<uint32_t>(m_Images.size()); }

    VkImage     GetImage(uint32_t i)     const { return m_Images[i];     }
    VkImageView GetImageView(uint32_t i) const { return m_ImageViews[i]; }

    const std::vector<VkImage>&     GetImages()     const { return m_Images;     }
    const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

    bool IsValid() const { return m_Swapchain != VK_NULL_HANDLE; }

    /**
     * @brief Present mode currently in use.
     */
    VkPresentModeKHR GetPresentMode() const { return m_PresentMode; }

    bool IsTripleBuffered() const { return m_PresentMode == VK_PRESENT_MODE_MAILBOX_KHR; }

    // ── Depth ─────────────────────────────────────────────────────────────────

    VkImage     GetDepthImage()     const { return m_DepthImage;     }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    VkFormat    GetDepthFormat()    const { return m_DepthFormat;    }

private:
    // ── Internal creation helpers ─────────────────────────────────────────────

    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateImageViews();
    void CreateDepthResources();
    void CreateFrameSyncObjects();
    void CreatePerFrameUniformBuffers();

    void DestroySwapchainResources();
    void DestroyFrameSyncObjects();
    void DestroyPerFrameUniformBuffers();

    // ── Format / mode selection ───────────────────────────────────────────────

    VkSurfaceFormatKHR ChooseSurfaceFormat() const;
    VkPresentModeKHR   ChoosePresentMode()   const;
    VkExtent2D         ChooseExtent(uint32_t width, uint32_t height) const;

    // ── State ─────────────────────────────────────────────────────────────────

    VulkanContext*    m_Ctx       = nullptr;
    VkSwapchainKHR    m_Swapchain = VK_NULL_HANDLE;

    VkFormat          m_ImageFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D        m_Extent       = {};
    VkPresentModeKHR  m_PresentMode  = VK_PRESENT_MODE_FIFO_KHR;

    std::vector<VkImage>     m_Images;
    std::vector<VkImageView> m_ImageViews;

    // Depth attachment (shared across all swapchain images)
    VkImage        m_DepthImage      = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory     = VK_NULL_HANDLE;
    VkImageView    m_DepthImageView  = VK_NULL_HANDLE;
    VkFormat       m_DepthFormat     = VK_FORMAT_UNDEFINED;

    // Per-frame resources (triple buffered)
    std::array<FrameData, FRAMES_IN_FLIGHT> m_Frames{};
    uint32_t m_CurrentFrame = 0;

    // Track which swapchain images are currently in-flight
    std::vector<VkFence> m_ImagesInFlight;
};

} // namespace Ermine::Vulkan
