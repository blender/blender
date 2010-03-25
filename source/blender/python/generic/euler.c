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

#include "Mathutils.h"

#include "BLI_math.h"
#include "BKE_utildefines.h"

#ifndef int32_t
#include "BLO_sys_types.h"
#endif

//----------------------------------Mathutils.Euler() -------------------
//makes a new euler for you to play with
static PyObject *Euler_new(PyTypeObject * type, PyObject * args, PyObject * kwargs)
{
	PyObject *listObject = NULL;
	int size, i;
	float eul[3];
	PyObject *e;
	short order= 0;  // TODO, add order option

	size = PyTuple_GET_SIZE(args);
	if (size == 1) {
		listObject = PyTuple_GET_ITEM(args, 0);
		if (PySequence_Check(listObject)) {
			size = PySequence_Length(listObject);
		} else { // Single argument was not a sequence
			PyErr_SetString(PyExc_TypeError, "Mathutils.Euler(): 3d numeric sequence expected\n");
			return NULL;
		}
	} else if (size == 0) {
		//returns a new empty 3d euler
		return newEulerObject(NULL, order, Py_NEW, NULL);
	} else {
		listObject = args;
	}

	if (size != 3) { // Invalid euler size
		PyErr_SetString(PyExc_AttributeError, "Mathutils.Euler(): 3d numeric sequence expected\n");
		return NULL;
	}

	for (i=0; i<size; i++) {
		e = PySequence_GetItem(listObject, i);
		if (e == NULL) { // Failed to read sequence
			Py_DECREF(listObject);
			PyErr_SetString(PyExc_RuntimeError, "Mathutils.Euler(): 3d numeric sequence expected\n");
			return NULL;
		}

		eul[i]= (float)PyFloat_AsDouble(e);
		Py_DECREF(e);
		
		if(eul[i]==-1 && PyErr_Occurred()) { // parsed item is not a number
			PyErr_SetString(PyExc_TypeError, "Mathutils.Euler(): 3d numeric sequence expected\n");
			return NULL;
		}
	}
	return newEulerObject(eul, order, Py_NEW, NULL);
}

short euler_order_from_string(const char *str, const char *error_prefix)
{
	if((str[0] && str[1] && str[2] && str[3]=='\0')) {
		switch(*((int32_t *)str)) {
			case 'X'|'Y'<<8|'Z'<<16:	return 0;
			case 'X'|'Z'<<8|'Y'<<16:	return 1;
			case 'Y'|'X'<<8|'Z'<<16:	return 2;
			case 'Y'|'Z'<<8|'X'<<16:	return 3;
			case 'Z'|'X'<<8|'Y'<<16:	return 4;
			case 'Z'|'Y'<<8|'X'<<16:	return 5;
		}
	}

	PyErr_Format(PyExc_TypeError, "%s: invalid euler order '%s'", error_prefix, str);
	return -1;
}

//-----------------------------METHODS----------------------------
//----------------------------Euler.toQuat()----------------------
//return a quaternion representation of the euler

static char Euler_ToQuat_doc[] =
".. method:: to_quat()\n"
"\n"
"   Return a quaternion representation of the euler.\n"
"\n"
"   :return: Quaternion representation of the euler.\n"
"   :rtype: :class:`Quaternion`\n";

static PyObject *Euler_ToQuat(EulerObject * self)
{
	float quat[4];

	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->order==0)	eul_to_quat(quat, self->eul);
	else				eulO_to_quat(quat, self->eul, self->order);

	return newQuaternionObject(quat, Py_NEW, NULL);
}
//----------------------------Euler.toMatrix()---------------------
//return a matrix representation of the euler
static char Euler_ToMatrix_doc[] =
".. method:: to_matrix()\n"
"\n"
"   Return a matrix representation of the euler.\n"
"\n"
"   :return: A 3x3 roation matrix representation of the euler.\n"
"   :rtype: :class:`Matrix`\n";

