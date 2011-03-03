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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Michel Selten & Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/mathutils_Matrix.c
 *  \ingroup pygen
 */


#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

static PyObject *Matrix_copy(MatrixObject *self);
static int Matrix_ass_slice(MatrixObject *self, int begin, int end, PyObject *value);
static PyObject *matrix__apply_to_copy(PyNoArgsFunction matrix_func, MatrixObject *self);

/* matrix vector callbacks */
int mathutils_matrix_vector_cb_index= -1;

static int mathutils_matrix_vector_check(BaseMathObject *bmo)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;
	return BaseMath_ReadCallback(self);
}

static int mathutils_matrix_vector_get(BaseMathObject *bmo, int subtype)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;
	int i;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	for(i=0; i < self->col_size; i++)
		bmo->data[i]= self->matrix[subtype][i];

	return 0;
}

static int mathutils_matrix_vector_set(BaseMathObject *bmo, int subtype)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;
	int i;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	for(i=0; i < self->col_size; i++)
		self->matrix[subtype][i]= bmo->data[i];

	(void)BaseMath_WriteCallback(self);
	return 0;
}

static int mathutils_matrix_vector_get_index(BaseMathObject *bmo, int subtype, int index)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	bmo->data[index]= self->matrix[subtype][index];
	return 0;
}

static int mathutils_matrix_vector_set_index(BaseMathObject *bmo, int subtype, int index)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	self->matrix[subtype][index]= bmo->data[index];

	(void)BaseMath_WriteCallback(self);
	return 0;
}

Mathutils_Callback mathutils_matrix_vector_cb = {
	mathutils_matrix_vector_check,
	mathutils_matrix_vector_get,
	mathutils_matrix_vector_set,
	mathutils_matrix_vector_get_index,
	mathutils_matrix_vector_set_index
};
/* matrix vector callbacks, this is so you can do matrix[i][j] = val  */

//----------------------------------mathutils.Matrix() -----------------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//create a new matrix type
static PyObject *Matrix_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	if(kwds && PyDict_Size(kwds)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Matrix(): takes no keyword args");
		return NULL;
	}

	switch(PyTuple_GET_SIZE(args)) {
		case 0:
			return (PyObject *) newMatrixObject(NULL, 4, 4, Py_NEW, type);
		case 1:
		{
			PyObject *arg= PyTuple_GET_ITEM(args, 0);

			const unsigned short row_size= PySequence_Size(arg); /* -1 is an error, size checks will accunt for this */

			if(IN_RANGE_INCL(row_size, 2, 4)) {
				PyObject *item= PySequence_GetItem(arg, 0);
				const unsigned short col_size= PySequence_Size(item);
				Py_XDECREF(item);

				if(IN_RANGE_INCL(col_size, 2, 4)) {
					/* sane row & col size, new matrix and assign as slice  */
					PyObject *matrix= newMatrixObject(NULL, row_size, col_size, Py_NEW, type);
					if(Matrix_ass_slice((MatrixObject *)matrix, 0, INT_MAX, arg) == 0) {
						return matrix;
					}
					else { /* matrix ok, slice assignment not */
						Py_DECREF(matrix);
					}
				}
			}
		}
	}

	/* will overwrite error */
	PyErr_SetString(PyExc_TypeError, "mathutils.Matrix(): expects no args or 2-4 numeric sequences");
	return NULL;
}

static PyObject *matrix__apply_to_copy(PyNoArgsFunction matrix_func, MatrixObject *self)
{
	PyObject *ret= Matrix_copy(self);
	PyObject *ret_dummy= matrix_func(ret);
	if(ret_dummy) {
		Py_DECREF(ret_dummy);
		return (PyObject *)ret;
	}
	else { /* error */
		Py_DECREF(ret);
		return NULL;
	}
}

/* when a matrix is 4x4 size but initialized as a 3x3, re-assign values for 4x4 */
static void matrix_3x3_as_4x4(float mat[16])
{
	mat[10] = mat[8];
	mat[9] = mat[7];
	mat[8] = mat[6];
	mat[7] = 0.0f;
	mat[6] = mat[5];
	mat[5] = mat[4];
	mat[4] = mat[3];
	mat[3] = 0.0f;
}

/*-----------------------CLASS-METHODS----------------------------*/

//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static char C_Matrix_Rotation_doc[] =
".. classmethod:: Rotation(angle, size, axis)\n"
"\n"
"   Create a matrix representing a rotation.\n"
"\n"
"   :arg angle: The angle of rotation desired, in radians.\n"
"   :type angle: float\n"
"   :arg size: The size of the rotation matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :arg axis: a string in ['X', 'Y', 'Z'] or a 3D Vector Object (optional when size is 2).\n"
"   :type axis: string or :class:`Vector`\n"
"   :return: A new rotation matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *C_Matrix_Rotation(PyObject *cls, PyObject *args)
{
	PyObject *vec= NULL;
	const char *axis= NULL;
	int matSize;
	double angle; /* use double because of precision problems at high values */
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "di|O", &angle, &matSize, &vec)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.RotationMatrix(angle, size, axis): expected float int and a string or vector");
		return NULL;
	}

	if(vec && PyUnicode_Check(vec)) {
		axis= _PyUnicode_AsString((PyObject *)vec);
		if(axis==NULL || axis[0]=='\0' || axis[1]!='\0' || axis[0] < 'X' || axis[0] > 'Z') {
			PyErr_SetString(PyExc_TypeError, "mathutils.RotationMatrix(): 3rd argument axis value must be a 3D vector or a string in 'X', 'Y', 'Z'");
			return NULL;
		}
		else {
			/* use the string */
			vec= NULL;
		}
	}

	/* clamp angle between -360 and 360 in radians */
	angle= fmod(angle + M_PI*2, M_PI*4) - M_PI*2;

	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}
	if(matSize == 2 && (vec != NULL)) {
		PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): cannot create a 2x2 rotation matrix around arbitrary axis");
		return NULL;
	}
	if((matSize == 3 || matSize == 4) && (axis == NULL) && (vec == NULL)) {
		PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): axis of rotation for 3d and 4d matrices is required");
		return NULL;
	}

	/* check for valid vector/axis above */
	if(vec) {
		float tvec[3];

		if (mathutils_array_parse(tvec, 3, 3, vec, "mathutils.RotationMatrix(angle, size, axis), invalid 'axis' arg") == -1)
			return NULL;

		axis_angle_to_mat3((float (*)[3])mat, tvec, angle);
	}
	else if(matSize == 2) {
		//2D rotation matrix
		mat[0] = (float) cos (angle);
		mat[1] = (float) sin (angle);
		mat[2] = -((float) sin(angle));
		mat[3] = (float) cos(angle);
	} else if(strcmp(axis, "X") == 0) {
		//rotation around X
		mat[0] = 1.0f;
		mat[4] = (float) cos(angle);
		mat[5] = (float) sin(angle);
		mat[7] = -((float) sin(angle));
		mat[8] = (float) cos(angle);
	} else if(strcmp(axis, "Y") == 0) {
		//rotation around Y
		mat[0] = (float) cos(angle);
		mat[2] = -((float) sin(angle));
		mat[4] = 1.0f;
		mat[6] = (float) sin(angle);
		mat[8] = (float) cos(angle);
	} else if(strcmp(axis, "Z") == 0) {
		//rotation around Z
		mat[0] = (float) cos(angle);
		mat[1] = (float) sin(angle);
		mat[3] = -((float) sin(angle));
		mat[4] = (float) cos(angle);
		mat[8] = 1.0f;
	}
	else {
		/* should never get here */
		PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): unknown error");
		return NULL;
	}

	if(matSize == 4) {
		matrix_3x3_as_4x4(mat);
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}


