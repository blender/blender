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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Mathutils.h"

//***************************************************************************
// Function:					M_Mathutils_Rand															
//***************************************************************************
static PyObject *M_Mathutils_Rand(PyObject *self, PyObject *args)
{

	float high, low, range;
	double rand;
	high = 1.0;
	low = 0.0;

	if (!PyArg_ParseTuple(args, "|ff", &low, &high))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected optional float & float\n"));

	if ( (high < low) ||(high < 0 && low > 0))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"high value should be larger than low value\n"));

	//seed the generator
	BLI_srand((unsigned int) (PIL_check_seconds_timer()*0x7FFFFFFF));

	//get the random number 0 - 1
	rand = BLI_drand();

	//set it to range
	range = high - low;
	rand = rand * range;
	rand = rand + low;

	return PyFloat_FromDouble((double)rand);
}

//***************************************************************************
// Function:					M_Mathutils_Vector																				
// Python equivalent:			Blender.Mathutils.Vector	
// Supports 2D, 3D, and 4D vector objects both int and float values
// accepted. Mixed float and int values accepted. Ints are parsed to float																	
//***************************************************************************
static PyObject *M_Mathutils_Vector(PyObject *self, PyObject *args)
{
	PyObject *listObject = NULL;
	PyObject *checkOb = NULL;
	int x;
	float *vec;

	if (!PyArg_ParseTuple(args, "|O!", &PyList_Type, &listObject))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"0 or 1 list expected"));

	if(!listObject)	return (PyObject *)newVectorObject(NULL, 3);

	//2D 3D 4D supported
	if(PyList_Size(listObject) != 2 && PyList_Size(listObject) != 3 
			&& PyList_Size(listObject) != 4)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,	
			"2D, 3D and 4D vectors supported\n"));

	for (x = 0; x < PyList_Size(listObject); x++) {
		checkOb = PyList_GetItem(listObject, x);
		if(!PyInt_Check(checkOb) && !PyFloat_Check(checkOb))
			return (EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected list of numbers\n"));
	}

	//allocate memory
	vec = PyMem_Malloc (PyList_Size(listObject)*sizeof (float));

	//parse it all as floats
	for (x = 0; x < PyList_Size(listObject); x++) {
		if (!PyArg_Parse(PyList_GetItem(listObject, x), "f", &vec[x])){
			return EXPP_ReturnPyObjError (PyExc_TypeError, 
				"python list not parseable\n");
		}
	}
	return (PyObject *)newVectorObject(vec, PyList_Size(listObject));
}

//***************************************************************************
//Begin Vector Utils

static PyObject *M_Mathutils_CopyVec(PyObject *self, PyObject *args)
{
	VectorObject * vector;
	float *vec;
	int x;

	if (!PyArg_ParseTuple(args, "O!", &vector_Type, &vector))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected vector type\n"));

	vec = PyMem_Malloc(vector->size * sizeof(float));
	for(x = 0; x < vector->size; x++){
		vec[x] = vector->vec[x];
	}

	return (PyObject *)newVectorObject(vec, vector->size);
}

//finds perpendicular vector - only 3D is supported
static PyObject *M_Mathutils_CrossVecs(PyObject *self, PyObject *args)
{
	PyObject * vecCross;
	VectorObject * vec1;
	VectorObject * vec2;

	if (!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected 2 vector types\n"));
	if(vec1->size != 3 || vec2->size != 3)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"only 3D vectors are supported\n"));

	vecCross = newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);
	Crossf(((VectorObject*)vecCross)->vec, vec1->vec, vec2->vec);

	return vecCross;
}

static PyObject *M_Mathutils_DotVecs(PyObject *self, PyObject *args)
{
	VectorObject * vec1;
	VectorObject * vec2;
	float dot;
	int x;

	dot = 0;
	if (!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected vector types\n"));
	if(vec1->size != vec2->size)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
		  "vectors must be of the same size\n"));

	for(x = 0; x < vec1->size; x++){
		dot += vec1->vec[x] * vec2->vec[x];
	}

	return PyFloat_FromDouble((double)dot);
}

static PyObject *M_Mathutils_AngleBetweenVecs(PyObject *self, PyObject *args)
{
	VectorObject * vec1;
	VectorObject * vec2;
	float dot, angleRads, norm;
	int x;

	dot = 0;
	if (!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected 2 vector types\n"));
	if(vec1->size != vec2->size)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"vectors must be of the same size\n"));
	if(vec1->size > 3 || vec2->size > 3)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"only 2D,3D vectors are supported\n"));

	//normalize vec1
	norm = 0.0f;
	for(x = 0; x < vec1->size; x++){
		norm += vec1->vec[x] * vec1->vec[x];
	}
	norm = (float)sqrt(norm);
	for(x = 0; x < vec1->size; x++){
		vec1->vec[x] /= norm;
	}

	//normalize vec2
	norm = 0.0f;
	for(x = 0; x < vec2->size; x++){
		norm += vec2->vec[x] * vec2->vec[x];
	}
	norm = (float)sqrt(norm);
	for(x = 0; x < vec2->size; x++){
		vec2->vec[x] /= norm;
	}

	//dot product
	for(x = 0; x < vec1->size; x++){
		dot += vec1->vec[x] * vec2->vec[x];
	}

	//I believe saacos checks to see if the vectors are normalized
	angleRads = saacos(dot);

	return PyFloat_FromDouble((double)(angleRads*(180/Py_PI)));
}

