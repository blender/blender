/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"
#include "GHOST_Types.h"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLDevice;
@class MTLRenderPipelineState;
@class MTLTexture;
@class NSView;

class GHOST_ContextMTL : public GHOST_Context {
  friend class GHOST_XrGraphicsBindingMetal;

 public:
  /* Defines the number of simultaneous command buffers which can be in flight.
   * The default limit of `64` is considered to be optimal for Blender. Too many command buffers
   * will result in workload fragmentation and additional system-level overhead. This limit should
   * also only be increased if the application is consistently exceeding the limit, and there are
   * no command buffer leaks.
   *
   * If this limit is reached, starting a new command buffer will fail. The Metal back-end will
   * therefore stall until completion and log a warning when this limit is reached in order to
   * ensure correct function of the app.
   *
   * It is generally preferable to reduce the prevalence of GPU_flush or GPU Context switches
   * (which will both break command submissions), rather than increasing this limit. */
  static const int max_command_buffer_count = 64;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextMTL(const GHOST_ContextParams &context_params,
                   NSView *metalView,
                   CAMetalLayer *metalLayer);

  /**
   * Destructor.
   */
  ~GHOST_ContextMTL() override;

  /** \copydoc #GHOST_IContext::swapBuffersAcquire */
  GHOST_TSuccess swapBufferAcquire() override
  {
    return GHOST_kSuccess;
  }

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess swapBufferRelease() override;

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext() override;

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext() override;

  unsigned int getDefaultFramebuffer() override;

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
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval) override;

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param interval_out: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &interval_out) override;

  /**
   * Updates the drawing context of this window.
   * Needed whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext() override;

  /**
   * Returns a texture that Metal code can use as a render target. The current
   * contents of this texture will be composited on top of the frame-buffer
   * each time `swapBuffers` is called.
   */
  id<MTLTexture> metalOverlayTexture();

  /**
   * Return a pointer to the Metal command queue used by this context.
   */
  MTLCommandQueue *metalCommandQueue();

  /**
   * Return a pointer to the Metal device associated with this context.
   */
  MTLDevice *metalDevice();

  /**
   * Callback registration.
   */
  void metalRegisterPresentCallback(void (*callback)(
      MTLRenderPassDescriptor *, id<MTLRenderPipelineState>, id<MTLTexture>, id<CAMetalDrawable>));
  void metalRegisterXrBlitCallback(void (*callback)(id<MTLTexture>, int, int, int, int));

 private:
  /** Metal state */
  NSView *metal_view_;
  CAMetalLayer *metal_layer_;
  MTLRenderPipelineState *metal_render_pipeline_;
  bool owns_metal_device_;

  /** The virtualized default frame-buffer's texture. */
  /**
   * Texture that you can render into with Metal. The texture will be
   * composited on top of `default_framebuffer_metal_texture_` whenever
   * `swapBuffers` is called.
   */
  static const int METAL_SWAPCHAIN_SIZE = 3;
  struct MTLSwapchainTexture {
    id<MTLTexture> texture;
    unsigned int index;
  };
  MTLSwapchainTexture default_framebuffer_metal_texture_[METAL_SWAPCHAIN_SIZE];
  unsigned int current_swapchain_index = 0;

  /* Present callback.
   * We use this such that presentation can be controlled from within the Metal
   * Context. This is required for optimal performance and clean control flow.
   * Also helps ensure flickering does not occur by present being dependent
   * on existing submissions. */
  void (*contextPresentCallback)(MTLRenderPassDescriptor *,
                                 id<MTLRenderPipelineState>,
                                 id<MTLTexture>,
                                 id<CAMetalDrawable>);
  /* XR texture blitting callback. */
  void (*xrBlitCallback)(id<MTLTexture>, int, int, int, int);

  int mtl_SwapInterval;

  static int s_sharedCount;

  /* Single device queue for multiple contexts. */
  static MTLCommandQueue *s_sharedMetalCommandQueue;

  /* Metal functions */
  void metalInit();
  void metalFree();
  void metalInitFramebuffer();
  void metalUpdateFramebuffer();
  void metalSwapBuffers();
  void initClear() {};
};
