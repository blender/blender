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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "rgbTuple.h"

/* This file is heavily based on the old bpython Constant object code in
   Blender */

/*****************************************************************************/
/* Python rgbTuple_Type callback function prototypes:                        */
/*****************************************************************************/
static void rgbTupleDeAlloc (C_rgbTuple *self);
static PyObject *rgbTupleGetAttr (C_rgbTuple *self, char *name);
static int rgbTupleSetAttr (C_rgbTuple *self, char *name, PyObject *v);
static int rgbTuplePrint(C_rgbTuple *self, FILE *fp, int flags);
static PyObject *rgbTupleRepr (C_rgbTuple *self);

static int rgbTupleLength(C_rgbTuple *self);

static PyObject *rgbTupleSubscript(C_rgbTuple *self, PyObject *key);
static int rgbTupleAssSubscript(C_rgbTuple *self, PyObject *who,
                                PyObject *cares);

static PyObject *rgbTupleItem(C_rgbTuple *self, int i);
static int rgbTupleAssItem(C_rgbTuple *self, int i, PyObject *ob);
static PyObject *rgbTupleSlice(C_rgbTuple *self, int begin, int end);
static int rgbTupleAssSlice(C_rgbTuple *self, int begin, int end, PyObject *seq);

/*****************************************************************************/
/* Python rgbTuple_Type Mapping Methods table:                               */
/*****************************************************************************/
static PyMappingMethods rgbTupleAsMapping =
{
  (inquiry)rgbTupleLength,             /* mp_length        */
  (binaryfunc)rgbTupleSubscript,       /* mp_subscript     */
  (objobjargproc)rgbTupleAssSubscript, /* mp_ass_subscript */
};

/*****************************************************************************/
/* Python rgbTuple_Type Sequence Methods table:                              */
/*****************************************************************************/
static PySequenceMethods rgbTupleAsSequence =
{
	(inquiry)			rgbTupleLength,			      /* sq_length */
	(binaryfunc)		0,				            	/* sq_concat */
	(intargfunc)		0,					            /* sq_repeat */
	(intargfunc)		rgbTupleItem,		        /* sq_item */
	(intintargfunc)		rgbTupleSlice,	    	/* sq_slice */
	(intobjargproc)		rgbTupleAssItem,	    /* sq_ass_item */
	(intintobjargproc)	rgbTupleAssSlice,	  /* sq_ass_slice	*/
};

/*****************************************************************************/
/* Python rgbTuple_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject rgbTuple_Type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,                                      /* ob_size */
  "rgbTuple",                             /* tp_name */
  sizeof (C_rgbTuple),                    /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)rgbTupleDeAlloc,            /* tp_dealloc */
  (printfunc)rgbTuplePrint,               /* tp_print */
  (getattrfunc)rgbTupleGetAttr,           /* tp_getattr */
  (setattrfunc)rgbTupleSetAttr,           /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)rgbTupleRepr,                 /* tp_repr */
  0,                                      /* tp_as_number */
  &rgbTupleAsSequence,                    /* tp_as_sequence */
  &rgbTupleAsMapping,                     /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  0,                                      /* tp_methods */
  0,                                      /* tp_members */
};

/*****************************************************************************/
/* Function:              rgbTuple_New                                       */
/*****************************************************************************/
PyObject *rgbTuple_New(float *rgb[3])
{
  C_rgbTuple *rgbTuple;

  printf ("In rgbTuple_New()\n");

  rgbTuple = (C_rgbTuple *)PyObject_NEW(C_rgbTuple, &rgbTuple_Type);

  if (rgbTuple == NULL)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create rgbTuple object");

	rgbTuple->rgb[0] = rgb[0];
	rgbTuple->rgb[1] = rgb[1];
	rgbTuple->rgb[2] = rgb[2];

  return (PyObject *)rgbTuple;
}

