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

/** \file blender/python/generic/mathutils_Euler.c
 *  \ingroup pygen
 */


#include <Python.h>

#include "mathutils.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#ifndef int32_t
#include "BLO_sys_types.h"
#endif

#define EULER_SIZE 3

//----------------------------------mathutils.Euler() -------------------
//makes a new euler for you to play with
static PyObject *Euler_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *seq= NULL;
	const char *order_str= NULL;

	float eul[EULER_SIZE]= {0.0f, 0.0f, 0.0f};
	short order= EULER_ORDER_XYZ;

	if(kwds && PyDict_Size(kwds)) {
		PyErr_SetString(PyExc_TypeError, "mathutils.Euler(): takes no keyword args");
		return NULL;
	}

	if(!PyArg_ParseTuple(args, "|Os:mathutils.Euler", &seq, &order_str))
		return NULL;

	switch(PyTuple_GET_SIZE(args)) {
	case 0:
		break;
	case 2:
		if((order=euler_order_from_string(order_str, "mathutils.Euler()")) == -1)
			return NULL;
		/* intentionally pass through */
	case 1:
		if (mathutils_array_parse(eul, EULER_SIZE, EULER_SIZE, seq, "mathutils.Euler()") == -1)
			return NULL;
		break;
	}
	return newEulerObject(eul, order, Py_NEW, type);
}

/* internal use, assuem read callback is done */
static const char *euler_order_str(EulerObject *self)
{
	static const char order[][4] = {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
	return order[self->order-EULER_ORDER_XYZ];
}

short euler_order_from_string(const char *str, const char *error_prefix)
{
	if((str[0] && str[1] && str[2] && str[3]=='\0')) {
		switch(*((int32_t *)str)) {
			case 'X'|'Y'<<8|'Z'<<16:	return EULER_ORDER_XYZ;
			case 'X'|'Z'<<8|'Y'<<16:	return EULER_ORDER_XZY;
			case 'Y'|'X'<<8|'Z'<<16:	return EULER_ORDER_YXZ;
			case 'Y'|'Z'<<8|'X'<<16:	return EULER_ORDER_YZX;
			case 'Z'|'X'<<8|'Y'<<16:	return EULER_ORDER_ZXY;
			case 'Z'|'Y'<<8|'X'<<16:	return EULER_ORDER_ZYX;
		}
	}

	PyErr_Format(PyExc_TypeError, "%s: invalid euler order '%s'", error_prefix, str);
	return -1;
}

/* note: BaseMath_ReadCallback must be called beforehand */
static PyObject *Euler_ToTupleExt(EulerObject *self, int ndigits)
{
	PyObject *ret;
	int i;

	ret= PyTuple_New(EULER_SIZE);

	if(ndigits >= 0) {
		for(i= 0; i < EULER_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(double_round((double)self->eul[i], ndigits)));
		}
	}
	else {
		for(i= 0; i < EULER_SIZE; i++) {
			PyTuple_SET_ITEM(ret, i, PyFloat_FromDouble(self->eul[i]));
		}
	}

	return ret;
}

//-----------------------------METHODS----------------------------
//return a quaternion representation of the euler

static char Euler_to_quaternion_doc[] =
".. method:: to_quaternion()\n"
"\n"
"   Return a quaternion representation of the euler.\n"
"\n"
"   :return: Quaternion representation of the euler.\n"
"   :rtype: :class:`Quaternion`\n"
;
static PyObject *Euler_to_quaternion(EulerObject * self)
{
	float quat[4];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	eulO_to_quat(quat, self->eul, self->order);

	return newQuaternionObject(quat, Py_NEW, NULL);
}

//return a matrix representation of the euler
static char Euler_to_matrix_doc[] =
".. method:: to_matrix()\n"
"\n"
"   Return a matrix representation of the euler.\n"
"\n"
"   :return: A 3x3 roation matrix representation of the euler.\n"
"   :rtype: :class:`Matrix`\n"
;
static PyObject *Euler_to_matrix(EulerObject * self)
{
	float mat[9];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	eulO_to_mat3((float (*)[3])mat, self->eul, self->order);

	return newMatrixObject(mat, 3, 3 , Py_NEW, NULL);
}