static char C_Matrix_Translation_doc[] =
".. classmethod:: Translation(vector)\n"
"\n"
"   Create a matrix representing a translation.\n"
"\n"
"   :arg vector: The translation vector.\n"
"   :type vector: :class:`Vector`\n"
"   :return: An identity matrix with a translation.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *C_Matrix_Translation(PyObject *cls, PyObject *value)
{
	float mat[16], tvec[3];

	if (mathutils_array_parse(tvec, 3, 4, value, "mathutils.Matrix.Translation(vector), invalid vector arg") == -1)
		return NULL;

	/* create a identity matrix and add translation */
	unit_m4((float(*)[4]) mat);
	copy_v3_v3(mat + 12, tvec); /* 12, 13, 14 */
	return newMatrixObject(mat, 4, 4, Py_NEW, (PyTypeObject *)cls);
}
//----------------------------------mathutils.Matrix.Scale() -------------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static char C_Matrix_Scale_doc[] =
".. classmethod:: Scale(factor, size, axis)\n"
"\n"
"   Create a matrix representing a scaling.\n"
"\n"
"   :arg factor: The factor of scaling to apply.\n"
"   :type factor: float\n"
"   :arg size: The size of the scale matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :arg axis: Direction to influence scale. (optional).\n"
"   :type axis: :class:`Vector`\n"
"   :return: A new scale matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *C_Matrix_Scale(PyObject *cls, PyObject *args)
{
	PyObject *vec= NULL;
	int vec_size;
	float tvec[3];
	float factor;
	int matSize;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "fi|O:Matrix.Scale", &factor, &matSize, &vec)) {
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.Scale(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}
	if(vec) {
		vec_size= (matSize == 2 ? 2 : 3);
		if(mathutils_array_parse(tvec, vec_size, vec_size, vec, "Matrix.Scale(factor, size, axis), invalid 'axis' arg") == -1) {
			return NULL;
		}
	}
	if(vec == NULL) {	//scaling along axis
		if(matSize == 2) {
			mat[0] = factor;
			mat[3] = factor;
		} else {
			mat[0] = factor;
			mat[4] = factor;
			mat[8] = factor;
		}
	}
	else { //scaling in arbitrary direction
		//normalize arbitrary axis
		float norm = 0.0f;
		int x;
		for(x = 0; x < vec_size; x++) {
			norm += tvec[x] * tvec[x];
		}
		norm = (float) sqrt(norm);
		for(x = 0; x < vec_size; x++) {
			tvec[x] /= norm;
		}
		if(matSize == 2) {
			mat[0] = 1 + ((factor - 1) *(tvec[0] * tvec[0]));
			mat[1] =     ((factor - 1) *(tvec[0] * tvec[1]));
			mat[2] =     ((factor - 1) *(tvec[0] * tvec[1]));
			mat[3] = 1 + ((factor - 1) *(tvec[1] * tvec[1]));
		} else {
			mat[0] = 1 + ((factor - 1) *(tvec[0] * tvec[0]));
			mat[1] =     ((factor - 1) *(tvec[0] * tvec[1]));
			mat[2] =     ((factor - 1) *(tvec[0] * tvec[2]));
			mat[3] =     ((factor - 1) *(tvec[0] * tvec[1]));
			mat[4] = 1 + ((factor - 1) *(tvec[1] * tvec[1]));
			mat[5] =     ((factor - 1) *(tvec[1] * tvec[2]));
			mat[6] =     ((factor - 1) *(tvec[0] * tvec[2]));
			mat[7] =     ((factor - 1) *(tvec[1] * tvec[2]));
			mat[8] = 1 + ((factor - 1) *(tvec[2] * tvec[2]));
		}
	}
	if(matSize == 4) {
		matrix_3x3_as_4x4(mat);
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}
//----------------------------------mathutils.Matrix.OrthoProjection() ---
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static char C_Matrix_OrthoProjection_doc[] =
".. classmethod:: OrthoProjection(axis, size)\n"
"\n"
"   Create a matrix to represent an orthographic projection.\n"
"\n"
"   :arg axis: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ'], where a single axis is for a 2D matrix. Or a vector for an arbitrary axis\n"
"   :type axis: string or :class:`Vector`\n"
"   :arg size: The size of the projection matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :return: A new projection matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *C_Matrix_OrthoProjection(PyObject *cls, PyObject *args)
{
	PyObject *axis;

	int matSize, x;
	float norm = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "Oi:Matrix.OrthoProjection", &axis, &matSize)) {
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"mathutils.Matrix.OrthoProjection(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}

	if(PyUnicode_Check(axis)) {	//ortho projection onto cardinal plane
		Py_ssize_t plane_len;
		const char *plane= _PyUnicode_AsStringAndSize(axis, &plane_len);
		if(matSize == 2) {
			if(plane_len == 1 && plane[0]=='X') {
				mat[0]= 1.0f;
			}
			else if (plane_len == 1 && plane[0]=='Y') {
				mat[3]= 1.0f;
			}
			else {
				PyErr_Format(PyExc_ValueError, "mathutils.Matrix.OrthoProjection(): unknown plane, expected: X, Y, not '%.200s'", plane);
				return NULL;
			}
		}
		else {
			if(plane_len == 2 && plane[0]=='X' && plane[1]=='Y') {
				mat[0]= 1.0f;
				mat[4]= 1.0f;
			}
			else if (plane_len == 2 && plane[0]=='X' && plane[1]=='Z') {
				mat[0]= 1.0f;
				mat[8]= 1.0f;
			}
			else if (plane_len == 2 && plane[0]=='Y' && plane[1]=='Z') {
				mat[4]= 1.0f;
				mat[8]= 1.0f;
			}
			else {
				PyErr_Format(PyExc_ValueError, "mathutils.Matrix.OrthoProjection(): unknown plane, expected: XY, XZ, YZ, not '%.200s'", plane);
				return NULL;
			}
		}
	}
	else {
		//arbitrary plane

		int vec_size= (matSize == 2 ? 2 : 3);
		float tvec[4];

		if(mathutils_array_parse(tvec, vec_size, vec_size, axis, "Matrix.OrthoProjection(axis, size), invalid 'axis' arg") == -1) {
			return NULL;
		}

		//normalize arbitrary axis
		for(x = 0; x < vec_size; x++) {
			norm += tvec[x] * tvec[x];
		}
		norm = (float) sqrt(norm);
		for(x = 0; x < vec_size; x++) {
			tvec[x] /= norm;
		}
		if(matSize == 2) {
			mat[0] = 1 - (tvec[0] * tvec[0]);
			mat[1] = -(tvec[0] * tvec[1]);
			mat[2] = -(tvec[0] * tvec[1]);
			mat[3] = 1 - (tvec[1] * tvec[1]);
		}
		else if(matSize > 2) {
			mat[0] = 1 - (tvec[0] * tvec[0]);
			mat[1] = -(tvec[0] * tvec[1]);
			mat[2] = -(tvec[0] * tvec[2]);
			mat[3] = -(tvec[0] * tvec[1]);
			mat[4] = 1 - (tvec[1] * tvec[1]);
			mat[5] = -(tvec[1] * tvec[2]);
			mat[6] = -(tvec[0] * tvec[2]);
			mat[7] = -(tvec[1] * tvec[2]);
			mat[8] = 1 - (tvec[2] * tvec[2]);
		}
	}
	if(matSize == 4) {
		matrix_3x3_as_4x4(mat);
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}

static char C_Matrix_Shear_doc[] =
".. classmethod:: Shear(plane, size, factor)\n"
"\n"
"   Create a matrix to represent an shear transformation.\n"
"\n"
"   :arg plane: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ'], where a single axis is for a 2D matrix only.\n"
"   :type plane: string\n"
"   :arg size: The size of the shear matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :arg factor: The factor of shear to apply. For a 3 or 4 *size* matrix pass a pair of floats corrasponding with the *plane* axis.\n"
"   :type factor: float or float pair\n"
"   :return: A new shear matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *C_Matrix_Shear(PyObject *cls, PyObject *args)
{
	int matSize;
	const char *plane;
	PyObject *fac;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "siO:Matrix.Shear", &plane, &matSize, &fac)) {
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"mathutils.Matrix.Shear(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}

	if(matSize == 2) {
		float const factor= PyFloat_AsDouble(fac);

		if(factor==-1.0 && PyErr_Occurred()) {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.Shear(): the factor to be a float");
			return NULL;
		}

		/* unit */
		mat[0] = 1.0f;
		mat[3] = 1.0f;

		if(strcmp(plane, "X") == 0) {
			mat[2] = factor;
		}
		else if(strcmp(plane, "Y") == 0) {
			mat[1] = factor;
		}
		else {
			PyErr_SetString(PyExc_AttributeError, "Matrix.Shear(): expected: X, Y or wrong matrix size for shearing plane");
			return NULL;
		}
	}
	else {
		/* 3 or 4, apply as 3x3, resize later if needed */
		float factor[2];

		if(mathutils_array_parse(factor, 2, 2, fac, "Matrix.Shear()") < 0) {
			return NULL;
		}

		/* unit */
		mat[0] = 1.0f;
		mat[4] = 1.0f;
		mat[8] = 1.0f;

		if(strcmp(plane, "XY") == 0) {
			mat[6] = factor[0];
			mat[7] = factor[1];
		}
		else if(strcmp(plane, "XZ") == 0) {
			mat[3] = factor[0];
			mat[5] = factor[1];
		}
		else if(strcmp(plane, "YZ") == 0) {
			mat[1] = factor[0];
			mat[2] = factor[1];
		}
		else {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.Shear(): expected: X, Y, XY, XZ, YZ");
			return NULL;
		}
	}

	if(matSize == 4) {
		matrix_3x3_as_4x4(mat);
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}

void matrix_as_3x3(float mat[3][3], MatrixObject *self)
{
	copy_v3_v3(mat[0], self->matrix[0]);
	copy_v3_v3(mat[1], self->matrix[1]);
	copy_v3_v3(mat[2], self->matrix[2]);
}

/* assumes rowsize == colsize is checked and the read callback has run */
static float matrix_determinant_internal(MatrixObject *self)
{
	if(self->row_size == 2) {
		return determinant_m2(self->matrix[0][0], self->matrix[0][1],
					 self->matrix[1][0], self->matrix[1][1]);
	} else if(self->row_size == 3) {
		return determinant_m3(self->matrix[0][0], self->matrix[0][1],
					 self->matrix[0][2], self->matrix[1][0],
					 self->matrix[1][1], self->matrix[1][2],
					 self->matrix[2][0], self->matrix[2][1],
					 self->matrix[2][2]);
	} else {
		return determinant_m4((float (*)[4])self->contigPtr);
	}
}


/*-----------------------------METHODS----------------------------*/
static char Matrix_to_quaternion_doc[] =
".. method:: to_quaternion()\n"
"\n"
"   Return a quaternion representation of the rotation matrix.\n"
"\n"
"   :return: Quaternion representation of the rotation matrix.\n"
"   :rtype: :class:`Quaternion`\n"
;
static PyObject *Matrix_to_quaternion(MatrixObject *self)
{
	float quat[4];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if((self->col_size < 3) || (self->row_size < 3) || (self->col_size != self->row_size)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_quat(): inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	}
	if(self->col_size == 3){
		mat3_to_quat( quat,(float (*)[3])self->contigPtr);
	}else{
		mat4_to_quat( quat,(float (*)[4])self->contigPtr);
	}

	return newQuaternionObject(quat, Py_NEW, NULL);
}

/*---------------------------Matrix.toEuler() --------------------*/
static char Matrix_to_euler_doc[] =
".. method:: to_euler(order, euler_compat)\n"
"\n"
"   Return an Euler representation of the rotation matrix (3x3 or 4x4 matrix only).\n"
"\n"
"   :arg order: Optional rotation order argument in ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'].\n"
"   :type order: string\n"
"   :arg euler_compat: Optional euler argument the new euler will be made compatible with (no axis flipping between them). Useful for converting a series of matrices to animation curves.\n"
"   :type euler_compat: :class:`Euler`\n"
"   :return: Euler representation of the matrix.\n"
"   :rtype: :class:`Euler`\n"
;
static PyObject *Matrix_to_euler(MatrixObject *self, PyObject *args)
{
	const char *order_str= NULL;
	short order= EULER_ORDER_XYZ;
	float eul[3], eul_compatf[3];
	EulerObject *eul_compat = NULL;

	float tmat[3][3];
	float (*mat)[3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(!PyArg_ParseTuple(args, "|sO!:to_euler", &order_str, &euler_Type, &eul_compat))
		return NULL;

	if(eul_compat) {
		if(BaseMath_ReadCallback(eul_compat) == -1)
			return NULL;

		copy_v3_v3(eul_compatf, eul_compat->eul);
	}

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->col_size ==3 && self->row_size ==3) {
		mat= (float (*)[3])self->contigPtr;
	}else if (self->col_size ==4 && self->row_size ==4) {
		copy_m3_m4(tmat, (float (*)[4])self->contigPtr);
		mat= tmat;
	}else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_euler(): inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	}

	if(order_str) {
		order= euler_order_from_string(order_str, "Matrix.to_euler()");

		if(order == -1)
			return NULL;
	}

	if(eul_compat) {
		if(order == 1)	mat3_to_compatible_eul( eul, eul_compatf, mat);
		else			mat3_to_compatible_eulO(eul, eul_compatf, order, mat);
	}
	else {
		if(order == 1)	mat3_to_eul(eul, mat);
		else			mat3_to_eulO(eul, order, mat);
	}

	return newEulerObject(eul, order, Py_NEW, NULL);
}

