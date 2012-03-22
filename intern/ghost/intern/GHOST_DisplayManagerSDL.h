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

/** \file ghost/intern/GHOST_DisplayManagerSDL.h
 *  \ingroup GHOST
 * Declaration of GHOST_DisplayManagerSDL class.
 */

#ifndef __GHOST_DISPLAYMANAGERSDL_H__
#define __GHOST_DISPLAYMANAGERSDL_H__

#include "GHOST_DisplayManager.h"

extern "C" {
	#include "SDL.h"
}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#  error "SDL 2.0 or newer is needed to build with Ghost"
#endif

class GHOST_SystemSDL;

class GHOST_DisplayManagerSDL : public GHOST_DisplayManager
{
public:
	GHOST_DisplayManagerSDL(GHOST_SystemSDL *system);

	GHOST_TSuccess
	getNumDisplays(GHOST_TUns8& numDisplays) const;

	GHOST_TSuccess
	getNumDisplaySettings(GHOST_TUns8 display,
	                      GHOST_TInt32& numSettings) const;

	GHOST_TSuccess
	getDisplaySetting(GHOST_TUns8 display,
	                  GHOST_TInt32 index,
	                  GHOST_DisplaySetting& setting) const;

	GHOST_TSuccess
	getCurrentDisplaySetting(GHOST_TUns8 display,
	                         GHOST_DisplaySetting& setting) const;

	GHOST_TSuccess
	setCurrentDisplaySetting(GHOST_TUns8 display,
	                         const GHOST_DisplaySetting& setting);

private :
	GHOST_SystemSDL * m_system;
};

#endif /* __GHOST_DISPLAYMANAGERSDL_H__ */
