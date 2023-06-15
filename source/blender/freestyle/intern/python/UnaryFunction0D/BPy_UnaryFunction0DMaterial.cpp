/* SPDX-FileCopyrightText: 2008-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction0DMaterial.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_Material/BPy_MaterialF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DMaterial_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DMaterial_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DMaterial_Type);
  PyModule_AddObject(module, "UnaryFunction0DMaterial", (PyObject *)&UnaryFunction0DMaterial_Type);

  if (PyType_Ready(&MaterialF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&MaterialF0D_Type);
  PyModule_AddObject(module, "MaterialF0D", (PyObject *)&MaterialF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DMaterial___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DMaterial`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a :class:`Material` object.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DMaterial___init__(BPy_UnaryFunction0DMaterial *self,
                                            PyObject *args,
                                            PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_material = new UnaryFunction0D<FrsMaterial>();
  self->uf0D_material->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DMaterial___dealloc__(BPy_UnaryFunction0DMaterial *self)
{
  delete self->uf0D_material;
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DMaterial___repr__(BPy_UnaryFunction0DMaterial *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_material);
}

static PyObject *UnaryFunction0DMaterial___call__(BPy_UnaryFunction0DMaterial *self,
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

  if (typeid(*(self->uf0D_material)) == typeid(UnaryFunction0D<FrsMaterial>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf0D_material->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return BPy_FrsMaterial_from_FrsMaterial(self->uf0D_material->result);
}

/*-----------------------BPy_UnaryFunction0DMaterial type definition ----------------------------*/

PyTypeObject UnaryFunction0DMaterial_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "UnaryFunction0DMaterial",
    /*tp_basicsize*/ sizeof(BPy_UnaryFunction0DMaterial),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)UnaryFunction0DMaterial___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)UnaryFunction0DMaterial___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)UnaryFunction0DMaterial___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ UnaryFunction0DMaterial___doc__,
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
    /*tp_init*/ (initproc)UnaryFunction0DMaterial___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