static char Matrix_resize_4x4_doc[] =
".. method:: resize_4x4()\n"
"\n"
"   Resize the matrix to 4x4.\n"
;
static PyObject *Matrix_resize_4x4(MatrixObject *self)
{
	int x, first_row_elem, curr_pos, new_pos, blank_columns, blank_rows, index;

	if(self->wrapped==Py_WRAP){
		PyErr_SetString(PyExc_TypeError, "cannot resize wrapped data - make a copy and resize that");
		return NULL;
	}
	if(self->cb_user){
		PyErr_SetString(PyExc_TypeError, "cannot resize owned data - make a copy and resize that");
		return NULL;
	}

	self->contigPtr = PyMem_Realloc(self->contigPtr, (sizeof(float) * 16));
	if(self->contigPtr == NULL) {
		PyErr_SetString(PyExc_MemoryError, "matrix.resize_4x4(): problem allocating pointer space");
		return NULL;
	}
	/*set row pointers*/
	for(x = 0; x < 4; x++) {
		self->matrix[x] = self->contigPtr + (x * 4);
	}
	/*move data to new spot in array + clean*/
	for(blank_rows = (4 - self->row_size); blank_rows > 0; blank_rows--){
		for(x = 0; x < 4; x++){
			index = (4 * (self->row_size + (blank_rows - 1))) + x;
			if (index == 10 || index == 15){
				self->contigPtr[index] = 1.0f;
			}else{
				self->contigPtr[index] = 0.0f;
			}
		}
	}
	for(x = 1; x <= self->row_size; x++){
		first_row_elem = (self->col_size * (self->row_size - x));
		curr_pos = (first_row_elem + (self->col_size -1));
		new_pos = (4 * (self->row_size - x )) + (curr_pos - first_row_elem);
		for(blank_columns = (4 - self->col_size); blank_columns > 0; blank_columns--){
			self->contigPtr[new_pos + blank_columns] = 0.0f;
		}
		for(curr_pos = curr_pos; curr_pos >= first_row_elem; curr_pos--){
			self->contigPtr[new_pos] = self->contigPtr[curr_pos];
			new_pos--;
		}
	}
	self->row_size = 4;
	self->col_size = 4;

	Py_RETURN_NONE;
}

