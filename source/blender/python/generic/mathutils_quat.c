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
 * 
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"



#define QUAT_SIZE 4

//-----------------------------METHODS------------------------------

/* note: BaseMath_ReadCallback must be called beforehand */
static PyObject *Quaternion_ToTupleExt(QuaternionObject *self, int ndigits)
{
	PyObject *ret;
	int i;

	ret= PyTuple_New(QUAT_SIZE);

	if(ndigits >= 0) {
		for(i= 0; i < QUAT_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->quat[i], ndigits)));
		}
	}
	else {
		for(i= 0; i < QUAT_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->quat[i]));
		}
	}

	return ret;
}

static char Quaternion_ToEuler_doc[] =
".. method:: to_euler(order, euler_compat)\n"
"\n"
"   Return Euler representation of the quaternion.\n"
"\n"
"   :arg order: Optional rotation order argument in ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX'].\n"
"   :type order: string\n"
"   :arg euler_compat: Optional euler argument the new euler will be made compatible with (no axis flipping between them). Useful for converting a series of matrices to animation curves.\n"
"   :type euler_compat: :class:`Euler`\n"
"   :return: Euler representation of the quaternion.\n"
"   :rtype: :class:`Euler`\n";

static PyObject *Quaternion_ToEuler(QuaternionObject * self, PyObject *args)
{
	float tquat[4];
	float eul[3];
	char *order_str= NULL;
	short order= EULER_ORDER_XYZ;
	EulerObject *eul_compat = NULL;
	
	if(!PyArg_ParseTuple(args, "|sO!:to_euler", &order_str, &euler_Type, &eul_compat))
		return NULL;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(order_str) {
		order= euler_order_from_string(order_str, "Matrix.to_euler()");

		if(order == -1)
			return NULL;
	}
	
	normalize_qt_qt(tquat, self->quat);

	if(eul_compat) {
		float mat[3][3];
		
		if(!BaseMath_ReadCallback(eul_compat))
			return NULL;
		
		quat_to_mat3(mat, tquat);

		if(order == EULER_ORDER_XYZ)	mat3_to_compatible_eul(eul, eul_compat->eul, mat);
		else							mat3_to_compatible_eulO(eul, eul_compat->eul, order, mat);
	}
	else {
		if(order == EULER_ORDER_XYZ)	quat_to_eul(eul, tquat);
		else							quat_to_eulO(eul, order, tquat);
	}
	
	return newEulerObject(eul, order, Py_NEW, NULL);
}
//----------------------------Quaternion.toMatrix()------------------
static char Quaternion_ToMatrix_doc[] =
".. method:: to_matrix()\n"
"\n"
"   Return a matrix representation of the quaternion.\n"
"\n"
"   :return: A 3x3 rotation matrix representation of the quaternion.\n"
"   :rtype: :class:`Matrix`\n";

static PyObject *Quaternion_ToMatrix(QuaternionObject * self)
{
	float mat[9]; /* all values are set */

	if(!BaseMath_ReadCallback(self))
		return NULL;

	quat_to_mat3( (float (*)[3]) mat,self->quat);
	return newMatrixObject(mat, 3, 3, Py_NEW, NULL);
}

