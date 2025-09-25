/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextSDL class.
 */

#include "GHOST_ContextSDL.hh"

#include <cassert>
#include <cstring>

SDL_GLContext GHOST_ContextSDL::s_sharedContext = nullptr;
int GHOST_ContextSDL::s_sharedCount = 0;

GHOST_ContextSDL::GHOST_ContextSDL(const GHOST_ContextParams &context_params,
                                   SDL_Window *window,
                                   int contextProfileMask,
                                   int contextMajorVersion,
                                   int contextMinorVersion,
                                   int contextFlags,
                                   int contextResetNotificationStrategy)
    : GHOST_Context(context_params),
      window_(window),
      hidden_window_(nullptr),
      context_profile_mask_(contextProfileMask),
      context_major_version_(contextMajorVersion),
      context_minor_version_(contextMinorVersion),
      context_flags_(contextFlags),
      context_reset_notification_strategy_(contextResetNotificationStrategy),
      context_(nullptr)
{
  // assert(window_  != nullptr);
}

GHOST_ContextSDL::~GHOST_ContextSDL()
{
  if (context_ == nullptr) {
    return;
  }

  if (window_ != nullptr && context_ == SDL_GL_GetCurrentContext()) {
    SDL_GL_MakeCurrent(window_, nullptr);
  }
  if (context_ != s_sharedContext || s_sharedCount == 1) {
    assert(s_sharedCount > 0);

    s_sharedCount--;

    if (s_sharedCount == 0) {
      s_sharedContext = nullptr;
    }
    SDL_GL_DeleteContext(context_);
  }

  if (hidden_window_ != nullptr) {
    SDL_DestroyWindow(hidden_window_);
  }
}

GHOST_TSuccess GHOST_ContextSDL::swapBufferRelease()
{
  SDL_GL_SwapWindow(window_);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextSDL::activateDrawingContext()
{
  if (context_ == nullptr) {
    return GHOST_kFailure;
  }
  active_context_ = this;
  return SDL_GL_MakeCurrent(window_, context_) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextSDL::releaseDrawingContext()
{
  if (context_ == nullptr) {
    return GHOST_kFailure;
  }
  active_context_ = nullptr;
  /* Untested, may not work. */
  return SDL_GL_MakeCurrent(nullptr, nullptr) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextSDL::initializeDrawingContext()
{
  const bool needAlpha = false;

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, context_profile_mask_);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, context_major_version_);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, context_minor_version_);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags_);

  SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

  if (needAlpha) {
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  }

  if (context_params_.is_stereo_visual) {
    SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
  }

  if (window_ == nullptr) {
    hidden_window_ = SDL_CreateWindow("Offscreen Context Windows",
                                      SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
                                      1,
                                      1,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS |
                                          SDL_WINDOW_HIDDEN);

    window_ = hidden_window_;
  }

  context_ = SDL_GL_CreateContext(window_);

  GHOST_TSuccess success;

  if (context_ != nullptr) {
    if (!s_sharedContext) {
      s_sharedContext = context_;
    }
    s_sharedCount++;

    success = (SDL_GL_MakeCurrent(window_, context_) < 0) ? GHOST_kFailure : GHOST_kSuccess;

    {
      const GHOST_TVSyncModes vsync = getVSync();
      if (vsync != GHOST_kVSyncModeUnset) {
        setSwapInterval(int(vsync));
      }
    }

    initClearGL();
    SDL_GL_SwapWindow(window_);

    active_context_ = this;
    success = GHOST_kSuccess;
  }
  else {
    success = GHOST_kFailure;
  }

  return success;
}

GHOST_TSuccess GHOST_ContextSDL::releaseNativeHandles()
{
  window_ = nullptr;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextSDL::setSwapInterval(int interval)
{
  if (SDL_GL_SetSwapInterval(interval) == -1) {
    return GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextSDL::getSwapInterval(int &interval_out)
{
  interval_out = SDL_GL_GetSwapInterval();
  return GHOST_kSuccess;
}
