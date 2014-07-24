/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Video mode switching
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Ported from Quake 2 by Alex Fraser <alex@phatcore.com>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DisplayManagerX11.cpp
 *  \ingroup GHOST
 */

#ifdef WITH_X11_XF86VMODE
#  include <X11/Xlib.h>
#  include <X11/extensions/xf86vmode.h>
#endif

#include "GHOST_DisplayManagerX11.h"
#include "GHOST_SystemX11.h"


GHOST_DisplayManagerX11::
GHOST_DisplayManagerX11(
        GHOST_SystemX11 *system)
    : GHOST_DisplayManager(),
      m_system(system)
{
	/* nothing to do. */
}

GHOST_TSuccess
GHOST_DisplayManagerX11::
getNumDisplays(GHOST_TUns8& numDisplays) const
{
	numDisplays =  m_system->getNumDisplays();
	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_DisplayManagerX11::
getNumDisplaySettings(
		GHOST_TUns8 display,
		GHOST_TInt32& numSettings) const
{
#ifdef WITH_X11_XF86VMODE
	int majorVersion, minorVersion;
	XF86VidModeModeInfo **vidmodes;
	Display *dpy = m_system->getXDisplay();

	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");

	if (dpy == NULL)
		return GHOST_kFailure;

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

	return GHOST_kSuccess;
}

/* from SDL2 */
#ifdef WITH_X11_XF86VMODE
static int
calculate_rate(XF86VidModeModeInfo *info)
{
	return (info->htotal
	        && info->vtotal) ? (1000 * info->dotclock / (info->htotal *
	                                                     info->vtotal)) : 0;
}
#endif

GHOST_TSuccess
GHOST_DisplayManagerX11::
getDisplaySetting(
		GHOST_TUns8 display,
		GHOST_TInt32 index,
		GHOST_DisplaySetting& setting) const
{
	Display *dpy = m_system->getXDisplay();

	if (dpy == NULL)
		return GHOST_kFailure;

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
#endif  /* WITH_X11_XF86VMODE */

	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
	GHOST_ASSERT(index < 1, "Requested setting outside of valid range.\n");

	setting.xPixels  = DisplayWidth(dpy, DefaultScreen(dpy));
	setting.yPixels = DisplayHeight(dpy, DefaultScreen(dpy));
	setting.bpp = DefaultDepth(dpy, DefaultScreen(dpy));
	setting.frequency = 60.0f;

	return GHOST_kSuccess;
}
	
GHOST_TSuccess
GHOST_DisplayManagerX11::
getCurrentDisplaySetting(
		GHOST_TUns8 display,
		GHOST_DisplaySetting& setting) const
{
	/* According to the xf86vidmodegetallmodelines man page,
	 * "The first element of the array corresponds to the current video mode."
	 */
	return getDisplaySetting(display, 0, setting);
}


GHOST_TSuccess
GHOST_DisplayManagerX11::
setCurrentDisplaySetting(
		GHOST_TUns8 display,
		const GHOST_DisplaySetting& setting)
{
#ifdef WITH_X11_XF86VMODE
	/* Mode switching code ported from SDL:
	 * See: src/video/x11/SDL_x11modes.c:set_best_resolution
	 */
	int majorVersion, minorVersion;
	XF86VidModeModeInfo **vidmodes;
	Display *dpy = m_system->getXDisplay();
	int scrnum, num_vidmodes;

	if (dpy == NULL)
		return GHOST_kFailure;

	scrnum = DefaultScreen(dpy);

	/* Get video mode list */
	majorVersion = minorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &majorVersion, &minorVersion)) {
		fprintf(stderr, "Error: XF86VidMode extension missing!\n");
		return GHOST_kFailure;
	}
#  ifdef _DEBUG
	printf("Using XFree86-VidModeExtension Version %d.%d\n",
	       majorVersion, minorVersion);
#  endif

	if (XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes)) {
		int best_fit = -1;

		for (int i = 0; i < num_vidmodes; i++) {
			if (vidmodes[i]->hdisplay < setting.xPixels ||
			    vidmodes[i]->vdisplay < setting.yPixels)
			{
				continue;
			}

			if (best_fit == -1 ||
			    (vidmodes[i]->hdisplay < vidmodes[best_fit]->hdisplay) ||
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
					if (calculate_rate(vidmodes[i]) >
					    calculate_rate(vidmodes[best_fit]))
					{
						best_fit = i;
					}
				}
				else {
					if (abs(calculate_rate(vidmodes[i]) - (int)setting.frequency) <
					    abs(calculate_rate(vidmodes[best_fit]) - (int)setting.frequency))
					{
						best_fit = i;
					}
				}
			}
		}

		if (best_fit != -1) {
#  ifdef _DEBUG
			printf("Switching to video mode %dx%d %dx%d %d\n",
			       vidmodes[best_fit]->hdisplay, vidmodes[best_fit]->vdisplay,
			       vidmodes[best_fit]->htotal, vidmodes[best_fit]->vtotal,
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
	/* Just pretend the request was successful. */
	return GHOST_kSuccess;
#endif
}
