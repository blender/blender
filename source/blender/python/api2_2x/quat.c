/*
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
 * 
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "quat.h"

//doc strings
char Quaternion_Identity_doc[] =
"() - set the quaternion to it's identity (1, vector)";
char Quaternion_Negate_doc[] =
"() - set all values in the quaternion to their negative";
char Quaternion_Conjugate_doc[] =
"() - set the quaternion to it's conjugate";
char Quaternion_Inverse_doc[] =
"() - set the quaternion to it's inverse";
char Quaternion_Normalize_doc[] =
"() - normalize the vector portion of the quaternion";
char Quaternion_ToEuler_doc[] =
"() - return a euler rotation representing the quaternion";
char Quaternion_ToMatrix_doc[] =
"() - return a rotation matrix representing the quaternion";

//methods table
struct PyMethodDef Quaternion_methods[] = {
	{"identity",(PyCFunction)Quaternion_Identity, METH_NOARGS,
				Quaternion_Identity_doc},
	{"negate",(PyCFunction)Quaternion_Negate, METH_NOARGS,
				Quaternion_Negate_doc},
	{"conjugate",(PyCFunction)Quaternion_Conjugate, METH_NOARGS,
				Quaternion_Conjugate_doc},
	{"inverse",(PyCFunction)Quaternion_Inverse, METH_NOARGS,
				Quaternion_Inverse_doc},
	{"normalize",(PyCFunction)Quaternion_Normalize, METH_NOARGS,
				Quaternion_Normalize_doc},
	{"toEuler",(PyCFunction)Quaternion_ToEuler, METH_NOARGS,
				Quaternion_ToEuler_doc},
	{"toMatrix",(PyCFunction)Quaternion_ToMatrix, METH_NOARGS,
				Quaternion_ToMatrix_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************/
//    Quaternion Python Object   
/*****************************/

PyObject *Quaternion_ToEuler(QuaternionObject *self)
{
	float *eul;
	int x;

	eul = PyMem_Malloc(3*sizeof(float));
	QuatToEul(self->quat, eul);

	for(x = 0; x < 3; x++){
		eul[x] *= (float)(180/Py_PI);
	}
	return (PyObject*)newEulerObject(eul);
}

PyObject *Quaternion_ToMatrix(QuaternionObject *self)
{
	float *mat;

	mat = PyMem_Malloc(3*3*sizeof(float));
	QuatToMat3(self->quat, (float(*)[3])mat);

	return (PyObject*)newMatrixObject(mat, 3,3);
}

//normalize the axis of rotation of [theta,vector]
PyObject *Quaternion_Normalize(QuaternionObject *self)
{
	NormalQuat(self->quat);
	return EXPP_incr_ret(Py_None);
}