//sets the euler to 0,0,0
static char Euler_zero_doc[] =
".. method:: zero()\n"
"\n"
"   Set all values to zero.\n"
;
static PyObject *Euler_zero(EulerObject * self)
{
	zero_v3(self->eul);

	if(BaseMath_WriteCallback(self) == -1)
		return NULL;

	Py_RETURN_NONE;
}

static char Euler_rotate_axis_doc[] =
".. method:: rotate_axis(axis, angle)\n"
"\n"
"   Rotates the euler a certain amount and returning a unique euler rotation (no 720 degree pitches).\n"
"\n"
"   :arg axis: single character in ['X, 'Y', 'Z'].\n"
"   :type axis: string\n"
"   :arg angle: angle in radians.\n"
"   :type angle: float\n"
;
static PyObject *Euler_rotate_axis(EulerObject * self, PyObject *args)
{
	float angle = 0.0f;
	const char *axis;

	if(!PyArg_ParseTuple(args, "sf:rotate", &axis, &angle)){
		PyErr_SetString(PyExc_TypeError, "euler.rotate(): expected angle (float) and axis (x, y, z)");
		return NULL;
	}
	if(!(ELEM3(*axis, 'X', 'Y', 'Z') && axis[1]=='\0')){
		PyErr_SetString(PyExc_TypeError, "euler.rotate(): expected axis to be 'X', 'Y' or 'Z'");
		return NULL;
	}

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;


	rotate_eulO(self->eul, self->order, *axis, angle);

	(void)BaseMath_WriteCallback(self);

	Py_RETURN_NONE;
}

static char Euler_rotate_doc[] =
".. method:: rotate(other)\n"
"\n"
"   Rotates the euler a by another mathutils value.\n"
"\n"
"   :arg other: rotation component of mathutils value\n"
"   :type other: :class:`Euler`, :class:`Quaternion` or :class:`Matrix`\n"
;
static PyObject *Euler_rotate(EulerObject * self, PyObject *value)
{
	float self_rmat[3][3], other_rmat[3][3], rmat[3][3];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_any_to_rotmat(other_rmat, value, "euler.rotate(value)") == -1)
		return NULL;

	eulO_to_mat3(self_rmat, self->eul, self->order);
	mul_m3_m3m3(rmat, self_rmat, other_rmat);

	mat3_to_compatible_eulO(self->eul, self->eul, self->order, rmat);

	(void)BaseMath_WriteCallback(self);
	Py_RETURN_NONE;
}

static char Euler_make_compatible_doc[] =
".. method:: make_compatible(other)\n"
"\n"
"   Make this euler compatible with another, so interpolating between them works as intended.\n"
"\n"
"   .. note:: the rotation order is not taken into account for this function.\n"
;
static PyObject *Euler_make_compatible(EulerObject * self, PyObject *value)
{
	float teul[EULER_SIZE];

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	if(mathutils_array_parse(teul, EULER_SIZE, EULER_SIZE, value, "euler.make_compatible(other), invalid 'other' arg") == -1)
		return NULL;

	compatible_eul(self->eul, teul);

	(void)BaseMath_WriteCallback(self);

	Py_RETURN_NONE;
}

//----------------------------Euler.rotate()-----------------------
// return a copy of the euler

static char Euler_copy_doc[] =
".. function:: copy()\n"
"\n"
"   Returns a copy of this euler.\n"
"\n"
"   :return: A copy of the euler.\n"
"   :rtype: :class:`Euler`\n"
"\n"
"   .. note:: use this to get a copy of a wrapped euler with no reference to the original data.\n"
;
static PyObject *Euler_copy(EulerObject *self)
{
	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	return newEulerObject(self->eul, self->order, Py_NEW, Py_TYPE(self));
}

//----------------------------print object (internal)--------------
//print the object to screen

static PyObject *Euler_repr(EulerObject * self)
{
	PyObject *ret, *tuple;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	tuple= Euler_ToTupleExt(self, -1);

	ret= PyUnicode_FromFormat("Euler(%R, '%s')", tuple, euler_order_str(self));

	Py_DECREF(tuple);
	return ret;
}