//----------------------------Quaternion.cross(other)------------------
static char Quaternion_Cross_doc[] =
".. method:: cross(other)\n"
"\n"
"   Return the cross product of this quaternion and another.\n"
"\n"
"   :arg other: The other quaternion to perform the cross product with.\n"
"   :type other: :class:`Quaternion`\n"
"   :return: The cross product.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Cross(QuaternionObject *self, QuaternionObject *value)
{
	float quat[QUAT_SIZE];
	
	if (!QuaternionObject_Check(value)) {
		PyErr_Format(PyExc_TypeError, "quat.cross(value): expected a quaternion argument, not %.200s", Py_TYPE(value)->tp_name);
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	mul_qt_qtqt(quat, self->quat, value->quat);
	return newQuaternionObject(quat, Py_NEW, Py_TYPE(self));
}

//----------------------------Quaternion.dot(other)------------------
static char Quaternion_Dot_doc[] =
".. method:: dot(other)\n"
"\n"
"   Return the dot product of this quaternion and another.\n"
"\n"
"   :arg other: The other quaternion to perform the dot product with.\n"
"   :type other: :class:`Quaternion`\n"
"   :return: The dot product.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Dot(QuaternionObject * self, QuaternionObject * value)
{
	if (!QuaternionObject_Check(value)) {
		PyErr_Format(PyExc_TypeError, "quat.dot(value): expected a quaternion argument, not %.200s", Py_TYPE(value)->tp_name);
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	return PyFloat_FromDouble(dot_qtqt(self->quat, value->quat));
}

static char Quaternion_Difference_doc[] =
".. function:: difference(other)\n"
"\n"
"   Returns a quaternion representing the rotational difference.\n"
"\n"
"   :arg other: second quaternion.\n"
"   :type other: :class:`Quaternion`\n"
"   :return: the rotational difference between the two quat rotations.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Difference(QuaternionObject * self, QuaternionObject * value)
{
	float quat[QUAT_SIZE];

	if (!QuaternionObject_Check(value)) {
		PyErr_Format(PyExc_TypeError, "quat.difference(value): expected a quaternion argument, not %.200s", Py_TYPE(value)->tp_name);
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	rotation_between_quats_to_quat(quat, self->quat, value->quat);

	return newQuaternionObject(quat, Py_NEW, Py_TYPE(self));
}

static char Quaternion_Slerp_doc[] =
".. function:: slerp(other, factor)\n"
"\n"
"   Returns the interpolation of two quaternions.\n"
"\n"
"   :arg other: value to interpolate with.\n"
"   :type other: :class:`Quaternion`\n"
"   :arg factor: The interpolation value in [0.0, 1.0].\n"
"   :type factor: float\n"
"   :return: The interpolated rotation.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Slerp(QuaternionObject *self, PyObject *args)
{
	QuaternionObject *value;
	float quat[QUAT_SIZE], fac;

	if(!PyArg_ParseTuple(args, "O!f:slerp", &quaternion_Type, &value, &fac)) {
		PyErr_SetString(PyExc_TypeError, "quat.slerp(): expected Quaternion types and float");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	if(fac > 1.0f || fac < 0.0f) {
		PyErr_SetString(PyExc_AttributeError, "quat.slerp(): interpolation factor must be between 0.0 and 1.0");
		return NULL;
	}

	interp_qt_qtqt(quat, self->quat, value->quat, fac);

	return newQuaternionObject(quat, Py_NEW, Py_TYPE(self));
}

//----------------------------Quaternion.normalize()----------------
//normalize the axis of rotation of [theta,vector]
static char Quaternion_Normalize_doc[] =
".. function:: normalize()\n"
"\n"
"   Normalize the quaternion.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Normalize(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	normalize_qt(self->quat);

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.inverse()------------------
static char Quaternion_Inverse_doc[] =
".. function:: inverse()\n"
"\n"
"   Set the quaternion to its inverse.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Inverse(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	invert_qt(self->quat);

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.identity()-----------------
static char Quaternion_Identity_doc[] =
".. function:: identity()\n"
"\n"
"   Set the quaternion to an identity quaternion.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Identity(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	unit_qt(self->quat);

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.negate()-------------------
static char Quaternion_Negate_doc[] =
".. function:: negate()\n"
"\n"
"   Set the quaternion to its negative.\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Negate(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	mul_qt_fl(self->quat, -1.0f);

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.conjugate()----------------
static char Quaternion_Conjugate_doc[] =
".. function:: conjugate()\n"
"\n"
"   Set the quaternion to its conjugate (negate x, y, z).\n"
"\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Quaternion_Conjugate(QuaternionObject * self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	conjugate_qt(self->quat);

	(void)BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject*)self;
}
//----------------------------Quaternion.copy()----------------
static char Quaternion_copy_doc[] =
".. function:: copy()\n"
"\n"
"   Returns a copy of this quaternion.\n"
"\n"
"   :return: A copy of the quaternion.\n"
"   :rtype: :class:`Quaternion`\n"
"\n"
"   .. note:: use this to get a copy of a wrapped quaternion with no reference to the original data.\n";

static PyObject *Quaternion_copy(QuaternionObject *self)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	return newQuaternionObject(self->quat, Py_NEW, Py_TYPE(self));
}

//----------------------------print object (internal)--------------
//print the object to screen
static PyObject *Quaternion_repr(QuaternionObject * self)
{
	PyObject *ret, *tuple;
	
	if(!BaseMath_ReadCallback(self))
		return NULL;

	tuple= Quaternion_ToTupleExt(self, -1);

	ret= PyUnicode_FromFormat("Quaternion(%R)", tuple);

	Py_DECREF(tuple);
	return ret;
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
			result = EXPP_VectorsAreEqual(quatA->quat, quatB->quat, QUAT_SIZE, 1);
			break;
		case Py_NE:
			result = EXPP_VectorsAreEqual(quatA->quat, quatB->quat, QUAT_SIZE, 1);
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
static int Quaternion_len(QuaternionObject *UNUSED(self))
{
	return QUAT_SIZE;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Quaternion_item(QuaternionObject * self, int i)
{
	if(i<0)	i= QUAT_SIZE-i;

	if(i < 0 || i >= QUAT_SIZE) {
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute]: array index out of range");
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
		PyErr_SetString(PyExc_TypeError, "quaternion[index] = x: index argument not a number");
		return -1;
	}

	if(i<0)	i= QUAT_SIZE-i;

	if(i < 0 || i >= QUAT_SIZE){
		PyErr_SetString(PyExc_IndexError, "quaternion[attribute] = x: array assignment index out of range");
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
	PyObject *tuple;
	int count;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	CLAMP(begin, 0, QUAT_SIZE);
	if (end<0) end= (QUAT_SIZE + 1) + end;
	CLAMP(end, 0, QUAT_SIZE);
	begin= MIN2(begin, end);

	tuple= PyTuple_New(end - begin);
	for(count= begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->quat[count]));
	}

	return tuple;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Quaternion_ass_slice(QuaternionObject * self, int begin, int end, PyObject * seq)
{
	int i, size;
	float quat[QUAT_SIZE];

	if(!BaseMath_ReadCallback(self))
		return -1;

	CLAMP(begin, 0, QUAT_SIZE);
	if (end<0) end= (QUAT_SIZE + 1) + end;
	CLAMP(end, 0, QUAT_SIZE);
	begin = MIN2(begin,end);

	if((size=mathutils_array_parse(quat, 0, QUAT_SIZE, seq, "mathutils.Quaternion[begin:end] = []")) == -1)
		return -1;
	
	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "quaternion[begin:end] = []: size mismatch in slice assignment");
		return -1;
	}

	/* parsed well - now set in vector */
	for(i= 0; i < size; i++)
		self->quat[begin + i] = quat[i];

	(void)BaseMath_WriteCallback(self);
	return 0;
}


