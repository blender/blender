/*
 *
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
 * Contributor(s): Michel Selten & Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "matrix.h"

//doc strings
char Matrix_Zero_doc[] =
"() - set all values in the matrix to 0";
char Matrix_Identity_doc[] =
"() - set the square matrix to it's identity matrix";
char Matrix_Transpose_doc[] =
"() - set the matrix to it's transpose";
char Matrix_Determinant_doc[] =
"() - return the determinant of the matrix";
char Matrix_Invert_doc[] =
"() - set the matrix to it's inverse if an inverse is possible";
char Matrix_TranslationPart_doc[] = 
"() - return a vector encompassing the translation of the matrix";
char Matrix_RotationPart_doc[] = 
"() - return a vector encompassing the rotation of the matrix";
char Matrix_Resize4x4_doc[] = 
"() - resize the matrix to a 4x4 square matrix";
char Matrix_toEuler_doc[] = 
"() - convert matrix to a euler angle rotation";
char Matrix_toQuat_doc[] = 
"() - convert matrix to a quaternion rotation";

//methods table
struct PyMethodDef Matrix_methods[] = {
	{"zero",(PyCFunction)Matrix_Zero, METH_NOARGS,
				Matrix_Zero_doc},
	{"identity",(PyCFunction)Matrix_Identity, METH_NOARGS,
				Matrix_Identity_doc},
	{"transpose",(PyCFunction)Matrix_Transpose, METH_NOARGS,
				Matrix_Transpose_doc},
	{"determinant",(PyCFunction)Matrix_Determinant, METH_NOARGS,
				Matrix_Determinant_doc},
	{"invert",(PyCFunction)Matrix_Invert, METH_NOARGS,
				Matrix_Invert_doc},
	{"translationPart",(PyCFunction)Matrix_TranslationPart, METH_NOARGS,
				Matrix_TranslationPart_doc},
	{"rotationPart",(PyCFunction)Matrix_RotationPart, METH_NOARGS,
				Matrix_RotationPart_doc},
	{"resize4x4",(PyCFunction)Matrix_Resize4x4, METH_NOARGS,
				Matrix_Resize4x4_doc},
	{"toEuler",(PyCFunction)Matrix_toEuler, METH_NOARGS,
				Matrix_toEuler_doc},
	{"toQuat",(PyCFunction)Matrix_toQuat, METH_NOARGS,
				Matrix_toQuat_doc},	
	{NULL, NULL, 0, NULL}
};

/*****************************/
//    Matrix Python Object   
/*****************************/

PyObject *Matrix_toQuat(MatrixObject *self)
{
	float *quat, *mat;

	if(self->colSize < 3){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "inappropriate matrix size\n");
	}else if (self->colSize > 2){	//3 or 4 col
		if(self->rowSize < 3)		//3 or 4 row
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			  "inappropriate matrix size\n");

		mat = PyMem_Malloc(3*3*sizeof(float));
		mat[0] = self->matrix[0][0]; mat[1] = self->matrix[0][1];
		mat[2] = self->matrix[0][2]; mat[3] = self->matrix[1][0];
		mat[4] = self->matrix[1][1]; mat[5] = self->matrix[1][2];
		mat[6] = self->matrix[2][0]; mat[7] = self->matrix[2][1];
		mat[8] = self->matrix[2][2];
	}
	quat = PyMem_Malloc(4*sizeof(float));
	Mat3ToQuat((float(*)[3])mat,quat);

	return (PyObject*)newQuaternionObject(quat);
}


PyObject *Matrix_toEuler(MatrixObject *self)
{
	float *eul, *mat;
	int x;

	if(self->colSize < 3){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "inappropriate matrix size\n");
	}else if (self->colSize > 2){	//3 or 4 col
		if(self->rowSize < 3)		//3 or 4 row
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			  "inappropriate matrix size\n");

		mat = PyMem_Malloc(3*3*sizeof(float));
		mat[0] = self->matrix[0][0]; mat[1] = self->matrix[0][1];
		mat[2] = self->matrix[0][2]; mat[3] = self->matrix[1][0];
		mat[4] = self->matrix[1][1]; mat[5] = self->matrix[1][2];
		mat[6] = self->matrix[2][0]; mat[7] = self->matrix[2][1];
		mat[8] = self->matrix[2][2];
	}
	eul = PyMem_Malloc(3*sizeof(float));
	Mat3ToEul((float(*)[3])mat,eul);

	for(x = 0; x < 3; x++){
		eul[x] *= (float)(180/Py_PI);
	}

	return (PyObject*)newEulerObject(eul);
}

