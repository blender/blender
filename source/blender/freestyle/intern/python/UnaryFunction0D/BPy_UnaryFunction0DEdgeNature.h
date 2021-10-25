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

/** \file source/blender/freestyle/intern/python/UnaryFunction0D/BPy_UnaryFunction0DEdgeNature.h
 *  \ingroup freestyle
 */

#ifndef __FREESTYLE_PYTHON_UNARYFUNCTION0DEDGENATURE_H__
#define __FREESTYLE_PYTHON_UNARYFUNCTION0DEDGENATURE_H__

#include "../BPy_UnaryFunction0D.h"

#include "../../winged_edge/Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

extern PyTypeObject UnaryFunction0DEdgeNature_Type;

#define BPy_UnaryFunction0DEdgeNature_Check(v) \
            (PyObject_IsInstance((PyObject *)v, (PyObject *)&UnaryFunction0DEdgeNature_Type))

/*---------------------------Python BPy_UnaryFunction0DEdgeNature structure definition----------*/
typedef struct {
	BPy_UnaryFunction0D py_uf0D;
	UnaryFunction0D<Nature::EdgeNature> *uf0D_edgenature;
} BPy_UnaryFunction0DEdgeNature;

/*---------------------------Python BPy_UnaryFunction0DEdgeNature visible prototypes-----------*/
int UnaryFunction0DEdgeNature_Init(PyObject *module);


///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif /* __FREESTYLE_PYTHON_UNARYFUNCTION0DEDGENATURE_H__ */
