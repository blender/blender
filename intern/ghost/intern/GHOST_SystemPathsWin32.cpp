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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *                 Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemPathsWin32.cpp
 *  \ingroup GHOST
 */


#include "GHOST_SystemPathsWin32.h"

#ifndef _WIN32_IE
#define _WIN32_IE 0x0501
#endif
#include <shlobj.h>
#include "utfconv.h"

#if defined(__MINGW32__) || defined(__CYGWIN__)

#if !defined(SHARD_PIDL)
#define SHARD_PIDL      0x00000001L
#endif

#if !defined(SHARD_PATHA)
#define SHARD_PATHA     0x00000002L
#endif

#if !defined(SHARD_PATHW)
#define SHARD_PATHW     0x00000003L
#endif

#if !defined(SHARD_PATH)
#ifdef UNICODE
#define SHARD_PATH  SHARD_PATHW
#else
#define SHARD_PATH  SHARD_PATHA
#endif
#endif

#endif

GHOST_SystemPathsWin32::GHOST_SystemPathsWin32()
{
}

GHOST_SystemPathsWin32::~GHOST_SystemPathsWin32()
{
}

const GHOST_TUns8 *GHOST_SystemPathsWin32::getSystemDir(int, const char *versionstr) const
{
	static char knownpath[MAX_PATH * 3 + 128] = {0}; /* 1 utf-16 might translante into 3 utf-8. 2 utf-16 translates into 4 utf-8*/
	wchar_t knownpath_16[MAX_PATH];

	HRESULT hResult = SHGetFolderPathW(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, knownpath_16);

	if (hResult == S_OK)
	{
		conv_utf_16_to_8(knownpath_16, knownpath, MAX_PATH * 3);
		strcat(knownpath, "\\Blender Foundation\\Blender\\");
		strcat(knownpath, versionstr);
		return (GHOST_TUns8*)knownpath;
	}

	return NULL;
}

const GHOST_TUns8 *GHOST_SystemPathsWin32::getUserDir(int, const char *versionstr) const
{
	static char knownpath[MAX_PATH * 3 + 128] = {0};
	wchar_t knownpath_16[MAX_PATH];

	HRESULT hResult = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, knownpath_16);

	if (hResult == S_OK)
	{
		conv_utf_16_to_8(knownpath_16, knownpath, MAX_PATH * 3);
		strcat(knownpath, "\\Blender Foundation\\Blender\\");
		strcat(knownpath, versionstr);
		return (GHOST_TUns8*)knownpath;
	}

	return NULL;
}

const GHOST_TUns8 *GHOST_SystemPathsWin32::getBinaryDir() const
{
	static char fullname[MAX_PATH * 3] = {0};
	wchar_t fullname_16[MAX_PATH * 3];

	if (GetModuleFileNameW(0, fullname_16, MAX_PATH)) {
		conv_utf_16_to_8(fullname_16, fullname, MAX_PATH * 3);
		return (GHOST_TUns8 *)fullname;
	}

	return NULL;
}

void GHOST_SystemPathsWin32::addToSystemRecentFiles(const char *filename) const
{
	/* SHARD_PATH resolves to SHARD_PATHA for non-UNICODE build */
	UTF16_ENCODE(filename);
	SHAddToRecentDocs(SHARD_PATHW, filename_16);
	UTF16_UN_ENCODE(filename);
}
