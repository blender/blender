/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction1DVoid.h"

#include "../BPy_Convert.h"
#include "../BPy_IntegrationType.h"
#include "../BPy_Interface1D.h"

#include "UnaryFunction1D_void/BPy_ChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_IncrementChainingTimeStampF1D.h"
#include "UnaryFunction1D_void/BPy_TimeStampF1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DVoid_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction1DVoid_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction1DVoid_Type);
  PyModule_AddObject(module, "UnaryFunction1DVoid", (PyObject *)&UnaryFunction1DVoid_Type);

  if (PyType_Ready(&ChainingTimeStampF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ChainingTimeStampF1D_Type);
  PyModule_AddObject(module, "ChainingTimeStampF1D", (PyObject *)&ChainingTimeStampF1D_Type);

  if (PyType_Ready(&IncrementChainingTimeStampF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&IncrementChainingTimeStampF1D_Type);
  PyModule_AddObject(
      module, "IncrementChainingTimeStampF1D", (PyObject *)&IncrementChainingTimeStampF1D_Type);

  if (PyType_Ready(&TimeStampF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&TimeStampF1D_Type);
  PyModule_AddObject(module, "TimeStampF1D", (PyObject *)&TimeStampF1D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    UnaryFunction1DVoid___doc__,
    "Class hierarchy: :class:`UnaryFunction1D` > :class:`UnaryFunction1DVoid`\n"
    "\n"
    "Base class for unary functions (functors) working on\n"
    ":class:`Interface1D`.\n"
    "\n"
    ".. method:: __init__()\n"
    "             __init__(integration_type)\n"
    "\n"
    "   Builds a unary 1D function using either a default constructor\n"
    "   or the integration method given as an argument.\n"
    "\n"
    "   :arg integration_type: An integration method.\n"
    "   :type integration_type: :class:`IntegrationType`\n");

static int UnaryFunction1DVoid___init__(BPy_UnaryFunction1DVoid *self,
                                        PyObject *args,
                                        PyObject *kwds)
{
  static const char *kwlist[] = {"integration", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj))
  {
    return -1;
  }

  if (!obj) {
    self->uf1D_void = new UnaryFunction1D_void();
  }
  else {
    self->uf1D_void = new UnaryFunction1D_void(IntegrationType_from_BPy_IntegrationType(obj));
  }

  self->uf1D_void->py_uf1D = (PyObject *)self;

  return 0;
}

static void UnaryFunction1DVoid___dealloc__(BPy_UnaryFunction1DVoid *self)
{
  delete self->uf1D_void;
  UnaryFunction1D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction1DVoid___repr__(BPy_UnaryFunction1DVoid *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf1D_void);
}

static PyObject *UnaryFunction1DVoid___call__(BPy_UnaryFunction1DVoid *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  static const char *kwlist[] = {"inter", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &obj)) {
    return nullptr;
  }

  if (typeid(*(self->uf1D_void)) == typeid(UnaryFunction1D_void)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf1D_void->operator()(*(((BPy_Interface1D *)obj)->if1D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  Py_RETURN_NONE;
}

/*----------------------UnaryFunction1DVoid get/setters ----------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    integration_type_doc,
    "The integration method.\n"
    "\n"
    ":type: :class:`IntegrationType`");

static PyObject *integration_type_get(BPy_UnaryFunction1DVoid *self, void * /*closure*/)
{
  return BPy_IntegrationType_from_IntegrationType(self->uf1D_void->getIntegrationType());
}

static int integration_type_set(BPy_UnaryFunction1DVoid *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_IntegrationType_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an IntegrationType");
    return -1;
  }
  self->uf1D_void->setIntegrationType(IntegrationType_from_BPy_IntegrationType(value));
  return 0;
}

static PyGetSetDef BPy_UnaryFunction1DVoid_getseters[] = {
    {"integration_type",
     (getter)integration_type_get,
     (setter)integration_type_set,
     integration_type_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction1DVoid type definition ------------------------------*/

PyTypeObject UnaryFunction1DVoid_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryFunction1DVoid",
    /*tp_basicsize*/ sizeof(BPy_UnaryFunction1DVoid),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryFunction1DVoid___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryFunction1DVoid___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryFunction1DVoid___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryFunction1DVoid___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_UnaryFunction1DVoid_getseters,
    /*tp_base*/ &UnaryFunction1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)UnaryFunction1DVoid___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
