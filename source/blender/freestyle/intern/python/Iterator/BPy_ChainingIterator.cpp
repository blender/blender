/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ChainingIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "BPy_AdjacencyIterator.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    ChainingIterator_doc,
    "Class hierarchy: :class:`Iterator` > :class:`ViewEdgeIterator` > :class:`ChainingIterator`\n"
    "\n"
    "Base class for chaining iterators. This class is designed to be\n"
    "overloaded in order to describe chaining rules. It makes the\n"
    "description of chaining rules easier. The two main methods that need\n"
    "to overloaded are traverse() and init(). traverse() tells which\n"
    ":class:`ViewEdge` to follow, among the adjacent ones. If you specify\n"
    "restriction rules (such as \"Chain only ViewEdges of the selection\"),\n"
    "they will be included in the adjacency iterator (i.e, the adjacent\n"
    "iterator will only stop on \"valid\" edges).\n"
    "\n"
    ".. method:: __init__(restrict_to_selection=True, restrict_to_unvisited=True,"
    "                     begin=None, orientation=True)\n"
    "            __init__(brother)\n"
    "\n"
    "   Builds a Chaining Iterator from the first ViewEdge used for\n"
    "   iteration and its orientation or by using the copy constructor.\n"
    "\n"
    "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
    "      to stay within the set of selected ViewEdges or not.\n"
    "   :type restrict_to_selection: bool\n"
    "   :arg restrict_to_unvisited: Indicates whether a ViewEdge that has\n"
    "      already been chained must be ignored ot not.\n"
    "   :type restrict_to_unvisited: bool\n"
    "   :arg begin: The ViewEdge from which to start the chain.\n"
    "   :type begin: :class:`ViewEdge` or None\n"
    "   :arg orientation: The direction to follow to explore the graph. If\n"
    "      true, the direction indicated by the first ViewEdge is used.\n"
    "   :type orientation: bool\n"
    "   :arg brother: \n"
    "   :type brother: ChainingIterator");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != nullptr && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {
      "restrict_to_selection", "restrict_to_unvisited", "begin", "orientation", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr, *obj4 = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainingIterator_Type, &obj1))
  {
    self->c_it = new ChainingIterator(*(((BPy_ChainingIterator *)obj1)->c_it));
  }
  else if ((void)PyErr_Clear(),
           (void)(obj1 = obj2 = obj3 = obj4 = nullptr),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "|O!O!O&O!",
                                       (char **)kwlist_2,
                                       &PyBool_Type,
                                       &obj1,
                                       &PyBool_Type,
                                       &obj2,
                                       check_begin,
                                       &obj3,
                                       &PyBool_Type,
                                       &obj4))
  {
    bool restrict_to_selection = (!obj1) ? true : bool_from_PyBool(obj1);
    bool restrict_to_unvisited = (!obj2) ? true : bool_from_PyBool(obj2);
    ViewEdge *begin = (!obj3 || obj3 == Py_None) ? nullptr : ((BPy_ViewEdge *)obj3)->ve;
    bool orientation = (!obj4) ? true : bool_from_PyBool(obj4);
    self->c_it = new ChainingIterator(
        restrict_to_selection, restrict_to_unvisited, begin, orientation);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_ve_it.ve_it = self->c_it;
  self->py_ve_it.py_it.it = self->c_it;

  self->c_it->py_c_it = (PyObject *)self;

  return 0;
}

PyDoc_STRVAR(ChainingIterator_init_doc,
             ".. method:: init()\n"
             "\n"
             "   Initializes the iterator context. This method is called each\n"
             "   time a new chain is started. It can be used to reset some\n"
             "   history information that you might want to keep.");

static PyObject *ChainingIterator_init(BPy_ChainingIterator *self)
{
  if (typeid(*(self->c_it)) == typeid(ChainingIterator)) {
    PyErr_SetString(PyExc_TypeError, "init() method not properly overridden");
    return nullptr;
  }
  self->c_it->init();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(ChainingIterator_traverse_doc,
             ".. method:: traverse(it)\n"
             "\n"
             "   This method iterates over the potential next ViewEdges and returns\n"
             "   the one that will be followed next. Returns the next ViewEdge to\n"
             "   follow or None when the end of the chain is reached.\n"
             "\n"
             "   :arg it: The iterator over the ViewEdges adjacent to the end vertex\n"
             "      of the current ViewEdge. The adjacency iterator reflects the\n"
             "      restriction rules by only iterating over the valid ViewEdges.\n"
             "   :type it: :class:`AdjacencyIterator`\n"
             "   :return: Returns the next ViewEdge to follow, or None if chaining ends.\n"
             "   :rtype: :class:`ViewEdge` or None");

static PyObject *ChainingIterator_traverse(BPy_ChainingIterator *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {"it", nullptr};
  PyObject *py_a_it;

  if (typeid(*(self->c_it)) == typeid(ChainingIterator)) {
    PyErr_SetString(PyExc_TypeError, "traverse() method not properly overridden");
    return nullptr;
  }
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &AdjacencyIterator_Type, &py_a_it))
  {
    return nullptr;
  }
  if (((BPy_AdjacencyIterator *)py_a_it)->a_it) {
    self->c_it->traverse(*(((BPy_AdjacencyIterator *)py_a_it)->a_it));
  }
  Py_RETURN_NONE;
}

static PyMethodDef BPy_ChainingIterator_methods[] = {
    {"init", (PyCFunction)ChainingIterator_init, METH_NOARGS, ChainingIterator_init_doc},
    {"traverse",
     (PyCFunction)ChainingIterator_traverse,
     METH_VARARGS | METH_KEYWORDS,
     ChainingIterator_traverse_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------ChainingIterator get/setters ----------------------------*/

PyDoc_STRVAR(ChainingIterator_object_doc,
             "The ViewEdge object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *ChainingIterator_object_get(BPy_ChainingIterator *self, void * /*closure*/)
{
  if (self->c_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  ViewEdge *ve = self->c_it->operator*();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(ChainingIterator_next_vertex_doc,
             "The ViewVertex that is the next crossing.\n"
             "\n"
             ":type: :class:`ViewVertex`");

static PyObject *ChainingIterator_next_vertex_get(BPy_ChainingIterator *self, void * /*closure*/)
{
  ViewVertex *v = self->c_it->getVertex();
  if (v) {
    return Any_BPy_ViewVertex_from_ViewVertex(*v);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(ChainingIterator_is_incrementing_doc,
             "True if the current iteration is an incrementation.\n"
             "\n"
             ":type: bool");

static PyObject *ChainingIterator_is_incrementing_get(BPy_ChainingIterator *self,
                                                      void * /*closure*/)
{
  return PyBool_from_bool(self->c_it->isIncrementing());
}

static PyGetSetDef BPy_ChainingIterator_getseters[] = {
    {"object",
     (getter)ChainingIterator_object_get,
     (setter) nullptr,
     ChainingIterator_object_doc,
     nullptr},
    {"next_vertex",
     (getter)ChainingIterator_next_vertex_get,
     (setter) nullptr,
     ChainingIterator_next_vertex_doc,
     nullptr},
    {"is_incrementing",
     (getter)ChainingIterator_is_incrementing_get,
     (setter) nullptr,
     ChainingIterator_is_incrementing_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_ChainingIterator type definition ------------------------------*/

PyTypeObject ChainingIterator_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ChainingIterator",
    /*tp_basicsize*/ sizeof(BPy_ChainingIterator),
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
    /*tp_doc*/ ChainingIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_ChainingIterator_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_ChainingIterator_getseters,
    /*tp_base*/ &ViewEdgeIterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ChainingIterator___init__,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
