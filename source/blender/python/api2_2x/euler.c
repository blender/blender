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
 * 
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "euler.h"

//doc strings
char Euler_Zero_doc[] =
"() - set all values in the euler to 0";
char Euler_Unique_doc[] =
"() - sets the euler rotation a unique shortest arc rotation - tests for gimbal lock";
char Euler_ToMatrix_doc[] =
"() - returns a rotation matrix representing the euler rotation";
char Euler_ToQuat_doc[] =
"() - returns a quaternion representing the euler rotation";

//methods table
struct PyMethodDef Euler_methods[] = {
	{"zero",(PyCFunction)Euler_Zero, METH_NOARGS,
				Euler_Zero_doc},
	{"unique",(PyCFunction)Euler_Unique, METH_NOARGS,
				Euler_Unique_doc},
	{"toMatrix",(PyCFunction)Euler_ToMatrix, METH_NOARGS,
				Euler_ToMatrix_doc},
	{"toQuat",(PyCFunction)Euler_ToQuat, METH_NOARGS,
				Euler_ToQuat_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************/
//    Euler Python Object   
/*****************************/

//euler methods
PyObject *Euler_ToQuat(EulerObject *self)
{
	float *quat;
	int x;

	for(x = 0; x < 3; x++){
		self->eul[x] *= (float)(Py_PI/180);
	}
	quat = PyMem_Malloc(4*sizeof(float));
	EulToQuat(self->eul, quat);	
	for(x = 0; x < 3; x++){
		self->eul[x] *= (float)(180/Py_PI);
	}
	return (PyObject*)newQuaternionObject(quat);
}

PyObject *Euler_ToMatrix(EulerObject *self)
{
	float *mat;
	int x;

	for(x = 0; x < 3; x++){
		self->eul[x] *= (float)(Py_PI/180);
	}
	mat = PyMem_Malloc(3*3*sizeof(float));
	EulToMat3(self->eul, (float(*)[3])mat);
	for(x = 0; x < 3; x++){
		self->eul[x] *= (float)(180/Py_PI);
	}
	return (PyObject*)newMatrixObject(mat,3,3);
}

PyObject *Euler_Unique(EulerObject *self)
{
	float heading, pitch, bank;
	float pi2  = (float)Py_PI * 2.0f;
	float piO2 = (float)Py_PI / 2.0f;
	float Opi2 = 1.0f / pi2;

	//radians
	heading = self->eul[0] * (float)(Py_PI/180);
	pitch   = self->eul[1] * (float)(Py_PI/180);
	bank    = self->eul[2] * (float)(Py_PI/180);
	
	//wrap heading in +180 / -180
	pitch += (float)Py_PI;
	pitch -= (float)floor(pitch * Opi2) * pi2;
	pitch -= (float)Py_PI;


	if(pitch < -piO2){
		pitch = (float)-Py_PI - pitch;
		heading += (float)Py_PI;
		bank += (float)Py_PI;
	}else if (pitch > piO2){
		pitch = (float)Py_PI - pitch;
		heading += (float)Py_PI;
		bank += (float)Py_PI;
	}

	//gimbal lock test
	if(fabs(pitch) > piO2 - 1e-4){
		heading += bank;
		bank = 0.0f;
	}else{
		bank += (float)Py_PI;
		bank -= (float)(floor(bank * Opi2)) * pi2;
		bank -= (float)Py_PI;
	}

	heading += (float)Py_PI;
	heading -= (float)(floor(heading * Opi2)) * pi2;
	heading -= (float)Py_PI;
	
	//back to degrees
	self->eul[0] = heading * (float)(180/Py_PI);
	self->eul[1] = pitch * (float)(180/Py_PI);
	self->eul[2] = bank * (float)(180/Py_PI);

	return EXPP_incr_ret(Py_None);
}

PyObject *Euler_Zero(EulerObject *self)
{
	self->eul[0] = 0.0;
	self->eul[1] = 0.0;
	self->eul[2] = 0.0;

	return EXPP_incr_ret(Py_None);
}

static void Euler_dealloc(EulerObject *self)
{
  PyObject_DEL (self);
}

static PyObject *Euler_getattr(EulerObject *self, char *name)
{
	if (ELEM3(name[0], 'x', 'y', 'z') && name[1]==0){
		return PyFloat_FromDouble(self->eul[name[0]-'x']);
	}
	return Py_FindMethod(Euler_methods, (PyObject*)self, name);
}

static int Euler_setattr(EulerObject *self, char *name, PyObject *e)
{
	float val;

	if (!PyArg_Parse(e, "f", &val))
		return EXPP_ReturnIntError(PyExc_TypeError,	
		"unable to parse float argument\n");

	if (ELEM3(name[0], 'x', 'y', 'z') && name[1]==0){
		self->eul[name[0]-'x']= val;
		return 0;
	}
	else return -1;
}

/* Eulers Sequence methods */
static PyObject *Euler_item(EulerObject *self, int i)
{
	if (i < 0 || i >= 3)
	  return EXPP_ReturnPyObjError (PyExc_IndexError, "array index out of range\n");

	return Py_BuildValue("f", self->eul[i]);
}

static PyObject *Euler_slice(EulerObject *self, int begin, int end)
{
	PyObject *list;
	int count;
  
	if (begin < 0) begin= 0;
	if (end > 3) end= 3;
	if (begin > end) begin= end;

	list= PyList_New(end-begin);

	for (count = begin; count < end; count++){
		PyList_SetItem(list, count-begin, PyFloat_FromDouble(self->eul[count]));
	}
	return list;
}

static int Euler_ass_item(EulerObject *self, int i, PyObject *ob)
{
	if (i < 0 || i >= 3)
		return EXPP_ReturnIntError(PyExc_IndexError,
					"array assignment index out of range\n");

	if (!PyNumber_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
					"Euler member must be a number\n");

	if(!PyFloat_Check(ob) && !PyInt_Check(ob)){
		return EXPP_ReturnIntError(PyExc_TypeError,"int or float expected\n");
	}else{
		self->eul[i]= (float)PyFloat_AsDouble(ob);
	}
	return 0;
}

static int Euler_ass_slice(EulerObject *self, int begin, int end, PyObject *seq)
{
	int count, z;

	if (begin < 0) begin= 0;
	if (end > 3) end= 3;
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
			if (!PyArg_Parse(ob, "f", &self->eul[count])) {
				Py_DECREF(ob);
				return -1;
			}
		}
	}
  return 0;
}

