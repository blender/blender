/* SPDX-FileCopyrightText: 2014-2023 Blender Authors
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
  friend class GHOST_XrGraphicsBindingOpenGLD3D;
  friend class GHOST_XrGraphicsBindingVulkanD3D;

 public:
  GHOST_ContextD3D(const GHOST_ContextParams &context_params, HWND hWnd);
  ~GHOST_ContextD3D() override;

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
   * Updates the drawing context of this window. Needed
   * whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext() override
  {
    return GHOST_kFailure;
  }

  /**
   * Checks if it is OK for a remove the native display
   * \return Indication as to whether removal has succeeded.
   */
  GHOST_TSuccess releaseNativeHandles() override;

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /*interval*/) override
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param interval_out: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int & /*unused*/) override
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the OpenGL frame-buffer associated with the OpenGL context
   * \return The ID of an OpenGL frame-buffer object.
   */
  unsigned int getDefaultFramebuffer() override
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

  bool isUpsideDown() const override
  {
    return true;
  }

 private:
  GHOST_TSuccess setupD3DLib();

  static HMODULE s_d3d_lib;
  static PFN_D3D11_CREATE_DEVICE s_D3D11CreateDeviceFn;

  HWND h_wnd_;

  ID3D11Device *device_;
  ID3D11DeviceContext *device_ctx_;
};
