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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Michel Selten & Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Mathutils.h"

#include "BKE_utildefines.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "gen_utils.h"

/*-------------------------DOC STRINGS ---------------------------*/
char Matrix_Zero_doc[] = "() - set all values in the matrix to 0";
char Matrix_Identity_doc[] = "() - set the square matrix to it's identity matrix";
char Matrix_Transpose_doc[] = "() - set the matrix to it's transpose";
char Matrix_Determinant_doc[] = "() - return the determinant of the matrix";
char Matrix_Invert_doc[] =  "() - set the matrix to it's inverse if an inverse is possible";
char Matrix_TranslationPart_doc[] = "() - return a vector encompassing the translation of the matrix";
char Matrix_RotationPart_doc[] = "() - return a vector encompassing the rotation of the matrix";
char Matrix_scalePart_doc[] = "() - convert matrix to a 3D vector";
char Matrix_Resize4x4_doc[] = "() - resize the matrix to a 4x4 square matrix";
char Matrix_toEuler_doc[] = "() - convert matrix to a euler angle rotation";
char Matrix_toQuat_doc[] = "() - convert matrix to a quaternion rotation";
char Matrix_copy_doc[] = "() - return a copy of the matrix";
/*-----------------------METHOD DEFINITIONS ----------------------*/
struct PyMethodDef Matrix_methods[] = {
	{"zero", (PyCFunction) Matrix_Zero, METH_NOARGS, Matrix_Zero_doc},
	{"identity", (PyCFunction) Matrix_Identity, METH_NOARGS, Matrix_Identity_doc},
	{"transpose", (PyCFunction) Matrix_Transpose, METH_NOARGS, Matrix_Transpose_doc},
	{"determinant", (PyCFunction) Matrix_Determinant, METH_NOARGS, Matrix_Determinant_doc},
	{"invert", (PyCFunction) Matrix_Invert, METH_NOARGS, Matrix_Invert_doc},
	{"translationPart", (PyCFunction) Matrix_TranslationPart, METH_NOARGS, Matrix_TranslationPart_doc},
	{"rotationPart", (PyCFunction) Matrix_RotationPart, METH_NOARGS, Matrix_RotationPart_doc},
	{"scalePart", (PyCFunction) Matrix_scalePart, METH_NOARGS, Matrix_scalePart_doc},
	{"resize4x4", (PyCFunction) Matrix_Resize4x4, METH_NOARGS, Matrix_Resize4x4_doc},
	{"toEuler", (PyCFunction) Matrix_toEuler, METH_NOARGS, Matrix_toEuler_doc},
	{"toQuat", (PyCFunction) Matrix_toQuat, METH_NOARGS, Matrix_toQuat_doc},
	{"copy", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},
	{"__copy__", (PyCFunction) Matrix_copy, METH_NOARGS, Matrix_copy_doc},
	{NULL, NULL, 0, NULL}
};
/*-----------------------------METHODS----------------------------*/
/*---------------------------Matrix.toQuat() ---------------------*/
PyObject *Matrix_toQuat(MatrixObject * self)
{
	float quat[4];

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize < 3 || self->rowSize < 3 || (self->colSize != self->rowSize)) {
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.toQuat(): inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
	} 
	if(self->colSize == 3){
        Mat3ToQuat((float (*)[3])*self->matrix, quat);
	}else{
		Mat4ToQuat((float (*)[4])*self->matrix, quat);
	}
	
	return newQuaternionObject(quat, Py_NEW);
}
/*---------------------------Matrix.toEuler() --------------------*/
PyObject *Matrix_toEuler(MatrixObject * self)
{
	float eul[3];
	
	int x;

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize ==3 && self->rowSize ==3) {
		Mat3ToEul((float (*)[3])*self->matrix, eul);
	}else if (self->colSize ==4 && self->rowSize ==4) {
		float tempmat3[3][3];
		Mat3CpyMat4(tempmat3, (float (*)[4])*self->matrix);
		Mat3ToEul(tempmat3, eul);
	}else
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.toEuler(): inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
	
	/*have to convert to degrees*/
	for(x = 0; x < 3; x++) {
		eul[x] *= (float) (180 / Py_PI);
	}
	return newEulerObject(eul, Py_NEW);
}
/*---------------------------Matrix.resize4x4() ------------------*/
PyObject *Matrix_Resize4x4(MatrixObject * self)
{
	int x, first_row_elem, curr_pos, new_pos, blank_columns, blank_rows, index;

	if(self->data.blend_data){
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"cannot resize wrapped data - only python matrices\n");
	}

	self->data.py_data = PyMem_Realloc(self->data.py_data, (sizeof(float) * 16));
	if(self->data.py_data == NULL) {
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"matrix.resize4x4(): problem allocating pointer space\n\n");
	}
	self->contigPtr = self->data.py_data;  /*force*/
	self->matrix = PyMem_Realloc(self->matrix, (sizeof(float *) * 4));
	if(self->matrix == NULL) {
		return EXPP_ReturnPyObjError(PyExc_MemoryError,
			"matrix.resize4x4(): problem allocating pointer space\n\n");
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
	return EXPP_incr_ret((PyObject*)self);
}
/*---------------------------Matrix.translationPart() ------------*/
PyObject *Matrix_TranslationPart(MatrixObject * self)
{
	float vec[4];

	if(self->colSize < 3 || self->rowSize < 4){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.translationPart: inappropriate matrix size\n");
	}

	vec[0] = self->matrix[3][0];
	vec[1] = self->matrix[3][1];
	vec[2] = self->matrix[3][2];

	return newVectorObject(vec, 3, Py_NEW);
}
/*---------------------------Matrix.rotationPart() ---------------*/
PyObject *Matrix_RotationPart(MatrixObject * self)
{
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(self->colSize < 3 || self->rowSize < 3){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.rotationPart: inappropriate matrix size\n");
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

	return newMatrixObject(mat, 3, 3, Py_NEW);
}
/*---------------------------Matrix.scalePart() --------------------*/
PyObject *Matrix_scalePart(MatrixObject * self)
{
	float scale[3], rot[3];
	float mat[3][3], imat[3][3], tmat[3][3];

	/*must be 3-4 cols, 3-4 rows, square matrix*/
	if(self->colSize == 4 && self->rowSize == 4)
		Mat3CpyMat4(mat, (float (*)[4])*self->matrix);
	else if(self->colSize == 3 && self->rowSize == 3)
		Mat3CpyMat3(mat, (float (*)[3])*self->matrix);
	else
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.scalePart(): inappropriate matrix size - expects 3x3 or 4x4 matrix\n");
	
	/* functionality copied from editobject.c apply_obmat */
	Mat3ToEul(mat, rot);
	EulToMat3(rot, tmat);
	Mat3Inv(imat, tmat);
	Mat3MulMat3(tmat, imat, mat);
	
	scale[0]= tmat[0][0];
	scale[1]= tmat[1][1];
	scale[2]= tmat[2][2];
	return newVectorObject(scale, 3, Py_NEW);
}
/*---------------------------Matrix.invert() ---------------------*/
PyObject *Matrix_Invert(MatrixObject * self)
{
	
	int x, y, z = 0;
	float det = 0.0f;
	PyObject *f = NULL;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(self->rowSize != self->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.invert(ed): only square matrices are supported\n");
	}

	/*calculate the determinant*/
	f = Matrix_Determinant(self);
	det = (float)PyFloat_AS_DOUBLE(f); /*Increfs, so we need to decref*/
	Py_DECREF(f);

	if(det != 0) {
		/*calculate the classical adjoint*/
		if(self->rowSize == 2) {
			mat[0] = self->matrix[1][1];
			mat[1] = -self->matrix[1][0];
			mat[2] = -self->matrix[0][1];
			mat[3] = self->matrix[0][0];
		} else if(self->rowSize == 3) {
			Mat3Adj((float (*)[3]) mat,(float (*)[3]) *self->matrix);
		} else if(self->rowSize == 4) {
			Mat4Adj((float (*)[4]) mat, (float (*)[4]) *self->matrix);
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
		return EXPP_ReturnPyObjError(PyExc_ValueError,
				"matrix does not have an inverse");
	}
	return EXPP_incr_ret((PyObject*)self);
}


/*---------------------------Matrix.determinant() ----------------*/
PyObject *Matrix_Determinant(MatrixObject * self)
{
	float det = 0.0f;

	if(self->rowSize != self->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.determinant: only square matrices are supported\n");
	}

	if(self->rowSize == 2) {
		det = Det2x2(self->matrix[0][0], self->matrix[0][1],
					 self->matrix[1][0], self->matrix[1][1]);
	} else if(self->rowSize == 3) {
		det = Det3x3(self->matrix[0][0], self->matrix[0][1],
					 self->matrix[0][2], self->matrix[1][0],
					 self->matrix[1][1], self->matrix[1][2],
					 self->matrix[2][0], self->matrix[2][1],
					 self->matrix[2][2]);
	} else {
		det = Det4x4((float (*)[4]) *self->matrix);
	}

	return PyFloat_FromDouble( (double) det );
}
/*---------------------------Matrix.transpose() ------------------*/
PyObject *Matrix_Transpose(MatrixObject * self)
{
	float t = 0.0f;

	if(self->rowSize != self->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.transpose(d): only square matrices are supported\n");
	}

	if(self->rowSize == 2) {
		t = self->matrix[1][0];
		self->matrix[1][0] = self->matrix[0][1];
		self->matrix[0][1] = t;
	} else if(self->rowSize == 3) {
		Mat3Transp((float (*)[3])*self->matrix);
	} else {
		Mat4Transp((float (*)[4])*self->matrix);
	}

	return EXPP_incr_ret((PyObject*)self);
}


/*---------------------------Matrix.zero() -----------------------*/
PyObject *Matrix_Zero(MatrixObject * self)
{
	int row, col;

	for(row = 0; row < self->rowSize; row++) {
		for(col = 0; col < self->colSize; col++) {
			self->matrix[row][col] = 0.0f;
		}
	}
	return EXPP_incr_ret((PyObject*)self);
}
/*---------------------------Matrix.identity(() ------------------*/
PyObject *Matrix_Identity(MatrixObject * self)
{
	if(self->rowSize != self->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix.identity: only square matrices are supported\n");
	}

	if(self->rowSize == 2) {
		self->matrix[0][0] = 1.0f;
		self->matrix[0][1] = 0.0f;
		self->matrix[1][0] = 0.0f;
		self->matrix[1][1] = 1.0f;
	} else if(self->rowSize == 3) {
		Mat3One((float (*)[3]) *self->matrix);
	} else {
		Mat4One((float (*)[4]) *self->matrix);
	}

	return EXPP_incr_ret((PyObject*)self);
}

/*---------------------------Matrix.inverted() ------------------*/
PyObject *Matrix_copy(MatrixObject * self)
{
	return (PyObject*)(MatrixObject*)newMatrixObject((float (*))*self->matrix, self->rowSize, self->colSize, Py_NEW);
}

/*----------------------------dealloc()(internal) ----------------*/
/*free the py_object*/
static void Matrix_dealloc(MatrixObject * self)
{
	Py_XDECREF(self->coerced_object);
	PyMem_Free(self->matrix);
	/*only free py_data*/
	if(self->data.py_data){
		PyMem_Free(self->data.py_data);
	}
	PyObject_DEL(self);
}
/*----------------------------getattr()(internal) ----------------*/
/*object.attribute access (get)*/
static PyObject *Matrix_getattr(MatrixObject * self, char *name)
{
	if(STREQ(name, "rowSize")) {
		return PyInt_FromLong((long) self->rowSize);
	} else if(STREQ(name, "colSize")) {
		return PyInt_FromLong((long) self->colSize);
	}
	if(STREQ(name, "wrapped")){
		if(self->wrapped == Py_WRAP)
			return EXPP_incr_ret((PyObject *)Py_True);
		else 
			return EXPP_incr_ret((PyObject *)Py_False);
	}
	return Py_FindMethod(Matrix_methods, (PyObject *) self, name);
}
/*----------------------------setattr()(internal) ----------------*/
/*object.attribute access (set)*/
static int Matrix_setattr(MatrixObject * self, char *name, PyObject * v)
{
	/* This is not supported. */
	return (-1);
}
/*----------------------------print object (internal)-------------*/
/*print the object to screen*/
static PyObject *Matrix_repr(MatrixObject * self)
{
	int x, y;
	char buffer[48], str[1024];

	BLI_strncpy(str,"",1024);
	for(x = 0; x < self->rowSize; x++){
		sprintf(buffer, "[");
		strcat(str,buffer);
		for(y = 0; y < (self->colSize - 1); y++) {
			sprintf(buffer, "%.6f, ", self->matrix[x][y]);
			strcat(str,buffer);
		}
		if(x < (self->rowSize-1)){
			sprintf(buffer, "%.6f](matrix [row %d])\n", self->matrix[x][y], x);
			strcat(str,buffer);
		}else{
			sprintf(buffer, "%.6f](matrix [row %d])", self->matrix[x][y], x);
			strcat(str,buffer);
		}
	}

	return PyString_FromString(str);
}
/*------------------------tp_richcmpr*/
/*returns -1 execption, 0 false, 1 true*/
static PyObject* Matrix_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	MatrixObject *matA = NULL, *matB = NULL;
	int result = 0;

	if (!MatrixObject_Check(objectA) || !MatrixObject_Check(objectB)){
		if (comparison_type == Py_NE){
			return EXPP_incr_ret(Py_True); 
		}else{
			return EXPP_incr_ret(Py_False);
		}
	}
	matA = (MatrixObject*)objectA;
	matB = (MatrixObject*)objectB;

	if (matA->colSize != matB->colSize || matA->rowSize != matB->rowSize){
		if (comparison_type == Py_NE){
			return EXPP_incr_ret(Py_True); 
		}else{
			return EXPP_incr_ret(Py_False);
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
		return EXPP_incr_ret(Py_True);
	}else{
		return EXPP_incr_ret(Py_False);
	}
}
/*------------------------tp_doc*/
static char MatrixObject_doc[] = "This is a wrapper for matrix objects.";
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
	if(i < 0 || i >= self->rowSize)
		return EXPP_ReturnPyObjError(PyExc_IndexError,
		"matrix[attribute]: array index out of range\n");

	return newVectorObject(self->matrix[i], self->colSize, Py_WRAP);
}
/*----------------------------object[]-------------------------
  sequence accessor (set)*/
static int Matrix_ass_item(MatrixObject * self, int i, PyObject * ob)
{
	int y, x, size = 0;
	float vec[4];
	PyObject *m, *f;

	if(i >= self->rowSize || i < 0){
		return EXPP_ReturnIntError(PyExc_TypeError,
			"matrix[attribute] = x: bad row\n");
	}

	if(PySequence_Check(ob)){
		size = PySequence_Length(ob);
		if(size != self->colSize){
			return EXPP_ReturnIntError(PyExc_TypeError,
				"matrix[attribute] = x: bad sequence size\n");
		}
		for (x = 0; x < size; x++) {
			m = PySequence_GetItem(ob, x);
			if (m == NULL) { /*Failed to read sequence*/
				return EXPP_ReturnIntError(PyExc_RuntimeError, 
					"matrix[attribute] = x: unable to read sequence\n");
			}

			f = PyNumber_Float(m);
			if(f == NULL) { /*parsed item not a number*/
				Py_DECREF(m);
				return EXPP_ReturnIntError(PyExc_TypeError, 
					"matrix[attribute] = x: sequence argument not a number\n");
			}

			vec[x] = (float)PyFloat_AS_DOUBLE(f);
			EXPP_decr2(m, f);
		}
		/*parsed well - now set in matrix*/
		for(y = 0; y < size; y++){
			self->matrix[i][y] = vec[y];
		}
		return 0;
	}else{
		return EXPP_ReturnIntError(PyExc_TypeError,
			"matrix[attribute] = x: expects a sequence of column size\n");
	}
}
/*----------------------------object[z:y]------------------------
  sequence slice (get)*/
static PyObject *Matrix_slice(MatrixObject * self, int begin, int end)
{

	PyObject *list = NULL;
	int count;

	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				newVectorObject(self->matrix[count], self->colSize, Py_WRAP));
	}

	return list;
}
/*----------------------------object[z:y]------------------------
  sequence slice (set)*/
