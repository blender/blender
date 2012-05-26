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

/** \file ghost/GHOST_Path-api.h
 *  \ingroup GHOST
 */


#ifndef __GHOST_PATH_API_H__
#define __GHOST_PATH_API_H__

#include "GHOST_Types.h"

#ifdef __cplusplus
extern "C" { 
#endif

GHOST_DECLARE_HANDLE(GHOST_SystemPathsHandle);

/**
 * Creates the one and only instance of the system path access.
 * @return An indication of success.
 */
extern GHOST_TSuccess GHOST_CreateSystemPaths(void);

/**
 * Disposes the one and only system.
 * @return An indication of success.
 */
extern GHOST_TSuccess GHOST_DisposeSystemPaths(void);

/**
 * Determine the base dir in which shared resources are located. It will first try to use
 * "unpack and run" path, then look for properly installed path, including versioning.
 * @return Unsigned char string pointing to system dir (eg /usr/share/blender/).
 */
extern const GHOST_TUns8 *GHOST_getSystemDir(int version, const char *versionstr);

/**
 * Determine the base dir in which user configuration is stored, including versioning.
 * @return Unsigned char string pointing to user dir (eg ~).
 */
extern const GHOST_TUns8 *GHOST_getUserDir(int version, const char *versionstr);


/**
 * Determine the dir in which the binary file is found.
 * @return Unsigned char string pointing to binary dir (eg ~/usr/local/bin/).
 */
extern const GHOST_TUns8 *GHOST_getBinaryDir(void);

/**
 * Add the file to the operating system most recently used files
 */
extern void GHOST_addToSystemRecentFiles(const char *filename);

#ifdef __cplusplus
} 
#endif

#endif
