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

#include "BLI_math.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"


//-------------------------DOC STRINGS ---------------------------

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
	{"identity", (PyCFunction) Quaternion_Identity, METH_NOARGS, NULL},
	{"negate", (PyCFunction) Quaternion_Negate, METH_NOARGS, NULL},
	{"conjugate", (PyCFunction) Quaternion_Conjugate, METH_NOARGS, NULL},
	{"inverse", (PyCFunction) Quaternion_Inverse, METH_NOARGS, NULL},
	{"normalize", (PyCFunction) Quaternion_Normalize, METH_NOARGS, NULL},
	{"toEuler", (PyCFunction) Quaternion_ToEuler, METH_VARARGS, NULL},
	{"toMatrix", (PyCFunction) Quaternion_ToMatrix, METH_NOARGS, NULL},
	{"cross", (PyCFunction) Quaternion_Cross, METH_O, NULL},
	{"dot", (PyCFunction) Quaternion_Dot, METH_O, NULL},
	{"__copy__", (PyCFunction) Quaternion_copy, METH_NOARGS, NULL},
	{"copy", (PyCFunction) Quaternion_copy, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};

//----------------------------------Mathutils.Quaternion() --------------
static PyObject *Quaternion_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *listObject = NULL, *n, *q;
	int size, i;
	float quat[4];
	double angle = 0.0f;

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
		return newQuaternionObject(NULL, Py_NEW, NULL);
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

		quat[i] = PyFloat_AsDouble(q);
		Py_DECREF(q);

		if (quat[i]==-1 && PyErr_Occurred()) {
			PyErr_SetString(PyExc_TypeError, "Mathutils.Quaternion(): 4d numeric sequence expected or 3d vector and number\n");
			return NULL;
		}
	}

	if(size == 3) //calculate the quat based on axis/angle
#ifdef USE_MATHUTILS_DEG
		axis_angle_to_quat(quat, quat, angle * (Py_PI / 180));
#else
		axis_angle_to_quat(quat, quat, angle);
#endif

	return newQuaternionObject(quat, Py_NEW, NULL);
}

//-----------------------------METHODS------------------------------
//----------------------------Quaternion.toEuler()------------------
//return the quat as a euler
static PyObject *Quaternion_ToEuler(QuaternionObject * self, PyObject *args)
{
	float eul[3];
	EulerObject *eul_compat = NULL;
	
	if(!PyArg_ParseTuple(args, "|O!:toEuler", &euler_Type, &eul_compat))
		return NULL;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(eul_compat) {
		float mat[3][3];
		
		if(!BaseMath_ReadCallback(eul_compat))
			return NULL;
		
		quat_to_mat3( mat,self->quat);

#ifdef USE_MATHUTILS_DEG
		{
			float  eul_compatf[3];
			int x;

			for(x = 0; x < 3; x++) {
				eul_compatf[x] = eul_compat->eul[x] * ((float)Py_PI / 180);
			}
			mat3_to_compatible_eul( eul, eul_compatf,mat);
		}
#else
		mat3_to_compatible_eul( eul, eul_compat->eul,mat);
#endif
	}
	else {
		quat_to_eul( eul,self->quat);
	}
	
#ifdef USE_MATHUTILS_DEG
	{
		int x;

		for(x = 0; x < 3; x++) {
			eul[x] *= (180 / (float)Py_PI);
		}
	}
#endif
	return newEulerObject(eul, Py_NEW, NULL);
}
//----------------------------Quaternion.toMatrix()------------------
//return the quat as a matrix
static PyObject *Quaternion_ToMatrix(QuaternionObject * self)
{
	float mat[9]; /* all values are set */

	if(!BaseMath_ReadCallback(self))
		return NULL;

	quat_to_mat3( (float (*)[3]) mat,self->quat);
	return newMatrixObject(mat, 3, 3, Py_NEW, NULL);
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
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	mul_qt_qtqt(quat, self->quat, value->quat);
	return newQuaternionObject(quat, Py_NEW, NULL);
}