static char Matrix_to_4x4_doc[] =
".. method:: to_4x4()\n"
"\n"
"   Return a 4x4 copy of this matrix.\n"
"\n"
"   :return: a new matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_to_4x4(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(self->col_size==4 && self->row_size==4) {
		return (PyObject *)newMatrixObject(self->contigPtr, 4, 4, Py_NEW, Py_TYPE(self));
	}
	else if(self->col_size==3 && self->row_size==3) {
		float mat[4][4];
		copy_m4_m3(mat, (float (*)[3])self->contigPtr);
		return (PyObject *)newMatrixObject((float *)mat, 4, 4, Py_NEW, Py_TYPE(self));
	}
	/* TODO, 2x2 matrix */

	PyErr_SetString(PyExc_TypeError, "Matrix.to_4x4(): inappropriate matrix size");
	return NULL;
}

static char Matrix_to_3x3_doc[] =
".. method:: to_3x3()\n"
"\n"
"   Return a 3x3 copy of this matrix.\n"
"\n"
"   :return: a new matrix.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_to_3x3(MatrixObject *self)
{
	float mat[3][3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if((self->col_size < 3) || (self->row_size < 3)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_3x3(): inappropriate matrix size");
		return NULL;
	}

	matrix_as_3x3(mat, self);

	return newMatrixObject((float *)mat, 3, 3, Py_NEW, Py_TYPE(self));
}

static char Matrix_to_translation_doc[] =
".. method:: to_translation()\n"
"\n"
"   Return a the translation part of a 4 row matrix.\n"
"\n"
"   :return: Return a the translation of a matrix.\n"
"   :rtype: :class:`Vector`\n"
;
static PyObject *Matrix_to_translation(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if((self->col_size < 3) || self->row_size < 4){
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_translation(): inappropriate matrix size");
		return NULL;
	}

	return newVectorObject(self->matrix[3], 3, Py_NEW, NULL);
}

static char Matrix_to_scale_doc[] =
".. method:: to_scale()\n"
"\n"
"   Return a the scale part of a 3x3 or 4x4 matrix.\n"
"\n"
"   :return: Return a the scale of a matrix.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: This method does not return negative a scale on any axis because it is not possible to obtain this data from the matrix alone.\n"
;
static PyObject *Matrix_to_scale(MatrixObject *self)
{
	float rot[3][3];
	float mat[3][3];
	float size[3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if((self->col_size < 3) || (self->row_size < 3)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_scale(): inappropriate matrix size, 3x3 minimum size");
		return NULL;
	}

	matrix_as_3x3(mat, self);

	/* compatible mat4_to_loc_rot_size */
	mat3_to_rot_size(rot, size, mat);

	return newVectorObject(size, 3, Py_NEW, NULL);
}

/*---------------------------Matrix.invert() ---------------------*/
static char Matrix_invert_doc[] =
".. method:: invert()\n"
"\n"
"   Set the matrix to its inverse.\n"
"\n"
"   .. note:: :exc:`ValueError` exception is raised.\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Inverse_matrix>\n"
;
static PyObject *Matrix_invert(MatrixObject *self)
{

	int x, y, z = 0;
	float det = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(self->row_size != self->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix.invert(ed): only square matrices are supported");
		return NULL;
	}

	/*calculate the determinant*/
	det = matrix_determinant_internal(self);

	if(det != 0) {
		/*calculate the classical adjoint*/
		if(self->row_size == 2) {
			mat[0] = self->matrix[1][1];
			mat[1] = -self->matrix[0][1];
			mat[2] = -self->matrix[1][0];
			mat[3] = self->matrix[0][0];
		} else if(self->row_size == 3) {
			adjoint_m3_m3((float (*)[3]) mat,(float (*)[3])self->contigPtr);
		} else if(self->row_size == 4) {
			adjoint_m4_m4((float (*)[4]) mat, (float (*)[4])self->contigPtr);
		}
		/*divide by determinate*/
		for(x = 0; x < (self->row_size * self->col_size); x++) {
			mat[x] /= det;
		}
		/*set values*/
		for(x = 0; x < self->row_size; x++) {
			for(y = 0; y < self->col_size; y++) {
				self->matrix[x][y] = mat[z];
				z++;
			}
		}
		/*transpose
		Matrix_transpose(self);*/
	} else {
		PyErr_SetString(PyExc_ValueError, "matrix does not have an inverse");
		return NULL;
	}

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}

static char Matrix_inverted_doc[] =
".. method:: inverted()\n"
"\n"
"   Return an inverted copy of the matrix.\n"
"\n"
"   :return: the  inverted matrix.\n"
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. note:: :exc:`ValueError` exception is raised.\n"
;
static PyObject *Matrix_inverted(MatrixObject *self)
{
	return matrix__apply_to_copy((PyNoArgsFunction)Matrix_invert, self);
}

static char Matrix_rotate_doc[] =
".. method:: rotate(other)\n"
"\n"
"   Rotates the matrix a by another mathutils value.\n"
"\n"
"   :arg other: rotation component of mathutils value\n"
"   :type other: :class:`Euler`, :class:`Quaternion` or :class:`Matrix`\n"
"\n"
"   .. note:: If any of the columns are not unit length this may not have desired results.\n"
;
static PyObject *Matrix_rotate(MatrixObject *self, PyObject *value)
{
	float self_rmat[3][3], other_rmat[3][3], rmat[3][3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_any_to_rotmat(other_rmat, value, "matrix.rotate(value)") == -1)
		return NULL;

	if(self->col_size != 3 || self->row_size != 3) {
		PyErr_SetString(PyExc_ValueError, "Matrix must have 3x3 dimensions");
		return NULL;
	}

	matrix_as_3x3(self_rmat, self);
	mul_m3_m3m3(rmat, self_rmat, other_rmat);

	copy_m3_m3((float (*)[3])(self->contigPtr), rmat);

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}

/*---------------------------Matrix.decompose() ---------------------*/
static char Matrix_decompose_doc[] =
".. method:: decompose()\n"
"\n"
"   Return the location, rotaion and scale components of this matrix.\n"
"\n"
"   :return: loc, rot, scale triple.\n"
"   :rtype: (:class:`Vector`, :class:`Quaternion`, :class:`Vector`)"
;
static PyObject *Matrix_decompose(MatrixObject *self)
{
	PyObject *ret;
	float loc[3];
	float rot[3][3];
	float quat[4];
	float size[3];

	if(self->col_size != 4 || self->row_size != 4) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.decompose(): inappropriate matrix size - expects 4x4 matrix");
		return NULL;
	}

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	mat4_to_loc_rot_size(loc, rot, size, (float (*)[4])self->contigPtr);
	mat3_to_quat(quat, rot);

	ret= PyTuple_New(3);
	PyTuple_SET_ITEM(ret, 0, newVectorObject(loc, 3, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 1, newQuaternionObject(quat, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 2, newVectorObject(size, 3, Py_NEW, NULL));

	return ret;
}



static char Matrix_lerp_doc[] =
".. function:: lerp(other, factor)\n"
"\n"
"   Returns the interpolation of two matricies.\n"
"\n"
"   :arg other: value to interpolate with.\n"
"   :type other: :class:`Matrix`\n"
"   :arg factor: The interpolation value in [0.0, 1.0].\n"
"   :type factor: float\n"
"   :return: The interpolated rotation.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_lerp(MatrixObject *self, PyObject *args)
{
	MatrixObject *mat2 = NULL;
	float fac, mat[MATRIX_MAX_DIM*MATRIX_MAX_DIM];

	if(!PyArg_ParseTuple(args, "O!f:lerp", &matrix_Type, &mat2, &fac))
		return NULL;

	if(self->row_size != mat2->row_size || self->col_size != mat2->col_size) {
		PyErr_SetString(PyExc_AttributeError, "matrix.lerp(): expects both matrix objects of the same dimensions");
		return NULL;
	}

	if(BaseMath_ReadCallback(self) == -1 || BaseMath_ReadCallback(mat2) == -1)
		return NULL;

	/* TODO, different sized matrix */
	if(self->row_size==4 && self->col_size==4) {
		blend_m4_m4m4((float (*)[4])mat, (float (*)[4])self->contigPtr, (float (*)[4])mat2->contigPtr, fac);
	}
	else if (self->row_size==3 && self->col_size==3) {
		blend_m3_m3m3((float (*)[3])mat, (float (*)[3])self->contigPtr, (float (*)[3])mat2->contigPtr, fac);
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "matrix.lerp(): only 3x3 and 4x4 matrices supported");
		return NULL;
	}

	return (PyObject*)newMatrixObject(mat, self->row_size, self->col_size, Py_NEW, Py_TYPE(self));
}

/*---------------------------Matrix.determinant() ----------------*/
static char Matrix_determinant_doc[] =
".. method:: determinant()\n"
"\n"
"   Return the determinant of a matrix.\n"
"\n"
"   :return: Return a the determinant of a matrix.\n"
"   :rtype: float\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Determinant>\n"
;
static PyObject *Matrix_determinant(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(self->row_size != self->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix.determinant: only square matrices are supported");
		return NULL;
	}

	return PyFloat_FromDouble((double)matrix_determinant_internal(self));
}
/*---------------------------Matrix.transpose() ------------------*/
static char Matrix_transpose_doc[] =
".. method:: transpose()\n"
"\n"
"   Set the matrix to its transpose.\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Transpose>\n"
;
static PyObject *Matrix_transpose(MatrixObject *self)
{
	float t = 0.0f;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(self->row_size != self->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix.transpose(d): only square matrices are supported");
		return NULL;
	}

	if(self->row_size == 2) {
		t = self->matrix[1][0];
		self->matrix[1][0] = self->matrix[0][1];
		self->matrix[0][1] = t;
	} else if(self->row_size == 3) {
		transpose_m3((float (*)[3])self->contigPtr);
	} else {
		transpose_m4((float (*)[4])self->contigPtr);
	}

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}

static char Matrix_transposed_doc[] =
".. method:: transposed()\n"
"\n"
"   Return a new, transposed matrix.\n"
"\n"
"   :return: a transposed matrix\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_transposed(MatrixObject *self)
{
	return matrix__apply_to_copy((PyNoArgsFunction)Matrix_transpose, self);
}

/*---------------------------Matrix.zero() -----------------------*/
static char Matrix_zero_doc[] =
".. method:: zero()\n"
"\n"
"   Set all the matrix values to zero.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_zero(MatrixObject *self)
{
	fill_vn(self->contigPtr, self->row_size * self->col_size, 0.0f);

	if(BaseMath_WriteCallback(self) == -1)
		return NULL;

	Py_RETURN_NONE;
}
/*---------------------------Matrix.identity(() ------------------*/
static char Matrix_identity_doc[] =
".. method:: identity()\n"
"\n"
"   Set the matrix to the identity matrix.\n"
"\n"
"   .. note:: An object with zero location and rotation, a scale of one, will have an identity matrix.\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Identity_matrix>\n"
;
static PyObject *Matrix_identity(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(self->row_size != self->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix.identity: only square matrices are supported");
		return NULL;
	}

	if(self->row_size == 2) {
		self->matrix[0][0] = 1.0f;
		self->matrix[0][1] = 0.0f;
		self->matrix[1][0] = 0.0f;
		self->matrix[1][1] = 1.0f;
	} else if(self->row_size == 3) {
		unit_m3((float (*)[3])self->contigPtr);
	} else {
		unit_m4((float (*)[4])self->contigPtr);
	}

	if(BaseMath_WriteCallback(self) == -1)
		return NULL;

	Py_RETURN_NONE;
}

/*---------------------------Matrix.copy() ------------------*/
static char Matrix_copy_doc[] =
".. method:: copy()\n"
"\n"
"   Returns a copy of this matrix.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Matrix_copy(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return (PyObject*)newMatrixObject((float (*))self->contigPtr, self->row_size, self->col_size, Py_NEW, Py_TYPE(self));
}

/*----------------------------print object (internal)-------------*/
/*print the object to screen*/
static PyObject *Matrix_repr(MatrixObject *self)
{
	int x, y;
	PyObject *rows[MATRIX_MAX_DIM]= {NULL};

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	for(x = 0; x < self->row_size; x++){
		rows[x]= PyTuple_New(self->col_size);
		for(y = 0; y < self->col_size; y++) {
			PyTuple_SET_ITEM(rows[x], y, PyFloat_FromDouble(self->matrix[x][y]));
		}
	}
	switch(self->row_size) {
	case 2:	return PyUnicode_FromFormat("Matrix(%R,\n"
										"       %R)", rows[0], rows[1]);

	case 3:	return PyUnicode_FromFormat("Matrix(%R,\n"
										"       %R,\n"
										"       %R)", rows[0], rows[1], rows[2]);

	case 4:	return PyUnicode_FromFormat("Matrix(%R,\n"
										"       %R,\n"
										"       %R,\n"
										"       %R)", rows[0], rows[1], rows[2], rows[3]);
	}

	PyErr_SetString(PyExc_RuntimeError, "invalid matrix size");
	return NULL;
}

static PyObject* Matrix_richcmpr(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok= -1; /* zero is true */

	if (MatrixObject_Check(a) && MatrixObject_Check(b)) {
		MatrixObject *matA= (MatrixObject*)a;
		MatrixObject *matB= (MatrixObject*)b;

		if(BaseMath_ReadCallback(matA) == -1 || BaseMath_ReadCallback(matB) == -1)
			return NULL;

		ok=	(	(matA->col_size == matB->col_size) &&
				(matA->row_size == matB->row_size) &&
				EXPP_VectorsAreEqual(matA->contigPtr, matB->contigPtr, (matA->row_size * matA->col_size), 1)
			) ? 0 : -1;
	}

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	return Py_INCREF(res), res;
}

/*---------------------SEQUENCE PROTOCOLS------------------------
  ----------------------------len(object)------------------------
  sequence length*/
static int Matrix_len(MatrixObject *self)
{
	return (self->row_size);
}
/*----------------------------object[]---------------------------
  sequence accessor (get)
  the wrapped vector gives direct access to the matrix data*/
static PyObject *Matrix_item(MatrixObject *self, int i)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(i < 0 || i >= self->row_size) {
		PyErr_SetString(PyExc_IndexError, "matrix[attribute]: array index out of range");
		return NULL;
	}
	return newVectorObject_cb((PyObject *)self, self->col_size, mathutils_matrix_vector_cb_index, i);
}
/*----------------------------object[]-------------------------
  sequence accessor (set) */

