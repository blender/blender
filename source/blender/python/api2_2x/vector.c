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
 * Contributor(s): Willian P. Germano & Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "vector.h"

//doc strings
char Vector_Zero_doc[] =
"() - set all values in the vector to 0";
char Vector_Normalize_doc[] =
"() - normalize the vector";
char Vector_Negate_doc[] =
"() - changes vector to it's additive inverse";
char Vector_Resize2D_doc[] =
"() - resize a vector to [x,y]";
char Vector_Resize3D_doc[] =
"() - resize a vector to [x,y,z]";
char Vector_Resize4D_doc[] =
"() - resize a vector to [x,y,z,w]";

//method table
struct PyMethodDef Vector_methods[] = {
	{"zero",(PyCFunction)Vector_Zero, METH_NOARGS,
				Vector_Zero_doc},
	{"normalize",(PyCFunction)Vector_Normalize, METH_NOARGS,
				Vector_Normalize_doc},
	{"negate",(PyCFunction)Vector_Negate, METH_NOARGS,
				Vector_Negate_doc},
	{"resize2D",(PyCFunction)Vector_Resize2D, METH_NOARGS,
				Vector_Resize2D_doc},
	{"resize3D",(PyCFunction)Vector_Resize3D, METH_NOARGS,
				Vector_Resize2D_doc},
	{"resize4D",(PyCFunction)Vector_Resize4D, METH_NOARGS,
				Vector_Resize2D_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************/
//    Vector Python Object   
/*****************************/

//object methods
PyObject *Vector_Zero(VectorObject *self)
{
	int x;
	for(x = 0; x < self->size; x++){
		self->vec[x] = 0.0f;
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Vector_Normalize(VectorObject *self)
{
	float norm;
	int x;

	norm = 0.0f;
	for(x = 0; x < self->size; x++){
		norm += self->vec[x] * self->vec[x];
	}
	norm = (float)sqrt(norm);
	for(x = 0; x < self->size; x++){
		self->vec[x] /= norm;
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Vector_Negate(VectorObject *self)
{
	int x;
	for(x = 0; x < self->size; x++){
		self->vec[x] = -(self->vec[x]);
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Vector_Resize2D(VectorObject *self)
{
	float x, y;

	if(self->size == 4 || self->size == 3){
		x = self->vec[0];
		y = self->vec[1];
		PyMem_Free(self->vec);
		self->vec = PyMem_Malloc(2*sizeof (float));
		self->vec[0] = x;
		self->vec[1] = y;
		self->size = 2;
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Vector_Resize3D(VectorObject *self)
{
	float x, y, z;

	if(self->size == 2){
		x = self->vec[0];
		y = self->vec[1];
		PyMem_Free(self->vec);
		self->vec = PyMem_Malloc(3*sizeof (float));
		self->vec[0] = x;
		self->vec[1] = y;
		self->vec[2] = 0.0f;
		self->size = 3;
	}
	else if (self->size == 4){
		x = self->vec[0];
		y = self->vec[1];
		z = self->vec[2];
		PyMem_Free(self->vec);
		self->vec = PyMem_Malloc(3*sizeof (float));
		self->vec[0] = x;
		self->vec[1] = y;
		self->vec[2] = z;
		self->size = 3;
	}

	return EXPP_incr_ret(Py_None);
}

PyObject *Vector_Resize4D(VectorObject *self)
{
	float x, y, z;

	if(self->size == 2){
		x = self->vec[0];
		y = self->vec[1];
		PyMem_Free(self->vec);
		self->vec = PyMem_Malloc(4*sizeof (float));
		self->vec[0] = x;
		self->vec[1] = y;
		self->vec[2] = 0.0f;
		self->vec[3] = 1.0f;
		self->size = 4;
	}
	else if (self->size == 3){
		x = self->vec[0];
		y = self->vec[1];
		z = self->vec[2];
		PyMem_Free(self->vec);
		self->vec = PyMem_Malloc(4*sizeof (float));
		self->vec[0] = x;
		self->vec[1] = y;
		self->vec[2] = z;
		self->vec[3] = 1.0f;
		self->size = 4;
	}

	return EXPP_incr_ret(Py_None);
}

static void Vector_dealloc(VectorObject *self)
{
  PyObject_DEL (self);
}

static PyObject *Vector_getattr(VectorObject *self, char *name)
{
	if (self->size==4 && ELEM4(name[0], 'x', 'y', 'z', 'w') && name[1]==0){
		if ((name[0]) == ('w')){
			return PyFloat_FromDouble(self->vec[3]);
		}else{
			return PyFloat_FromDouble(self->vec[name[0]-'x']);
		}
	}
	else if (self->size==3 && ELEM3(name[0], 'x', 'y', 'z') && name[1]==0)
		return PyFloat_FromDouble(self->vec[name[0]-'x']);
	else if (self->size==2 && ELEM(name[0], 'x', 'y') && name[1]==0)
		return PyFloat_FromDouble(self->vec[name[0]-'x']);

	if ((strcmp(name,"length") == 0)){
		if(self->size == 4){
			return PyFloat_FromDouble(sqrt(self->vec[0] * self->vec[0] +
										   self->vec[1] * self->vec[1] +
										   self->vec[2] * self->vec[2] +
										   self->vec[3] * self->vec[3]));
		}
		else if(self->size == 3){
			return PyFloat_FromDouble(sqrt(self->vec[0] * self->vec[0] + 
										   self->vec[1] * self->vec[1] +
										   self->vec[2] * self->vec[2]));
		}else if (self->size == 2){
			return PyFloat_FromDouble(sqrt(self->vec[0] * self->vec[0] + 
										   self->vec[1] * self->vec[1]));
		}else EXPP_ReturnPyObjError(PyExc_AttributeError, 
					"can only return the length of a 2D ,3D or 4D vector\n");
	}

	return Py_FindMethod(Vector_methods, (PyObject*)self, name);
}

static int Vector_setattr(VectorObject *self, char *name, PyObject *v)
{
	float val;
	int valTemp;

	if(!PyFloat_Check(v)){
		if(!PyInt_Check(v)){
			return EXPP_ReturnIntError(PyExc_TypeError,"int or float expected\n");
		}else{
			if (!PyArg_Parse(v, "i", &valTemp))
				return EXPP_ReturnIntError(PyExc_TypeError,	"unable to parse int argument\n");
			val = (float)valTemp;
		}
	}else{
		if (!PyArg_Parse(v, "f", &val))
			return EXPP_ReturnIntError(PyExc_TypeError,	"unable to parse float argument\n");
	}
	if (self->size==4 && ELEM4(name[0], 'x', 'y', 'z', 'w') && name[1]==0){
		if ((name[0]) == ('w')){
			self->vec[3]= val;
		}else{
			self->vec[name[0]-'x']= val;
		}
	}
	else if (self->size==3 && ELEM3(name[0], 'x', 'y', 'z') && name[1]==0)
		self->vec[name[0]-'x']= val;
	else if (self->size==2 && ELEM(name[0], 'x', 'y') && name[1]==0)
		self->vec[name[0]-'x']= val;
	else return -1;

	return 0;
}

/* Vectors Sequence methods */
static int Vector_len(VectorObject *self) 
{
	return self->size;
}

static PyObject *Vector_item(VectorObject *self, int i)
{
	if (i < 0 || i >= self->size)
	  return EXPP_ReturnPyObjError (PyExc_IndexError, "array index out of range\n");

	return Py_BuildValue("f", self->vec[i]);

}

static PyObject *Vector_slice(VectorObject *self, int begin, int end)
{
	PyObject *list;
	int count;
  
	if (begin < 0) begin= 0;
	if (end > self->size) end= self->size;
	if (begin > end) begin= end;

	list= PyList_New(end-begin);

	for (count = begin; count < end; count++){
		PyList_SetItem(list, count-begin, PyFloat_FromDouble(self->vec[count]));
	}

	return list;
}

static int Vector_ass_item(VectorObject *self, int i, PyObject *ob)
{
	if (i < 0 || i >= self->size)
		return EXPP_ReturnIntError(PyExc_IndexError,
					"array assignment index out of range\n");
	if (!PyInt_Check(ob) && !PyFloat_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
					"vector member must be a number\n");

	self->vec[i]= (float)PyFloat_AsDouble(ob);

	return 0;
}

static int Vector_ass_slice(VectorObject *self, int begin, int end, PyObject *seq)
{
	int count, z;

	if (begin < 0) begin= 0;
	if (end > self->size) end= self->size;
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
		if (!PyInt_Check(ob) && !PyFloat_Check(ob))
			return EXPP_ReturnIntError(PyExc_IndexError,
						"list member must be a number\n");

		if (!PyArg_Parse(ob, "f", &self->vec[count])){
			Py_DECREF(ob);
			return -1;
		}
	}

  return 0;
}

static PyObject *Vector_repr (VectorObject *self)
{
	int i, maxindex = self->size - 1;
	char ftoa[24];
	PyObject *str1, *str2;

	str1 = PyString_FromString ("[");

	for (i = 0; i < maxindex; i++) {
		sprintf(ftoa, "%.4f, ", self->vec[i]);
		str2 = PyString_FromString (ftoa);
		if (!str1 || !str2) goto error; 
		PyString_ConcatAndDel (&str1, str2);
	}

	sprintf(ftoa, "%.4f]\n", self->vec[maxindex]);
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


PyObject * Vector_add(PyObject *v1, PyObject *v2)
{
	float * vec;
	int x;

	if((!VectorObject_Check(v1)) || (!VectorObject_Check(v2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((VectorObject*)v1)->flag != 0 || ((VectorObject*)v2)->flag != 0)
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"cannot add a scalar to a vector\n");
	if(((VectorObject*)v1)->size != ((VectorObject*)v2)->size)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"vectors must have the same dimensions for this operation\n");

	vec = PyMem_Malloc ((((VectorObject*)v1)->size)*sizeof (float));

	for(x = 0; x < ((VectorObject*)v1)->size; x++){
		vec[x] = ((VectorObject*)v1)->vec[x] + ((VectorObject*)v2)->vec[x];
	}
	
	return (PyObject*)newVectorObject(vec, (((VectorObject*)v1)->size));
}

PyObject * Vector_sub(PyObject *v1, PyObject *v2)
{
	float * vec;
	int x;

	if((!VectorObject_Check(v1)) || (!VectorObject_Check(v2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((VectorObject*)v1)->flag != 0 || ((VectorObject*)v2)->flag != 0)
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"cannot subtract a scalar from a vector\n");
	if(((VectorObject*)v1)->size != ((VectorObject*)v2)->size)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"vectors must have the same dimensions for this operation\n");

	vec = PyMem_Malloc ((((VectorObject*)v1)->size)*sizeof (float));

	for(x = 0; x < ((VectorObject*)v1)->size; x++){
		vec[x] = ((VectorObject*)v1)->vec[x] - ((VectorObject*)v2)->vec[x];
	}
	
	return (PyObject*)newVectorObject(vec, (((VectorObject*)v1)->size));
}

PyObject * Vector_mul(PyObject *v1, PyObject * v2)
{
	float * vec;
	int x;

	if((!VectorObject_Check(v1)) || (!VectorObject_Check(v2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((VectorObject*)v1)->flag == 0 && ((VectorObject*)v2)->flag == 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"please use the dot product or the cross product to multiply vectors\n");
	if(((VectorObject*)v1)->size != ((VectorObject*)v2)->size)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"vector dimension error during Vector_mul\n");

	vec = PyMem_Malloc ((((VectorObject*)v1)->size)*sizeof(float));

	for(x = 0; x < ((VectorObject*)v1)->size; x++){
		vec[x] = ((VectorObject*)v1)->vec[x] * ((VectorObject*)v2)->vec[x];
	}
	
	return (PyObject*)newVectorObject(vec, (((VectorObject*)v1)->size));
}

PyObject * Vector_div(PyObject *v1, PyObject * v2)
{
	float * vec;
	int x;

	if((!VectorObject_Check(v1)) || (!VectorObject_Check(v2)))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
				"unsupported type for this operation\n");
	if(((VectorObject*)v1)->flag == 0 && ((VectorObject*)v2)->flag == 0)
		return EXPP_ReturnPyObjError (PyExc_ArithmeticError,
			"cannot divide two vectors\n");
	if(((VectorObject*)v1)->flag != 0 && ((VectorObject*)v2)->flag == 0)
		return EXPP_ReturnPyObjError (PyExc_TypeError,
					"cannot divide a scalar by a vector\n");
	if(((VectorObject*)v1)->size != ((VectorObject*)v2)->size)
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
				"vector dimension error during Vector_mul\n");

	vec = PyMem_Malloc ((((VectorObject*)v1)->size)*sizeof(float));

	for(x = 0; x < ((VectorObject*)v1)->size; x++){
		vec[x] = ((VectorObject*)v1)->vec[x] / ((VectorObject*)v2)->vec[x];
	}
	
	return (PyObject*)newVectorObject(vec, (((VectorObject*)v1)->size));
}

//coercion of unknown types to type VectorObject for numeric protocols
int Vector_coerce(PyObject **v1, PyObject **v2)
{ 
	long *tempI;
	double *tempF;
	float *vec;
	int x;

	if (VectorObject_Check(*v1)) {
		if (VectorObject_Check(*v2)) { //two vectors
			Py_INCREF(*v1);
			Py_INCREF(*v2);
			return 0;
		}else{
			if(Matrix_CheckPyObject(*v2)){
				printf("vector/matrix numeric protocols unsupported...\n");
				Py_INCREF(*v1);
				return 0; //operation will type check
			}else if(PyNumber_Check(*v2)){
				if(PyInt_Check(*v2)){ //cast scalar to vector
					tempI = PyMem_Malloc(1*sizeof(long));
					*tempI = PyInt_AsLong(*v2);
					vec = PyMem_Malloc ((((VectorObject*)*v1)->size)*sizeof (float));
					for(x = 0; x < (((VectorObject*)*v1)->size); x++){
						vec[x] = (float)*tempI;
					}
					PyMem_Free(tempI);
					*v2 = newVectorObject(vec, (((VectorObject*)*v1)->size));
					((VectorObject*)*v2)->flag = 1;	//int coercion
					Py_INCREF(*v1);
					return 0;
				}else if(PyFloat_Check(*v2)){ //cast scalar to vector
					tempF = PyMem_Malloc(1*sizeof(double));
					*tempF = PyFloat_AsDouble(*v2);
					vec = PyMem_Malloc ((((VectorObject*)*v1)->size)*sizeof (float));
					for(x = 0; x < (((VectorObject*)*v1)->size); x++){
						vec[x] = (float)*tempF;
					}
					PyMem_Free(tempF);
					*v2 = newVectorObject(vec, (((VectorObject*)*v1)->size));
					((VectorObject*)*v2)->flag = 2;	//float coercion
					Py_INCREF(*v1);
					return 0;
				}
			}
			//unknown type or numeric cast failure
			printf("attempting vector operation with unsupported type...\n");
			Py_INCREF(*v1);
			return 0; //operation will type check
		}
	}else{
		printf("numeric protocol failure...\n");
		return -1; //this should not occur - fail
	}
	return -1;
}


static PySequenceMethods Vector_SeqMethods =
{
	(inquiry)     Vector_len,               /* sq_length */
	(binaryfunc)    0,                      /* sq_concat */
	(intargfunc)    0,                      /* sq_repeat */
	(intargfunc)    Vector_item,            /* sq_item */
	(intintargfunc)   Vector_slice,         /* sq_slice */
	(intobjargproc)   Vector_ass_item,      /* sq_ass_item */
	(intintobjargproc)  Vector_ass_slice,   /* sq_ass_slice */
};

static PyNumberMethods Vector_NumMethods =
{
    (binaryfunc)	Vector_add,               /* __add__ */
    (binaryfunc)	Vector_sub,               /* __sub__ */
    (binaryfunc)	Vector_mul,               /* __mul__ */
    (binaryfunc)	Vector_div,		          /* __div__ */
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
    (coercion)		Vector_coerce,			  /* __coerce__ */
    (unaryfunc)		0,                        /* __int__ */
    (unaryfunc)		0,                        /* __long__ */
    (unaryfunc)		0,                        /* __float__ */
    (unaryfunc)		0,                        /* __oct__ */
    (unaryfunc)		0,                        /* __hex__ */

};

PyTypeObject vector_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "vector",                    /*tp_name*/
  sizeof(VectorObject),        /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  (destructor)  Vector_dealloc, /*tp_dealloc*/
  (printfunc)   0,              /*tp_print*/
  (getattrfunc) Vector_getattr, /*tp_getattr*/
  (setattrfunc) Vector_setattr, /*tp_setattr*/
  0,                            /*tp_compare*/
  (reprfunc)    Vector_repr,    /*tp_repr*/
  &Vector_NumMethods,           /*tp_as_number*/
  &Vector_SeqMethods,           /*tp_as_sequence*/
};

PyObject *newVectorObject(float *vec, int size)
{
  VectorObject *self;
  int x;

  vector_Type.ob_type = &PyType_Type;

  self = PyObject_NEW(VectorObject, &vector_Type);

  if(!vec){
	  self->vec = PyMem_Malloc (size *sizeof (float));
	  for(x = 0; x < size; x++){
		  self->vec[x] = 0.0f;
	  }
	  if(size == 4) self->vec[3] = 1.0f;
  }else{
	self->vec = vec;
  }

  self->size = size;
  self->flag = 0;
 
  return (PyObject*) self;
}

