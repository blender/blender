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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OPENVDB_UTIL_H__
#define __OPENVDB_UTIL_H__

#include <openvdb/openvdb.h>
#include <openvdb/util/CpuTimer.h>

#define CATCH_KEYERROR \
	catch (const openvdb::KeyError &e) { \
		std::cerr << e.what() << '\n'; \
	}

//#define DEBUG_TIME

/* A utility class which prints the time elapsed during its lifetime, useful for
 * e.g. timing the overall execution time of a function */
class ScopeTimer {
	std::string m_message;
	openvdb::util::CpuTimer m_timer;

public:
	ScopeTimer(const std::string &message);
	~ScopeTimer();
};

#ifdef DEBUG_TIME
#	define Timer(x) \
		ScopeTimer prof(x);
#else
#	define Timer(x)
#endif

#endif /* __OPENVDB_UTIL_H__ */
