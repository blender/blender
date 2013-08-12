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

#ifndef __FREESTYLE_TIME_STAMP_H__
#define __FREESTYLE_TIME_STAMP_H__

/** \file blender/freestyle/intern/system/TimeStamp.h
 *  \ingroup freestyle
 *  \brief Class defining a singleton used as timestamp
 *  \author Stephane Grabli
 *  \date 12/12/2002
 */

#include "FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class LIB_SYSTEM_EXPORT TimeStamp
{
public:
	static inline TimeStamp *instance()
	{
		return &_instance;
	}

	inline unsigned getTimeStamp() const
	{
		return _time_stamp;
	}

	inline void increment()
	{
		++_time_stamp;
	}

	inline void reset()
	{
		_time_stamp = 1;
	}

protected:
	TimeStamp()
	{
		_time_stamp = 1;
	}

	TimeStamp(const TimeStamp&) {}

private:
	static TimeStamp _instance;
	unsigned _time_stamp;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:TimeStamp")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_TIME_STAMP_H__
