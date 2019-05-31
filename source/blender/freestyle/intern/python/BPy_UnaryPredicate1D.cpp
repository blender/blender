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

#include "BPy_UnaryPredicate1D.h"

#include "BPy_Convert.h"
#include "BPy_Interface1D.h"

#include "UnaryPredicate1D/BPy_ContourUP1D.h"
#include "UnaryPredicate1D/BPy_DensityLowerThanUP1D.h"
#include "UnaryPredicate1D/BPy_EqualToChainingTimeStampUP1D.h"
#include "UnaryPredicate1D/BPy_EqualToTimeStampUP1D.h"
#include "UnaryPredicate1D/BPy_ExternalContourUP1D.h"
#include "UnaryPredicate1D/BPy_FalseUP1D.h"
#include "UnaryPredicate1D/BPy_QuantitativeInvisibilityUP1D.h"
#include "UnaryPredicate1D/BPy_ShapeUP1D.h"
#include "UnaryPredicate1D/BPy_TrueUP1D.h"
#include "UnaryPredicate1D/BPy_WithinImageBoundaryUP1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryPredicate1D_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&UnaryPredicate1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryPredicate1D_Type);
  PyModule_AddObject(module, "UnaryPredicate1D", (PyObject *)&UnaryPredicate1D_Type);

  if (PyType_Ready(&ContourUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ContourUP1D_Type);
  PyModule_AddObject(module, "ContourUP1D", (PyObject *)&ContourUP1D_Type);

  if (PyType_Ready(&DensityLowerThanUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&DensityLowerThanUP1D_Type);
  PyModule_AddObject(module, "DensityLowerThanUP1D", (PyObject *)&DensityLowerThanUP1D_Type);

  if (PyType_Ready(&EqualToChainingTimeStampUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&EqualToChainingTimeStampUP1D_Type);
  PyModule_AddObject(
      module, "EqualToChainingTimeStampUP1D", (PyObject *)&EqualToChainingTimeStampUP1D_Type);

  if (PyType_Ready(&EqualToTimeStampUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&EqualToTimeStampUP1D_Type);
  PyModule_AddObject(module, "EqualToTimeStampUP1D", (PyObject *)&EqualToTimeStampUP1D_Type);

  if (PyType_Ready(&ExternalContourUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ExternalContourUP1D_Type);
  PyModule_AddObject(module, "ExternalContourUP1D", (PyObject *)&ExternalContourUP1D_Type);

  if (PyType_Ready(&FalseUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FalseUP1D_Type);
  PyModule_AddObject(module, "FalseUP1D", (PyObject *)&FalseUP1D_Type);

  if (PyType_Ready(&QuantitativeInvisibilityUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&QuantitativeInvisibilityUP1D_Type);
  PyModule_AddObject(
      module, "QuantitativeInvisibilityUP1D", (PyObject *)&QuantitativeInvisibilityUP1D_Type);

  if (PyType_Ready(&ShapeUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ShapeUP1D_Type);
  PyModule_AddObject(module, "ShapeUP1D", (PyObject *)&ShapeUP1D_Type);

  if (PyType_Ready(&TrueUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&TrueUP1D_Type);
  PyModule_AddObject(module, "TrueUP1D", (PyObject *)&TrueUP1D_Type);

  if (PyType_Ready(&WithinImageBoundaryUP1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&WithinImageBoundaryUP1D_Type);
  PyModule_AddObject(module, "WithinImageBoundaryUP1D", (PyObject *)&WithinImageBoundaryUP1D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryPredicate1D___doc__[] =
    "Base class for unary predicates that work on :class:`Interface1D`.  A\n"
    "UnaryPredicate1D is a functor that evaluates a condition on a\n"
    "Interface1D and returns true or false depending on whether this\n"
    "condition is satisfied or not.  The UnaryPredicate1D is used by\n"
    "invoking its __call__() method.  Any inherited class must overload the\n"
    "__call__() method.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Must be overload by inherited classes.\n"
    "\n"
    "   :arg inter: The Interface1D on which we wish to evaluate the predicate.\n"
    "   :type inter: :class:`Interface1D`\n"
    "   :return: True if the condition is satisfied, false otherwise.\n"
    "   :rtype: bool\n";

static int UnaryPredicate1D___init__(BPy_UnaryPredicate1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->up1D = new UnaryPredicate1D();
  self->up1D->py_up1D = (PyObject *)self;
  return 0;
}

static void UnaryPredicate1D___dealloc__(BPy_UnaryPredicate1D *self)
{
  if (self->up1D) {
    delete self->up1D;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *UnaryPredicate1D___repr__(BPy_UnaryPredicate1D *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->up1D);
}

static PyObject *UnaryPredicate1D___call__(BPy_UnaryPredicate1D *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {"inter", NULL};
  PyObject *py_if1D;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &py_if1D)) {
    return NULL;
  }

  Interface1D *if1D = ((BPy_Interface1D *)py_if1D)->if1D;

  if (!if1D) {
    string class_name(Py_TYPE(self)->tp_name);
    PyErr_SetString(PyExc_RuntimeError, (class_name + " has no Interface1D").c_str());
    return NULL;
  }
  if (typeid(*(self->up1D)) == typeid(UnaryPredicate1D)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->up1D->operator()(*if1D) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyBool_from_bool(self->up1D->result);
}

/*----------------------UnaryPredicate1D get/setters ----------------------------*/

PyDoc_STRVAR(UnaryPredicate1D_name_doc,
             "The name of the unary 1D predicate.\n"
             "\n"
             ":type: str");

static PyObject *UnaryPredicate1D_name_get(BPy_UnaryPredicate1D *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_UnaryPredicate1D_getseters[] = {
    {(char *)"name",
     (getter)UnaryPredicate1D_name_get,
     (setter)NULL,
     (char *)UnaryPredicate1D_name_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_UnaryPredicate1D type definition ------------------------------*/

PyTypeObject UnaryPredicate1D_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryPredicate1D", /* tp_name */
    sizeof(BPy_UnaryPredicate1D),                      /* tp_basicsize */
    0,                                                 /* tp_itemsize */
    (destructor)UnaryPredicate1D___dealloc__,          /* tp_dealloc */
    0,                                                 /* tp_print */
    0,                                                 /* tp_getattr */
    0,                                                 /* tp_setattr */
    0,                                                 /* tp_reserved */
    (reprfunc)UnaryPredicate1D___repr__,               /* tp_repr */
    0,                                                 /* tp_as_number */
    0,                                                 /* tp_as_sequence */
    0,                                                 /* tp_as_mapping */
    0,                                                 /* tp_hash  */
    (ternaryfunc)UnaryPredicate1D___call__,            /* tp_call */
    0,                                                 /* tp_str */
    0,                                                 /* tp_getattro */
    0,                                                 /* tp_setattro */
    0,                                                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,          /* tp_flags */
    UnaryPredicate1D___doc__,                          /* tp_doc */
    0,                                                 /* tp_traverse */
    0,                                                 /* tp_clear */
    0,                                                 /* tp_richcompare */
    0,                                                 /* tp_weaklistoffset */
    0,                                                 /* tp_iter */
    0,                                                 /* tp_iternext */
    0,                                                 /* tp_methods */
    0,                                                 /* tp_members */
    BPy_UnaryPredicate1D_getseters,                    /* tp_getset */
    0,                                                 /* tp_base */
    0,                                                 /* tp_dict */
    0,                                                 /* tp_descr_get */
    0,                                                 /* tp_descr_set */
    0,                                                 /* tp_dictoffset */
    (initproc)UnaryPredicate1D___init__,               /* tp_init */
    0,                                                 /* tp_alloc */
    PyType_GenericNew,                                 /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