static int Matrix_ass_item(MatrixObject *self, int i, PyObject *value)
{
	float vec[4];
	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	if(i >= self->row_size || i < 0){
		PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: bad column");
		return -1;
	}

	if(mathutils_array_parse(vec, self->col_size, self->col_size, value, "matrix[i] = value assignment") < 0) {
		return -1;
	}

	memcpy(self->matrix[i], vec, self->col_size *sizeof(float));

	(void)BaseMath_WriteCallback(self);
	return 0;
}

/*----------------------------object[z:y]------------------------
  sequence slice (get)*/
static PyObject *Matrix_slice(MatrixObject *self, int begin, int end)
{

	PyObject *tuple;
	int count;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	CLAMP(begin, 0, self->row_size);
	CLAMP(end, 0, self->row_size);
	begin= MIN2(begin,end);

	tuple= PyTuple_New(end - begin);
	for(count= begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin,
				newVectorObject_cb((PyObject *)self, self->col_size, mathutils_matrix_vector_cb_index, count));

	}

	return tuple;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set)*/
static int Matrix_ass_slice(MatrixObject *self, int begin, int end, PyObject *value)
{
	PyObject *value_fast= NULL;

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	CLAMP(begin, 0, self->row_size);
	CLAMP(end, 0, self->row_size);
	begin = MIN2(begin,end);

	/* non list/tuple cases */
	if(!(value_fast=PySequence_Fast(value, "matrix[begin:end] = value"))) {
		/* PySequence_Fast sets the error */
		return -1;
	}
	else {
		const int size= end - begin;
		int i;
		float mat[16];

		if(PySequence_Fast_GET_SIZE(value_fast) != size) {
			Py_DECREF(value_fast);
			PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: size mismatch in slice assignment");
			return -1;
		}

		/*parse sub items*/
		for (i = 0; i < size; i++) {
			/*parse each sub sequence*/
			PyObject *item= PySequence_Fast_GET_ITEM(value_fast, i);

			if(mathutils_array_parse(&mat[i * self->col_size], self->col_size, self->col_size, item, "matrix[begin:end] = value assignment") < 0) {
				return -1;
			}
		}

		Py_DECREF(value_fast);

		/*parsed well - now set in matrix*/
		memcpy(self->contigPtr + (begin * self->col_size), mat, sizeof(float) * (size * self->col_size));

		(void)BaseMath_WriteCallback(self);
		return 0;
	}
}
/*------------------------NUMERIC PROTOCOLS----------------------
  ------------------------obj + obj------------------------------*/
