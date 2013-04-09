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

/** \file source/blender/freestyle/intern/python/BPy_UnaryPredicate1D.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYPREDICATE1D_H__
#define __FREESTYLE_PYTHON_UNARYPREDICATE1D_H__

#include <Python.h>

#include "../stroke/Predicates1D.h"

using namespace Freestyle;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryPredicate1D_Type;

#define BPy_UnaryPredicate1D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryPredicate1D_Type))

/*---------------------------Python BPy_UnaryPredicate1D structure definition----------*/
typedef struct {
	PyObject_HEAD
	UnaryPredicate1D *up1D;
} BPy_UnaryPredicate1D;

/*---------------------------Python BPy_UnaryPredicate1D visible prototypes-----------*/

int UnaryPredicate1D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYPREDICATE1D_H__ */
