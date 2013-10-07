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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemPathsX11.cpp
 *  \ingroup GHOST
 */


#include "GHOST_SystemPathsX11.h"

#include "GHOST_Debug.h"

// For timing

#include <sys/time.h>
#include <unistd.h>

#include <stdio.h>   /* for fprintf only */
#include <cstdlib>   /* for exit */

#include <pwd.h>     /* for get home without use getenv() */
#include <string>

using std::string;

#ifdef PREFIX
static const char *static_path = PREFIX "/share";
#else
static const char *static_path = NULL;
#endif

GHOST_SystemPathsX11::GHOST_SystemPathsX11()
{
}

GHOST_SystemPathsX11::~GHOST_SystemPathsX11()
{
}

const GHOST_TUns8 *GHOST_SystemPathsX11::getSystemDir(int, const char *versionstr) const
{
	/* no prefix assumes a portable build which only uses bundled scripts */
	if (static_path) {
		static string system_path = string(static_path) + "/blender/" + versionstr;
		return (GHOST_TUns8 *)system_path.c_str();
	}

	return NULL;
}

const GHOST_TUns8 *GHOST_SystemPathsX11::getUserDir(int version, const char *versionstr) const
{
	static string user_path = "";
	static int last_version = 0;

	/* in blender 2.64, we migrate to XDG. to ensure the copy previous settings
	 * operator works we give a different path depending on the requested version */
	if (version < 264) {
		if (user_path.empty() || last_version != version) {
			const char *home = getenv("HOME");

			last_version = version;

			if (home) {
				user_path = string(home) + "/.blender/" + versionstr;
			}
			else {
				return NULL;
			}
		}
		return (GHOST_TUns8 *)user_path.c_str();
	}
	else {
		if (user_path.empty() || last_version != version) {
			const char *home = getenv("XDG_CONFIG_HOME");

			last_version = version;

			if (home) {
				user_path = string(home) + "/blender/" + versionstr;
			}
			else {
				home = getenv("HOME");

				if (home == NULL)
					home = getpwuid(getuid())->pw_dir;

				user_path = string(home) + "/.config/blender/" + versionstr;
			}
		}

		return (const GHOST_TUns8 *)user_path.c_str();
	}
}

const GHOST_TUns8 *GHOST_SystemPathsX11::getBinaryDir() const
{
	return NULL;
}

void GHOST_SystemPathsX11::addToSystemRecentFiles(const char *filename) const
{
	/* XXXXX TODO: Implementation for X11 if possible */

}