static PyObject *M_Mathutils_MidpointVecs(PyObject *self, PyObject *args)
{
	
	VectorObject * vec1;
	VectorObject * vec2;
	float * vec;
	int x;

	if (!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected vector types\n"));
	if(vec1->size != vec2->size)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"vectors must be of the same size\n"));

	vec = PyMem_Malloc (vec1->size*sizeof (float));

	for(x = 0; x < vec1->size; x++){
		vec[x]= 0.5f*(vec1->vec[x] + vec2->vec[x]);
	}
	return (PyObject *)newVectorObject(vec, vec1->size);
}

//row vector multiplication
static PyObject *M_Mathutils_VecMultMat(PyObject *self, PyObject *args)
{
	PyObject * ob1 = NULL;
	PyObject * ob2 = NULL;
	MatrixObject * mat;
	VectorObject * vec;
	float * vecNew;
	int x, y;
	int z = 0;
	float dot = 0.0f;

	//get pyObjects
	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &ob1, &matrix_Type, &ob2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,	
				"vector and matrix object expected - in that order\n"));

	mat = (MatrixObject*)ob2; 
	vec = (VectorObject*)ob1;
	if(mat->colSize != vec->size)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,	
				"matrix col size and vector size must be the same\n"));

	vecNew = PyMem_Malloc (vec->size*sizeof (float));
	
	for(x = 0; x < mat->colSize; x++){
		for(y = 0; y < mat->rowSize; y++){
			dot += mat->matrix[y][x] * vec->vec[y];
		}
		vecNew[z] = dot; 
		z++; dot = 0;
	}

	return (PyObject *)newVectorObject(vecNew, vec->size);
}

static PyObject *M_Mathutils_ProjectVecs(PyObject *self, PyObject *args)
{
	VectorObject * vec1;
	VectorObject * vec2;
	float *vec;
	float dot = 0.0f;
	float dot2 = 0.0f;
	int x;

	if (!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected vector types\n"));
	if(vec1->size != vec2->size)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"vectors must be of the same size\n"));

	vec = PyMem_Malloc (vec1->size * sizeof (float));

	//dot of vec1 & vec2
	for(x = 0; x < vec1->size; x++){
		dot +=  vec1->vec[x] * vec2->vec[x];
	}
	//dot of vec2 & vec2
	for(x = 0; x < vec2->size; x++){
		dot2 +=  vec2->vec[x] * vec2->vec[x];
	}
	dot /= dot2; 
	for(x = 0; x < vec1->size; x++){
		vec[x] = dot * vec2->vec[x];
	}
	return (PyObject *)newVectorObject(vec, vec1->size);
}

//End Vector Utils

//***************************************************************************
// Function:					M_Mathutils_Matrix																				
// Python equivalent:			Blender.Mathutils.Matrix																		
//***************************************************************************
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static PyObject *M_Mathutils_Matrix(PyObject *self, PyObject *args)
{

	PyObject *rowA = NULL;
	PyObject *rowB = NULL;
	PyObject *rowC = NULL;
	PyObject *rowD = NULL;
	PyObject *checkOb = NULL;
	int x, rowSize, colSize;
	float * mat;
	int OK;

	if (!PyArg_ParseTuple(args, "|O!O!O!O!", &PyList_Type, &rowA,
		                                     &PyList_Type, &rowB,
											 &PyList_Type, &rowC,
											 &PyList_Type, &rowD)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected 0, 2,3 or 4 lists\n"));
	}

	if(!rowA)
		return newMatrixObject (NULL, 4, 4);
	
	if(!rowB)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected 0, 2,3 or 4 lists\n"));

	//get rowSize
	if(rowC){
		if(rowD){
			rowSize = 4;
		}else{
			rowSize = 3;
		}
	}else{
		rowSize = 2;
	}

	//check size and get colSize
	OK = 0;
	if((PyList_Size(rowA) == PyList_Size(rowB))){
		if(rowC){
			if((PyList_Size(rowA) == PyList_Size(rowC))){
				if(rowD){
					if((PyList_Size(rowA) == PyList_Size(rowD))){
						OK = 1;
					}
				} OK = 1;
			}
		}else OK = 1;
	}

	if(!OK)	return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"each row of vector must contain the same number of parameters\n");			
	colSize = PyList_Size(rowA);

	//check for numeric types
	for (x = 0; x < colSize; x++) {
		checkOb = PyList_GetItem(rowA, x);
		if(!PyInt_Check(checkOb) && !PyFloat_Check(checkOb))
			return (EXPP_ReturnPyObjError (PyExc_TypeError,
							"1st list - expected list of numbers\n"));
		checkOb = PyList_GetItem(rowB, x);
		if(!PyInt_Check(checkOb) && !PyFloat_Check(checkOb))
			return (EXPP_ReturnPyObjError (PyExc_TypeError,
							"2nd list - expected list of numbers\n"));
		if(rowC){
			checkOb = PyList_GetItem(rowC, x);
			if(!PyInt_Check(checkOb) && !PyFloat_Check(checkOb))
				return (EXPP_ReturnPyObjError (PyExc_TypeError,
								"3rd list - expected list of numbers\n"));
		}
		if(rowD){
			checkOb = PyList_GetItem(rowD, x);
			if(!PyInt_Check(checkOb) && !PyFloat_Check(checkOb))
				return (EXPP_ReturnPyObjError (PyExc_TypeError,
								"4th list - expected list of numbers\n"));
		}
	}

	//allocate space for 1D array
	mat = PyMem_Malloc (rowSize * colSize * sizeof (float));

	//parse rows
	for (x = 0; x < colSize; x++) {
		if (!PyArg_Parse(PyList_GetItem(rowA, x), "f", &mat[x]))
			return EXPP_ReturnPyObjError (PyExc_TypeError,	
			"rowA - python list not parseable\n");
	}
	for (x = 0; x < colSize; x++) {
		if (!PyArg_Parse(PyList_GetItem(rowB, x), "f", &mat[(colSize + x)]))
			return EXPP_ReturnPyObjError (PyExc_TypeError,	
			"rowB - python list not parseable\n");
	}
	if(rowC){
		for (x = 0; x < colSize; x++) {
			if (!PyArg_Parse(PyList_GetItem(rowC, x), "f", &mat[((2*colSize) + x)]))
				return EXPP_ReturnPyObjError (PyExc_TypeError,	
				"rowC - python list not parseable\n");
		}
	}
	if(rowD){
		for (x = 0; x < colSize; x++) {
			if (!PyArg_Parse(PyList_GetItem(rowD, x), "f", &mat[((3*colSize) + x)]))
				return EXPP_ReturnPyObjError (PyExc_TypeError,	
				"rowD - python list not parseable\n");
		}
	}

	//pass to matrix creation
	return newMatrixObject (mat, rowSize, colSize);
}

