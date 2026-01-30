/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <list>

#define VMA_VULKAN_VERSION 1002000  // Vulkan 1.2
#if !defined(_WIN32) or defined(_M_ARM64)
/* Silence compilation warning on non-windows x64 systems. */
#  define VMA_EXTERNAL_MEMORY_WIN32 0
#endif
#include "vk_mem_alloc.h"

#include "GHOST_ContextVK.hh"
#include "GHOST_IXrGraphicsBinding.hh"
#include "GHOST_Types.hh"

/**
 * \brief Graphics binding between OpenXR and Vulkan
 *
 * OpenXR and Blender can have different requirements for a Vulkan device to be used.
 * Some platforms require extensions that Blender didn't enable and other want to use
 * a specific device.
 *
 * Depending on the enabled extensions and device selection process one of the next three
 * behaviors can be done.
 *
 * \note See #152482 for the initial design.
 *
 * **Reusing Blender Vulkan Instance**
 *
 * Blender tries to reuse the already created vulkan instance of GHOST_ContextVK. This is only
 * possible when:
 *
 * - XR_KHR_vulkan_enable is supported by the Xr platform
 * - xrGetVulkanGraphicsDeviceKHR returns the same device
 * - extensions returned by xrGetVulkanInstanceExtensionsKHR are all enabled by GHOST_ContextVK
 * - extensions returned by xrGetVulkanDeviceExtensionsKHR are all enabled by GHOST_ContextVK
 *
 * This is typically the fastest way as almost everything can be done using Blenders' rendergraph.
 *
 * **Create a OpenXR Vulkan Instance**
 *
 * When we cannot reuse the existing instance an Xr specific Vulkan instance is being created. The
 * data is transferred using external memory. Additional synchronization is needed as part of the
 * pipeline cannot be done by Blender.
 *
 * - XR_KHR_vulkan_enable2 is supported by the Xr platform
 * - Created physical device is the same as GHOST_ContextVK
 *
 * **Use CPU transfer**
 *
 * This is a fallback when devices don't match. In that case the results are transferred from the
 * device that Blender is using to draw the UI to RAM and then transferred to the GPU that OpenXR
 * is using.
 */
class GHOST_XrGraphicsBindingVulkan : public GHOST_IXrGraphicsBinding {
 public:
  GHOST_XrGraphicsBindingVulkan(GHOST_Context &ghost_ctx);
  ~GHOST_XrGraphicsBindingVulkan() override;

  bool loadExtensionFunctions(XrInstance instance) override;

  /**
   * Check the version requirements to use OpenXR with the Vulkan backend.
   */
  bool checkVersionRequirements(GHOST_Context &ghost_ctx,
                                XrInstance instance,
                                XrSystemId system_id,
                                std::string *r_requirement_info) const override;

  void initFromGhostContext(GHOST_Context &ghost_ctx,
                            XrInstance instance,
                            XrSystemId system_id) override;
  std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                               GHOST_TXrSwapchainFormat &r_format,
                                               bool &r_is_srgb_format) const override;
  std::vector<XrSwapchainImageBaseHeader *> createSwapchainImages(uint32_t image_count) override;

  void submitToSwapchainBegin() override;
  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override;
  void submitToSwapchainEnd() override;

  bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const override;

 private:
  struct {
    /* XR_KHR_vulkan_enable */
    PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR = nullptr;
    PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR = nullptr;
    PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR = nullptr;
    PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR = nullptr;

    /* XR_KHR_vulkan_enable2 */
    PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = nullptr;
    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = nullptr;
    PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR = nullptr;
  } functions_;

  struct {
    /** Is XK_KHR_vulkan_enable extension available */
    bool vulkan_enable = false;
    /** Is XK_KHR_vulkan_enable2 extension available */
    bool vulkan_enable2 = false;
  } extensions_;

  GHOST_ContextVK &ghost_ctx_;

  VkInstance vk_instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice vk_physical_device_ = VK_NULL_HANDLE;
  uint32_t graphics_queue_family_ = 0;
  VkQueue vk_queue_ = VK_NULL_HANDLE;
  VkDevice vk_device_ = VK_NULL_HANDLE;
  VmaAllocator vma_allocator_ = VK_NULL_HANDLE;
  VmaAllocation vk_buffer_allocation_ = VK_NULL_HANDLE;
  VkBuffer vk_buffer_ = VK_NULL_HANDLE;
  VmaAllocationInfo vk_buffer_allocation_info_ = {};
  GHOST_TVulkanXRModes data_transfer_mode_ = GHOST_kVulkanXRModeCPU;

  std::list<std::vector<XrSwapchainImageVulkan2KHR>> image_cache_;
  VkCommandPool vk_command_pool_ = VK_NULL_HANDLE;

  struct ImportedMemory {
    char view_idx;
    VkImage vk_image_blender;
    VkImage vk_image_xr;
    VkDeviceMemory vk_device_memory_xr;
  };
  std::vector<ImportedMemory> imported_memory_;

  GHOST_TVulkanXRModes choseDataTransferMode();
  void submitToSwapchainImageCpu(XrSwapchainImageVulkan2KHR &swapchain_image,
                                 const GHOST_XrDrawViewInfo &draw_info);
  void submitToSwapchainImageGpu(XrSwapchainImageVulkan2KHR &swapchain_image,
                                 const GHOST_XrDrawViewInfo &draw_info);
  void submitToSwapchainImageRenderGraph(XrSwapchainImageVulkan2KHR &swapchain_image,
                                         const GHOST_XrDrawViewInfo &draw_info);

  /**
   * Single VkCommandBuffer that is used for all views/swap-chains.
   *
   * This can be improved by having a single command buffer per swap-chain image.
   */
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;

  /**
   * \brief try to reuse GHOST context vulkan instance.
   *
   * \returns true when blenders' Vulkan instance will be reused, false when not.
   */
  bool tryReuseVulkanInstance(GHOST_ContextVK &ghost_ctx, XrInstance instance, XrSystemId system);
  bool areRequiredInstanceExtensionsEnabled(XrInstance instance, XrSystemId system_id) const;
  bool areRequiredDeviceExtensionsEnabled(XrInstance instance, XrSystemId system_id) const;
  bool isSamePhysicalDeviceSelected(XrInstance instance,
                                    XrSystemId system_id,
                                    const GHOST_VulkanHandles &context_handles) const;
};
