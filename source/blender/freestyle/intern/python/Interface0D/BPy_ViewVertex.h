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

/** \file source/blender/freestyle/intern/python/Interface0D/BPy_ViewVertex.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_VIEWVERTEX_H__
#define __FREESTYLE_PYTHON_VIEWVERTEX_H__

#include "../../view_map/ViewMap.h"
#include "../BPy_Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject ViewVertex_Type;

#define BPy_ViewVertex_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&ViewVertex_Type))

/*---------------------------Python BPy_ViewVertex structure definition----------*/
typedef struct {
	BPy_Interface0D py_if0D;
	ViewVertex *vv;
} BPy_ViewVertex;

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_VIEWVERTEX_H__ */