//***************************************************************************
// Function:					M_Mathutils_RotationMatrix																				
// Python equivalent:			Blender.Mathutils.RotationMatrix																		
//***************************************************************************
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static PyObject *M_Mathutils_RotationMatrix(PyObject *self, PyObject *args)
{

	float *mat;
	float angle = 0.0f;
	char *axis = NULL;
	VectorObject * vec = NULL;
	int matSize;
	float norm = 0.0f;
	float cosAngle = 0.0f;
	float sinAngle = 0.0f;

	if (!PyArg_ParseTuple(args, "fi|sO!",  &angle, &matSize, &axis, &vector_Type, &vec)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float int and optional string and vector\n"));
	}
	if(angle < -360.0f || angle > 360.0f)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"angle size not appropriate\n");
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"can only return a 2x2 3x3 or 4x4 matrix\n");
	if(matSize == 2 && (axis != NULL || vec != NULL))
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"cannot create a 2x2 rotation matrix around arbitrary axis\n");
	if((matSize == 3 || matSize == 4) && axis == NULL)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"please choose an axis of rotation\n");
	if(axis){
		if(((strcmp (axis, "r") == 0) ||
			(strcmp (axis, "R") == 0)) && vec == NULL)
	   		return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"please define the arbitrary axis of rotation\n");
	}
	if(vec){
		if(vec->size != 3)
	   		return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"the arbitrary axis must be a 3D vector\n");
	}

	mat = PyMem_Malloc(matSize * matSize * sizeof(float));

	//convert to radians
	angle = angle * (float)(Py_PI/180);

	if(axis == NULL && matSize == 2){
		//2D rotation matrix
		mat[0] = ((float)cos((double)(angle)));
		mat[1] = ((float)sin((double)(angle)));
		mat[2] = (-((float)sin((double)(angle))));
		mat[3] = ((float)cos((double)(angle)));
	}else if((strcmp(axis,"x") == 0) ||
		(strcmp(axis,"X") == 0)){
		//rotation around X
		mat[0] = 1.0f; mat[1] = 0.0f; mat[2] = 0.0f; mat[3] = 0.0f;
		mat[4] = ((float)cos((double)(angle)));
		mat[5] = ((float)sin((double)(angle)));
		mat[6] = 0.0f;
		mat[7] = (-((float)sin((double)(angle))));
		mat[8] = ((float)cos((double)(angle)));
	}else if ((strcmp(axis,"y") == 0) ||
		      (strcmp(axis,"Y") == 0)){
		//rotation around Y
		mat[0] = ((float)cos((double)(angle)));
		mat[1] = 0.0f;
		mat[2] = (-((float)sin((double)(angle))));
		mat[3] = 0.0f;	mat[4] = 1.0f;	mat[5] = 0.0f;
		mat[6] = ((float)sin((double)(angle)));
		mat[7] = 0.0f;
		mat[8] = ((float)cos((double)(angle)));
	}else if ((strcmp(axis,"z") == 0) ||
		      (strcmp(axis,"Z") == 0)){
		//rotation around Z
		mat[0] = ((float)cos((double)(angle)));
		mat[1] = ((float)sin((double)(angle)));
		mat[2] = 0.0f;
		mat[3] = (-((float)sin((double)(angle))));
		mat[4] = ((float)cos((double)(angle)));
		mat[5] = 0.0f; mat[6] = 0.0f; mat[7] = 0.0f; mat[8] = 1.0f;
	}else if ((strcmp(axis,"r") == 0) ||
		      (strcmp(axis,"R") == 0)){
		//arbitrary rotation
		//normalize arbitrary axis
		norm = (float)sqrt(vec->vec[0] * vec->vec[0] +  vec->vec[1] * vec->vec[1] +
						   vec->vec[2] * vec->vec[2]);
		vec->vec[0] /= norm; vec->vec[1] /= norm; vec->vec[2] /= norm;

		//create matrix
		cosAngle = ((float)cos((double)(angle)));
		sinAngle = ((float)sin((double)(angle)));
		mat[0] = ((vec->vec[0] * vec->vec[0]) * (1 - cosAngle)) +
				 cosAngle;
		mat[1] = ((vec->vec[0] * vec->vec[1]) * (1 - cosAngle)) +
				 (vec->vec[2] * sinAngle);
		mat[2] = ((vec->vec[0] * vec->vec[2]) * (1 - cosAngle)) -
				 (vec->vec[1] * sinAngle);
		mat[3] = ((vec->vec[0] * vec->vec[1]) * (1 - cosAngle)) -
				 (vec->vec[2] * sinAngle);
		mat[4] = ((vec->vec[1] * vec->vec[1]) * (1 - cosAngle)) +
				 cosAngle;
		mat[5] = ((vec->vec[1] * vec->vec[2]) * (1 - cosAngle)) +
				 (vec->vec[0] * sinAngle);
		mat[6] = ((vec->vec[0] * vec->vec[2]) * (1 - cosAngle)) +
				 (vec->vec[1] * sinAngle);
		mat[7] = ((vec->vec[1] * vec->vec[2]) * (1 - cosAngle)) -
				 (vec->vec[0] * sinAngle);
		mat[8] = ((vec->vec[2] * vec->vec[2]) * (1 - cosAngle)) +
				 cosAngle;
	}else{
	   	return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"unrecognizable axis of rotation type - expected x,y,z or r\n");
	}
	if(matSize == 4){
		//resize matrix
		mat[15] = 1.0f;	mat[14] = 0.0f;
		mat[13] = 0.0f;	mat[12] = 0.0f;
		mat[11] = 0.0f;	mat[10] = mat[8];
		mat[9] = mat[7]; mat[8] = mat[6];
		mat[7] = 0.0f;	mat[6] = mat[5];
		mat[5] = mat[4];mat[4] = mat[3];
		mat[3] = 0.0f;
	}

	//pass to matrix creation
	return newMatrixObject (mat, matSize, matSize);
}

