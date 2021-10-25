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

/** \file source/blender/freestyle/intern/python/BPy_UnaryPredicate0D.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYPREDICATE0D_H__
#define __FREESTYLE_PYTHON_UNARYPREDICATE0D_H__

extern "C" {
#include <Python.h>
}

#include "../stroke/Predicates0D.h"

using namespace Freestyle;

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryPredicate0D_Type;

#define BPy_UnaryPredicate0D_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryPredicate0D_Type))

/*---------------------------Python BPy_UnaryPredicate0D structure definition----------*/
typedef struct {
	PyObject_HEAD
	UnaryPredicate0D *up0D;
} BPy_UnaryPredicate0D;

/*---------------------------Python BPy_UnaryPredicate0D visible prototypes-----------*/

int UnaryPredicate0D_Init(PyObject *module);

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYPREDICATE0D_H__ */
