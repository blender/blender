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
	GHOST_SystemX11 *system
) :
	GHOST_DisplayManager(),
	m_system(system)
{
	//nothing to do.
}

	GHOST_TSuccess 
GHOST_DisplayManagerX11::
getNumDisplays(
	GHOST_TUns8& numDisplays
) const{	
	numDisplays =  m_system->getNumDisplays();
	return GHOST_kSuccess;
}


	GHOST_TSuccess 
GHOST_DisplayManagerX11::
getNumDisplaySettings(
	GHOST_TUns8 display,
	GHOST_TInt32& numSettings
) const{
	
	// We only have one X11 setting at the moment.
	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");	
	numSettings = GHOST_TInt32(1);
	return GHOST_kSuccess;
}

	GHOST_TSuccess 
GHOST_DisplayManagerX11::
getDisplaySetting(
	GHOST_TUns8 display,
	GHOST_TInt32 index,
	GHOST_DisplaySetting& setting
) const {
	
	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");	
	GHOST_ASSERT(index < 1, "Requested setting outside of valid range.\n");	
	
	Display * x_display = m_system->getXDisplay();

	if (x_display == NULL) {
		return GHOST_kFailure;
	}

	setting.xPixels  = DisplayWidth(x_display, DefaultScreen(x_display));
	setting.yPixels = DisplayHeight(x_display, DefaultScreen(x_display));
	setting.bpp = DefaultDepth(x_display,DefaultScreen(x_display));

	// Don't think it's possible to get this value from X!
	// So let's guess!!
	setting.frequency = 60;

	return GHOST_kSuccess;
}
	
	GHOST_TSuccess 
GHOST_DisplayManagerX11::
getCurrentDisplaySetting(
	GHOST_TUns8 display,
	GHOST_DisplaySetting& setting
) const {
	return getDisplaySetting(display,GHOST_TInt32(0),setting);
}


	GHOST_TSuccess 
GHOST_DisplayManagerX11::
setCurrentDisplaySetting(
	GHOST_TUns8 display,
	const GHOST_DisplaySetting& setting
){
#ifdef WITH_X11_XF86VMODE
	//
	// Mode switching code ported from Quake 2:
	// ftp://ftp.idsoftware.com/idstuff/source/q2source-3.21.zip
	// See linux/gl_glx.c:GLimp_SetMode
	//
	int majorVersion, minorVersion;
	XF86VidModeModeInfo **vidmodes;
	Display *dpy = m_system->getXDisplay();
	int scrnum, num_vidmodes;
	int best_fit, best_dist, dist, x, y;

	scrnum = DefaultScreen(dpy);

	// Get video mode list
	majorVersion = minorVersion = 0;
	if (!XF86VidModeQueryVersion(dpy, &majorVersion, &minorVersion)) {
		fprintf(stderr, "Error: XF86VidMode extension missing!\n");
		return GHOST_kFailure;
	}
#  ifdef _DEBUG
	printf("Using XFree86-VidModeExtension Version %d.%d\n",
			majorVersion, minorVersion);
#  endif

	XF86VidModeGetAllModeLines(dpy, scrnum, &num_vidmodes, &vidmodes);

	best_dist = 9999999;
	best_fit = -1;

	for (int i = 0; i < num_vidmodes; i++) {
		if (setting.xPixels > vidmodes[i]->hdisplay ||
			setting.yPixels > vidmodes[i]->vdisplay)
			continue;

		x = setting.xPixels - vidmodes[i]->hdisplay;
		y = setting.yPixels - vidmodes[i]->vdisplay;
		dist = (x * x) + (y * y);
		if (dist < best_dist) {
			best_dist = dist;
			best_fit = i;
		}
	}

	if (best_fit != -1) {
#  ifdef _DEBUG
		int actualWidth, actualHeight;
		actualWidth = vidmodes[best_fit]->hdisplay;
		actualHeight = vidmodes[best_fit]->vdisplay;
		printf("Switching to video mode %dx%d\n",
				actualWidth, actualHeight);
#  endif

		// change to the mode
		XF86VidModeSwitchToMode(dpy, scrnum, vidmodes[best_fit]);

		// Move the viewport to top left
		XF86VidModeSetViewPort(dpy, scrnum, 0, 0);
	} else
		return GHOST_kFailure;

	XFlush(dpy);
	return GHOST_kSuccess;

#else
	// Just pretend the request was successful.
	return GHOST_kSuccess;
#endif
}