//***************************************************************************
// Function:					M_Mathutils_TranslationMatrix																				
// Python equivalent:			Blender.Mathutils.TranslationMatrix																		
//***************************************************************************
static PyObject *M_Mathutils_TranslationMatrix(PyObject *self, PyObject *args)
{
	VectorObject *vec;
	float *mat;

    if (!PyArg_ParseTuple(args, "O!",  &vector_Type, &vec)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected vector\n"));
	}
	if(vec->size != 3 && vec->size != 4){
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"vector must be 3D or 4D\n");
	}

	mat = PyMem_Malloc(4*4*sizeof(float));
	Mat4One((float(*)[4])mat);

	mat[12] = vec->vec[0];
	mat[13] = vec->vec[1];
	mat[14] = vec->vec[2];

	return newMatrixObject(mat, 4,4);
}


//***************************************************************************
// Function:					M_Mathutils_ScaleMatrix																				
// Python equivalent:			Blender.Mathutils.ScaleMatrix																		
//***************************************************************************
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static PyObject *M_Mathutils_ScaleMatrix(PyObject *self, PyObject *args)
{
	float factor;
	int matSize;
	VectorObject *vec = NULL;
	float *mat;
	float norm = 0.0f;
	int x;

	if (!PyArg_ParseTuple(args, "fi|O!",  &factor, &matSize, &vector_Type, &vec)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected float int and optional vector\n"));
	}
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"can only return a 2x2 3x3 or 4x4 matrix\n");
	if(vec){
		if(vec->size > 2 && matSize == 2)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"please use 2D vectors when scaling in 2D\n");
	}
	mat = PyMem_Malloc(matSize * matSize * sizeof(float));

	if(vec == NULL){ //scaling along axis
		if(matSize == 2){
			mat[0] = factor;
			mat[1] = 0.0f;	mat[2] = 0.0f;
			mat[3] = factor;
		}else {
			mat[0] = factor;
			mat[1] = 0.0f; mat[2] = 0.0f; mat[3] = 0.0f;
			mat[4] = factor;
			mat[5] = 0.0f; mat[6] = 0.0f; mat[7] = 0.0f;
			mat[8] = factor;
		}
	}else{ //scaling in arbitrary direction

		//normalize arbitrary axis
		for(x = 0; x < vec->size; x++){
			norm += vec->vec[x] * vec->vec[x];
		}
		norm = (float)sqrt(norm);
		for(x = 0; x < vec->size; x++){
			vec->vec[x] /= norm;
		}
		if(matSize ==2){
			mat[0] = 1 + ((factor - 1) * (vec->vec[0] * vec->vec[0]));
			mat[1] = ((factor - 1) * (vec->vec[0] * vec->vec[1]));
			mat[2] = ((factor - 1) * (vec->vec[0] * vec->vec[1]));
			mat[3] = 1 + ((factor - 1) * (vec->vec[1] * vec->vec[1]));
		}else{
			mat[0] = 1 + ((factor - 1) * (vec->vec[0] * vec->vec[0]));
			mat[1] = ((factor - 1) * (vec->vec[0] * vec->vec[1]));
			mat[2] = ((factor - 1) * (vec->vec[0] * vec->vec[2]));
			mat[3] = ((factor - 1) * (vec->vec[0] * vec->vec[1]));
			mat[4] = 1 + ((factor - 1) * (vec->vec[1] * vec->vec[1]));
			mat[5] = ((factor - 1) * (vec->vec[1] * vec->vec[2]));
			mat[6] = ((factor - 1) * (vec->vec[0] * vec->vec[2]));
			mat[7] = ((factor - 1) * (vec->vec[1] * vec->vec[2]));
			mat[8] = 1 + ((factor - 1) * (vec->vec[2] * vec->vec[2]));
		}
	}
	if(matSize == 4){
		//resize matrix
		mat[15] = 1.0f;	mat[14] = 0.0f;	mat[13] = 0.0f;
		mat[12] = 0.0f;	mat[11] = 0.0f;
		mat[10] = mat[8]; mat[9] = mat[7];
		mat[8] = mat[6]; mat[7] = 0.0f;
		mat[6] = mat[5]; mat[5] = mat[4];
		mat[4] = mat[3]; mat[3] = 0.0f;
	}

	//pass to matrix creation
	return newMatrixObject (mat, matSize, matSize);
}

