/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Iterator.h"

#include "BPy_Convert.h"
#include "Iterator/BPy_AdjacencyIterator.h"
#include "Iterator/BPy_ChainPredicateIterator.h"
#include "Iterator/BPy_ChainSilhouetteIterator.h"
#include "Iterator/BPy_ChainingIterator.h"
#include "Iterator/BPy_CurvePointIterator.h"
#include "Iterator/BPy_Interface0DIterator.h"
#include "Iterator/BPy_SVertexIterator.h"
#include "Iterator/BPy_StrokeVertexIterator.h"
#include "Iterator/BPy_ViewEdgeIterator.h"
#include "Iterator/BPy_orientedViewEdgeIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Iterator_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Iterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Iterator_Type);
  PyModule_AddObject(module, "Iterator", (PyObject *)&Iterator_Type);

  if (PyType_Ready(&AdjacencyIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&AdjacencyIterator_Type);
  PyModule_AddObject(module, "AdjacencyIterator", (PyObject *)&AdjacencyIterator_Type);

  if (PyType_Ready(&Interface0DIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Interface0DIterator_Type);
  PyModule_AddObject(module, "Interface0DIterator", (PyObject *)&Interface0DIterator_Type);

  if (PyType_Ready(&CurvePointIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&CurvePointIterator_Type);
  PyModule_AddObject(module, "CurvePointIterator", (PyObject *)&CurvePointIterator_Type);

  if (PyType_Ready(&StrokeVertexIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&StrokeVertexIterator_Type);
  PyModule_AddObject(module, "StrokeVertexIterator", (PyObject *)&StrokeVertexIterator_Type);

  if (PyType_Ready(&SVertexIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SVertexIterator_Type);
  PyModule_AddObject(module, "SVertexIterator", (PyObject *)&SVertexIterator_Type);

  if (PyType_Ready(&orientedViewEdgeIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&orientedViewEdgeIterator_Type);
  PyModule_AddObject(
      module, "orientedViewEdgeIterator", (PyObject *)&orientedViewEdgeIterator_Type);

  if (PyType_Ready(&ViewEdgeIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ViewEdgeIterator_Type);
  PyModule_AddObject(module, "ViewEdgeIterator", (PyObject *)&ViewEdgeIterator_Type);

  if (PyType_Ready(&ChainingIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ChainingIterator_Type);
  PyModule_AddObject(module, "ChainingIterator", (PyObject *)&ChainingIterator_Type);

  if (PyType_Ready(&ChainPredicateIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ChainPredicateIterator_Type);
  PyModule_AddObject(module, "ChainPredicateIterator", (PyObject *)&ChainPredicateIterator_Type);

  if (PyType_Ready(&ChainSilhouetteIterator_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ChainSilhouetteIterator_Type);
  PyModule_AddObject(module, "ChainSilhouetteIterator", (PyObject *)&ChainSilhouetteIterator_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(Iterator_doc,
             "Base class to define iterators.\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Default constructor.");

static int Iterator_init(BPy_Iterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->it = new Iterator();
  return 0;
}

static void Iterator_dealloc(BPy_Iterator *self)
{
  delete self->it;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Iterator_repr(BPy_Iterator *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->it);
}

PyDoc_STRVAR(Iterator_increment_doc,
             ".. method:: increment()\n"
             "\n"
             "   Makes the iterator point the next element.");

static PyObject *Iterator_increment(BPy_Iterator *self)
{
  if (self->it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "cannot increment any more");
    return nullptr;
  }
  self->it->increment();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(Iterator_decrement_doc,
             ".. method:: decrement()\n"
             "\n"
             "   Makes the iterator point the previous element.");

static PyObject *Iterator_decrement(BPy_Iterator *self)
{
  if (self->it->isBegin()) {
    PyErr_SetString(PyExc_RuntimeError, "cannot decrement any more");
    return nullptr;
  }
  self->it->decrement();
  Py_RETURN_NONE;
}

static PyMethodDef BPy_Iterator_methods[] = {
    {"increment", (PyCFunction)Iterator_increment, METH_NOARGS, Iterator_increment_doc},
    {"decrement", (PyCFunction)Iterator_decrement, METH_NOARGS, Iterator_decrement_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------Iterator get/setters ----------------------------*/

PyDoc_STRVAR(Iterator_name_doc,
             "The string of the name of this iterator.\n"
             "\n"
             ":type: str");

static PyObject *Iterator_name_get(BPy_Iterator *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

PyDoc_STRVAR(Iterator_is_begin_doc,
             "True if the iterator points to the first element.\n"
             "\n"
             ":type: bool");

static PyObject *Iterator_is_begin_get(BPy_Iterator *self, void * /*closure*/)
{
  return PyBool_from_bool(self->it->isBegin());
}

PyDoc_STRVAR(Iterator_is_end_doc,
             "True if the iterator points to the last element.\n"
             "\n"
             ":type: bool");

static PyObject *Iterator_is_end_get(BPy_Iterator *self, void * /*closure*/)
{
  return PyBool_from_bool(self->it->isEnd());
}

static PyGetSetDef BPy_Iterator_getseters[] = {
    {"name", (getter)Iterator_name_get, (setter) nullptr, Iterator_name_doc, nullptr},
    {"is_begin", (getter)Iterator_is_begin_get, (setter) nullptr, Iterator_is_begin_doc, nullptr},
    {"is_end", (getter)Iterator_is_end_get, (setter) nullptr, Iterator_is_end_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Iterator type definition ------------------------------*/

PyTypeObject Iterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Iterator",
    /*tp_basicsize*/ sizeof(BPy_Iterator),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)Iterator_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Iterator_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ Iterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_Iterator_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Iterator_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Iterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
