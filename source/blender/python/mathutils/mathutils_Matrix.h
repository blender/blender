/* 
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/python/generic/mathutils_Matrix.h
 *  \ingroup pygen
 */


#ifndef MATHUTILS_MATRIX_H
#define MATHUTILS_MATRIX_H

extern PyTypeObject matrix_Type;
#define MatrixObject_Check(_v) PyObject_TypeCheck((_v), &matrix_Type)
#define MATRIX_MAX_DIM 4

typedef struct {
	BASE_MATH_MEMBERS(contigPtr)
	float *matrix[MATRIX_MAX_DIM];		/* ptr to the contigPtr (accessor) */
	unsigned short row_size;
	unsigned short col_size;
} MatrixObject;

/*struct data contains a pointer to the actual data that the
object uses. It can use either PyMem allocated data (which will
be stored in py_data) or be a wrapper for data allocated through
blender (stored in blend_data). This is an either/or struct not both*/

/*prototypes*/
PyObject *newMatrixObject(float *mat, const unsigned short row_size, const unsigned short col_size, int type, PyTypeObject *base_type);
PyObject *newMatrixObject_cb(PyObject *user, int row_size, int col_size, int cb_type, int cb_subtype);

extern int mathutils_matrix_vector_cb_index;
extern struct Mathutils_Callback mathutils_matrix_vector_cb;

void matrix_as_3x3(float mat[3][3], MatrixObject *self);

#endif /* MATHUTILS_MATRIX_H */
