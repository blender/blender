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
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

#ifndef __GHOST_CONTEXTGLX_H__
#define __GHOST_CONTEXTGLX_H__

#include "GHOST_Context.h"

#include <GL/glxew.h>

#ifndef GHOST_OPENGL_GLX_CONTEXT_FLAGS
/* leave as convenience define for the future */
#  define GHOST_OPENGL_GLX_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_GLX_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextGLX : public GHOST_Context {
 public:
  /**
   * Constructor.
   */
  GHOST_ContextGLX(bool stereoVisual,
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
  ~GHOST_ContextGLX();

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
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles();

  /**
   * Sets the swap interval for swapBuffers.
   * \param interval The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval);

  /**
   * Gets the current swap interval for swapBuffers.
   * \param intervalOut Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut);

 private:
  void initContextGLXEW();

  Display *m_display;
  GLXFBConfig m_fbconfig;
  Window m_window;

  const int m_contextProfileMask;
  const int m_contextMajorVersion;
  const int m_contextMinorVersion;
  const int m_contextFlags;
  const int m_contextResetNotificationStrategy;

  GLXContext m_context;

  /** The first created OpenGL context (for sharing display lists) */
  static GLXContext s_sharedContext;
  static int s_sharedCount;
};

/* used to get GLX info */
int GHOST_X11_GL_GetAttributes(
    int *attribs, int attribs_max, bool is_stereo_visual, bool need_alpha, bool for_fb_config);

#endif  // __GHOST_CONTEXTGLX_H__