//***************************************************************************
// Function:					M_Mathutils_OrthoProjectionMatrix																				
// Python equivalent:			Blender.Mathutils.OrthoProjectionMatrix																		
//***************************************************************************
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
static PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject *self, PyObject *args)
{
	char *plane;
	int matSize;
	float *mat;
	VectorObject *vec = NULL;
	float norm = 0.0f;
	int x;

	if (!PyArg_ParseTuple(args, "si|O!",  &plane, &matSize, &vector_Type, &vec)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string and int and optional vector\n"));
	}
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"can only return a 2x2 3x3 or 4x4 matrix\n");
	if(vec){
		if(vec->size > 2 && matSize == 2)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"please use 2D vectors when scaling in 2D\n");
	}
	if(vec == NULL){ //ortho projection onto cardinal plane
		if (((strcmp(plane, "x") == 0) || (strcmp(plane, "X") == 0)) &&
			matSize == 2){
			mat = PyMem_Malloc(matSize * matSize * sizeof(float));
			mat[0] = 1.0f;
			mat[1] = 0.0f;	mat[2] = 0.0f;	mat[3] = 0.0f;
		}else if(((strcmp(plane, "y") == 0) || (strcmp(plane, "Y") == 0)) &&
			matSize == 2){
			mat = PyMem_Malloc(matSize * matSize * sizeof(float));
			mat[0] = 0.0f;	mat[1] = 0.0f;	mat[2] = 0.0f;
			mat[3] = 1.0f;
		}else if(((strcmp(plane, "xy") == 0) || (strcmp(plane, "XY") == 0)) &&
			matSize > 2){
			mat = PyMem_Malloc(matSize * matSize * sizeof(float));
			mat[0] = 1.0f;
			mat[1] = 0.0f;	mat[2] = 0.0f;	mat[3] = 0.0f;
			mat[4] = 1.0f;
			mat[5] = 0.0f;	mat[6] = 0.0f;	mat[7] = 0.0f;	mat[8] = 0.0f;
		}else if(((strcmp(plane, "xz") == 0) || (strcmp(plane, "XZ") == 0)) &&
			matSize > 2){
			mat = PyMem_Malloc(matSize * matSize * sizeof(float));
			mat[0] = 1.0f;
			mat[1] = 0.0f;	mat[2] = 0.0f;	mat[3] = 0.0f;	mat[4] = 0.0f;
			mat[5] = 0.0f;	mat[6] = 0.0f;	mat[7] = 0.0f;
			mat[8] = 1.0f;
		}else if(((strcmp(plane, "yz") == 0) || (strcmp(plane, "YZ") == 0)) &&
			matSize > 2){
			mat = PyMem_Malloc(matSize * matSize * sizeof(float));
			mat[0] = 0.0f;	mat[1] = 0.0f;	mat[2] = 0.0f;	mat[3] = 0.0f;
			mat[4] = 1.0f;
			mat[5] = 0.0f;	mat[6] = 0.0f;	mat[7] = 0.0f;
			mat[8] = 1.0f;
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"unknown plane - expected: x, y, xy, xz, yz\n");
		}
	}else{ //arbitrary plane
		//normalize arbitrary axis
		for(x = 0; x < vec->size; x++){
			norm += vec->vec[x] * vec->vec[x];
		}
		norm = (float)sqrt(norm);

		for(x = 0; x < vec->size; x++){
			vec->vec[x] /= norm;
		}

		if (((strcmp(plane, "r") == 0) || (strcmp(plane, "R") == 0)) &&
			matSize == 2){
				mat = PyMem_Malloc(matSize * matSize * sizeof(float));
				mat[0] = 1 - (vec->vec[0] * vec->vec[0]);
				mat[1] = - (vec->vec[0] * vec->vec[1]);
				mat[2] = - (vec->vec[0] * vec->vec[1]);
				mat[3] = 1 - (vec->vec[1] * vec->vec[1]);
		}else if (((strcmp(plane, "r") == 0) || (strcmp(plane, "R") == 0)) &&
			matSize > 2){
				mat = PyMem_Malloc(matSize * matSize * sizeof(float));
				mat[0] = 1 - (vec->vec[0] * vec->vec[0]);
				mat[1] = - (vec->vec[0] * vec->vec[1]);
				mat[2] = - (vec->vec[0] * vec->vec[2]);
				mat[3] = - (vec->vec[0] * vec->vec[1]);
				mat[4] = 1 - (vec->vec[1] * vec->vec[1]);
				mat[5] = - (vec->vec[1] * vec->vec[2]);
				mat[6] = - (vec->vec[0] * vec->vec[2]);
				mat[7] = - (vec->vec[1] * vec->vec[2]);
				mat[8] = 1 - (vec->vec[2] * vec->vec[2]);
		}else{
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"unknown plane - expected: 'r' expected for axis designation\n");
		}
	}

	if(matSize == 4){
		//resize matrix
		mat[15] = 1.0f;	mat[14] = 0.0f;
		mat[13] = 0.0f;	mat[12] = 0.0f;
		mat[11] = 0.0f;	mat[10] = mat[8];
		mat[9] = mat[7];mat[8] = mat[6];
		mat[7] = 0.0f;	mat[6] = mat[5];
		mat[5] = mat[4];mat[4] = mat[3];
		mat[3] = 0.0f;
	}

	//pass to matrix creation
	return newMatrixObject (mat, matSize, matSize);
}