static int Matrix_ass_slice(MatrixObject * self, int begin, int end,
			     PyObject * seq)
{
	int i, x, y, size, sub_size = 0;
	float mat[16];
	PyObject *subseq;
	PyObject *m, *f;

	CLAMP(begin, 0, self->rowSize);
	CLAMP(end, 0, self->rowSize);
	begin = MIN2(begin,end);

	if(PySequence_Check(seq)){
		size = PySequence_Length(seq);
		if(size != (end - begin)){
			return EXPP_ReturnIntError(PyExc_TypeError,
				"matrix[begin:end] = []: size mismatch in slice assignment\n");
		}
		/*parse sub items*/
		for (i = 0; i < size; i++) {
			/*parse each sub sequence*/
			subseq = PySequence_GetItem(seq, i);
			if (subseq == NULL) { /*Failed to read sequence*/
				return EXPP_ReturnIntError(PyExc_RuntimeError, 
					"matrix[begin:end] = []: unable to read sequence\n");
			}

			if(PySequence_Check(subseq)){
				/*subsequence is also a sequence*/
				sub_size = PySequence_Length(subseq);
				if(sub_size != self->colSize){
					Py_DECREF(subseq);
					return EXPP_ReturnIntError(PyExc_TypeError,
						"matrix[begin:end] = []: size mismatch in slice assignment\n");
				}
				for (y = 0; y < sub_size; y++) {
					m = PySequence_GetItem(subseq, y);
					if (m == NULL) { /*Failed to read sequence*/
						Py_DECREF(subseq);
						return EXPP_ReturnIntError(PyExc_RuntimeError, 
							"matrix[begin:end] = []: unable to read sequence\n");
					}

					f = PyNumber_Float(m);
					if(f == NULL) { /*parsed item not a number*/
						EXPP_decr2(m, subseq);
						return EXPP_ReturnIntError(PyExc_TypeError, 
							"matrix[begin:end] = []: sequence argument not a number\n");
					}

					mat[(i * self->colSize) + y] = (float)PyFloat_AS_DOUBLE(f);
					EXPP_decr2(f, m);
				}
			}else{
				Py_DECREF(subseq);
				return EXPP_ReturnIntError(PyExc_TypeError,
					"matrix[begin:end] = []: illegal argument type for built-in operation\n");
			}
			Py_DECREF(subseq);
		}
		/*parsed well - now set in matrix*/
		for(x = 0; x < (size * sub_size); x++){
			self->matrix[begin + (int)floor(x / self->colSize)][x % self->colSize] = mat[x];
		}
		return 0;
	}else{
		return EXPP_ReturnIntError(PyExc_TypeError,
			"matrix[begin:end] = []: illegal argument type for built-in operation\n");
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

	if(mat1->coerced_object || mat2->coerced_object){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix addition: arguments not valid for this operation....\n");
	}
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix addition: matrices must have the same dimensions for this operation\n");
	}

	for(x = 0; x < mat1->rowSize; x++) {
		for(y = 0; y < mat1->colSize; y++) {
			mat[((x * mat1->colSize) + y)] = mat1->matrix[x][y] + mat2->matrix[x][y];
		}
	}

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW);
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

	if(mat1->coerced_object || mat2->coerced_object){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix addition: arguments not valid for this operation....\n");
	}
	if(mat1->rowSize != mat2->rowSize || mat1->colSize != mat2->colSize){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Matrix addition: matrices must have the same dimensions for this operation\n");
	}

	for(x = 0; x < mat1->rowSize; x++) {
		for(y = 0; y < mat1->colSize; y++) {
			mat[((x * mat1->colSize) + y)] = mat1->matrix[x][y] - mat2->matrix[x][y];
		}
	}

	return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW);
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
	PyObject *f = NULL;
	PointObject *pt = NULL;

	mat1 = (MatrixObject*)m1;
	mat2 = (MatrixObject*)m2;

	if(mat1->coerced_object){
		if (PyFloat_Check(mat1->coerced_object) || 
			PyInt_Check(mat1->coerced_object)){	/*FLOAT/INT * MATRIX*/
			f = PyNumber_Float(mat1->coerced_object);
			if(f == NULL) { /*parsed item not a number*/
				return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"Matrix multiplication: arguments not acceptable for this operation\n");
			}

			scalar = (float)PyFloat_AS_DOUBLE(f);
			Py_DECREF(f);
			for(x = 0; x < mat2->rowSize; x++) {
				for(y = 0; y < mat2->colSize; y++) {
					mat[((x * mat2->colSize) + y)] = scalar * mat2->matrix[x][y];
				}
			}
			return newMatrixObject(mat, mat2->rowSize, mat2->colSize, Py_NEW);
		}
	}else{
		if(mat2->coerced_object){
			/* MATRIX * VECTOR   operation is now being done by vector */
			/*if(VectorObject_Check(mat2->coerced_object)){ 
				vec = (VectorObject*)mat2->coerced_object;
				return column_vector_multiplication(mat1, vec);
			}else */
			if(PointObject_Check(mat2->coerced_object)){ /*MATRIX * POINT*/
				pt = (PointObject*)mat2->coerced_object;
				return column_point_multiplication(mat1, pt);
			}else if (PyFloat_Check(mat2->coerced_object) || 
				PyInt_Check(mat2->coerced_object)){	/*MATRIX * FLOAT/INT*/
				f = PyNumber_Float(mat2->coerced_object);
				if(f == NULL) { /*parsed item not a number*/
					return EXPP_ReturnPyObjError(PyExc_TypeError, 
						"Matrix multiplication: arguments not acceptable for this operation\n");
				}

				scalar = (float)PyFloat_AS_DOUBLE(f);
				Py_DECREF(f);
				for(x = 0; x < mat1->rowSize; x++) {
					for(y = 0; y < mat1->colSize; y++) {
						mat[((x * mat1->colSize) + y)] = scalar * mat1->matrix[x][y];
					}
				}
				return newMatrixObject(mat, mat1->rowSize, mat1->colSize, Py_NEW);
			}
		}else{  /*MATRIX * MATRIX*/
			if(mat1->colSize != mat2->rowSize){
				return EXPP_ReturnPyObjError(PyExc_AttributeError,
					"Matrix multiplication: matrix A rowsize must equal matrix B colsize\n");
			}
			for(x = 0; x < mat1->rowSize; x++) {
				for(y = 0; y < mat2->colSize; y++) {
					for(z = 0; z < mat1->colSize; z++) {
						dot += (mat1->matrix[x][z] * mat2->matrix[z][y]);
					}
					mat[((x * mat1->rowSize) + y)] = (float)dot;
					dot = 0.0f;
				}
			}
			return newMatrixObject(mat, mat1->rowSize, mat2->colSize, Py_NEW);
		}
	}

	return EXPP_ReturnPyObjError(PyExc_TypeError, 
		"Matrix multiplication: arguments not acceptable for this operation\n");
}
PyObject* Matrix_inv(MatrixObject *self)
{
	return Matrix_Invert(self);
}
/*------------------------coerce(obj, obj)-----------------------
  coercion of unknown types to type MatrixObject for numeric protocols.

  Coercion() is called whenever a math operation has 2 operands that
 it doesn't understand how to evaluate. 2+Matrix for example. We want to 
 evaluate some of these operations like: (vector * 2), however, for math
 to proceed, the unknown operand must be cast to a type that python math will
 understand. (e.g. in the case above case, 2 must be cast to a vector and 
 then call vector.multiply(vector, scalar_cast_as_vector)*/
