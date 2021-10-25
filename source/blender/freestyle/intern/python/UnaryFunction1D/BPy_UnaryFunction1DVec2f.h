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

/** \file source/blender/freestyle/intern/python/UnaryFunction1D/BPy_UnaryFunction1DVec2f.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H__
#define __FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H__

#include "../BPy_UnaryFunction1D.h"

#include "../../geometry/Geom.h"
using namespace Geometry;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DVec2f_Type;

#define BPy_UnaryFunction1DVec2f_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DVec2f_Type))

/*---------------------------Python BPy_UnaryFunction1DVec2f structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<Vec2f> *uf1D_vec2f;
} BPy_UnaryFunction1DVec2f;

/*---------------------------Python BPy_UnaryFunction1DVec2f visible prototypes-----------*/
int UnaryFunction1DVec2f_Init(PyObject *module);


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYFUNCTION1DVEC2F_H__ */
