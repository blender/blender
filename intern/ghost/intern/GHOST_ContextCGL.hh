/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLDevice;
@class MTLRenderPipelineState;
@class MTLTexture;
@class NSOpenGLContext;
@class NSOpenGLView;
@class NSView;

class GHOST_ContextCGL : public GHOST_Context {

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
  GHOST_ContextCGL(bool stereoVisual,
                   NSView *metalView,
                   CAMetalLayer *metalLayer,
                   NSOpenGLView *openglView,
                   GHOST_TDrawingContextType type);

  /**
   * Destructor.
   */
  ~GHOST_ContextCGL();

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers() override;

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
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &) override;

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
   * Register present callback
   */
  void metalRegisterPresentCallback(void (*callback)(
      MTLRenderPassDescriptor *, id<MTLRenderPipelineState>, id<MTLTexture>, id<CAMetalDrawable>));

 private:
  /** Metal state */
  /* Set this flag to `true` when rendering with Metal API for Viewport.
   * TODO(Metal): This should be assigned to externally. */
  bool m_useMetalForRendering = false;
  NSView *m_metalView;
  CAMetalLayer *m_metalLayer;
  MTLRenderPipelineState *m_metalRenderPipeline;
  bool m_ownsMetalDevice;

  /** OpenGL state, for GPUs that don't support Metal */
  NSOpenGLView *m_openGLView;

  /** The OpenGL drawing context */
  NSOpenGLContext *m_openGLContext;

  /** The virtualized default frame-buffer. */
  unsigned int m_defaultFramebuffer;

  /** The virtualized default frame-buffer's texture. */
  /**
   * Texture that you can render into with Metal. The texture will be
   * composited on top of `m_defaultFramebufferMetalTexture` whenever
   * `swapBuffers` is called.
   */
  static const int METAL_SWAPCHAIN_SIZE = 3;
  struct MTLSwapchainTexture {
    id<MTLTexture> texture;
    unsigned int index;
  };
  MTLSwapchainTexture m_defaultFramebufferMetalTexture[METAL_SWAPCHAIN_SIZE];
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

  int mtl_SwapInterval;
  const bool m_debug;

  /** The first created OpenGL context (for sharing display lists) */
  static NSOpenGLContext *s_sharedOpenGLContext;
  static int s_sharedCount;

  /* Single device queue for multiple contexts. */
  static MTLCommandQueue *s_sharedMetalCommandQueue;

  /* Metal functions */
  void metalInit();
  void metalFree();
  void metalInitFramebuffer();
  void metalUpdateFramebuffer();
  void metalSwapBuffers();
  void initClear();
};