static PyObject *Quaternion_subscript(QuaternionObject *self, PyObject *item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += QUAT_SIZE;
		return Quaternion_item(self, i);
	} else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, QUAT_SIZE, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyList_New(0);
		}
		else if (step == 1) {
			return Quaternion_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with quaternions");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "quaternion indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return NULL;
	}
}


static int Quaternion_ass_subscript(QuaternionObject *self, PyObject *item, PyObject *value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += QUAT_SIZE;
		return Quaternion_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, QUAT_SIZE, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Quaternion_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with quaternion");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "quaternion indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return -1;
	}
}

//------------------------NUMERIC PROTOCOLS----------------------
//------------------------obj + obj------------------------------
//addition
static PyObject *Quaternion_add(PyObject * q1, PyObject * q2)
{
	float quat[QUAT_SIZE];
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

	if(!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
		PyErr_SetString(PyExc_AttributeError, "Quaternion addition: arguments not valid for this operation");
		return NULL;
	}
	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;
	
	if(!BaseMath_ReadCallback(quat1) || !BaseMath_ReadCallback(quat2))
		return NULL;

	add_qt_qtqt(quat, quat1->quat, quat2->quat, 1.0f);
	return newQuaternionObject(quat, Py_NEW, Py_TYPE(q1));
}
//------------------------obj - obj------------------------------
//subtraction
static PyObject *Quaternion_sub(PyObject * q1, PyObject * q2)
{
	int x;
	float quat[QUAT_SIZE];
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

	if(!QuaternionObject_Check(q1) || !QuaternionObject_Check(q2)) {
		PyErr_SetString(PyExc_AttributeError, "Quaternion addition: arguments not valid for this operation");
		return NULL;
	}
	
	quat1 = (QuaternionObject*)q1;
	quat2 = (QuaternionObject*)q2;
	
	if(!BaseMath_ReadCallback(quat1) || !BaseMath_ReadCallback(quat2))
		return NULL;

	for(x = 0; x < QUAT_SIZE; x++) {
		quat[x] = quat1->quat[x] - quat2->quat[x];
	}

	return newQuaternionObject(quat, Py_NEW, Py_TYPE(q1));
}