/*****************************************************************************/
/* Functions:      rgbTuple_getCol and rgbTuple_setCol                       */
/* Description:    These functions get/set rgb color triplet values.  The    */
/*                 get function returns a tuple, the set one accepts three   */
/*                 floats (separated or in a tuple) as arguments.            */
/*****************************************************************************/
PyObject *rgbTuple_getCol (C_rgbTuple *self)
{
	PyObject *list = PyList_New (3);

	if (!list) return EXPP_ReturnPyObjError (PyExc_MemoryError,
									"couldn't create PyList");

	PyList_SET_ITEM (list, 0, Py_BuildValue ("f", *(self->rgb[0]) ));
	PyList_SET_ITEM (list, 1, Py_BuildValue ("f", *(self->rgb[0]) ));
	PyList_SET_ITEM (list, 2, Py_BuildValue ("f", *(self->rgb[0]) ));

	return list;
}

PyObject *rgbTuple_setCol (C_rgbTuple *self, PyObject *args)
{
	int ok;
	float r = 0, g = 0, b = 0;

	if (PyObject_Length (args) == 3)
		ok = PyArg_ParseTuple (args, "fff", &r, &g, &b);

	else ok = PyArg_ParseTuple (args, "|(fff)", &r, &g, &b);

	if (!ok) 
		return EXPP_ReturnPyObjError (PyExc_TypeError,
										"expected [f,f,f] or f,f,f as arguments (or nothing)");

	*(self->rgb[0]) = EXPP_ClampFloat (r, 0.0, 1.0);
	*(self->rgb[1]) = EXPP_ClampFloat (g, 0.0, 1.0);
	*(self->rgb[2]) = EXPP_ClampFloat (b, 0.0, 1.0);

	return EXPP_incr_ret (Py_None);
}

/*****************************************************************************/
/* Function:    rgbTupleDeAlloc                                              */
/* Description: This is a callback function for the C_rgbTuple type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
static void rgbTupleDeAlloc (C_rgbTuple *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    rgbTupleGetAttr                                              */
/* Description: This is a callback function for the C_rgbTuple type. It is   */
/*              the function that accesses C_rgbTuple member variables and   */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* rgbTupleGetAttr (C_rgbTuple *self, char *name)
{
	int i;

  if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s]", "R", "G", "B");

	else if (!strcmp(name, "R") || !strcmp(name, "r")) i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1;
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2;
	else
	  return (EXPP_ReturnPyObjError (PyExc_AttributeError,
														"attribute not found"));

	return Py_BuildValue("f", *(self->rgb[i]));
}

/*****************************************************************************/
/* Function:    rgbTupleSetAttr                                              */
/* Description: This is a callback function for the C_rgbTuple type. It is   */
/*              the function that changes C_rgbTuple member variables.       */
/*****************************************************************************/
static int rgbTupleSetAttr (C_rgbTuple *self, char *name, PyObject *v)
{
	float value;

	if (!PyArg_Parse (v, "f", &value))
		return EXPP_ReturnIntError (PyExc_TypeError,
									"expected float argument");

	value = EXPP_ClampFloat(value, 0.0, 1.0);

	if (!strcmp(name, "R") || !strcmp(name, "r"))
					*(self->rgb[0]) = value;

	else if (!strcmp(name, "G") || !strcmp(name, "g"))
					*(self->rgb[1]) = value;

	else if (!strcmp(name, "B") || !strcmp(name, "b"))
					*(self->rgb[2]) = value;

	else return (EXPP_ReturnIntError (PyExc_AttributeError,
                        "attribute not found"));

	return 0;
}

/*****************************************************************************/
/* Section:    rgbTuple as Mapping                                           */
/*             These functions provide code to access rgbTuple objects as    */
/*             mappings.                                                     */
/*****************************************************************************/
static int rgbTupleLength(C_rgbTuple *self)
{
  return 3;
}

static PyObject *rgbTupleSubscript(C_rgbTuple *self, PyObject *key)
{
	char *name = NULL;
	int i;

	if (PyNumber_Check(key)) return rgbTupleItem(self, (int)PyInt_AsLong(key));

	if (!PyArg_ParseTuple(key, "s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected int or string argument");

	if (!strcmp(name, "R") || !strcmp(name, "r"))      i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1; 
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2; 
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError, name);

	return Py_BuildValue("f", *(self->rgb[i]));
}

