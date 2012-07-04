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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemPaths.h
 *  \ingroup GHOST
 */

#ifndef __GHOST_SYSTEMPATHS_H__
#define __GHOST_SYSTEMPATHS_H__

#include "GHOST_ISystemPaths.h"

class GHOST_SystemPaths : public GHOST_ISystemPaths
{
protected:
	/**
	 * Constructor.
	 * Protected default constructor to force use of static createSystem member.
	 */
	GHOST_SystemPaths() {};

	/**
	 * Destructor.
	 * Protected default constructor to force use of static dispose member.
	 */
	virtual ~GHOST_SystemPaths() {};

public:

	/**
	 * Determine the base dir in which shared resources are located. It will first try to use
	 * "unpack and run" path, then look for properly installed path, including versioning.
	 * @return Unsigned char string pointing to system dir (eg /usr/share/blender/).
	 */
	virtual const GHOST_TUns8 *getSystemDir(int version, const char *versionstr) const = 0;

	/**
	 * Determine the base dir in which user configuration is stored, including versioning.
	 * If needed, it will create the base directory.
	 * @return Unsigned char string pointing to user dir (eg ~/.blender/).
	 */
	virtual const GHOST_TUns8 *getUserDir(int version, const char *versionstr) const = 0;

	/**
	 * Determine the directory of the current binary
	 * @return Unsigned char string pointing to the binary dir
	 */
	virtual const GHOST_TUns8 *getBinaryDir() const = 0;

	/**
	 * Add the file to the operating system most recently used files
	 */
	virtual void addToSystemRecentFiles(const char *filename) const = 0;
};

#endif