static PyObject *Matrix_add(PyObject *m1, PyObject *m2)
{
	float mat[16];
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation");
		return NULL;
	}

	if(BaseMath_ReadCallback(mat1) == -1 || BaseMath_ReadCallback(mat2) == -1)
		return NULL;

	if(mat1->row_size != mat2->row_size || mat1->col_size != mat2->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	add_vn_vnvn(mat, mat1->contigPtr, mat2->contigPtr, mat1->row_size * mat1->col_size);

	return newMatrixObject(mat, mat1->row_size, mat1->col_size, Py_NEW, Py_TYPE(mat1));
}
/*------------------------obj - obj------------------------------
  subtraction*/
static PyObject *Matrix_sub(PyObject *m1, PyObject *m2)
{
	float mat[16];
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation");
		return NULL;
	}

	if(BaseMath_ReadCallback(mat1) == -1 || BaseMath_ReadCallback(mat2) == -1)
		return NULL;

	if(mat1->row_size != mat2->row_size || mat1->col_size != mat2->col_size){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	sub_vn_vnvn(mat, mat1->contigPtr, mat2->contigPtr, mat1->row_size * mat1->col_size);

	return newMatrixObject(mat, mat1->row_size, mat1->col_size, Py_NEW, Py_TYPE(mat1));
}
/*------------------------obj * obj------------------------------
  mulplication*/
static PyObject *matrix_mul_float(MatrixObject *mat, const float scalar)
{
	float tmat[16];
	mul_vn_vn_fl(tmat, mat->contigPtr, mat->row_size * mat->col_size, scalar);
	return newMatrixObject(tmat, mat->row_size, mat->col_size, Py_NEW, Py_TYPE(mat));
}

static PyObject *Matrix_mul(PyObject * m1, PyObject * m2)
{
	float scalar;

	MatrixObject *mat1 = NULL, *mat2 = NULL;

	if(MatrixObject_Check(m1)) {
		mat1 = (MatrixObject*)m1;
		if(BaseMath_ReadCallback(mat1) == -1)
			return NULL;
	}
	if(MatrixObject_Check(m2)) {
		mat2 = (MatrixObject*)m2;
		if(BaseMath_ReadCallback(mat2) == -1)
			return NULL;
	}

	if(mat1 && mat2) { /*MATRIX * MATRIX*/
		if(mat1->row_size != mat2->col_size){
			PyErr_SetString(PyExc_AttributeError,"Matrix multiplication: matrix A rowsize must equal matrix B colsize");
			return NULL;
		}
		else {
			float mat[16]= {0.0f, 0.0f, 0.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 1.0f};
			double dot = 0.0f;
			int x, y, z;

			for(x = 0; x < mat2->row_size; x++) {
				for(y = 0; y < mat1->col_size; y++) {
					for(z = 0; z < mat1->row_size; z++) {
						dot += (mat1->matrix[z][y] * mat2->matrix[x][z]);
					}
					mat[((x * mat1->col_size) + y)] = (float)dot;
					dot = 0.0f;
				}
			}

			return newMatrixObject(mat, mat2->row_size, mat1->col_size, Py_NEW, Py_TYPE(mat1));
		}
	}
	else if(mat2) {
		if (((scalar= PyFloat_AsDouble(m1)) == -1.0 && PyErr_Occurred())==0) { /*FLOAT/INT * MATRIX */
			return matrix_mul_float(mat2, scalar);
		}
	}
	else if(mat1) {
		if (((scalar= PyFloat_AsDouble(m2)) == -1.0 && PyErr_Occurred())==0) { /*FLOAT/INT * MATRIX */
			return matrix_mul_float(mat1, scalar);
		}
	}
	else {
		BLI_assert(!"internal error");
	}

	PyErr_Format(PyExc_TypeError, "Matrix multiplication: not supported between '%.200s' and '%.200s' types", Py_TYPE(m1)->tp_name, Py_TYPE(m2)->tp_name);
	return NULL;
}
static PyObject* Matrix_inv(MatrixObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return Matrix_invert(self);
}

