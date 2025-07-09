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
  bool is_color_managed;
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
   * Semaphore for presenting; being signaled when the swap chain image is ready to be presented.
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
  /** Semaphore for acquiring; being signaled when the swap chain image is ready to be updated. */
  VkSemaphore acquire_semaphore = VK_NULL_HANDLE;

  GHOST_FrameDiscard discard_pile;

  void destroy(VkDevice vk_device);
};

/**
 * The number of frames that GHOST manages.
 *
 * This must be kept in sync with any frame-aligned resources in the
 * Vulkan backend. Notably, VKThreadData::resource_pools_count must
 * match this value.
 */
constexpr static uint32_t GHOST_FRAMES_IN_FLIGHT = 5;

class GHOST_ContextVK : public GHOST_Context {
  friend class GHOST_XrGraphicsBindingVulkan;
  friend class GHOST_XrGraphicsBindingVulkanD3D;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextVK(bool stereoVisual,
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
                  int debug,
                  const GHOST_GPUDevice &preferred_device);

  /**
   * Destructor.
   */
  ~GHOST_ContextVK() override;

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers() override;

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
      std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffers_pre_callback,
      std::function<void(void)> swap_buffers_post_callback,
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
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int & /*intervalOut*/) override
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
  HWND m_hwnd;
#elif defined(__APPLE__)
  CAMetalLayer *m_metal_layer;
#else /* Linux */
  GHOST_TVulkanPlatformType m_platform;
  /* X11 */
  Display *m_display;
  Window m_window;
  /* Wayland */
  wl_surface *m_wayland_surface;
  wl_display *m_wayland_display;
  const GHOST_ContextVK_WindowInfo *m_wayland_window_info;
#endif

  const int m_context_major_version;
  const int m_context_minor_version;
  const int m_debug;
  const GHOST_GPUDevice m_preferred_device;

  VkQueue m_graphic_queue;
  VkQueue m_present_queue;

  /* For display only. */
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  std::vector<GHOST_SwapchainImage> m_swapchain_images;
  std::vector<GHOST_Frame> m_frame_data;
  uint64_t m_render_frame;
  uint64_t m_image_count;

  VkExtent2D m_render_extent;
  VkExtent2D m_render_extent_min;
  VkSurfaceFormatKHR m_surface_format;

  std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffers_pre_callback_;
  std::function<void(void)> swap_buffers_post_callback_;
  std::function<void(GHOST_VulkanOpenXRData *)> openxr_acquire_framebuffer_image_callback_;
  std::function<void(GHOST_VulkanOpenXRData *)> openxr_release_framebuffer_image_callback_;

  const char *getPlatformSpecificSurfaceExtension() const;
  GHOST_TSuccess recreateSwapchain(bool use_hdr_swapchain);
  GHOST_TSuccess initializeFrameData();
  GHOST_TSuccess destroySwapchain();
};
