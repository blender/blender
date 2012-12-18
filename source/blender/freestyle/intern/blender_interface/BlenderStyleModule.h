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

#ifndef __BLENDERSTYLEMODULE_H__
#define __BLENDERSTYLEMODULE_H__

/** \file blender/freestyle/intern/blender_interface/BlenderStyleModule.h
 *  \ingroup freestyle
 */

#include "../stroke/StyleModule.h"
#include "../system/PythonInterpreter.h"

extern "C" {
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_text.h"
}

class BlenderStyleModule : public StyleModule
{
public:
	BlenderStyleModule(struct Text *text, const string &name, Interpreter *inter) : StyleModule(name, inter)
	{
		_text = text;
	}

	virtual ~BlenderStyleModule()
	{
		BKE_text_unlink(G.main, _text);
		BKE_libblock_free(&G.main->text, _text);
	}

protected:
	virtual int interpret()
	{
		PythonInterpreter* py_inter = dynamic_cast<PythonInterpreter*>(_inter);
		assert(py_inter != 0);
		return py_inter->interpretText(_text, getFileName());
	}

private:
	struct Text *_text;
};

#endif // __BLENDERSTYLEMODULE_H__
