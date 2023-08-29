/* SPDX-FileCopyrightText: 1997-2001 Id Software, Inc.
 * SPDX-FileCopyrightText: 1993-2011 Tim Riker
 * SPDX-FileCopyrightText: 2012 Alex Fraser
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_DisplayManagerSDL.hh"
#include "GHOST_SystemSDL.hh"

#include "GHOST_WindowManager.hh"

GHOST_DisplayManagerSDL::GHOST_DisplayManagerSDL(GHOST_SystemSDL *system)
    : GHOST_DisplayManager(), m_system(system)
{
  memset(&m_mode, 0, sizeof(m_mode));
}

GHOST_TSuccess GHOST_DisplayManagerSDL::getNumDisplays(uint8_t &numDisplays) const
{
  numDisplays = SDL_GetNumVideoDisplays();
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerSDL::getNumDisplaySettings(uint8_t display,
                                                              int32_t &numSettings) const
{
  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

  numSettings = SDL_GetNumDisplayModes(display - 1);

  return GHOST_kSuccess;
}

static void ghost_mode_from_sdl(GHOST_DisplaySetting &setting, SDL_DisplayMode *mode)
{
  setting.xPixels = mode->w;
  setting.yPixels = mode->h;
  setting.bpp = SDL_BYTESPERPIXEL(mode->format) * 8;
  /* Just guess the frequency :( */
  setting.frequency = mode->refresh_rate ? mode->refresh_rate : 60;
}

static void ghost_mode_to_sdl(const GHOST_DisplaySetting &setting, SDL_DisplayMode *mode)
{
  mode->w = setting.xPixels;
  mode->h = setting.yPixels;
  // setting.bpp = SDL_BYTESPERPIXEL(mode->format) * 8; ???
  mode->refresh_rate = setting.frequency;
}

GHOST_TSuccess GHOST_DisplayManagerSDL::getDisplaySetting(uint8_t display,
                                                          int32_t index,
                                                          GHOST_DisplaySetting &setting) const
{
  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

  SDL_DisplayMode mode;
  SDL_GetDisplayMode(display, index, &mode);

  ghost_mode_from_sdl(setting, &mode);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerSDL::getCurrentDisplaySetting(
    uint8_t display, GHOST_DisplaySetting &setting) const
{
  SDL_DisplayMode mode;
  SDL_GetCurrentDisplayMode(display, &mode);

  ghost_mode_from_sdl(setting, &mode);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerSDL::getCurrentDisplayModeSDL(SDL_DisplayMode &mode) const
{
  mode = m_mode;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerSDL::setCurrentDisplaySetting(
    uint8_t display, const GHOST_DisplaySetting &setting)
{
  /*
   * Mode switching code ported from Quake 2 version 3.21 and BZFLAG version 2.4.0:
   * ftp://ftp.idsoftware.com/idstuff/source/q2source-3.21.zip
   * See linux/gl_glx.c:GLimp_SetMode
   * http://wiki.bzflag.org/BZFlag_Source
   * See: `src/platform/SDLDisplay.cxx:SDLDisplay` and `createWindow`.
   */
  SDL_DisplayMode mode;
  const int num_modes = SDL_GetNumDisplayModes(display);
  int best_fit, best_dist, dist, x, y;

  best_dist = 9999999;
  best_fit = -1;

  if (num_modes == 0) {
    /* Any mode is OK. */
    ghost_mode_to_sdl(setting, &mode);
  }
  else {
    for (int i = 0; i < num_modes; i++) {

      SDL_GetDisplayMode(display, i, &mode);

      if (int(setting.xPixels) > mode.w || int(setting.yPixels) > mode.h) {
        continue;
      }

      x = setting.xPixels - mode.w;
      y = setting.yPixels - mode.h;
      dist = (x * x) + (y * y);
      if (dist < best_dist) {
        best_dist = dist;
        best_fit = i;
      }
    }

    if (best_fit == -1) {
      return GHOST_kFailure;
    }
    SDL_GetDisplayMode(display, best_fit, &mode);
  }

  m_mode = mode;

  /* evil, SDL2 needs a window to adjust display modes */
  GHOST_WindowSDL *win = (GHOST_WindowSDL *)m_system->getWindowManager()->getActiveWindow();

  if (win) {
    SDL_Window *sdl_win = win->getSDLWindow();

    SDL_SetWindowDisplayMode(sdl_win, &mode);
    SDL_ShowWindow(sdl_win);
    SDL_SetWindowFullscreen(sdl_win, SDL_TRUE);

    return GHOST_kSuccess;
  }
  /* This is a problem for the BGE player :S, perhaps SDL2 will resolve at some point.
   * we really need SDL_SetDisplayModeForDisplay() to become an API func! - campbell. */
  printf("no windows available, can't fullscreen\n");

  /* do not fail, we will try again later when the window is created - wander */
  return GHOST_kSuccess;
}