static PyObject* Euler_richcmpr(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok= -1; /* zero is true */

	if (EulerObject_Check(a) && EulerObject_Check(b)) {
		EulerObject *eulA= (EulerObject*)a;
		EulerObject *eulB= (EulerObject*)b;

		if(BaseMath_ReadCallback(eulA) == -1 || BaseMath_ReadCallback(eulB) == -1)
			return NULL;

		ok= ((eulA->order == eulB->order) && EXPP_VectorsAreEqual(eulA->eul, eulB->eul, EULER_SIZE, 1)) ? 0 : -1;
	}

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	return Py_INCREF(res), res;
}

//---------------------SEQUENCE PROTOCOLS------------------------
//----------------------------len(object)------------------------
//sequence length
static int Euler_len(EulerObject *UNUSED(self))
{
	return EULER_SIZE;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Euler_item(EulerObject * self, int i)
{
	if(i<0) i= EULER_SIZE-i;

	if(i < 0 || i >= EULER_SIZE) {
		PyErr_SetString(PyExc_IndexError, "euler[attribute]: array index out of range");
		return NULL;
	}

	if(BaseMath_ReadIndexCallback(self, i) == -1)
		return NULL;

	return PyFloat_FromDouble(self->eul[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Euler_ass_item(EulerObject * self, int i, PyObject *value)
{
	float f = PyFloat_AsDouble(value);

	if(f == -1 && PyErr_Occurred()) { // parsed item not a number
		PyErr_SetString(PyExc_TypeError, "euler[attribute] = x: argument not a number");
		return -1;
	}

	if(i<0) i= EULER_SIZE-i;

	if(i < 0 || i >= EULER_SIZE){
		PyErr_SetString(PyExc_IndexError, "euler[attribute] = x: array assignment index out of range");
		return -1;
	}

	self->eul[i] = f;

	if(BaseMath_WriteIndexCallback(self, i) == -1)
		return -1;

	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Euler_slice(EulerObject * self, int begin, int end)
{
	PyObject *tuple;
	int count;

	if(BaseMath_ReadCallback(self) == -1)
		return NULL;

	CLAMP(begin, 0, EULER_SIZE);
	if (end<0) end= (EULER_SIZE + 1) + end;
	CLAMP(end, 0, EULER_SIZE);
	begin= MIN2(begin, end);

	tuple= PyTuple_New(end - begin);
	for(count = begin; count < end; count++) {
		PyTuple_SET_ITEM(tuple, count - begin, PyFloat_FromDouble(self->eul[count]));
	}

	return tuple;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Euler_ass_slice(EulerObject * self, int begin, int end, PyObject * seq)
{
	int i, size;
	float eul[EULER_SIZE];

	if(BaseMath_ReadCallback(self) == -1)
		return -1;

	CLAMP(begin, 0, EULER_SIZE);
	if (end<0) end= (EULER_SIZE + 1) + end;
	CLAMP(end, 0, EULER_SIZE);
	begin = MIN2(begin, end);

	if((size=mathutils_array_parse(eul, 0, EULER_SIZE, seq, "mathutils.Euler[begin:end] = []")) == -1)
		return -1;

	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "euler[begin:end] = []: size mismatch in slice assignment");
		return -1;
	}

	for(i= 0; i < EULER_SIZE; i++)
		self->eul[begin + i] = eul[i];

	(void)BaseMath_WriteCallback(self);
	return 0;
}

static PyObject *Euler_subscript(EulerObject *self, PyObject *item)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i;
		i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		if (i < 0)
			i += EULER_SIZE;
		return Euler_item(self, i);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, EULER_SIZE, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyTuple_New(0);
		}
		else if (step == 1) {
			return Euler_slice(self, start, stop);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with eulers");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "euler indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return NULL;
	}
}


static int Euler_ass_subscript(EulerObject *self, PyObject *item, PyObject *value)
{
	if (PyIndex_Check(item)) {
		Py_ssize_t i = PyNumber_AsSsize_t(item, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;
		if (i < 0)
			i += EULER_SIZE;
		return Euler_ass_item(self, i, value);
	}
	else if (PySlice_Check(item)) {
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)item, EULER_SIZE, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (step == 1)
			return Euler_ass_slice(self, start, stop, value);
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with euler");
			return -1;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "euler indices must be integers, not %.200s", Py_TYPE(item)->tp_name);
		return -1;
	}
}

//-----------------PROTCOL DECLARATIONS--------------------------
static PySequenceMethods Euler_SeqMethods = {
	(lenfunc) Euler_len,					/* sq_length */
	(binaryfunc) NULL,						/* sq_concat */
	(ssizeargfunc) NULL,					/* sq_repeat */
	(ssizeargfunc) Euler_item,				/* sq_item */
	(ssizessizeargfunc) NULL,				/* sq_slice, deprecated  */
	(ssizeobjargproc) Euler_ass_item,		/* sq_ass_item */
	(ssizessizeobjargproc) NULL,			/* sq_ass_slice, deprecated */
	(objobjproc) NULL,						/* sq_contains */
	(binaryfunc) NULL,						/* sq_inplace_concat */
	(ssizeargfunc) NULL,					/* sq_inplace_repeat */
};

static PyMappingMethods Euler_AsMapping = {
	(lenfunc)Euler_len,
	(binaryfunc)Euler_subscript,
	(objobjargproc)Euler_ass_subscript
};

/*
 * euler axis, euler.x/y/z
 */
static PyObject *Euler_getAxis(EulerObject *self, void *type)
{
	return Euler_item(self, GET_INT_FROM_POINTER(type));
}

static int Euler_setAxis(EulerObject *self, PyObject *value, void *type)
{
	return Euler_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

/* rotation order */
static PyObject *Euler_getOrder(EulerObject *self, void *UNUSED(closure))
{
	if(BaseMath_ReadCallback(self) == -1) /* can read order too */
		return NULL;

	return PyUnicode_FromString(euler_order_str(self));
}

static int Euler_setOrder(EulerObject *self, PyObject *value, void *UNUSED(closure))
{
	const char *order_str= _PyUnicode_AsString(value);
	short order= euler_order_from_string(order_str, "euler.order");

	if(order == -1)
		return -1;

	self->order= order;
	(void)BaseMath_WriteCallback(self); /* order can be written back */
	return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Euler_getseters[] = {
	{(char *)"x", (getter)Euler_getAxis, (setter)Euler_setAxis, (char *)"Euler X axis in radians.\n\n:type: float", (void *)0},
	{(char *)"y", (getter)Euler_getAxis, (setter)Euler_setAxis, (char *)"Euler Y axis in radians.\n\n:type: float", (void *)1},
	{(char *)"z", (getter)Euler_getAxis, (setter)Euler_setAxis, (char *)"Euler Z axis in radians.\n\n:type: float", (void *)2},
	{(char *)"order", (getter)Euler_getOrder, (setter)Euler_setOrder, (char *)"Euler rotation order.\n\n:type: string in ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']", (void *)NULL},

	{(char *)"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, (char *)BaseMathObject_Wrapped_doc, NULL},
	{(char *)"owner", (getter)BaseMathObject_getOwner, (setter)NULL, (char *)BaseMathObject_Owner_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL}  /* Sentinel */
};


//-----------------------METHOD DEFINITIONS ----------------------
static struct PyMethodDef Euler_methods[] = {
	{"zero", (PyCFunction) Euler_zero, METH_NOARGS, Euler_zero_doc},
	{"to_matrix", (PyCFunction) Euler_to_matrix, METH_NOARGS, Euler_to_matrix_doc},
	{"to_quaternion", (PyCFunction) Euler_to_quaternion, METH_NOARGS, Euler_to_quaternion_doc},
	{"rotate_axis", (PyCFunction) Euler_rotate_axis, METH_VARARGS, Euler_rotate_axis_doc},
	{"rotate", (PyCFunction) Euler_rotate, METH_O, Euler_rotate_doc},
	{"make_compatible", (PyCFunction) Euler_make_compatible, METH_O, Euler_make_compatible_doc},
	{"__copy__", (PyCFunction) Euler_copy, METH_NOARGS, Euler_copy_doc},
	{"copy", (PyCFunction) Euler_copy, METH_NOARGS, Euler_copy_doc},
	{NULL, NULL, 0, NULL}
};

//------------------PY_OBECT DEFINITION--------------------------
static char euler_doc[] =
"This object gives access to Eulers in Blender."
;
PyTypeObject euler_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"mathutils.Euler",				//tp_name
	sizeof(EulerObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)BaseMathObject_dealloc,		//tp_dealloc
	NULL,							//tp_print
	NULL,							//tp_getattr
	NULL,							//tp_setattr
	NULL,							//tp_compare
	(reprfunc) Euler_repr,			//tp_repr
	NULL,							//tp_as_number
	&Euler_SeqMethods,				//tp_as_sequence
	&Euler_AsMapping,				//tp_as_mapping
	NULL,							//tp_hash
	NULL,							//tp_call
	NULL,							//tp_str
	NULL,							//tp_getattro
	NULL,							//tp_setattro
	NULL,							//tp_as_buffer
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, //tp_flags
	euler_doc, //tp_doc
	(traverseproc)BaseMathObject_traverse,	//tp_traverse
	(inquiry)BaseMathObject_clear,	//tp_clear
	(richcmpfunc)Euler_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	NULL,							//tp_iter
	NULL,							//tp_iternext
	Euler_methods,					//tp_methods
	NULL,							//tp_members
	Euler_getseters,				//tp_getset
	NULL,							//tp_base
	NULL,							//tp_dict
	NULL,							//tp_descr_get
	NULL,							//tp_descr_set
	0,								//tp_dictoffset
	NULL,							//tp_init
	NULL,							//tp_alloc
	Euler_new,						//tp_new
	NULL,							//tp_free
	NULL,							//tp_is_gc
	NULL,							//tp_bases
	NULL,							//tp_mro
	NULL,							//tp_cache
	NULL,							//tp_subclasses
	NULL,							//tp_weaklist
	NULL							//tp_del
};
//------------------------newEulerObject (internal)-------------
//creates a new euler object
/*pass Py_WRAP - if vector is a WRAPPER for data allocated by BLENDER
 (i.e. it was allocated elsewhere by MEM_mallocN())
  pass Py_NEW - if vector is not a WRAPPER and managed by PYTHON
 (i.e. it must be created here with PyMEM_malloc())*/
PyObject *newEulerObject(float *eul, short order, int type, PyTypeObject *base_type)
{
	EulerObject *self;

	self= base_type ?	(EulerObject *)base_type->tp_alloc(base_type, 0) :
						(EulerObject *)PyObject_GC_New(EulerObject, &euler_Type);

	if(self) {
		/* init callbacks as NULL */
		self->cb_user= NULL;
		self->cb_type= self->cb_subtype= 0;

		if(type == Py_WRAP) {
			self->eul = eul;
			self->wrapped = Py_WRAP;
		}
		else if (type == Py_NEW) {
			self->eul = PyMem_Malloc(EULER_SIZE * sizeof(float));
			if(eul) {
				copy_v3_v3(self->eul, eul);
			}
			else {
				zero_v3(self->eul);
			}

			self->wrapped = Py_NEW;
		}
		else {
			PyErr_SetString(PyExc_RuntimeError, "Euler(): invalid type");
			return NULL;
		}

		self->order= order;
	}

	return (PyObject *)self;
}

PyObject *newEulerObject_cb(PyObject *cb_user, short order, int cb_type, int cb_subtype)
{
	EulerObject *self= (EulerObject *)newEulerObject(NULL, order, Py_NEW, NULL);
	if(self) {
		Py_INCREF(cb_user);
		self->cb_user=			cb_user;
		self->cb_type=			(unsigned char)cb_type;
		self->cb_subtype=		(unsigned char)cb_subtype;
		PyObject_GC_Track(self);
	}

	return (PyObject *)self;
}
