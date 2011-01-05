/**
 * $Id: GHOST_SystemWin32.cpp 30060 2010-07-06 20:31:55Z elubie $
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *                 Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GHOST_SystemPathsWin32.h"

#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#include <windows.h>
#include <shlobj.h>


GHOST_SystemPathsWin32::GHOST_SystemPathsWin32()
{
}

GHOST_SystemPathsWin32::~GHOST_SystemPathsWin32()
{
}

const GHOST_TUns8* GHOST_SystemPathsWin32::getSystemDir() const
{
	static char knownpath[MAX_PATH];
	HRESULT hResult = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, knownpath);

	if (hResult == S_OK)
	{
		return (GHOST_TUns8*)knownpath;
	}

	return NULL;
}

const GHOST_TUns8* GHOST_SystemPathsWin32::getUserDir() const
{
	static char knownpath[MAX_PATH];
	HRESULT hResult = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, knownpath);

	if (hResult == S_OK)
	{
		return (GHOST_TUns8*)knownpath;
	}

	return NULL;
}

const GHOST_TUns8* GHOST_SystemPathsWin32::getBinaryDir() const
{
	static char fullname[MAX_PATH];
	if(GetModuleFileName(0, fullname, MAX_PATH)) {
		return (GHOST_TUns8*)fullname;
	}

	return NULL;
}
