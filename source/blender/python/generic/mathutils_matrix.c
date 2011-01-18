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

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

static int Matrix_ass_slice(MatrixObject * self, int begin, int end, PyObject *value);

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

	if(!BaseMath_ReadCallback(self))
		return 0;

	for(i=0; i < self->colSize; i++)
		bmo->data[i]= self->matrix[subtype][i];

	return 1;
}

static int mathutils_matrix_vector_set(BaseMathObject *bmo, int subtype)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;
	int i;

	if(!BaseMath_ReadCallback(self))
		return 0;

	for(i=0; i < self->colSize; i++)
		self->matrix[subtype][i]= bmo->data[i];

	(void)BaseMath_WriteCallback(self);
	return 1;
}

static int mathutils_matrix_vector_get_index(BaseMathObject *bmo, int subtype, int index)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;

	if(!BaseMath_ReadCallback(self))
		return 0;

	bmo->data[index]= self->matrix[subtype][index];
	return 1;
}

static int mathutils_matrix_vector_set_index(BaseMathObject *bmo, int subtype, int index)
{
	MatrixObject *self= (MatrixObject *)bmo->cb_user;

	if(!BaseMath_ReadCallback(self))
		return 0;

	self->matrix[subtype][index]= bmo->data[index];

	(void)BaseMath_WriteCallback(self);
	return 1;
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

/*-----------------------CLASS-METHODS----------------------------*/

//----------------------------------mathutils.RotationMatrix() ----------
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
"   :rtype: :class:`Matrix`\n";

static PyObject *C_Matrix_Rotation(PyObject *cls, PyObject *args)
{
	VectorObject *vec= NULL;
	char *axis= NULL;
	int matSize;
	double angle; /* use double because of precission problems at high values */
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "di|O", &angle, &matSize, &vec)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.RotationMatrix(angle, size, axis): expected float int and a string or vector");
		return NULL;
	}

	if(vec && !VectorObject_Check(vec)) {
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
		PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): please choose an axis of rotation for 3d and 4d matrices");
		return NULL;
	}
	if(vec) {
		if(vec->size != 3) {
			PyErr_SetString(PyExc_AttributeError, "mathutils.RotationMatrix(): the vector axis must be a 3D vector");
			return NULL;
		}
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
	}

	/* check for valid vector/axis above */
	if(vec) {
		axis_angle_to_mat3( (float (*)[3])mat,vec->vec, angle);
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
		//resize matrix
		mat[10] = mat[8];
		mat[9] = mat[7];
		mat[8] = mat[6];
		mat[7] = 0.0f;
		mat[6] = mat[5];
		mat[5] = mat[4];
		mat[4] = mat[3];
		mat[3] = 0.0f;
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
"   :rtype: :class:`Matrix`\n";

static PyObject *C_Matrix_Translation(PyObject *cls, VectorObject * vec)
{
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!VectorObject_Check(vec)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Matrix.Translation(): expected vector");
		return NULL;
	}
	if(vec->size != 3 && vec->size != 4) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Matrix.Translation(): vector must be 3D or 4D");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec))
		return NULL;
	
	//create a identity matrix and add translation
	unit_m4((float(*)[4]) mat);
	mat[12] = vec->vec[0];
	mat[13] = vec->vec[1];
	mat[14] = vec->vec[2];

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
"   :rtype: :class:`Matrix`\n";

static PyObject *C_Matrix_Scale(PyObject *cls, PyObject *args)
{
	VectorObject *vec = NULL;
	float norm = 0.0f, factor;
	int matSize, x;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "fi|O!", &factor, &matSize, &vector_Type, &vec)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Matrix.Scale(): expected float int and optional vector");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.Scale(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}
	if(vec) {
		if(vec->size > 2 && matSize == 2) {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.Scale(): please use 2D vectors when scaling in 2D");
			return NULL;
		}
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
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
	} else { //scaling in arbitrary direction
		//normalize arbitrary axis
		for(x = 0; x < vec->size; x++) {
			norm += vec->vec[x] * vec->vec[x];
		}
		norm = (float) sqrt(norm);
		for(x = 0; x < vec->size; x++) {
			vec->vec[x] /= norm;
		}
		if(matSize == 2) {
			mat[0] = 1 +((factor - 1) *(vec->vec[0] * vec->vec[0]));
			mat[1] =((factor - 1) *(vec->vec[0] * vec->vec[1]));
			mat[2] =((factor - 1) *(vec->vec[0] * vec->vec[1]));
			mat[3] = 1 + ((factor - 1) *(vec->vec[1] * vec->vec[1]));
		} else {
			mat[0] = 1 + ((factor - 1) *(vec->vec[0] * vec->vec[0]));
			mat[1] =((factor - 1) *(vec->vec[0] * vec->vec[1]));
			mat[2] =((factor - 1) *(vec->vec[0] * vec->vec[2]));
			mat[3] =((factor - 1) *(vec->vec[0] * vec->vec[1]));
			mat[4] = 1 + ((factor - 1) *(vec->vec[1] * vec->vec[1]));
			mat[5] =((factor - 1) *(vec->vec[1] * vec->vec[2]));
			mat[6] =((factor - 1) *(vec->vec[0] * vec->vec[2]));
			mat[7] =((factor - 1) *(vec->vec[1] * vec->vec[2]));
			mat[8] = 1 + ((factor - 1) *(vec->vec[2] * vec->vec[2]));
		}
	}
	if(matSize == 4) {
		//resize matrix
		mat[10] = mat[8];
		mat[9] = mat[7];
		mat[8] = mat[6];
		mat[7] = 0.0f;
		mat[6] = mat[5];
		mat[5] = mat[4];
		mat[4] = mat[3];
		mat[3] = 0.0f;
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}
//----------------------------------mathutils.Matrix.OrthoProjection() ---
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static char C_Matrix_OrthoProjection_doc[] =
".. classmethod:: OrthoProjection(plane, size, axis)\n"
"\n"
"   Create a matrix to represent an orthographic projection.\n"
"\n"
"   :arg plane: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ', 'R'], where a single axis is for a 2D matrix and 'R' requires axis is given.\n"
"   :type plane: string\n"
"   :arg size: The size of the projection matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :arg axis: Arbitrary perpendicular plane vector (optional).\n"
"   :type axis: :class:`Vector`\n"
"   :return: A new projection matrix.\n"
"   :rtype: :class:`Matrix`\n";
static PyObject *C_Matrix_OrthoProjection(PyObject *cls, PyObject *args)
{
	VectorObject *vec = NULL;
	char *plane;
	int matSize, x;
	float norm = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!PyArg_ParseTuple(args, "si|O!", &plane, &matSize, &vector_Type, &vec)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Matrix.OrthoProjection(): expected string and int and optional vector");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"mathutils.Matrix.OrthoProjection(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}
	if(vec) {
		if(vec->size > 2 && matSize == 2) {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.OrthoProjection(): please use 2D vectors when scaling in 2D");
			return NULL;
		}
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
	}
	if(vec == NULL) {	//ortho projection onto cardinal plane
		if((strcmp(plane, "X") == 0) && matSize == 2) {
			mat[0] = 1.0f;
		} else if((strcmp(plane, "Y") == 0) && matSize == 2) {
			mat[3] = 1.0f;
		} else if((strcmp(plane, "XY") == 0) && matSize > 2) {
			mat[0] = 1.0f;
			mat[4] = 1.0f;
		} else if((strcmp(plane, "XZ") == 0) && matSize > 2) {
			mat[0] = 1.0f;
			mat[8] = 1.0f;
		} else if((strcmp(plane, "YZ") == 0) && matSize > 2) {
			mat[4] = 1.0f;
			mat[8] = 1.0f;
		} else {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.OrthoProjection(): unknown plane - expected: X, Y, XY, XZ, YZ");
			return NULL;
		}
	} else { //arbitrary plane
		//normalize arbitrary axis
		for(x = 0; x < vec->size; x++) {
			norm += vec->vec[x] * vec->vec[x];
		}
		norm = (float) sqrt(norm);
		for(x = 0; x < vec->size; x++) {
			vec->vec[x] /= norm;
		}
		if((strcmp(plane, "R") == 0) && matSize == 2) {
			mat[0] = 1 - (vec->vec[0] * vec->vec[0]);
			mat[1] = -(vec->vec[0] * vec->vec[1]);
			mat[2] = -(vec->vec[0] * vec->vec[1]);
			mat[3] = 1 - (vec->vec[1] * vec->vec[1]);
		} else if((strcmp(plane, "R") == 0) && matSize > 2) {
			mat[0] = 1 - (vec->vec[0] * vec->vec[0]);
			mat[1] = -(vec->vec[0] * vec->vec[1]);
			mat[2] = -(vec->vec[0] * vec->vec[2]);
			mat[3] = -(vec->vec[0] * vec->vec[1]);
			mat[4] = 1 - (vec->vec[1] * vec->vec[1]);
			mat[5] = -(vec->vec[1] * vec->vec[2]);
			mat[6] = -(vec->vec[0] * vec->vec[2]);
			mat[7] = -(vec->vec[1] * vec->vec[2]);
			mat[8] = 1 - (vec->vec[2] * vec->vec[2]);
		} else {
			PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.OrthoProjection(): unknown plane - expected: 'r' expected for axis designation");
			return NULL;
		}
	}
	if(matSize == 4) {
		//resize matrix
		mat[10] = mat[8];
		mat[9] = mat[7];
		mat[8] = mat[6];
		mat[7] = 0.0f;
		mat[6] = mat[5];
		mat[5] = mat[4];
		mat[4] = mat[3];
		mat[3] = 0.0f;
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}

static char C_Matrix_Shear_doc[] =
".. classmethod:: Shear(plane, factor, size)\n"
"\n"
"   Create a matrix to represent an shear transformation.\n"
"\n"
"   :arg plane: Can be any of the following: ['X', 'Y', 'XY', 'XZ', 'YZ'], where a single axis is for a 2D matrix.\n"
"   :type plane: string\n"
"   :arg factor: The factor of shear to apply.\n"
"   :type factor: float\n"
"   :arg size: The size of the shear matrix to construct [2, 4].\n"
"   :type size: int\n"
"   :return: A new shear matrix.\n"
"   :rtype: :class:`Matrix`\n";

static PyObject *C_Matrix_Shear(PyObject *cls, PyObject *args)
{
	int matSize;
	char *plane;
	float factor;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "sfi", &plane, &factor, &matSize)) {
		PyErr_SetString(PyExc_TypeError,"mathutils.Matrix.Shear(): expected string float and int");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"mathutils.Matrix.Shear(): can only return a 2x2 3x3 or 4x4 matrix");
		return NULL;
	}

	if((strcmp(plane, "X") == 0)
		&& matSize == 2) {
		mat[0] = 1.0f;
		mat[2] = factor;
		mat[3] = 1.0f;
	} else if((strcmp(plane, "Y") == 0) && matSize == 2) {
		mat[0] = 1.0f;
		mat[1] = factor;
		mat[3] = 1.0f;
	} else if((strcmp(plane, "XY") == 0) && matSize > 2) {
		mat[0] = 1.0f;
		mat[4] = 1.0f;
		mat[6] = factor;
		mat[7] = factor;
	} else if((strcmp(plane, "XZ") == 0) && matSize > 2) {
		mat[0] = 1.0f;
		mat[3] = factor;
		mat[4] = 1.0f;
		mat[5] = factor;
		mat[8] = 1.0f;
	} else if((strcmp(plane, "YZ") == 0) && matSize > 2) {
		mat[0] = 1.0f;
		mat[1] = factor;
		mat[2] = factor;
		mat[4] = 1.0f;
		mat[8] = 1.0f;
	} else {
		PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix.Shear(): expected: x, y, xy, xz, yz or wrong matrix size for shearing plane");
		return NULL;
	}
	if(matSize == 4) {
		//resize matrix
		mat[10] = mat[8];
		mat[9] = mat[7];
		mat[8] = mat[6];
		mat[7] = 0.0f;
		mat[6] = mat[5];
		mat[5] = mat[4];
		mat[4] = mat[3];
		mat[3] = 0.0f;
	}
	//pass to matrix creation
	return newMatrixObject(mat, matSize, matSize, Py_NEW, (PyTypeObject *)cls);
}

