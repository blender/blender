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

/** \file source/blender/freestyle/intern/python/UnaryFunction1D/BPy_UnaryFunction1DFloat.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H__
#define __FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H__

#include "../BPy_UnaryFunction1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction1DFloat_Type;

#define BPy_UnaryFunction1DFloat_Check(v) (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction1DFloat_Type))

/*---------------------------Python BPy_UnaryFunction1DFloat structure definition----------*/
typedef struct {
	BPy_UnaryFunction1D py_uf1D;
	UnaryFunction1D<float> *uf1D_float;
} BPy_UnaryFunction1DFloat;

/*---------------------------Python BPy_UnaryFunction1DFloat visible prototypes-----------*/
int UnaryFunction1DFloat_Init(PyObject *module);


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYFUNCTION1DFLOAT_H__ */
