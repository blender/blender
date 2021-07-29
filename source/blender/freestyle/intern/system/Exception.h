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

#ifndef __FREESTYLE_EXCEPTION_H__
#define __FREESTYLE_EXCEPTION_H__

/** \file blender/freestyle/intern/system/Exception.h
 *  \ingroup freestyle
 *  \brief Singleton to manage exceptions
 *  \author Stephane Grabli
 *  \date 10/01/2003
 */

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class Exception
{
public:
	typedef enum {
		NO_EXCEPTION,
		UNDEFINED,
	} exception_type;

	static int getException()
	{
		exception_type e = _exception;
		_exception = NO_EXCEPTION;
		return e;
	}

	static int raiseException(exception_type exception = UNDEFINED)
	{
		_exception = exception;
		return _exception;
	}

	static void reset()
	{
		_exception = NO_EXCEPTION;
	}

private:
	static exception_type _exception;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Exception")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_EXCEPTION_H__