PyObject *Matrix_Resize4x4(MatrixObject *self)
{
	float *mat;
	float * contigPtr;
	int x, row, col;

	if(self->colSize == 4 && self->rowSize == 4)
		return EXPP_incr_ret(Py_None);

	mat = PyMem_Malloc(4*4*sizeof(float));
	for(x = 0; x < 16; x++){
		mat[x] = 0.0f;
	}

	if(self->colSize == 2){ //2x2, 2x3, 2x4
			mat[0] = self->matrix[0][0];mat[1] = self->matrix[0][1];
			mat[4] = self->matrix[1][0];mat[5] = self->matrix[1][1];
		if (self->rowSize > 2){
			mat[8] = self->matrix[2][0];mat[9] = self->matrix[2][1];
		}
		if (self->rowSize > 3){
			mat[12] = self->matrix[3][0];mat[13] = self->matrix[3][1];
		}
		mat[10] = 1.0f; mat[15] = 1.0f;
	}else if (self->colSize == 3){  //3x2, 3x3, 3x4
			mat[0] = self->matrix[0][0];mat[1] = self->matrix[0][1];
			mat[2] = self->matrix[0][2];mat[4] = self->matrix[1][0];
			mat[5] = self->matrix[1][1];mat[6] = self->matrix[1][2];
		if (self->rowSize > 2){
			mat[8] = self->matrix[2][0];mat[9] = self->matrix[2][1];
			mat[10] = self->matrix[2][2];
		}
		if (self->rowSize > 3){
			mat[12] = self->matrix[3][0];mat[13] = self->matrix[3][1];
			mat[14] = self->matrix[3][2];
		}
		if(self->rowSize == 2) mat[10] = 1.0f;
		mat[15] = 1.0f;	
	}else if (self->colSize == 4){  //2x4, 3x4
			mat[0] = self->matrix[0][0];mat[1] = self->matrix[0][1];
			mat[2] = self->matrix[0][2];mat[3] = self->matrix[0][3];
			mat[4] = self->matrix[1][0];mat[5] = self->matrix[1][1];
			mat[6] = self->matrix[1][2];mat[7] = self->matrix[1][3];
		if (self->rowSize > 2
			){
			mat[8] = self->matrix[2][0];mat[9] = self->matrix[2][1];
			mat[10] = self->matrix[2][2];mat[11] = self->matrix[2][3];
		}
		if(self->rowSize == 2) mat[10] = 1.0f;
		mat[15] = 1.0f;
	}

	PyMem_Free(*self->matrix);
	contigPtr = PyMem_Malloc(4 * 4 * sizeof(float));
	if(contigPtr == NULL){
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				"problem allocating array space\n\n"));
	}
	self->matrix = PyMem_Malloc(4* sizeof(float*));
	if(self->matrix == NULL){
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				"problem allocating pointer space\n\n"));
	}
    for (x = 0; x < 4; x++){
        self->matrix[x] = contigPtr + (x *4);
    }

	for (row = 0; row < 4; row++){
		for(col = 0; col < 4; col++){
			self->matrix[row][col] = mat[(row * 4) + col];
		}
	}
	PyMem_Free(mat);

	self->colSize = 4;
	self->rowSize = 4;

	return EXPP_incr_ret(Py_None);
}

PyObject *Matrix_TranslationPart(MatrixObject *self)
{
	float *vec;

	if(self->colSize < 3){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "inappropriate matrix size\n");
	}else if (self->colSize > 2){    //3 or 4 columns
		if(self->rowSize < 4)		//all 4 rows
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			  "inappropriate matrix size\n");

		vec = PyMem_Malloc(3 * sizeof(float));
		vec[0] = self->matrix[3][0];
		vec[1] = self->matrix[3][1];
		vec[2] = self->matrix[3][2];
	}

	return (PyObject*)newVectorObject(vec,3);
}

