/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_IContext interface class.
 */

#pragma once

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
  virtual ~GHOST_IContext()
  {
  }

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

  /**
   * Get Vulkan handles for the given context.
   *
   * These handles are the same for a given context.
   * Should should only be called when using a Vulkan context.
   * Other contexts will not return any handles and leave the
   * handles where the parameters are referring to unmodified.
   *
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
   * \returns GHOST_kFailure when context isn't a Vulkan context.
   *     GHOST_kSuccess when the context is a Vulkan context and the
   *     handles have been set.
   */
  virtual GHOST_TSuccess getVulkanHandles(void *r_instance,
                                          void *r_physical_device,
                                          void *r_device,
                                          uint32_t *r_graphic_queue_family,
                                          void *r_queue) = 0;

  /**
   * Return Vulkan command buffer.
   *
   * Command buffers are different for each image in the swap chain.
   * At the start of each frame the correct command buffer should be
   * retrieved with this function.
   *
   * \param r_command_buffer: After calling this function the VkCommandBuffer
   *     referenced by this parameter will contain the VKCommandBuffer handle
   *     of the current back buffer (when swap chains are enabled) or
   *     it will contain a general VkCommandQueue.
   * \returns GHOST_kFailure when context isn't a Vulkan context.
   *     GHOST_kSuccess when the context is a Vulkan context and the
   *     handles have been set.
   */
  virtual GHOST_TSuccess getVulkanCommandBuffer(void *r_command_buffer) = 0;

  /**
   * Gets the Vulkan backbuffer related resource handles associated with the Vulkan context.
   * Needs to be called after each swap event as the backbuffer will change.
   *
   * \param r_image: After calling this function the VkImage
   *     referenced by this parameter will contain the VKImage handle
   *     of the current back buffer.
   * \param r_framebuffer: After calling this function the VkFramebuffer
   *     referenced by this parameter will contain the VKFramebuffer handle
   *     of the current back buffer.
   * \param r_render_pass: After calling this function the VkRenderPass
   *     referenced by this parameter will contain the VKRenderPass handle
   *     of the current back buffer.
   * \param r_extent: After calling this function the VkExtent2D
   *     referenced by this parameter will contain the size of the
   *     frame buffer and image in pixels.
   * \param r_fb_id: After calling this function the uint32_t
   *     referenced by this parameter will contain the id of the
   *     framebuffer of the current back buffer.
   * \returns GHOST_kFailure when context isn't a Vulkan context.
   *     GHOST_kSuccess when the context is a Vulkan context and the
   *     handles have been set.
   */
  virtual GHOST_TSuccess getVulkanBackbuffer(void *r_image,
                                             void *r_framebuffer,
                                             void *r_render_pass,
                                             void *r_extent,
                                             uint32_t *r_fb_id) = 0;

  virtual GHOST_TSuccess swapBuffers() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IContext")
#endif
};
