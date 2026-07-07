/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Context.hh"

extern "C" {
#include "SDL.h"
}

#ifndef GHOST_OPENGL_SDL_CONTEXT_FLAGS
#  ifdef WITH_GPU_DEBUG
#    define GHOST_OPENGL_SDL_CONTEXT_FLAGS SDL_GL_CONTEXT_DEBUG_FLAG
#  else
#    define GHOST_OPENGL_SDL_CONTEXT_FLAGS 0
#  endif
#endif

#ifndef GHOST_OPENGL_SDL_RESET_NOTIFICATION_STRATEGY
#  define GHOST_OPENGL_SDL_RESET_NOTIFICATION_STRATEGY 0
#endif

class GHOST_ContextSDL : public GHOST_Context {
 public:
  /**
   * Constructor.
   */
  GHOST_ContextSDL(const GHOST_ContextParams &context_params,
                   SDL_Window *window,
                   int contextProfileMask,
                   int contextMajorVersion,
                   int contextMinorVersion,
                   int contextFlags,
                   int contextResetNotificationStrategy);

  /**
   * Destructor.
   */
  ~GHOST_ContextSDL() override;

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
  SDL_Window *window_;
  SDL_Window *hidden_window_;

  const int context_profile_mask_;
  const int context_major_version_;
  const int context_minor_version_;
  const int context_flags_;
  const int context_reset_notification_strategy_;

  SDL_GLContext context_; /* sdl_glcontext_ */

  /** The first created OpenGL context (for sharing display lists) */
  static SDL_GLContext s_sharedContext;
  static int s_sharedCount;
};
