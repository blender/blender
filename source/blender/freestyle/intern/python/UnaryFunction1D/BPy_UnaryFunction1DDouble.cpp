/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction1DDouble.h"

#include "../BPy_Convert.h"
#include "../BPy_IntegrationType.h"
#include "../BPy_Interface1D.h"

#include "UnaryFunction1D_double/BPy_Curvature2DAngleF1D.h"
#include "UnaryFunction1D_double/BPy_DensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetCompleteViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetDirectionalViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedXF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedYF1D.h"
#include "UnaryFunction1D_double/BPy_GetProjectedZF1D.h"
#include "UnaryFunction1D_double/BPy_GetSteerableViewMapDensityF1D.h"
#include "UnaryFunction1D_double/BPy_GetViewMapGradientNormF1D.h"
#include "UnaryFunction1D_double/BPy_GetXF1D.h"
#include "UnaryFunction1D_double/BPy_GetYF1D.h"
#include "UnaryFunction1D_double/BPy_GetZF1D.h"
#include "UnaryFunction1D_double/BPy_LocalAverageDepthF1D.h"
#include "UnaryFunction1D_double/BPy_ZDiscontinuityF1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DDouble_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction1DDouble_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction1DDouble_Type);
  PyModule_AddObject(module, "UnaryFunction1DDouble", (PyObject *)&UnaryFunction1DDouble_Type);

  if (PyType_Ready(&DensityF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&DensityF1D_Type);
  PyModule_AddObject(module, "DensityF1D", (PyObject *)&DensityF1D_Type);

  if (PyType_Ready(&Curvature2DAngleF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Curvature2DAngleF1D_Type);
  PyModule_AddObject(module, "Curvature2DAngleF1D", (PyObject *)&Curvature2DAngleF1D_Type);

  if (PyType_Ready(&GetCompleteViewMapDensityF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetCompleteViewMapDensityF1D_Type);
  PyModule_AddObject(
      module, "GetCompleteViewMapDensityF1D", (PyObject *)&GetCompleteViewMapDensityF1D_Type);

  if (PyType_Ready(&GetDirectionalViewMapDensityF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetDirectionalViewMapDensityF1D_Type);
  PyModule_AddObject(module,
                     "GetDirectionalViewMapDensityF1D",
                     (PyObject *)&GetDirectionalViewMapDensityF1D_Type);

  if (PyType_Ready(&GetProjectedXF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedXF1D_Type);
  PyModule_AddObject(module, "GetProjectedXF1D", (PyObject *)&GetProjectedXF1D_Type);

  if (PyType_Ready(&GetProjectedYF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedYF1D_Type);
  PyModule_AddObject(module, "GetProjectedYF1D", (PyObject *)&GetProjectedYF1D_Type);

  if (PyType_Ready(&GetProjectedZF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedZF1D_Type);
  PyModule_AddObject(module, "GetProjectedZF1D", (PyObject *)&GetProjectedZF1D_Type);

  if (PyType_Ready(&GetSteerableViewMapDensityF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetSteerableViewMapDensityF1D_Type);
  PyModule_AddObject(
      module, "GetSteerableViewMapDensityF1D", (PyObject *)&GetSteerableViewMapDensityF1D_Type);

  if (PyType_Ready(&GetViewMapGradientNormF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetViewMapGradientNormF1D_Type);
  PyModule_AddObject(
      module, "GetViewMapGradientNormF1D", (PyObject *)&GetViewMapGradientNormF1D_Type);

  if (PyType_Ready(&GetXF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetXF1D_Type);
  PyModule_AddObject(module, "GetXF1D", (PyObject *)&GetXF1D_Type);

  if (PyType_Ready(&GetYF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetYF1D_Type);
  PyModule_AddObject(module, "GetYF1D", (PyObject *)&GetYF1D_Type);

  if (PyType_Ready(&GetZF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetZF1D_Type);
  PyModule_AddObject(module, "GetZF1D", (PyObject *)&GetZF1D_Type);

  if (PyType_Ready(&LocalAverageDepthF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&LocalAverageDepthF1D_Type);
  PyModule_AddObject(module, "LocalAverageDepthF1D", (PyObject *)&LocalAverageDepthF1D_Type);

  if (PyType_Ready(&ZDiscontinuityF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ZDiscontinuityF1D_Type);
  PyModule_AddObject(module, "ZDiscontinuityF1D", (PyObject *)&ZDiscontinuityF1D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DDouble___doc__[] =
    "Class hierarchy: :class:`UnaryFunction1D` > :class:`UnaryFunction1DDouble`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface1D` and return a float value.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(integration_type)\n"
    "\n"
    "   Builds a unary 1D function using the default constructor\n"
    "   or the integration method given as an argument.\n"
    "\n"
    "   :arg integration_type: An integration method.\n"
    "   :type integration_type: :class:`IntegrationType`\n";

static int UnaryFunction1DDouble___init__(BPy_UnaryFunction1DDouble *self,
                                          PyObject *args,
                                          PyObject *kwds)
{
  static const char *kwlist[] = {"integration", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj)) {
    return -1;
  }

  if (!obj) {
    self->uf1D_double = new UnaryFunction1D<double>();
  }
  else {
    self->uf1D_double = new UnaryFunction1D<double>(IntegrationType_from_BPy_IntegrationType(obj));
  }

  self->uf1D_double->py_uf1D = (PyObject *)self;

  return 0;
}

static void UnaryFunction1DDouble___dealloc__(BPy_UnaryFunction1DDouble *self)
{
  delete self->uf1D_double;
  UnaryFunction1D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction1DDouble___repr__(BPy_UnaryFunction1DDouble *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf1D_double);
}

static PyObject *UnaryFunction1DDouble___call__(BPy_UnaryFunction1DDouble *self,
                                                PyObject *args,
                                                PyObject *kwds)
{
  static const char *kwlist[] = {"inter", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &obj)) {
    return nullptr;
  }

  if (typeid(*(self->uf1D_double)) == typeid(UnaryFunction1D<double>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf1D_double->operator()(*(((BPy_Interface1D *)obj)->if1D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return PyFloat_FromDouble(self->uf1D_double->result);
}

/*----------------------UnaryFunction1DDouble get/setters ----------------------------*/

PyDoc_STRVAR(integration_type_doc,
             "The integration method.\n"
             "\n"
             ":type: :class:`IntegrationType`");

static PyObject *integration_type_get(BPy_UnaryFunction1DDouble *self, void * /*closure*/)
{
  return BPy_IntegrationType_from_IntegrationType(self->uf1D_double->getIntegrationType());
}

static int integration_type_set(BPy_UnaryFunction1DDouble *self,
                                PyObject *value,
                                void * /*closure*/)
{
  if (!BPy_IntegrationType_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an IntegrationType");
    return -1;
  }
  self->uf1D_double->setIntegrationType(IntegrationType_from_BPy_IntegrationType(value));
  return 0;
}

static PyGetSetDef BPy_UnaryFunction1DDouble_getseters[] = {
    {"integration_type",
     (getter)integration_type_get,
     (setter)integration_type_set,
     integration_type_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction1DDouble type definition ------------------------------*/

PyTypeObject UnaryFunction1DDouble_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryFunction1DDouble",
    /*tp_basicsize*/ sizeof(BPy_UnaryFunction1DDouble),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryFunction1DDouble___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryFunction1DDouble___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryFunction1DDouble___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryFunction1DDouble___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_UnaryFunction1DDouble_getseters,
    /*tp_base*/ &UnaryFunction1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)UnaryFunction1DDouble___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
