/* 
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */

#ifndef EXPP_matrix_h
#define EXPP_matrix_h

#include "Python.h"
#include "BLI_arithb.h"
#include "vector.h"
#include "gen_utils.h"
#include "Types.h"
#include "quat.h"
#include "euler.h"

#define Matrix_CheckPyObject(v) ((v)->ob_type == &matrix_Type)

/*****************************/
/*    Matrix Python Object   */
/*****************************/
typedef float **ptRow;

typedef struct _Matrix {
	PyObject_VAR_HEAD

	ptRow matrix;
	int rowSize;
	int colSize;
	int flag;
		//0 - no coercion
		//1 - coerced from int
		//2 - coerced from float
} MatrixObject;

/*****************************************************************************/
/* Python API function prototypes.												*/
/*****************************************************************************/
PyObject *newMatrixObject(float * mat, int rowSize, int colSize);
PyObject *Matrix_Zero(MatrixObject *self);
PyObject *Matrix_Identity(MatrixObject *self);
PyObject *Matrix_Transpose(MatrixObject *self);
PyObject *Matrix_Determinant(MatrixObject *self);
PyObject *Matrix_Invert(MatrixObject *self);
PyObject *Matrix_TranslationPart(MatrixObject *self);
PyObject *Matrix_RotationPart(MatrixObject *self);
PyObject *Matrix_Resize4x4(MatrixObject *self);
PyObject *Matrix_toEuler(MatrixObject *self);
PyObject *Matrix_toQuat(MatrixObject *self);

#endif /* EXPP_matrix_H */