static PyObject *Euler_ToMatrix(EulerObject * self)
{
	float mat[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->order==0)	eul_to_mat3((float (*)[3])mat, self->eul);
	else				eulO_to_mat3((float (*)[3])mat, self->eul, self->order);

	return newMatrixObject(mat, 3, 3 , Py_NEW, NULL);
}
//----------------------------Euler.unique()-----------------------
//sets the x,y,z values to a unique euler rotation
// TODO, check if this works with rotation order!!!
static char Euler_Unique_doc[] =
".. method:: unique()\n"
"\n"
"   Calculate a unique rotation for this euler. Avoids gimble lock.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Euler`\n";

static PyObject *Euler_Unique(EulerObject * self)
{
#define PI_2		(Py_PI * 2.0)
#define PI_HALF		(Py_PI / 2.0)
#define PI_INV		(1.0 / Py_PI)

	double heading, pitch, bank;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	heading = self->eul[0];
	pitch = self->eul[1];
	bank = self->eul[2];

	//wrap heading in +180 / -180
	pitch += Py_PI;
	pitch -= floor(pitch * PI_INV) * PI_2;
	pitch -= Py_PI;


	if(pitch < -PI_HALF) {
		pitch = -Py_PI - pitch;
		heading += Py_PI;
		bank += Py_PI;
	} else if(pitch > PI_HALF) {
		pitch = Py_PI - pitch;
		heading += Py_PI;
		bank += Py_PI;
	}
	//gimbal lock test
	if(fabs(pitch) > PI_HALF - 1e-4) {
		heading += bank;
		bank = 0.0f;
	} else {
		bank += Py_PI;
		bank -= (floor(bank * PI_INV)) * PI_2;
		bank -= Py_PI;
	}

	heading += Py_PI;
	heading -= (floor(heading * PI_INV)) * PI_2;
	heading -= Py_PI;

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
}
//----------------------------Euler.zero()-------------------------
//sets the euler to 0,0,0
static char Euler_Zero_doc[] =
".. method:: zero()\n"
"\n"
"   Set all values to zero.\n"
"\n"
"   :return: an instance of itself\n"
"   :rtype: :class:`Euler`\n";

static PyObject *Euler_Zero(EulerObject * self)
{
	self->eul[0] = 0.0;
	self->eul[1] = 0.0;
	self->eul[2] = 0.0;

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
}
//----------------------------Euler.rotate()-----------------------
//rotates a euler a certain amount and returns the result
//should return a unique euler rotation (i.e. no 720 degree pitches :)
static PyObject *Euler_Rotate(EulerObject * self, PyObject *args)
{
	float angle = 0.0f;
	char *axis;

	if(!PyArg_ParseTuple(args, "fs", &angle, &axis)){
		PyErr_SetString(PyExc_TypeError, "euler.rotate():expected angle (float) and axis (x,y,z)");
		return NULL;
	}
	if(ELEM3(*axis, 'x', 'y', 'z') && axis[1]=='\0'){
		PyErr_SetString(PyExc_TypeError, "euler.rotate(): expected axis to be 'x', 'y' or 'z'");
		return NULL;
	}

	if(!BaseMath_ReadCallback(self))
		return NULL;

	if(self->order == 0)	rotate_eul(self->eul, *axis, angle);
	else					rotate_eulO(self->eul, self->order, *axis, angle);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
}

static char Euler_MakeCompatible_doc[] =
".. method:: make_compatible(other)\n"
"\n"
"   Make this euler compatible with another, so interpolating between them works as intended.\n"
"\n"
"   :arg other: make compatible with this rotation.\n"
"   :type other: :class:`Euler`\n"
"   :return: an instance of itself.\n"
"   :rtype: :class:`Euler`\n"
"\n"
"   .. note:: the order of eulers must match or an exception is raised.\n";

