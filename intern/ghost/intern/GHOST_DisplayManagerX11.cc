/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 1997-2001 Id Software, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cstdio>

#include "GHOST_DisplayManagerX11.hh"
#include "GHOST_SystemX11.hh"

GHOST_DisplayManagerX11::GHOST_DisplayManagerX11(GHOST_SystemX11 *system)
    : GHOST_DisplayManager(), m_system(system)
{
  /* nothing to do. */
}

GHOST_TSuccess GHOST_DisplayManagerX11::getNumDisplays(uint8_t &numDisplays) const
{
  numDisplays = m_system->getNumDisplays();
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerX11::getNumDisplaySettings(uint8_t display,
                                                              int32_t &numSettings) const
{
  /* We only have one X11 setting at the moment. */
  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
  numSettings = 1;
  (void)display;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerX11::getDisplaySetting(uint8_t display,
                                                          int32_t index,
                                                          GHOST_DisplaySetting &setting) const
{
  Display *dpy = m_system->getXDisplay();

  if (dpy == nullptr) {
    return GHOST_kFailure;
  }

  (void)display;
  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
  GHOST_ASSERT(index < 1, "Requested setting outside of valid range.\n");
  (void)index;

  setting.xPixels = DisplayWidth(dpy, DefaultScreen(dpy));
  setting.yPixels = DisplayHeight(dpy, DefaultScreen(dpy));
  setting.bpp = DefaultDepth(dpy, DefaultScreen(dpy));
  setting.frequency = 60.0f;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerX11::getCurrentDisplaySetting(
    uint8_t display, GHOST_DisplaySetting &setting) const
{
  /* According to the xf86vidmodegetallmodelines man page,
   * "The first element of the array corresponds to the current video mode."
   */
  return getDisplaySetting(display, 0, setting);
}

GHOST_TSuccess GHOST_DisplayManagerX11::setCurrentDisplaySetting(
    uint8_t /*display*/, const GHOST_DisplaySetting &setting)
{
  (void)setting;

  /* Just pretend the request was successful. */
  return GHOST_kSuccess;
}
