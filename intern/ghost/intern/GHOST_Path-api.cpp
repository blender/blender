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
 * The Original Code is Copyright (C) 2010 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_Path-api.cpp
 *  \ingroup GHOST
 */


#include "intern/GHOST_Debug.h"
#include "GHOST_Types.h"
#include "GHOST_Path-api.h"
#include "GHOST_ISystemPaths.h"

GHOST_TSuccess GHOST_CreateSystemPaths(void)
{
	return GHOST_ISystemPaths::create();
}

GHOST_TSuccess GHOST_DisposeSystemPaths(void)
{
	return GHOST_ISystemPaths::dispose();
}

const GHOST_TUns8 *GHOST_getSystemDir(int version, const char *versionstr)
{
	GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
	return systemPaths ? systemPaths->getSystemDir(version, versionstr) : 0;
}

const GHOST_TUns8 *GHOST_getUserDir(int version, const char *versionstr)
{
	GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
	return systemPaths ? systemPaths->getUserDir(version, versionstr) : 0; /* shouldn't be NULL */
}

const GHOST_TUns8 *GHOST_getBinaryDir()
{
	GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
	return systemPaths ? systemPaths->getBinaryDir() : 0;  /* shouldn't be NULL */
}

void GHOST_addToSystemRecentFiles(const char *filename)
{
	GHOST_ISystemPaths *systemPaths = GHOST_ISystemPaths::get();
	if (systemPaths) {
		systemPaths->addToSystemRecentFiles(filename);
	}
}