static PyObject *Euler_MakeCompatible(EulerObject * self, EulerObject *value)
{
	if(!EulerObject_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "euler.make_compatible(euler): expected a single euler argument.");
		return NULL;
	}
	
	if(!BaseMath_ReadCallback(self) || !BaseMath_ReadCallback(value))
		return NULL;

	if(self->order != value->order) {
		PyErr_SetString(PyExc_ValueError, "euler.make_compatible(euler): rotation orders don't match\n");
		return NULL;
	}

	compatible_eul(self->eul, value->eul);

	BaseMath_WriteCallback(self);
	Py_INCREF(self);
	return (PyObject *)self;
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
"   .. note:: use this to get a copy of a wrapped euler with no reference to the original data.\n";

static PyObject *Euler_copy(EulerObject * self, PyObject *args)
{
	if(!BaseMath_ReadCallback(self))
		return NULL;

	return newEulerObject(self->eul, self->order, Py_NEW, Py_TYPE(self));
}

//----------------------------print object (internal)--------------
//print the object to screen
static PyObject *Euler_repr(EulerObject * self)
{
	char str[64];

	if(!BaseMath_ReadCallback(self))
		return NULL;

	sprintf(str, "[%.6f, %.6f, %.6f](euler)", self->eul[0], self->eul[1], self->eul[2]);
	return PyUnicode_FromString(str);
}
//------------------------tp_richcmpr
//returns -1 execption, 0 false, 1 true
static PyObject* Euler_richcmpr(PyObject *objectA, PyObject *objectB, int comparison_type)
{
	EulerObject *eulA = NULL, *eulB = NULL;
	int result = 0;

	if(EulerObject_Check(objectA)) {
		eulA = (EulerObject*)objectA;
		if(!BaseMath_ReadCallback(eulA))
			return NULL;
	}
	if(EulerObject_Check(objectB)) {
		eulB = (EulerObject*)objectB;
		if(!BaseMath_ReadCallback(eulB))
			return NULL;
	}

	if (!eulA || !eulB){
		if (comparison_type == Py_NE){
			Py_RETURN_TRUE;
		}else{
			Py_RETURN_FALSE;
		}
	}
	eulA = (EulerObject*)objectA;
	eulB = (EulerObject*)objectB;

	switch (comparison_type){
		case Py_EQ:
			result = EXPP_VectorsAreEqual(eulA->eul, eulB->eul, 3, 1);
			break;
		case Py_NE:
			result = !EXPP_VectorsAreEqual(eulA->eul, eulB->eul, 3, 1);
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
static int Euler_len(EulerObject * self)
{
	return 3;
}
//----------------------------object[]---------------------------
//sequence accessor (get)
static PyObject *Euler_item(EulerObject * self, int i)
{
	if(i<0) i= 3-i;
	
	if(i < 0 || i >= 3) {
		PyErr_SetString(PyExc_IndexError, "euler[attribute]: array index out of range");
		return NULL;
	}

	if(!BaseMath_ReadIndexCallback(self, i))
		return NULL;

	return PyFloat_FromDouble(self->eul[i]);

}
//----------------------------object[]-------------------------
//sequence accessor (set)
static int Euler_ass_item(EulerObject * self, int i, PyObject * value)
{
	float f = PyFloat_AsDouble(value);

	if(f == -1 && PyErr_Occurred()) { // parsed item not a number
		PyErr_SetString(PyExc_TypeError, "euler[attribute] = x: argument not a number");
		return -1;
	}

	if(i<0) i= 3-i;
	
	if(i < 0 || i >= 3){
		PyErr_SetString(PyExc_IndexError, "euler[attribute] = x: array assignment index out of range\n");
		return -1;
	}
	
	self->eul[i] = f;

	if(!BaseMath_WriteIndexCallback(self, i))
		return -1;

	return 0;
}
//----------------------------object[z:y]------------------------
//sequence slice (get)
static PyObject *Euler_slice(EulerObject * self, int begin, int end)
{
	PyObject *list = NULL;
	int count;

	if(!BaseMath_ReadCallback(self))
		return NULL;

	CLAMP(begin, 0, 3);
	if (end<0) end= 4+end;
	CLAMP(end, 0, 3);
	begin = MIN2(begin,end);

	list = PyList_New(end - begin);
	for(count = begin; count < end; count++) {
		PyList_SetItem(list, count - begin,
				PyFloat_FromDouble(self->eul[count]));
	}

	return list;
}
//----------------------------object[z:y]------------------------
//sequence slice (set)
static int Euler_ass_slice(EulerObject * self, int begin, int end,
				 PyObject * seq)
{
	int i, y, size = 0;
	float eul[3];
	PyObject *e;

	if(!BaseMath_ReadCallback(self))
		return -1;

	CLAMP(begin, 0, 3);
	if (end<0) end= 4+end;
	CLAMP(end, 0, 3);
	begin = MIN2(begin,end);

	size = PySequence_Length(seq);
	if(size != (end - begin)){
		PyErr_SetString(PyExc_TypeError, "euler[begin:end] = []: size mismatch in slice assignment");
		return -1;
	}

	for (i = 0; i < size; i++) {
		e = PySequence_GetItem(seq, i);
		if (e == NULL) { // Failed to read sequence
			PyErr_SetString(PyExc_RuntimeError, "euler[begin:end] = []: unable to read sequence");
			return -1;
		}

		eul[i] = (float)PyFloat_AsDouble(e);
		Py_DECREF(e);

		if(eul[i]==-1 && PyErr_Occurred()) { // parsed item not a number
			PyErr_SetString(PyExc_TypeError, "euler[begin:end] = []: sequence argument not a number");
			return -1;
		}
	}
	//parsed well - now set in vector
	for(y = 0; y < 3; y++){
		self->eul[begin + y] = eul[y];
	}

	BaseMath_WriteCallback(self);
	return 0;
}
//-----------------PROTCOL DECLARATIONS--------------------------
static PySequenceMethods Euler_SeqMethods = {
	(lenfunc) Euler_len,						/* sq_length */
	(binaryfunc) 0,								/* sq_concat */
	(ssizeargfunc) 0,								/* sq_repeat */
	(ssizeargfunc) Euler_item,					/* sq_item */
	(ssizessizeargfunc) Euler_slice,				/* sq_slice */
	(ssizeobjargproc) Euler_ass_item,				/* sq_ass_item */
	(ssizessizeobjargproc) Euler_ass_slice,			/* sq_ass_slice */
};


/*
 * vector axis, vector.x/y/z/w
 */
static PyObject *Euler_getAxis( EulerObject * self, void *type )
{
	return Euler_item(self, GET_INT_FROM_POINTER(type));
}

static int Euler_setAxis( EulerObject * self, PyObject * value, void * type )
{
	return Euler_ass_item(self, GET_INT_FROM_POINTER(type), value);
}

/* rotation order */
static PyObject *Euler_getOrder(EulerObject *self, void *type)
{
	static char order[][4] = {"XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"};
	return PyUnicode_FromString(order[self->order]);
}

static int Euler_setOrder( EulerObject * self, PyObject * value, void * type )
{
	char *order_str= _PyUnicode_AsString(value);
	short order= euler_order_from_string(order_str, "euler.order");

	if(order < 0)
		return -1;

	if(self->cb_user) {
		PyErr_SetString(PyExc_TypeError, "euler.order: assignment is not allowed on eulers with an owner");
		return -1;
	}

	self->order= order;
	return 0;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef Euler_getseters[] = {
	{"x", (getter)Euler_getAxis, (setter)Euler_setAxis, "Euler X axis in radians. **type** float", (void *)0},
	{"y", (getter)Euler_getAxis, (setter)Euler_setAxis, "Euler Y axis in radians. **type** float", (void *)1},
	{"z", (getter)Euler_getAxis, (setter)Euler_setAxis, "Euler Z axis in radians. **type** float", (void *)2},
	{"order", (getter)Euler_getOrder, (setter)Euler_setOrder, "Euler rotation order. **type** string in ['XYZ', 'XZY', 'YXZ', 'YZX', 'ZXY', 'ZYX']", (void *)NULL},

	{"is_wrapped", (getter)BaseMathObject_getWrapped, (setter)NULL, BaseMathObject_Wrapped_doc, NULL},
	{"_owner", (getter)BaseMathObject_getOwner, (setter)NULL, BaseMathObject_Owner_doc, NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};


//-----------------------METHOD DEFINITIONS ----------------------
static struct PyMethodDef Euler_methods[] = {
	{"zero", (PyCFunction) Euler_Zero, METH_NOARGS, Euler_Zero_doc},
	{"unique", (PyCFunction) Euler_Unique, METH_NOARGS, Euler_Unique_doc},
	{"to_matrix", (PyCFunction) Euler_ToMatrix, METH_NOARGS, Euler_ToMatrix_doc},
	{"to_quat", (PyCFunction) Euler_ToQuat, METH_NOARGS, Euler_ToQuat_doc},
	{"rotate", (PyCFunction) Euler_Rotate, METH_VARARGS, NULL},
	{"make_compatible", (PyCFunction) Euler_MakeCompatible, METH_O, Euler_MakeCompatible_doc},
	{"__copy__", (PyCFunction) Euler_copy, METH_VARARGS, Euler_copy_doc},
	{"copy", (PyCFunction) Euler_copy, METH_VARARGS, Euler_copy_doc},
	{NULL, NULL, 0, NULL}
};

//------------------PY_OBECT DEFINITION--------------------------
static char euler_doc[] =
"This object gives access to Eulers in Blender.";

PyTypeObject euler_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"euler",						//tp_name
	sizeof(EulerObject),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)BaseMathObject_dealloc,		//tp_dealloc
	0,								//tp_print
	0,								//tp_getattr
	0,								//tp_setattr
	0,								//tp_compare
	(reprfunc) Euler_repr,			//tp_repr
	0,				//tp_as_number
	&Euler_SeqMethods,				//tp_as_sequence
	0,								//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, //tp_flags
	euler_doc, //tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	(richcmpfunc)Euler_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	0,								//tp_iter
	0,								//tp_iternext
	Euler_methods,					//tp_methods
	0,								//tp_members
	Euler_getseters,				//tp_getset
	0,								//tp_base
	0,								//tp_dict
	0,								//tp_descr_get
	0,								//tp_descr_set
	0,								//tp_dictoffset
	0,								//tp_init
	0,								//tp_alloc
	Euler_new,						//tp_new
	0,								//tp_free
	0,								//tp_is_gc
	0,								//tp_bases
	0,								//tp_mro
	0,								//tp_cache
	0,								//tp_subclasses
	0,								//tp_weaklist
	0								//tp_del
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
	int x;

	if(base_type)	self = (EulerObject *)base_type->tp_alloc(base_type, 0);
	else			self = PyObject_NEW(EulerObject, &euler_Type);

	/* init callbacks as NULL */
	self->cb_user= NULL;
	self->cb_type= self->cb_subtype= 0;

	if(type == Py_WRAP){
		self->eul = eul;
		self->wrapped = Py_WRAP;
	}else if (type == Py_NEW){
		self->eul = PyMem_Malloc(3 * sizeof(float));
		if(!eul) { //new empty
			for(x = 0; x < 3; x++) {
				self->eul[x] = 0.0f;
			}
		}else{
			VECCOPY(self->eul, eul);
		}
		self->wrapped = Py_NEW;
	}else{ //bad type
		return NULL;
	}

	self->order= order;
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
	}

	return (PyObject *)self;
}