/*-----------------PROTOCOL DECLARATIONS--------------------------*/
static PySequenceMethods Matrix_SeqMethods = {
	(lenfunc) Matrix_len,						/* sq_length */
	(binaryfunc) NULL,							/* sq_concat */
	(ssizeargfunc) NULL,						/* sq_repeat */
	(ssizeargfunc) Matrix_item,					/* sq_item */
	(ssizessizeargfunc) NULL,					/* sq_slice, deprecated */
	(ssizeobjargproc) Matrix_ass_item,			/* sq_ass_item */
	(ssizessizeobjargproc) NULL,				/* sq_ass_slice, deprecated */
	(objobjproc) NULL,							/* sq_contains */
	(binaryfunc) NULL,							/* sq_inplace_concat */
	(ssizeargfunc) NULL,						/* sq_inplace_repeat */
};


static PyObject *Matrix_subscript(MatrixObject* self, PyObject* item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += self->row_size;
		return Matrix_item(self, i);
	} else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->row_size, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyTuple_New(0);
		}
		else if (step == 1) {
			return Matrix_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with matricies");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "vector indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return NULL;
	}
}

static int Matrix_ass_subscript(MatrixObject* self, PyObject* item, PyObject* value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += self->row_size;
		return Matrix_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->row_size, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Matrix_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with matricies");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "matrix indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return -1;
	}
}

static PyMappingMethods Matrix_AsMapping = {
	(lenfunc)Matrix_len,
	(binaryfunc)Matrix_subscript,
	(objobjargproc)Matrix_ass_subscript
};


static PyNumberMethods Matrix_NumMethods = {
		(binaryfunc)	Matrix_add,	/*nb_add*/
		(binaryfunc)	Matrix_sub,	/*nb_subtract*/
		(binaryfunc)	Matrix_mul,	/*nb_multiply*/
		NULL,							/*nb_remainder*/
		NULL,							/*nb_divmod*/
		NULL,							/*nb_power*/
		(unaryfunc) 	0,	/*nb_negative*/
		(unaryfunc) 	0,	/*tp_positive*/
		(unaryfunc) 	0,	/*tp_absolute*/
		(inquiry)	0,	/*tp_bool*/
		(unaryfunc)	Matrix_inv,	/*nb_invert*/
		NULL,				/*nb_lshift*/
		(binaryfunc)0,	/*nb_rshift*/
		NULL,				/*nb_and*/
		NULL,				/*nb_xor*/
		NULL,				/*nb_or*/
		NULL,				/*nb_int*/
		NULL,				/*nb_reserved*/
		NULL,				/*nb_float*/
		NULL,				/* nb_inplace_add */
		NULL,				/* nb_inplace_subtract */
		NULL,				/* nb_inplace_multiply */
		NULL,				/* nb_inplace_remainder */
		NULL,				/* nb_inplace_power */
		NULL,				/* nb_inplace_lshift */
		NULL,				/* nb_inplace_rshift */
		NULL,				/* nb_inplace_and */
		NULL,				/* nb_inplace_xor */
		NULL,				/* nb_inplace_or */
		NULL,				/* nb_floor_divide */
		NULL,				/* nb_true_divide */
		NULL,				/* nb_inplace_floor_divide */
		NULL,				/* nb_inplace_true_divide */
		NULL,				/* nb_index */
};

static PyObject *Matrix_getRowSize(MatrixObject *self, void *UNUSED(closure))
{
	return PyLong_FromLong((long) self->row_size);
}

static PyObject *Matrix_getColSize(MatrixObject *self, void *UNUSED(closure))
{
	return PyLong_FromLong((long) self->col_size);
}

