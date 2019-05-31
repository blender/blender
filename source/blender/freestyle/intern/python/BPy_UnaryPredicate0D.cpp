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

#include "BPy_UnaryPredicate0D.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "UnaryPredicate0D/BPy_FalseUP0D.h"
#include "UnaryPredicate0D/BPy_TrueUP0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryPredicate0D_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&UnaryPredicate0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryPredicate0D_Type);
  PyModule_AddObject(module, "UnaryPredicate0D", (PyObject *)&UnaryPredicate0D_Type);

  if (PyType_Ready(&FalseUP0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FalseUP0D_Type);
  PyModule_AddObject(module, "FalseUP0D", (PyObject *)&FalseUP0D_Type);

  if (PyType_Ready(&TrueUP0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&TrueUP0D_Type);
  PyModule_AddObject(module, "TrueUP0D", (PyObject *)&TrueUP0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryPredicate0D___doc__[] =
    "Base class for unary predicates that work on\n"
    ":class:`Interface0DIterator`.  A UnaryPredicate0D is a functor that\n"
    "evaluates a condition on an Interface0DIterator and returns true or\n"
    "false depending on whether this condition is satisfied or not.  The\n"
    "UnaryPredicate0D is used by invoking its __call__() method.  Any\n"
    "inherited class must overload the __call__() method.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Must be overload by inherited classes.\n"
    "\n"
    "   :arg it: The Interface0DIterator pointing onto the Interface0D at\n"
    "      which we wish to evaluate the predicate.\n"
    "   :type it: :class:`Interface0DIterator`\n"
    "   :return: True if the condition is satisfied, false otherwise.\n"
    "   :rtype: bool\n";

static int UnaryPredicate0D___init__(BPy_UnaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->up0D = new UnaryPredicate0D();
  self->up0D->py_up0D = (PyObject *)self;
  return 0;
}

static void UnaryPredicate0D___dealloc__(BPy_UnaryPredicate0D *self)
{
  if (self->up0D) {
    delete self->up0D;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *UnaryPredicate0D___repr__(BPy_UnaryPredicate0D *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->up0D);
}

static PyObject *UnaryPredicate0D___call__(BPy_UnaryPredicate0D *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {"it", NULL};
  PyObject *py_if0D_it;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &py_if0D_it)) {
    return NULL;
  }

  Interface0DIterator *if0D_it = ((BPy_Interface0DIterator *)py_if0D_it)->if0D_it;

  if (!if0D_it) {
    string class_name(Py_TYPE(self)->tp_name);
    PyErr_SetString(PyExc_RuntimeError, (class_name + " has no Interface0DIterator").c_str());
    return NULL;
  }
  if (typeid(*(self->up0D)) == typeid(UnaryPredicate0D)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->up0D->operator()(*if0D_it) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyBool_from_bool(self->up0D->result);
}

/*----------------------UnaryPredicate0D get/setters ----------------------------*/

PyDoc_STRVAR(UnaryPredicate0D_name_doc,
             "The name of the unary 0D predicate.\n"
             "\n"
             ":type: str");

static PyObject *UnaryPredicate0D_name_get(BPy_UnaryPredicate0D *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_UnaryPredicate0D_getseters[] = {
    {(char *)"name",
     (getter)UnaryPredicate0D_name_get,
     (setter)NULL,
     (char *)UnaryPredicate0D_name_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_UnaryPredicate0D type definition ------------------------------*/

PyTypeObject UnaryPredicate0D_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryPredicate0D", /* tp_name */
    sizeof(BPy_UnaryPredicate0D),                      /* tp_basicsize */
    0,                                                 /* tp_itemsize */
    (destructor)UnaryPredicate0D___dealloc__,          /* tp_dealloc */
    0,                                                 /* tp_print */
    0,                                                 /* tp_getattr */
    0,                                                 /* tp_setattr */
    0,                                                 /* tp_reserved */
    (reprfunc)UnaryPredicate0D___repr__,               /* tp_repr */
    0,                                                 /* tp_as_number */
    0,                                                 /* tp_as_sequence */
    0,                                                 /* tp_as_mapping */
    0,                                                 /* tp_hash  */
    (ternaryfunc)UnaryPredicate0D___call__,            /* tp_call */
    0,                                                 /* tp_str */
    0,                                                 /* tp_getattro */
    0,                                                 /* tp_setattro */
    0,                                                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,          /* tp_flags */
    UnaryPredicate0D___doc__,                          /* tp_doc */
    0,                                                 /* tp_traverse */
    0,                                                 /* tp_clear */
    0,                                                 /* tp_richcompare */
    0,                                                 /* tp_weaklistoffset */
    0,                                                 /* tp_iter */
    0,                                                 /* tp_iternext */
    0,                                                 /* tp_methods */
    0,                                                 /* tp_members */
    BPy_UnaryPredicate0D_getseters,                    /* tp_getset */
    0,                                                 /* tp_base */
    0,                                                 /* tp_dict */
    0,                                                 /* tp_descr_get */
    0,                                                 /* tp_descr_set */
    0,                                                 /* tp_dictoffset */
    (initproc)UnaryPredicate0D___init__,               /* tp_init */
    0,                                                 /* tp_alloc */
    PyType_GenericNew,                                 /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