//***************************************************************************
// Function:					M_Mathutils_ShearMatrix																				
// Python equivalent:			Blender.Mathutils.ShearMatrix																		
//***************************************************************************
static PyObject *M_Mathutils_ShearMatrix(PyObject *self, PyObject *args)
{
	float factor;
	int matSize;
	char *plane;
	float *mat;

	if (!PyArg_ParseTuple(args, "sfi",  &plane, &factor, &matSize)){
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string float and int\n"));
	}

	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"can only return a 2x2 3x3 or 4x4 matrix\n");

	if (((strcmp(plane, "x") == 0) || (strcmp(plane, "X") == 0)) &&
		matSize == 2){
		mat = PyMem_Malloc(matSize * matSize * sizeof(float));
		mat[0] = 1.0f;	 mat[1] = 0.0f;	
		mat[2] = factor; mat[3] = 1.0f;
	}else if(((strcmp(plane, "y") == 0) || (strcmp(plane, "Y") == 0)) &&
		matSize == 2){
		mat = PyMem_Malloc(matSize * matSize * sizeof(float));
		mat[0] = 1.0f;	mat[1] = factor;	
		mat[2] = 0.0f;	mat[3] = 1.0f;
	}else if(((strcmp(plane, "xy") == 0) || (strcmp(plane, "XY") == 0)) &&
		matSize > 2){
		mat = PyMem_Malloc(matSize * matSize * sizeof(float));
		mat[0] = 1.0f;	mat[1] = 0.0f;	mat[2] = 0.0f;	mat[3] = 0.0f;
		mat[4] = 1.0f;	mat[5] = 0.0f;	
		mat[6] = factor; mat[7] = factor; mat[8] = 0.0f;
	}else if(((strcmp(plane, "xz") == 0) || (strcmp(plane, "XZ") == 0)) &&
		matSize > 2){
		mat = PyMem_Malloc(matSize * matSize * sizeof(float));
		mat[0] = 1.0f;	mat[1] = 0.0f;	mat[2] = 0.0f;	
		mat[3] = factor; mat[4] = 1.0f; mat[5] = factor;
		mat[6] = 0.0f;	mat[7] = 0.0f;	mat[8] = 1.0f;
	}else if(((strcmp(plane, "yz") == 0) || (strcmp(plane, "YZ") == 0)) &&
		matSize > 2){
		mat = PyMem_Malloc(matSize * matSize * sizeof(float));
		mat[0] = 1.0f;	mat[1] = factor; mat[2] = factor;
		mat[3] = 0.0f;	mat[4] = 1.0f;
		mat[5] = 0.0f;	mat[6] = 0.0f;	mat[7] = 0.0f;
		mat[8] = 1.0f;
	}else{
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"expected: x, y, xy, xz, yz or wrong matrix size for shearing plane\n");
	}

	if(matSize == 4){
		//resize matrix
		mat[15] = 1.0f;	mat[14] = 0.0f;
		mat[13] = 0.0f;	mat[12] = 0.0f;
		mat[11] = 0.0f;	mat[10] = mat[8];
		mat[9] = mat[7];mat[8] = mat[6];
		mat[7] = 0.0f;	mat[6] = mat[5];
		mat[5] = mat[4];mat[4] = mat[3];
		mat[3] = 0.0f;
	}

	//pass to matrix creation
	return newMatrixObject (mat, matSize, matSize);
}

