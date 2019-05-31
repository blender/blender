/*
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
 */

/** \file
 * \ingroup freestyle
 */

#include "BPy_BinaryPredicate0D.h"

#include "BPy_Convert.h"
#include "BPy_Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int BinaryPredicate0D_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&BinaryPredicate0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&BinaryPredicate0D_Type);
  PyModule_AddObject(module, "BinaryPredicate0D", (PyObject *)&BinaryPredicate0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char BinaryPredicate0D___doc__[] =
    "Base class for binary predicates working on :class:`Interface0D`\n"
    "objects.  A BinaryPredicate0D is typically an ordering relation\n"
    "between two Interface0D objects.  The predicate evaluates a relation\n"
    "between the two Interface0D instances and returns a boolean value (true\n"
    "or false).  It is used by invoking the __call__() method.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __call__(inter1, inter2)\n"
    "\n"
    "   Must be overload by inherited classes.  It evaluates a relation\n"
    "   between two Interface0D objects.\n"
    "\n"
    "   :arg inter1: The first Interface0D object.\n"
    "   :type inter1: :class:`Interface0D`\n"
    "   :arg inter2: The second Interface0D object.\n"
    "   :type inter2: :class:`Interface0D`\n"
    "   :return: True or false.\n"
    "   :rtype: bool\n";

static int BinaryPredicate0D___init__(BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->bp0D = new BinaryPredicate0D();
  self->bp0D->py_bp0D = (PyObject *)self;
  return 0;
}

static void BinaryPredicate0D___dealloc__(BPy_BinaryPredicate0D *self)
{
  if (self->bp0D) {
    delete self->bp0D;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *BinaryPredicate0D___repr__(BPy_BinaryPredicate0D *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->bp0D);
}

static PyObject *BinaryPredicate0D___call__(BPy_BinaryPredicate0D *self,
                                            PyObject *args,
                                            PyObject *kwds)
{
  static const char *kwlist[] = {"inter1", "inter2", NULL};
  BPy_Interface0D *obj1, *obj2;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kwds,
                                   "O!O!",
                                   (char **)kwlist,
                                   &Interface0D_Type,
                                   &obj1,
                                   &Interface0D_Type,
                                   &obj2)) {
    return NULL;
  }
  if (typeid(*(self->bp0D)) == typeid(BinaryPredicate0D)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->bp0D->operator()(*(obj1->if0D), *(obj2->if0D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyBool_from_bool(self->bp0D->result);
}

/*----------------------BinaryPredicate0D get/setters ----------------------------*/

PyDoc_STRVAR(BinaryPredicate0D_name_doc,
             "The name of the binary 0D predicate.\n"
             "\n"
             ":type: str");

static PyObject *BinaryPredicate0D_name_get(BPy_BinaryPredicate0D *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_BinaryPredicate0D_getseters[] = {
    {(char *)"name",
     (getter)BinaryPredicate0D_name_get,
     (setter)NULL,
     (char *)BinaryPredicate0D_name_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_BinaryPredicate0D type definition ------------------------------*/

PyTypeObject BinaryPredicate0D_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "BinaryPredicate0D", /* tp_name */
    sizeof(BPy_BinaryPredicate0D),                      /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    (destructor)BinaryPredicate0D___dealloc__,          /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_reserved */
    (reprfunc)BinaryPredicate0D___repr__,               /* tp_repr */
    0,                                                  /* tp_as_number */
    0,                                                  /* tp_as_sequence */
    0,                                                  /* tp_as_mapping */
    0,                                                  /* tp_hash  */
    (ternaryfunc)BinaryPredicate0D___call__,            /* tp_call */
    0,                                                  /* tp_str */
    0,                                                  /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,           /* tp_flags */
    BinaryPredicate0D___doc__,                          /* tp_doc */
    0,                                                  /* tp_traverse */
    0,                                                  /* tp_clear */
    0,                                                  /* tp_richcompare */
    0,                                                  /* tp_weaklistoffset */
    0,                                                  /* tp_iter */
    0,                                                  /* tp_iternext */
    0,                                                  /* tp_methods */
    0,                                                  /* tp_members */
    BPy_BinaryPredicate0D_getseters,                    /* tp_getset */
    0,                                                  /* tp_base */
    0,                                                  /* tp_dict */
    0,                                                  /* tp_descr_get */
    0,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    (initproc)BinaryPredicate0D___init__,               /* tp_init */
    0,                                                  /* tp_alloc */
    PyType_GenericNew,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