PyObject *Matrix_RotationPart(MatrixObject *self)
{
	float *mat;

	if(self->colSize < 3){
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "inappropriate matrix size\n");
	}else if (self->colSize > 2){	//3 or 4 col
		if(self->rowSize < 3)		//3 or 4 row
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
			  "inappropriate matrix size\n");

		mat = PyMem_Malloc(3*3*sizeof(float));
		mat[0] = self->matrix[0][0]; mat[1] = self->matrix[0][1];
		mat[2] = self->matrix[0][2]; mat[3] = self->matrix[1][0];
		mat[4] = self->matrix[1][1]; mat[5] = self->matrix[1][2];
		mat[6] = self->matrix[2][0]; mat[7] = self->matrix[2][1];
		mat[8] = self->matrix[2][2];
	}

	return (PyObject*)newMatrixObject(mat,3,3);
}

PyObject *Matrix_Invert(MatrixObject *self)
{
	float det;
	int x,y,z;
	float *mat = NULL;
	float t;

	if(self->rowSize != self->colSize)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "only square matrices are supported\n");

	//calculate the determinant
	if(self->rowSize == 2){
		det =  Det2x2(self->matrix[0][0], self->matrix[0][1],
					  self->matrix[1][0], self->matrix[1][1]);
	}else if(self->rowSize == 3){
		det =  Det3x3(self->matrix[0][0], self->matrix[0][1],
					  self->matrix[0][2], self->matrix[1][0],
					  self->matrix[1][1], self->matrix[1][2],
					  self->matrix[2][0], self->matrix[2][1],
					  self->matrix[2][2]);
	}else if(self->rowSize == 4){
		det =  Det4x4(*self->matrix);
	}else{
		return EXPP_ReturnPyObjError(PyExc_StandardError,
		  "error calculating determinant for inverse()\n");
	}

	if(det != 0){

		//calculate the classical adjoint
		if(self->rowSize == 2){
			mat = PyMem_Malloc(self->rowSize * self->colSize * sizeof(float));
			mat[0] = self->matrix[1][1];
			mat[1] = -self->matrix[1][0];
			mat[2] = -self->matrix[0][1];
			mat[3] = self->matrix[0][0];
		}else if(self->rowSize == 3){
			mat = PyMem_Malloc(self->rowSize * self->colSize * sizeof(float));
			Mat3Adj((float(*)[3])mat, *self->matrix);
		}else if (self->rowSize == 4){
			mat = PyMem_Malloc(self->rowSize * self->colSize * sizeof(float));
			Mat4Adj((float(*)[4])mat, *self->matrix);
		}

		//divide by determinate
		for(x = 0; x < (self->rowSize * self->colSize); x++){
			mat[x] /= det;
		}

		//set values
		z = 0;
		for(x = 0; x < self->rowSize; x++){
			for(y = 0; y < self->colSize; y++){
				self->matrix[x][y] = mat[z];
				z++;
			}
		}

		//transpose
		if(self->rowSize == 2){
			t = self->matrix[1][0];
			self->matrix[1][0] = self->matrix[0][1];
			self->matrix[0][1] = t;
		}else if(self->rowSize == 3){
			Mat3Transp((float(*)[3])mat);
		}
		else if (self->rowSize == 4){
			Mat4Transp((float(*)[4])mat);
		}
	}else{
		printf("matrix does not have an inverse - none attempted\n");
	}

	return EXPP_incr_ret(Py_None);
}


PyObject *Matrix_Determinant(MatrixObject *self)
{
	float det;

	if(self->rowSize != self->colSize)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "only square matrices are supported\n");

	if(self->rowSize == 2){
		det =  Det2x2(self->matrix[0][0], self->matrix[0][1],
					  self->matrix[1][0], self->matrix[1][1]);
	}else if(self->rowSize == 3){
		det =  Det3x3(self->matrix[0][0], self->matrix[0][1],
					  self->matrix[0][2], self->matrix[1][0],
					  self->matrix[1][1], self->matrix[1][2],
					  self->matrix[2][0], self->matrix[2][1],
					  self->matrix[2][2]);
	}else if(self->rowSize == 4){
		det =  Det4x4(*self->matrix);
	}else{
		return EXPP_ReturnPyObjError(PyExc_StandardError,
		  "error in determinant()\n");
	}
	return PyFloat_FromDouble(det);
}

PyObject *Matrix_Transpose(MatrixObject *self)
{
	float t;

	if(self->rowSize != self->colSize)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
		  "only square matrices are supported\n");

	if(self->rowSize == 2){
		t = self->matrix[1][0];
		self->matrix[1][0] = self->matrix[0][1];
		self->matrix[0][1] = t;
	}
	else if(self->rowSize == 3){
		Mat3Transp(*self->matrix);
	}
	else if (self->rowSize == 4){
		Mat4Transp(*self->matrix);
	}
	else
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"unable to transpose matrix\n"));

	return EXPP_incr_ret(Py_None);
}