static PyObject *quat_mul_float(QuaternionObject *quat, const float scalar)
{
	float tquat[4];
	copy_qt_qt(tquat, quat->quat);
	mul_qt_fl(tquat, scalar);
	return newQuaternionObject(tquat, Py_NEW, Py_TYPE(quat));
}

//------------------------obj * obj------------------------------
//mulplication
static PyObject *Quaternion_mul(PyObject * q1, PyObject * q2)
{
	float quat[QUAT_SIZE], scalar;
	QuaternionObject *quat1 = NULL, *quat2 = NULL;

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

	if(quat1 && quat2) { /* QUAT*QUAT (cross product) */
		mul_qt_qtqt(quat, quat1->quat, quat2->quat);
		return newQuaternionObject(quat, Py_NEW, Py_TYPE(q1));
	}
	/* the only case this can happen (for a supported type is "FLOAT*QUAT" ) */
	else if(quat2) { /* FLOAT*QUAT */
		if(((scalar= PyFloat_AsDouble(q1)) == -1.0 && PyErr_Occurred())==0) {
			return quat_mul_float(quat2, scalar);
		}
	}
	else if (quat1) { /* QUAT*FLOAT */
		if((((scalar= PyFloat_AsDouble(q2)) == -1.0 && PyErr_Occurred())==0)) {
			return quat_mul_float(quat1, scalar);
		}
	}
	else {
		BLI_assert(!"internal error");
	}

	PyErr_Format(PyExc_TypeError, "Quaternion multiplication: not supported between '%.200s' and '%.200s' types", Py_TYPE(q1)->tp_name, Py_TYPE(q2)->tp_name);
	return NULL;
}

//-----------------PROTOCOL DECLARATIONS--------------------------
static PySequenceMethods Quaternion_SeqMethods = {
	(lenfunc) Quaternion_len,				/* sq_length */
	(binaryfunc) NULL,						/* sq_concat */
	(ssizeargfunc) NULL,					/* sq_repeat */
	(ssizeargfunc) Quaternion_item,			/* sq_item */
	(ssizessizeargfunc) NULL,				/* sq_slice, deprecated */
	(ssizeobjargproc) Quaternion_ass_item,	/* sq_ass_item */
	(ssizessizeobjargproc) NULL,			/* sq_ass_slice, deprecated */
	(objobjproc) NULL,						/* sq_contains */
	(binaryfunc) NULL,						/* sq_inplace_concat */
	(ssizeargfunc) NULL,					/* sq_inplace_repeat */
};

static PyMappingMethods Quaternion_AsMapping = {
	(lenfunc)Quaternion_len,
	(binaryfunc)Quaternion_subscript,
	(objobjargproc)Quaternion_ass_subscript
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

static PyObject *Quaternion_getMagnitude(QuaternionObject * self, void *UNUSED(closure))
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	return PyFloat_FromDouble(sqrt(dot_qtqt(self->quat, self->quat)));
}

static PyObject *Quaternion_getAngle(QuaternionObject * self, void *UNUSED(closure))
{
	float tquat[4];

	if(!BaseMath_ReadCallback(self))
		return NULL;

	normalize_qt_qt(tquat, self->quat);
	return PyFloat_FromDouble(2.0 * (saacos(tquat[0])));
}

