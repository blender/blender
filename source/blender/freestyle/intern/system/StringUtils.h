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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_STRING_UTILS_H__
#define __FREESTYLE_STRING_UTILS_H__

/** \file blender/freestyle/intern/system/StringUtils.h
 *  \ingroup freestyle
 *  \brief String utilities
 *  \author Emmanuel Turquin
 *  \date 20/05/2003
 */

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "FreestyleConfig.h"

//soc
extern "C" {

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

}

using namespace std;

namespace StringUtils {

LIB_SYSTEM_EXPORT
void getPathName(const string& path, const string& base, vector<string>& pathnames);
string toAscii(const string &str);
const char *toAscii(const char *str);

// STL related
struct ltstr
{
	bool operator()(const char *s1, const char *s2) const
	{
		return strcmp(s1, s2) < 0;
	}
};

} // end of namespace StringUtils

#endif // __FREESTYLE_STRING_UTILS_H__