PyObject *Matrix_Zero(MatrixObject *self)
{
	int row, col;

	for (row = 0; row < self->rowSize; row++){
		for (col = 0; col < self->colSize; col++){
			self->matrix[row][col] = 0.0f;
		}
	}
	return EXPP_incr_ret(Py_None);
}

PyObject *Matrix_Identity(MatrixObject *self)
{
	if(self->rowSize != self->colSize)
		return (EXPP_ReturnPyObjError(PyExc_AttributeError,
			"only square matrices supported\n"));

	if(self->rowSize == 2){
		self->matrix[0][0] = 1.0f;
		self->matrix[0][1] = 0.0f;
		self->matrix[1][0] = 0.0f;
		self->matrix[1][1] = 1.0f;
	}
	else if(self->rowSize == 3){
		Mat3One(*self->matrix);
	}
	else if (self->rowSize == 4){
		Mat4One(*self->matrix);
	}
	else
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"unable to create identity matrix\n"));

	return EXPP_incr_ret(Py_None);
}

static void Matrix_dealloc (MatrixObject *self)
{
	PyMem_Free (self->matrix);
    PyMem_DEL (self);
}

static PyObject * Matrix_getattr (MatrixObject *self, char *name)
{
    if (strcmp (name, "rowSize") == 0){
		return PyInt_FromLong((long)self->rowSize);
    }
    else if (strcmp (name, "colSize") == 0){
		return PyInt_FromLong((long)self->colSize);
    }

	return Py_FindMethod(Matrix_methods, (PyObject*)self, name);
}

static int Matrix_setattr (MatrixObject *self, char *name, PyObject *v)
{
    /* This is not supported. */
    return (-1);
}

static PyObject * Matrix_repr (MatrixObject *self)
{
	PyObject *repr, *str;
	int x,y;
	char ftoa[24];

	repr = PyString_FromString("");
	if (!repr) 
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
                "Attribute error in PyMatrix (repr)\n"));

	for(x = 0; x < self->rowSize; x++){
		str = PyString_FromString ("[");
		PyString_ConcatAndDel(&repr,str);

		for(y = 0; y < (self->colSize - 1); y++){
			sprintf(ftoa, "%.4f, ", self->matrix[x][y]);
			str = PyString_FromString (ftoa);
			PyString_ConcatAndDel(&repr,str);
		}
		sprintf(ftoa, "%.4f]\n", self->matrix[x][y]);
		str = PyString_FromString (ftoa);
		PyString_ConcatAndDel(&repr,str);
	}
	return repr;
}

//no support for matrix[x][y] so have to return by sequence index
//will return a row from the matrix to support previous API
//compatability
static PyObject * Matrix_item (MatrixObject *self, int i)
{
	float *vec;
	int x;

	if(i < 0 || i >= self->rowSize)
		return EXPP_ReturnPyObjError(PyExc_IndexError,
			"matrix row index out of range\n");

	vec = PyMem_Malloc (self->colSize *sizeof (float));
	for(x = 0; x < self->colSize; x++){
		vec[x] = self->matrix[i][x];
	}

	return (PyObject*)newVectorObject(vec, self->colSize);
}

static PyObject *Matrix_slice(MatrixObject *self, int begin, int end)
{
	PyObject *list;
	int count, maxsize, x, y;
  
	maxsize = self->colSize * self->rowSize;
	if (begin < 0) begin= 0;
	if (end > maxsize) end= maxsize;
	if (begin > end) begin= end;

	list= PyList_New(end-begin);

	for (count = begin; count < end; count++){
		x = (int)floor((double)(count / self->colSize));
		y = count % self->colSize;
		PyList_SetItem(list, count-begin, PyFloat_FromDouble(self->matrix[x][y]));
	}

	return list;
}

static int Matrix_ass_item(MatrixObject *self, int i, PyObject *ob)
{
	int maxsize, x, y;

	maxsize = self->colSize * self->rowSize;
	if (i < 0 || i >= maxsize)
		return EXPP_ReturnIntError(PyExc_IndexError,
					"array assignment index out of range\n");
	if (!PyInt_Check(ob) && !PyFloat_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
					"matrix member must be a number\n");

	x = (int)floor((double)(i / self->colSize));
	y = i % self->colSize;
	self->matrix[x][y] = (float)PyFloat_AsDouble(ob);

	return 0;
}

