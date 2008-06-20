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
 * Contributor(s): Joseph Gilbert, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Mathutils.h"

#include "BLI_arithb.h"
#include "PIL_time.h"
#include "BLI_rand.h"
#include "BKE_utildefines.h"

#include "gen_utils.h"

//-------------------------DOC STRINGS ---------------------------
static char M_Mathutils_doc[] = "The Blender Mathutils module\n\n";
static char M_Mathutils_Vector_doc[] = "() - create a new vector object from a list of floats";
static char M_Mathutils_Matrix_doc[] = "() - create a new matrix object from a list of floats";
static char M_Mathutils_Quaternion_doc[] = "() - create a quaternion from a list or an axis of rotation and an angle";
static char M_Mathutils_Euler_doc[] = "() - create and return a new euler object";
static char M_Mathutils_Rand_doc[] = "() - return a random number";
static char M_Mathutils_CrossVecs_doc[] = "() - returns a vector perpedicular to the 2 vectors crossed";
static char M_Mathutils_CopyVec_doc[] = "() - create a copy of vector";
static char M_Mathutils_DotVecs_doc[] = "() - return the dot product of two vectors";
static char M_Mathutils_AngleBetweenVecs_doc[] = "() - returns the angle between two vectors in degrees";
static char M_Mathutils_MidpointVecs_doc[] = "() - return the vector to the midpoint between two vectors";
static char M_Mathutils_MatMultVec_doc[] = "() - multiplies a matrix by a column vector";
static char M_Mathutils_VecMultMat_doc[] = "() - multiplies a row vector by a matrix";
static char M_Mathutils_ProjectVecs_doc[] =	"() - returns the projection vector from the projection of vecA onto vecB";
static char M_Mathutils_RotationMatrix_doc[] = "() - construct a rotation matrix from an angle and axis of rotation";
static char M_Mathutils_ScaleMatrix_doc[] =	"() - construct a scaling matrix from a scaling factor";
static char M_Mathutils_OrthoProjectionMatrix_doc[] = "() - construct a orthographic projection matrix from a selected plane";
static char M_Mathutils_ShearMatrix_doc[] = "() - construct a shearing matrix from a plane of shear and a shear factor";
static char M_Mathutils_CopyMat_doc[] = "() - create a copy of a matrix";
static char M_Mathutils_TranslationMatrix_doc[] = "(vec) - create a translation matrix from a vector";
static char M_Mathutils_CopyQuat_doc[] = "() - copy quatB to quatA";
static char M_Mathutils_CopyEuler_doc[] = "() - copy eulB to eultA";
static char M_Mathutils_CrossQuats_doc[] = "() - return the mutliplication of two quaternions";
static char M_Mathutils_DotQuats_doc[] = "() - return the dot product of two quaternions";
static char M_Mathutils_Slerp_doc[] = "() - returns the interpolation between two quaternions";
static char M_Mathutils_DifferenceQuats_doc[] = "() - return the angular displacment difference between two quats";
static char M_Mathutils_RotateEuler_doc[] = "() - rotate euler by an axis and angle";
static char M_Mathutils_Intersect_doc[] = "(v1, v2, v3, ray, orig, clip=1) - returns the intersection between a ray and a triangle, if possible, returns None otherwise";
static char M_Mathutils_TriangleArea_doc[] = "(v1, v2, v3) - returns the area size of the 2D or 3D triangle defined";
static char M_Mathutils_TriangleNormal_doc[] = "(v1, v2, v3) - returns the normal of the 3D triangle defined";
static char M_Mathutils_QuadNormal_doc[] = "(v1, v2, v3, v4) - returns the normal of the 3D quad defined";
static char M_Mathutils_LineIntersect_doc[] = "(v1, v2, v3, v4) - returns a tuple with the points on each line respectively closest to the other";
static char M_Mathutils_Point_doc[] = "Creates a 2d or 3d point object";
//-----------------------METHOD DEFINITIONS ----------------------
struct PyMethodDef M_Mathutils_methods[] = {
	{"Rand", (PyCFunction) M_Mathutils_Rand, METH_VARARGS, M_Mathutils_Rand_doc},
	{"Vector", (PyCFunction) M_Mathutils_Vector, METH_VARARGS, M_Mathutils_Vector_doc},
	{"CrossVecs", (PyCFunction) M_Mathutils_CrossVecs, METH_VARARGS, M_Mathutils_CrossVecs_doc},
	{"DotVecs", (PyCFunction) M_Mathutils_DotVecs, METH_VARARGS, M_Mathutils_DotVecs_doc},
	{"AngleBetweenVecs", (PyCFunction) M_Mathutils_AngleBetweenVecs, METH_VARARGS, M_Mathutils_AngleBetweenVecs_doc},
	{"MidpointVecs", (PyCFunction) M_Mathutils_MidpointVecs, METH_VARARGS, M_Mathutils_MidpointVecs_doc},
	{"VecMultMat", (PyCFunction) M_Mathutils_VecMultMat, METH_VARARGS, M_Mathutils_VecMultMat_doc},
	{"ProjectVecs", (PyCFunction) M_Mathutils_ProjectVecs, METH_VARARGS, M_Mathutils_ProjectVecs_doc},
	{"CopyVec", (PyCFunction) M_Mathutils_CopyVec, METH_VARARGS, M_Mathutils_CopyVec_doc},
	{"Matrix", (PyCFunction) M_Mathutils_Matrix, METH_VARARGS, M_Mathutils_Matrix_doc},
	{"RotationMatrix", (PyCFunction) M_Mathutils_RotationMatrix, METH_VARARGS, M_Mathutils_RotationMatrix_doc},
	{"ScaleMatrix", (PyCFunction) M_Mathutils_ScaleMatrix, METH_VARARGS, M_Mathutils_ScaleMatrix_doc},
	{"ShearMatrix", (PyCFunction) M_Mathutils_ShearMatrix, METH_VARARGS, M_Mathutils_ShearMatrix_doc},
	{"TranslationMatrix", (PyCFunction) M_Mathutils_TranslationMatrix, METH_O, M_Mathutils_TranslationMatrix_doc},
	{"CopyMat", (PyCFunction) M_Mathutils_CopyMat, METH_VARARGS, M_Mathutils_CopyMat_doc},
	{"OrthoProjectionMatrix", (PyCFunction) M_Mathutils_OrthoProjectionMatrix,  METH_VARARGS, M_Mathutils_OrthoProjectionMatrix_doc},
	{"MatMultVec", (PyCFunction) M_Mathutils_MatMultVec, METH_VARARGS, M_Mathutils_MatMultVec_doc},
	{"Quaternion", (PyCFunction) M_Mathutils_Quaternion, METH_VARARGS, M_Mathutils_Quaternion_doc},
	{"CopyQuat", (PyCFunction) M_Mathutils_CopyQuat, METH_VARARGS, M_Mathutils_CopyQuat_doc},
	{"CrossQuats", (PyCFunction) M_Mathutils_CrossQuats, METH_VARARGS, M_Mathutils_CrossQuats_doc},
	{"DotQuats", (PyCFunction) M_Mathutils_DotQuats, METH_VARARGS, M_Mathutils_DotQuats_doc},
	{"DifferenceQuats", (PyCFunction) M_Mathutils_DifferenceQuats, METH_VARARGS,M_Mathutils_DifferenceQuats_doc},
	{"Slerp", (PyCFunction) M_Mathutils_Slerp, METH_VARARGS, M_Mathutils_Slerp_doc},
	{"Euler", (PyCFunction) M_Mathutils_Euler, METH_VARARGS, M_Mathutils_Euler_doc},
	{"CopyEuler", (PyCFunction) M_Mathutils_CopyEuler, METH_VARARGS, M_Mathutils_CopyEuler_doc},
	{"RotateEuler", (PyCFunction) M_Mathutils_RotateEuler, METH_VARARGS, M_Mathutils_RotateEuler_doc},
	{"Intersect", ( PyCFunction ) M_Mathutils_Intersect, METH_VARARGS, M_Mathutils_Intersect_doc},
	{"TriangleArea", ( PyCFunction ) M_Mathutils_TriangleArea, METH_VARARGS, M_Mathutils_TriangleArea_doc},
	{"TriangleNormal", ( PyCFunction ) M_Mathutils_TriangleNormal, METH_VARARGS, M_Mathutils_TriangleNormal_doc},
	{"QuadNormal", ( PyCFunction ) M_Mathutils_QuadNormal, METH_VARARGS, M_Mathutils_QuadNormal_doc},
	{"LineIntersect", ( PyCFunction ) M_Mathutils_LineIntersect, METH_VARARGS, M_Mathutils_LineIntersect_doc},
	{"Point", (PyCFunction) M_Mathutils_Point, METH_VARARGS, M_Mathutils_Point_doc},
	{NULL, NULL, 0, NULL}
};
//----------------------------MODULE INIT-------------------------
PyObject *Mathutils_Init(void)
{
	PyObject *submodule;

	//seed the generator for the rand function
	BLI_srand((unsigned int) (PIL_check_seconds_timer() *
				      0x7FFFFFFF));
	
	/* needed for getseters */
	if( PyType_Ready( &vector_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &matrix_Type ) < 0 )
		return NULL;	
	if( PyType_Ready( &euler_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &quaternion_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3("Blender.Mathutils",
				    M_Mathutils_methods, M_Mathutils_doc);
	return (submodule);
}
//-----------------------------METHODS----------------------------
//----------------column_vector_multiplication (internal)---------
//COLUMN VECTOR Multiplication (Matrix X Vector)
// [1][2][3]   [a]
// [4][5][6] * [b]
// [7][8][9]   [c]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
PyObject *column_vector_multiplication(MatrixObject * mat, VectorObject* vec)
{
	float vecNew[4], vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0;

	if(mat->rowSize != vec->size){
		if(mat->rowSize == 4 && vec->size != 3){
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"matrix * vector: matrix row size and vector size must be the same");
		}else{
			vecCopy[3] = 1.0f;
		}
	}

	for(x = 0; x < vec->size; x++){
		vecCopy[x] = vec->vec[x];
		}

	for(x = 0; x < mat->rowSize; x++) {
		for(y = 0; y < mat->colSize; y++) {
			dot += mat->matrix[x][y] * vecCopy[y];
		}
		vecNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newVectorObject(vecNew, vec->size, Py_NEW);
}
//This is a helper for point/matrix translation 

PyObject *column_point_multiplication(MatrixObject * mat, PointObject* pt)
{
	float ptNew[4], ptCopy[4];
	double dot = 0.0f;
	int x, y, z = 0;

	if(mat->rowSize != pt->size){
		if(mat->rowSize == 4 && pt->size != 3){
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"matrix * point: matrix row size and point size must be the same\n");
		}else{
			ptCopy[3] = 0.0f;
		}
	}

	for(x = 0; x < pt->size; x++){
		ptCopy[x] = pt->coord[x];
		}

	for(x = 0; x < mat->rowSize; x++) {
		for(y = 0; y < mat->colSize; y++) {
			dot += mat->matrix[x][y] * ptCopy[y];
		}
		ptNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newPointObject(ptNew, pt->size, Py_NEW);
}
//-----------------row_vector_multiplication (internal)-----------
//ROW VECTOR Multiplication - Vector X Matrix
//[x][y][z] *  [1][2][3]
//             [4][5][6]
//             [7][8][9]
//vector/matrix multiplication IS NOT COMMUTATIVE!!!!
PyObject *row_vector_multiplication(VectorObject* vec, MatrixObject * mat)
{
	float vecNew[4], vecCopy[4];
	double dot = 0.0f;
	int x, y, z = 0, vec_size = vec->size;

	if(mat->colSize != vec_size){
		if(mat->rowSize == 4 && vec_size != 3){
			return EXPP_ReturnPyObjError(PyExc_AttributeError, 
				"vector * matrix: matrix column size and the vector size must be the same");
		}else{
			vecCopy[3] = 1.0f;
		}
	}
	
	for(x = 0; x < vec_size; x++){
		vecCopy[x] = vec->vec[x];
	}

	//muliplication
	for(x = 0; x < mat->colSize; x++) {
		for(y = 0; y < mat->rowSize; y++) {
			dot += mat->matrix[y][x] * vecCopy[y];
		}
		vecNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newVectorObject(vecNew, vec_size, Py_NEW);
}
//This is a helper for the point class
PyObject *row_point_multiplication(PointObject* pt, MatrixObject * mat)
{
	float ptNew[4], ptCopy[4];
	double dot = 0.0f;
	int x, y, z = 0, size;

	if(mat->colSize != pt->size){
		if(mat->rowSize == 4 && pt->size != 3){
			return EXPP_ReturnPyObjError(PyExc_AttributeError, 
				"point * matrix: matrix column size and the point size must be the same\n");
		}else{
			ptCopy[3] = 0.0f;
		}
	}
	size = pt->size;
	for(x = 0; x < pt->size; x++){
		ptCopy[x] = pt->coord[x];
	}

	//muliplication
	for(x = 0; x < mat->colSize; x++) {
		for(y = 0; y < mat->rowSize; y++) {
			dot += mat->matrix[y][x] * ptCopy[y];
		}
		ptNew[z++] = (float)dot;
		dot = 0.0f;
	}
	return newPointObject(ptNew, size, Py_NEW);
}
//-----------------quat_rotation (internal)-----------
//This function multiplies a vector/point * quat or vice versa
//to rotate the point/vector by the quaternion
//arguments should all be 3D
PyObject *quat_rotation(PyObject *arg1, PyObject *arg2)
{
	float rot[3];
	QuaternionObject *quat = NULL;
	VectorObject *vec = NULL;
	PointObject *pt = NULL;

	if(QuaternionObject_Check(arg1)){
		quat = (QuaternionObject*)arg1;
		if(VectorObject_Check(arg2)){
			vec = (VectorObject*)arg2;
			rot[0] = quat->quat[0]*quat->quat[0]*vec->vec[0] + 2*quat->quat[2]*quat->quat[0]*vec->vec[2] - 
				2*quat->quat[3]*quat->quat[0]*vec->vec[1] + quat->quat[1]*quat->quat[1]*vec->vec[0] + 
				2*quat->quat[2]*quat->quat[1]*vec->vec[1] + 2*quat->quat[3]*quat->quat[1]*vec->vec[2] - 
				quat->quat[3]*quat->quat[3]*vec->vec[0] - quat->quat[2]*quat->quat[2]*vec->vec[0];
			rot[1] = 2*quat->quat[1]*quat->quat[2]*vec->vec[0] + quat->quat[2]*quat->quat[2]*vec->vec[1] + 
				2*quat->quat[3]*quat->quat[2]*vec->vec[2] + 2*quat->quat[0]*quat->quat[3]*vec->vec[0] - 
				quat->quat[3]*quat->quat[3]*vec->vec[1] + quat->quat[0]*quat->quat[0]*vec->vec[1] - 
				2*quat->quat[1]*quat->quat[0]*vec->vec[2] - quat->quat[1]*quat->quat[1]*vec->vec[1];
			rot[2] = 2*quat->quat[1]*quat->quat[3]*vec->vec[0] + 2*quat->quat[2]*quat->quat[3]*vec->vec[1] + 
				quat->quat[3]*quat->quat[3]*vec->vec[2] - 2*quat->quat[0]*quat->quat[2]*vec->vec[0] - 
				quat->quat[2]*quat->quat[2]*vec->vec[2] + 2*quat->quat[0]*quat->quat[1]*vec->vec[1] - 
				quat->quat[1]*quat->quat[1]*vec->vec[2] + quat->quat[0]*quat->quat[0]*vec->vec[2];
			return newVectorObject(rot, 3, Py_NEW);
		}else if(PointObject_Check(arg2)){
			pt = (PointObject*)arg2;
			rot[0] = quat->quat[0]*quat->quat[0]*pt->coord[0] + 2*quat->quat[2]*quat->quat[0]*pt->coord[2] - 
				2*quat->quat[3]*quat->quat[0]*pt->coord[1] + quat->quat[1]*quat->quat[1]*pt->coord[0] + 
				2*quat->quat[2]*quat->quat[1]*pt->coord[1] + 2*quat->quat[3]*quat->quat[1]*pt->coord[2] - 
				quat->quat[3]*quat->quat[3]*pt->coord[0] - quat->quat[2]*quat->quat[2]*pt->coord[0];
			rot[1] = 2*quat->quat[1]*quat->quat[2]*pt->coord[0] + quat->quat[2]*quat->quat[2]*pt->coord[1] + 
				2*quat->quat[3]*quat->quat[2]*pt->coord[2] + 2*quat->quat[0]*quat->quat[3]*pt->coord[0] - 
				quat->quat[3]*quat->quat[3]*pt->coord[1] + quat->quat[0]*quat->quat[0]*pt->coord[1] - 
				2*quat->quat[1]*quat->quat[0]*pt->coord[2] - quat->quat[1]*quat->quat[1]*pt->coord[1];
			rot[2] = 2*quat->quat[1]*quat->quat[3]*pt->coord[0] + 2*quat->quat[2]*quat->quat[3]*pt->coord[1] + 
				quat->quat[3]*quat->quat[3]*pt->coord[2] - 2*quat->quat[0]*quat->quat[2]*pt->coord[0] - 
				quat->quat[2]*quat->quat[2]*pt->coord[2] + 2*quat->quat[0]*quat->quat[1]*pt->coord[1] - 
				quat->quat[1]*quat->quat[1]*pt->coord[2] + quat->quat[0]*quat->quat[0]*pt->coord[2];
			return newPointObject(rot, 3, Py_NEW);
		}
	}else if(VectorObject_Check(arg1)){
		vec = (VectorObject*)arg1;
		if(QuaternionObject_Check(arg2)){
			quat = (QuaternionObject*)arg2;
			rot[0] = quat->quat[0]*quat->quat[0]*vec->vec[0] + 2*quat->quat[2]*quat->quat[0]*vec->vec[2] - 
				2*quat->quat[3]*quat->quat[0]*vec->vec[1] + quat->quat[1]*quat->quat[1]*vec->vec[0] + 
				2*quat->quat[2]*quat->quat[1]*vec->vec[1] + 2*quat->quat[3]*quat->quat[1]*vec->vec[2] - 
				quat->quat[3]*quat->quat[3]*vec->vec[0] - quat->quat[2]*quat->quat[2]*vec->vec[0];
			rot[1] = 2*quat->quat[1]*quat->quat[2]*vec->vec[0] + quat->quat[2]*quat->quat[2]*vec->vec[1] + 
				2*quat->quat[3]*quat->quat[2]*vec->vec[2] + 2*quat->quat[0]*quat->quat[3]*vec->vec[0] - 
				quat->quat[3]*quat->quat[3]*vec->vec[1] + quat->quat[0]*quat->quat[0]*vec->vec[1] - 
				2*quat->quat[1]*quat->quat[0]*vec->vec[2] - quat->quat[1]*quat->quat[1]*vec->vec[1];
			rot[2] = 2*quat->quat[1]*quat->quat[3]*vec->vec[0] + 2*quat->quat[2]*quat->quat[3]*vec->vec[1] + 
				quat->quat[3]*quat->quat[3]*vec->vec[2] - 2*quat->quat[0]*quat->quat[2]*vec->vec[0] - 
				quat->quat[2]*quat->quat[2]*vec->vec[2] + 2*quat->quat[0]*quat->quat[1]*vec->vec[1] - 
				quat->quat[1]*quat->quat[1]*vec->vec[2] + quat->quat[0]*quat->quat[0]*vec->vec[2];
			return newVectorObject(rot, 3, Py_NEW);
		}
	}else if(PointObject_Check(arg1)){
		pt = (PointObject*)arg1;
		if(QuaternionObject_Check(arg2)){
			quat = (QuaternionObject*)arg2;
			rot[0] = quat->quat[0]*quat->quat[0]*pt->coord[0] + 2*quat->quat[2]*quat->quat[0]*pt->coord[2] - 
				2*quat->quat[3]*quat->quat[0]*pt->coord[1] + quat->quat[1]*quat->quat[1]*pt->coord[0] + 
				2*quat->quat[2]*quat->quat[1]*pt->coord[1] + 2*quat->quat[3]*quat->quat[1]*pt->coord[2] - 
				quat->quat[3]*quat->quat[3]*pt->coord[0] - quat->quat[2]*quat->quat[2]*pt->coord[0];
			rot[1] = 2*quat->quat[1]*quat->quat[2]*pt->coord[0] + quat->quat[2]*quat->quat[2]*pt->coord[1] + 
				2*quat->quat[3]*quat->quat[2]*pt->coord[2] + 2*quat->quat[0]*quat->quat[3]*pt->coord[0] - 
				quat->quat[3]*quat->quat[3]*pt->coord[1] + quat->quat[0]*quat->quat[0]*pt->coord[1] - 
				2*quat->quat[1]*quat->quat[0]*pt->coord[2] - quat->quat[1]*quat->quat[1]*pt->coord[1];
			rot[2] = 2*quat->quat[1]*quat->quat[3]*pt->coord[0] + 2*quat->quat[2]*quat->quat[3]*pt->coord[1] + 
				quat->quat[3]*quat->quat[3]*pt->coord[2] - 2*quat->quat[0]*quat->quat[2]*pt->coord[0] - 
				quat->quat[2]*quat->quat[2]*pt->coord[2] + 2*quat->quat[0]*quat->quat[1]*pt->coord[1] - 
				quat->quat[1]*quat->quat[1]*pt->coord[2] + quat->quat[0]*quat->quat[0]*pt->coord[2];
			return newPointObject(rot, 3, Py_NEW);
		}
	}

	return (EXPP_ReturnPyObjError(PyExc_RuntimeError,
		"quat_rotation(internal): internal problem rotating vector/point\n"));
}

//----------------------------------Mathutils.Rand() --------------------
//returns a random number between a high and low value
PyObject *M_Mathutils_Rand(PyObject * self, PyObject * args)
{
	float high, low, range;
	double rand;
	//initializers
	high = 1.0;
	low = 0.0;

	if(!PyArg_ParseTuple(args, "|ff", &low, &high))
		return (EXPP_ReturnPyObjError(PyExc_TypeError,
			"Mathutils.Rand(): expected nothing or optional (float, float)\n"));

	if((high < low) || (high < 0 && low > 0))
		return (EXPP_ReturnPyObjError(PyExc_ValueError,
			"Mathutils.Rand(): high value should be larger than low value\n"));

	//get the random number 0 - 1
	rand = BLI_drand();

	//set it to range
	range = high - low;
	rand = rand * range;
	rand = rand + low;

	return PyFloat_FromDouble(rand);
}
//----------------------------------VECTOR FUNCTIONS---------------------
//----------------------------------Mathutils.Vector() ------------------
// Supports 2D, 3D, and 4D vector objects both int and float values
// accepted. Mixed float and int values accepted. Ints are parsed to float 
PyObject *M_Mathutils_Vector(PyObject * self, PyObject * args)
{
	PyObject *listObject = NULL;
	int size, i;
	float vec[4];
	PyObject *v, *f;

	size = PySequence_Length(args);
	if (size == 1) {
		listObject = PySequence_GetItem(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
		} else { // Single argument was not a sequence
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
		}
	} else if (size == 0) {
		//returns a new empty 3d vector
		return newVectorObject(NULL, 3, Py_NEW); 
	} else {
		listObject = EXPP_incr_ret(args);
	}

	if (size<2 || size>4) { // Invalid vector size
		Py_XDECREF(listObject);
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
	}

	for (i=0; i<size; i++) {
		v=PySequence_GetItem(listObject, i);
		if (v==NULL) { // Failed to read sequence
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
				"Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
		}

		f=PyNumber_Float(v);
		if(f==NULL) { // parsed item not a number
			Py_DECREF(v);
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Vector(): 2-4 floats or ints expected (optionally in a sequence)\n");
		}

		vec[i]=(float)PyFloat_AS_DOUBLE(f);
		EXPP_decr2(f,v);
	}
	Py_DECREF(listObject);
	return newVectorObject(vec, size, Py_NEW);
}
//----------------------------------Mathutils.CrossVecs() ---------------
//finds perpendicular vector - only 3D is supported
PyObject *M_Mathutils_CrossVecs(PyObject * self, PyObject * args)
{
	PyObject *vecCross = NULL;
	VectorObject *vec1 = NULL, *vec2 = NULL;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.CrossVecs(): expects (2) 3D vector objects\n");
	if(vec1->size != 3 || vec2->size != 3)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.CrossVecs(): expects (2) 3D vector objects\n");

	vecCross = newVectorObject(NULL, 3, Py_NEW);
	Crossf(((VectorObject*)vecCross)->vec, vec1->vec, vec2->vec);
	return vecCross;
}
//----------------------------------Mathutils.DotVec() -------------------
//calculates the dot product of two vectors
PyObject *M_Mathutils_DotVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	double dot = 0.0f;
	int x;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.DotVecs(): expects (2) vector objects of the same size\n");
	if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.DotVecs(): expects (2) vector objects of the same size\n");

	for(x = 0; x < vec1->size; x++) {
		dot += vec1->vec[x] * vec2->vec[x];
	}
	return PyFloat_FromDouble(dot);
}
//----------------------------------Mathutils.AngleBetweenVecs() ---------
//calculates the angle between 2 vectors
PyObject *M_Mathutils_AngleBetweenVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	double dot = 0.0f, angleRads, test_v1 = 0.0f, test_v2 = 0.0f;
	int x, size;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		goto AttributeError1; //not vectors
	if(vec1->size != vec2->size)
		goto AttributeError1; //bad sizes

	//since size is the same....
	size = vec1->size;

	for(x = 0; x < size; x++) {
		test_v1 += vec1->vec[x] * vec1->vec[x];
		test_v2 += vec2->vec[x] * vec2->vec[x];
	}
	if (!test_v1 || !test_v2){
		goto AttributeError2; //zero-length vector
	}

	//dot product
	for(x = 0; x < size; x++) {
		dot += vec1->vec[x] * vec2->vec[x];
	}
	dot /= (sqrt(test_v1) * sqrt(test_v2));

	angleRads = (double)saacos(dot);

	return PyFloat_FromDouble(angleRads * (180/ Py_PI));

AttributeError1:
	return EXPP_ReturnPyObjError(PyExc_AttributeError, 
		"Mathutils.AngleBetweenVecs(): expects (2) VECTOR objects of the same size\n");

AttributeError2:
	return EXPP_ReturnPyObjError(PyExc_AttributeError, 
		"Mathutils.AngleBetweenVecs(): zero length vectors are not acceptable arguments\n");
}
//----------------------------------Mathutils.MidpointVecs() -------------
//calculates the midpoint between 2 vectors
PyObject *M_Mathutils_MidpointVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[4];
	int x;
	
	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.MidpointVecs(): expects (2) vector objects of the same size\n");
	if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.MidpointVecs(): expects (2) vector objects of the same size\n");

	for(x = 0; x < vec1->size; x++) {
		vec[x] = 0.5f * (vec1->vec[x] + vec2->vec[x]);
	}
	return newVectorObject(vec, vec1->size, Py_NEW);
}
//----------------------------------Mathutils.ProjectVecs() -------------
//projects vector 1 onto vector 2
PyObject *M_Mathutils_ProjectVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[4]; 
	double dot = 0.0f, dot2 = 0.0f;
	int x, size;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.ProjectVecs(): expects (2) vector objects of the same size\n");
	if(vec1->size != vec2->size)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.ProjectVecs(): expects (2) vector objects of the same size\n");

	//since they are the same size...
	size = vec1->size;

	//get dot products
	for(x = 0; x < size; x++) {
		dot += vec1->vec[x] * vec2->vec[x];
		dot2 += vec2->vec[x] * vec2->vec[x];
	}
	//projection
	dot /= dot2;
	for(x = 0; x < size; x++) {
		vec[x] = (float)(dot * vec2->vec[x]);
	}
	return newVectorObject(vec, size, Py_NEW);
}
//----------------------------------MATRIX FUNCTIONS--------------------
//----------------------------------Mathutils.Matrix() -----------------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//create a new matrix type
PyObject *M_Mathutils_Matrix(PyObject * self, PyObject * args)
{
	PyObject *listObject = NULL;
	PyObject *argObject, *m, *s, *f;
	MatrixObject *mat;
	int argSize, seqSize = 0, i, j;
	float matrix[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	argSize = PySequence_Length(args);
	if(argSize > 4){	//bad arg nums
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
	} else if (argSize == 0) { //return empty 4D matrix
		return (PyObject *) newMatrixObject(NULL, 4, 4, Py_NEW);
	}else if (argSize == 1){
		//copy constructor for matrix objects
		argObject = PySequence_GetItem(args, 0);
		if(MatrixObject_Check(argObject)){
			mat = (MatrixObject*)argObject;

			argSize = mat->rowSize; //rows
			seqSize = mat->colSize; //col
			for(i = 0; i < (seqSize * argSize); i++){
				matrix[i] = mat->contigPtr[i];
			}
		}
		Py_DECREF(argObject);
	}else{ //2-4 arguments (all seqs? all same size?)
		for(i =0; i < argSize; i++){
			argObject = PySequence_GetItem(args, i);
			if (PySequence_Check(argObject)) { //seq?
				if(seqSize){ //0 at first
					if(PySequence_Length(argObject) != seqSize){ //seq size not same
						Py_DECREF(argObject);
						return EXPP_ReturnPyObjError(PyExc_AttributeError, 
						"Mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
					}
				}
				seqSize = PySequence_Length(argObject);
			}else{ //arg not a sequence
				Py_XDECREF(argObject);
				return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"Mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
			}
			Py_DECREF(argObject);
		}
		//all is well... let's continue parsing
		listObject = args;
		for (i = 0; i < argSize; i++){
			m = PySequence_GetItem(listObject, i);
			if (m == NULL) { // Failed to read sequence
				return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
					"Mathutils.Matrix(): failed to parse arguments...\n");
			}

			for (j = 0; j < seqSize; j++) {
				s = PySequence_GetItem(m, j);
					if (s == NULL) { // Failed to read sequence
					Py_DECREF(m);
					return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
						"Mathutils.Matrix(): failed to parse arguments...\n");
				}

				f = PyNumber_Float(s);
				if(f == NULL) { // parsed item is not a number
					EXPP_decr2(m,s);
					return EXPP_ReturnPyObjError(PyExc_AttributeError, 
						"Mathutils.Matrix(): expects 0-4 numeric sequences of the same size\n");
				}

				matrix[(seqSize*i)+j]=(float)PyFloat_AS_DOUBLE(f);
				EXPP_decr2(f,s);
			}
			Py_DECREF(m);
		}
	}
	return newMatrixObject(matrix, argSize, seqSize, Py_NEW);
}
//----------------------------------Mathutils.RotationMatrix() ----------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates a rotation matrix
PyObject *M_Mathutils_RotationMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	char *axis = NULL;
	int matSize;
	float angle = 0.0f, norm = 0.0f, cosAngle = 0.0f, sinAngle = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple
	    (args, "fi|sO!", &angle, &matSize, &axis, &vector_Type, &vec)) {
		return EXPP_ReturnPyObjError (PyExc_TypeError, 
			"Mathutils.RotationMatrix(): expected float int and optional string and vector\n");
	}
	
	/* Clamp to -360:360 */
	while (angle<-360.0f)
		angle+=360.0;
	while (angle>360.0f)
		angle-=360.0;
	
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.RotationMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
	if(matSize == 2 && (axis != NULL || vec != NULL))
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.RotationMatrix(): cannot create a 2x2 rotation matrix around arbitrary axis\n");
	if((matSize == 3 || matSize == 4) && axis == NULL)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.RotationMatrix(): please choose an axis of rotation for 3d and 4d matrices\n");
	if(axis) {
		if(((strcmp(axis, "r") == 0) ||
		      (strcmp(axis, "R") == 0)) && vec == NULL)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Mathutils.RotationMatrix(): please define the arbitrary axis of rotation\n");
	}
	if(vec) {
		if(vec->size != 3)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
						      "Mathutils.RotationMatrix(): the arbitrary axis must be a 3D vector\n");
	}
	//convert to radians
	angle = angle * (float) (Py_PI / 180);
	if(axis == NULL && matSize == 2) {
		//2D rotation matrix
		mat[0] = (float) cos (angle);
		mat[1] = (float) sin (angle);
		mat[2] = -((float) sin(angle));
		mat[3] = (float) cos(angle);
	} else if((strcmp(axis, "x") == 0) || (strcmp(axis, "X") == 0)) {
		//rotation around X
		mat[0] = 1.0f;
		mat[4] = (float) cos(angle);
		mat[5] = (float) sin(angle);
		mat[7] = -((float) sin(angle));
		mat[8] = (float) cos(angle);
	} else if((strcmp(axis, "y") == 0) || (strcmp(axis, "Y") == 0)) {
		//rotation around Y
		mat[0] = (float) cos(angle);
		mat[2] = -((float) sin(angle));
		mat[4] = 1.0f;
		mat[6] = (float) sin(angle);
		mat[8] = (float) cos(angle);
	} else if((strcmp(axis, "z") == 0) || (strcmp(axis, "Z") == 0)) {
		//rotation around Z
		mat[0] = (float) cos(angle);
		mat[1] = (float) sin(angle);
		mat[3] = -((float) sin(angle));
		mat[4] = (float) cos(angle);
		mat[8] = 1.0f;
	} else if((strcmp(axis, "r") == 0) || (strcmp(axis, "R") == 0)) {
		//arbitrary rotation
		//normalize arbitrary axis
		norm = (float) sqrt(vec->vec[0] * vec->vec[0] +
				       vec->vec[1] * vec->vec[1] +
				       vec->vec[2] * vec->vec[2]);
		vec->vec[0] /= norm;
		vec->vec[1] /= norm;
		vec->vec[2] /= norm;
		
		if (isnan(vec->vec[0]) || isnan(vec->vec[1]) || isnan(vec->vec[2])) {
			/* zero length vector, return an identity matrix, could also return an error */
			mat[0]= mat[4] = mat[8] = 1.0f;
		} else {	
			/* create matrix */
			cosAngle = (float) cos(angle);
			sinAngle = (float) sin(angle);
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
		}
	} else {
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.RotationMatrix(): unrecognizable axis of rotation type - expected x,y,z or r\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW);
}
//----------------------------------Mathutils.TranslationMatrix() -------
//creates a translation matrix
PyObject *M_Mathutils_TranslationMatrix(PyObject * self, VectorObject * vec)
{
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!VectorObject_Check(vec)) {
		return EXPP_ReturnPyObjError(PyExc_TypeError,
						"Mathutils.TranslationMatrix(): expected vector\n");
	}
	if(vec->size != 3 && vec->size != 4) {
		return EXPP_ReturnPyObjError(PyExc_TypeError,
					      "Mathutils.TranslationMatrix(): vector must be 3D or 4D\n");
	}
	//create a identity matrix and add translation
	Mat4One((float(*)[4]) mat);
	mat[12] = vec->vec[0];
	mat[13] = vec->vec[1];
	mat[14] = vec->vec[2];

	return newMatrixObject(mat, 4, 4, Py_NEW);
}
//----------------------------------Mathutils.ScaleMatrix() -------------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates a scaling matrix
PyObject *M_Mathutils_ScaleMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	float norm = 0.0f, factor;
	int matSize, x;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple
	    (args, "fi|O!", &factor, &matSize, &vector_Type, &vec)) {
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"Mathutils.ScaleMatrix(): expected float int and optional vector\n");
	}
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.ScaleMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
	if(vec) {
		if(vec->size > 2 && matSize == 2)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Mathutils.ScaleMatrix(): please use 2D vectors when scaling in 2D\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW);
}
//----------------------------------Mathutils.OrthoProjectionMatrix() ---
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates an ortho projection matrix
PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	char *plane;
	int matSize, x;
	float norm = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!PyArg_ParseTuple
	    (args, "si|O!", &plane, &matSize, &vector_Type, &vec)) {
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"Mathutils.OrthoProjectionMatrix(): expected string and int and optional vector\n");
	}
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.OrthoProjectionMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
	if(vec) {
		if(vec->size > 2 && matSize == 2)
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Mathutils.OrthoProjectionMatrix(): please use 2D vectors when scaling in 2D\n");
	}
	if(vec == NULL) {	//ortho projection onto cardinal plane
		if(((strcmp(plane, "x") == 0)
		      || (strcmp(plane, "X") == 0)) && matSize == 2) {
			mat[0] = 1.0f;
		} else if(((strcmp(plane, "y") == 0) 
			|| (strcmp(plane, "Y") == 0))
			   && matSize == 2) {
			mat[3] = 1.0f;
		} else if(((strcmp(plane, "xy") == 0)
			     || (strcmp(plane, "XY") == 0))
			   && matSize > 2) {
			mat[0] = 1.0f;
			mat[4] = 1.0f;
		} else if(((strcmp(plane, "xz") == 0)
			     || (strcmp(plane, "XZ") == 0))
			   && matSize > 2) {
			mat[0] = 1.0f;
			mat[8] = 1.0f;
		} else if(((strcmp(plane, "yz") == 0)
			     || (strcmp(plane, "YZ") == 0))
			   && matSize > 2) {
			mat[4] = 1.0f;
			mat[8] = 1.0f;
		} else {
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Mathutils.OrthoProjectionMatrix(): unknown plane - expected: x, y, xy, xz, yz\n");
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
		if(((strcmp(plane, "r") == 0)
		      || (strcmp(plane, "R") == 0)) && matSize == 2) {
			mat[0] = 1 - (vec->vec[0] * vec->vec[0]);
			mat[1] = -(vec->vec[0] * vec->vec[1]);
			mat[2] = -(vec->vec[0] * vec->vec[1]);
			mat[3] = 1 - (vec->vec[1] * vec->vec[1]);
		} else if(((strcmp(plane, "r") == 0)
			     || (strcmp(plane, "R") == 0))
			   && matSize > 2) {
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
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
				"Mathutils.OrthoProjectionMatrix(): unknown plane - expected: 'r' expected for axis designation\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW);
}
//----------------------------------Mathutils.ShearMatrix() -------------
//creates a shear matrix
PyObject *M_Mathutils_ShearMatrix(PyObject * self, PyObject * args)
{
	int matSize;
	char *plane;
	float factor;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "sfi", &plane, &factor, &matSize)) {
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			"Mathutils.ShearMatrix(): expected string float and int\n");
	}
	if(matSize != 2 && matSize != 3 && matSize != 4)
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.ShearMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");

	if(((strcmp(plane, "x") == 0) || (strcmp(plane, "X") == 0))
	    && matSize == 2) {
		mat[0] = 1.0f;
		mat[2] = factor;
		mat[3] = 1.0f;
	} else if(((strcmp(plane, "y") == 0)
		     || (strcmp(plane, "Y") == 0)) && matSize == 2) {
		mat[0] = 1.0f;
		mat[1] = factor;
		mat[3] = 1.0f;
	} else if(((strcmp(plane, "xy") == 0)
		     || (strcmp(plane, "XY") == 0)) && matSize > 2) {
		mat[0] = 1.0f;
		mat[4] = 1.0f;
		mat[6] = factor;
		mat[7] = factor;
	} else if(((strcmp(plane, "xz") == 0)
		     || (strcmp(plane, "XZ") == 0)) && matSize > 2) {
		mat[0] = 1.0f;
		mat[3] = factor;
		mat[4] = 1.0f;
		mat[5] = factor;
		mat[8] = 1.0f;
	} else if(((strcmp(plane, "yz") == 0)
		     || (strcmp(plane, "YZ") == 0)) && matSize > 2) {
		mat[0] = 1.0f;
		mat[1] = factor;
		mat[2] = factor;
		mat[4] = 1.0f;
		mat[8] = 1.0f;
	} else {
		return EXPP_ReturnPyObjError(PyExc_AttributeError,
			"Mathutils.ShearMatrix(): expected: x, y, xy, xz, yz or wrong matrix size for shearing plane\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW);
}
//----------------------------------QUATERNION FUNCTIONS-----------------
//----------------------------------Mathutils.Quaternion() --------------
PyObject *M_Mathutils_Quaternion(PyObject * self, PyObject * args)
{
	PyObject *listObject = NULL, *n, *q, *f;
	int size, i;
	float quat[4];
	double norm = 0.0f, angle = 0.0f;

	size = PySequence_Length(args);
	if (size == 1 || size == 2) { //seq?
		listObject = PySequence_GetItem(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
			if ((size == 4 && PySequence_Length(args) !=1) || 
				(size == 3 && PySequence_Length(args) !=2) || (size >4 || size < 3)) { 
				// invalid args/size
				Py_DECREF(listObject);
				return EXPP_ReturnPyObjError(PyExc_AttributeError, 
					"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			}
	   		if(size == 3){ //get angle in axis/angle
				n = PyNumber_Float(PySequence_GetItem(args, 1));
				if(n == NULL) { // parsed item not a number or getItem fail
					Py_DECREF(listObject);
					return EXPP_ReturnPyObjError(PyExc_TypeError, 
						"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
				}
				angle = PyFloat_AS_DOUBLE(n);
				Py_DECREF(n);
			}
		}else{
			listObject = PySequence_GetItem(args, 1);
			if (size>1 && PySequence_Check(listObject)) {
				size = PySequence_Length(listObject);
				if (size != 3) { 
					// invalid args/size
					Py_DECREF(listObject);
					return EXPP_ReturnPyObjError(PyExc_AttributeError, 
						"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
				}
				n = PyNumber_Float(PySequence_GetItem(args, 0));
				if(n == NULL) { // parsed item not a number or getItem fail
					Py_DECREF(listObject);
					return EXPP_ReturnPyObjError(PyExc_TypeError, 
						"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
				}
				angle = PyFloat_AS_DOUBLE(n);
				Py_DECREF(n);
			} else { // argument was not a sequence
				Py_XDECREF(listObject);
				return EXPP_ReturnPyObjError(PyExc_TypeError, 
					"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			}
		}
	} else if (size == 0) { //returns a new empty quat
		return newQuaternionObject(NULL, Py_NEW); 
	} else {
		listObject = EXPP_incr_ret(args);
	}

	if (size == 3) { // invalid quat size
		if(PySequence_Length(args) != 2){
			Py_DECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_AttributeError, 
				"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
		}
	}else{
		if(size != 4){
			Py_DECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_AttributeError, 
				"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
		}
	}

	for (i=0; i<size; i++) { //parse
		q = PySequence_GetItem(listObject, i);
		if (q == NULL) { // Failed to read sequence
			Py_DECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
				"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
		}

		f = PyNumber_Float(q);
		if(f == NULL) { // parsed item not a number
			EXPP_decr2(q, listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
		}

		quat[i] = (float)PyFloat_AS_DOUBLE(f);
		EXPP_decr2(f, q);
	}
	if(size == 3){ //calculate the quat based on axis/angle
		norm = sqrt(quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2]);
		quat[0] /= (float)norm;
		quat[1] /= (float)norm;
		quat[2] /= (float)norm;

		angle = angle * (Py_PI / 180);
		quat[3] =(float) (sin(angle/ 2.0f)) * quat[2];
		quat[2] =(float) (sin(angle/ 2.0f)) * quat[1];
		quat[1] =(float) (sin(angle/ 2.0f)) * quat[0];
		quat[0] =(float) (cos(angle/ 2.0f));
	}

	Py_DECREF(listObject);
	return newQuaternionObject(quat, Py_NEW);
}
//----------------------------------Mathutils.CrossQuats() ----------------
//quaternion multiplication - associate not commutative
PyObject *M_Mathutils_CrossQuats(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	float quat[4];

	if(!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, &quatU, 
		&quaternion_Type, &quatV))
		return EXPP_ReturnPyObjError(PyExc_TypeError,"Mathutils.CrossQuats(): expected Quaternion types");
	QuatMul(quat, quatU->quat, quatV->quat);

	return newQuaternionObject(quat, Py_NEW);
}
//----------------------------------Mathutils.DotQuats() ----------------
//returns the dot product of 2 quaternions
PyObject *M_Mathutils_DotQuats(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	double dot = 0.0f;
	int x;

	if(!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, &quatU, 
		&quaternion_Type, &quatV))
		return EXPP_ReturnPyObjError(PyExc_TypeError, "Mathutils.DotQuats(): expected Quaternion types");

	for(x = 0; x < 4; x++) {
		dot += quatU->quat[x] * quatV->quat[x];
	}
	return PyFloat_FromDouble(dot);
}
//----------------------------------Mathutils.DifferenceQuats() ---------
//returns the difference between 2 quaternions
PyObject *M_Mathutils_DifferenceQuats(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	float quat[4], tempQuat[4];
	double dot = 0.0f;
	int x;

	if(!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, 
		&quatU, &quaternion_Type, &quatV))
		return EXPP_ReturnPyObjError(PyExc_TypeError, "Mathutils.DifferenceQuats(): expected Quaternion types");

	tempQuat[0] = quatU->quat[0];
	tempQuat[1] = -quatU->quat[1];
	tempQuat[2] = -quatU->quat[2];
	tempQuat[3] = -quatU->quat[3];

	dot = sqrt(tempQuat[0] * tempQuat[0] + tempQuat[1] *  tempQuat[1] +
			       tempQuat[2] * tempQuat[2] + tempQuat[3] * tempQuat[3]);

	for(x = 0; x < 4; x++) {
		tempQuat[x] /= (float)(dot * dot);
	}
	QuatMul(quat, tempQuat, quatV->quat);
	return newQuaternionObject(quat, Py_NEW);
}
//----------------------------------Mathutils.Slerp() ------------------
//attemps to interpolate 2 quaternions and return the result
PyObject *M_Mathutils_Slerp(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	float quat[4], quat_u[4], quat_v[4], param;
	double x, y, dot, sinT, angle, IsinT;
	int z;

	if(!PyArg_ParseTuple(args, "O!O!f", &quaternion_Type, 
		&quatU, &quaternion_Type, &quatV, &param))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.Slerp(): expected Quaternion types and float");

	if(param > 1.0f || param < 0.0f)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
					"Mathutils.Slerp(): interpolation factor must be between 0.0 and 1.0");

	//copy quats
	for(z = 0; z < 4; z++){
		quat_u[z] = quatU->quat[z];
		quat_v[z] = quatV->quat[z];
	}

	//dot product
	dot = quat_u[0] * quat_v[0] + quat_u[1] * quat_v[1] +
		quat_u[2] * quat_v[2] + quat_u[3] * quat_v[3];

	//if negative negate a quat (shortest arc)
	if(dot < 0.0f) {
		quat_v[0] = -quat_v[0];
		quat_v[1] = -quat_v[1];
		quat_v[2] = -quat_v[2];
		quat_v[3] = -quat_v[3];
		dot = -dot;
	}
	if(dot > .99999f) { //very close
		x = 1.0f - param;
		y = param;
	} else {
		//calculate sin of angle
		sinT = sqrt(1.0f - (dot * dot));
		//calculate angle
		angle = atan2(sinT, dot);
		//caluculate inverse of sin(theta)
		IsinT = 1.0f / sinT;
		x = sin((1.0f - param) * angle) * IsinT;
		y = sin(param * angle) * IsinT;
	}
	//interpolate
	quat[0] = (float)(quat_u[0] * x + quat_v[0] * y);
	quat[1] = (float)(quat_u[1] * x + quat_v[1] * y);
	quat[2] = (float)(quat_u[2] * x + quat_v[2] * y);
	quat[3] = (float)(quat_u[3] * x + quat_v[3] * y);

	return newQuaternionObject(quat, Py_NEW);
}
//----------------------------------EULER FUNCTIONS----------------------
//----------------------------------Mathutils.Euler() -------------------
//makes a new euler for you to play with
PyObject *M_Mathutils_Euler(PyObject * self, PyObject * args)
{

	PyObject *listObject = NULL;
	int size, i;
	float eul[3];
	PyObject *e, *f;

	size = PySequence_Length(args);
	if (size == 1) {
		listObject = PySequence_GetItem(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
		} else { // Single argument was not a sequence
			Py_DECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Euler(): 3d numeric sequence expected\n");
		}
	} else if (size == 0) {
		//returns a new empty 3d euler
		return newEulerObject(NULL, Py_NEW); 
	} else {
		listObject = EXPP_incr_ret(args);
	}

	if (size != 3) { // Invalid euler size
		Py_DECREF(listObject);
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.Euler(): 3d numeric sequence expected\n");
	}

	for (i=0; i<size; i++) {
		e = PySequence_GetItem(listObject, i);
		if (e == NULL) { // Failed to read sequence
			Py_DECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
				"Mathutils.Euler(): 3d numeric sequence expected\n");
		}

		f = PyNumber_Float(e);
		if(f == NULL) { // parsed item not a number
			EXPP_decr2(e, listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Euler(): 3d numeric sequence expected\n");
		}

		eul[i]=(float)PyFloat_AS_DOUBLE(f);
		EXPP_decr2(f,e);
	}
	Py_DECREF(listObject);
	return newEulerObject(eul, Py_NEW);
}
//----------------------------------POINT FUNCTIONS---------------------
//----------------------------------Mathutils.Point() ------------------
PyObject *M_Mathutils_Point(PyObject * self, PyObject * args)
{
	PyObject *listObject = NULL;
	int size, i;
	float point[3];
	PyObject *v, *f;

	size = PySequence_Length(args);
	if (size == 1) {
		listObject = PySequence_GetItem(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
		} else { // Single argument was not a sequence
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Point(): 2-3 floats or ints expected (optionally in a sequence)\n");
		}
	} else if (size == 0) {
		//returns a new empty 3d point
		return newPointObject(NULL, 3, Py_NEW); 
	} else {
		listObject = EXPP_incr_ret(args);
	}

	if (size<2 || size>3) { // Invalid vector size
		Py_XDECREF(listObject);
		return EXPP_ReturnPyObjError(PyExc_AttributeError, 
			"Mathutils.Point(): 2-3 floats or ints expected (optionally in a sequence)\n");
	}

	for (i=0; i<size; i++) {
		v=PySequence_GetItem(listObject, i);
		if (v==NULL) { // Failed to read sequence
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_RuntimeError, 
				"Mathutils.Point(): 2-3 floats or ints expected (optionally in a sequence)\n");
		}

		f=PyNumber_Float(v);
		if(f==NULL) { // parsed item not a number
			Py_DECREF(v);
			Py_XDECREF(listObject);
			return EXPP_ReturnPyObjError(PyExc_TypeError, 
				"Mathutils.Point(): 2-3 floats or ints expected (optionally in a sequence)\n");
		}

		point[i]=(float)PyFloat_AS_DOUBLE(f);
		EXPP_decr2(f,v);
	}
	Py_DECREF(listObject);
	return newPointObject(point, size, Py_NEW);
}
//---------------------------------INTERSECTION FUNCTIONS--------------------
//----------------------------------Mathutils.Intersect() -------------------
PyObject *M_Mathutils_Intersect( PyObject * self, PyObject * args )
{
	VectorObject *ray, *ray_off, *vec1, *vec2, *vec3;
	float dir[3], orig[3], v1[3], v2[3], v3[3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
	float det, inv_det, u, v, t;
	int clip = 1;

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!O!O!|i", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3, &vector_Type, &ray, &vector_Type, &ray_off , &clip) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 5 vector types\n" ) );
	if( vec1->size != 3 || vec2->size != 3 || vec3->size != 3 || 
		ray->size != 3 || ray_off->size != 3)
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"only 3D vectors for all parameters\n" ) );

	VECCOPY(v1, vec1->vec);
	VECCOPY(v2, vec2->vec);
	VECCOPY(v3, vec3->vec);

	VECCOPY(dir, ray->vec);
	Normalize(dir);

	VECCOPY(orig, ray_off->vec);

	/* find vectors for two edges sharing v1 */
	VecSubf(e1, v2, v1);
	VecSubf(e2, v3, v1);

	/* begin calculating determinant - also used to calculated U parameter */
	Crossf(pvec, dir, e2);	

	/* if determinant is near zero, ray lies in plane of triangle */
	det = Inpf(e1, pvec);

	if (det > -0.000001 && det < 0.000001) {
		return EXPP_incr_ret( Py_None );
	}

	inv_det = 1.0f / det;

	/* calculate distance from v1 to ray origin */
	VecSubf(tvec, orig, v1);

	/* calculate U parameter and test bounds */
	u = Inpf(tvec, pvec) * inv_det;
	if (clip && (u < 0.0f || u > 1.0f)) {
		return EXPP_incr_ret( Py_None );
	}

	/* prepare to test the V parameter */
	Crossf(qvec, tvec, e1);

	/* calculate V parameter and test bounds */
	v = Inpf(dir, qvec) * inv_det;

	if (clip && (v < 0.0f || u + v > 1.0f)) {
		return EXPP_incr_ret( Py_None );
	}

	/* calculate t, ray intersects triangle */
	t = Inpf(e2, qvec) * inv_det;

	VecMulf(dir, t);
	VecAddf(pvec, orig, dir);

	return newVectorObject(pvec, 3, Py_NEW);
}
//----------------------------------Mathutils.LineIntersect() -------------------
/* Line-Line intersection using algorithm from mathworld.wolfram.com */
PyObject *M_Mathutils_LineIntersect( PyObject * self, PyObject * args )
{
	PyObject * tuple;
	VectorObject *vec1, *vec2, *vec3, *vec4;
	float v1[3], v2[3], v3[3], v4[3], i1[3], i2[3];

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3, &vector_Type, &vec4 ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 4 vector types\n" ) );
	if( vec1->size != vec2->size || vec1->size != vec3->size || vec1->size != vec2->size)
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"vectors must be of the same size\n" ) );

	if( vec1->size == 3 || vec1->size == 2) {
		float a[3], b[3], c[3], ab[3], cb[3], dir1[3], dir2[3];
		float d;
		if (vec1->size == 3) {
			VECCOPY(v1, vec1->vec);
			VECCOPY(v2, vec2->vec);
			VECCOPY(v3, vec3->vec);
			VECCOPY(v4, vec4->vec);
		}
		else {
			v1[0] = vec1->vec[0];
			v1[1] = vec1->vec[1];
			v1[2] = 0.0f;

			v2[0] = vec2->vec[0];
			v2[1] = vec2->vec[1];
			v2[2] = 0.0f;

			v3[0] = vec3->vec[0];
			v3[1] = vec3->vec[1];
			v3[2] = 0.0f;

			v4[0] = vec4->vec[0];
			v4[1] = vec4->vec[1];
			v4[2] = 0.0f;
		}

		VecSubf(c, v3, v1);
		VecSubf(a, v2, v1);
		VecSubf(b, v4, v3);

		VECCOPY(dir1, a);
		Normalize(dir1);
		VECCOPY(dir2, b);
		Normalize(dir2);
		d = Inpf(dir1, dir2);
		if (d == 1.0f || d == -1.0f) {
			/* colinear */
			return EXPP_incr_ret( Py_None );
		}

		Crossf(ab, a, b);
		d = Inpf(c, ab);

		/* test if the two lines are coplanar */
		if (d > -0.000001f && d < 0.000001f) {
			Crossf(cb, c, b);

			VecMulf(a, Inpf(cb, ab) / Inpf(ab, ab));
			VecAddf(i1, v1, a);
			VECCOPY(i2, i1);
		}
		/* if not */
		else {
			float n[3], t[3];
			VecSubf(t, v1, v3);

			/* offset between both plane where the lines lies */
			Crossf(n, a, b);
			Projf(t, t, n);

			/* for the first line, offset the second line until it is coplanar */
			VecAddf(v3, v3, t);
			VecAddf(v4, v4, t);
			
			VecSubf(c, v3, v1);
			VecSubf(a, v2, v1);
			VecSubf(b, v4, v3);

			Crossf(ab, a, b);
			Crossf(cb, c, b);

			VecMulf(a, Inpf(cb, ab) / Inpf(ab, ab));
			VecAddf(i1, v1, a);

			/* for the second line, just substract the offset from the first intersection point */
			VecSubf(i2, i1, t);
		}

		tuple = PyTuple_New( 2 );
		PyTuple_SetItem( tuple, 0, newVectorObject(i1, vec1->size, Py_NEW) );
		PyTuple_SetItem( tuple, 1, newVectorObject(i2, vec1->size, Py_NEW) );
		return tuple;
	}
	else {
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"2D/3D vectors only\n" ) );
	}
}