static int rgbTupleAssSubscript(C_rgbTuple *self, PyObject *key, PyObject *v)
{
  char *name = NULL;
	int i;

	if (!PyNumber_Check(v)) return EXPP_ReturnIntError(PyExc_TypeError,
									"value to assign must be a number");

	if (PyNumber_Check(key))
					return rgbTupleAssItem(self, (int)PyInt_AsLong(key), v);

	if (!PyArg_Parse(key, "s", &name))
		return EXPP_ReturnIntError (PyExc_TypeError,
							"expected int or string argument");

	if (!strcmp(name, "R") || !strcmp(name, "r"))      i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1; 
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2; 
	else
		return EXPP_ReturnIntError (PyExc_AttributeError, name);

	*(self->rgb[i]) = EXPP_ClampFloat(PyFloat_AsDouble(v), 0.0, 1.0);

  return 0;
}

/*****************************************************************************/
/* Section:    rgbTuple as Sequence                                          */
/*             These functions provide code to access rgbTuple objects as    */
/*             sequences.                                                    */
/*****************************************************************************/
static PyObject *rgbTupleItem(C_rgbTuple *self, int i)
{
	if (i < 0 || i >= 3)
			return EXPP_ReturnPyObjError (PyExc_IndexError,
											"array index out of range");

	return Py_BuildValue("f", *(self->rgb[i]));
}

static PyObject *rgbTupleSlice(C_rgbTuple *self, int begin, int end)
{
	PyObject *list;
	int count;

	if (begin < 0) begin = 0;
	if (end > 3) end = 3;
	if (begin > end) begin = end;

	list = PyList_New(end - begin);

	for (count = begin; count < end; count++)
		PyList_SetItem(list, count - begin,
										PyFloat_FromDouble(*(self->rgb[count])));

	return list;
}

static int rgbTupleAssItem(C_rgbTuple *self, int i, PyObject *ob)
{
	if (i < 0 || i >= 3)
		return EXPP_ReturnIntError(PyExc_IndexError,
										"array assignment index out of range");

	if (!PyNumber_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
										"color component must be a number");
/* XXX this check above is probably ... */
	*(self->rgb[i]) = EXPP_ClampFloat(PyFloat_AsDouble(ob), 0.0, 1.0);

	return 0;
}

static int rgbTupleAssSlice(C_rgbTuple *self, int begin, int end, PyObject *seq)
{
	int count;
	
	if (begin < 0) begin = 0;
	if (end > 3) end = 3;
	if (begin > end) begin = end;

	if (!PySequence_Check(seq))
		return EXPP_ReturnIntError(PyExc_TypeError,
										"illegal argument type for built-in operation");

	if (PySequence_Length(seq) != (end - begin))
		return EXPP_ReturnIntError(PyExc_TypeError,
										"size mismatch in slice assignment");

	for (count = begin; count < end; count++) {
		float value;
		PyObject *ob = PySequence_GetItem(seq, count);

		if (!PyArg_Parse(ob, "f", &value)) {
			Py_DECREF(ob);
			return -1;
		}

    *(self->rgb[count]) = EXPP_ClampFloat(value, 0.0, 1.0);

		Py_DECREF(ob);
	}

	return 0;
}

/*****************************************************************************/
/* Function:    rgbTuplePrint                                                */
/* Description: This is a callback function for the C_rgbTuple type. It      */
/*              builds a meaninful string to 'print' rgbTuple objects.       */
/*****************************************************************************/
static int rgbTuplePrint(C_rgbTuple *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[%f, %f, %f]",
					*(self->rgb[0]), *(self->rgb[1]), *(self->rgb[2]));
  return 0;
}

/*****************************************************************************/
/* Function:    rgbTupleRepr                                                 */
/* Description: This is a callback function for the C_rgbTuple type. It      */
/*              builds a meaninful string to represent rgbTuple objects.     */
/*****************************************************************************/
static PyObject *rgbTupleRepr (C_rgbTuple *self)
{
	float r, g, b;
	char buf[64];

	r = *(self->rgb[0]);
	g = *(self->rgb[1]);
	b = *(self->rgb[2]);

	PyOS_snprintf(buf, sizeof(buf), "[%f, %f, %f]", r, g, b);

	return PyString_FromString(buf);
}
