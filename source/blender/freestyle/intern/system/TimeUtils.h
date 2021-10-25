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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_TIME_UTILS_H__
#define __FREESTYLE_TIME_UTILS_H__

/** \file blender/freestyle/intern/system/TimeUtils.h
 *  \ingroup freestyle
 *  \brief Class to measure elapsed time
 *  \author Stephane Grabli
 *  \date 10/04/2002
 */

#include <time.h>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Chronometer
{
public:
	inline Chronometer() {}
	inline ~Chronometer() {}

	inline clock_t start()
	{
		_start = clock();
		return _start;
	}

	inline double stop()
	{
		clock_t stop = clock();
		return (double)(stop - _start) / CLOCKS_PER_SEC;
	}

private:
	clock_t _start;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Chronometer")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_TIME_UTILS_H__