//----------------------------Quaternion.dot(other)------------------
//return the dot quat
static PyObject *Quaternion_Dot(QuaternionObject * self, QuaternionObject * value)
{
	if (!QuaternionObject_Check(value)) {
		PyErr_SetString( PyExc_TypeError, "quat.dot(value): expected a quaternion argument" );
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	return PyFloat_FromDouble(dot_qtqt(self->quat, value->quat));
}

//----------------------------Quaternion.normalize()----------------
//normalize the axis of rotation of [theta,vector]
static PyObject *Quaternion_Normalize(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	normalize_qt(self->quat);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.inverse()------------------
//invert the quat
static PyObject *Quaternion_Inverse(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	invert_qt(self->quat);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.identity()-----------------
//generate the identity quaternion
static PyObject *Quaternion_Identity(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	unit_qt(self->quat);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.negate()-------------------
//negate the quat
static PyObject *Quaternion_Negate(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	mul_qt_fl(self->quat, -1.0f);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.conjugate()----------------
//negate the vector part
static PyObject *Quaternion_Conjugate(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	conjugate_qt(self->quat);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.copy()----------------
//return a copy of the quat
static PyObject *Quaternion_copy(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	return newQuaternionObject(self->quat, Py_NEW, Py_TYPE(self));
}

//----------------------------print object (internal)--------------
//print the object to screen
static PyObject *Quaternion_repr(QuaternionObject * self)
{
	char str[64];

	if(!BaseMath_ReadCallback(self))
		return NULL;

	sprintf(str, "[%.6f, %.6f, %.6f, %.6f](quaternion)", self->quat[0], self->quat[1], self->quat[2], self->quat[3]);
	return PyUnicode_FromString(str);
}
//------------------------tp_richcmpr
//returns -1 execption, 0 false, 1 true
static PyObject* Quaternion_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	QuaternionObject *quatA = NULL, *quatB = NULL;
	int result = 0;

	if(QuaternionObject_Check(objectA)) {
		quatA = (QuaternionObject*)objectA;
		if(!BaseMath_ReadCallback(quatA))
			return NULL;
	}
	if(QuaternionObject_Check(objectB)) {
		quatB = (QuaternionObject*)objectB;
		if(!BaseMath_ReadCallback(quatB))
			return NULL;
	}

	if (!quatA || !quatB){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}

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
	if(i<0)	i= 4-i;

	if(i < 0 || i >= 4) {
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute]: array index out of range\n");
		return NULL;
	}

	if(!BaseMath_ReadIndexCallback(self, i))
		return NULL;

	return PyFloat_FromDouble(self->quat[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Quaternion_ass_item(QuaternionObject * self, int i, PyObject * ob)
{
	float scalar= (float)PyFloat_AsDouble(ob);
	if(scalar==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "quaternion[index] = x: index argument not a number\n");
		return -1;
	}

	if(i<0)	i= 4-i;

	if(i < 0 || i >= 4){
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute] = x: array assignment index out of range\n");
		return -1;
	}
	self->quat[i] = scalar;

	if(!BaseMath_WriteIndexCallback(self, i))
		return -1;

	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Quaternion_slice(QuaternionObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	if(!BaseMath_ReadCallback(self))
		return NULL;

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
static int Quaternion_ass_slice(QuaternionObject * self, int begin, int end, PyObject * seq)
{
	int i, y, size = 0;
	float quat[4];
	PyObject *q;

	if(!BaseMath_ReadCallback(self))
		return -1;

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

		quat[i]= (float)PyFloat_AsDouble(q);
		Py_DECREF(q);

		if(quat[i]==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
			PyErr_SetString(PyExc_TypeError, "quaternion[begin:end] = []: sequence argument not a number\n");
			return -1;
		}
	}
	//parsed well - now set in vector
	for(y = 0; y < size; y++)
		self->quat[begin + y] = quat[y];

	BaseMath_WriteCallback(self);
	return 0;
}
//------------------------NUMERIC PROTOCOLS----------------------
//------------------------obj + obj------------------------------
//addition
static PyObject *Quaternion_add(PyObject * q1, PyObject * q2)
{
	float quat[4];
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

	if(!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
		PyErr_SetString(PyExc_AttributeError, "Quaternion addition: arguments not valid for this operation....\n");
		return NULL;
	}
	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;
	
	if(!BaseMath_ReadCallback(quat1) || !BaseMath_ReadCallback(quat2))
		return NULL;

	add_qt_qtqt(quat, quat1->quat, quat2->quat, 1.0f);
	return newQuaternionObject(quat, Py_NEW, NULL);
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
	
	if(!BaseMath_ReadCallback(quat1) || !BaseMath_ReadCallback(quat2))
		return NULL;

	for(x = 0; x < 4; x++) {
		quat[x] = quat1->quat[x] - quat2->quat[x];
	}

	return newQuaternionObject(quat, Py_NEW, NULL);
}
//------------------------obj * obj------------------------------
//mulplication
static PyObject *Quaternion_mul(PyObject * q1, PyObject * q2)
{
	float quat[4], scalar;
	QuaternionObject *quat1 = NULL, *quat2 = NULL;
	VectorObject *vec = NULL;

	if(QuaternionObject_Check(q1)) {
		quat1 = (QuaternionObject*)q1;
		if(!BaseMath_ReadCallback(quat1))
			return NULL;
	}
	if(QuaternionObject_Check(q2)) {
		quat2 = (QuaternionObject*)q2;
		if(!BaseMath_ReadCallback(quat2))
			return NULL;
	}

	if(quat1 && quat2) { /* QUAT*QUAT (dot product) */
		return PyFloat_FromDouble(dot_qtqt(quat1->quat, quat2->quat));
	}
	
	/* the only case this can happen (for a supported type is "FLOAT*QUAT" ) */
	if(!QuaternionObject_Check(q1)) {
		scalar= PyFloat_AsDouble(q1);
		if ((scalar == -1.0 && PyErr_Occurred())==0) { /* FLOAT*QUAT */
			QUATCOPY(quat, quat2->quat);
			mul_qt_fl(quat, scalar);
			return newQuaternionObject(quat, Py_NEW, NULL);
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
			return quat_rotation((PyObject*)quat1, (PyObject*)vec); /* vector updating done inside the func */
		}
		
		scalar= PyFloat_AsDouble(q2);
		if ((scalar == -1.0 && PyErr_Occurred())==0) { /* QUAT*FLOAT */
			QUATCOPY(quat, quat1->quat);
			mul_qt_fl(quat, scalar);
			return newQuaternionObject(quat, Py_NEW, NULL);
		}
	}
	
	PyErr_SetString(PyExc_TypeError, "Quaternion multiplication: arguments not acceptable for this operation\n");
	return NULL;
}

//-----------------PROTOCOL DECLARATIONS--------------------------
static PySequenceMethods Quaternion_SeqMethods = {
	(lenfunc) Quaternion_len,					/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(ssizeargfunc) 0,								/* sq_repeat */
	(ssizeargfunc) Quaternion_item,				/* sq_item */
	(ssizessizeargfunc) Quaternion_slice,			/* sq_slice */
	(ssizeobjargproc) Quaternion_ass_item,		/* sq_ass_item */
	(ssizessizeobjargproc) Quaternion_ass_slice,	/* sq_ass_slice */
};

static PyNumberMethods Quaternion_NumMethods = {
		(binaryfunc)	Quaternion_add,	/*nb_add*/
		(binaryfunc)	Quaternion_sub,	/*nb_subtract*/
		(binaryfunc)	Quaternion_mul,	/*nb_multiply*/
		0,							/*nb_remainder*/
		0,							/*nb_divmod*/
		0,							/*nb_power*/
		(unaryfunc) 	0,	/*nb_negative*/
		(unaryfunc) 	0,	/*tp_positive*/
		(unaryfunc) 	0,	/*tp_absolute*/
		(inquiry)	0,	/*tp_bool*/
		(unaryfunc)	0,	/*nb_invert*/
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

static PyObject *Quaternion_getAxis( QuaternionObject * self, void *type )
{
	return Quaternion_item(self, GET_INT_FROM_POINTER(type));
}

static int Quaternion_setAxis( QuaternionObject * self, PyObject * value, void * type )
{
	return Quaternion_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

static PyObject *Quaternion_getMagnitude( QuaternionObject * self, void *type )
{
	return PyFloat_FromDouble(sqrt(dot_qtqt(self->quat, self->quat)));
}

static PyObject *Quaternion_getAngle( QuaternionObject * self, void *type )
{
	double ang = self->quat[0];
	ang = 2 * (saacos(ang));
#ifdef USE_MATHUTILS_DEG
	ang *= (180 / Py_PI);
#endif
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
	
	normalize_v3(vec);
	//If the axis of rotation is 0,0,0 set it to 1,0,0 - for zero-degree rotations
	if( EXPP_FloatsAreEqual(vec[0], 0.0f, 10) &&
		EXPP_FloatsAreEqual(vec[1], 0.0f, 10) &&
		EXPP_FloatsAreEqual(vec[2], 0.0f, 10) ){
		vec[0] = 1.0f;
	}
	return (PyObject *) newVectorObject(vec, 3, Py_NEW, NULL);
}


/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Quaternion_getseters[] = {
	{"w",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion W value",
	 (void *)0},
	{"x",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion X axis",
	 (void *)1},
	{"y",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion Y axis",
	 (void *)2},
	{"z",
	 (getter)Quaternion_getAxis, (setter)Quaternion_setAxis,
	 "Quaternion Z axis",
	 (void *)3},
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
	 (getter)BaseMathObject_getWrapped, (setter)NULL,
	 "True when this wraps blenders internal data",
	 NULL},
	{"_owner",
	 (getter)BaseMathObject_getOwner, (setter)NULL,
	 "Read only owner for vectors that depend on another object",
	 NULL},

	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


//------------------PY_OBECT DEFINITION--------------------------
PyTypeObject quaternion_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"quaternion",						//tp_name
	sizeof(QuaternionObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)BaseMathObject_dealloc,		//tp_dealloc
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
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, //tp_flags
	0,								//tp_doc
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
PyObject *newQuaternionObject(float *quat, int type, PyTypeObject *base_type)
{
	QuaternionObject *self;
	
	if(base_type)	self = (QuaternionObject *)base_type->tp_alloc(base_type, 0);
	else			self = PyObject_NEW(QuaternionObject, &quaternion_Type);

	/* init callbacks as NULL */
	self->cb_user= NULL;
	self->cb_type= self->cb_subtype= 0;

	if(type == Py_WRAP){
		self->quat = quat;
		self->wrapped = Py_WRAP;
	}else if (type == Py_NEW){
		self->quat = PyMem_Malloc(4 * sizeof(float));
		if(!quat) { //new empty
			unit_qt(self->quat);
		}else{
			QUATCOPY(self->quat, quat);
		}
		self->wrapped = Py_NEW;
	}else{ //bad type
		return NULL;
	}
	return (PyObject *) self;
}

PyObject *newQuaternionObject_cb(PyObject *cb_user, int cb_type, int cb_subtype)
{
	QuaternionObject *self= (QuaternionObject *)newQuaternionObject(NULL, Py_NEW, NULL);
	if(self) {
		Py_INCREF(cb_user);
		self->cb_user=			cb_user;
		self->cb_type=			(unsigned char)cb_type;
		self->cb_subtype=		(unsigned char)cb_subtype;
	}

	return (PyObject *)self;
}
