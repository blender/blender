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

//-------------------------DOC STRINGS ---------------------------
static char M_Mathutils_doc[] = "The Blender Mathutils module\n\n";
static char M_Mathutils_Rand_doc[] = "() - return a random number";
static char M_Mathutils_AngleBetweenVecs_doc[] = "() - returns the angle between two vectors in degrees";
static char M_Mathutils_MidpointVecs_doc[] = "() - return the vector to the midpoint between two vectors";
static char M_Mathutils_ProjectVecs_doc[] =	"() - returns the projection vector from the projection of vecA onto vecB";
static char M_Mathutils_RotationMatrix_doc[] = "() - construct a rotation matrix from an angle and axis of rotation";
static char M_Mathutils_ScaleMatrix_doc[] =	"() - construct a scaling matrix from a scaling factor";
static char M_Mathutils_OrthoProjectionMatrix_doc[] = "() - construct a orthographic projection matrix from a selected plane";
static char M_Mathutils_ShearMatrix_doc[] = "() - construct a shearing matrix from a plane of shear and a shear factor";
static char M_Mathutils_TranslationMatrix_doc[] = "(vec) - create a translation matrix from a vector";
static char M_Mathutils_Slerp_doc[] = "() - returns the interpolation between two quaternions";
static char M_Mathutils_DifferenceQuats_doc[] = "() - return the angular displacment difference between two quats";
static char M_Mathutils_Intersect_doc[] = "(v1, v2, v3, ray, orig, clip=1) - returns the intersection between a ray and a triangle, if possible, returns None otherwise";
static char M_Mathutils_TriangleArea_doc[] = "(v1, v2, v3) - returns the area size of the 2D or 3D triangle defined";
static char M_Mathutils_TriangleNormal_doc[] = "(v1, v2, v3) - returns the normal of the 3D triangle defined";
static char M_Mathutils_QuadNormal_doc[] = "(v1, v2, v3, v4) - returns the normal of the 3D quad defined";
static char M_Mathutils_LineIntersect_doc[] = "(v1, v2, v3, v4) - returns a tuple with the points on each line respectively closest to the other";
//-----------------------METHOD DEFINITIONS ----------------------

