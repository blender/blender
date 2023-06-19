/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

//#define WIN32_COMPOSITING

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
  GHOST_ContextWGL(bool stereoVisual,
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
  ~GHOST_ContextWGL();

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
  GHOST_TSuccess getSwapInterval(int &intervalOut);

 private:
  int choose_pixel_format_arb(bool stereoVisual, bool needAlpha);
  int _choose_pixel_format_arb_1(bool stereoVisual, bool needAlpha);

  HWND m_hWnd;
  HDC m_hDC;

  const int m_contextProfileMask;
  const int m_contextMajorVersion;
  const int m_contextMinorVersion;
  const int m_contextFlags;
  const bool m_alphaBackground;
  const int m_contextResetNotificationStrategy;

  HGLRC m_hGLRC;

#ifndef NDEBUG
  const char *m_dummyVendor;
  const char *m_dummyRenderer;
  const char *m_dummyVersion;
#endif

  static HGLRC s_sharedHGLRC;
  static int s_sharedCount;
};
