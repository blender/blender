/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction0DFloat.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_float/BPy_GetCurvilinearAbscissaF0D.h"
#include "UnaryFunction0D_float/BPy_GetParameterF0D.h"
#include "UnaryFunction0D_float/BPy_GetViewMapGradientNormF0D.h"
#include "UnaryFunction0D_float/BPy_ReadCompleteViewMapPixelF0D.h"
#include "UnaryFunction0D_float/BPy_ReadMapPixelF0D.h"
#include "UnaryFunction0D_float/BPy_ReadSteerableViewMapPixelF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DFloat_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DFloat_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DFloat_Type);
  PyModule_AddObject(module, "UnaryFunction0DFloat", (PyObject *)&UnaryFunction0DFloat_Type);

  if (PyType_Ready(&GetCurvilinearAbscissaF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetCurvilinearAbscissaF0D_Type);
  PyModule_AddObject(
      module, "GetCurvilinearAbscissaF0D", (PyObject *)&GetCurvilinearAbscissaF0D_Type);

  if (PyType_Ready(&GetParameterF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetParameterF0D_Type);
  PyModule_AddObject(module, "GetParameterF0D", (PyObject *)&GetParameterF0D_Type);

  if (PyType_Ready(&GetViewMapGradientNormF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetViewMapGradientNormF0D_Type);
  PyModule_AddObject(
      module, "GetViewMapGradientNormF0D", (PyObject *)&GetViewMapGradientNormF0D_Type);

  if (PyType_Ready(&ReadCompleteViewMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadCompleteViewMapPixelF0D_Type);
  PyModule_AddObject(
      module, "ReadCompleteViewMapPixelF0D", (PyObject *)&ReadCompleteViewMapPixelF0D_Type);

  if (PyType_Ready(&ReadMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadMapPixelF0D_Type);
  PyModule_AddObject(module, "ReadMapPixelF0D", (PyObject *)&ReadMapPixelF0D_Type);

  if (PyType_Ready(&ReadSteerableViewMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadSteerableViewMapPixelF0D_Type);
  PyModule_AddObject(
      module, "ReadSteerableViewMapPixelF0D", (PyObject *)&ReadSteerableViewMapPixelF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DFloat___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DFloat`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a float value.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DFloat___init__(BPy_UnaryFunction0DFloat *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_float = new UnaryFunction0D<float>();
  self->uf0D_float->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DFloat___dealloc__(BPy_UnaryFunction0DFloat *self)
{
  delete self->uf0D_float;
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DFloat___repr__(BPy_UnaryFunction0DFloat *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_float);
}

static PyObject *UnaryFunction0DFloat___call__(BPy_UnaryFunction0DFloat *self,
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

  if (typeid(*(self->uf0D_float)) == typeid(UnaryFunction0D<float>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf0D_float->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return PyFloat_FromDouble(self->uf0D_float->result);
}

/*-----------------------BPy_UnaryFunction0DFloat type definition ------------------------------*/

PyTypeObject UnaryFunction0DFloat_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryFunction0DFloat",
    /*tp_basicsize*/ sizeof(BPy_UnaryFunction0DFloat),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryFunction0DFloat___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryFunction0DFloat___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryFunction0DFloat___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryFunction0DFloat___doc__,
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
    /*tp_init*/ (initproc)UnaryFunction0DFloat___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
