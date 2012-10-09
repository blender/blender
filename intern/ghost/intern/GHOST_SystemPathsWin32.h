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

/** \file ghost/intern/GHOST_SystemPathsWin32.h
 *  \ingroup GHOST
 */


#ifndef __GHOST_SYSTEMPATHSWIN32_H__
#define __GHOST_SYSTEMPATHSWIN32_H__

#ifndef WIN32
#error WIN32 only!
#endif // WIN32

#define _WIN32_WINNT 0x501 // require Windows XP or newer
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "GHOST_SystemPaths.h"


/**
 * WIN32 Implementation of GHOST_SystemPaths class.
 * \see GHOST_SystemPaths.
 * \author	Andrea Weikert
 * \date	August 1, 2010
 */
class GHOST_SystemPathsWin32 : public GHOST_SystemPaths {
public:
	/**
	 * Constructor.
	 */
	GHOST_SystemPathsWin32();

	/**
	 * Destructor.
	 */
	virtual ~GHOST_SystemPathsWin32();

	/**
	 * Determine the base dir in which shared resources are located. It will first try to use
	 * "unpack and run" path, then look for properly installed path, including versioning.
	 * \return Unsigned char string pointing to system dir (eg /usr/share/).
	 */
	const GHOST_TUns8 *getSystemDir(int version, const char *versionstr) const;

	/**
	 * Determine the base dir in which user configuration is stored, including versioning.
	 * If needed, it will create the base directory.
	 * \return Unsigned char string pointing to user dir (eg ~/).
	 */
	const GHOST_TUns8 *getUserDir(int version, const char *versionstr) const;

	/**
	 * Determine the directory of the current binary
	 * \return Unsigned char string pointing to the binary dir
	 */
	const GHOST_TUns8 *getBinaryDir() const;

	/**
	 * Add the file to the operating system most recently used files
	 */
	void addToSystemRecentFiles(const char *filename) const;
};

#endif // __GHOST_SYSTEMPATHSWIN32_H__

