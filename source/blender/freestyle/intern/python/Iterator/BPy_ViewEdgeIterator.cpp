/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ViewEdgeIterator.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(ViewEdgeIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`ViewEdgeIterator`\n"
             "\n"
             "Base class for iterators over ViewEdges of the :class:`ViewMap` Graph.\n"
             "Basically the increment() operator of this class should be able to\n"
             "take the decision of \"where\" (on which ViewEdge) to go when pointing\n"
             "on a given ViewEdge.\n"
             "\n"
             ".. method:: __init__(begin=None, orientation=True)\n"
             "            __init__(brother)\n"
             "\n"
             "   Builds a ViewEdgeIterator from a starting ViewEdge and its\n"
             "   orientation or the copy constructor.\n"
             "\n"
             "   :arg begin: The ViewEdge from where to start the iteration.\n"
             "   :type begin: :class:`ViewEdge` or None\n"
             "   :arg orientation: If true, we'll look for the next ViewEdge among\n"
             "      the ViewEdges that surround the ending ViewVertex of begin. If\n"
             "      false, we'll search over the ViewEdges surrounding the ending\n"
             "      ViewVertex of begin.\n"
             "   :type orientation: bool\n"
             "   :arg brother: A ViewEdgeIterator object.\n"
             "   :type brother: :class:`ViewEdgeIterator`");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != nullptr && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ViewEdgeIterator_init(BPy_ViewEdgeIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"begin", "orientation", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ViewEdgeIterator_Type, &obj1))
  {
    self->ve_it = new ViewEdgeInternal::ViewEdgeIterator(*(((BPy_ViewEdgeIterator *)obj1)->ve_it));
  }
  else if ((void)PyErr_Clear(),
           (void)(obj1 = obj2 = nullptr),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "|O&O!", (char **)kwlist_2, check_begin, &obj1, &PyBool_Type, &obj2))
  {
    ViewEdge *begin = (!obj1 || obj1 == Py_None) ? nullptr : ((BPy_ViewEdge *)obj1)->ve;
    bool orientation = (!obj2) ? true : bool_from_PyBool(obj2);
    self->ve_it = new ViewEdgeInternal::ViewEdgeIterator(begin, orientation);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_it.it = self->ve_it;
  return 0;
}

PyDoc_STRVAR(ViewEdgeIterator_change_orientation_doc,
             ".. method:: change_orientation()\n"
             "\n"
             "   Changes the current orientation.");

static PyObject *ViewEdgeIterator_change_orientation(BPy_ViewEdgeIterator *self)
{
  self->ve_it->changeOrientation();
  Py_RETURN_NONE;
}

static PyMethodDef BPy_ViewEdgeIterator_methods[] = {
    {"change_orientation",
     (PyCFunction)ViewEdgeIterator_change_orientation,
     METH_NOARGS,
     ViewEdgeIterator_change_orientation_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------ViewEdgeIterator get/setters ----------------------------*/

PyDoc_STRVAR(ViewEdgeIterator_object_doc,
             "The ViewEdge object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *ViewEdgeIterator_object_get(BPy_ViewEdgeIterator *self, void * /*closure*/)
{
  if (!self->ve_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  ViewEdge *ve = self->ve_it->operator*();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(ViewEdgeIterator_current_edge_doc,
             "The ViewEdge object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *ViewEdgeIterator_current_edge_get(BPy_ViewEdgeIterator *self, void * /*closure*/)
{
  ViewEdge *ve = self->ve_it->getCurrentEdge();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }
  Py_RETURN_NONE;
}

static int ViewEdgeIterator_current_edge_set(BPy_ViewEdgeIterator *self,
                                             PyObject *value,
                                             void * /*closure*/)
{
  if (!BPy_ViewEdge_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a ViewEdge");
    return -1;
  }
  self->ve_it->setCurrentEdge(((BPy_ViewEdge *)value)->ve);
  return 0;
}

PyDoc_STRVAR(ViewEdgeIterator_orientation_doc,
             "The orientation of the pointed ViewEdge in the iteration.\n"
             "If true, the iterator looks for the next ViewEdge among those ViewEdges\n"
             "that surround the ending ViewVertex of the \"begin\" ViewEdge. If false,\n"
             "the iterator searches over the ViewEdges surrounding the ending ViewVertex\n"
             "of the \"begin\" ViewEdge.\n"
             "\n"
             ":type: bool");

static PyObject *ViewEdgeIterator_orientation_get(BPy_ViewEdgeIterator *self, void * /*closure*/)
{
  return PyBool_from_bool(self->ve_it->getOrientation());
}

static int ViewEdgeIterator_orientation_set(BPy_ViewEdgeIterator *self,
                                            PyObject *value,
                                            void * /*closure*/)
{
  if (!PyBool_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a boolean");
    return -1;
  }
  self->ve_it->setOrientation(bool_from_PyBool(value));
  return 0;
}

PyDoc_STRVAR(ViewEdgeIterator_begin_doc,
             "The first ViewEdge used for the iteration.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *ViewEdgeIterator_begin_get(BPy_ViewEdgeIterator *self, void * /*closure*/)
{
  ViewEdge *ve = self->ve_it->getBegin();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }
  Py_RETURN_NONE;
}

static int ViewEdgeIterator_begin_set(BPy_ViewEdgeIterator *self,
                                      PyObject *value,
                                      void * /*closure*/)
{
  if (!BPy_ViewEdge_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a ViewEdge");
    return -1;
  }
  self->ve_it->setBegin(((BPy_ViewEdge *)value)->ve);
  return 0;
}

static PyGetSetDef BPy_ViewEdgeIterator_getseters[] = {
    {"object",
     (getter)ViewEdgeIterator_object_get,
     (setter) nullptr,
     ViewEdgeIterator_object_doc,
     nullptr},
    {"current_edge",
     (getter)ViewEdgeIterator_current_edge_get,
     (setter)ViewEdgeIterator_current_edge_set,
     ViewEdgeIterator_current_edge_doc,
     nullptr},
    {"orientation",
     (getter)ViewEdgeIterator_orientation_get,
     (setter)ViewEdgeIterator_orientation_set,
     ViewEdgeIterator_orientation_doc,
     nullptr},
    {"begin",
     (getter)ViewEdgeIterator_begin_get,
     (setter)ViewEdgeIterator_begin_set,
     ViewEdgeIterator_begin_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_ViewEdgeIterator type definition ------------------------------*/

PyTypeObject ViewEdgeIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ViewEdgeIterator",
    /*tp_basicsize*/ sizeof(BPy_ViewEdgeIterator),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
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
    /*tp_doc*/ ViewEdgeIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_ViewEdgeIterator_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_ViewEdgeIterator_getseters,
    /*tp_base*/ &Iterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ViewEdgeIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
