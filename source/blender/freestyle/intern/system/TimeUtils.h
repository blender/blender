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

#ifndef __FREESTYLE_TIME_UTILS_H__
#define __FREESTYLE_TIME_UTILS_H__

/** \file blender/freestyle/intern/system/TimeUtils.h
 *  \ingroup freestyle
 *  \brief Class to measure ellapsed time
 *  \author Stephane Grabli
 *  \date 10/04/2002
 */

#include <time.h>

#include "FreestyleConfig.h"

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
};

#endif // __FREESTYLE_TIME_UTILS_H__
