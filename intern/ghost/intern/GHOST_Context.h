/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_Context class.
 */

#pragma once

#include "GHOST_IContext.h"
#include "GHOST_Types.h"

#include "glew-mx.h"

#include <cstdlib>  // for NULL

class GHOST_Context : public GHOST_IContext {
 public:
  /**
   * Constructor.
   * \param stereoVisual: Stereo visual for quad buffered stereo.
   */
  GHOST_Context(bool stereoVisual) : m_stereoVisual(stereoVisual)
  {
  }

  /**
   * Destructor.
   */
  virtual ~GHOST_Context()
  {
  }

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess swapBuffers() = 0;

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess activateDrawingContext() = 0;

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess releaseDrawingContext() = 0;

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  virtual GHOST_TSuccess initializeDrawingContext() = 0;

  /**
   * Updates the drawing context of this window. Needed
   * whenever the window is changed.
   * \return Indication of success.
   */
  virtual GHOST_TSuccess updateDrawingContext()
  {
    return GHOST_kFailure;
  }

  /**
   * Checks if it is OK for a remove the native display
   * \return Indication as to whether removal has succeeded.
   */
  virtual GHOST_TSuccess releaseNativeHandles() = 0;

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  virtual GHOST_TSuccess setSwapInterval(int /*interval*/)
  {
    return GHOST_kFailure;
  }

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  virtual GHOST_TSuccess getSwapInterval(int &)
  {
    return GHOST_kFailure;
  }

  /**
   * Get user data.
   */
  void *getUserData()
  {
    return m_user_data;
  }

  /**
   * Set user data (intended for the caller to use as needed).
   */
  void setUserData(void *user_data)
  {
    m_user_data = user_data;
  }

  /**
   * Stereo visual created. Only necessary for 'real' stereo support,
   * ie quad buffered stereo. This is not always possible, depends on
   * the graphics h/w
   */
  inline bool isStereoVisual() const
  {
    return m_stereoVisual;
  }

  /**
   * Returns if the context is rendered upside down compared to OpenGL.
   */
  virtual inline bool isUpsideDown() const
  {
    return false;
  }

  /**
   * Gets the OpenGL frame-buffer associated with the OpenGL context
   * \return The ID of an OpenGL frame-buffer object.
   */
  virtual unsigned int getDefaultFramebuffer()
  {
    return 0;
  }

 protected:
  void initContextGLEW();

  bool m_stereoVisual;

  /** Caller specified, not for internal use. */
  void *m_user_data = nullptr;

  static void initClearGL();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_Context")
#endif
};

#ifdef _WIN32
bool win32_chk(bool result, const char *file = NULL, int line = 0, const char *text = NULL);
bool win32_silent_chk(bool result);

#  ifndef NDEBUG
#    define WIN32_CHK(x) win32_chk((x), __FILE__, __LINE__, #    x)
#  else
#    define WIN32_CHK(x) win32_chk(x)
#  endif

#  define WIN32_CHK_SILENT(x, silent) ((silent) ? win32_silent_chk(x) : WIN32_CHK(x))
#endif /* _WIN32 */
