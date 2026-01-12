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
#include "GHOST_Types.h"

class GHOST_XrGraphicsBindingVulkan : public GHOST_IXrGraphicsBinding {
  struct {
    /* XR_KHR_vulkan_enable2 */
    PFN_xrGetVulkanGraphicsRequirements2KHR xrGetVulkanGraphicsRequirements2KHR = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR xrGetVulkanGraphicsDevice2KHR = nullptr;
    PFN_xrCreateVulkanInstanceKHR xrCreateVulkanInstanceKHR = nullptr;
    PFN_xrCreateVulkanDeviceKHR xrCreateVulkanDeviceKHR = nullptr;
  } functions_;

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

  /**
   * Single VkCommandBuffer that is used for all views/swap-chains.
   *
   * This can be improved by having a single command buffer per swap-chain image.
   */
  VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
};
