/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"

#include <epoxy/glx.h>

#ifndef GHOST_OPENGL_GLX_CONTEXT_FLAGS
/* leave as convenience define for the future */
#  define GHOST_OPENGL_GLX_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextGLX : public GHOST_Context {
  /* XR code needs low level graphics data to send to OpenXR. */
  friend class GHOST_XrGraphicsBindingOpenGL;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextGLX(const GHOST_ContextParams &context_params,
                   Window window,
                   Display *display,
                   GLXFBConfig fbconfig,
                   int contextProfileMask,
                   int contextMajorVersion,
                   int contextMinorVersion,
                   int contextFlags,
                   int contextResetNotificationStrategy);

  /**
   * Destructor.
   */
  ~GHOST_ContextGLX() override;

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

 private:
  Display *display_;
  GLXFBConfig fbconfig_;
  Window window_;

  const int context_profile_mask_;
  const int context_major_version_;
  const int context_minor_version_;
  const int context_flags_;
  const int context_reset_notification_strategy_;

  GLXContext context_;

  /** The first created OpenGL context (for sharing display lists) */
  static GLXContext s_sharedContext;
  static int s_sharedCount;
};

/* used to get GLX info */
int GHOST_X11_GL_GetAttributes(
    int *attribs, int attribs_max, bool is_stereo_visual, bool need_alpha, bool for_fb_config);