//***************************************************************************
//Begin Matrix Utils

static PyObject *M_Mathutils_CopyMat(PyObject *self, PyObject *args)
{
	MatrixObject *matrix;
	float *mat;
	int x,y,z;

	if(!PyArg_ParseTuple(args, "O!", &matrix_Type, &matrix))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,	
				"expected matrix\n"));

	mat = PyMem_Malloc(matrix->rowSize * matrix->colSize * sizeof(float));

	z = 0;
	for(x = 0; x < matrix->rowSize; x++){
		for(y = 0; y < matrix->colSize; y++){
			mat[z] = matrix->matrix[x][y];
			z++;
		}
	}

	return (PyObject*)newMatrixObject (mat, matrix->rowSize, matrix->colSize);
}
static PyObject *M_Mathutils_MatMultVec(PyObject *self, PyObject *args)
{

	PyObject * ob1 = NULL;
	PyObject * ob2 = NULL;
	MatrixObject * mat;
	VectorObject * vec;
	float * vecNew;
	int x, y;
	int z = 0;
	float dot = 0.0f;

	//get pyObjects
	if(!PyArg_ParseTuple(args, "O!O!", &matrix_Type, &ob1, &vector_Type, &ob2))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,	
				"matrix and vector object expected - in that order\n"));

	mat = (MatrixObject*)ob1;
	vec = (VectorObject*)ob2;

	if(mat->rowSize != vec->size)
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,	
				"matrix row size and vector size must be the same\n"));

	vecNew = PyMem_Malloc (vec->size*sizeof (float));
	
	for(x = 0; x < mat->rowSize; x++){
		for(y = 0; y < mat->colSize; y++){
			dot += mat->matrix[x][y] * vec->vec[y];
		}
		vecNew[z] = dot;
		z++;
		dot = 0;
	}

	return (PyObject *)newVectorObject(vecNew, vec->size);
}

//***************************************************************************
// Function:					M_Mathutils_Quaternion																				
// Python equivalent:			Blender.Mathutils.Quaternion																		
//***************************************************************************
static PyObject *M_Mathutils_Quaternion(PyObject *self, PyObject *args)
{
	PyObject *listObject;
	float *vec;
	float *quat;
	float angle = 0.0f;
	int x;
	float norm;

	if (!PyArg_ParseTuple(args, "O!|f", &PyList_Type, &listObject, &angle))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected list and optional float\n"));

	if(PyList_Size(listObject) != 4 && PyList_Size(listObject) != 3)
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"3 or 4 expected floats for the quaternion\n"));

	vec = PyMem_Malloc (PyList_Size(listObject)*sizeof (float));
	for (x = 0; x < PyList_Size(listObject); x++) {
		if (!PyArg_Parse(PyList_GetItem(listObject, x), "f", &vec[x]))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
							"python list not parseable\n");		
	}

	if(PyList_Size(listObject) == 3){ //an axis of rotation
		norm = (float)sqrt(vec[0] * vec[0] + vec[1] * vec[1] +
						   vec[2] * vec[2]);

		vec[0] /= norm;	vec[1] /= norm;	vec[2] /= norm;

		angle = angle * (float)(Py_PI/180);
		quat = PyMem_Malloc(4*sizeof(float));
		quat[0] = (float)(cos((double)(angle)/2));
		quat[1] = (float)(sin((double)(angle)/2)) * vec[0];
		quat[2] = (float)(sin((double)(angle)/2)) * vec[1];
		quat[3] = (float)(sin((double)(angle)/2)) * vec[2];

		PyMem_Free(vec);

		return newQuaternionObject(quat);
	}else
		return newQuaternionObject(vec); 
}

//***************************************************************************
//Begin Quaternion Utils

static PyObject *M_Mathutils_CopyQuat(PyObject *self, PyObject *args)
{
	QuaternionObject * quatU;
	float * quat;

	if (!PyArg_ParseTuple(args, "O!", &quaternion_Type, &quatU))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Quaternion type"));

	quat = PyMem_Malloc (4*sizeof(float));
	quat[0] = quatU->quat[0];
	quat[1] = quatU->quat[1];
	quat[2] = quatU->quat[2];
	quat[3] = quatU->quat[3];

	return (PyObject*)newQuaternionObject(quat);
}

static PyObject *M_Mathutils_CrossQuats(PyObject *self, PyObject *args)
{
	QuaternionObject * quatU;
	QuaternionObject * quatV;
	float * quat;

	if (!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, &quatU, 
		&quaternion_Type, &quatV))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Quaternion types"));
	quat = PyMem_Malloc (4*sizeof(float));
	QuatMul(quat, quatU->quat, quatV->quat);

	return (PyObject*)newQuaternionObject(quat);
}

static PyObject *M_Mathutils_DotQuats(PyObject *self, PyObject *args)
{
	QuaternionObject * quatU;
	QuaternionObject * quatV;
	float * quat;
	int x;
	float dot = 0.0f;

	if (!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, &quatU, 
		&quaternion_Type, &quatV))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Quaternion types"));

	quat = PyMem_Malloc (4*sizeof(float));
	for(x = 0; x < 4; x++){
		dot += quatU->quat[x] * quatV->quat[x];
	}

	return PyFloat_FromDouble((double)(dot));
}