PyObject *Quaternion_Inverse(QuaternionObject *self)
{
	float mag = 0.0f;
	int x;

	for(x = 1; x < 4; x++){
		self->quat[x] = -self->quat[x];
	}
	for(x = 0; x < 4; x++){
		mag += (self->quat[x] * self->quat[x]);
	}
	mag = (float)sqrt(mag);
	for(x = 0; x < 4; x++){
		self->quat[x] /= (mag * mag);
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Quaternion_Identity(QuaternionObject *self)
{
	self->quat[0] = 1.0;
	self->quat[1] = 0.0; self->quat[2] = 0.0; self->quat[3] = 0.0;

	return EXPP_incr_ret(Py_None);
}

PyObject *Quaternion_Negate(QuaternionObject *self)
{
	int x;

	for(x = 0; x < 4; x++){
		self->quat[x] = -self->quat[x];
	}
	return EXPP_incr_ret(Py_None);
}

PyObject *Quaternion_Conjugate(QuaternionObject *self)
{
	int x;

	for(x = 1; x < 4; x++){
		self->quat[x] = -self->quat[x];
	}
	return EXPP_incr_ret(Py_None);
}

static void Quaternion_dealloc(QuaternionObject *self)
{
  PyObject_DEL (self);
}

static PyObject *Quaternion_getattr(QuaternionObject *self, char *name)
{
	double mag = 0.0f;
	float *vec;
	int x;

	if (ELEM4(name[0], 'w', 'x', 'y', 'z') && name[1]==0){
		return PyFloat_FromDouble(self->quat[name[0]-'w']);
	}
	if(strcmp(name,"magnitude") == 0){
		for(x = 0; x < 4; x++){
			mag += self->quat[x] * self->quat[x];
		}
		mag = (float)sqrt(mag);
		return PyFloat_FromDouble(mag);
	}
	if(strcmp(name,"angle") == 0){

		mag = self->quat[0];
		mag = 2 * (acos(mag));
		mag *= (180/Py_PI);
		return PyFloat_FromDouble(mag);
	}
	if(strcmp(name,"axis") == 0){

		mag = (double)(self->quat[0] * (Py_PI/180));
		mag = 2 * (acos(mag));
		mag = sin(mag/2);
		vec = PyMem_Malloc(3*sizeof(float));
		for(x = 0; x < 3; x++){
			vec[x] = (self->quat[x + 1]/((float)(mag)));
		}
		Normalise(vec);
		return (PyObject*)newVectorObject(vec,3);
	}
	return Py_FindMethod(Quaternion_methods, (PyObject*)self, name);
}

static int Quaternion_setattr(QuaternionObject *self, char *name, PyObject *v)
{
	float val;

	if(!PyFloat_Check(v) && !PyInt_Check(v)){
			return EXPP_ReturnIntError(PyExc_TypeError,"int or float expected\n");
	}else{
		if (!PyArg_Parse(v, "f", &val))
			return EXPP_ReturnIntError(PyExc_TypeError,	"unable to parse float argument\n");
	}
	if (ELEM4(name[0], 'w', 'x', 'y', 'z') && name[1]==0){
		self->quat[name[0]-'w']= val;
	}else return -1;

	return 0;
}

/* Quaternions Sequence methods */
static PyObject *Quaternion_item(QuaternionObject *self, int i)
{
	if (i < 0 || i >= 4)
	  return EXPP_ReturnPyObjError (PyExc_IndexError, "array index out of range\n");

	return Py_BuildValue("f", self->quat[i]);
}

static PyObject *Quaternion_slice(QuaternionObject *self, int begin, int end)
{
	PyObject *list;
	int count;
  
	if (begin < 0) begin= 0;
	if (end > 4) end= 4;
	if (begin > end) begin= end;

	list= PyList_New(end-begin);

	for (count = begin; count < end; count++){
		PyList_SetItem(list, count-begin, PyFloat_FromDouble(self->quat[count]));
	}
	return list;
}

static int Quaternion_ass_item(QuaternionObject *self, int i, PyObject *ob)
{
	if (i < 0 || i >= 4)
		return EXPP_ReturnIntError(PyExc_IndexError,
					"array assignment index out of range\n");
	if (!PyNumber_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
					"Quaternion member must be a number\n");

	if(!PyFloat_Check(ob) && !PyInt_Check(ob)){
			return EXPP_ReturnIntError(PyExc_TypeError,"int or float expected\n");
	}else{
		self->quat[i]= (float)PyFloat_AsDouble(ob);
	}
	return 0;
}

static int Quaternion_ass_slice(QuaternionObject *self, int begin, int end, PyObject *seq)
{
	int count, z;

	if (begin < 0) begin= 0;
	if (end > 4) end= 4;
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

		if(!PyFloat_Check(ob) && !PyInt_Check(ob)){
			Py_DECREF(ob);
			return -1;
		}else{
			if (!PyArg_Parse(ob, "f", &self->quat[count])) {
				Py_DECREF(ob);
				return -1;
			}
		}
	}
	return 0;
}

static PyObject *Quaternion_repr (QuaternionObject *self)
{
	int i, maxindex = 4 - 1;
	char ftoa[24];
	PyObject *str1, *str2;

	str1 = PyString_FromString ("[");

	for (i = 0; i < maxindex; i++) {
		sprintf(ftoa, "%.4f, ", self->quat[i]);
		str2 = PyString_FromString (ftoa);
		if (!str1 || !str2) goto error; 
		PyString_ConcatAndDel (&str1, str2);
	}

	sprintf(ftoa, "%.4f]\n", self->quat[maxindex]);
	str2 = PyString_FromString (ftoa);
	if (!str1 || !str2) goto error; 
	PyString_ConcatAndDel (&str1, str2);

	if (str1) return str1;

error:
	Py_XDECREF (str1);
	Py_XDECREF (str2);
	return EXPP_ReturnPyObjError (PyExc_MemoryError,
			"couldn't create PyString!\n");
}


PyObject * Quaternion_add(PyObject *q1, PyObject *q2)
{
	float * quat;
	int x;

	if((!QuaternionObject_Check(q1)) || (!QuaternionObject_Check(q2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((QuaternionObject*)q1)->flag > 0 || ((QuaternionObject*)q2)->flag > 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"cannot add a scalar and a quat\n");

	quat = PyMem_Malloc (4*sizeof(float));
	for(x = 0; x < 4; x++){
		quat[x] =  (((QuaternionObject*)q1)->quat[x]) +  (((QuaternionObject*)q2)->quat[x]);
	}

	return (PyObject*)newQuaternionObject(quat);
}

PyObject * Quaternion_sub(PyObject *q1, PyObject *q2)
{
	float * quat;
	int x;

	if((!QuaternionObject_Check(q1)) || (!QuaternionObject_Check(q2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((QuaternionObject*)q1)->flag > 0 || ((QuaternionObject*)q2)->flag > 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"cannot subtract a scalar and a quat\n");

	quat = PyMem_Malloc (4*sizeof(float));
	for(x = 0; x < 4; x++){
		quat[x] =  (((QuaternionObject*)q1)->quat[x]) -  (((QuaternionObject*)q2)->quat[x]);
	}
	return (PyObject*)newQuaternionObject(quat);
}

PyObject * Quaternion_mul(PyObject *q1, PyObject * q2)
{
	float * quat;
	int x;

	if((!QuaternionObject_Check(q1)) || (!QuaternionObject_Check(q2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((QuaternionObject*)q1)->flag == 0 && ((QuaternionObject*)q2)->flag == 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"please use the dot or cross product to multiply quaternions\n");

	quat = PyMem_Malloc (4*sizeof(float));
	//scalar mult by quat
	for(x = 0; x < 4; x++){
		quat[x] = ((QuaternionObject*)q1)->quat[x] * ((QuaternionObject*)q2)->quat[x];
	}
	return (PyObject*)newQuaternionObject(quat);
}

//coercion of unknown types to type QuaternionObject for numeric protocols
int Quaternion_coerce(PyObject **q1, PyObject **q2)
{ 
	long *tempI;
	double *tempF;
	float *quat;
	int x;

	if (QuaternionObject_Check(*q1)) {
		if (QuaternionObject_Check(*q2)) { //two Quaternions
			Py_INCREF(*q1);
			Py_INCREF(*q2);
			return 0;
		}else{
			if(PyNumber_Check(*q2)){
				if(PyInt_Check(*q2)){ //cast scalar to Quaternion
					tempI = PyMem_Malloc(1*sizeof(long));
					*tempI = PyInt_AsLong(*q2);
					quat = PyMem_Malloc (4*sizeof (float));
					for(x = 0; x < 4; x++){
						quat[x] = (float)*tempI;
					}
					PyMem_Free(tempI);
					*q2 = newQuaternionObject(quat);
					((QuaternionObject*)*q2)->flag = 1;	//int coercion
					Py_INCREF(*q1);
					return 0;
				}else if(PyFloat_Check(*q2)){ //cast scalar to Quaternion
					tempF = PyMem_Malloc(1*sizeof(double));
					*tempF = PyFloat_AsDouble(*q2);
					quat = PyMem_Malloc (4*sizeof (float));
					for(x = 0; x < 4; x++){
						quat[x] = (float)*tempF;
					}
					PyMem_Free(tempF);
					*q2 = newQuaternionObject(quat);
					((QuaternionObject*)*q2)->flag = 2;	//float coercion
					Py_INCREF(*q1);
					return 0;
				}
			}
			//unknown type or numeric cast failure
			printf("attempting quaternion operation with unsupported type...\n");
			Py_INCREF(*q1);
			return 0; //operation will type check
		}
	}else{
		printf("numeric protocol failure...\n");
		return -1; //this should not occur - fail
	}
	return -1;
}

static PySequenceMethods Quaternion_SeqMethods =
{
	(inquiry)       0,                          /* sq_length */
	(binaryfunc)    0,                          /* sq_concat */
	(intargfunc)    0,                          /* sq_repeat */
	(intargfunc)    Quaternion_item,            /* sq_item */
	(intintargfunc)   Quaternion_slice,         /* sq_slice */
	(intobjargproc)   Quaternion_ass_item,      /* sq_ass_item */
	(intintobjargproc)  Quaternion_ass_slice,   /* sq_ass_slice */
};

static PyNumberMethods Quaternion_NumMethods =
{
    (binaryfunc)	Quaternion_add,           /* __add__ */
    (binaryfunc)	Quaternion_sub,           /* __sub__ */
    (binaryfunc)	Quaternion_mul,           /* __mul__ */
    (binaryfunc)	0,		                  /* __div__ */
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
    (coercion)		Quaternion_coerce,        /* __coerce__ */
    (unaryfunc)		0,                        /* __int__ */
    (unaryfunc)		0,                        /* __long__ */
    (unaryfunc)		0,                        /* __float__ */
    (unaryfunc)		0,                        /* __oct__ */
    (unaryfunc)		0,                        /* __hex__ */

};

PyTypeObject quaternion_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                /*ob_size*/
  "quaternion",                    /*tp_name*/
  sizeof(QuaternionObject),        /*tp_basicsize*/
  0,                               /*tp_itemsize*/
  (destructor)  Quaternion_dealloc, /*tp_dealloc*/
  (printfunc)   0,                  /*tp_print*/
  (getattrfunc) Quaternion_getattr, /*tp_getattr*/
  (setattrfunc) Quaternion_setattr, /*tp_setattr*/
  0,                                /*tp_compare*/
  (reprfunc)    Quaternion_repr,    /*tp_repr*/
  &Quaternion_NumMethods,           /*tp_as_number*/
  &Quaternion_SeqMethods,           /*tp_as_sequence*/
};

PyObject *newQuaternionObject(float *quat)
{
  QuaternionObject *self;
  int x;

  quaternion_Type.ob_type = &PyType_Type;

  self = PyObject_NEW(QuaternionObject, &quaternion_Type);

  if(!quat){
	  self->quat = PyMem_Malloc (4 *sizeof (float));
	  for(x = 0; x < 4; x++){
		  self->quat[x] = 0.0f;
	  }
	  self->quat[3] = 1.0f;
  }else{
	self->quat = quat;
  }
  self->flag = 0;
 
  return (PyObject*) self;
}