/* assumes rowsize == colsize is checked and the read callback has run */
static float matrix_determinant(MatrixObject * self)
{
	if(self->rowSize == 2) {
		return determinant_m2(self->matrix[0][0], self->matrix[0][1],
					 self->matrix[1][0], self->matrix[1][1]);
	} else if(self->rowSize == 3) {
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
static char Matrix_toQuat_doc[] =
".. method:: to_quat()\n"
"\n"
"   Return a quaternion representation of the rotation matrix.\n"
"\n"
"   :return: Quaternion representation of the rotation matrix.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Matrix_toQuat(MatrixObject * self)
{
	float quat[4];

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize < 3 || self->rowSize < 3 || (self->colSize != self->rowSize)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_quat(): inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	} 
	if(self->colSize == 3){
		mat3_to_quat( quat,(float (*)[3])self->contigPtr);
	}else{
		mat4_to_quat( quat,(float (*)[4])self->contigPtr);
	}
	
	return newQuaternionObject(quat, Py_NEW, NULL);
}

/*---------------------------Matrix.toEuler() --------------------*/
static char Matrix_toEuler_doc[] =
".. method:: to_euler(order, euler_compat)\n"
"\n"
"   Return an Euler representation of the rotation matrix (3x3 or 4x4 matrix only).\n"
"\n"
"   :arg order: Optional rotation order argument in ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'].\n"
"   :type order: string\n"
"   :arg euler_compat: Optional euler argument the new euler will be made compatible with (no axis flipping between them). Useful for converting a series of matrices to animation curves.\n"
"   :type euler_compat: :class:`Euler`\n"
"   :return: Euler representation of the matrix.\n"
"   :rtype: :class:`Euler`\n";

PyObject *Matrix_toEuler(MatrixObject * self, PyObject *args)
{
	char *order_str= NULL;
	short order= EULER_ORDER_XYZ;
	float eul[3], eul_compatf[3];
	EulerObject *eul_compat = NULL;

	float tmat[3][3];
	float (*mat)[3];
	
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(!PyArg_ParseTuple(args, "|sO!:to_euler", &order_str, &euler_Type, &eul_compat))
		return NULL;
	
	if(eul_compat) {
		if(!BaseMath_ReadCallback(eul_compat))
			return NULL;

		copy_v3_v3(eul_compatf, eul_compat->eul);
	}
	
	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize ==3 && self->rowSize ==3) {
		mat= (float (*)[3])self->contigPtr;
	}else if (self->colSize ==4 && self->rowSize ==4) {
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
/*---------------------------Matrix.resize4x4() ------------------*/
static char Matrix_Resize4x4_doc[] =
".. method:: resize4x4()\n"
"\n"
"   Resize the matrix to 4x4.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Matrix`\n";

PyObject *Matrix_Resize4x4(MatrixObject * self)
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
		PyErr_SetString(PyExc_MemoryError, "matrix.resize4x4(): problem allocating pointer space");
		return NULL;
	}
	/*set row pointers*/
	for(x = 0; x < 4; x++) {
		self->matrix[x] = self->contigPtr + (x * 4);
	}
	/*move data to new spot in array + clean*/
	for(blank_rows = (4 - self->rowSize); blank_rows > 0; blank_rows--){
		for(x = 0; x < 4; x++){
			index = (4 * (self->rowSize + (blank_rows - 1))) + x;
			if (index == 10 || index == 15){
				self->contigPtr[index] = 1.0f;
			}else{
				self->contigPtr[index] = 0.0f;
			}
		}
	}
	for(x = 1; x <= self->rowSize; x++){
		first_row_elem = (self->colSize * (self->rowSize - x));
		curr_pos = (first_row_elem + (self->colSize -1));
		new_pos = (4 * (self->rowSize - x )) + (curr_pos - first_row_elem);
		for(blank_columns = (4 - self->colSize); blank_columns > 0; blank_columns--){
			self->contigPtr[new_pos + blank_columns] = 0.0f;
		}
		for(curr_pos = curr_pos; curr_pos >= first_row_elem; curr_pos--){
			self->contigPtr[new_pos] = self->contigPtr[curr_pos];
			new_pos--;
		}
	}
	self->rowSize = 4;
	self->colSize = 4;
	
	Py_INCREF(self);
	return (PyObject *)self;
}