static PyObject *M_Mathutils_Rand(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_AngleBetweenVecs(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_MidpointVecs(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_ProjectVecs(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_RotationMatrix(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_TranslationMatrix(PyObject * self, VectorObject * value);
static PyObject *M_Mathutils_ScaleMatrix(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_ShearMatrix(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_DifferenceQuats(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_Slerp(PyObject * self, PyObject * args);
static PyObject *M_Mathutils_Intersect( PyObject * self, PyObject * args );
static PyObject *M_Mathutils_TriangleArea( PyObject * self, PyObject * args );
static PyObject *M_Mathutils_TriangleNormal( PyObject * self, PyObject * args );
static PyObject *M_Mathutils_QuadNormal( PyObject * self, PyObject * args );
static PyObject *M_Mathutils_LineIntersect( PyObject * self, PyObject * args );

struct PyMethodDef M_Mathutils_methods[] = {
	{"Rand", (PyCFunction) M_Mathutils_Rand, METH_VARARGS, M_Mathutils_Rand_doc},
	{"AngleBetweenVecs", (PyCFunction) M_Mathutils_AngleBetweenVecs, METH_VARARGS, M_Mathutils_AngleBetweenVecs_doc},
	{"MidpointVecs", (PyCFunction) M_Mathutils_MidpointVecs, METH_VARARGS, M_Mathutils_MidpointVecs_doc},
	{"ProjectVecs", (PyCFunction) M_Mathutils_ProjectVecs, METH_VARARGS, M_Mathutils_ProjectVecs_doc},
	{"RotationMatrix", (PyCFunction) M_Mathutils_RotationMatrix, METH_VARARGS, M_Mathutils_RotationMatrix_doc},
	{"ScaleMatrix", (PyCFunction) M_Mathutils_ScaleMatrix, METH_VARARGS, M_Mathutils_ScaleMatrix_doc},
	{"ShearMatrix", (PyCFunction) M_Mathutils_ShearMatrix, METH_VARARGS, M_Mathutils_ShearMatrix_doc},
	{"TranslationMatrix", (PyCFunction) M_Mathutils_TranslationMatrix, METH_O, M_Mathutils_TranslationMatrix_doc},
	{"OrthoProjectionMatrix", (PyCFunction) M_Mathutils_OrthoProjectionMatrix,  METH_VARARGS, M_Mathutils_OrthoProjectionMatrix_doc},
	{"DifferenceQuats", (PyCFunction) M_Mathutils_DifferenceQuats, METH_VARARGS,M_Mathutils_DifferenceQuats_doc},
	{"Slerp", (PyCFunction) M_Mathutils_Slerp, METH_VARARGS, M_Mathutils_Slerp_doc},
	{"Intersect", ( PyCFunction ) M_Mathutils_Intersect, METH_VARARGS, M_Mathutils_Intersect_doc},
	{"TriangleArea", ( PyCFunction ) M_Mathutils_TriangleArea, METH_VARARGS, M_Mathutils_TriangleArea_doc},
	{"TriangleNormal", ( PyCFunction ) M_Mathutils_TriangleNormal, METH_VARARGS, M_Mathutils_TriangleNormal_doc},
	{"QuadNormal", ( PyCFunction ) M_Mathutils_QuadNormal, METH_VARARGS, M_Mathutils_QuadNormal_doc},
	{"LineIntersect", ( PyCFunction ) M_Mathutils_LineIntersect, METH_VARARGS, M_Mathutils_LineIntersect_doc},
	{NULL, NULL, 0, NULL}
};

/*----------------------------MODULE INIT-------------------------*/
/* from can be Blender.Mathutils or GameLogic.Mathutils for the BGE */

static struct PyModuleDef M_Mathutils_module_def = {
	PyModuleDef_HEAD_INIT,
	"Mathutils",  /* m_name */
	M_Mathutils_doc,  /* m_doc */
	0,  /* m_size */
	M_Mathutils_methods,  /* m_methods */
	0,  /* m_reload */
	0,  /* m_traverse */
	0,  /* m_clear */
	0,  /* m_free */
};

PyObject *Mathutils_Init(void)
{
	PyObject *submodule;

	//seed the generator for the rand function
	BLI_srand((unsigned int) (PIL_check_seconds_timer() * 0x7FFFFFFF));
	
	if( PyType_Ready( &vector_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &matrix_Type ) < 0 )
		return NULL;	
	if( PyType_Ready( &euler_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &quaternion_Type ) < 0 )
		return NULL;
	
	submodule = PyModule_Create(&M_Mathutils_module_def);
	PyDict_SetItemString(PySys_GetObject("modules"), M_Mathutils_module_def.m_name, submodule);
	
	/* each type has its own new() function */
	PyModule_AddObject( submodule, "Vector",		(PyObject *)&vector_Type );
	PyModule_AddObject( submodule, "Matrix",		(PyObject *)&matrix_Type );
	PyModule_AddObject( submodule, "Euler",			(PyObject *)&euler_Type );
	PyModule_AddObject( submodule, "Quaternion",	(PyObject *)&quaternion_Type );
	
	mathutils_matrix_vector_cb_index= Mathutils_RegisterCallback(&mathutils_matrix_vector_cb);

	return (submodule);
}

//-----------------------------METHODS----------------------------
//-----------------quat_rotation (internal)-----------
//This function multiplies a vector/point * quat or vice versa
//to rotate the point/vector by the quaternion
//arguments should all be 3D
PyObject *quat_rotation(PyObject *arg1, PyObject *arg2)
{
	float rot[3];
	QuaternionObject *quat = NULL;
	VectorObject *vec = NULL;

	if(QuaternionObject_Check(arg1)){
		quat = (QuaternionObject*)arg1;
		if(!BaseMath_ReadCallback(quat))
			return NULL;

		if(VectorObject_Check(arg2)){
			vec = (VectorObject*)arg2;
			
			if(!BaseMath_ReadCallback(vec))
				return NULL;
			
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
			return newVectorObject(rot, 3, Py_NEW, NULL);
		}
	}else if(VectorObject_Check(arg1)){
		vec = (VectorObject*)arg1;
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
		if(QuaternionObject_Check(arg2)){
			quat = (QuaternionObject*)arg2;
			if(!BaseMath_ReadCallback(quat))
				return NULL;

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
			return newVectorObject(rot, 3, Py_NEW, NULL);
		}
	}

	PyErr_SetString(PyExc_RuntimeError, "quat_rotation(internal): internal problem rotating vector/point\n");
	return NULL;
	
}

//----------------------------------Mathutils.Rand() --------------------
//returns a random number between a high and low value
static PyObject *M_Mathutils_Rand(PyObject * self, PyObject * args)
{
	float high, low, range;
	double drand;
	//initializers
	high = 1.0;
	low = 0.0;

	if(!PyArg_ParseTuple(args, "|ff", &low, &high)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.Rand(): expected nothing or optional (float, float)\n");
		return NULL;
	}

	if((high < low) || (high < 0 && low > 0)) {
		PyErr_SetString(PyExc_ValueError, "Mathutils.Rand(): high value should be larger than low value\n");
		return NULL;
	}
	//get the random number 0 - 1
	drand = BLI_drand();

	//set it to range
	range = high - low;
	drand = drand * range;
	drand = drand + low;

	return PyFloat_FromDouble(drand);
}
//----------------------------------VECTOR FUNCTIONS---------------------
//----------------------------------Mathutils.AngleBetweenVecs() ---------
//calculates the angle between 2 vectors
static PyObject *M_Mathutils_AngleBetweenVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	double dot = 0.0f, angleRads, test_v1 = 0.0f, test_v2 = 0.0f;
	int x, size;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2))
		goto AttributeError1; //not vectors
	if(vec1->size != vec2->size)
		goto AttributeError1; //bad sizes

	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;
	
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

#ifdef USE_MATHUTILS_DEG
	return PyFloat_FromDouble(angleRads * (180/ Py_PI));
#else
	return PyFloat_FromDouble(angleRads);
#endif
AttributeError1:
	PyErr_SetString(PyExc_AttributeError, "Mathutils.AngleBetweenVecs(): expects (2) VECTOR objects of the same size\n");
	return NULL;

AttributeError2:
	PyErr_SetString(PyExc_AttributeError, "Mathutils.AngleBetweenVecs(): zero length vectors are not acceptable arguments\n");
	return NULL;
}
//----------------------------------Mathutils.MidpointVecs() -------------
//calculates the midpoint between 2 vectors
static PyObject *M_Mathutils_MidpointVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[4];
	int x;
	
	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.MidpointVecs(): expects (2) vector objects of the same size\n");
		return NULL;
	}
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.MidpointVecs(): expects (2) vector objects of the same size\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	for(x = 0; x < vec1->size; x++) {
		vec[x] = 0.5f * (vec1->vec[x] + vec2->vec[x]);
	}
	return newVectorObject(vec, vec1->size, Py_NEW, NULL);
}
//----------------------------------Mathutils.ProjectVecs() -------------
//projects vector 1 onto vector 2
static PyObject *M_Mathutils_ProjectVecs(PyObject * self, PyObject * args)
{
	VectorObject *vec1 = NULL, *vec2 = NULL;
	float vec[4]; 
	double dot = 0.0f, dot2 = 0.0f;
	int x, size;

	if(!PyArg_ParseTuple(args, "O!O!", &vector_Type, &vec1, &vector_Type, &vec2)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.ProjectVecs(): expects (2) vector objects of the same size\n");
		return NULL;
	}
	if(vec1->size != vec2->size) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.ProjectVecs(): expects (2) vector objects of the same size\n");
		return NULL;
	}

	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2))
		return NULL;

	
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
	return newVectorObject(vec, size, Py_NEW, NULL);
}
//----------------------------------MATRIX FUNCTIONS--------------------
//----------------------------------Mathutils.RotationMatrix() ----------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates a rotation matrix
static PyObject *M_Mathutils_RotationMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	char *axis = NULL;
	int matSize;
	float angle = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "fi|sO!", &angle, &matSize, &axis, &vector_Type, &vec)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.RotationMatrix(): expected float int and optional string and vector\n");
		return NULL;
	}

