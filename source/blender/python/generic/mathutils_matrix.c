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

#include "BKE_utildefines.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"

static PyObject *column_vector_multiplication(MatrixObject * mat, VectorObject* vec); /* utility func */


/* matrix vector callbacks */
int mathutils_matrix_vector_cb_index= -1;

static int mathutils_matrix_vector_check(PyObject *self_p)
{
	MatrixObject *self= (MatrixObject*)self_p;
	return BaseMath_ReadCallback(self);
}

static int mathutils_matrix_vector_get(PyObject *self_p, int subtype, float *vec_from)
{
	MatrixObject *self= (MatrixObject*)self_p;
	int i;

	if(!BaseMath_ReadCallback(self))
		return 0;

	for(i=0; i<self->colSize; i++)
		vec_from[i]= self->matrix[subtype][i];

	return 1;
}

static int mathutils_matrix_vector_set(PyObject *self_p, int subtype, float *vec_to)
{
	MatrixObject *self= (MatrixObject*)self_p;
	int i;

	if(!BaseMath_ReadCallback(self))
		return 0;

	for(i=0; i<self->colSize; i++)
		self->matrix[subtype][i]= vec_to[i];

	BaseMath_WriteCallback(self);
	return 1;
}

static int mathutils_matrix_vector_get_index(PyObject *self_p, int subtype, float *vec_from, int index)
{
	MatrixObject *self= (MatrixObject*)self_p;

	if(!BaseMath_ReadCallback(self))
		return 0;

	vec_from[index]= self->matrix[subtype][index];
	return 1;
}