static char Matrix_to_4x4_doc[] =
".. method:: to_4x4()\n"
"\n"
"   Return a 4x4 copy of this matrix.\n"
"\n"
"   :return: a new matrix.\n"
"   :rtype: :class:`Matrix`\n";
PyObject *Matrix_to_4x4(MatrixObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->colSize==4 && self->rowSize==4) {
		return (PyObject *)newMatrixObject(self->contigPtr, 4, 4, Py_NEW, Py_TYPE(self));
	}
	else if(self->colSize==3 && self->rowSize==3) {
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
"   :rtype: :class:`Matrix`\n";
PyObject *Matrix_to_3x3(MatrixObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->colSize==3 && self->rowSize==3) {
		return (PyObject *)newMatrixObject(self->contigPtr, 3, 3, Py_NEW, Py_TYPE(self));
	}
	else if(self->colSize==4 && self->rowSize==4) {
		float mat[3][3];
		copy_m3_m4(mat, (float (*)[4])self->contigPtr);
		return (PyObject *)newMatrixObject((float *)mat, 3, 3, Py_NEW, Py_TYPE(self));
	}
	/* TODO, 2x2 matrix */

	PyErr_SetString(PyExc_TypeError, "Matrix.to_3x3(): inappropriate matrix size");
	return NULL;
}

