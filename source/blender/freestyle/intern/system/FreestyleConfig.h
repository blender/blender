/*
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
 */

#ifndef __FREESTYLE_CONFIG_H__
#define __FREESTYLE_CONFIG_H__

/** \file \ingroup freestyle
 *  \brief Configuration definitions
 */

#include <string>

using namespace std;

namespace Freestyle {

namespace Config {

// Directory separators
// TODO Use Blender's stuff for such things!
#ifdef WIN32
	static const string DIR_SEP("\\");
	static const string PATH_SEP(";");
#else
	static const string DIR_SEP("/");
	static const string PATH_SEP(":");
#endif // WIN32

} // end of namespace Config

} /* namespace Freestyle */

#endif // __FREESTYLE_CONFIG_H__
