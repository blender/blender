/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.h"

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;
@class MTLTexture;
@class NSOpenGLContext;
@class NSOpenGLView;
@class NSView;

class GHOST_ContextCGL : public GHOST_Context {
 public:
  /**
   * Constructor.
   */
  GHOST_ContextCGL(bool stereoVisual,
                   NSView *metalView,
                   CAMetalLayer *metalLayer,
                   NSOpenGLView *openglView);

  /**
   * Destructor.
   */
  ~GHOST_ContextCGL();

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext();

  unsigned int getDefaultFramebuffer();

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
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval);

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &);

  /**
   * Updates the drawing context of this window.
   * Needed whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext();

 private:
  /** Metal state */
  NSView *m_metalView;
  CAMetalLayer *m_metalLayer;
  MTLCommandQueue *m_metalCmdQueue;
  MTLRenderPipelineState *m_metalRenderPipeline;

  /** OpenGL state, for GPUs that don't support Metal */
  NSOpenGLView *m_openGLView;

  /** The OpenGL drawing context */
  NSOpenGLContext *m_openGLContext;

  /** The virtualized default frame-buffer. */
  unsigned int m_defaultFramebuffer;

  /** The virtualized default frame-buffer's texture. */
  MTLTexture *m_defaultFramebufferMetalTexture;

  const bool m_debug;

  /** The first created OpenGL context (for sharing display lists) */
  static NSOpenGLContext *s_sharedOpenGLContext;
  static int s_sharedCount;

  /* Metal functions */
  void metalInit();
  void metalFree();
  void metalInitFramebuffer();
  void metalUpdateFramebuffer();
  void metalSwapBuffers();
};
