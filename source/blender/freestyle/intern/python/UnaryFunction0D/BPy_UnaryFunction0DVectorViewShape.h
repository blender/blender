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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/BPy_UnaryFunction0DVectorViewShape.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H__
#define __FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H__

#include "../BPy_UnaryFunction0D.h"

#include <vector>
#include "../../view_map/ViewMap.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

#include <Python.h>

extern PyTypeObject UnaryFunction0DVectorViewShape_Type;

#define BPy_UnaryFunction0DVectorViewShape_Check(v) \
            (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DVectorViewShape_Type))

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D< std::vector<ViewShape*> > *uf0D_vectorviewshape;
} BPy_UnaryFunction0DVectorViewShape;

/*---------------------------Python BPy_UnaryFunction0DVectorViewShape visible prototypes-----------*/
int UnaryFunction0DVectorViewShape_Init(PyObject *module);


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYFUNCTION0DVECTORVIEWSHAPE_H__ */
