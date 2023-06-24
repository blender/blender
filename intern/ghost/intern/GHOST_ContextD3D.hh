/* SPDX-FileCopyrightText: 2014-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

#include <D3D11.h>

#include "GHOST_Context.hh"

class GHOST_ContextD3D : public GHOST_Context {
  /* XR code needs low level graphics data to send to OpenXR. */
  friend class GHOST_XrGraphicsBindingD3D;

 public:
  GHOST_ContextD3D(bool stereoVisual, HWND hWnd);
  ~GHOST_ContextD3D();

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
   * Updates the drawing context of this window. Needed
   * whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext()
  {
    return GHOST_kFailure;
  }

  /**
   * Checks if it is OK for a remove the native display
   * \return Indication as to whether removal has succeeded.
   */
  GHOST_TSuccess releaseNativeHandles();

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /*interval*/)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the OpenGL frame-buffer associated with the OpenGL context
   * \return The ID of an OpenGL frame-buffer object.
   */
  unsigned int getDefaultFramebuffer()
  {
    return 0;
  }

  class GHOST_SharedOpenGLResource *createSharedOpenGLResource(
      unsigned int width,
      unsigned int height,
      DXGI_FORMAT format,
      ID3D11RenderTargetView *render_target);
  class GHOST_SharedOpenGLResource *createSharedOpenGLResource(unsigned int width,
                                                               unsigned int height,
                                                               DXGI_FORMAT format);
  void disposeSharedOpenGLResource(class GHOST_SharedOpenGLResource *shared_res);
  GHOST_TSuccess blitFromOpenGLContext(class GHOST_SharedOpenGLResource *shared_res,
                                       unsigned int width,
                                       unsigned int height);
  ID3D11Texture2D *getSharedTexture2D(class GHOST_SharedOpenGLResource *shared_res);

  bool isUpsideDown() const
  {
    return true;
  }

 private:
  GHOST_TSuccess setupD3DLib();

  static HMODULE s_d3d_lib;
  static PFN_D3D11_CREATE_DEVICE s_D3D11CreateDeviceFn;

  HWND m_hWnd;

  ID3D11Device *m_device;
  ID3D11DeviceContext *m_device_ctx;
};
