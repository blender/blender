/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_DisplayManagerSDL class.
 */

#pragma once

#include "GHOST_DisplayManager.hh"

extern "C" {
#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif

class GHOST_SystemSDL;

class GHOST_DisplayManagerSDL : public GHOST_DisplayManager {
 public:
  GHOST_DisplayManagerSDL(GHOST_SystemSDL *system);

  GHOST_TSuccess getNumDisplays(uint8_t &numDisplays) const;

  GHOST_TSuccess getNumDisplaySettings(uint8_t display, int32_t &numSettings) const;

  GHOST_TSuccess getDisplaySetting(uint8_t display,
                                   int32_t index,
                                   GHOST_DisplaySetting &setting) const;

  GHOST_TSuccess getCurrentDisplaySetting(uint8_t display, GHOST_DisplaySetting &setting) const;

  GHOST_TSuccess getCurrentDisplayModeSDL(SDL_DisplayMode &mode) const;

  GHOST_TSuccess setCurrentDisplaySetting(uint8_t display, const GHOST_DisplaySetting &setting);

 private:
  GHOST_SystemSDL *m_system;
  SDL_DisplayMode m_mode;
};
