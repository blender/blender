/* 
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include "charRGBA.h"

/* This file is heavily based on the old bpython Constant object code in
	 Blender */

/*****************************************************************************/
/* Python charRGBA_Type callback function prototypes:												 */
/*****************************************************************************/
static void charRGBA_dealloc (BPy_charRGBA *self);
static PyObject *charRGBA_getAttr (BPy_charRGBA *self, char *name);
static int charRGBA_setAttr (BPy_charRGBA *self, char *name, PyObject *v);
static PyObject *charRGBA_repr (BPy_charRGBA *self);

static int charRGBALength(BPy_charRGBA *self);

static PyObject *charRGBASubscript(BPy_charRGBA *self, PyObject *key);
static int charRGBAAssSubscript(BPy_charRGBA *self, PyObject *who,
	PyObject *cares);

static PyObject *charRGBAItem(BPy_charRGBA *self, int i);
static int charRGBAAssItem(BPy_charRGBA *self, int i, PyObject *ob);
static PyObject *charRGBASlice(BPy_charRGBA *self, int begin, int end);
static int charRGBAAssSlice(BPy_charRGBA *self, int begin, int end,
	PyObject *seq);

/*****************************************************************************/
/* Python charRGBA_Type Mapping Methods table:															 */
/*****************************************************************************/
static PyMappingMethods charRGBAAsMapping =
{
	(inquiry)charRGBALength,						 /* mp_length				 */
	(binaryfunc)charRGBASubscript,			 /* mp_subscript		 */
	(objobjargproc)charRGBAAssSubscript, /* mp_ass_subscript */
};

/*****************************************************************************/
/* Python charRGBA_Type Sequence Methods table:															 */
/*****************************************************************************/
static PySequenceMethods charRGBAAsSequence =
{
	(inquiry)			charRGBALength, /* sq_length */
	(binaryfunc)		0, /* sq_concat */
	(intargfunc)		0, /* sq_repeat */
	(intargfunc)		charRGBAItem, /* sq_item */
	(intintargfunc)		charRGBASlice, /* sq_slice */
	(intobjargproc)		charRGBAAssItem, /* sq_ass_item */
	(intintobjargproc)	charRGBAAssSlice, /* sq_ass_slice	*/
};

/*****************************************************************************/
/* Python charRGBA_Type structure definition:																 */
/*****************************************************************************/
PyTypeObject charRGBA_Type =
{
	PyObject_HEAD_INIT(NULL)
	0, /* ob_size */
	"charRGBA", /* tp_name */
	sizeof (BPy_charRGBA), /* tp_basicsize */
	0, /* tp_itemsize */
	/* methods */
	(destructor)charRGBA_dealloc, /* tp_dealloc */
	0, /* tp_print */
	(getattrfunc)charRGBA_getAttr, /* tp_getattr */
	(setattrfunc)charRGBA_setAttr, /* tp_setattr */
	0, /* tp_compare */
	(reprfunc)charRGBA_repr, /* tp_repr */
	0, /* tp_as_number */
	&charRGBAAsSequence, /* tp_as_sequence */
	&charRGBAAsMapping, /* tp_as_mapping */
	0, /* tp_as_hash */
	0,0,0,0,0,0,
	0, /* tp_doc */ 
	0,0,0,0,0,0,
	0, /* tp_methods */
	0, /* tp_members */
};

/*****************************************************************************/
/* Function:							charRGBA_New																			 */
/*****************************************************************************/
PyObject *charRGBA_New(char *rgba)
{
	BPy_charRGBA *charRGBA;

	charRGBA_Type.ob_type = &PyType_Type;

	charRGBA = (BPy_charRGBA *)PyObject_NEW(BPy_charRGBA, &charRGBA_Type);

	if (charRGBA == NULL)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
			"couldn't create charRGBA object");

	/* rgba is a pointer to the first item of a char[4] array */
	charRGBA->rgba[0] = &rgba[0];
	charRGBA->rgba[1] = &rgba[1];
	charRGBA->rgba[2] = &rgba[2];
	charRGBA->rgba[3] = &rgba[3];

	return (PyObject *)charRGBA;
}

