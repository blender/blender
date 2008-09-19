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
#include "vector.h"
#include "matrix.h"
#include "quat.h"
#include "euler.h"
#include "point.h"

PyObject *Mathutils_Init( char * from );
PyObject *row_vector_multiplication(VectorObject* vec, MatrixObject * mat);
PyObject *column_vector_multiplication(MatrixObject * mat, VectorObject* vec);
PyObject *row_point_multiplication(PointObject* pt, MatrixObject * mat);
PyObject *column_point_multiplication(MatrixObject * mat, PointObject* pt);
PyObject *quat_rotation(PyObject *arg1, PyObject *arg2);

PyObject *M_Mathutils_Rand(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Vector(PyObject * self, PyObject * args);
PyObject *M_Mathutils_CrossVecs(PyObject * self, PyObject * args);
PyObject *M_Mathutils_DotVecs(PyObject * self, PyObject * args);
PyObject *M_Mathutils_AngleBetweenVecs(PyObject * self, PyObject * args);
PyObject *M_Mathutils_MidpointVecs(PyObject * self, PyObject * args);
PyObject *M_Mathutils_ProjectVecs(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Matrix(PyObject * self, PyObject * args);
PyObject *M_Mathutils_RotationMatrix(PyObject * self, PyObject * args);
PyObject *M_Mathutils_TranslationMatrix(PyObject * self, VectorObject * value);
PyObject *M_Mathutils_ScaleMatrix(PyObject * self, PyObject * args);
PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject * self, PyObject * args);
PyObject *M_Mathutils_ShearMatrix(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Quaternion(PyObject * self, PyObject * args);
PyObject *M_Mathutils_CrossQuats(PyObject * self, PyObject * args);
PyObject *M_Mathutils_DotQuats(PyObject * self, PyObject * args);
PyObject *M_Mathutils_DifferenceQuats(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Slerp(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Euler(PyObject * self, PyObject * args);
PyObject *M_Mathutils_Intersect( PyObject * self, PyObject * args );
PyObject *M_Mathutils_TriangleArea( PyObject * self, PyObject * args );
PyObject *M_Mathutils_TriangleNormal( PyObject * self, PyObject * args );
PyObject *M_Mathutils_QuadNormal( PyObject * self, PyObject * args );
PyObject *M_Mathutils_LineIntersect( PyObject * self, PyObject * args );
PyObject *M_Mathutils_Point(PyObject * self, PyObject * args);
//DEPRECATED	
PyObject *M_Mathutils_CopyMat(PyObject * self, PyObject * args);
PyObject *M_Mathutils_CopyVec(PyObject * self, PyObject * args);
PyObject *M_Mathutils_CopyQuat(PyObject * self, PyObject * args);
PyObject *M_Mathutils_CopyEuler(PyObject * self, PyObject * args);
PyObject *M_Mathutils_RotateEuler(PyObject * self, PyObject * args);
PyObject *M_Mathutils_MatMultVec(PyObject * self, PyObject * args);
PyObject *M_Mathutils_VecMultMat(PyObject * self, PyObject * args);

#endif				/* EXPP_Mathutils_H */
