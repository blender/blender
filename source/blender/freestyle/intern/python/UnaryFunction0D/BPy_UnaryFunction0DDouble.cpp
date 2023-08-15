/* SPDX-FileCopyrightText: 2008-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction0DDouble.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_double/BPy_Curvature2DAngleF0D.h"
#include "UnaryFunction0D_double/BPy_DensityF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedXF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedYF0D.h"
#include "UnaryFunction0D_double/BPy_GetProjectedZF0D.h"
#include "UnaryFunction0D_double/BPy_GetXF0D.h"
#include "UnaryFunction0D_double/BPy_GetYF0D.h"
#include "UnaryFunction0D_double/BPy_GetZF0D.h"
#include "UnaryFunction0D_double/BPy_LocalAverageDepthF0D.h"
#include "UnaryFunction0D_double/BPy_ZDiscontinuityF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DDouble_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DDouble_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DDouble_Type);
  PyModule_AddObject(module, "UnaryFunction0DDouble", (PyObject *)&UnaryFunction0DDouble_Type);

  if (PyType_Ready(&DensityF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&DensityF0D_Type);
  PyModule_AddObject(module, "DensityF0D", (PyObject *)&DensityF0D_Type);

  if (PyType_Ready(&LocalAverageDepthF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&LocalAverageDepthF0D_Type);
  PyModule_AddObject(module, "LocalAverageDepthF0D", (PyObject *)&LocalAverageDepthF0D_Type);

  if (PyType_Ready(&Curvature2DAngleF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Curvature2DAngleF0D_Type);
  PyModule_AddObject(module, "Curvature2DAngleF0D", (PyObject *)&Curvature2DAngleF0D_Type);

  if (PyType_Ready(&GetProjectedXF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedXF0D_Type);
  PyModule_AddObject(module, "GetProjectedXF0D", (PyObject *)&GetProjectedXF0D_Type);

  if (PyType_Ready(&GetProjectedYF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedYF0D_Type);
  PyModule_AddObject(module, "GetProjectedYF0D", (PyObject *)&GetProjectedYF0D_Type);

  if (PyType_Ready(&GetProjectedZF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetProjectedZF0D_Type);
  PyModule_AddObject(module, "GetProjectedZF0D", (PyObject *)&GetProjectedZF0D_Type);

  if (PyType_Ready(&GetXF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetXF0D_Type);
  PyModule_AddObject(module, "GetXF0D", (PyObject *)&GetXF0D_Type);

  if (PyType_Ready(&GetYF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetYF0D_Type);
  PyModule_AddObject(module, "GetYF0D", (PyObject *)&GetYF0D_Type);

  if (PyType_Ready(&GetZF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetZF0D_Type);
  PyModule_AddObject(module, "GetZF0D", (PyObject *)&GetZF0D_Type);

  if (PyType_Ready(&ZDiscontinuityF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ZDiscontinuityF0D_Type);
  PyModule_AddObject(module, "ZDiscontinuityF0D", (PyObject *)&ZDiscontinuityF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DDouble___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DDouble`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a float value.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DDouble___init__(BPy_UnaryFunction0DDouble *self,
                                          PyObject *args,
                                          PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_double = new UnaryFunction0D<double>();
  self->uf0D_double->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DDouble___dealloc__(BPy_UnaryFunction0DDouble *self)
{
  delete self->uf0D_double;
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DDouble___repr__(BPy_UnaryFunction0DDouble *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_double);
}

static PyObject *UnaryFunction0DDouble___call__(BPy_UnaryFunction0DDouble *self,
                                                PyObject *args,
                                                PyObject *kwds)
{
  static const char *kwlist[] = {"it", nullptr};
  PyObject *obj;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &obj))
  {
    return nullptr;
  }

  if (typeid(*(self->uf0D_double)) == typeid(UnaryFunction0D<double>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf0D_double->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return PyFloat_FromDouble(self->uf0D_double->result);
}

/*-----------------------BPy_UnaryFunction0DDouble type definition ------------------------------*/

PyTypeObject UnaryFunction0DDouble_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryFunction0DDouble",
    /*tp_basicsize*/ sizeof(BPy_UnaryFunction0DDouble),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryFunction0DDouble___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryFunction0DDouble___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryFunction0DDouble___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryFunction0DDouble___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ &UnaryFunction0D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)UnaryFunction0DDouble___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
