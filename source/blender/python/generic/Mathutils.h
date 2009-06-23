/* 
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
*/
//Include this file for access to vector, quat, matrix, euler, etc...

#ifndef EXPP_Mathutils_H
#define EXPP_Mathutils_H

#include <Python.h>
#include "../intern/bpy_compat.h"
#include "vector.h"
#include "matrix.h"
#include "quat.h"
#include "euler.h"

PyObject *Mathutils_Init( const char * from );

PyObject *quat_rotation(PyObject *arg1, PyObject *arg2);

int EXPP_FloatsAreEqual(float A, float B, int floatSteps);
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps);


#define Py_PI  3.14159265358979323846

#define Py_NEW  1
#define Py_WRAP 2


/* Mathutils is used by the BGE and Blender so have to define 
 * some things here for luddite mac users of py2.3 */
#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE  return Py_INCREF(Py_None), Py_None
#endif
#ifndef Py_RETURN_FALSE
#define Py_RETURN_FALSE  return Py_INCREF(Py_False), Py_False
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE  return Py_INCREF(Py_True), Py_True
#endif

typedef struct Mathutils_Callback Mathutils_Callback;
struct Mathutils_Callback {
	int		(*check)(PyObject *user);					/* checks the user is still valid */
	int		(*get)(PyObject *user, int subtype, float *vec_from);	/* gets the vector from the user */
	int		(*set)(PyObject *user, int subtype, float *vec_to);	/* sets the users vector values once the vector is modified */
	int		(*get_index)(PyObject *user, int subtype, float *vec_from,	int index);	/* same as above but only for an index */
	int		(*set_index)(PyObject *user, int subtype, float *vec_to,	int index);	/* same as above but only for an index */
};

int Mathutils_RegisterCallback(Mathutils_Callback *cb);

int Vector_ReadCallback(VectorObject *self);
int Vector_WriteCallback(VectorObject *self);
int Vector_ReadIndexCallback(VectorObject *self, int index);
int Vector_WriteIndexCallback(VectorObject *self, int index);

#endif				/* EXPP_Mathutils_H */
