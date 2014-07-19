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

#ifndef __BLENDERSTYLEMODULE_H__
#define __BLENDERSTYLEMODULE_H__

/** \file blender/freestyle/intern/blender_interface/BlenderStyleModule.h
 *  \ingroup freestyle
 */

#include "../stroke/StyleModule.h"
#include "../system/PythonInterpreter.h"

extern "C" {
#include "BLI_utildefines.h" // BLI_assert()

struct Scene;
struct Text;
}

namespace Freestyle {

class BlenderStyleModule : public StyleModule
{
public:
	BlenderStyleModule(struct Text *text, const string &name, Interpreter *inter) : StyleModule(name, inter)
	{
		_text = text;
	}

	virtual ~BlenderStyleModule()
	{
	}

protected:
	virtual int interpret()
	{
		PythonInterpreter *py_inter = dynamic_cast<PythonInterpreter*>(_inter);
		BLI_assert(py_inter != 0);
		return py_inter->interpretText(_text, getFileName());
	}

private:
	struct Text *_text;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BlenderStyleModule")
#endif

};

} /* namespace Freestyle */

#endif // __BLENDERSTYLEMODULE_H__