static int mathutils_matrix_vector_set_index(PyObject *self_p, int subtype, float *vec_to, int index)
{
	MatrixObject *self= (MatrixObject*)self_p;

	if(!BaseMath_ReadCallback(self))
		return 0;

	self->matrix[subtype][index]= vec_to[index];

	BaseMath_WriteCallback(self);
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
	PyObject *argObject, *m, *s;
	MatrixObject *mat;
	int argSize, seqSize = 0, i, j;
	float matrix[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	float scalar;

	argSize = PyTuple_GET_SIZE(args);
	if(argSize > MATRIX_MAX_DIM) {	//bad arg nums
		PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
		return NULL;
	} else if (argSize == 0) { //return empty 4D matrix
		return (PyObject *) newMatrixObject(NULL, 4, 4, Py_NEW, NULL);
	}else if (argSize == 1){
		//copy constructor for matrix objects
		argObject = PyTuple_GET_ITEM(args, 0);
		if(MatrixObject_Check(argObject)){
			mat = (MatrixObject*)argObject;
			if(!BaseMath_ReadCallback(mat))
				return NULL;

			memcpy(matrix, mat->contigPtr, sizeof(float) * mat->rowSize * mat->colSize);
			argSize = mat->rowSize;
			seqSize = mat->colSize;
		}
	}else{ //2-4 arguments (all seqs? all same size?)
		for(i =0; i < argSize; i++){
			argObject = PyTuple_GET_ITEM(args, i);
			if (PySequence_Check(argObject)) { //seq?
				if(seqSize){ //0 at first
					if(PySequence_Length(argObject) != seqSize){ //seq size not same
						PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
						return NULL;
					}
				}
				seqSize = PySequence_Length(argObject);
			}else{ //arg not a sequence
				PyErr_SetString(PyExc_TypeError, "mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
				return NULL;
			}
		}
		//all is well... let's continue parsing
		for (i = 0; i < argSize; i++){
			m = PyTuple_GET_ITEM(args, i);
			if (m == NULL) { // Failed to read sequence
				PyErr_SetString(PyExc_RuntimeError, "mathutils.Matrix(): failed to parse arguments...\n");
				return NULL;
			}

			for (j = 0; j < seqSize; j++) {
				s = PySequence_GetItem(m, j);
				if (s == NULL) { // Failed to read sequence
					PyErr_SetString(PyExc_RuntimeError, "mathutils.Matrix(): failed to parse arguments...\n");
					return NULL;
				}
				
				scalar= (float)PyFloat_AsDouble(s);
				Py_DECREF(s);
				
				if(scalar==-1 && PyErr_Occurred()) { // parsed item is not a number
					PyErr_SetString(PyExc_AttributeError, "mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
					return NULL;
				}

				matrix[(seqSize*i)+j]= scalar;
			}
		}
	}
	return newMatrixObject(matrix, argSize, seqSize, Py_NEW, NULL);
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
	short order= 0;
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

		VECCOPY(eul_compatf, eul_compat->eul);
	}
	
	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize ==3 && self->rowSize ==3) {
		mat= (float (*)[3])self->contigPtr;
	}else if (self->colSize ==4 && self->rowSize ==4) {
		copy_m3_m4(tmat, (float (*)[4])self->contigPtr);
		mat= tmat;
	}else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.to_euler(): inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
		return NULL;
	}

	if(order_str) {
		order= euler_order_from_string(order_str, "Matrix.to_euler()");

		if(order < 0)
			return NULL;
	}

	if(eul_compat) {
		if(order == 0)	mat3_to_compatible_eul( eul, eul_compatf, mat);
		else			mat3_to_compatible_eulO(eul, eul_compatf, order, mat);
	}
	else {
		if(order == 0)	mat3_to_eul(eul, mat);
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
"   :rtype: :class:`Matrix`\n"
"\n"
"   .. note:: Note that the (4,4) element of a matrix can be used for uniform scaling too.\n";

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
		PyErr_SetString(PyExc_AttributeError, "Matrix.rotation_part(): inappropriate matrix size\n");
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
		PyErr_SetString(PyExc_AttributeError, "Matrix.scale_part(): inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
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
	
	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
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

	BaseMath_WriteCallback(self);
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
		PyErr_SetString(PyExc_AttributeError, "Matrix.identity: only square matrices are supported\n");
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

PyObject *Matrix_copy(MatrixObject * self)
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
	char buffer[48], str[1024];

	if(!BaseMath_ReadCallback(self))
		return NULL;
	
	BLI_strncpy(str,"",1024);
	for(x = 0; x < self->colSize; x++){
		sprintf(buffer, "[");
		strcat(str,buffer);
		for(y = 0; y < (self->rowSize - 1); y++) {
			sprintf(buffer, "%.6f, ", self->matrix[y][x]);
			strcat(str,buffer);
		}
		if(x < (self->colSize-1)){
			sprintf(buffer, "%.6f](matrix [row %d])\n", self->matrix[y][x], x);
			strcat(str,buffer);
		}else{
			sprintf(buffer, "%.6f](matrix [row %d])", self->matrix[y][x], x);
			strcat(str,buffer);
		}
	}

	return PyUnicode_FromString(str);
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
  sequence accessor (set)*/
static int Matrix_ass_item(MatrixObject * self, int i, PyObject * ob)
{
	int y, x, size = 0;
	float vec[4];
	PyObject *m, *f;

	if(!BaseMath_ReadCallback(self))
		return -1;
	
	if(i >= self->rowSize || i < 0){
		PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: bad column\n");
		return -1;
	}

	if(PySequence_Check(ob)){
		size = PySequence_Length(ob);
		if(size != self->colSize){
			PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: bad sequence size\n");
			return -1;
		}
		for (x = 0; x < size; x++) {
			m = PySequence_GetItem(ob, x);
			if (m == NULL) { /*Failed to read sequence*/
				PyErr_SetString(PyExc_RuntimeError, "matrix[attribute] = x: unable to read sequence\n");
				return -1;
			}

			f = PyNumber_Float(m);
			if(f == NULL) { /*parsed item not a number*/
				Py_DECREF(m);
				PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: sequence argument not a number\n");
				return -1;
			}

			vec[x] = (float)PyFloat_AS_DOUBLE(f);
			Py_DECREF(m);
			Py_DECREF(f);
		}
		/*parsed well - now set in matrix*/
		for(y = 0; y < size; y++){
			self->matrix[i][y] = vec[y];
		}
		
		BaseMath_WriteCallback(self);
		return 0;
	}else{
		PyErr_SetString(PyExc_TypeError, "matrix[attribute] = x: expects a sequence of column size\n");
		return -1;
	}
}
/*----------------------------object[z:y]------------------------
  sequence slice (get)*/
static PyObject *Matrix_slice(MatrixObject * self, int begin, int end)
{

	PyObject *list = NULL;
	int count;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				newVectorObject_cb((PyObject *)self, self->colSize, mathutils_matrix_vector_cb_index, count));

	}

	return list;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set)*/
static int Matrix_ass_slice(MatrixObject * self, int begin, int end, PyObject * seq)
{
	int i, x, y, size, sub_size = 0;
	float mat[16], f;
	PyObject *subseq;
	PyObject *m;

	if(!BaseMath_ReadCallback(self))
		return -1;
	
	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
	begin = MIN2(begin,end);

	if(PySequence_Check(seq)){
		size = PySequence_Length(seq);
		if(size != (end - begin)){
			PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: size mismatch in slice assignment\n");
			return -1;
		}
		/*parse sub items*/
		for (i = 0; i < size; i++) {
			/*parse each sub sequence*/
			subseq = PySequence_GetItem(seq, i);
			if (subseq == NULL) { /*Failed to read sequence*/
				PyErr_SetString(PyExc_RuntimeError, "matrix[begin:end] = []: unable to read sequence");
				return -1;
			}

			if(PySequence_Check(subseq)){
				/*subsequence is also a sequence*/
				sub_size = PySequence_Length(subseq);
				if(sub_size != self->colSize){
					Py_DECREF(subseq);
					PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: size mismatch in slice assignment\n");
					return -1;
				}
				for (y = 0; y < sub_size; y++) {
					m = PySequence_GetItem(subseq, y);
					if (m == NULL) { /*Failed to read sequence*/
						Py_DECREF(subseq);
						PyErr_SetString(PyExc_RuntimeError, "matrix[begin:end] = []: unable to read sequence\n");
						return -1;
					}
					
					f = PyFloat_AsDouble(m); /* faster to assume a float and raise an error after */
					if(f == -1 && PyErr_Occurred()) { /*parsed item not a number*/
						Py_DECREF(m);
						Py_DECREF(subseq);
						PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: sequence argument not a number\n");
						return -1;
					}

					mat[(i * self->colSize) + y] = f;
					Py_DECREF(m);
				}
			}else{
				Py_DECREF(subseq);
				PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: illegal argument type for built-in operation\n");
				return -1;
			}
			Py_DECREF(subseq);
		}
		/*parsed well - now set in matrix*/
		for(x = 0; x < (size * sub_size); x++){
			self->matrix[begin + (int)floor(x / self->colSize)][x % self->colSize] = mat[x];
		}
		
		BaseMath_WriteCallback(self);
		return 0;
	}else{
		PyErr_SetString(PyExc_TypeError, "matrix[begin:end] = []: illegal argument type for built-in operation\n");
		return -1;
	}
}
/*------------------------NUMERIC PROTOCOLS----------------------
  ------------------------obj + obj------------------------------*/
static PyObject *Matrix_add(PyObject * m1, PyObject * m2)
{
	int x, y;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation....");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(mat1) || !BaseMath_ReadCallback(mat2))
		return NULL;
	
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	for(x = 0; x < mat1->rowSize; x++) {
		for(y = 0; y < mat1->colSize; y++) {
			mat[((x * mat1->colSize) + y)] = mat1->matrix[x][y] + mat2->matrix[x][y];
		}
	}

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW, NULL);
}
/*------------------------obj - obj------------------------------
  subtraction*/
