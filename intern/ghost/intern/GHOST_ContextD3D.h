/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#ifndef __GHOST_CONTEXTD3D_H__
#define __GHOST_CONTEXTD3D_H__

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

#include <D3D11.h>

#include "GHOST_Context.h"

class GHOST_ContextD3D : public GHOST_Context {
 public:
  GHOST_ContextD3D(bool stereoVisual, HWND hWnd);
  ~GHOST_ContextD3D();

  /**
   * Swaps front and back buffers of a window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers();

  /**
   * Activates the drawing context of this window.
   * \return  A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext();

  /**
   * Release the drawing context of the calling thread.
   * \return  A boolean success indicator.
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
   * Sets the swap interval for swapBuffers.
   * \param interval The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int /*interval*/)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the OpenGL framebuffer associated with the OpenGL context
   * \return The ID of an OpenGL framebuffer object.
   */
  unsigned int getDefaultFramebuffer()
  {
    return 0;
  }

  class GHOST_SharedOpenGLResource *createSharedOpenGLResource(
      unsigned int width, unsigned int height, ID3D11RenderTargetView *render_target);
  class GHOST_SharedOpenGLResource *createSharedOpenGLResource(unsigned int width,
                                                               unsigned int height);
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

#endif /* __GHOST_CONTEXTD3D_H__ */