/*****************************************************************************/
/* Functions:			 charRGBA_getCol and charRGBA_setCol											 */
/* Description:		 These functions get/set rgba color triplet values.	The		 */
/*								 get function returns a tuple, the set one accepts three	 */
/*								 chars (separated or in a tuple) as arguments.						*/
/*****************************************************************************/
PyObject *charRGBA_getCol (BPy_charRGBA *self)
{
	PyObject *list = PyList_New (4);

	if (!list) return EXPP_ReturnPyObjError (PyExc_MemoryError,
		"couldn't create PyList");

	PyList_SET_ITEM (list, 0, Py_BuildValue ("b", *(self->rgba[0]) ));
	PyList_SET_ITEM (list, 1, Py_BuildValue ("b", *(self->rgba[1]) ));
	PyList_SET_ITEM (list, 2, Py_BuildValue ("b", *(self->rgba[2]) ));
	PyList_SET_ITEM (list, 3, Py_BuildValue ("b", *(self->rgba[3]) ));

	return list;
}

PyObject *charRGBA_setCol (BPy_charRGBA *self, PyObject *args)
{
	int ok;
	char r = 0, g = 0, b = 0, a = 0;

	if (PyObject_Length (args) == 4)
		ok = PyArg_ParseTuple (args, "bbbb", &r, &g, &b, &a);

	else ok = PyArg_ParseTuple (args, "|(bbbb)", &r, &g, &b, &a);

	if (!ok) 
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected 1-byte ints [b,b,b,b] or b,b,b,b as arguments (or nothing)");

	*(self->rgba[0]) = EXPP_ClampInt (r, 0, 255);
	*(self->rgba[1]) = EXPP_ClampInt (g, 0, 255);
	*(self->rgba[2]) = EXPP_ClampInt (b, 0, 255);
	*(self->rgba[3]) = EXPP_ClampInt (a, 0, 255);

	return EXPP_incr_ret (Py_None);
}

/*****************************************************************************/
/* Function:		charRGBA_dealloc																						 */
/* Description: This is a callback function for the BPy_charRGBA type. It is */
/*							the destructor function.																		 */
/*****************************************************************************/
static void charRGBA_dealloc (BPy_charRGBA *self)
{
	PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:		charRGBA_getAttr																						 */
/* Description: This is a callback function for the BPy_charRGBA type. It is */
/*							the function that accesses BPy_charRGBA member variables and */
/*							methods.																										 */
/*****************************************************************************/
static PyObject* charRGBA_getAttr (BPy_charRGBA *self, char *name)
{
	int i;

	if (strcmp(name, "__members__") == 0)
		return Py_BuildValue("[s,s,s,s]", "R", "G", "B", "A");

	else if (!strcmp(name, "R") || !strcmp(name, "r")) i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1;
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2;
	else if (!strcmp(name, "A") || !strcmp(name, "a")) i = 3;
	else
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
			"attribute not found"));

	return Py_BuildValue("b", *(self->rgba[i]));
}

/*****************************************************************************/
/* Function:		charRGBA_setAttr																						 */
/* Description: This is a callback function for the BPy_charRGBA type. It is */
/*							the function that changes BPy_charRGBA member variables.		 */
/*****************************************************************************/
static int charRGBA_setAttr (BPy_charRGBA *self, char *name, PyObject *v)
{
	char value;

	if (!PyArg_Parse (v, "b", &value))
		return EXPP_ReturnIntError (PyExc_TypeError,
			"expected char argument");

	value = EXPP_ClampInt(value, 0, 255);

	if (!strcmp(name, "R") || !strcmp(name, "r"))
		*(self->rgba[0]) = value;

	else if (!strcmp(name, "G") || !strcmp(name, "g"))
		*(self->rgba[1]) = value;

	else if (!strcmp(name, "B") || !strcmp(name, "b"))
		*(self->rgba[2]) = value;

	else if (!strcmp(name, "A") || !strcmp(name, "a"))
		*(self->rgba[3]) = value;

	else return (EXPP_ReturnIntError (PyExc_AttributeError,
		"attribute not found"));

	return 0;
}

/*****************************************************************************/
/* Section:		 charRGBA as Mapping																					 */
/*						 These functions provide code to access charRGBA objects as		 */
/*						 mappings.																										 */
/*****************************************************************************/
static int charRGBALength(BPy_charRGBA *self)
{
	return 4;
}

