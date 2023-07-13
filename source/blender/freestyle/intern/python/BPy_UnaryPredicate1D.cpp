/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryPredicate1D_Init(PyObject *module)
{
  if (module == nullptr) {
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
    "Base class for unary predicates that work on :class:`Interface1D`. A\n"
    "UnaryPredicate1D is a functor that evaluates a condition on a\n"
    "Interface1D and returns true or false depending on whether this\n"
    "condition is satisfied or not. The UnaryPredicate1D is used by\n"
    "invoking its __call__() method. Any inherited class must overload the\n"
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
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->up1D = new UnaryPredicate1D();
  self->up1D->py_up1D = (PyObject *)self;
  return 0;
}

static void UnaryPredicate1D___dealloc__(BPy_UnaryPredicate1D *self)
{
  delete self->up1D;
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
  static const char *kwlist[] = {"inter", nullptr};
  PyObject *py_if1D;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &py_if1D))
  {
    return nullptr;
  }

  Interface1D *if1D = ((BPy_Interface1D *)py_if1D)->if1D;

  if (!if1D) {
    string class_name(Py_TYPE(self)->tp_name);
    PyErr_SetString(PyExc_RuntimeError, (class_name + " has no Interface1D").c_str());
    return nullptr;
  }
  if (typeid(*(self->up1D)) == typeid(UnaryPredicate1D)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->up1D->operator()(*if1D) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return PyBool_from_bool(self->up1D->result);
}

/*----------------------UnaryPredicate1D get/setters ----------------------------*/

PyDoc_STRVAR(UnaryPredicate1D_name_doc,
             "The name of the unary 1D predicate.\n"
             "\n"
             ":type: str");

static PyObject *UnaryPredicate1D_name_get(BPy_UnaryPredicate1D *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_UnaryPredicate1D_getseters[] = {
    {"name",
     (getter)UnaryPredicate1D_name_get,
     (setter) nullptr,
     UnaryPredicate1D_name_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryPredicate1D type definition ------------------------------*/

PyTypeObject UnaryPredicate1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryPredicate1D",
    /*tp_basicsize*/ sizeof(BPy_UnaryPredicate1D),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryPredicate1D___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryPredicate1D___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryPredicate1D___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryPredicate1D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_UnaryPredicate1D_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)UnaryPredicate1D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
