/**
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/***********************/

/* (c) Jan Walter 2000 */
/***********************/

/* CVS */
/* $Author$ */
/* $Date$ */
/* $RCSfile$ */
/* $Revision$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Python.h"

/************************************/
/* struct for the class "TestClass" */
/************************************/

typedef struct {
  PyObject_HEAD
  PyObject* memberVariable;
} testclass;

staticforward PyTypeObject TestclassType;

/************************************/
/* make instance of class TestClass */
/************************************/

static testclass*
newtestclass()
{
  testclass* tc;
  tc = PyObject_NEW(testclass, &TestclassType);
  if (tc == NULL)
    {
      return NULL;
    }
  tc->memberVariable = PyString_FromString("initValue");

  return tc;
}

/*********************************/
/* instance methods of TestClass */
/*********************************/

static PyObject*
testclass_memberFunction(PyObject* tc, PyObject* args)
{
  PyRun_SimpleString("print \"inside memberFunction() ...\"\n");

  Py_INCREF(Py_None);

  return Py_None;
}

static struct PyMethodDef testclass_methods[] = {
  { "memberFunction", testclass_memberFunction, METH_VARARGS },
  { NULL, NULL }
};

/*********************************/
/* basic operations on TestClass */
/*********************************/

static PyObject*
testclass_dealloc(testclass* tc)
{
  PyMem_DEL(tc);
}

static PyObject*
testclass_getattr(testclass* tc, char* name)
{
  if (strcmp(name, "memberVariable") == 0)
    {
      return Py_BuildValue("O", tc->memberVariable);
    }
  else
    {
      return Py_FindMethod(testclass_methods, (PyObject*) tc, name);
    }  
}

static int
testclass_setattr(testclass* tc, char* name, PyObject* attr)
{
  if (strcmp(name, "memberVariable") == 0)
    {
      Py_XDECREF(tc->memberVariable);
      tc->memberVariable = attr;
      Py_INCREF(attr);
    }
  else
    {
      PyErr_SetString(PyExc_AttributeError, name);

      return -1;
    }  

  return 0;
}

/************************************/
/* table of functions for TestClass */
/************************************/

static PyTypeObject TestclassType = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,                               /* ob_size */
  "testclass",                     /* tp_name */
  sizeof(testclass),               /* tp_basicsize */
  0,                               /* tp_itemsize */
  (destructor) testclass_dealloc,  /* tp_dealloc */
  (printfunc) 0,                   /* tp_print */
  (getattrfunc) testclass_getattr, /* tp_getattr */
  (setattrfunc) testclass_setattr, /* tp_setattr */
  (cmpfunc) 0,                     /* tp_compare */
  (reprfunc) 0,                    /* tp_repr */
  /* there is more ... */
}; /* see Include/object.h */

/***********************************/
/* constructor for class TestClass */
/***********************************/

static PyObject*
testclass_new(PyObject* self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, ""))
    {
      return NULL;
    }

  return (PyObject*) newtestclass();
}

/*********************/
/* module testmodule */
/*********************/

static struct PyMethodDef testmodule_methods[] = {
  { "TestClass", testclass_new, METH_VARARGS },
  { NULL, NULL }
};

void
inittestmodule()
{
  (void) Py_InitModule("testmodule", testmodule_methods);
}