static PyObject *charRGBASubscript(BPy_charRGBA *self, PyObject *key)
{
	char *name = NULL;
	int i;

	if (PyNumber_Check(key)) return charRGBAItem(self, (int)PyInt_AsLong(key));

	if (!PyArg_ParseTuple(key, "s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected int or string argument");

	if (!strcmp(name, "R") || !strcmp(name, "r"))			 i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1; 
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2; 
	else if (!strcmp(name, "A") || !strcmp(name, "a")) i = 3; 
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError, name);

	return Py_BuildValue("b", *(self->rgba[i]));
}

static int charRGBAAssSubscript(BPy_charRGBA *self, PyObject *key, PyObject *v)
{
	char *name = NULL;
	int i;

	if (!PyNumber_Check(v)) return EXPP_ReturnIntError(PyExc_TypeError,
		"value to assign must be a number");

	if (PyNumber_Check(key))
		return charRGBAAssItem(self, (int)PyInt_AsLong(key), v);

	if (!PyArg_Parse(key, "s", &name))
		return EXPP_ReturnIntError (PyExc_TypeError,
			"expected int or string argument");

	if (!strcmp(name, "R") || !strcmp(name, "r"))			 i = 0;
	else if (!strcmp(name, "G") || !strcmp(name, "g")) i = 1; 
	else if (!strcmp(name, "B") || !strcmp(name, "b")) i = 2; 
	else if (!strcmp(name, "A") || !strcmp(name, "a")) i = 3; 
	else
		return EXPP_ReturnIntError (PyExc_AttributeError, name);

	*(self->rgba[i]) = EXPP_ClampInt(PyInt_AsLong(v), 0, 255);

	return 0;
}

/*****************************************************************************/
/* Section:		 charRGBA as Sequence																					 */
/*						 These functions provide code to access charRGBA objects as		 */
/*						 sequences.																										 */
/*****************************************************************************/
static PyObject *charRGBAItem(BPy_charRGBA *self, int i)
{
	if (i < 0 || i >= 4)
		return EXPP_ReturnPyObjError (PyExc_IndexError,
			"array index out of range");

	return Py_BuildValue("b", *(self->rgba[i]));
}

static PyObject *charRGBASlice(BPy_charRGBA *self, int begin, int end)
{
	PyObject *list;
	int count;

	if (begin < 0) begin = 0;
	if (end > 4) end = 4;
	if (begin > end) begin = end;

	list = PyList_New(end - begin);

	for (count = begin; count < end; count++)
		PyList_SetItem(list, count - begin,
			PyInt_FromLong(*(self->rgba[count])));

	return list;
}

static int charRGBAAssItem(BPy_charRGBA *self, int i, PyObject *ob)
{
	if (i < 0 || i >= 4)
		return EXPP_ReturnIntError(PyExc_IndexError,
			"array assignment index out of range");

	if (!PyNumber_Check(ob))
		return EXPP_ReturnIntError(PyExc_IndexError,
			"color component must be a number");

	*(self->rgba[i]) = EXPP_ClampInt(PyInt_AsLong(ob), 0, 255);

	return 0;
}

static int charRGBAAssSlice(BPy_charRGBA *self, int begin, int end,
	PyObject *seq)
{
	int count;
	
	if (begin < 0) begin = 0;
	if (end > 4) end = 4;
	if (begin > end) begin = end;

	if (!PySequence_Check(seq))
		return EXPP_ReturnIntError(PyExc_TypeError,
			"illegal argument type for built-in operation");

	if (PySequence_Length(seq) != (end - begin))
		return EXPP_ReturnIntError(PyExc_TypeError,
			"size mismatch in slice assignment");

	for (count = begin; count < end; count++) {
		char value;
		PyObject *ob = PySequence_GetItem(seq, count);

		if (!PyArg_Parse(ob, "b", &value)) {
			Py_DECREF(ob);
			return -1;
		}

		*(self->rgba[count]) = EXPP_ClampInt(value, 0, 255);

		Py_DECREF(ob);
	}

	return 0;
}

/*****************************************************************************/
/* Function:		charRGBA_repr																								 */
/* Description: This is a callback function for the BPy_charRGBA type. It		 */
/*							builds a meaninful string to represent charRGBA objects.		 */
/*****************************************************************************/
static PyObject *charRGBA_repr (BPy_charRGBA *self)
{
	char r, g, b, a;

	r = *(self->rgba[0]);
	g = *(self->rgba[1]);
	b = *(self->rgba[2]);
	a = *(self->rgba[3]);

	return PyString_FromFormat("[%d, %d, %d, %d]", r, g, b, a);
}
