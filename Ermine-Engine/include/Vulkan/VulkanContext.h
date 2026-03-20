/* Start Header ************************************************************************/
/*!
\file       VulkanContext.h
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Vulkan context: instance, surface, physical device selection,
            logical device, and command pool management.

            VulkanContext owns the Vulkan instance lifetime. All other Vulkan
            objects hold a non-owning pointer to the context.

            Usage:
              VulkanContext ctx;
              ctx.Init("ERMINE-ENGINE", GLFW_window_handle);
              // ... render ...
              ctx.Shutdown();

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#pragma once
#include "VulkanTypes.h"
#include "VulkanDeviceCapabilities.h"
#include <functional>
#include <string>
#include <vector>

// Forward declare GLFW window to avoid pulling in GLFW from every header
struct GLFWwindow;

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// VulkanContext
// ─────────────────────────────────────────────────────────────────────────────

class VulkanContext
{
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    VulkanContext() = default;
    ~VulkanContext() { Shutdown(); }

    // Non-copyable, movable
    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&)                 = default;
    VulkanContext& operator=(VulkanContext&&)      = default;

    /**
     * @brief Initialize Vulkan: instance → surface → physical device → logical device.
     * @param appName    Application name embedded in VkApplicationInfo.
     * @param window     GLFW window handle for surface creation.
     * @param enableValidation  Enable VK_LAYER_KHRONOS_validation (debug builds).
     */
    void Init(const std::string& appName, GLFWwindow* window, bool enableValidation = false);

    /**
     * @brief Destroy all Vulkan objects in reverse creation order.
     *        Safe to call multiple times.
     */
    void Shutdown();

    // ── Core Vulkan handles ───────────────────────────────────────────────────

    VkInstance       GetInstance()       const { return m_Instance;    }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysDevice;  }
    VkDevice         GetDevice()         const { return m_Device;      }
    VkSurfaceKHR     GetSurface()        const { return m_Surface;     }

    // ── Queue handles ─────────────────────────────────────────────────────────

    VkQueue  GetGraphicsQueue()  const { return m_GraphicsQueue;  }
    VkQueue  GetPresentQueue()   const { return m_PresentQueue;   }
    VkQueue  GetComputeQueue()   const { return m_ComputeQueue;   }
    VkQueue  GetTransferQueue()  const { return m_TransferQueue;  }

    // ── Command pools ─────────────────────────────────────────────────────────

    /**
     * @brief Command pool for main-thread graphics commands.
     *        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT set.
     */
    VkCommandPool GetGraphicsCommandPool() const { return m_GraphicsCmdPool; }

    /**
     * @brief Command pool for async compute commands.
     */
    VkCommandPool GetComputeCommandPool()  const { return m_ComputeCmdPool;  }

    /**
     * @brief Command pool for DMA / staging uploads (dedicated transfer queue if available).
     */
    VkCommandPool GetTransferCommandPool() const { return m_TransferCmdPool; }

    // ── Capabilities ──────────────────────────────────────────────────────────

    const VulkanDeviceCapabilities& GetCapabilities() const { return m_Caps; }
          VulkanDeviceCapabilities& GetCapabilities()       { return m_Caps; }

    /**
     * @brief Convenience: adaptive buffer sizes derived from device limits.
     */
    const AdaptiveConfig& GetAdaptiveConfig() const { return m_AdaptiveConfig; }

    // ── Immediate submit helper ───────────────────────────────────────────────

    /**
     * @brief Allocate a transient command buffer, record commands, submit, and wait.
     *        Use for one-shot uploads and image layout transitions.
     *        Blocks the CPU until the GPU completes.
     *
     * @param pool    Command pool to allocate from (use GetTransferCommandPool for uploads).
     * @param queue   Queue to submit to.
     * @param fn      Lambda receiving the recorded command buffer.
     */
    void ImmediateSubmit(VkCommandPool pool, VkQueue queue,
                         std::function<void(VkCommandBuffer)> fn) const;

    /**
     * @brief Shorthand: immediate submit on the graphics queue.
     */
    void ImmediateGraphicsSubmit(std::function<void(VkCommandBuffer)> fn) const
    {
        ImmediateSubmit(m_GraphicsCmdPool, m_GraphicsQueue, std::move(fn));
    }

    /**
     * @brief Shorthand: immediate submit on the transfer queue (or graphics if no dedicated).
     */
    void ImmediateTransferSubmit(std::function<void(VkCommandBuffer)> fn) const
    {
        VkCommandPool pool  = m_TransferCmdPool  ? m_TransferCmdPool  : m_GraphicsCmdPool;
        VkQueue       queue = m_TransferQueue    ? m_TransferQueue    : m_GraphicsQueue;
        ImmediateSubmit(pool, queue, std::move(fn));
    }

    // ── Utility ───────────────────────────────────────────────────────────────

    /**
     * @brief Insert a debug label into a command buffer (visible in RenderDoc/NSight).
     *        No-op in release builds.
     */
    void BeginDebugLabel(VkCommandBuffer cmd, const char* label,
                         float r = 1.f, float g = 1.f, float b = 1.f) const;
    void EndDebugLabel(VkCommandBuffer cmd) const;
    void InsertDebugLabel(VkCommandBuffer cmd, const char* label) const;

    /**
     * @brief Set a debug name on any Vulkan object (visible in RenderDoc).
     *        No-op if VK_EXT_debug_utils not available.
     */
    void SetDebugName(VkObjectType type, uint64_t handle, const char* name) const;

    bool IsValid() const { return m_Device != VK_NULL_HANDLE; }

private:
    // ── Initialization sub-steps ──────────────────────────────────────────────

    void CreateInstance(const std::string& appName, bool enableValidation);
    void SetupDebugMessenger();
    void CreateSurface(GLFWwindow* window);
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateCommandPools();
    void LoadExtensionFunctions();

    // ── Physical device scoring ───────────────────────────────────────────────

    int  ScorePhysicalDevice(VkPhysicalDevice dev) const;
    bool IsDeviceSuitable(VkPhysicalDevice dev) const;

    // ── Validation layer helpers ───────────────────────────────────────────────

    static bool CheckValidationLayerSupport();
    static std::vector<const char*> GetRequiredInstanceExtensions(bool enableValidation);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*                                       pUserData);

    // ── State ─────────────────────────────────────────────────────────────────

    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_Surface        = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysDevice     = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;

    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue  = VK_NULL_HANDLE;
    VkQueue m_ComputeQueue  = VK_NULL_HANDLE;
    VkQueue m_TransferQueue = VK_NULL_HANDLE;

    VkCommandPool m_GraphicsCmdPool = VK_NULL_HANDLE;
    VkCommandPool m_ComputeCmdPool  = VK_NULL_HANDLE;
    VkCommandPool m_TransferCmdPool = VK_NULL_HANDLE;

    VulkanDeviceCapabilities m_Caps;
    AdaptiveConfig           m_AdaptiveConfig;
    bool                     m_ValidationEnabled = false;

    // Extension function pointers (loaded at runtime)
    PFN_vkSetDebugUtilsObjectNameEXT    m_pfnSetDebugName   = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT    m_pfnBeginLabel     = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT      m_pfnEndLabel       = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT   m_pfnInsertLabel    = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT  m_pfnCreateDebugMsg = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT m_pfnDestroyDebugMsg= nullptr;

    // Required device extensions
    static constexpr const char* k_RequiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_maintenance1",  // Negative viewport heights for Vulkan Y-flip
    };
};

} // namespace Ermine::Vulkan
