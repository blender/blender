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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DisplayManagerWin32.cpp
 *  \ingroup GHOST
 *  \author	Maarten Gribnau
 *  \date	September 21, 2001
 */

#include "GHOST_DisplayManagerWin32.h"
#include "GHOST_Debug.h"

#define _WIN32_WINNT 0x501 // require Windows XP or newer
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// We do not support multiple monitors at the moment
#define COMPILE_MULTIMON_STUBS
#ifndef FREE_WINDOWS
#include <multimon.h>
#endif


GHOST_DisplayManagerWin32::GHOST_DisplayManagerWin32(void)
{
}


GHOST_TSuccess GHOST_DisplayManagerWin32::getNumDisplays(GHOST_TUns8& numDisplays) const
{
	numDisplays = ::GetSystemMetrics(SM_CMONITORS);
	return numDisplays > 0 ? GHOST_kSuccess : GHOST_kFailure;
}

static BOOL get_dd(DWORD d, DISPLAY_DEVICE *dd)
{
	dd->cb = sizeof(DISPLAY_DEVICE);
	return ::EnumDisplayDevices(NULL, d, dd, 0);
}

/*
 * When you call EnumDisplaySettings with iModeNum set to zero, the operating system 
 * initializes and caches information about the display device. When you call 
 * EnumDisplaySettings with iModeNum set to a non-zero value, the function returns 
 * the information that was cached the last time the function was called with iModeNum
 * set to zero. 
 */
GHOST_TSuccess GHOST_DisplayManagerWin32::getNumDisplaySettings(GHOST_TUns8 display, GHOST_TInt32& numSettings) const
{
	DISPLAY_DEVICE display_device;
	if (!get_dd(display, &display_device)) return GHOST_kFailure;

	numSettings = 0;
	DEVMODE dm;
	while (::EnumDisplaySettings(display_device.DeviceName, numSettings, &dm)) {
		numSettings++;
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerWin32::getDisplaySetting(GHOST_TUns8 display, GHOST_TInt32 index, GHOST_DisplaySetting& setting) const
{
	DISPLAY_DEVICE display_device;
	if (!get_dd(display, &display_device)) return GHOST_kFailure;

	GHOST_TSuccess success;
	DEVMODE dm;
	if (::EnumDisplaySettings(display_device.DeviceName, index, &dm)) {
#ifdef GHOST_DEBUG
		printf("display mode: width=%d, height=%d, bpp=%d, frequency=%d\n", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmDisplayFrequency);
#endif // GHOST_DEBUG
		setting.xPixels     = dm.dmPelsWidth;
		setting.yPixels     = dm.dmPelsHeight;
		setting.bpp         = dm.dmBitsPerPel;
		/* When you call the EnumDisplaySettings function, the dmDisplayFrequency member
		 * may return with the value 0 or 1. These values represent the display hardware's
		 * default refresh rate. This default rate is typically set by switches on a display 
		 * card or computer motherboard, or by a configuration program that does not use 
		 * Win32 display functions such as ChangeDisplaySettings. 
		 */
		/* First, we tried to explicitly set the frequency to 60 if EnumDisplaySettings
		 * returned 0 or 1 but this doesn't work since later on an exact match will
		 * be searched. And this will never happen if we change it to 60. Now we rely
		 * on the default h/w setting.
		 */
		setting.frequency = dm.dmDisplayFrequency;
		success = GHOST_kSuccess;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}


GHOST_TSuccess GHOST_DisplayManagerWin32::getCurrentDisplaySetting(GHOST_TUns8 display, GHOST_DisplaySetting& setting) const
{
	return getDisplaySetting(display, ENUM_CURRENT_SETTINGS, setting);
}


GHOST_TSuccess GHOST_DisplayManagerWin32::setCurrentDisplaySetting(GHOST_TUns8 display, const GHOST_DisplaySetting& setting)
{
	DISPLAY_DEVICE display_device;
	if (!get_dd(display, &display_device)) return GHOST_kFailure;

	GHOST_DisplaySetting match;
	findMatch(display, setting, match);
	DEVMODE dm;
	int i = 0;
	while (::EnumDisplaySettings(display_device.DeviceName, i++, &dm)) {
		if ((dm.dmBitsPerPel       == match.bpp    ) &&
		    (dm.dmPelsWidth        == match.xPixels) &&
		    (dm.dmPelsHeight       == match.yPixels) &&
		    (dm.dmDisplayFrequency == match.frequency))
		{
			break;
		}
	}
	/*
	 * dm.dmBitsPerPel = match.bpp;
	 * dm.dmPelsWidth = match.xPixels;
	 * dm.dmPelsHeight = match.yPixels;
	 * dm.dmDisplayFrequency = match.frequency;
	 * dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
	 * dm.dmSize = sizeof(DEVMODE);
	 * dm.dmDriverExtra = 0;
	 */
#ifdef GHOST_DEBUG
	printf("display change: Requested settings:\n");
	printf("  dmBitsPerPel=%d\n", dm.dmBitsPerPel);
	printf("  dmPelsWidth=%d\n", dm.dmPelsWidth);
	printf("  dmPelsHeight=%d\n", dm.dmPelsHeight);
	printf("  dmDisplayFrequency=%d\n", dm.dmDisplayFrequency);
#endif // GHOST_DEBUG

	LONG status = ::ChangeDisplaySettings(&dm, CDS_FULLSCREEN);
#ifdef GHOST_DEBUG
	switch (status) {
		case DISP_CHANGE_SUCCESSFUL:
			printf("display change: The settings change was successful.\n");
			break;
		case DISP_CHANGE_RESTART:
			printf("display change: The computer must be restarted in order for the graphics mode to work.\n");
			break;
		case DISP_CHANGE_BADFLAGS:
			printf("display change: An invalid set of flags was passed in.\n");
			break;
		case DISP_CHANGE_BADPARAM:
			printf("display change: An invalid parameter was passed in. "
			       "This can include an invalid flag or combination of flags.\n");
			break;
		case DISP_CHANGE_FAILED:
			printf("display change: The display driver failed the specified graphics mode.\n");
			break;
		case DISP_CHANGE_BADMODE:
			printf("display change: The graphics mode is not supported.\n");
			break;
		case DISP_CHANGE_NOTUPDATED:
			printf("display change: Windows NT: Unable to write settings to the registry.\n");
			break;
		default:
			printf("display change: Return value invalid\n");
			break;
	}
#endif // GHOST_DEBUG
	return status == DISP_CHANGE_SUCCESSFUL ? GHOST_kSuccess : GHOST_kFailure;
}