static PyObject *Matrix_getMedianScale(MatrixObject *self, void *UNUSED(closure))
{
	float mat[3][3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if((self->col_size < 3) || (self->row_size < 3)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.median_scale: inappropriate matrix size, 3x3 minimum");
		return NULL;
	}

	matrix_as_3x3(mat, self);

	return PyFloat_FromDouble(mat3_to_scale(mat));
}

static PyObject *Matrix_getIsNegative(MatrixObject *self, void *UNUSED(closure))
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->col_size == 4 && self->row_size == 4)
		return PyBool_FromLong(is_negative_m4((float (*)[4])self->contigPtr));
	else if(self->col_size == 3 && self->row_size == 3)
		return PyBool_FromLong(is_negative_m3((float (*)[3])self->contigPtr));
	else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.is_negative: inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	}
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Matrix_getseters[] = {
	{(char *)"row_size", (getter)Matrix_getRowSize, (setter)NULL, (char *)"The row size of the matrix (readonly).\n\n:type: int", NULL},
	{(char *)"col_size", (getter)Matrix_getColSize, (setter)NULL, (char *)"The column size of the matrix (readonly).\n\n:type: int", NULL},
	{(char *)"median_scale", (getter)Matrix_getMedianScale, (setter)NULL, (char *)"The average scale applied to each axis (readonly).\n\n:type: float", NULL},
	{(char *)"is_negative", (getter)Matrix_getIsNegative, (setter)NULL, (char *)"True if this matrix results in a negative scale, 3x3 and 4x4 only, (readonly).\n\n:type: bool", NULL},
	{(char *)"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, (char *)BaseMathObject_Wrapped_doc, NULL},
	{(char *)"owner",(getter)BaseMathObject_getOwner, (setter)NULL, (char *)BaseMathObject_Owner_doc, NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*-----------------------METHOD DEFINITIONS ----------------------*/
static struct PyMethodDef Matrix_methods[] = {
	/* derived values */
	{"determinant", (PyCFunction) Matrix_determinant, METH_NOARGS, Matrix_determinant_doc},
	{"decompose", (PyCFunction) Matrix_decompose, METH_NOARGS, Matrix_decompose_doc},

	/* in place only */
	{"zero", (PyCFunction) Matrix_zero, METH_NOARGS, Matrix_zero_doc},
	{"identity", (PyCFunction) Matrix_identity, METH_NOARGS, Matrix_identity_doc},

	/* operate on original or copy */
	{"transpose", (PyCFunction) Matrix_transpose, METH_NOARGS, Matrix_transpose_doc},
	{"transposed", (PyCFunction) Matrix_transposed, METH_NOARGS, Matrix_transposed_doc},
	{"invert", (PyCFunction) Matrix_invert, METH_NOARGS, Matrix_invert_doc},
	{"inverted", (PyCFunction) Matrix_inverted, METH_NOARGS, Matrix_inverted_doc},
	{"to_3x3", (PyCFunction) Matrix_to_3x3, METH_NOARGS, Matrix_to_3x3_doc},
	// TODO. {"resize_3x3", (PyCFunction) Matrix_resize3x3, METH_NOARGS, Matrix_resize3x3_doc},
	{"to_4x4", (PyCFunction) Matrix_to_4x4, METH_NOARGS, Matrix_to_4x4_doc},
	{"resize_4x4", (PyCFunction) Matrix_resize_4x4, METH_NOARGS, Matrix_resize_4x4_doc},
	{"rotate", (PyCFunction) Matrix_rotate, METH_O, Matrix_rotate_doc},

	/* return converted representation */
	{"to_euler", (PyCFunction) Matrix_to_euler, METH_VARARGS, Matrix_to_euler_doc},
	{"to_quaternion", (PyCFunction) Matrix_to_quaternion, METH_NOARGS, Matrix_to_quaternion_doc},
	{"to_scale", (PyCFunction) Matrix_to_scale, METH_NOARGS, Matrix_to_scale_doc},
	{"to_translation", (PyCFunction) Matrix_to_translation, METH_NOARGS, Matrix_to_translation_doc},

	/* operation between 2 or more types  */
	{"lerp", (PyCFunction) Matrix_lerp, METH_VARARGS, Matrix_lerp_doc},
	{"copy", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},
	{"__copy__", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},

	/* class methods */
	{"Rotation", (PyCFunction) C_Matrix_Rotation, METH_VARARGS | METH_CLASS, C_Matrix_Rotation_doc},
	{"Scale", (PyCFunction) C_Matrix_Scale, METH_VARARGS | METH_CLASS, C_Matrix_Scale_doc},
	{"Shear", (PyCFunction) C_Matrix_Shear, METH_VARARGS | METH_CLASS, C_Matrix_Shear_doc},
	{"Translation", (PyCFunction) C_Matrix_Translation, METH_O | METH_CLASS, C_Matrix_Translation_doc},
	{"OrthoProjection", (PyCFunction) C_Matrix_OrthoProjection,  METH_VARARGS | METH_CLASS, C_Matrix_OrthoProjection_doc},
	{NULL, NULL, 0, NULL}
};

/*------------------PY_OBECT DEFINITION--------------------------*/
static char matrix_doc[] =
"This object gives access to Matrices in Blender."
;
PyTypeObject matrix_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"mathutils.Matrix",					/*tp_name*/
	sizeof(MatrixObject),				/*tp_basicsize*/
	0,									/*tp_itemsize*/
	(destructor)BaseMathObject_dealloc,	/*tp_dealloc*/
	NULL,								/*tp_print*/
	NULL,								/*tp_getattr*/
	NULL,								/*tp_setattr*/
	NULL,								/*tp_compare*/
	(reprfunc) Matrix_repr,				/*tp_repr*/
	&Matrix_NumMethods,					/*tp_as_number*/
	&Matrix_SeqMethods,					/*tp_as_sequence*/
	&Matrix_AsMapping,					/*tp_as_mapping*/
	NULL,								/*tp_hash*/
	NULL,								/*tp_call*/
	NULL,								/*tp_str*/
	NULL,								/*tp_getattro*/
	NULL,								/*tp_setattro*/
	NULL,								/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /*tp_flags*/
	matrix_doc,							/*tp_doc*/
	(traverseproc)BaseMathObject_traverse,	//tp_traverse
	(inquiry)BaseMathObject_clear,	//tp_clear
	(richcmpfunc)Matrix_richcmpr,		/*tp_richcompare*/
	0,									/*tp_weaklistoffset*/
	NULL,								/*tp_iter*/
	NULL,								/*tp_iternext*/
	Matrix_methods,						/*tp_methods*/
	NULL,								/*tp_members*/
	Matrix_getseters,					/*tp_getset*/
	NULL,								/*tp_base*/
	NULL,								/*tp_dict*/
	NULL,								/*tp_descr_get*/
	NULL,								/*tp_descr_set*/
	0,									/*tp_dictoffset*/
	NULL,								/*tp_init*/
	NULL,								/*tp_alloc*/
	Matrix_new,							/*tp_new*/
	NULL,								/*tp_free*/
	NULL,								/*tp_is_gc*/
	NULL,								/*tp_bases*/
	NULL,								/*tp_mro*/
	NULL,								/*tp_cache*/
	NULL,								/*tp_subclasses*/
	NULL,								/*tp_weaklist*/
	NULL								/*tp_del*/
};

/*------------------------newMatrixObject (internal)-------------
creates a new matrix object
self->matrix     self->contiguous_ptr (reference to data.xxx)
	   [0]------------->[0]
						[1]
						[2]
	   [1]------------->[3]
						[4]
						[5]

self->matrix[1][1] = self->contigPtr[4] */

/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newMatrixObject(float *mat, const unsigned short rowSize, const unsigned short colSize, int type, PyTypeObject *base_type)
{
	MatrixObject *self;
	int x, row, col;

	/*matrix objects can be any 2-4row x 2-4col matrix*/
	if(rowSize < 2 || rowSize > 4 || colSize < 2 || colSize > 4) {
		PyErr_SetString(PyExc_RuntimeError, "matrix(): row and column sizes must be between 2 and 4");
		return NULL;
	}

	self= base_type ?	(MatrixObject *)base_type->tp_alloc(base_type, 0) :
						(MatrixObject *)PyObject_GC_New(MatrixObject, &matrix_Type);

	if(self) {
		self->row_size = rowSize;
		self->col_size = colSize;

		/* init callbacks as NULL */
		self->cb_user= NULL;
		self->cb_type= self->cb_subtype= 0;

		if(type == Py_WRAP){
			self->contigPtr = mat;
			/*pointer array points to contigous memory*/
			for(x = 0; x < rowSize; x++) {
				self->matrix[x] = self->contigPtr + (x * colSize);
			}
			self->wrapped = Py_WRAP;
		}
		else if (type == Py_NEW){
			self->contigPtr = PyMem_Malloc(rowSize * colSize * sizeof(float));
			if(self->contigPtr == NULL) { /*allocation failure*/
				PyErr_SetString(PyExc_MemoryError, "matrix(): problem allocating pointer space");
				return NULL;
			}
			/*pointer array points to contigous memory*/
			for(x = 0; x < rowSize; x++) {
				self->matrix[x] = self->contigPtr + (x * colSize);
			}
			/*parse*/
			if(mat) {	/*if a float array passed*/
				for(row = 0; row < rowSize; row++) {
					for(col = 0; col < colSize; col++) {
						self->matrix[row][col] = mat[(row * colSize) + col];
					}
				}
			}
			else if (rowSize == colSize ) { /*or if no arguments are passed return identity matrix for square matrices */
				PyObject *ret_dummy= Matrix_identity(self);
				Py_DECREF(ret_dummy);
			}
			self->wrapped = Py_NEW;
		}
		else {
			PyErr_SetString(PyExc_RuntimeError, "Matrix(): invalid type");
			return NULL;
		}
	}
	return (PyObject *) self;
}

PyObject *newMatrixObject_cb(PyObject *cb_user, int rowSize, int colSize, int cb_type, int cb_subtype)
{
	MatrixObject *self= (MatrixObject *)newMatrixObject(NULL, rowSize, colSize, Py_NEW, NULL);
	if(self) {
		Py_INCREF(cb_user);
		self->cb_user=			cb_user;
		self->cb_type=			(unsigned char)cb_type;
		self->cb_subtype=		(unsigned char)cb_subtype;
		PyObject_GC_Track(self);
	}
	return (PyObject *) self;
}