/*---------------------------Matrix.translationPart() ------------*/
static char Matrix_TranslationPart_doc[] =
".. method:: translation_part()\n"
"\n"
"   Return a the translation part of a 4 row matrix.\n"
"\n"
"   :return: Return a the translation of a matrix.\n"
"   :rtype: :class:`Vector`\n"
;
PyObject *Matrix_TranslationPart(MatrixObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(self->colSize < 3 || self->rowSize < 4){
		PyErr_SetString(PyExc_AttributeError, "Matrix.translation_part(): inappropriate matrix size");
		return NULL;
	}

	return newVectorObject(self->matrix[3], 3, Py_NEW, NULL);
}
/*---------------------------Matrix.rotationPart() ---------------*/
static char Matrix_RotationPart_doc[] =
".. method:: rotation_part()\n"
"\n"
"   Return the 3d submatrix corresponding to the linear term of the embedded affine transformation in 3d. This matrix represents rotation and scale.\n"
"\n"
"   :return: Return the 3d matrix for rotation and scale.\n"
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. note:: Note that the (4,4) element of a matrix can be used for uniform scaling too.\n";

PyObject *Matrix_RotationPart(MatrixObject * self)
{
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->colSize < 3 || self->rowSize < 3){
		PyErr_SetString(PyExc_AttributeError, "Matrix.rotation_part(): inappropriate matrix size");
		return NULL;
	}

	mat[0] = self->matrix[0][0];
	mat[1] = self->matrix[0][1];
	mat[2] = self->matrix[0][2];
	mat[3] = self->matrix[1][0];
	mat[4] = self->matrix[1][1];
	mat[5] = self->matrix[1][2];
	mat[6] = self->matrix[2][0];
	mat[7] = self->matrix[2][1];
	mat[8] = self->matrix[2][2];

	return newMatrixObject(mat, 3, 3, Py_NEW, Py_TYPE(self));
}
/*---------------------------Matrix.scalePart() --------------------*/
static char Matrix_scalePart_doc[] =
".. method:: scale_part()\n"
"\n"
"   Return a the scale part of a 3x3 or 4x4 matrix.\n"
"\n"
"   :return: Return a the scale of a matrix.\n"
"   :rtype: :class:`Vector`\n"
"\n"
"   .. note:: This method does not return negative a scale on any axis because it is not possible to obtain this data from the matrix alone.\n";

PyObject *Matrix_scalePart(MatrixObject * self)
{
	float scale[3], rot[3];
	float mat[3][3], imat[3][3], tmat[3][3];

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize == 4 && self->rowSize == 4)
		copy_m3_m4(mat, (float (*)[4])self->contigPtr);
	else if(self->colSize == 3 && self->rowSize == 3)
		copy_m3_m3(mat, (float (*)[3])self->contigPtr);
	else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.scale_part(): inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	}
	/* functionality copied from editobject.c apply_obmat */
	mat3_to_eul( rot,mat);
	eul_to_mat3( tmat,rot);
	invert_m3_m3(imat, tmat);
	mul_m3_m3m3(tmat, imat, mat);
	
	scale[0]= tmat[0][0];
	scale[1]= tmat[1][1];
	scale[2]= tmat[2][2];
	return newVectorObject(scale, 3, Py_NEW, NULL);
}

/*---------------------------Matrix.invert() ---------------------*/
static char Matrix_Invert_doc[] =
".. method:: invert()\n"
"\n"
"   Set the matrix to its inverse.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. note:: :exc:`ValueError` exception is raised.\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Inverse_matrix>\n";

