/* Start Header ************************************************************************/
/*!
\file       VulkanSwapchain.cpp
\author     Ridhwan Afandi, mohamedridhwan.b, 2301367, mohamedridhwan.b@digipen.edu
\date       2026/03/20
\brief      Triple-buffered Vulkan swapchain implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "Vulkan/VulkanSwapchain.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cstring>

namespace Ermine::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
// Init
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::Init(VulkanContext& ctx, uint32_t width, uint32_t height)
{
    m_Ctx = &ctx;

    CreateSwapchain(width, height);
    CreateImageViews();
    CreateDepthResources();
    CreateFrameSyncObjects();
    CreatePerFrameUniformBuffers();
}

// ─────────────────────────────────────────────────────────────────────────────
// Recreate
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::Recreate(uint32_t newWidth, uint32_t newHeight)
{
    // Skip zero-size swapchains (minimized window)
    if (newWidth == 0 || newHeight == 0) return;

    vkDeviceWaitIdle(m_Ctx->GetDevice());

    DestroySwapchainResources();

    CreateSwapchain(newWidth, newHeight);
    CreateImageViews();
    CreateDepthResources();
    // Sync objects and uniform buffers remain valid across recreates
}

// ─────────────────────────────────────────────────────────────────────────────
// Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::Shutdown()
{
    if (!m_Ctx || m_Ctx->GetDevice() == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(m_Ctx->GetDevice());

    DestroyPerFrameUniformBuffers();
    DestroyFrameSyncObjects();
    DestroySwapchainResources();

    m_Ctx = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// AcquireNextImage
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanSwapchain::AcquireNextImage(uint32_t& imageIndex)
{
    auto& frame = m_Frames[m_CurrentFrame];
    VkDevice device = m_Ctx->GetDevice();

    // Wait for this frame's previous submission to complete
    VK_CHECK(vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));

    VkResult result = vkAcquireNextImageKHR(
        device, m_Swapchain, UINT64_MAX,
        frame.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        return false;  // Caller must Recreate

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkAcquireNextImageKHR failed");

    // If a previous frame is still using this swapchain image, wait on it
    if (m_ImagesInFlight[imageIndex] != VK_NULL_HANDLE)
        VK_CHECK(vkWaitForFences(device, 1, &m_ImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX));
    m_ImagesInFlight[imageIndex] = frame.inFlightFence;

    // Reset the fence for this frame
    VK_CHECK(vkResetFences(device, 1, &frame.inFlightFence));

    // Reset and begin the command buffer
    VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SubmitAndPresent
// ─────────────────────────────────────────────────────────────────────────────

bool VulkanSwapchain::SubmitAndPresent(uint32_t imageIndex)
{
    auto& frame = m_Frames[m_CurrentFrame];
    VkDevice device = m_Ctx->GetDevice();

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(frame.commandBuffer));

    // Submit: wait on imageAvailable, signal renderFinished
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &frame.imageAvailable;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &frame.commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &frame.renderFinished;

    VK_CHECK(vkQueueSubmit(m_Ctx->GetGraphicsQueue(), 1, &submitInfo, frame.inFlightFence));

    // Present: wait on renderFinished
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &frame.renderFinished;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_Swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_Ctx->GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        return false;  // Caller must Recreate

    if (result != VK_SUCCESS)
        throw std::runtime_error("vkQueuePresentKHR failed");

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateSwapchain
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height)
{
    const auto& support = m_Ctx->GetCapabilities().GetSwapchainSupport();
    const auto& qf      = m_Ctx->GetCapabilities().GetQueueFamilies();

    auto surfaceFormat = ChooseSurfaceFormat();
    auto presentMode   = ChoosePresentMode();
    auto extent        = ChooseExtent(width, height);

    m_ImageFormat = surfaceFormat.format;
    m_Extent      = extent;
    m_PresentMode = presentMode;

    // Triple buffering: request 3 images (or 2 minimum, whatever the device supports)
    uint32_t imageCount = FRAMES_IN_FLIGHT;
    imageCount = std::clamp(imageCount,
                             support.capabilities.minImageCount + 1,
                             support.capabilities.maxImageCount == 0
                                 ? std::numeric_limits<uint32_t>::max()
                                 : support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = m_Ctx->GetSurface();
    ci.minImageCount    = imageCount;
    ci.imageFormat      = surfaceFormat.format;
    ci.imageColorSpace  = surfaceFormat.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT;    // Allow blit to/from swapchain

    uint32_t queueFamilies[] = {
        qf.graphicsFamily.value(),
        qf.presentFamily.value()
    };

    if (qf.graphicsFamily.value() != qf.presentFamily.value())
    {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = queueFamilies;
    }
    else
    {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform   = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = presentMode;
    ci.clipped        = VK_TRUE;
    ci.oldSwapchain   = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_Ctx->GetDevice(), &ci, nullptr, &m_Swapchain));

    // Retrieve the actual swapchain images
    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(m_Ctx->GetDevice(), m_Swapchain, &actualCount, nullptr);
    m_Images.resize(actualCount);
    vkGetSwapchainImagesKHR(m_Ctx->GetDevice(), m_Swapchain, &actualCount, m_Images.data());

    m_ImagesInFlight.assign(actualCount, VK_NULL_HANDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateImageViews
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::CreateImageViews()
{
    m_ImageViews.resize(m_Images.size());
    for (size_t i = 0; i < m_Images.size(); ++i)
    {
        VkImageViewCreateInfo ci{};
        ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image                           = m_Images[i];
        ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ci.format                          = m_ImageFormat;
        ci.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel   = 0;
        ci.subresourceRange.levelCount     = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount     = 1;
        VK_CHECK(vkCreateImageView(m_Ctx->GetDevice(), &ci, nullptr, &m_ImageViews[i]));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateDepthResources
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::CreateDepthResources()
{
    m_DepthFormat = m_Ctx->GetCapabilities().FindDepthFormat();
    VkDevice device = m_Ctx->GetDevice();

    VkImageCreateInfo imgCI{};
    imgCI.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCI.imageType     = VK_IMAGE_TYPE_2D;
    imgCI.extent        = { m_Extent.width, m_Extent.height, 1 };
    imgCI.mipLevels     = 1;
    imgCI.arrayLayers   = 1;
    imgCI.format        = m_DepthFormat;
    imgCI.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgCI.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imgCI.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgCI.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateImage(device, &imgCI, nullptr, &m_DepthImage));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_DepthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Ctx->GetCapabilities().FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_DepthMemory));
    VK_CHECK(vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0));

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (m_DepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        m_DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT)
        aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkImageViewCreateInfo viewCI{};
    viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image                           = m_DepthImage;
    viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format                          = m_DepthFormat;
    viewCI.subresourceRange.aspectMask     = aspect;
    viewCI.subresourceRange.baseMipLevel   = 0;
    viewCI.subresourceRange.levelCount     = 1;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount     = 1;
    VK_CHECK(vkCreateImageView(device, &viewCI, nullptr, &m_DepthImageView));

    // Transition depth image to optimal layout
    m_Ctx->ImmediateGraphicsSubmit([&](VkCommandBuffer cmd)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = m_DepthImage;
        barrier.subresourceRange.aspectMask     = aspect;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateFrameSyncObjects
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::CreateFrameSyncObjects()
{
    VkDevice device = m_Ctx->GetDevice();

    VkSemaphoreCreateInfo semCI{};
    semCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled so first frame doesn't wait

    // Allocate command buffers from the graphics pool
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_Ctx->GetGraphicsCommandPool();
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = FRAMES_IN_FLIGHT;

    std::array<VkCommandBuffer, FRAMES_IN_FLIGHT> cmdBufs{};
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, cmdBufs.data()));

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_Frames[i].commandBuffer = cmdBufs[i];
        VK_CHECK(vkCreateSemaphore(device, &semCI,   nullptr, &m_Frames[i].imageAvailable));
        VK_CHECK(vkCreateSemaphore(device, &semCI,   nullptr, &m_Frames[i].renderFinished));
        VK_CHECK(vkCreateFence   (device, &fenceCI, nullptr, &m_Frames[i].inFlightFence));

        // Debug names
        m_Ctx->SetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER,
            reinterpret_cast<uint64_t>(m_Frames[i].commandBuffer),
            ("Frame[" + std::to_string(i) + "].CmdBuf").c_str());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CreatePerFrameUniformBuffers
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::CreatePerFrameUniformBuffers()
{
    VkDevice device = m_Ctx->GetDevice();
    const VkDeviceSize bufSize = m_Ctx->GetAdaptiveConfig().cameraUBOSize;

    constexpr VkMemoryPropertyFlags kHostProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        VkBufferCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size        = bufSize;
        ci.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &ci, nullptr, &m_Frames[i].uniformBuffer));

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_Frames[i].uniformBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Ctx->GetCapabilities().FindMemoryType(
            memReqs.memoryTypeBits, kHostProps);
        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &m_Frames[i].uniformMemory));
        VK_CHECK(vkBindBufferMemory(device, m_Frames[i].uniformBuffer,
                                     m_Frames[i].uniformMemory, 0));

        // Persistent mapping – avoids map/unmap overhead each frame
        VK_CHECK(vkMapMemory(device, m_Frames[i].uniformMemory, 0, bufSize, 0,
                              &m_Frames[i].uniformMapped));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DestroySwapchainResources
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::DestroySwapchainResources()
{
    VkDevice device = m_Ctx->GetDevice();

    if (m_DepthImageView) { vkDestroyImageView(device, m_DepthImageView, nullptr); m_DepthImageView = VK_NULL_HANDLE; }
    if (m_DepthImage)     { vkDestroyImage    (device, m_DepthImage,     nullptr); m_DepthImage     = VK_NULL_HANDLE; }
    if (m_DepthMemory)    { vkFreeMemory      (device, m_DepthMemory,    nullptr); m_DepthMemory    = VK_NULL_HANDLE; }

    for (auto view : m_ImageViews)
        if (view) vkDestroyImageView(device, view, nullptr);
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain)
    {
        vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DestroyFrameSyncObjects
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::DestroyFrameSyncObjects()
{
    VkDevice device = m_Ctx->GetDevice();
    for (auto& frame : m_Frames)
    {
        if (frame.imageAvailable) { vkDestroySemaphore(device, frame.imageAvailable, nullptr); frame.imageAvailable = VK_NULL_HANDLE; }
        if (frame.renderFinished) { vkDestroySemaphore(device, frame.renderFinished, nullptr); frame.renderFinished = VK_NULL_HANDLE; }
        if (frame.inFlightFence)  { vkDestroyFence    (device, frame.inFlightFence,  nullptr); frame.inFlightFence  = VK_NULL_HANDLE; }
        // Note: command buffers are freed with the pool
        frame.commandBuffer = VK_NULL_HANDLE;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DestroyPerFrameUniformBuffers
// ─────────────────────────────────────────────────────────────────────────────

void VulkanSwapchain::DestroyPerFrameUniformBuffers()
{
    VkDevice device = m_Ctx->GetDevice();
    for (auto& frame : m_Frames)
    {
        if (frame.uniformMapped)  { vkUnmapMemory  (device, frame.uniformMemory);                    frame.uniformMapped = nullptr; }
        if (frame.uniformBuffer)  { vkDestroyBuffer(device, frame.uniformBuffer, nullptr);           frame.uniformBuffer = VK_NULL_HANDLE; }
        if (frame.uniformMemory)  { vkFreeMemory   (device, frame.uniformMemory, nullptr);           frame.uniformMemory = VK_NULL_HANDLE; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// ChooseSurfaceFormat
// ─────────────────────────────────────────────────────────────────────────────

VkSurfaceFormatKHR VulkanSwapchain::ChooseSurfaceFormat() const
{
    const auto& formats = m_Ctx->GetCapabilities().GetSwapchainSupport().formats;

    // Prefer SRGB B8G8R8A8 (matches OpenGL backbuffer behavior)
    for (const auto& fmt : formats)
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return fmt;

    // Fallback to UNORM
    for (const auto& fmt : formats)
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM)
            return fmt;

    return formats[0];
}

// ─────────────────────────────────────────────────────────────────────────────
// ChoosePresentMode
// ─────────────────────────────────────────────────────────────────────────────

VkPresentModeKHR VulkanSwapchain::ChoosePresentMode() const
{
    const auto& modes = m_Ctx->GetCapabilities().GetSwapchainSupport().presentModes;

    // Prefer MAILBOX (triple buffer – tear-free and low-latency)
    if (m_Ctx->GetAdaptiveConfig().useMailboxPresent)
    {
        for (auto mode : modes)
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode;
    }

    // FIFO is always available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

// ─────────────────────────────────────────────────────────────────────────────
// ChooseExtent
// ─────────────────────────────────────────────────────────────────────────────

VkExtent2D VulkanSwapchain::ChooseExtent(uint32_t width, uint32_t height) const
{
    const auto& caps = m_Ctx->GetCapabilities().GetSwapchainSupport().capabilities;

    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return caps.currentExtent;

    VkExtent2D extent{
        std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)
    };
    return extent;
}

} // namespace Ermine::Vulkan