static int Matrix_coerce(PyObject ** m1, PyObject ** m2)
{
	if(VectorObject_Check(*m2) || PyFloat_Check(*m2) || PyInt_Check(*m2) ||
			PointObject_Check(*m2)) {
		PyObject *coerced = EXPP_incr_ret(*m2);
		*m2 = newMatrixObject(NULL,3,3,Py_NEW);
		((MatrixObject*)*m2)->coerced_object = coerced;
		Py_INCREF (*m1);
		return 0;
	}

	return EXPP_ReturnIntError(PyExc_TypeError, 
		"matrix.coerce(): unknown operand - can't coerce for numeric protocols");
}
/*-----------------PROTOCOL DECLARATIONS--------------------------*/
static PySequenceMethods Matrix_SeqMethods = {
	(inquiry) Matrix_len,					/* sq_length */
	(binaryfunc) 0,							/* sq_concat */
	(intargfunc) 0,							/* sq_repeat */
	(intargfunc) Matrix_item,				/* sq_item */
	(intintargfunc) Matrix_slice,			/* sq_slice */
	(intobjargproc) Matrix_ass_item,		/* sq_ass_item */
	(intintobjargproc) Matrix_ass_slice,	/* sq_ass_slice */
};
static PyNumberMethods Matrix_NumMethods = {
	(binaryfunc) Matrix_add,				/* __add__ */
	(binaryfunc) Matrix_sub,				/* __sub__ */
	(binaryfunc) Matrix_mul,				/* __mul__ */
	(binaryfunc) 0,							/* __div__ */
	(binaryfunc) 0,							/* __mod__ */
	(binaryfunc) 0,							/* __divmod__ */
	(ternaryfunc) 0,						/* __pow__ */
	(unaryfunc) 0,							/* __neg__ */
	(unaryfunc) 0,							/* __pos__ */
	(unaryfunc) 0,							/* __abs__ */
	(inquiry) 0,							/* __nonzero__ */
	(unaryfunc) Matrix_inv,					/* __invert__ */
	(binaryfunc) 0,							/* __lshift__ */
	(binaryfunc) 0,							/* __rshift__ */
	(binaryfunc) 0,							/* __and__ */
	(binaryfunc) 0,							/* __xor__ */
	(binaryfunc) 0,							/* __or__ */
	(coercion) Matrix_coerce,				/* __coerce__ */
	(unaryfunc) 0,							/* __int__ */
	(unaryfunc) 0,							/* __long__ */
	(unaryfunc) 0,							/* __float__ */
	(unaryfunc) 0,							/* __oct__ */
	(unaryfunc) 0,							/* __hex__ */
};
/*------------------PY_OBECT DEFINITION--------------------------*/
PyTypeObject matrix_Type = {
	PyObject_HEAD_INIT(NULL)		/*tp_head*/
	0,								/*tp_internal*/
	"matrix",						/*tp_name*/
	sizeof(MatrixObject),			/*tp_basicsize*/
	0,								/*tp_itemsize*/
	(destructor)Matrix_dealloc,		/*tp_dealloc*/
	0,								/*tp_print*/
	(getattrfunc)Matrix_getattr,	/*tp_getattr*/
	(setattrfunc) Matrix_setattr,	/*tp_setattr*/
	0,								/*tp_compare*/
	(reprfunc) Matrix_repr,			/*tp_repr*/
	&Matrix_NumMethods,				/*tp_as_number*/
	&Matrix_SeqMethods,				/*tp_as_sequence*/
	0,								/*tp_as_mapping*/
	0,								/*tp_hash*/
	0,								/*tp_call*/
	0,								/*tp_str*/
	0,								/*tp_getattro*/
	0,								/*tp_setattro*/
	0,								/*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,				/*tp_flags*/
	MatrixObject_doc,				/*tp_doc*/
	0,								/*tp_traverse*/
	0,								/*tp_clear*/
	(richcmpfunc)Matrix_richcmpr,	/*tp_richcompare*/
	0,								/*tp_weaklistoffset*/
	0,								/*tp_iter*/
	0,								/*tp_iternext*/
	0,								/*tp_methods*/
	0,								/*tp_members*/
	0,								/*tp_getset*/
	0,								/*tp_base*/
	0,								/*tp_dict*/
	0,								/*tp_descr_get*/
	0,								/*tp_descr_set*/
	0,								/*tp_dictoffset*/
	0,								/*tp_init*/
	0,								/*tp_alloc*/
	0,								/*tp_new*/
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
self->matrix[1][1] = self->contiguous_ptr[4] = self->data.xxx_data[4]*/

/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newMatrixObject(float *mat, int rowSize, int colSize, int type)
{
	MatrixObject *self;
	int x, row, col;

	/*matrix objects can be any 2-4row x 2-4col matrix*/
	if(rowSize < 2 || rowSize > 4 || colSize < 2 || colSize > 4){
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
			"matrix(): row and column sizes must be between 2 and 4\n");
	}

	self = PyObject_NEW(MatrixObject, &matrix_Type);
	self->data.blend_data = NULL;
	self->data.py_data = NULL;
	self->rowSize = rowSize;
	self->colSize = colSize;
	self->coerced_object = NULL;

	if(type == Py_WRAP){
		self->data.blend_data = mat;
		self->contigPtr = self->data.blend_data;
		/*create pointer array*/
		self->matrix = PyMem_Malloc(rowSize * sizeof(float *));
		if(self->matrix == NULL) { /*allocation failure*/
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"matrix(): problem allocating pointer space\n");
		}
		/*pointer array points to contigous memory*/
		for(x = 0; x < rowSize; x++) {
			self->matrix[x] = self->contigPtr + (x * colSize);
		}
		self->wrapped = Py_WRAP;
	}else if (type == Py_NEW){
		self->data.py_data = PyMem_Malloc(rowSize * colSize * sizeof(float));
		if(self->data.py_data == NULL) { /*allocation failure*/
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"matrix(): problem allocating pointer space\n");
		}
		self->contigPtr = self->data.py_data;
		/*create pointer array*/
		self->matrix = PyMem_Malloc(rowSize * sizeof(float *));
		if(self->matrix == NULL) { /*allocation failure*/
			PyMem_Free(self->data.py_data);
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
				"matrix(): problem allocating pointer space\n");
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