PyObject *Matrix_Invert(MatrixObject * self)
{
	
	int x, y, z = 0;
	float det = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->rowSize != self->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix.invert(ed): only square matrices are supported");
		return NULL;
	}

	/*calculate the determinant*/
	det = matrix_determinant(self);

	if(det != 0) {
		/*calculate the classical adjoint*/
		if(self->rowSize == 2) {
			mat[0] = self->matrix[1][1];
			mat[1] = -self->matrix[0][1];
			mat[2] = -self->matrix[1][0];
			mat[3] = self->matrix[0][0];
		} else if(self->rowSize == 3) {
			adjoint_m3_m3((float (*)[3]) mat,(float (*)[3])self->contigPtr);
		} else if(self->rowSize == 4) {
			adjoint_m4_m4((float (*)[4]) mat, (float (*)[4])self->contigPtr);
		}
		/*divide by determinate*/
		for(x = 0; x < (self->rowSize * self->colSize); x++) {
			mat[x] /= det;
		}
		/*set values*/
		for(x = 0; x < self->rowSize; x++) {
			for(y = 0; y < self->colSize; y++) {
				self->matrix[x][y] = mat[z];
				z++;
			}
		}
		/*transpose
		Matrix_Transpose(self);*/
	} else {
		PyErr_SetString(PyExc_ValueError, "matrix does not have an inverse");
		return NULL;
	}
	
	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
}

/*---------------------------Matrix.decompose() ---------------------*/
static char Matrix_decompose_doc[] =
".. method:: decompose()\n"
"\n"
"   Return the location, rotaion and scale components of this matrix.\n"
"\n"
"   :return: loc, rot, scale triple.\n"
"   :rtype: (:class:`Vector`, :class:`Quaternion`, :class:`Vector`)";
static PyObject *Matrix_decompose(MatrixObject * self)
{
	PyObject *ret;
	float loc[3];
	float rot[3][3];
	float quat[4];
	float size[3];

	if(self->colSize != 4 || self->rowSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "Matrix.decompose(): inappropriate matrix size - expects 4x4 matrix");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self))
		return NULL;

	mat4_to_loc_rot_size(loc, rot, size, (float (*)[4])self->contigPtr);
	mat3_to_quat(quat, rot);

	ret= PyTuple_New(3);
	PyTuple_SET_ITEM(ret, 0, newVectorObject(loc, 3, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 1, newQuaternionObject(quat, Py_NEW, NULL));
	PyTuple_SET_ITEM(ret, 2, newVectorObject(size, 3, Py_NEW, NULL));

	return ret;
}



static char Matrix_Lerp_doc[] =
".. function:: lerp(other, factor)\n"
"\n"
"   Returns the interpolation of two matricies.\n"
"\n"
"   :arg other: value to interpolate with.\n"
"   :type other: :class:`Matrix`\n"
"   :arg factor: The interpolation value in [0.0, 1.0].\n"
"   :type factor: float\n"
"   :return: The interpolated rotation.\n"
"   :rtype: :class:`Matrix`\n";

static PyObject *Matrix_Lerp(MatrixObject *self, PyObject *args)
{
	MatrixObject *mat2 = NULL;
	float fac, mat[MATRIX_MAX_DIM*MATRIX_MAX_DIM];

	if(!PyArg_ParseTuple(args, "O!f:lerp", &matrix_Type, &mat2, &fac))
		return NULL;

	if(self->rowSize != mat2->rowSize || self->colSize != mat2->colSize) {
		PyErr_SetString(PyExc_AttributeError, "matrix.lerp(): expects both matrix objects of the same dimensions");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(mat2))
		return NULL;

	/* TODO, different sized matrix */
	if(self->rowSize==4 && self->colSize==4) {
		blend_m4_m4m4((float (*)[4])mat, (float (*)[4])self->contigPtr, (float (*)[4])mat2->contigPtr, fac);
	}
	else if (self->rowSize==3 && self->colSize==3) {
		blend_m3_m3m3((float (*)[3])mat, (float (*)[3])self->contigPtr, (float (*)[3])mat2->contigPtr, fac);
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "matrix.lerp(): only 3x3 and 4x4 matrices supported");
		return NULL;
	}

	return (PyObject*)newMatrixObject(mat, self->rowSize, self->colSize, Py_NEW, Py_TYPE(self));
}

/*---------------------------Matrix.determinant() ----------------*/
static char Matrix_Determinant_doc[] =
".. method:: determinant()\n"
"\n"
"   Return the determinant of a matrix.\n"
"\n"
"   :return: Return a the determinant of a matrix.\n"
"   :rtype: float\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Determinant>\n";

PyObject *Matrix_Determinant(MatrixObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(self->rowSize != self->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix.determinant: only square matrices are supported");
		return NULL;
	}

	return PyFloat_FromDouble((double)matrix_determinant(self));
}
/*---------------------------Matrix.transpose() ------------------*/
static char Matrix_Transpose_doc[] =
".. method:: transpose()\n"
"\n"
"   Set the matrix to its transpose.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Transpose>\n";

PyObject *Matrix_Transpose(MatrixObject * self)
{
	float t = 0.0f;

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(self->rowSize != self->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix.transpose(d): only square matrices are supported");
		return NULL;
	}

	if(self->rowSize == 2) {
		t = self->matrix[1][0];
		self->matrix[1][0] = self->matrix[0][1];
		self->matrix[0][1] = t;
	} else if(self->rowSize == 3) {
		transpose_m3((float (*)[3])self->contigPtr);
	} else {
		transpose_m4((float (*)[4])self->contigPtr);
	}

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
}


