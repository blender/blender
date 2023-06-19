/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 1997-2001 Id Software, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Video mode switching ported from Quake 2 by `Alex Fraser <alex@phatcore.com>`. */

/** \file
 * \ingroup GHOST
 */

#include <stdio.h>

#ifdef WITH_X11_XF86VMODE
#  include <X11/Xlib.h>
#  include <X11/extensions/xf86vmode.h>
#endif

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
#ifdef WITH_X11_XF86VMODE
  int majorVersion, minorVersion;
  XF86VidModeModeInfo **vidmodes;
  Display *dpy = m_system->getXDisplay();

  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

  if (dpy == nullptr) {
    return GHOST_kFailure;
  }

  majorVersion = minorVersion = 0;
  if (!XF86VidModeQueryVersion(dpy, &majorVersion, &minorVersion)) {
    fprintf(stderr, "Error: XF86VidMode extension missing!\n");
    return GHOST_kFailure;
  }

  if (XF86VidModeGetAllModeLines(dpy, DefaultScreen(dpy), &numSettings, &vidmodes)) {
    XFree(vidmodes);
  }

#else
  /* We only have one X11 setting at the moment. */
  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
  numSettings = 1;
#endif

  (void)display;
  return GHOST_kSuccess;
}

/* from SDL2 */
#ifdef WITH_X11_XF86VMODE
static int calculate_rate(XF86VidModeModeInfo *info)
{
  return (info->htotal && info->vtotal) ? (1000 * info->dotclock / (info->htotal * info->vtotal)) :
                                          0;
}
#endif

GHOST_TSuccess GHOST_DisplayManagerX11::getDisplaySetting(uint8_t display,
                                                          int32_t index,
                                                          GHOST_DisplaySetting &setting) const
{
  Display *dpy = m_system->getXDisplay();

  if (dpy == nullptr) {
    return GHOST_kFailure;
  }

  (void)display;

#ifdef WITH_X11_XF86VMODE
  int majorVersion, minorVersion;

  GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

  majorVersion = minorVersion = 0;
  if (XF86VidModeQueryVersion(dpy, &majorVersion, &minorVersion)) {
    XF86VidModeModeInfo **vidmodes;
    int numSettings;

    if (XF86VidModeGetAllModeLines(dpy, DefaultScreen(dpy), &numSettings, &vidmodes)) {
      GHOST_ASSERT(index < numSettings, "Requested setting outside of valid range.\n");

      setting.xPixels = vidmodes[index]->hdisplay;
      setting.yPixels = vidmodes[index]->vdisplay;
      setting.bpp = DefaultDepth(dpy, DefaultScreen(dpy));
      setting.frequency = calculate_rate(vidmodes[index]);
      XFree(vidmodes);

      return GHOST_kSuccess;
    }
  }
  else {
    fprintf(stderr, "Warning: XF86VidMode extension missing!\n");
    /* fallback to non xf86vmode below */
  }
#endif /* WITH_X11_XF86VMODE */

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
#ifdef WITH_X11_XF86VMODE
  /* Mode switching code ported from SDL:
   * See: src/video/x11/SDL_x11modes.c:set_best_resolution
   */
  int majorVersion, minorVersion;
  XF86VidModeModeInfo **vidmodes;
  Display *dpy = m_system->getXDisplay();
  int scrnum, num_vidmodes;

  if (dpy == nullptr) {
    return GHOST_kFailure;
  }

  scrnum = DefaultScreen(dpy);

  /* Get video mode list */
  majorVersion = minorVersion = 0;
  if (!XF86VidModeQueryVersion(dpy, &majorVersion, &minorVersion)) {
    fprintf(stderr, "Error: XF86VidMode extension missing!\n");
    return GHOST_kFailure;
  }
#  ifdef DEBUG
  printf("Using XFree86-VidModeExtension Version %d.%d\n", majorVersion, minorVersion);
#  endif

  if (XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes)) {
    int best_fit = -1;

    for (int i = 0; i < num_vidmodes; i++) {
      if (vidmodes[i]->hdisplay < setting.xPixels || vidmodes[i]->vdisplay < setting.yPixels) {
        continue;
      }

      if (best_fit == -1 || (vidmodes[i]->hdisplay < vidmodes[best_fit]->hdisplay) ||
          (vidmodes[i]->hdisplay == vidmodes[best_fit]->hdisplay &&
           vidmodes[i]->vdisplay < vidmodes[best_fit]->vdisplay))
      {
        best_fit = i;
        continue;
      }

      if ((vidmodes[i]->hdisplay == vidmodes[best_fit]->hdisplay) &&
          (vidmodes[i]->vdisplay == vidmodes[best_fit]->vdisplay))
      {
        if (!setting.frequency) {
          /* Higher is better, right? */
          if (calculate_rate(vidmodes[i]) > calculate_rate(vidmodes[best_fit])) {
            best_fit = i;
          }
        }
        else {
          if (abs(calculate_rate(vidmodes[i]) - int(setting.frequency)) <
              abs(calculate_rate(vidmodes[best_fit]) - int(setting.frequency)))
          {
            best_fit = i;
          }
        }
      }
    }

    if (best_fit != -1) {
#  ifdef DEBUG
      printf("Switching to video mode %dx%d %dx%d %d\n",
             vidmodes[best_fit]->hdisplay,
             vidmodes[best_fit]->vdisplay,
             vidmodes[best_fit]->htotal,
             vidmodes[best_fit]->vtotal,
             calculate_rate(vidmodes[best_fit]));
#  endif

      /* change to the mode */
      XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);

      /* Move the viewport to top left */
      XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
    }

    XFree(vidmodes);
  }
  else {
    return GHOST_kFailure;
  }

  XFlush(dpy);
  return GHOST_kSuccess;

#else
  (void)setting;

  /* Just pretend the request was successful. */
  return GHOST_kSuccess;
#endif
}
