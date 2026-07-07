/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef WITH_OPENGL_BACKEND
#  error "ContextEGL requires WITH_OPENGL_BACKEND"
#endif

#include "GHOST_Context.hh"
#include "GHOST_System.hh"

#include <epoxy/egl.h>
#include <epoxy/gl.h>

#ifndef GHOST_OPENGL_EGL_CONTEXT_FLAGS
#  define GHOST_OPENGL_EGL_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextEGL : public GHOST_Context {
  /* XR code needs low level graphics data to send to OpenXR. */
  friend class GHOST_XrGraphicsBindingOpenGL;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextEGL(const GHOST_System *const system,
                   const GHOST_ContextParams &context_params,
                   EGLNativeWindowType nativeWindow,
                   EGLNativeDisplayType nativeDisplay,
                   EGLint contextProfileMask,
                   EGLint contextMajorVersion,
                   EGLint contextMinorVersion,
                   EGLint contextFlags,
                   EGLint contextResetNotificationStrategy,
                   EGLenum api);

  /**
   * Destructor.
   */
  ~GHOST_ContextEGL() override;

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

  EGLDisplay getDisplay() const;

  EGLConfig getConfig() const;

  EGLContext getContext() const;

 private:
  bool bindAPI(EGLenum api);

  const GHOST_System *const system_;

  EGLNativeDisplayType native_display_;
  EGLNativeWindowType native_window_;

  const EGLint context_profile_mask_;
  const EGLint context_major_version_;
  const EGLint context_minor_version_;
  const EGLint context_flags_;
  const EGLint context_reset_notification_strategy_;

  const EGLenum api_;

  EGLContext context_;
  EGLSurface surface_;
  EGLDisplay display_;
  EGLConfig config_;

  EGLint swap_interval_;

  EGLContext &shared_context_;
  EGLint &shared_count_;

  /**
   * True when the surface is created from `native_window_`.
   */
  bool surface_from_native_window_;

  static EGLContext s_gl_sharedContext;
  static EGLint s_gl_sharedCount;

  static EGLContext s_gles_sharedContext;
  static EGLint s_gles_sharedCount;

  static EGLContext s_vg_sharedContext;
  static EGLint s_vg_sharedCount;
};
