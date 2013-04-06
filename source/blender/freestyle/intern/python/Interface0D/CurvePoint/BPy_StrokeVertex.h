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

/** \file source/blender/freestyle/intern/python/Interface0D/CurvePoint/BPy_StrokeVertex.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_STROKEVERTEX_H__
#define __FREESTYLE_PYTHON_STROKEVERTEX_H__

#include "../BPy_CurvePoint.h"
#include "../../../stroke/Stroke.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject StrokeVertex_Type;

#define BPy_StrokeVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&StrokeVertex_Type))

/*---------------------------Python BPy_StrokeVertex structure definition----------*/
typedef struct {
	BPy_CurvePoint py_cp;
	StrokeVertex *sv;
} BPy_StrokeVertex;

/*---------------------------Python BPy_StrokeVertex visible prototypes-----------*/

void StrokeVertex_mathutils_register_callback();

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_STROKEVERTEX_H__ */
