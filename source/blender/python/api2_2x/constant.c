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

#include "constant.h"

/* This file is heavily based on the old bpython Constant object code in
   Blender */

/*****************************************************************************/
/* Python constant_Type callback function prototypes:                        */
/*****************************************************************************/
static void constant_dealloc (BPy_constant *cam);
static PyObject *constant_getAttr (BPy_constant *cam, char *name);
static PyObject *constant_repr (BPy_constant *cam);
static int constantLength(BPy_constant *self);
static PyObject *constantSubscript(BPy_constant *self, PyObject *key);
static int constantAssSubscript(BPy_constant *self, PyObject *who,
                                PyObject *cares);

/*****************************************************************************/
/* Python constant_Type Mapping Methods table:                               */
/*****************************************************************************/
static PyMappingMethods constantAsMapping =
{
  (inquiry)constantLength,             /* mp_length        */
  (binaryfunc)constantSubscript,       /* mp_subscript     */
  (objobjargproc)constantAssSubscript, /* mp_ass_subscript */
};

/*****************************************************************************/
/* Python constant_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject constant_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "Blender constant",                     /* tp_name */
  sizeof (BPy_constant),                  /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)constant_dealloc,           /* tp_dealloc */
  0,                                      /* tp_print */
  (getattrfunc)constant_getAttr,          /* tp_getattr */
  0,                                      /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)constant_repr,                /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  &constantAsMapping,                     /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  0,                                      /* tp_methods */
  0,                                      /* tp_members */
};

/*****************************************************************************/
/* Function:              constant_New                                       */
/*****************************************************************************/
static PyObject *new_const(void);

PyObject *M_constant_New(void) /* can't be static, we call it in other files */
{
  return new_const();
}

static PyObject *new_const(void)
{ /* this is the static one */
  BPy_constant *constant;

  constant_Type.ob_type = &PyType_Type;

  constant = (BPy_constant *)PyObject_NEW(BPy_constant, &constant_Type);

  if (constant == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create constant object"));

  if ((constant->dict = PyDict_New()) == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                    "couldn't create constant object's dictionary"));
  
  return (PyObject *)constant;
}

/*****************************************************************************/
/* Python BPy_constant methods:                                              */
/*****************************************************************************/
int constant_insert (BPy_constant *self, char *name, PyObject *value)
{
	return PyDict_SetItemString (self->dict, name, value);
}

/*****************************************************************************/
/* Function:    constant_dealloc                                              */
/* Description: This is a callback function for the BPy_constant type. It is */
/*              the destructor function.                                     */
/*****************************************************************************/
static void constant_dealloc (BPy_constant *self)
{
  Py_DECREF(self->dict);
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    constant_getAttr                                              */
/* Description: This is a callback function for the BPy_constant type. It is */
/*              the function that accesses BPy_constant member variables and */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *constant_getAttr (BPy_constant *self, char *name)
{
  if (self->dict)
  {
    PyObject *v;

    if (!strcmp(name, "__members__"))
      return PyDict_Keys(self->dict);
 
    v  = PyDict_GetItemString(self->dict, name);
    if (v) {
      Py_INCREF(v); /* was a borrowed ref */
      return v;
    }

    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
                                     "attribute not found"));
  }
  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "constant object lacks a dictionary"));
}

/*****************************************************************************/
/* Section:    Sequence Mapping                                              */
/*             These functions provide code to access constant objects as    */
/*             mappings.                                                     */
/*****************************************************************************/
static int constantLength(BPy_constant *self)
{
  return 0;
}

static PyObject *constantSubscript(BPy_constant *self, PyObject *key)
{
  if (self->dict) {
    PyObject *v = PyDict_GetItem(self->dict, key);

		if (v) {
      Py_INCREF(v);
      return v;
    }
  }

  return NULL;
}

static int constantAssSubscript(BPy_constant *self, PyObject *who,
                                PyObject *cares)
{
  /* no user assignments allowed */
  return 0;
}

/*****************************************************************************/
/* Function:    constant_repr                                                */
/* Description: This is a callback function for the BPy_constant type. It    */
/*              builds a meaninful string to represent constant objects.     */
/*****************************************************************************/
static PyObject *constant_repr (BPy_constant *self)
{
  PyObject *repr = PyObject_Repr(self->dict);
  return repr;
}
