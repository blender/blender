/**
 * $Id$
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
	// This is never going to work robustly in X 
	// but it's currently part of the full screen interface

	// we fudge it for now.

	return GHOST_kSuccess;
}