static int Matrix_ass_slice(MatrixObject *self, int begin, int end, PyObject *seq)
{
	int count, maxsize, x, y, z;
	
	maxsize = self->colSize * self->rowSize;
	if (begin < 0) begin= 0;
	if (end > maxsize) end= maxsize;
	if (begin > end) begin= end;

	if (!PySequence_Check(seq))
		return EXPP_ReturnIntError(PyExc_TypeError,
					"illegal argument type for built-in operation\n");
	if (PySequence_Length(seq) != (end - begin))
		return EXPP_ReturnIntError(PyExc_TypeError,
					"size mismatch in slice assignment\n");

	z = 0;
	for (count = begin; count < end; count++) {
		PyObject *ob = PySequence_GetItem(seq, z); z++;
		if (!PyInt_Check(ob) && !PyFloat_Check(ob))
			return EXPP_ReturnIntError(PyExc_IndexError,
						"list member must be a number\n");

		x = (int)floor((double)(count / self->colSize));
		y = count % self->colSize;
		if (!PyArg_Parse(ob, "f", &self->matrix[x][y])){
			Py_DECREF(ob);
			return -1;
		}
	}
  return 0;
}

static int Matrix_len(MatrixObject *self) 
{
	return (self->colSize * self->rowSize);
}

PyObject * Matrix_add(PyObject *m1, PyObject * m2)
{
	float * mat;
	int matSize, rowSize, colSize, x,y;

	if((!Matrix_CheckPyObject(m1)) || (!Matrix_CheckPyObject(m2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");

	if(((MatrixObject*)m1)->flag > 0 || ((MatrixObject*)m2)->flag > 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"cannot add scalar to a matrix\n");
	
	if(((MatrixObject*)m1)->rowSize!= ((MatrixObject*)m2)->rowSize ||
		((MatrixObject*)m1)->colSize != ((MatrixObject*)m2)->colSize)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"matrices must be the same same for this operation\n");

	rowSize = (((MatrixObject*)m1)->rowSize);
	colSize = (((MatrixObject*)m1)->colSize);
	matSize = rowSize * colSize;
	
	mat = PyMem_Malloc (matSize * sizeof(float));

	for(x = 0; x < rowSize; x++){
		for(y = 0; y < colSize; y++){
			mat[((x * rowSize) + y)] = 
				((MatrixObject*)m1)->matrix[x][y] +
				((MatrixObject*)m2)->matrix[x][y];
		}
	}

	return newMatrixObject(mat, rowSize, colSize);
}

PyObject * Matrix_sub(PyObject *m1, PyObject * m2)
{
	float * mat;
	int matSize, rowSize, colSize, x,y;

	if((!Matrix_CheckPyObject(m1)) || (!Matrix_CheckPyObject(m2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");

	if(((MatrixObject*)m1)->flag > 0 || ((MatrixObject*)m2)->flag > 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"cannot subtract a scalar from a matrix\n");

	if(((MatrixObject*)m1)->rowSize!= ((MatrixObject*)m2)->rowSize ||
		((MatrixObject*)m1)->colSize != ((MatrixObject*)m2)->colSize)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"matrices must be the same same for this operation\n");

	rowSize = (((MatrixObject*)m1)->rowSize);
	colSize = (((MatrixObject*)m1)->colSize);
	matSize = rowSize * colSize;
	
	mat = PyMem_Malloc (matSize * sizeof(float));

	for(x = 0; x < rowSize; x++){
		for(y = 0; y < colSize; y++){
			mat[((x * rowSize) + y)] = 
				((MatrixObject*)m1)->matrix[x][y] -
				((MatrixObject*)m2)->matrix[x][y];
		}
	}

	return newMatrixObject(mat, rowSize, colSize);
}

PyObject * Matrix_mul(PyObject *m1, PyObject * m2)
{
	float * mat;
	int matSizeV, rowSizeV, colSizeV, rowSizeW, colSizeW, matSizeW, x, y, z;
	float dot = 0;
	MatrixObject * matV;
	MatrixObject * matW;

	if((!Matrix_CheckPyObject(m1)) || (!Matrix_CheckPyObject(m2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");

	//get some vars
	rowSizeV = (((MatrixObject*)m1)->rowSize);
	colSizeV = (((MatrixObject*)m1)->colSize);
	matSizeV = rowSizeV * colSizeV;
	rowSizeW = (((MatrixObject*)m2)->rowSize);
	colSizeW = (((MatrixObject*)m2)->colSize);
	matSizeW = rowSizeW * colSizeW;
	matV = ((MatrixObject*)m1);
	matW = ((MatrixObject*)m2);

	//coerced int or float for scalar multiplication
	if(matW->flag > 1 || matW->flag > 2){

		if(rowSizeV != rowSizeW &&	colSizeV != colSizeW)
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
					"Matrix dimension error during scalar multiplication\n");
		
		mat = PyMem_Malloc (matSizeV * sizeof(float));

		for(x = 0; x < rowSizeV; x++){
			for(y = 0; y < colSizeV; y++){
				mat[((x * rowSizeV) + y)] = 
					matV->matrix[x][y] * matW->matrix[x][y];
			}
		}
		return newMatrixObject(mat, rowSizeV, colSizeV);
	}
	else if (matW->flag == 0 && matV->flag == 0){		//true matrix multiplication
		if(colSizeV != rowSizeW){
			return EXPP_ReturnPyObjError (PyExc_AttributeError,
					"Matrix multiplication undefined...\n");
		}

		mat = PyMem_Malloc((rowSizeV * colSizeW) * sizeof(float));

		for(x = 0; x < rowSizeV; x++){
			for(y = 0; y < colSizeW; y++){
				for(z = 0; z < colSizeV; z++){
					dot += (matV->matrix[x][z] * matW->matrix[z][y]);
				}
				mat[((x * rowSizeV) + y)] = dot;
				dot = 0;
			}
		}
		return newMatrixObject(mat, rowSizeV, colSizeW);
	}
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
					"Error in matrix_mul...\n");
}

//coercion of unknown types to type MatrixObject for numeric protocols
int Matrix_coerce(PyObject **m1, PyObject **m2)
{ 
	long *tempI;
	double *tempF;
	float *mat;
	int x, matSize;

	matSize = (((MatrixObject*)*m1)->rowSize) * (((MatrixObject*)*m1)->rowSize);
	if (Matrix_CheckPyObject(*m1)) {
		if (Matrix_CheckPyObject(*m2)) { //matrix & matrix
			Py_INCREF(*m1);
			Py_INCREF(*m2);
			return 0;
		}else{
			if(VectorObject_Check(*m2)){ //matrix & vector?
				printf("use MatMultVec() for column vector multiplication\n");
				Py_INCREF(*m1);
				return 0;
			}else if(PyNumber_Check(*m2)){ //& scalar?
				if(PyInt_Check(*m2)){ //it's a int
					tempI = PyMem_Malloc(1*sizeof(long));
					*tempI = PyInt_AsLong(*m2);
					mat = PyMem_Malloc (matSize * sizeof (float));
					for(x = 0; x < matSize; x++){
						mat[x] = (float)*tempI;
					}
					PyMem_Free(tempI);
					*m2 = newMatrixObject(mat, (((MatrixObject*)*m1)->rowSize),
						(((MatrixObject*)*m1)->colSize));
					((MatrixObject*)*m2)->flag = 1;	//int coercion
					PyMem_Free(mat);
					Py_INCREF(*m1);
					return 0;
				}else if(PyFloat_Check(*m2)){ //it's a float
					tempF = PyMem_Malloc(1*sizeof(double));
					*tempF = PyFloat_AsDouble(*m2);
					mat = PyMem_Malloc (matSize * sizeof (float));
					for(x = 0; x < matSize; x++){
						mat[x] = (float)*tempF;
					}
					PyMem_Free(tempF);
					*m2 = newMatrixObject(mat, (((MatrixObject*)*m1)->rowSize),
						(((MatrixObject*)*m1)->colSize));
					((MatrixObject*)*m2)->flag = 2;	//float coercion
					PyMem_Free(mat);
					Py_INCREF(*m1);
					return 0;
				}
			}
			//unknom2n type or numeric cast failure
			printf("attempting matrix operation m2ith unsupported type...\n");
			Py_INCREF(*m1);
			return 0; //operation m2ill type check
		}
	}else{
		//1st not Matrix
		printf("numeric protocol failure...\n");
		return -1; //this should not occur - fail
	}
	return -1; 
}

//******************************************************************
//					Matrix definition
//******************************************************************
static PySequenceMethods Matrix_SeqMethods =
{
	(inquiry)          Matrix_len,            /* sq_length */
	(binaryfunc)       0,                     /* sq_concat */
	(intargfunc)       0,                     /* sq_repeat */
	(intargfunc)       Matrix_item,           /* sq_item */
	(intintargfunc)    Matrix_slice,          /* sq_slice */
	(intobjargproc)    Matrix_ass_item,       /* sq_ass_item */
	(intintobjargproc) Matrix_ass_slice,      /* sq_ass_slice */
};

static PyNumberMethods Matrix_NumMethods =
{
    (binaryfunc)	Matrix_add,               /* __add__ */
    (binaryfunc)	Matrix_sub,               /* __sub__ */
    (binaryfunc)	Matrix_mul,               /* __mul__ */
    (binaryfunc)	0,       		          /* __div__ */
    (binaryfunc)	0,				          /* __mod__ */
    (binaryfunc)	0,                        /* __divmod__ */
    (ternaryfunc)	0,                        /* __pow__ */
    (unaryfunc)		0,                        /* __neg__ */
    (unaryfunc)		0,                        /* __pos__ */
    (unaryfunc)		0,                        /* __abs__ */
    (inquiry)		0,                        /* __nonzero__ */
    (unaryfunc)		0,                        /* __invert__ */
    (binaryfunc)	0,                        /* __lshift__ */
    (binaryfunc)	0,                        /* __rshift__ */
    (binaryfunc)	0,                        /* __and__ */
    (binaryfunc)	0,                        /* __xor__ */
    (binaryfunc)	0,                        /* __or__ */
    (coercion)		Matrix_coerce,			  /* __coerce__ */
    (unaryfunc)		0,                        /* __int__ */
    (unaryfunc)		0,                        /* __long__ */
    (unaryfunc)		0,                        /* __float__ */
    (unaryfunc)		0,                        /* __oct__ */
    (unaryfunc)		0,                        /* __hex__ */
};

PyTypeObject matrix_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "Matrix",                       /*tp_name*/
    sizeof(MatrixObject),           /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    (destructor)    Matrix_dealloc, /*tp_dealloc*/
    (printfunc)     0,              /*tp_print*/
    (getattrfunc)   Matrix_getattr, /*tp_getattr*/
    (setattrfunc)   Matrix_setattr, /*tp_setattr*/
    0,                              /*tp_compare*/
    (reprfunc)      Matrix_repr,    /*tp_repr*/
    &Matrix_NumMethods,             /*tp_as_number*/
    &Matrix_SeqMethods,             /*tp_as_sequence*/
};

//******************************************************************
//Function:				 newMatrixObject
//******************************************************************
PyObject * newMatrixObject (float * mat, int rowSize, int colSize)
{
    MatrixObject    * self;
	float * contigPtr;
	int row, col, x;

	if (rowSize < 2 || rowSize > 4 || colSize < 2 || colSize > 4)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                "row and column sizes must be between 2 and 4\n"));

    self = PyObject_NEW (MatrixObject, &matrix_Type);

	//generate contigous memory space
	contigPtr = PyMem_Malloc(rowSize * colSize* sizeof(float));
	if(contigPtr == NULL){
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				"problem allocating array space\n\n"));
	}

	//create pointer array
	self->matrix = PyMem_Malloc(rowSize * sizeof(float*));
	if(self->matrix == NULL){
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				"problem allocating pointer space\n\n"));
	}

	//pointer array points to contigous memory
    for (x = 0; x < rowSize; x++){
        self->matrix[x] = contigPtr + (x * colSize);
    }

	if(mat){	//if a float array passed
		for (row = 0; row < rowSize; row++){
			for(col = 0; col < colSize; col++){
				self->matrix[row][col] = mat[(row * colSize) + col];
			}
		}
	}else{		//or if NULL passed
		for (row = 0; row < rowSize; row++){
			for (col = 0; col < colSize; col++){
				self->matrix[row][col] = 0.0f;
			}
		}
	}

	//set size vars of matrix
	self->rowSize = rowSize;
	self->colSize = colSize;

	//set coercion flag
	self->flag = 0;

    return ((PyObject *)self);
}