static int Quaternion_setAngle(QuaternionObject * self, PyObject * value, void *UNUSED(closure))
{
	float tquat[4];
	float len;
	
	float axis[3], angle_dummy;
	double angle;

	if(!BaseMath_ReadCallback(self))
		return -1;

	len= normalize_qt_qt(tquat, self->quat);
	quat_to_axis_angle(axis, &angle_dummy, tquat);

	angle= PyFloat_AsDouble(value);

	if(angle==-1.0f && PyErr_Occurred()) { /* parsed item not a number */
		PyErr_SetString(PyExc_TypeError, "quaternion.angle = value: float expected");
		return -1;
	}

	angle= fmod(angle + M_PI*2, M_PI*4) - M_PI*2;

	/* If the axis of rotation is 0,0,0 set it to 1,0,0 - for zero-degree rotations */
	if( EXPP_FloatsAreEqual(axis[0], 0.0f, 10) &&
		EXPP_FloatsAreEqual(axis[1], 0.0f, 10) &&
		EXPP_FloatsAreEqual(axis[2], 0.0f, 10)
	) {
		axis[0] = 1.0f;
	}
	
	axis_angle_to_quat(self->quat, axis, angle);
	mul_qt_fl(self->quat, len);

	if(!BaseMath_WriteCallback(self))
		return -1;

	return 0;
}

static PyObject *Quaternion_getAxisVec(QuaternionObject *self, void *UNUSED(closure))
{
	float tquat[4];
	
	float axis[3];
	float angle;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	normalize_qt_qt(tquat, self->quat);
	quat_to_axis_angle(axis, &angle, tquat);

	/* If the axis of rotation is 0,0,0 set it to 1,0,0 - for zero-degree rotations */
	if( EXPP_FloatsAreEqual(axis[0], 0.0f, 10) &&
		EXPP_FloatsAreEqual(axis[1], 0.0f, 10) &&
		EXPP_FloatsAreEqual(axis[2], 0.0f, 10)
	) {
		axis[0] = 1.0f;
	}

	return (PyObject *) newVectorObject(axis, 3, Py_NEW, NULL);
}

static int Quaternion_setAxisVec(QuaternionObject *self, PyObject *value, void *UNUSED(closure))
{
	float tquat[4];
	float len;

	float axis[3];
	float angle;
	
	VectorObject *vec;

	
	if(!BaseMath_ReadCallback(self))
		return -1;

	len= normalize_qt_qt(tquat, self->quat);
	quat_to_axis_angle(axis, &angle, tquat);

	if(!VectorObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "quaternion.axis = value: expected a 3D Vector");
		return -1;
	}
	
	vec= (VectorObject *)value;
	if(!BaseMath_ReadCallback(vec))
		return -1;

	axis_angle_to_quat(self->quat, vec->vec, angle);
	mul_qt_fl(self->quat, len);

	if(!BaseMath_WriteCallback(self))
		return -1;

	return 0;
}

//----------------------------------mathutils.Quaternion() --------------
static PyObject *Quaternion_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *seq= NULL;
	double angle = 0.0f;
	float quat[QUAT_SIZE]= {0.0f, 0.0f, 0.0f, 0.0f};

	if(kwds && PyDict_Size(kwds)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Quaternion(): takes no keyword args");
		return NULL;
	}
	
	if(!PyArg_ParseTuple(args, "|Od:mathutils.Quaternion", &seq, &angle))
		return NULL;

	switch(PyTuple_GET_SIZE(args)) {
	case 0:
		break;
	case 1:
		if (mathutils_array_parse(quat, QUAT_SIZE, QUAT_SIZE, seq, "mathutils.Quaternion()") == -1)
			return NULL;
		break;
	case 2:
		if (mathutils_array_parse(quat, 3, 3, seq, "mathutils.Quaternion()") == -1)
			return NULL;
		angle= fmod(angle + M_PI*2, M_PI*4) - M_PI*2; /* clamp because of precission issues */
		axis_angle_to_quat(quat, quat, angle);
		break;
	/* PyArg_ParseTuple assures no more then 2 */
	}
	return newQuaternionObject(quat, Py_NEW, type);
}