/*---------------------------Matrix.zero() -----------------------*/
static char Matrix_Zero_doc[] =
".. method:: zero()\n"
"\n"
"   Set all the matrix values to zero.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n";

PyObject *Matrix_Zero(MatrixObject * self)
{
	int row, col;
	
	for(row = 0; row < self->rowSize; row++) {
		for(col = 0; col < self->colSize; col++) {
			self->matrix[row][col] = 0.0f;
		}
	}
	
	if(!BaseMath_WriteCallback(self))
		return NULL;
	
	Py_INCREF(self);
	return (PyObject *)self;
}
/*---------------------------Matrix.identity(() ------------------*/
static char Matrix_Identity_doc[] =
".. method:: identity()\n"
"\n"
"   Set the matrix to the identity matrix.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. note:: An object with zero location and rotation, a scale of one, will have an identity matrix.\n"
"\n"
"   .. seealso:: <http://en.wikipedia.org/wiki/Identity_matrix>\n";

PyObject *Matrix_Identity(MatrixObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(self->rowSize != self->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix.identity: only square matrices are supported");
		return NULL;
	}

	if(self->rowSize == 2) {
		self->matrix[0][0] = 1.0f;
		self->matrix[0][1] = 0.0f;
		self->matrix[1][0] = 0.0f;
		self->matrix[1][1] = 1.0f;
	} else if(self->rowSize == 3) {
		unit_m3((float (*)[3])self->contigPtr);
	} else {
		unit_m4((float (*)[4])self->contigPtr);
	}

	if(!BaseMath_WriteCallback(self))
		return NULL;
	
	Py_INCREF(self);
	return (PyObject *)self;
}

/*---------------------------Matrix.copy() ------------------*/
static char Matrix_copy_doc[] =
".. method:: copy()\n"
"\n"
"   Returns a copy of this matrix.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Matrix`\n";

PyObject *Matrix_copy(MatrixObject *self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	return (PyObject*)newMatrixObject((float (*))self->contigPtr, self->rowSize, self->colSize, Py_NEW, Py_TYPE(self));
}

/*----------------------------print object (internal)-------------*/
/*print the object to screen*/
static PyObject *Matrix_repr(MatrixObject * self)
{
	int x, y;
	PyObject *rows[MATRIX_MAX_DIM]= {0};

	if(!BaseMath_ReadCallback(self))
		return NULL;

	for(x = 0; x < self->rowSize; x++){
		rows[x]= PyTuple_New(self->colSize);
		for(y = 0; y < self->colSize; y++) {
			PyTuple_SET_ITEM(rows[x], y, PyFloat_FromDouble(self->matrix[x][y]));
		}
	}
	switch(self->rowSize) {
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

/*------------------------tp_richcmpr*/
/*returns -1 execption, 0 false, 1 true*/
static PyObject* Matrix_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	MatrixObject *matA = NULL, *matB = NULL;
	int result = 0;

	if (!MatrixObject_Check(objectA) || !MatrixObject_Check(objectB)){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}
	matA = (MatrixObject*)objectA;
	matB = (MatrixObject*)objectB;

	if(!BaseMath_ReadCallback(matA) || !BaseMath_ReadCallback(matB))
		return NULL;
	
	if (matA->colSize != matB->colSize || matA->rowSize != matB->rowSize){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}

	switch (comparison_type){
		case Py_EQ:
			/*contigPtr is basically a really long vector*/
			result = EXPP_VectorsAreEqual(matA->contigPtr, matB->contigPtr,
				(matA->rowSize * matA->colSize), 1);
			break;
		case Py_NE:
			result = EXPP_VectorsAreEqual(matA->contigPtr, matB->contigPtr,
				(matA->rowSize * matA->colSize), 1);
			if (result == 0){
				result = 1;
			}else{
				result = 0;
			}
			break;
		default:
			printf("The result of the comparison could not be evaluated");
			break;
	}
	if (result == 1){
		Py_RETURN_TRUE;
	}else{
		Py_RETURN_FALSE;
	}
}

/*---------------------SEQUENCE PROTOCOLS------------------------
  ----------------------------len(object)------------------------
  sequence length*/
static int Matrix_len(MatrixObject * self)
{
	return (self->rowSize);
}
/*----------------------------object[]---------------------------
  sequence accessor (get)
  the wrapped vector gives direct access to the matrix data*/
static PyObject *Matrix_item(MatrixObject * self, int i)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	if(i < 0 || i >= self->rowSize) {
		PyErr_SetString(PyExc_IndexError, "matrix[attribute]: array index out of range");
		return NULL;
	}
	return newVectorObject_cb((PyObject *)self, self->colSize, mathutils_matrix_vector_cb_index, i);
}
/*----------------------------object[]-------------------------
  sequence accessor (set) */

static int Matrix_ass_item(MatrixObject *self, int i, PyObject *value)
{
	float vec[4];
	if(!BaseMath_ReadCallback(self))
		return -1;

	if(i >= self->rowSize || i < 0){
		PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: bad column");
		return -1;
	}

	if(mathutils_array_parse(vec, self->colSize, self->colSize, value, "matrix[i] = value assignment") < 0) {
		return -1;
	}

	memcpy(self->matrix[i], vec, self->colSize * sizeof(float));

	(void)BaseMath_WriteCallback(self);
	return 0;
}

/*----------------------------object[z:y]------------------------
  sequence slice (get)*/