static PyObject *Matrix_sub(PyObject * m1, PyObject * m2)
{
	int x, y;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	MatrixObject *mat1 = NULL, *mat2 = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(!MatrixObject_Check(m1) || !MatrixObject_Check(m2)) {
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: arguments not valid for this operation....");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(mat1) || !BaseMath_ReadCallback(mat2))
		return NULL;
	
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		PyErr_SetString(PyExc_AttributeError, "Matrix addition: matrices must have the same dimensions for this operation");
		return NULL;
	}

	for(x = 0; x < mat1->rowSize; x++) {
		for(y = 0; y < mat1->colSize; y++) {
			mat[((x * mat1->colSize) + y)] = mat1->matrix[x][y] - mat2->matrix[x][y];
		}
	}

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW, NULL);
}
/*------------------------obj * obj------------------------------
  mulplication*/
static PyObject *Matrix_mul(PyObject * m1, PyObject * m2)
{
	int x, y, z;
	float scalar;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	double dot = 0.0f;
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
		for(x = 0; x < mat2->rowSize; x++) {
			for(y = 0; y < mat1->colSize; y++) {
				for(z = 0; z < mat1->rowSize; z++) {
					dot += (mat1->matrix[z][y] * mat2->matrix[x][z]);
				}
				mat[((x * mat1->colSize) + y)] = (float)dot;
				dot = 0.0f;
			}
		}
		
		return newMatrixObject(mat, mat2->rowSize, mat1->colSize, Py_NEW, NULL);
	}
	
	if(mat1==NULL){
		scalar=PyFloat_AsDouble(m1); // may not be a float...
		if ((scalar == -1.0 && PyErr_Occurred())==0) { /*FLOAT/INT * MATRIX, this line annoys theeth, lets see if he finds it */
			for(x = 0; x < mat2->rowSize; x++) {
				for(y = 0; y < mat2->colSize; y++) {
					mat[((x * mat2->colSize) + y)] = scalar * mat2->matrix[x][y];
				}
			}
			return newMatrixObject(mat, mat2->rowSize, mat2->colSize, Py_NEW, NULL);
		}
		
		PyErr_SetString(PyExc_TypeError, "Matrix multiplication: arguments not acceptable for this operation");
		return NULL;
	}
	else /* if(mat1) { */ {
		
		if(VectorObject_Check(m2)) { /* MATRIX*VECTOR */
			return column_vector_multiplication(mat1, (VectorObject *)m2); /* vector update done inside the function */
		}
		else {
			scalar= PyFloat_AsDouble(m2);
			if ((scalar == -1.0 && PyErr_Occurred())==0) { /* MATRIX*FLOAT/INT */
				for(x = 0; x < mat1->rowSize; x++) {
					for(y = 0; y < mat1->colSize; y++) {
						mat[((x * mat1->colSize) + y)] = scalar * mat1->matrix[x][y];
					}
				}
				return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW, NULL);
			}
		}
		PyErr_SetString(PyExc_TypeError, "Matrix multiplication: arguments not acceptable for this operation");
		return NULL;
	}

	PyErr_SetString(PyExc_TypeError, "Matrix multiplication: arguments not acceptable for this operation\n");
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
	(lenfunc) Matrix_len,					/* sq_length */
	(binaryfunc) 0,							/* sq_concat */
	(ssizeargfunc) 0,							/* sq_repeat */
	(ssizeargfunc) Matrix_item,				/* sq_item */
	(ssizessizeargfunc) Matrix_slice,			/* sq_slice */
	(ssizeobjargproc) Matrix_ass_item,		/* sq_ass_item */
	(ssizessizeobjargproc) Matrix_ass_slice,	/* sq_ass_slice */
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

		if (PySlice_GetIndicesEx((PySliceObject*)item, self->rowSize, &start, &stop, &step, &slicelength) < 0)
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
		PyErr_Format(PyExc_TypeError,
				 "vector indices must be integers, not %.200s",
				 item->ob_type->tp_name);
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

		if (PySlice_GetIndicesEx((PySliceObject*)item, self->rowSize, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Matrix_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with matricies");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
				 "matrix indices must be integers, not %.200s",
				 item->ob_type->tp_name);
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

static PyObject *Matrix_getRowSize( MatrixObject * self, void *type )
{
	return PyLong_FromLong((long) self->rowSize);
}

static PyObject *Matrix_getColSize( MatrixObject * self, void *type )
{
	return PyLong_FromLong((long) self->colSize);
}

static PyObject *Matrix_getMedianScale( MatrixObject * self, void *type )
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
		PyErr_SetString(PyExc_AttributeError, "Matrix.median_scale: inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
		return NULL;
	}
    
	return PyFloat_FromDouble(mat3_to_scale(mat));
}

