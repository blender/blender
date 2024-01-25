/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_BinaryPredicate0D.h"

#include "BPy_Convert.h"
#include "BPy_Interface0D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int BinaryPredicate0D_Init(PyObject *module)
{
  if (module == nullptr) {
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

PyDoc_STRVAR(
    /* Wrap. */
    BinaryPredicate0D___doc__,
    "Base class for binary predicates working on :class:`Interface0D`\n"
    "objects. A BinaryPredicate0D is typically an ordering relation\n"
    "between two Interface0D objects. The predicate evaluates a relation\n"
    "between the two Interface0D instances and returns a boolean value (true\n"
    "or false). It is used by invoking the __call__() method.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __call__(inter1, inter2)\n"
    "\n"
    "   Must be overload by inherited classes. It evaluates a relation\n"
    "   between two Interface0D objects.\n"
    "\n"
    "   :arg inter1: The first Interface0D object.\n"
    "   :type inter1: :class:`Interface0D`\n"
    "   :arg inter2: The second Interface0D object.\n"
    "   :type inter2: :class:`Interface0D`\n"
    "   :return: True or false.\n"
    "   :rtype: bool\n");

static int BinaryPredicate0D___init__(BPy_BinaryPredicate0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->bp0D = new BinaryPredicate0D();
  self->bp0D->py_bp0D = (PyObject *)self;
  return 0;
}

static void BinaryPredicate0D___dealloc__(BPy_BinaryPredicate0D *self)
{
  delete self->bp0D;

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
  static const char *kwlist[] = {"inter1", "inter2", nullptr};
  BPy_Interface0D *obj1, *obj2;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!O!", (char **)kwlist, &Interface0D_Type, &obj1, &Interface0D_Type, &obj2))
  {
    return nullptr;
  }
  if (typeid(*(self->bp0D)) == typeid(BinaryPredicate0D)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->bp0D->operator()(*(obj1->if0D), *(obj2->if0D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return PyBool_from_bool(self->bp0D->result);
}

/*----------------------BinaryPredicate0D get/setters ----------------------------*/

PyDoc_STRVAR(
    /* Wrap. */
    BinaryPredicate0D_name_doc,
    "The name of the binary 0D predicate.\n"
    "\n"
    ":type: str");

static PyObject *BinaryPredicate0D_name_get(BPy_BinaryPredicate0D *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_BinaryPredicate0D_getseters[] = {
    {"name",
     (getter)BinaryPredicate0D_name_get,
     (setter) nullptr,
     BinaryPredicate0D_name_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_BinaryPredicate0D type definition ------------------------------*/

PyTypeObject BinaryPredicate0D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BinaryPredicate0D",
    /*tp_basicsize*/ sizeof(BPy_BinaryPredicate0D),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BinaryPredicate0D___dealloc__,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)BinaryPredicate0D___repr__,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)BinaryPredicate0D___call__,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ BinaryPredicate0D___doc__,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_BinaryPredicate0D_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)BinaryPredicate0D___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