//---------------------------------NORMALS FUNCTIONS--------------------
//----------------------------------Mathutils.QuadNormal() -------------------
PyObject *M_Mathutils_QuadNormal( PyObject * self, PyObject * args )
{
	VectorObject *vec1;
	VectorObject *vec2;
	VectorObject *vec3;
	VectorObject *vec4;
	float v1[3], v2[3], v3[3], v4[3], e1[3], e2[3], n1[3], n2[3];

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3, &vector_Type, &vec4 ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 4 vector types\n" ) );
	if( vec1->size != vec2->size || vec1->size != vec3->size || vec1->size != vec4->size)
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"vectors must be of the same size\n" ) );
	if( vec1->size != 3 )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"only 3D vectors\n" ) );

	VECCOPY(v1, vec1->vec);
	VECCOPY(v2, vec2->vec);
	VECCOPY(v3, vec3->vec);
	VECCOPY(v4, vec4->vec);

	/* find vectors for two edges sharing v2 */
	VecSubf(e1, v1, v2);
	VecSubf(e2, v3, v2);

	Crossf(n1, e2, e1);
	Normalize(n1);

	/* find vectors for two edges sharing v4 */
	VecSubf(e1, v3, v4);
	VecSubf(e2, v1, v4);

	Crossf(n2, e2, e1);
	Normalize(n2);

	/* adding and averaging the normals of both triangles */
	VecAddf(n1, n2, n1);
	Normalize(n1);

	return newVectorObject(n1, 3, Py_NEW);
}

