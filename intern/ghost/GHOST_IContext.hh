/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IContext interface class.
 */

#pragma once

#include <functional>

#include "GHOST_Types.h"

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
  virtual ~GHOST_IContext() {}

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

  virtual unsigned int getDefaultFramebuffer() = 0;
  virtual GHOST_TSuccess swapBuffers() = 0;

#ifdef WITH_VULKAN_BACKEND
  /**
   * Get Vulkan handles for the given context.
   *
   * These handles are the same for a given context.
   * Should only be called when using a Vulkan context.
   * Other contexts will not return any handles and leave the
   * handles where the parameters are referring to unmodified.
   *
   * \param context: GHOST context handle of a vulkan context to
   *     get the Vulkan handles from.
   * \param r_instance: After calling this function the VkInstance
   *     referenced by this parameter will contain the VKInstance handle
   *     of the context associated with the `context` parameter.
   * \param r_physical_device: After calling this function the VkPhysicalDevice
   *     referenced by this parameter will contain the VKPhysicalDevice handle
   *     of the context associated with the `context` parameter.
   * \param r_device: After calling this function the VkDevice
   *     referenced by this parameter will contain the VKDevice handle
   *     of the context associated with the `context` parameter.
   * \param r_graphic_queue_family: After calling this function the uint32_t
   *     referenced by this parameter will contain the graphic queue family id
   *     of the context associated with the `context` parameter.
   * \param r_queue: After calling this function the VkQueue
   *     referenced by this parameter will contain the VKQueue handle
   *     of the context associated with the `context` parameter.
   */
  virtual GHOST_TSuccess getVulkanHandles(void *r_instance,
                                          void *r_physical_device,
                                          void *r_device,
                                          uint32_t *r_graphic_queue_family,
                                          void *r_queue) = 0;

  /**
   * Acquire the current swap chain format.
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
   * Set the pre and post callbacks for vulkan swap chain in the given context.
   *
   * \param context: GHOST context handle of a vulkan context to
   *     get the Vulkan handles from.
   * \param swap_buffers_pre_callback: Function pointer to be called at the beginning of
   * swapBuffers. Inside this callback the next swap chain image needs to be acquired and filled.
   * \param swap_buffers_post_callback: Function to be called at th end of swapBuffers. swapBuffers
   *     can recreate the swap chain. When this is done the application should be informed by those
   *     changes.
   */
  virtual GHOST_TSuccess setVulkanSwapBuffersCallbacks(
      std::function<void(const GHOST_VulkanSwapChainData *)> swap_buffers_pre_callback,
      std::function<void(void)> swap_buffers_post_callback) = 0;
#endif

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IContext")
#endif
};
