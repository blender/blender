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

#ifndef __FREESTYLE_INTERPRETER_H__
#define __FREESTYLE_INTERPRETER_H__

/** \file blender/freestyle/intern/system/Interpreter.h
 *  \ingroup freestyle
 *  \brief Base Class of all script interpreters
 *  \author Emmanuel Turquin
 *  \date 17/04/2003
 */

#include <string>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

class LIB_SYSTEM_EXPORT Interpreter
{
public:
	Interpreter()
	{
		_language = "Unknown";
	}

	virtual ~Interpreter() {}; //soc

	virtual int interpretFile(const string& filename) = 0;

	virtual string getLanguage() const
	{
		return _language;
	}

	virtual void reset() = 0;

protected:
	string _language;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Interpreter")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_INTERPRETER_H__
