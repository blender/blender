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
 * Contributor(s): Damien Plisson 2010
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemPathsCarbon.cpp
 *  \ingroup GHOST
 */


#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include "GHOST_SystemPathsCarbon.h"


/***/

GHOST_SystemPathsCarbon::GHOST_SystemPathsCarbon() 
{
}

GHOST_SystemPathsCarbon::~GHOST_SystemPathsCarbon()
{
}

const GHOST_TUns8 *GHOST_SystemPathsCarbon::getSystemDir() const
{
	return (GHOST_TUns8 *)"/Library/Application Support";
}

const GHOST_TUns8 *GHOST_SystemPathsCarbon::getUserDir() const
{
	static char usrPath[256] = "";
	char *env = getenv("HOME");
	
	if (env) {
		strncpy(usrPath, env, 245);
		usrPath[245] = 0;
		strcat(usrPath, "/Library/Application Support");
		return (GHOST_TUns8 *) usrPath;
	}
	else
		return NULL;
}

const GHOST_TUns8 *GHOST_SystemPathsCarbon::getBinaryDir() const
{
	CFURLRef bundleURL;
	CFStringRef pathStr;
	static char path[256];
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	
	bundleURL = CFBundleCopyBundleURL(mainBundle);
	pathStr = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
	CFStringGetCString(pathStr, path, 255, kCFStringEncodingASCII);
	CFRelease(pathStr);
	CFRelease(bundleURL);
	return (GHOST_TUns8 *)path;
}

void GHOST_SystemPathsCarbon::addToSystemRecentFiles(const char *filename) const
{
	/* XXXXX TODO: Implementation for Carbon if possible */

}
