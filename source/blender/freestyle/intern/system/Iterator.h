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

#ifndef __FREESTYLE_ITERATOR_H__
#define __FREESTYLE_ITERATOR_H__

/** \file blender/freestyle/intern/system/Iterator.h
 *  \ingroup freestyle
 */

#include <iostream>
#include <string>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

class Iterator
{
public:
	virtual ~Iterator() {}

	virtual string getExactTypeName() const
	{
		return "Iterator";
	}

	virtual int increment()
	{
		cerr << "Warning: increment() not implemented" << endl;
		return 0;
	}

	virtual int decrement()
	{
		cerr << "Warning: decrement() not implemented" << endl;
		return 0;
	}

	virtual bool isBegin() const
	{
		cerr << "Warning: isBegin() not implemented" << endl;
		return false;
	}

	virtual bool isEnd() const
	{
		cerr << "Warning:  isEnd() not implemented" << endl;
		return false;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Iterator")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_ITERATOR_H__