static PyObject *Matrix_getIsNegative( MatrixObject * self, void *type )
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize == 4 && self->rowSize == 4)
		return PyBool_FromLong(is_negative_m4((float (*)[4])self->contigPtr));
	else if(self->colSize == 3 && self->rowSize == 3)
		return PyBool_FromLong(is_negative_m3((float (*)[3])self->contigPtr));
	else {
		PyErr_SetString(PyExc_AttributeError, "Matrix.is_negative: inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
		return NULL;
	}
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Matrix_getseters[] = {
	{"row_size", (getter)Matrix_getRowSize, (setter)NULL, "The row size of the matrix (readonly). **type** int", NULL},
	{"col_size", (getter)Matrix_getColSize, (setter)NULL, "The column size of the matrix (readonly). **type** int", NULL},
	{"median_scale", (getter)Matrix_getMedianScale, (setter)NULL, "The average scale applied to each axis (readonly). **type** float", NULL},
	{"is_negative", (getter)Matrix_getIsNegative, (setter)NULL, "True if this matrix results in a negative scale, 3x3 and 4x4 only, (readonly). **type** bool", NULL},
	{"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, BaseMathObject_Wrapped_doc, NULL},
	{"_owner",(getter)BaseMathObject_getOwner, (setter)NULL, BaseMathObject_Owner_doc, NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*-----------------------METHOD DEFINITIONS ----------------------*/
static struct PyMethodDef Matrix_methods[] = {
	{"zero", (PyCFunction) Matrix_Zero, METH_NOARGS, Matrix_Zero_doc},
	{"identity", (PyCFunction) Matrix_Identity, METH_NOARGS, Matrix_Identity_doc},
	{"transpose", (PyCFunction) Matrix_Transpose, METH_NOARGS, Matrix_Transpose_doc},
	{"determinant", (PyCFunction) Matrix_Determinant, METH_NOARGS, Matrix_Determinant_doc},
	{"invert", (PyCFunction) Matrix_Invert, METH_NOARGS, Matrix_Invert_doc},
	{"translation_part", (PyCFunction) Matrix_TranslationPart, METH_NOARGS, Matrix_TranslationPart_doc},
	{"rotation_part", (PyCFunction) Matrix_RotationPart, METH_NOARGS, Matrix_RotationPart_doc},
	{"scale_part", (PyCFunction) Matrix_scalePart, METH_NOARGS, Matrix_scalePart_doc},
	{"resize4x4", (PyCFunction) Matrix_Resize4x4, METH_NOARGS, Matrix_Resize4x4_doc},
	{"to_4x4", (PyCFunction) Matrix_to_4x4, METH_NOARGS, Matrix_to_4x4_doc},
	{"to_3x3", (PyCFunction) Matrix_to_3x3, METH_NOARGS, Matrix_to_3x3_doc},
	{"to_euler", (PyCFunction) Matrix_toEuler, METH_VARARGS, Matrix_toEuler_doc},
	{"to_quat", (PyCFunction) Matrix_toQuat, METH_NOARGS, Matrix_toQuat_doc},
	{"copy", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},
	{"__copy__", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},
	{NULL, NULL, 0, NULL}
};

/*------------------PY_OBECT DEFINITION--------------------------*/
static char matrix_doc[] =
"This object gives access to Matrices in Blender.";

PyTypeObject matrix_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"matrix",						/*tp_name*/
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
				 ....
self->matrix[1][1] = self->contigPtr[4] */

/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newMatrixObject(float *mat, int rowSize, int colSize, int type, PyTypeObject *base_type)
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
			PyErr_SetString( PyExc_MemoryError, "matrix(): problem allocating pointer space\n");
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

//----------------column_vector_multiplication (internal)---------
//COLUMN VECTOR Multiplication (Matrix X Vector)
// [1][4][7]   [a]
// [2][5][8] * [b]
// [3][6][9]   [c]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
static PyObject *column_vector_multiplication(MatrixObject * mat, VectorObject* vec)
{
	float vecNew[4], vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0;

	if(!BaseMath_ReadCallback(mat) || !BaseMath_ReadCallback(vec))
		return NULL;
	
	if(mat->rowSize != vec->size){
		if(mat->rowSize == 4 && vec->size != 3){
			PyErr_SetString(PyExc_AttributeError, "matrix * vector: matrix row size and vector size must be the same");
			return NULL;
		}else{
			vecCopy[3] = 1.0f;
		}
	}

	for(x = 0; x < vec->size; x++){
		vecCopy[x] = vec->vec[x];
	}
	vecNew[3] = 1.0f;

	for(x = 0; x < mat->colSize; x++) {
		for(y = 0; y < mat->rowSize; y++) {
			dot += mat->matrix[y][x] * vecCopy[y];
		}
		vecNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newVectorObject(vecNew, vec->size, Py_NEW, NULL);
}