static PyObject *Matrix_slice(MatrixObject * self, int begin, int end)
{

	PyObject *tuple;
	int count;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
	begin= MIN2(begin,end);

	tuple= PyTuple_New(end - begin);
	for(count= begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin,
				newVectorObject_cb((PyObject *)self, self->colSize, mathutils_matrix_vector_cb_index, count));

	}

	return tuple;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set)*/
static int Matrix_ass_slice(MatrixObject * self, int begin, int end, PyObject *value)
{
	PyObject *value_fast= NULL;

	if(!BaseMath_ReadCallback(self))
		return -1;
	
	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
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

			if(mathutils_array_parse(&mat[i * self->colSize], self->colSize, self->colSize, item, "matrix[begin:end] = value assignment") < 0) {
				return -1;
			}
		}

		Py_DECREF(value_fast);

		/*parsed well - now set in matrix*/
		memcpy(self->contigPtr + (begin * self->colSize), mat, sizeof(float) * (size * self->colSize));

		(void)BaseMath_WriteCallback(self);
		return 0;
	}
}
/*------------------------NUMERIC PROTOCOLS----------------------
  ------------------------obj + obj------------------------------*/
static PyObject *Matrix_add(PyObject * m1, PyObject * m2)
{
	float mat[16];
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(mat1) || !BaseMath_ReadCallback(mat2))
		return NULL;
	
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	add_vn_vnvn(mat, mat1->contigPtr, mat2->contigPtr, mat1->rowSize * mat1->colSize);

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW, Py_TYPE(mat1));
}
/*------------------------obj - obj------------------------------
  subtraction*/
static PyObject *Matrix_sub(PyObject * m1, PyObject * m2)
{
	float mat[16];
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(mat1) || !BaseMath_ReadCallback(mat2))
		return NULL;
	
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	sub_vn_vnvn(mat, mat1->contigPtr, mat2->contigPtr, mat1->rowSize * mat1->colSize);

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW, Py_TYPE(mat1));
}
/*------------------------obj * obj------------------------------
  mulplication*/
static PyObject *matrix_mul_float(MatrixObject *mat, const float scalar)
{
	float tmat[16];
	mul_vn_vn_fl(tmat, mat->contigPtr, mat->rowSize * mat->colSize, scalar);
	return newMatrixObject(tmat, mat->rowSize, mat->colSize, Py_NEW, Py_TYPE(mat));
}

static PyObject *Matrix_mul(PyObject * m1, PyObject * m2)
{
	float scalar;

	MatrixObject *mat1 = NULL, *mat2 = NULL;

	if(MatrixObject_Check(m1)) {
		mat1 = (MatrixObject*)m1;
		if(!BaseMath_ReadCallback(mat1))
			return NULL;
	}
	if(MatrixObject_Check(m2)) {
		mat2 = (MatrixObject*)m2;
		if(!BaseMath_ReadCallback(mat2))
			return NULL;
	}

	if(mat1 && mat2) { /*MATRIX * MATRIX*/
		if(mat1->rowSize != mat2->colSize){
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

			for(x = 0; x < mat2->rowSize; x++) {
				for(y = 0; y < mat1->colSize; y++) {
					for(z = 0; z < mat1->rowSize; z++) {
						dot += (mat1->matrix[z][y] * mat2->matrix[x][z]);
					}
					mat[((x * mat1->colSize) + y)] = (float)dot;
					dot = 0.0f;
				}
			}

			return newMatrixObject(mat, mat2->rowSize, mat1->colSize, Py_NEW, Py_TYPE(mat1));
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
	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	return Matrix_Invert(self);
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
			i += self->rowSize;
		return Matrix_item(self, i);
	} else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->rowSize, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyList_New(0);
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
			i += self->rowSize;
		return Matrix_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, self->rowSize, &start, &stop, &step, &slicelength) < 0)
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
		0,							/*nb_remainder*/
		0,							/*nb_divmod*/
		0,							/*nb_power*/
		(unaryfunc) 	0,	/*nb_negative*/
		(unaryfunc) 	0,	/*tp_positive*/
		(unaryfunc) 	0,	/*tp_absolute*/
		(inquiry)	0,	/*tp_bool*/
		(unaryfunc)	Matrix_inv,	/*nb_invert*/
		0,				/*nb_lshift*/
		(binaryfunc)0,	/*nb_rshift*/
		0,				/*nb_and*/
		0,				/*nb_xor*/
		0,				/*nb_or*/
		0,				/*nb_int*/
		0,				/*nb_reserved*/
		0,				/*nb_float*/
		0,				/* nb_inplace_add */
		0,				/* nb_inplace_subtract */
		0,				/* nb_inplace_multiply */
		0,				/* nb_inplace_remainder */
		0,				/* nb_inplace_power */
		0,				/* nb_inplace_lshift */
		0,				/* nb_inplace_rshift */
		0,				/* nb_inplace_and */
		0,				/* nb_inplace_xor */
		0,				/* nb_inplace_or */
		0,				/* nb_floor_divide */
		0,				/* nb_true_divide */
		0,				/* nb_inplace_floor_divide */
		0,				/* nb_inplace_true_divide */
		0,				/* nb_index */
};

static PyObject *Matrix_getRowSize(MatrixObject *self, void *UNUSED(closure))
{
	return PyLong_FromLong((long) self->rowSize);
}

static PyObject *Matrix_getColSize(MatrixObject *self, void *UNUSED(closure))
{
	return PyLong_FromLong((long) self->colSize);
}

