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
 * 
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Mathutils.h"

#include "BLI_arithb.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"


//-------------------------DOC STRINGS ---------------------------
static char Quaternion_Identity_doc[] = "() - set the quaternion to it's identity (1, vector)";
static char Quaternion_Negate_doc[] = "() - set all values in the quaternion to their negative";
static char Quaternion_Conjugate_doc[] = "() - set the quaternion to it's conjugate";
static char Quaternion_Inverse_doc[] = "() - set the quaternion to it's inverse";
static char Quaternion_Normalize_doc[] = "() - normalize the vector portion of the quaternion";
static char Quaternion_ToEuler_doc[] = "(eul_compat) - return a euler rotation representing the quaternion, optional euler argument that the new euler will be made compatible with.";
static char Quaternion_ToMatrix_doc[] = "() - return a rotation matrix representing the quaternion";
static char Quaternion_Cross_doc[] = "(other) - return the cross product between this quaternion and another";
static char Quaternion_Dot_doc[] = "(other) - return the dot product between this quaternion and another";
static char Quaternion_copy_doc[] = "() - return a copy of the quat";

static PyObject *Quaternion_Identity( QuaternionObject * self );
static PyObject *Quaternion_Negate( QuaternionObject * self );
static PyObject *Quaternion_Conjugate( QuaternionObject * self );
static PyObject *Quaternion_Inverse( QuaternionObject * self );
static PyObject *Quaternion_Normalize( QuaternionObject * self );
static PyObject *Quaternion_ToEuler( QuaternionObject * self, PyObject *args );
static PyObject *Quaternion_ToMatrix( QuaternionObject * self );
static PyObject *Quaternion_Cross( QuaternionObject * self, QuaternionObject * value );
static PyObject *Quaternion_Dot( QuaternionObject * self, QuaternionObject * value );
static PyObject *Quaternion_copy( QuaternionObject * self );

