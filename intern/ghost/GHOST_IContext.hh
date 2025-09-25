/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IContext interface class.
 */

#pragma once

#include "GHOST_Types.h"

#ifdef WITH_VULKAN_BACKEND
#  include <functional>
#endif

/**
 * Interface for GHOST context.
 *
 * You can create a off-screen context (windowless) with the system's
 * #GHOST_ISystem::createOffscreenContext method.
 * \see GHOST_ISystem#createOffscreenContext
 */
class GHOST_IContext {
 public:
  /**
   * Destructor.
   */
  virtual ~GHOST_IContext() = default;

  /**
   * Returns the thread's currently active drawing context.
   */
  static GHOST_IContext *getActiveDrawingContext();

  /**
   * Activates the drawing context.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess releaseDrawingContext() = 0;

  /**
   * Gets the OpenGL frame-buffer associated with the OpenGL context
   * \return The ID of an OpenGL frame-buffer object.
   */
  virtual unsigned int getDefaultFramebuffer() = 0;
  /**
   * Acquire next buffer for drawing.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBufferAcquire() = 0;

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBufferRelease() = 0;

#ifdef WITH_VULKAN_BACKEND
  /**
   * Get Vulkan handles for the given context.
   *
   * These handles are the same for a given context.
   * Should only be called when using a Vulkan context.
   * Other contexts will not return any handles and leave the
   * handles where the parameters are referring to unmodified.
   *
   * \param r_handles: After calling this structure is filled with
   *     the vulkan handles of the context.
   */
  virtual GHOST_TSuccess getVulkanHandles(GHOST_VulkanHandles &r_handles) = 0;

  /**
   * Acquire the current swap-chain format.
   *
   * \param windowhandle:  GHOST window handle to a window to get the resource from.
   * \param r_surface_format: After calling this function the VkSurfaceFormatKHR
   *     referenced by this parameter will contain the surface format of the
   *     surface. The format is the same as the image returned in the r_image
   *     parameter.
   * \param r_extent: After calling this function the VkExtent2D
   *     referenced by this parameter will contain the size of the
   *     frame buffer and image in pixels.
   */
  virtual GHOST_TSuccess getVulkanSwapChainFormat(
      GHOST_VulkanSwapChainData *r_swap_chain_data) = 0;

  /**
   * Set the pre and post callbacks for vulkan swap-chain in the given context.
   *
   * \param context: GHOST context handle of a vulkan context to
   *     get the Vulkan handles from.
   * \param swap_buffer_draw_callback: Function pointer to be called when acquired swap buffer is
   *     released, allowing Vulkan backend to update the swap chain.
   * \param swap_buffer_acquired_callback: Function to be called at when swap buffer is acquired.
   *     Allowing Vulkan backend to update the framebuffer. It is also called when no swap chain
   *     exists indicating that the window was minimuzed.
   * \param openxr_acquire_image_callback: Function to be called when an image needs to be acquired
   *     to be drawn to an OpenXR swap-chain.
   * \param openxr_release_image_callback: Function to be called after an image has been drawn to
   *     the OpenXR swap-chain.
   */
  virtual GHOST_TSuccess setVulkanSwapBuffersCallbacks(
      std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffer_draw_callback,
      std::function<void(void)> swap_buffer_acquired_callback,
      std::function<void(GHOST_VulkanOpenXRData *)> openxr_acquire_framebuffer_image_callback,
      std::function<void(GHOST_VulkanOpenXRData *)> openxr_release_framebuffer_image_callback) = 0;
#endif

  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IContext")
};
