/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef WITH_VULKAN_BACKEND
#  error "ContextVK requires WITH_VULKAN_BACKEND"
#endif

#include "GHOST_Context.hh"

#ifdef _WIN32
#  include "GHOST_SystemWin32.hh"
#elif defined(__APPLE__)
#  include "GHOST_SystemCocoa.hh"
#else
#  ifdef WITH_GHOST_X11
#    include "GHOST_SystemX11.hh"
#  else
#    define Display void
#    define Window void *
#  endif
#  ifdef WITH_GHOST_WAYLAND
#    include "GHOST_SystemWayland.hh"
#  else
#    define wl_surface void
#    define wl_display void
#  endif
#endif

#include <map>
#include <optional>
#include <vector>

#ifndef GHOST_OPENGL_VK_CONTEXT_FLAGS
/* leave as convenience define for the future */
#  define GHOST_OPENGL_VK_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_VK_RESET_NOTIFICATION_STRATEGY 0
#endif

enum GHOST_TVulkanPlatformType {
  GHOST_kVulkanPlatformHeadless = 0,
#ifdef WITH_GHOST_X11
  GHOST_kVulkanPlatformX11 = 1,
#endif
#ifdef WITH_GHOST_WAYLAND
  GHOST_kVulkanPlatformWayland = 2,
#endif
};

struct GHOST_ContextVK_WindowInfo {
  int size[2];
};

struct GHOST_FrameDiscard {
  std::vector<VkSwapchainKHR> swapchains;
  std::vector<VkSemaphore> semaphores;

  void destroy(VkDevice vk_device);
};

struct GHOST_SwapchainImage {
  /** Swap-chain image (owned by the swapchain). */
  VkImage vk_image = VK_NULL_HANDLE;

  /**
   * Semaphore for presenting; being signaled when the swap-chain image is ready to be presented.
   */
  VkSemaphore present_semaphore = VK_NULL_HANDLE;

  void destroy(VkDevice vk_device);
};

struct GHOST_Frame {
  /**
   * Fence signaled when "previous" use of the frame has finished rendering. When signaled the
   * frame can acquire a new image and the semaphores can be reused.
   */
  VkFence submission_fence = VK_NULL_HANDLE;
  /** Semaphore for acquiring; being signaled when the swap-chain image is ready to be updated. */
  VkSemaphore acquire_semaphore = VK_NULL_HANDLE;

  GHOST_FrameDiscard discard_pile;

  void destroy(VkDevice vk_device);
};

class GHOST_ContextVK : public GHOST_Context {
  friend class GHOST_XrGraphicsBindingVulkan;
  friend class GHOST_XrGraphicsBindingVulkanD3D;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextVK(const GHOST_ContextParams &context_params,
#ifdef _WIN32
                  HWND hwnd,
#elif defined(__APPLE__)
                  /* FIXME CAMetalLayer but have issue with linking. */
                  void *metal_layer,
#else
                  GHOST_TVulkanPlatformType platform,
                  /* X11 */
                  Window window,
                  Display *display,
                  /* Wayland */
                  wl_surface *wayland_surface,
                  wl_display *wayland_display,
                  const GHOST_ContextVK_WindowInfo *wayland_window_info,
#endif
                  int contextMajorVersion,
                  int contextMinorVersion,
                  const GHOST_GPUDevice &preferred_device,
                  const GHOST_WindowHDRInfo *hdr_info_ = nullptr);

  /**
   * Destructor.
   */
  ~GHOST_ContextVK() override;

  /** \copydoc #GHOST_IContext::swapBuffersAcquire */
  GHOST_TSuccess swapBufferAcquire() override;
  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBufferRelease() override;

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext() override;

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext() override;

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  GHOST_TSuccess initializeDrawingContext() override;

  /**
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles() override;

  /**
   * Gets the Vulkan context related resource handles.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess getVulkanHandles(GHOST_VulkanHandles &r_handles) override;

  GHOST_TSuccess getVulkanSwapChainFormat(GHOST_VulkanSwapChainData *r_swap_chain_data) override;

  GHOST_TSuccess setVulkanSwapBuffersCallbacks(
      std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffer_draw_callback,
      std::function<void(void)> swap_buffer_acquired_callback,
      std::function<void(GHOST_VulkanOpenXRData *)> openxr_acquire_framebuffer_image_callback,
      std::function<void(GHOST_VulkanOpenXRData *)> openxr_release_framebuffer_image_callback)
      override;

  /**
   * Sets the swap interval for `swapBuffers`.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /*interval*/) override
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for swapBuffers.
   * \param interval_out: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int & /*interval_out*/) override
  {
    return GHOST_kFailure;
  };

  /**
   * Returns if the context is rendered upside down compared to OpenGL.
   *
   * Vulkan is always rendered upside down.
   */
  bool isUpsideDown() const override
  {
    return true;
  }

 private:
#ifdef _WIN32
  HWND hwnd_;
#elif defined(__APPLE__)
  CAMetalLayer *metal_layer_;
#else /* Linux */
  GHOST_TVulkanPlatformType platform_;
  /* X11 */
  Display *display_;
  Window window_;
  /* Wayland */
  wl_surface *wayland_surface_;
  wl_display *wayland_display_;
  const GHOST_ContextVK_WindowInfo *wayland_window_info_;
#endif

  const int context_major_version_;
  const int context_minor_version_;
  const GHOST_GPUDevice preferred_device_;

  /* Optional HDR info updated by window. */
  const GHOST_WindowHDRInfo *hdr_info_;

  /* For display only. */
  VkSurfaceKHR surface_;
  VkSwapchainKHR swapchain_;
  std::vector<GHOST_SwapchainImage> swapchain_images_;
  std::vector<GHOST_Frame> frame_data_;
  uint64_t render_frame_;
  uint64_t image_count_;

  VkExtent2D render_extent_;
  VkExtent2D render_extent_min_;
  VkSurfaceFormatKHR surface_format_;
  bool use_hdr_swapchain_;

  std::optional<uint32_t> acquired_swapchain_image_index_;

  std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffer_draw_callback_;
  std::function<void(void)> swap_buffer_acquired_callback_;
  std::function<void(GHOST_VulkanOpenXRData *)> openxr_acquire_framebuffer_image_callback_;
  std::function<void(GHOST_VulkanOpenXRData *)> openxr_release_framebuffer_image_callback_;

  std::vector<VkFence> fence_pile_;
  std::map<VkSwapchainKHR, std::vector<VkFence>> present_fences_;

  const char *getPlatformSpecificSurfaceExtension() const;
  GHOST_TSuccess recreateSwapchain(bool use_hdr_swapchain);
  GHOST_TSuccess initializeFrameData();
  GHOST_TSuccess destroySwapchain();

  VkFence getFence();
  void setPresentFence(VkSwapchainKHR swapchain, VkFence fence);
  void destroySwapchainPresentFences(VkSwapchainKHR swapchain);
};
