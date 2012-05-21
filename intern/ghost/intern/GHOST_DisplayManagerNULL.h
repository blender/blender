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

/** \file ghost/intern/GHOST_DisplayManagerNULL.h
 *  \ingroup GHOST
 * Declaration of GHOST_DisplayManagerNULL class.
 */

#ifndef __GHOST_DISPLAYMANAGERNULL_H__
#define __GHOST_DISPLAYMANAGERNULL_H__

#include "GHOST_DisplayManager.h"
#include "GHOST_SystemNULL.h"

class GHOST_SystemNULL;

class GHOST_DisplayManagerNULL : public GHOST_DisplayManager
{
public:
	GHOST_DisplayManagerNULL( GHOST_SystemNULL *system ) : GHOST_DisplayManager(), m_system(system) { /* nop */ }
	GHOST_TSuccess getNumDisplays( GHOST_TUns8& numDisplays ) const { return GHOST_kFailure; }
	GHOST_TSuccess getNumDisplaySettings( GHOST_TUns8 display, GHOST_TInt32& numSettings ) const{  return GHOST_kFailure; }
	GHOST_TSuccess getDisplaySetting( GHOST_TUns8 display, GHOST_TInt32 index, GHOST_DisplaySetting& setting ) const { return GHOST_kFailure; }
	GHOST_TSuccess getCurrentDisplaySetting( GHOST_TUns8 display, GHOST_DisplaySetting& setting ) const { return getDisplaySetting(display,GHOST_TInt32(0),setting); }
	GHOST_TSuccess setCurrentDisplaySetting( GHOST_TUns8 display, const GHOST_DisplaySetting& setting ){ return GHOST_kSuccess; }

private:
	GHOST_SystemNULL * m_system;
};

#endif /* __GHOST_DISPLAYMANAGERNULL_H__ */
