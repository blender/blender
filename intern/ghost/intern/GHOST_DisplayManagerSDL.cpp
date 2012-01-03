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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DisplayManagerSDL.cpp
 *  \ingroup GHOST
 */

#include "GHOST_SystemSDL.h"
#include "GHOST_DisplayManagerSDL.h"

GHOST_DisplayManagerSDL::GHOST_DisplayManagerSDL(GHOST_SystemSDL *system)
    :
      GHOST_DisplayManager(),
      m_system(system)
{
	/* do nothing */
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getNumDisplays(GHOST_TUns8& numDisplays) const
{
	numDisplays=  SDL_GetNumVideoDisplays();
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerSDL::getNumDisplaySettings(GHOST_TUns8 display,
                                                              GHOST_TInt32& numSettings) const
{
	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
	numSettings= GHOST_TInt32(1);
	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getDisplaySetting(GHOST_TUns8 display,
                                           GHOST_TInt32 index,
                                           GHOST_DisplaySetting& setting) const
{

	GHOST_ASSERT(display < 1, "Only single display systems are currently supported.\n");
	GHOST_ASSERT(index < 1, "Requested setting outside of valid range.\n");
	SDL_DisplayMode mode;

	SDL_GetDesktopDisplayMode(display, &mode);

	setting.xPixels= mode.w;
	setting.yPixels= mode.h;
	setting.bpp= SDL_BYTESPERPIXEL(mode.format);
	/* assume 60 when unset */
	setting.frequency= mode.refresh_rate ? mode.refresh_rate : 60;

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_DisplayManagerSDL::getCurrentDisplaySetting(GHOST_TUns8 display,
                                                  GHOST_DisplaySetting& setting) const
{
	return getDisplaySetting(display,GHOST_TInt32(0),setting);
}

GHOST_TSuccess
GHOST_DisplayManagerSDL:: setCurrentDisplaySetting(GHOST_TUns8 display,
                                                   const GHOST_DisplaySetting& setting)
{
	// This is never going to work robustly in X
	// but it's currently part of the full screen interface

	// we fudge it for now.

	return GHOST_kSuccess;
}