static PyObject *Matrix_getMedianScale(MatrixObject *self, void *UNUSED(closure))
{
	float mat[3][3];

	if(!BaseMath_ReadCallback(self))
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize == 4 && self->rowSize == 4)
		copy_m3_m4(mat, (float (*)[4])self->contigPtr);
	else if(self->colSize == 3 && self->rowSize == 3)
		copy_m3_m3(mat, (float (*)[3])self->contigPtr);
	else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.median_scale: inappropriate matrix size - expects 3x3 or 4x4 matrix");
		return NULL;
	}
    
	return PyFloat_FromDouble(mat3_to_scale(mat));
}

static PyObject *Matrix_getIsNegative(MatrixObject *self, void *UNUSED(closure))
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize == 4 && self->rowSize == 4)
		return PyBool_FromLong(is_negative_m4((float (*)[4])self->contigPtr));
	else if(self->colSize == 3 && self->rowSize == 3)
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
	{"zero", (PyCFunction) Matrix_Zero, METH_NOARGS, Matrix_Zero_doc},
	{"identity", (PyCFunction) Matrix_Identity, METH_NOARGS, Matrix_Identity_doc},
	{"transpose", (PyCFunction) Matrix_Transpose, METH_NOARGS, Matrix_Transpose_doc},
	{"lerp", (PyCFunction) Matrix_Lerp, METH_VARARGS, Matrix_Lerp_doc},
	{"determinant", (PyCFunction) Matrix_Determinant, METH_NOARGS, Matrix_Determinant_doc},
	{"invert", (PyCFunction) Matrix_Invert, METH_NOARGS, Matrix_Invert_doc},
	{"translation_part", (PyCFunction) Matrix_TranslationPart, METH_NOARGS, Matrix_TranslationPart_doc},
	{"rotation_part", (PyCFunction) Matrix_RotationPart, METH_NOARGS, Matrix_RotationPart_doc},
	{"scale_part", (PyCFunction) Matrix_scalePart, METH_NOARGS, Matrix_scalePart_doc},
	{"decompose", (PyCFunction) Matrix_decompose, METH_NOARGS, Matrix_decompose_doc},
	{"resize4x4", (PyCFunction) Matrix_Resize4x4, METH_NOARGS, Matrix_Resize4x4_doc},
	{"to_4x4", (PyCFunction) Matrix_to_4x4, METH_NOARGS, Matrix_to_4x4_doc},
	{"to_3x3", (PyCFunction) Matrix_to_3x3, METH_NOARGS, Matrix_to_3x3_doc},
	{"to_euler", (PyCFunction) Matrix_toEuler, METH_VARARGS, Matrix_toEuler_doc},
	{"to_quat", (PyCFunction) Matrix_toQuat, METH_NOARGS, Matrix_toQuat_doc},
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
"This object gives access to Matrices in Blender.";

PyTypeObject matrix_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"mathutils.Matrix",						/*tp_name*/
	sizeof(MatrixObject),			/*tp_basicsize*/
	0,								/*tp_itemsize*/
	(destructor)BaseMathObject_dealloc,		/*tp_dealloc*/
	0,								/*tp_print*/
	0,								/*tp_getattr*/
	0,								/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc) Matrix_repr,			/*tp_repr*/
	&Matrix_NumMethods,				/*tp_as_number*/
	&Matrix_SeqMethods,				/*tp_as_sequence*/
	&Matrix_AsMapping,				/*tp_as_mapping*/
	0,								/*tp_hash*/
	0,								/*tp_call*/
	0,								/*tp_str*/
	0,								/*tp_getattro*/
	0,								/*tp_setattro*/
	0,								/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
	matrix_doc,						/*tp_doc*/
	0,								/*tp_traverse*/
	0,								/*tp_clear*/
	(richcmpfunc)Matrix_richcmpr,	/*tp_richcompare*/
	0,								/*tp_weaklistoffset*/
	0,								/*tp_iter*/
	0,								/*tp_iternext*/
	Matrix_methods,					/*tp_methods*/
	0,								/*tp_members*/
	Matrix_getseters,				/*tp_getset*/
	0,								/*tp_base*/
	0,								/*tp_dict*/
	0,								/*tp_descr_get*/
	0,								/*tp_descr_set*/
	0,								/*tp_dictoffset*/
	0,								/*tp_init*/
	0,								/*tp_alloc*/
	Matrix_new,						/*tp_new*/
	0,								/*tp_free*/
	0,								/*tp_is_gc*/
	0,								/*tp_bases*/
	0,								/*tp_mro*/
	0,								/*tp_cache*/
	0,								/*tp_subclasses*/
	0,								/*tp_weaklist*/
	0								/*tp_del*/
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
	if(rowSize < 2 || rowSize > 4 || colSize < 2 || colSize > 4){
		PyErr_SetString(PyExc_RuntimeError, "matrix(): row and column sizes must be between 2 and 4");
		return NULL;
	}

	if(base_type)	self = (MatrixObject *)base_type->tp_alloc(base_type, 0);
	else			self = PyObject_NEW(MatrixObject, &matrix_Type);

	self->rowSize = rowSize;
	self->colSize = colSize;
	
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
	}else if (type == Py_NEW){
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
		} else if (rowSize == colSize ) { /*or if no arguments are passed return identity matrix for square matrices */
			Matrix_Identity(self);
			Py_DECREF(self);
		}
		self->wrapped = Py_NEW;
	}else{ /*bad type*/
		return NULL;
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
	}
	return (PyObject *) self;
}
