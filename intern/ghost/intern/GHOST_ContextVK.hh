/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"

#ifdef _WIN32
#  include "GHOST_SystemWin32.hh"
#elif defined(__APPLE__)
#  include "GHOST_SystemCocoa.hh"
#else
#  include "GHOST_SystemX11.hh"
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

typedef enum {
  GHOST_kVulkanPlatformX11 = 0,
#ifdef WITH_GHOST_WAYLAND
  GHOST_kVulkanPlatformWayland,
#endif
} GHOST_TVulkanPlatformType;

class GHOST_ContextVK : public GHOST_Context {
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
#endif
                  int contextMajorVersion,
                  int contextMinorVersion,
                  int m_debug);

  /**
   * Destructor.
   */
  ~GHOST_ContextVK();

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext();

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  GHOST_TSuccess initializeDrawingContext();

  /**
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles();

  /**
   * Gets the Vulkan context related resource handles.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess getVulkanHandles(void *r_instance,
                                  void *r_physical_device,
                                  void *r_device,
                                  uint32_t *r_graphic_queue_family,
                                  void *r_queue);

  GHOST_TSuccess getVulkanSwapChainFormat(GHOST_VulkanSwapChainData *r_swap_chain_data) override;

  GHOST_TSuccess setVulkanSwapBuffersCallbacks(
      std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffers_pre_callback,
      std::function<void(void)> swap_buffers_post_callback) override;

  /**
   * Sets the swap interval for `swapBuffers`.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /* interval */)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &)
  {
    return GHOST_kFailure;
  };

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
#endif

  const int m_context_major_version;
  const int m_context_minor_version;
  const int m_debug;

  VkCommandPool m_command_pool;
  VkCommandBuffer m_command_buffer;

  VkQueue m_graphic_queue;
  VkQueue m_present_queue;

  /* For display only. */
  VkSurfaceKHR m_surface;
  VkSwapchainKHR m_swapchain;
  std::vector<VkImage> m_swapchain_images;

  VkExtent2D m_render_extent;
  VkSurfaceFormatKHR m_surface_format;
  VkFence m_fence;

  /** frame modulo swapchain_len. Used as index for sync objects. */
  int m_currentFrame = 0;
  /** Image index in the swapchain. Used as index for render objects. */
  uint32_t m_currentImage = 0;

  std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffers_pre_callback_;
  std::function<void(void)> swap_buffers_post_callback_;

  const char *getPlatformSpecificSurfaceExtension() const;
  GHOST_TSuccess createSwapchain();
  GHOST_TSuccess destroySwapchain();
  GHOST_TSuccess createCommandPools();
  GHOST_TSuccess createGraphicsCommandBuffers();
  GHOST_TSuccess createGraphicsCommandBuffer();
  GHOST_TSuccess recordCommandBuffers();
};