#ifdef USE_MATHUTILS_DEG
	/* Clamp to -360:360 */
	while (angle<-360.0f)
		angle+=360.0;
	while (angle>360.0f)
		angle-=360.0;
#else
	while (angle<-(Py_PI*2))
		angle+=(Py_PI*2);
	while (angle>(Py_PI*2))
		angle-=(Py_PI*2);
#endif
	
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
		return NULL;
	}
	if(matSize == 2 && (axis != NULL || vec != NULL)) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): cannot create a 2x2 rotation matrix around arbitrary axis\n");
		return NULL;
	}
	if((matSize == 3 || matSize == 4) && axis == NULL) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): please choose an axis of rotation for 3d and 4d matrices\n");
		return NULL;
	}
	if(axis) {
		if(((strcmp(axis, "r") == 0) || (strcmp(axis, "R") == 0)) && vec == NULL) {
			PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): please define the arbitrary axis of rotation\n");
			return NULL;
		}
	}
	if(vec) {
		if(vec->size != 3) {
			PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): the arbitrary axis must be a 3D vector\n");
			return NULL;
		}
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
	}
#ifdef USE_MATHUTILS_DEG
	//convert to radians
	angle = angle * (float) (Py_PI / 180);
#endif

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
		AxisAngleToMat3(vec->vec, angle, (float (*)[3])mat);

	} else {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.RotationMatrix(): unrecognizable axis of rotation type - expected x,y,z or r\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW, NULL);
}
//----------------------------------Mathutils.TranslationMatrix() -------
//creates a translation matrix
static PyObject *M_Mathutils_TranslationMatrix(PyObject * self, VectorObject * vec)
{
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!VectorObject_Check(vec)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.TranslationMatrix(): expected vector\n");
		return NULL;
	}
	if(vec->size != 3 && vec->size != 4) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.TranslationMatrix(): vector must be 3D or 4D\n");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec))
		return NULL;
	
	//create a identity matrix and add translation
	Mat4One((float(*)[4]) mat);
	mat[12] = vec->vec[0];
	mat[13] = vec->vec[1];
	mat[14] = vec->vec[2];

	return newMatrixObject(mat, 4, 4, Py_NEW, NULL);
}
//----------------------------------Mathutils.ScaleMatrix() -------------
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates a scaling matrix
static PyObject *M_Mathutils_ScaleMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	float norm = 0.0f, factor;
	int matSize, x;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "fi|O!", &factor, &matSize, &vector_Type, &vec)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.ScaleMatrix(): expected float int and optional vector\n");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.ScaleMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
		return NULL;
	}
	if(vec) {
		if(vec->size > 2 && matSize == 2) {
			PyErr_SetString(PyExc_AttributeError, "Mathutils.ScaleMatrix(): please use 2D vectors when scaling in 2D\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW, NULL);
}
//----------------------------------Mathutils.OrthoProjectionMatrix() ---
//mat is a 1D array of floats - row[0][0],row[0][1], row[1][0], etc.
//creates an ortho projection matrix
static PyObject *M_Mathutils_OrthoProjectionMatrix(PyObject * self, PyObject * args)
{
	VectorObject *vec = NULL;
	char *plane;
	int matSize, x;
	float norm = 0.0f;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
	
	if(!PyArg_ParseTuple(args, "si|O!", &plane, &matSize, &vector_Type, &vec)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.OrthoProjectionMatrix(): expected string and int and optional vector\n");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"Mathutils.OrthoProjectionMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
		return NULL;
	}
	if(vec) {
		if(vec->size > 2 && matSize == 2) {
			PyErr_SetString(PyExc_AttributeError, "Mathutils.OrthoProjectionMatrix(): please use 2D vectors when scaling in 2D\n");
			return NULL;
		}
		
		if(!BaseMath_ReadCallback(vec))
			return NULL;
		
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
			PyErr_SetString(PyExc_AttributeError, "Mathutils.OrthoProjectionMatrix(): unknown plane - expected: x, y, xy, xz, yz\n");
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
			PyErr_SetString(PyExc_AttributeError, "Mathutils.OrthoProjectionMatrix(): unknown plane - expected: 'r' expected for axis designation\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW, NULL);
}
//----------------------------------Mathutils.ShearMatrix() -------------
//creates a shear matrix
static PyObject *M_Mathutils_ShearMatrix(PyObject * self, PyObject * args)
{
	int matSize;
	char *plane;
	float factor;
	float mat[16] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

	if(!PyArg_ParseTuple(args, "sfi", &plane, &factor, &matSize)) {
		PyErr_SetString(PyExc_TypeError,"Mathutils.ShearMatrix(): expected string float and int\n");
		return NULL;
	}
	if(matSize != 2 && matSize != 3 && matSize != 4) {
		PyErr_SetString(PyExc_AttributeError,"Mathutils.ShearMatrix(): can only return a 2x2 3x3 or 4x4 matrix\n");
		return NULL;
	}

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
		PyErr_SetString(PyExc_AttributeError, "Mathutils.ShearMatrix(): expected: x, y, xy, xz, yz or wrong matrix size for shearing plane\n");
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
	return newMatrixObject(mat, matSize, matSize, Py_NEW, NULL);
}
//----------------------------------QUATERNION FUNCTIONS-----------------

//----------------------------------Mathutils.DifferenceQuats() ---------
//returns the difference between 2 quaternions
static PyObject *M_Mathutils_DifferenceQuats(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	float quat[4], tempQuat[4];
	double dot = 0.0f;
	int x;

	if(!PyArg_ParseTuple(args, "O!O!", &quaternion_Type, &quatU, &quaternion_Type, &quatV)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.DifferenceQuats(): expected Quaternion types");
		return NULL;
	}

	if(!BaseMath_ReadCallback(quatU) || !BaseMath_ReadCallback(quatV))
		return NULL;

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
	return newQuaternionObject(quat, Py_NEW, NULL);
}
//----------------------------------Mathutils.Slerp() ------------------
//attemps to interpolate 2 quaternions and return the result
static PyObject *M_Mathutils_Slerp(PyObject * self, PyObject * args)
{
	QuaternionObject *quatU = NULL, *quatV = NULL;
	float quat[4], quat_u[4], quat_v[4], param;
	double x, y, dot, sinT, angle, IsinT;
	int z;

	if(!PyArg_ParseTuple(args, "O!O!f", &quaternion_Type, &quatU, &quaternion_Type, &quatV, &param)) {
		PyErr_SetString(PyExc_TypeError, "Mathutils.Slerp(): expected Quaternion types and float");
		return NULL;
	}

	if(!BaseMath_ReadCallback(quatU) || !BaseMath_ReadCallback(quatV))
		return NULL;

	if(param > 1.0f || param < 0.0f) {
		PyErr_SetString(PyExc_AttributeError, "Mathutils.Slerp(): interpolation factor must be between 0.0 and 1.0");
		return NULL;
	}

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

	return newQuaternionObject(quat, Py_NEW, NULL);
}
//----------------------------------EULER FUNCTIONS----------------------
//---------------------------------INTERSECTION FUNCTIONS--------------------
//----------------------------------Mathutils.Intersect() -------------------
static PyObject *M_Mathutils_Intersect( PyObject * self, PyObject * args )
{
	VectorObject *ray, *ray_off, *vec1, *vec2, *vec3;
	float dir[3], orig[3], v1[3], v2[3], v3[3], e1[3], e2[3], pvec[3], tvec[3], qvec[3];
	float det, inv_det, u, v, t;
	int clip = 1;

	if(!PyArg_ParseTuple(args, "O!O!O!O!O!|i", &vector_Type, &vec1, &vector_Type, &vec2, &vector_Type, &vec3, &vector_Type, &ray, &vector_Type, &ray_off , &clip)) {
		PyErr_SetString( PyExc_TypeError, "expected 5 vector types\n" );
		return NULL;
	}
	if(vec1->size != 3 || vec2->size != 3 || vec3->size != 3 || ray->size != 3 || ray_off->size != 3) {
		PyErr_SetString( PyExc_TypeError, "only 3D vectors for all parameters\n");
		return NULL;
	}

	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2) || !BaseMath_ReadCallback(vec3) || !BaseMath_ReadCallback(ray) || !BaseMath_ReadCallback(ray_off))
		return NULL;
	
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
		Py_RETURN_NONE;
	}

	inv_det = 1.0f / det;

	/* calculate distance from v1 to ray origin */
	VecSubf(tvec, orig, v1);

	/* calculate U parameter and test bounds */
	u = Inpf(tvec, pvec) * inv_det;
	if (clip && (u < 0.0f || u > 1.0f)) {
		Py_RETURN_NONE;
	}

	/* prepare to test the V parameter */
	Crossf(qvec, tvec, e1);

	/* calculate V parameter and test bounds */
	v = Inpf(dir, qvec) * inv_det;

	if (clip && (v < 0.0f || u + v > 1.0f)) {
		Py_RETURN_NONE;
	}

	/* calculate t, ray intersects triangle */
	t = Inpf(e2, qvec) * inv_det;

	VecMulf(dir, t);
	VecAddf(pvec, orig, dir);

	return newVectorObject(pvec, 3, Py_NEW, NULL);
}
//----------------------------------Mathutils.LineIntersect() -------------------
/* Line-Line intersection using algorithm from mathworld.wolfram.com */
static PyObject *M_Mathutils_LineIntersect( PyObject * self, PyObject * args )
{
	PyObject * tuple;
	VectorObject *vec1, *vec2, *vec3, *vec4;
	float v1[3], v2[3], v3[3], v4[3], i1[3], i2[3];

	if( !PyArg_ParseTuple( args, "O!O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2, &vector_Type, &vec3, &vector_Type, &vec4 ) ) {
		PyErr_SetString( PyExc_TypeError, "expected 4 vector types\n" );
		return NULL;
	}
	if( vec1->size != vec2->size || vec1->size != vec3->size || vec1->size != vec2->size) {
		PyErr_SetString( PyExc_TypeError,"vectors must be of the same size\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2) || !BaseMath_ReadCallback(vec3) || !BaseMath_ReadCallback(vec4))
		return NULL;
	
	if( vec1->size == 3 || vec1->size == 2) {
		int result;
		
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
		
		result = LineIntersectLine(v1, v2, v3, v4, i1, i2);

		if (result == 0) {
			/* colinear */
			Py_RETURN_NONE;
		}
		else {
			tuple = PyTuple_New( 2 );
			PyTuple_SetItem( tuple, 0, newVectorObject(i1, vec1->size, Py_NEW, NULL) );
			PyTuple_SetItem( tuple, 1, newVectorObject(i2, vec1->size, Py_NEW, NULL) );
			return tuple;
		}
	}
	else {
		PyErr_SetString( PyExc_TypeError, "2D/3D vectors only\n" );
		return NULL;
	}
}



//---------------------------------NORMALS FUNCTIONS--------------------
//----------------------------------Mathutils.QuadNormal() -------------------
static PyObject *M_Mathutils_QuadNormal( PyObject * self, PyObject * args )
{
	VectorObject *vec1;
	VectorObject *vec2;
	VectorObject *vec3;
	VectorObject *vec4;
	float v1[3], v2[3], v3[3], v4[3], e1[3], e2[3], n1[3], n2[3];

	if( !PyArg_ParseTuple( args, "O!O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2, &vector_Type, &vec3, &vector_Type, &vec4 ) ) {
		PyErr_SetString( PyExc_TypeError, "expected 4 vector types\n" );
		return NULL;
	}
	if( vec1->size != vec2->size || vec1->size != vec3->size || vec1->size != vec4->size) {
		PyErr_SetString( PyExc_TypeError,"vectors must be of the same size\n" );
		return NULL;
	}
	if( vec1->size != 3 ) {
		PyErr_SetString( PyExc_TypeError, "only 3D vectors\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2) || !BaseMath_ReadCallback(vec3) || !BaseMath_ReadCallback(vec4))
		return NULL;
	
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

	return newVectorObject(n1, 3, Py_NEW, NULL);
}

//----------------------------Mathutils.TriangleNormal() -------------------
static PyObject *M_Mathutils_TriangleNormal( PyObject * self, PyObject * args )
{
	VectorObject *vec1, *vec2, *vec3;
	float v1[3], v2[3], v3[3], e1[3], e2[3], n[3];

	if( !PyArg_ParseTuple( args, "O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2, &vector_Type, &vec3 ) ) {
		PyErr_SetString( PyExc_TypeError, "expected 3 vector types\n" );
		return NULL;
	}
	if( vec1->size != vec2->size || vec1->size != vec3->size ) {
		PyErr_SetString( PyExc_TypeError, "vectors must be of the same size\n" );
		return NULL;
	}
	if( vec1->size != 3 ) {
		PyErr_SetString( PyExc_TypeError, "only 3D vectors\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2) || !BaseMath_ReadCallback(vec3))
		return NULL;

	VECCOPY(v1, vec1->vec);
	VECCOPY(v2, vec2->vec);
	VECCOPY(v3, vec3->vec);

	/* find vectors for two edges sharing v2 */
	VecSubf(e1, v1, v2);
	VecSubf(e2, v3, v2);

	Crossf(n, e2, e1);
	Normalize(n);

	return newVectorObject(n, 3, Py_NEW, NULL);
}

//--------------------------------- AREA FUNCTIONS--------------------
//----------------------------------Mathutils.TriangleArea() -------------------
static PyObject *M_Mathutils_TriangleArea( PyObject * self, PyObject * args )
{
	VectorObject *vec1, *vec2, *vec3;
	float v1[3], v2[3], v3[3];

	if( !PyArg_ParseTuple
	    ( args, "O!O!O!", &vector_Type, &vec1, &vector_Type, &vec2
		, &vector_Type, &vec3 ) ) {
		PyErr_SetString( PyExc_TypeError, "expected 3 vector types\n");
		return NULL;
	}
	if( vec1->size != vec2->size || vec1->size != vec3->size ) {
		PyErr_SetString( PyExc_TypeError, "vectors must be of the same size\n" );
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(vec1) || !BaseMath_ReadCallback(vec2) || !BaseMath_ReadCallback(vec3))
		return NULL;

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
		PyErr_SetString( PyExc_TypeError, "only 2D,3D vectors are supported\n" );
		return NULL;
	}
}

/* Utility functions */

/*---------------------- EXPP_FloatsAreEqual -------------------------
  Floating point comparisons 
  floatStep = number of representable floats allowable in between
   float A and float B to be considered equal. */
int EXPP_FloatsAreEqual(float A, float B, int floatSteps)
{
	int a, b, delta;
    assert(floatSteps > 0 && floatSteps < (4 * 1024 * 1024));
    a = *(int*)&A;
    if (a < 0)	
		a = 0x80000000 - a;
    b = *(int*)&B;
    if (b < 0)	
		b = 0x80000000 - b;
    delta = abs(a - b);
    if (delta <= floatSteps)	
		return 1;
    return 0;
}
/*---------------------- EXPP_VectorsAreEqual -------------------------
  Builds on EXPP_FloatsAreEqual to test vectors */
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps)
{
	int x;
	for (x=0; x< size; x++){
		if (EXPP_FloatsAreEqual(vecA[x], vecB[x], floatSteps) == 0)
			return 0;
	}
	return 1;
}


/* Mathutils Callbacks */

/* for mathutils internal use only, eventually should re-alloc but to start with we only have a few users */
Mathutils_Callback *mathutils_callbacks[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

int Mathutils_RegisterCallback(Mathutils_Callback *cb)
{
	int i;
	
	/* find the first free slot */
	for(i= 0; mathutils_callbacks[i]; i++) {
		if(mathutils_callbacks[i]==cb) /* alredy registered? */
			return i;
	}
	
	mathutils_callbacks[i] = cb;
	return i;
}

/* use macros to check for NULL */
int _BaseMathObject_ReadCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->get(self->cb_user, self->cb_subtype, self->data))
		return 1;

	PyErr_Format(PyExc_SystemError, "%s user has become invalid", Py_TYPE(self)->tp_name);
	return 0;
}

int _BaseMathObject_WriteCallback(BaseMathObject *self)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->set(self->cb_user, self->cb_subtype, self->data))
		return 1;

	PyErr_Format(PyExc_SystemError, "%s user has become invalid", Py_TYPE(self)->tp_name);
	return 0;
}

int _BaseMathObject_ReadIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->get_index(self->cb_user, self->cb_subtype, self->data, index))
		return 1;

	PyErr_Format(PyExc_SystemError, "%s user has become invalid", Py_TYPE(self)->tp_name);
	return 0;
}

int _BaseMathObject_WriteIndexCallback(BaseMathObject *self, int index)
{
	Mathutils_Callback *cb= mathutils_callbacks[self->cb_type];
	if(cb->set_index(self->cb_user, self->cb_subtype, self->data, index))
		return 1;

	PyErr_Format(PyExc_SystemError, "%s user has become invalid", Py_TYPE(self)->tp_name);
	return 0;
}

/* BaseMathObject generic functions for all mathutils types */
PyObject *BaseMathObject_getOwner( BaseMathObject * self, void *type )
{
	PyObject *ret= self->cb_user ? self->cb_user : Py_None;
	Py_INCREF(ret);
	return ret;
}

PyObject *BaseMathObject_getWrapped( BaseMathObject *self, void *type )
{
	return PyBool_FromLong((self->wrapped == Py_WRAP) ? 1:0);
}

void BaseMathObject_dealloc(BaseMathObject * self)
{
	/* only free non wrapped */
	if(self->wrapped != Py_WRAP)
		PyMem_Free(self->data);

	Py_XDECREF(self->cb_user);
	Py_TYPE(self)->tp_free(self); // PyObject_DEL(self); // breaks subtypes
}