//-----------------------METHOD DEFINITIONS ----------------------
static struct PyMethodDef Quaternion_methods[] = {
	{"identity", (PyCFunction) Quaternion_Identity, METH_NOARGS, Quaternion_Identity_doc},
	{"negate", (PyCFunction) Quaternion_Negate, METH_NOARGS, Quaternion_Negate_doc},
	{"conjugate", (PyCFunction) Quaternion_Conjugate, METH_NOARGS, Quaternion_Conjugate_doc},
	{"inverse", (PyCFunction) Quaternion_Inverse, METH_NOARGS, Quaternion_Inverse_doc},
	{"normalize", (PyCFunction) Quaternion_Normalize, METH_NOARGS, Quaternion_Normalize_doc},
	{"to_euler", (PyCFunction) Quaternion_ToEuler, METH_VARARGS, Quaternion_ToEuler_doc},
	{"to_matrix", (PyCFunction) Quaternion_ToMatrix, METH_NOARGS, Quaternion_ToMatrix_doc},
	{"cross", (PyCFunction) Quaternion_Cross, METH_O, Quaternion_Cross_doc},
	{"dot", (PyCFunction) Quaternion_Dot, METH_O, Quaternion_Dot_doc},
	{"difference", (PyCFunction) Quaternion_Difference, METH_O, Quaternion_Difference_doc},
	{"slerp", (PyCFunction) Quaternion_Slerp, METH_VARARGS, Quaternion_Slerp_doc},
	{"__copy__", (PyCFunction) Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
	{"copy", (PyCFunction) Quaternion_copy, METH_NOARGS, Quaternion_copy_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Quaternion_getseters[] = {
	{(char *)"w", (getter)Quaternion_getAxis, (setter)Quaternion_setAxis, (char *)"Quaternion W value.\n\n:type: float", (void *)0},
	{(char *)"x", (getter)Quaternion_getAxis, (setter)Quaternion_setAxis, (char *)"Quaternion X axis.\n\n:type: float", (void *)1},
	{(char *)"y", (getter)Quaternion_getAxis, (setter)Quaternion_setAxis, (char *)"Quaternion Y axis.\n\n:type: float", (void *)2},
	{(char *)"z", (getter)Quaternion_getAxis, (setter)Quaternion_setAxis, (char *)"Quaternion Z axis.\n\n:type: float", (void *)3},
	{(char *)"magnitude", (getter)Quaternion_getMagnitude, (setter)NULL, (char *)"Size of the quaternion (readonly).\n\n:type: float", NULL},
	{(char *)"angle", (getter)Quaternion_getAngle, (setter)Quaternion_setAngle, (char *)"angle of the quaternion.\n\n:type: float", NULL},
	{(char *)"axis",(getter)Quaternion_getAxisVec, (setter)Quaternion_setAxisVec, (char *)"quaternion axis as a vector.\n\n:type: :class:`Vector`", NULL},
	{(char *)"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, (char *)BaseMathObject_Wrapped_doc, NULL},
	{(char *)"owner", (getter)BaseMathObject_getOwner, (setter)NULL, (char *)BaseMathObject_Owner_doc, NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

//------------------PY_OBECT DEFINITION--------------------------
static char quaternion_doc[] =
"This object gives access to Quaternions in Blender.";

PyTypeObject quaternion_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"mathutils.Quaternion",						//tp_name
	sizeof(QuaternionObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)BaseMathObject_dealloc,		//tp_dealloc
	0,								//tp_print
	0,								//tp_getattr
	0,								//tp_setattr
	0,								//tp_compare
	(reprfunc) Quaternion_repr,		//tp_repr
	&Quaternion_NumMethods,			//tp_as_number
	&Quaternion_SeqMethods,			//tp_as_sequence
	&Quaternion_AsMapping,			//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, //tp_flags
	quaternion_doc, //tp_doc
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
		self->quat = PyMem_Malloc(QUAT_SIZE * sizeof(float));
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