//----------------------------Mathutils.TriangleNormal() -------------------
PyObject *M_Mathutils_TriangleNormal( PyObject * self, PyObject * args )
{
	VectorObject *vec1, *vec2, *vec3;
	float v1[3], v2[3], v3[3], e1[3], e2[3], n[3];

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3 ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 3 vector types\n" ) );
	if( vec1->size != vec2->size || vec1->size != vec3->size )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"vectors must be of the same size\n" ) );
	if( vec1->size != 3 )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"only 3D vectors\n" ) );

	VECCOPY(v1, vec1->vec);
	VECCOPY(v2, vec2->vec);
	VECCOPY(v3, vec3->vec);

	/* find vectors for two edges sharing v2 */
	VecSubf(e1, v1, v2);
	VecSubf(e2, v3, v2);

	Crossf(n, e2, e1);
	Normalize(n);

	return newVectorObject(n, 3, Py_NEW);
}

//--------------------------------- AREA FUNCTIONS--------------------
//----------------------------------Mathutils.TriangleArea() -------------------
PyObject *M_Mathutils_TriangleArea( PyObject * self, PyObject * args )
{
	VectorObject *vec1, *vec2, *vec3;
	float v1[3], v2[3], v3[3];

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3 ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError, "expected 3 vector types\n" ) );
	if( vec1->size != vec2->size || vec1->size != vec3->size )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"vectors must be of the same size\n" ) );

	if (vec1->size == 3) {
		VECCOPY(v1, vec1->vec);
		VECCOPY(v2, vec2->vec);
		VECCOPY(v3, vec3->vec);

		return PyFloat_FromDouble( AreaT3Dfl(v1, v2, v3) );
	}
	else if (vec1->size == 2) {
		v1[0] = vec1->vec[0];
		v1[1] = vec1->vec[1];

		v2[0] = vec2->vec[0];
		v2[1] = vec2->vec[1];

		v3[0] = vec3->vec[0];
		v3[1] = vec3->vec[1];

		return PyFloat_FromDouble( AreaF2Dfl(v1, v2, v3) );
	}
	else {
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"only 2D,3D vectors are supported\n" ) );
	}
}
//#############################DEPRECATED################################
//#######################################################################
//----------------------------------Mathutils.CopyMat() -----------------
//copies a matrix into a new matrix
PyObject *M_Mathutils_CopyMat(PyObject * self, PyObject * args)
{
	PyObject *matrix = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.CopyMat(): deprecated :use Mathutils.Matrix() to copy matrices\n");
		--warning;
	}

	matrix = M_Mathutils_Matrix(self, args);
	if(matrix == NULL)
		return NULL; //error string already set if we get here
	else
		return matrix;
}
//----------------------------------Mathutils.CopyVec() -----------------
//makes a new vector that is a copy of the input
PyObject *M_Mathutils_CopyVec(PyObject * self, PyObject * args)
{
	PyObject *vec = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.CopyVec(): Deprecated: use Mathutils.Vector() to copy vectors\n");
		--warning;
	}

	vec = M_Mathutils_Vector(self, args);
	if(vec == NULL)
		return NULL; //error string already set if we get here
	else
		return vec;
}
//----------------------------------Mathutils.CopyQuat() --------------
//Copies a quaternion to a new quat
PyObject *M_Mathutils_CopyQuat(PyObject * self, PyObject * args)
{
	PyObject *quat = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.CopyQuat(): Deprecated: use Mathutils.Quaternion() to copy vectors\n");
		--warning;
	}

	quat = M_Mathutils_Quaternion(self, args);
	if(quat == NULL)
		return NULL; //error string already set if we get here
	else
		return quat;
}
//----------------------------------Mathutils.CopyEuler() ---------------
//copies a euler to a new euler
PyObject *M_Mathutils_CopyEuler(PyObject * self, PyObject * args)
{
	PyObject *eul = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.CopyEuler(): deprecated:use Mathutils.Euler() to copy vectors\n");
		--warning;
	}

	eul = M_Mathutils_Euler(self, args);
	if(eul == NULL)
		return NULL; //error string already set if we get here
	else
		return eul;
}
//----------------------------------Mathutils.RotateEuler() ------------
//rotates a euler a certain amount and returns the result
//should return a unique euler rotation (i.e. no 720 degree pitches :)
PyObject *M_Mathutils_RotateEuler(PyObject * self, PyObject * args)
{
	EulerObject *Eul = NULL;
	float angle;
	char *axis;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.RotateEuler(): Deprecated:use Euler.rotate() to rotate a euler\n");
		--warning;
	}

	if(!PyArg_ParseTuple(args, "O!fs", &euler_Type, &Eul, &angle, &axis))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
			   "Mathutils.RotateEuler(): expected euler type & float & string");

	Euler_Rotate(Eul, Py_BuildValue("fs", angle, axis));
	Py_RETURN_NONE;
}
//----------------------------------Mathutils.MatMultVec() --------------
//COLUMN VECTOR Multiplication (Matrix X Vector)
PyObject *M_Mathutils_MatMultVec(PyObject * self, PyObject * args)
{
	MatrixObject *mat = NULL;
	VectorObject *vec = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.MatMultVec(): Deprecated: use matrix * vec to perform column vector multiplication\n");
		--warning;
	}

	//get pyObjects
	if(!PyArg_ParseTuple(args, "O!O!", &matrix_Type, &mat, &vector_Type, &vec))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.MatMultVec(): MatMultVec() expects a matrix and a vector object - in that order\n");

	return column_vector_multiplication(mat, vec);
}
//----------------------------------Mathutils.VecMultMat() ---------------
//ROW VECTOR Multiplication - Vector X Matrix
PyObject *M_Mathutils_VecMultMat(PyObject * self, PyObject * args)
{
	MatrixObject *mat = NULL;
	VectorObject *vec = NULL;
	static char warning = 1;

	if( warning ) {
		printf("Mathutils.VecMultMat(): Deprecated: use vec * matrix to perform row vector multiplication\n");
		--warning;
	}

	//get pyObjects
	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec, &matrix_Type, &mat))
		return EXPP_ReturnPyObjError(PyExc_TypeError, 
			"Mathutils.VecMultMat(): VecMultMat() expects a vector and matrix object - in that order\n");

	return row_vector_multiplication(vec, mat);
}
//#######################################################################
//#############################DEPRECATED################################
