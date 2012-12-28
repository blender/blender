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

#ifndef __FREESTYLE_ITERATOR_H__
#define __FREESTYLE_ITERATOR_H__

/** \file blender/freestyle/intern/system/Iterator.h
 *  \ingroup freestyle
 */

#include <iostream>
#include <string>

using namespace std;

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
};

#endif // __FREESTYLE_ITERATOR_H__