//-----------------------METHOD DEFINITIONS ----------------------
static struct PyMethodDef Quaternion_methods[] = {
	{"identity", (PyCFunction) Quaternion_Identity, METH_NOARGS, Quaternion_Identity_doc},
	{"negate", (PyCFunction) Quaternion_Negate, METH_NOARGS, Quaternion_Negate_doc},
	{"conjugate", (PyCFunction) Quaternion_Conjugate, METH_NOARGS, Quaternion_Conjugate_doc},
	{"inverse", (PyCFunction) Quaternion_Inverse, METH_NOARGS, Quaternion_Inverse_doc},
	{"normalize", (PyCFunction) Quaternion_Normalize, METH_NOARGS, Quaternion_Normalize_doc},
	{"toEuler", (PyCFunction) Quaternion_ToEuler, METH_VARARGS, Quaternion_ToEuler_doc},
	{"toMatrix", (PyCFunction) Quaternion_ToMatrix, METH_NOARGS, Quaternion_ToMatrix_doc},
	{"cross", (PyCFunction) Quaternion_Cross, METH_O, Quaternion_Cross_doc},
	{"dot", (PyCFunction) Quaternion_Dot, METH_O, Quaternion_Dot_doc},
	{"__copy__", (PyCFunction) Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
	{"copy", (PyCFunction) Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
	{NULL, NULL, 0, NULL}
};

//----------------------------------Mathutils.Quaternion() --------------
static PyObject *Quaternion_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *listObject = NULL, *n, *q;
	int size, i;
	float quat[4], scalar;
	double norm = 0.0f, angle = 0.0f;

	size = PyTuple_GET_SIZE(args);
	if (size == 1 || size == 2) { //seq?
		listObject = PyTuple_GET_ITEM(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
			if ((size == 4 && PySequence_Length(args) !=1) || 
				(size == 3 && PySequence_Length(args) !=2) || (size >4 || size < 3)) { 
				// invalid args/size
				PyErr_SetString(PyExc_AttributeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
				return NULL;
			}
	   		if(size == 3){ //get angle in axis/angle
				n = PySequence_GetItem(args, 1);
				if(n == NULL) { // parsed item not a number or getItem fail
					PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
					return NULL;
				}
				
				angle = PyFloat_AsDouble(n);
				Py_DECREF(n);
				
				if (angle==-1 && PyErr_Occurred()) {
					PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
					return NULL;
				}
			}
		}else{
			listObject = PyTuple_GET_ITEM(args, 1);
			if (size>1 && PySequence_Check(listObject)) {
				size = PySequence_Length(listObject);
				if (size != 3) { 
					// invalid args/size
					PyErr_SetString(PyExc_AttributeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
					return NULL;
				}
				angle = PyFloat_AsDouble(PyTuple_GET_ITEM(args, 0));
				
				if (angle==-1 && PyErr_Occurred()) {
					PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
					return NULL;
				}
			} else { // argument was not a sequence
				PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
				return NULL;
			}
		}
	} else if (size == 0) { //returns a new empty quat
		return newQuaternionObject(NULL, Py_NEW); 
	} else {
		listObject = args;
	}

	if (size == 3) { // invalid quat size
		if(PySequence_Length(args) != 2){
			PyErr_SetString(PyExc_AttributeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			return NULL;
		}
	}else{
		if(size != 4){
			PyErr_SetString(PyExc_AttributeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			return NULL;
		}
	}

	for (i=0; i<size; i++) { //parse
		q = PySequence_GetItem(listObject, i);
		if (q == NULL) { // Failed to read sequence
			PyErr_SetString(PyExc_RuntimeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			return NULL;
		}

		scalar = PyFloat_AsDouble(q);
		if (scalar==-1 && PyErr_Occurred()) {
			Py_DECREF(q);
			PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			return NULL;
		}

		quat[i] = scalar;
		Py_DECREF(q);
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

	return newQuaternionObject(quat, Py_NEW);
}

//-----------------------------METHODS------------------------------
//----------------------------Quaternion.toEuler()------------------
//return the quat as a euler
static PyObject *Quaternion_ToEuler(QuaternionObject * self, PyObject *args)
{
	float eul[3];
	EulerObject *eul_compat = NULL;
	int x;
	
	if(!PyArg_ParseTuple(args, "|O!:toEuler", &euler_Type, &eul_compat))
		return NULL;
	
	if(eul_compat) {
		float mat[3][3], eul_compatf[3];
		
		for(x = 0; x < 3; x++) {
			eul_compatf[x] = eul_compat->eul[x] * ((float)Py_PI / 180);
		}
		
		QuatToMat3(self->quat, mat);
		Mat3ToCompatibleEul(mat, eul, eul_compatf);
	}
	else {
		QuatToEul(self->quat, eul);
	}
	
	
	for(x = 0; x < 3; x++) {
		eul[x] *= (180 / (float)Py_PI);
	}
	return newEulerObject(eul, Py_NEW);
}
//----------------------------Quaternion.toMatrix()------------------
//return the quat as a matrix
static PyObject *Quaternion_ToMatrix(QuaternionObject * self)
{
	float mat[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	QuatToMat3(self->quat, (float (*)[3]) mat);

	return newMatrixObject(mat, 3, 3, Py_NEW);
}

//----------------------------Quaternion.cross(other)------------------
//return the cross quat
static PyObject *Quaternion_Cross(QuaternionObject * self, QuaternionObject * value)
{
	float quat[4];
	
	if (!QuaternionObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "quat.cross(value): expected a quaternion argument" );
		return NULL;
	}
	
	QuatMul(quat, self->quat, value->quat);
	return newQuaternionObject(quat, Py_NEW);
}

//----------------------------Quaternion.dot(other)------------------
//return the dot quat
static PyObject *Quaternion_Dot(QuaternionObject * self, QuaternionObject * value)
{
	int x;
	double dot = 0.0;
	
	if (!QuaternionObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "quat.dot(value): expected a quaternion argument" );
		return NULL;
	}
	
	for(x = 0; x < 4; x++) {
		dot += self->quat[x] * value->quat[x];
	}
	return PyFloat_FromDouble(dot);
}

//----------------------------Quaternion.normalize()----------------
//normalize the axis of rotation of [theta,vector]
static PyObject *Quaternion_Normalize(QuaternionObject * self)
{
	NormalQuat(self->quat);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.inverse()------------------
//invert the quat
static PyObject *Quaternion_Inverse(QuaternionObject * self)
{
	double mag = 0.0f;
	int x;

	for(x = 1; x < 4; x++) {
		self->quat[x] = -self->quat[x];
	}
	for(x = 0; x < 4; x++) {
		mag += (self->quat[x] * self->quat[x]);
	}
	mag = sqrt(mag);
	for(x = 0; x < 4; x++) {
		self->quat[x] /= (float)(mag * mag);
	}

	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.identity()-----------------
//generate the identity quaternion
static PyObject *Quaternion_Identity(QuaternionObject * self)
{
	self->quat[0] = 1.0;
	self->quat[1] = 0.0;
	self->quat[2] = 0.0;
	self->quat[3] = 0.0;

	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.negate()-------------------
//negate the quat
static PyObject *Quaternion_Negate(QuaternionObject * self)
{
	int x;
	for(x = 0; x < 4; x++) {
		self->quat[x] = -self->quat[x];
	}
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.conjugate()----------------
//negate the vector part
static PyObject *Quaternion_Conjugate(QuaternionObject * self)
{
	int x;
	for(x = 1; x < 4; x++) {
		self->quat[x] = -self->quat[x];
	}
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.copy()----------------
//return a copy of the quat
static PyObject *Quaternion_copy(QuaternionObject * self)
{
	return newQuaternionObject(self->quat, Py_NEW);	
}

//----------------------------dealloc()(internal) ------------------
//free the py_object
static void Quaternion_dealloc(QuaternionObject * self)
{
	//only free py_data
	if(self->data.py_data){
		PyMem_Free(self->data.py_data);
	}
	PyObject_DEL(self);
}

//----------------------------print object (internal)--------------
//print the object to screen
static PyObject *Quaternion_repr(QuaternionObject * self)
{
	char str[64];
	sprintf(str, "[%.6f, %.6f, %.6f, %.6f](quaternion)", self->quat[0], self->quat[1], self->quat[2], self->quat[3]);
	return PyUnicode_FromString(str);
}
//------------------------tp_richcmpr
//returns -1 execption, 0 false, 1 true
static PyObject* Quaternion_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	QuaternionObject *quatA = NULL, *quatB = NULL;
	int result = 0;

	if (!QuaternionObject_Check(objectA) || !QuaternionObject_Check(objectB)){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}
	quatA = (QuaternionObject*)objectA;
	quatB = (QuaternionObject*)objectB;

	switch (comparison_type){
		case Py_EQ:
			result = EXPP_VectorsAreEqual(quatA->quat, quatB->quat, 4, 1);
			break;
		case Py_NE:
			result = EXPP_VectorsAreEqual(quatA->quat, quatB->quat, 4, 1);
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
//------------------------tp_doc
static char QuaternionObject_doc[] = "This is a wrapper for quaternion objects.";
//---------------------SEQUENCE PROTOCOLS------------------------
//----------------------------len(object)------------------------
//sequence length
static int Quaternion_len(QuaternionObject * self)
{
	return 4;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Quaternion_item(QuaternionObject * self, int i)
{
	if(i < 0 || i >= 4) {
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute]: array index out of range\n");
		return NULL;
	}
	return PyFloat_FromDouble(self->quat[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Quaternion_ass_item(QuaternionObject * self, int i, PyObject * ob)
{
	PyObject *f = NULL;

	f = PyNumber_Float(ob);
	if(f == NULL) { // parsed item not a number
		PyErr_SetString(PyExc_TypeError, "quaternion[attribute] = x: argument not a number\n");
		return -1;
	}

	if(i < 0 || i >= 4){
		Py_DECREF(f);
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute] = x: array assignment index out of range\n");
		return -1;
	}
	self->quat[i] = (float)PyFloat_AS_DOUBLE(f);
	Py_DECREF(f);
	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Quaternion_slice(QuaternionObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	CLAMP(begin, 0, 4);
	if (end<0) end= 5+end;
	CLAMP(end, 0, 4);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				PyFloat_FromDouble(self->quat[count]));
	}

	return list;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Quaternion_ass_slice(QuaternionObject * self, int begin, int end,
			     PyObject * seq)
{
	int i, y, size = 0;
	float quat[4];
	PyObject *q, *f;

	CLAMP(begin, 0, 4);
	if (end<0) end= 5+end;
	CLAMP(end, 0, 4);
	begin = MIN2(begin,end);

	size = PySequence_Length(seq);
	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "quaternion[begin:end] = []: size mismatch in slice assignment\n");
		return -1;
	}

	for (i = 0; i < size; i++) {
		q = PySequence_GetItem(seq, i);
		if (q == NULL) { // Failed to read sequence
			PyErr_SetString(PyExc_RuntimeError, "quaternion[begin:end] = []: unable to read sequence\n");
			return -1;
		}

		f = PyNumber_Float(q);
		if(f == NULL) { // parsed item not a number
			Py_DECREF(q);
			PyErr_SetString(PyExc_TypeError, "quaternion[begin:end] = []: sequence argument not a number\n");
			return -1;
		}

		quat[i] = (float)PyFloat_AS_DOUBLE(f);
		Py_DECREF(f);
		Py_DECREF(q);
	}
	//parsed well - now set in vector
	for(y = 0; y < size; y++){
		self->quat[begin + y] = quat[y];
	}
	return 0;
}
//------------------------NUMERIC PROTOCOLS----------------------
//------------------------obj + obj------------------------------
//addition
static PyObject *Quaternion_add(PyObject * q1, PyObject * q2)
{
	int x;
	float quat[4];
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

	if(!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
		PyErr_SetString(PyExc_AttributeError, "Quaternion addition: arguments not valid for this operation....\n");
		return NULL;
	}
	
	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;
	
	for(x = 0; x < 4; x++) {
		quat[x] = quat1->quat[x] + quat2->quat[x];
	}

	return newQuaternionObject(quat, Py_NEW);
}
//------------------------obj - obj------------------------------
//subtraction
static PyObject *Quaternion_sub(PyObject * q1, PyObject * q2)
{
	int x;
	float quat[4];
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

	if(!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
		PyErr_SetString(PyExc_AttributeError, "Quaternion addition: arguments not valid for this operation....\n");
		return NULL;
	}
	
	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;
	
	for(x = 0; x < 4; x++) {
		quat[x] = quat1->quat[x] - quat2->quat[x];
	}

	return newQuaternionObject(quat, Py_NEW);
}
//------------------------obj * obj------------------------------
//mulplication
static PyObject *Quaternion_mul(PyObject * q1, PyObject * q2)
{
	int x;
	float quat[4], scalar;
	double dot = 0.0f;
	QuaternionObject *quat1 = NULL, *quat2 = NULL;
	VectorObject *vec = NULL;

	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;

	if(QuaternionObject_Check(q1) && QuaternionObject_Check(q2)) { /* QUAT*QUAT (dot product) */
		for(x = 0; x < 4; x++) {
			dot += quat1->quat[x] * quat1->quat[x];
		}
		return PyFloat_FromDouble(dot);
	}
	
	/* the only case this can happen (for a supported type is "FLOAT*QUAT" ) */
	if(!QuaternionObject_Check(q1)) {
		scalar= PyFloat_AsDouble(q1);
		if ((scalar == -1.0 && PyErr_Occurred())==0) { /* FLOAT*QUAT */
			for(x = 0; x < 4; x++) {
				quat[x] = quat2->quat[x] * scalar;
			}
			return newQuaternionObject(quat, Py_NEW);
		}
		PyErr_SetString(PyExc_TypeError, "Quaternion multiplication: val * quat, val is not an acceptable type");
		return NULL;
	}
	else { /* QUAT*SOMETHING */
		if(VectorObject_Check(q2)){  /* QUAT*VEC */
			vec = (VectorObject*)q2;
			if(vec->size != 3){
				PyErr_SetString(PyExc_TypeError, "Quaternion multiplication: only 3D vector rotations currently supported\n");
				return NULL;
			}
			return quat_rotation((PyObject*)quat1, (PyObject*)vec);
		}
		
		scalar= PyFloat_AsDouble(q2);
		if ((scalar == -1.0 && PyErr_Occurred())==0) { /* QUAT*FLOAT */
			for(x = 0; x < 4; x++) {
				quat[x] = quat1->quat[x] * scalar;
			}
			return newQuaternionObject(quat, Py_NEW);
		}
	}
	
	PyErr_SetString(PyExc_TypeError, "Quaternion multiplication: arguments not acceptable for this operation\n");
	return NULL;
}

//-----------------PROTOCOL DECLARATIONS--------------------------
static PySequenceMethods Quaternion_SeqMethods = {
	(inquiry) Quaternion_len,					/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(ssizeargfunc) 0,								/* sq_repeat */
	(ssizeargfunc) Quaternion_item,				/* sq_item */
	(ssizessizeargfunc) Quaternion_slice,			/* sq_slice */
	(ssizeobjargproc) Quaternion_ass_item,		/* sq_ass_item */
	(ssizessizeobjargproc) Quaternion_ass_slice,	/* sq_ass_slice */
};
static PyNumberMethods Quaternion_NumMethods = {
	(binaryfunc) Quaternion_add,				/* __add__ */
	(binaryfunc) Quaternion_sub,				/* __sub__ */
	(binaryfunc) Quaternion_mul,				/* __mul__ */
	(binaryfunc) 0,								/* __div__ */
	(binaryfunc) 0,								/* __mod__ */
	(binaryfunc) 0,								/* __divmod__ */
	(ternaryfunc) 0,							/* __pow__ */
	(unaryfunc) 0,								/* __neg__ */
	(unaryfunc) 0,								/* __pos__ */
	(unaryfunc) 0,								/* __abs__ */
	(inquiry) 0,								/* __nonzero__ */
	(unaryfunc) 0,								/* __invert__ */
	(binaryfunc) 0,								/* __lshift__ */
	(binaryfunc) 0,								/* __rshift__ */
	(binaryfunc) 0,								/* __and__ */
	(binaryfunc) 0,								/* __xor__ */
	(binaryfunc) 0,								/* __or__ */
	/*(coercion)*/  0,								/* __coerce__ */
	(unaryfunc) 0,								/* __int__ */
	(unaryfunc) 0,								/* __long__ */
	(unaryfunc) 0,								/* __float__ */
	(unaryfunc) 0,								/* __oct__ */
	(unaryfunc) 0,								/* __hex__ */

};


static PyObject *Quaternion_getAxis( QuaternionObject * self, void *type )
{
	switch( (long)type ) {
    case 'W':
		return PyFloat_FromDouble(self->quat[0]);
    case 'X':
		return PyFloat_FromDouble(self->quat[1]);
    case 'Y':
		return PyFloat_FromDouble(self->quat[2]);
    case 'Z':
		return PyFloat_FromDouble(self->quat[3]);
	}
	
	PyErr_SetString(PyExc_SystemError, "corrupt quaternion, cannot get axis");
	return NULL;
}

static int Quaternion_setAxis( QuaternionObject * self, PyObject * value, void * type )
{
	float param= (float)PyFloat_AsDouble( value );
	
	if (param==-1 && PyErr_Occurred()) {
		PyErr_SetString( PyExc_TypeError, "expected a number for the vector axis" );
		return -1;
	}
	switch( (long)type ) {
    case 'W':
		self->quat[0]= param;
		break;
    case 'X':
		self->quat[1]= param;
		break;
    case 'Y':
		self->quat[2]= param;
		break;
    case 'Z':
		self->quat[3]= param;
		break;
	}

	return 0;
}

static PyObject *Quaternion_getWrapped( QuaternionObject * self, void *type )
{
	if (self->wrapped == Py_WRAP)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}

static PyObject *Quaternion_getMagnitude( QuaternionObject * self, void *type )
{
	double mag = 0.0;
	int i;
	for(i = 0; i < 4; i++) {
		mag += self->quat[i] * self->quat[i];
	}
	return PyFloat_FromDouble(sqrt(mag));
}

static PyObject *Quaternion_getAngle( QuaternionObject * self, void *type )
{
	double ang = self->quat[0];
	ang = 2 * (saacos(ang));
	ang *= (180 / Py_PI);
	return PyFloat_FromDouble(ang);
}

static PyObject *Quaternion_getAxisVec( QuaternionObject * self, void *type )
{
	int i;
	float vec[3];
	double mag = self->quat[0] * (Py_PI / 180);
	mag = 2 * (saacos(mag));
	mag = sin(mag / 2);
	for(i = 0; i < 3; i++)
		vec[i] = (float)(self->quat[i + 1] / mag);
	
	Normalize(vec);
	//If the axis of rotation is 0,0,0 set it to 1,0,0 - for zero-degree rotations
	if( EXPP_FloatsAreEqual(vec[0], 0.0f, 10) &&
		EXPP_FloatsAreEqual(vec[1], 0.0f, 10) &&
		EXPP_FloatsAreEqual(vec[2], 0.0f, 10) ){
		vec[0] = 1.0f;
	}
	return (PyObject *) newVectorObject(vec, 3, Py_NEW);
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Quaternion_getseters[] = {
	{"w",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion W value",
	 (void *)'W'},
	{"x",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion X axis",
	 (void *)'X'},
	{"y",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion Y axis",
	 (void *)'Y'},
	{"z",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion Z axis",
	 (void *)'Z'},
	{"magnitude",
	 (getter)Quaternion_getMagnitude, (setter)NULL,
	 "Size of the quaternion",
	 NULL},
	{"angle",
	 (getter)Quaternion_getAngle, (setter)NULL,
	 "angle of the quaternion",
	 NULL},
	{"axis",
	 (getter)Quaternion_getAxisVec, (setter)NULL,
	 "quaternion axis as a vector",
	 NULL},
	{"wrapped",
	 (getter)Quaternion_getWrapped, (setter)NULL,
	 "True when this wraps blenders internal data",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


//------------------PY_OBECT DEFINITION--------------------------
PyTypeObject quaternion_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	"quaternion",						//tp_name
	sizeof(QuaternionObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)Quaternion_dealloc,		//tp_dealloc
	0,								//tp_print
	0,								//tp_getattr
	0,								//tp_setattr
	0,								//tp_compare
	(reprfunc) Quaternion_repr,			//tp_repr
	&Quaternion_NumMethods,				//tp_as_number
	&Quaternion_SeqMethods,				//tp_as_sequence
	0,								//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT,				//tp_flags
	QuaternionObject_doc,				//tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	(richcmpfunc)Quaternion_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	0,								//tp_iter
	0,								//tp_iternext
	Quaternion_methods,				//tp_methods
	0,								//tp_members
	Quaternion_getseters,			//tp_getset
	0,								//tp_base
	0,								//tp_dict
	0,								//tp_descr_get
	0,								//tp_descr_set
	0,								//tp_dictoffset
	0,								//tp_init
	0,								//tp_alloc
	Quaternion_new,					//tp_new
	0,								//tp_free
	0,								//tp_is_gc
	0,								//tp_bases
	0,								//tp_mro
	0,								//tp_cache
	0,								//tp_subclasses
	0,								//tp_weaklist
	0								//tp_del
};
//------------------------newQuaternionObject (internal)-------------
//creates a new quaternion object
/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newQuaternionObject(float *quat, int type)
{
	QuaternionObject *self;
	int x;
	
	self = PyObject_NEW(QuaternionObject, &quaternion_Type);
	self->data.blend_data = NULL;
	self->data.py_data = NULL;

	if(type == Py_WRAP){
		self->data.blend_data = quat;
		self->quat = self->data.blend_data;
		self->wrapped = Py_WRAP;
	}else if (type == Py_NEW){
		self->data.py_data = PyMem_Malloc(4 * sizeof(float));
		self->quat = self->data.py_data;
		if(!quat) { //new empty
			Quaternion_Identity(self);
			Py_DECREF(self);
		}else{
			for(x = 0; x < 4; x++){
				self->quat[x] = quat[x];
			}
		}
		self->wrapped = Py_NEW;
	}else{ //bad type
		return NULL;
	}
	return (PyObject *) self;
}
