/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"

#include <epoxy/wgl.h>

#ifndef GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextWGL : public GHOST_Context {
  /* XR code needs low level graphics data to send to OpenXR. */
  friend class GHOST_XrGraphicsBindingOpenGL;

 public:
  /**
   * Constructor.
   */
  GHOST_ContextWGL(const GHOST_ContextParams &context_params,
                   bool alphaBackground,
                   HWND hWnd,
                   HDC hDC,
                   int contextProfileMask,
                   int contextMajorVersion,
                   int contextMinorVersion,
                   int contextFlags,
                   int contextResetNotificationStrategy);

  /**
   * Destructor.
   */
  ~GHOST_ContextWGL() override;

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
  int choose_pixel_format_arb(bool stereoVisual, bool needAlpha);
  int _choose_pixel_format_arb_1(bool stereoVisual, bool needAlpha);

  HWND h_wnd_;
  HDC h_DC_;

  const int context_profile_mask_;
  const int context_major_version_;
  const int context_minor_version_;
  const int context_flags_;
  const bool alpha_background_;
  const int context_reset_notification_strategy_;

  HGLRC h_GLRC_;

#ifndef NDEBUG
  const char *dummy_vendor_;
  const char *dummy_renderer_;
  const char *dummy_version_;
#endif

  static HGLRC s_sharedHGLRC;
  static int s_sharedCount;
};