static PyObject *Euler_repr (EulerObject *self)
{
	int i, maxindex = 3 - 1;
	char ftoa[24];
	PyObject *str1, *str2;

	str1 = PyString_FromString ("[");

	for (i = 0; i < maxindex; i++) {
		sprintf(ftoa, "%.4f, ", self->eul[i]);
		str2 = PyString_FromString (ftoa);
		if (!str1 || !str2) goto error;
		PyString_ConcatAndDel (&str1, str2);
	}

	sprintf(ftoa, "%.4f]\n", self->eul[maxindex]);
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

static PySequenceMethods Euler_SeqMethods =
{
	(inquiry)       0,                     /* sq_length */
	(binaryfunc)    0,                     /* sq_concat */
	(intargfunc)    0,                     /* sq_repeat */
	(intargfunc)    Euler_item,            /* sq_item */
	(intintargfunc)   Euler_slice,         /* sq_slice */
	(intobjargproc)   Euler_ass_item,      /* sq_ass_item */
	(intintobjargproc)  Euler_ass_slice,   /* sq_ass_slice */
};

PyTypeObject euler_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "euler",                     /*tp_name*/
  sizeof(EulerObject),         /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  (destructor)  Euler_dealloc,  /*tp_dealloc*/
  (printfunc)   0,              /*tp_print*/
  (getattrfunc) Euler_getattr, /*tp_getattr*/
  (setattrfunc) Euler_setattr, /*tp_setattr*/
  0,                           /*tp_compare*/
  (reprfunc)    Euler_repr,    /*tp_repr*/
  0,                           /*tp_as_number*/
  &Euler_SeqMethods,           /*tp_as_sequence*/
};

PyObject *newEulerObject(float *eul)
{
  EulerObject *self;
  int x;

  euler_Type.ob_type = &PyType_Type;

  self = PyObject_NEW(EulerObject, &euler_Type);

  if(!eul){
	  self->eul = PyMem_Malloc (3*sizeof (float));
	  for(x = 0; x < 3; x++){
		  self->eul[x] = 0.0f;
	  }
  }else	self->eul = eul;
 
  return (PyObject*) self;
}