static PyObject *M_Mathutils_DifferenceQuats(PyObject *self, PyObject *args)
{
	QuaternionObject * quatU;
	QuaternionObject * quatV;
	float * quat;
	float * tempQuat;
	int x;
	float dot = 0.0f;

	if (!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, 
		&quatU, &quaternion_Type, &quatV))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Quaternion types"));

	quat = PyMem_Malloc (4*sizeof(float));
	tempQuat = PyMem_Malloc (4*sizeof(float));

	tempQuat[0] = quatU->quat[0];
	tempQuat[1] = -quatU->quat[1];
	tempQuat[2] = -quatU->quat[2];
	tempQuat[3] = -quatU->quat[3];

	dot= (float)sqrt((double)tempQuat[0] * (double)tempQuat[0] +
				(double)tempQuat[1] * (double)tempQuat[1] +
				(double)tempQuat[2] * (double)tempQuat[2] +
				(double)tempQuat[3] * (double)tempQuat[3]);

	for(x = 0; x < 4; x++){
		tempQuat[x] /= (dot * dot);
	}
	QuatMul(quat, tempQuat, quatV->quat);

	return (PyObject*)newQuaternionObject(quat);
}

static PyObject *M_Mathutils_Slerp(PyObject *self, PyObject *args)
{
	QuaternionObject * quatU;
	QuaternionObject * quatV;
	float * quat;
	float param, x,y, cosD, sinD, deltaD, IsinD, val;
	int flag, z;

	if (!PyArg_ParseTuple(args, "O!O!f", &quaternion_Type, 
		&quatU, &quaternion_Type, &quatV, &param))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Quaternion types and float"));

	quat = PyMem_Malloc (4*sizeof(float));
	
	cosD = quatU->quat[0] * quatV->quat[0] + 
		    quatU->quat[1] * quatV->quat[1] + 
			quatU->quat[2] * quatV->quat[2] + 
			quatU->quat[3] * quatV->quat[3]; 

	flag = 0;
	if(cosD< 0.0f){
		flag = 1;
		cosD = -cosD;
	}
	if(cosD > .99999f){
		x = 1.0f - param;
		y = param;
	}else{
		sinD = (float)sqrt(1.0f - cosD * cosD);
		deltaD = (float)atan2(sinD, cosD);
		IsinD = 1.0f/sinD;
		x = (float)sin((1.0f - param) * deltaD) * IsinD;
		y = (float)sin(param * deltaD) * IsinD;
	}
	for(z = 0; z < 4; z++){
		val = quatV->quat[z];
		if(val) val = -val;
		quat[z] = (quatU->quat[z] * x) + (val * y);
	}
	return (PyObject*)newQuaternionObject(quat);
}

//***************************************************************************
// Function:					M_Mathutils_Euler																			
// Python equivalent:			Blender.Mathutils.Euler																		
//***************************************************************************
static PyObject *M_Mathutils_Euler(PyObject *self, PyObject *args)
{
	PyObject *listObject;
	float *vec;
	int x;

	if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &listObject))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected list\n"));

	if(PyList_Size(listObject) != 3)
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"only 3d eulers are supported\n");

	vec = PyMem_Malloc (3*sizeof (float));
	for (x = 0; x < 3; x++) {
		if (!PyArg_Parse(PyList_GetItem(listObject, x), "f", &vec[x]))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
							"python list not parseable\n");		
	}

	return (PyObject*)newEulerObject(vec);
}


//***************************************************************************
//Begin Euler Util

  static PyObject *M_Mathutils_CopyEuler(PyObject *self, PyObject *args)
{
	EulerObject * eulU;
	float * eul;

	if (!PyArg_ParseTuple(args, "O!", &euler_Type, &eulU))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Euler types"));

	eul = PyMem_Malloc (3*sizeof(float));
	eul[0] = eulU->eul[0];
	eul[1] = eulU->eul[1];
	eul[2] = eulU->eul[2];

	return (PyObject*)newEulerObject(eul);
}

static PyObject *M_Mathutils_RotateEuler(PyObject *self, PyObject *args)
{
	EulerObject * Eul;
	float angle;
	char *axis;
	int x;

	if (!PyArg_ParseTuple(args, "O!fs", &euler_Type, &Eul, &angle, &axis))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,	
		"expected euler type & float & string"));

	angle *= (float)(Py_PI/180);
	for(x = 0; x < 3; x++){
		Eul->eul[x] *= (float)(Py_PI/180);
	}
	euler_rot(Eul->eul, angle, *axis);
	for(x = 0; x < 3; x++){
		Eul->eul[x] *= (float)(180/Py_PI);
	}

	return EXPP_incr_ret(Py_None);
}

//***************************************************************************
// Function:					Mathutils_Init																					
//***************************************************************************
PyObject *Mathutils_Init (void)
{
	PyObject *mod= Py_InitModule3("Blender.Mathutils", M_Mathutils_methods, M_Mathutils_doc);
	return(mod);
}
