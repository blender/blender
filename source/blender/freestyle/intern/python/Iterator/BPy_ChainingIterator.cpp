/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    ChainingIterator_doc,
    "Class hierarchy: :class:`Iterator` > :class:`ViewEdgeIterator` > :class:`ChainingIterator`\n"
    "\n"
    "Base class for chaining iterators.  This class is designed to be\n"
    "overloaded in order to describe chaining rules.  It makes the\n"
    "description of chaining rules easier.  The two main methods that need\n"
    "to overloaded are traverse() and init().  traverse() tells which\n"
    ":class:`ViewEdge` to follow, among the adjacent ones.  If you specify\n"
    "restriction rules (such as \"Chain only ViewEdges of the selection\"),\n"
    "they will be included in the adjacency iterator (i.e, the adjacent\n"
    "iterator will only stop on \"valid\" edges).\n"
    "\n"
    ".. method:: __init__(restrict_to_selection=True, restrict_to_unvisited=True, begin=None, "
    "orientation=True)\n"
    "\n"
    "   Builds a Chaining Iterator from the first ViewEdge used for\n"
    "   iteration and its orientation.\n"
    "\n"
    "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
    "      to stay within the set of selected ViewEdges or not.\n"
    "   :type restrict_to_selection: bool\n"
    "   :arg restrict_to_unvisited: Indicates whether a ViewEdge that has\n"
    "      already been chained must be ignored ot not.\n"
    "   :type restrict_to_unvisited: bool\n"
    "   :arg begin: The ViewEdge from which to start the chain.\n"
    "   :type begin: :class:`ViewEdge` or None\n"
    "   :arg orientation: The direction to follow to explore the graph.  If\n"
    "      true, the direction indicated by the first ViewEdge is used.\n"
    "   :type orientation: bool\n"
    "\n"
    ".. method:: __init__(brother)\n"
    "\n"
    "   Copy constructor.\n"
    "\n"
    "   :arg brother: \n"
    "   :type brother: ChainingIterator");

static int check_begin(PyObject *obj, void *v)
{
  if (obj != NULL && obj != Py_None && !BPy_ViewEdge_Check(obj)) {
    return 0;
  }
  *((PyObject **)v) = obj;
  return 1;
}

static int ChainingIterator___init__(BPy_ChainingIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {
      "restrict_to_selection", "restrict_to_unvisited", "begin", "orientation", NULL};
  PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist_1, &ChainingIterator_Type, &obj1)) {
    self->c_it = new ChainingIterator(*(((BPy_ChainingIterator *)obj1)->c_it));
  }
  else if (PyErr_Clear(),
           (obj1 = obj2 = obj3 = obj4 = 0),
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
                                       &obj4)) {
    bool restrict_to_selection = (!obj1) ? true : bool_from_PyBool(obj1);
    bool restrict_to_unvisited = (!obj2) ? true : bool_from_PyBool(obj2);
    ViewEdge *begin = (!obj3 || obj3 == Py_None) ? NULL : ((BPy_ViewEdge *)obj3)->ve;
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
             "   Initializes the iterator context.  This method is called each\n"
             "   time a new chain is started.  It can be used to reset some\n"
             "   history information that you might want to keep.");

static PyObject *ChainingIterator_init(BPy_ChainingIterator *self)
{
  if (typeid(*(self->c_it)) == typeid(ChainingIterator)) {
    PyErr_SetString(PyExc_TypeError, "init() method not properly overridden");
    return NULL;
  }
  self->c_it->init();
  Py_RETURN_NONE;
}

PyDoc_STRVAR(ChainingIterator_traverse_doc,
             ".. method:: traverse(it)\n"
             "\n"
             "   This method iterates over the potential next ViewEdges and returns\n"
             "   the one that will be followed next.  Returns the next ViewEdge to\n"
             "   follow or None when the end of the chain is reached.\n"
             "\n"
             "   :arg it: The iterator over the ViewEdges adjacent to the end vertex\n"
             "      of the current ViewEdge.  The adjacency iterator reflects the\n"
             "      restriction rules by only iterating over the valid ViewEdges.\n"
             "   :type it: :class:`AdjacencyIterator`\n"
             "   :return: Returns the next ViewEdge to follow, or None if chaining ends.\n"
             "   :rtype: :class:`ViewEdge` or None");

static PyObject *ChainingIterator_traverse(BPy_ChainingIterator *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {"it", NULL};
  PyObject *py_a_it;

  if (typeid(*(self->c_it)) == typeid(ChainingIterator)) {
    PyErr_SetString(PyExc_TypeError, "traverse() method not properly overridden");
    return NULL;
  }
  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &AdjacencyIterator_Type, &py_a_it)) {
    return NULL;
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
    {NULL, NULL, 0, NULL},
};

/*----------------------ChainingIterator get/setters ----------------------------*/

PyDoc_STRVAR(ChainingIterator_object_doc,
             "The ViewEdge object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *ChainingIterator_object_get(BPy_ChainingIterator *self, void *UNUSED(closure))
{
  if (self->c_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return NULL;
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

static PyObject *ChainingIterator_next_vertex_get(BPy_ChainingIterator *self,
                                                  void *UNUSED(closure))
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
                                                      void *UNUSED(closure))
{
  return PyBool_from_bool(self->c_it->isIncrementing());
}

static PyGetSetDef BPy_ChainingIterator_getseters[] = {
    {(char *)"object",
     (getter)ChainingIterator_object_get,
     (setter)NULL,
     (char *)ChainingIterator_object_doc,
     NULL},
    {(char *)"next_vertex",
     (getter)ChainingIterator_next_vertex_get,
     (setter)NULL,
     (char *)ChainingIterator_next_vertex_doc,
     NULL},
    {(char *)"is_incrementing",
     (getter)ChainingIterator_is_incrementing_get,
     (setter)NULL,
     (char *)ChainingIterator_is_incrementing_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_ChainingIterator type definition ------------------------------*/

PyTypeObject ChainingIterator_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "ChainingIterator", /* tp_name */
    sizeof(BPy_ChainingIterator),                      /* tp_basicsize */
    0,                                                 /* tp_itemsize */
    0,                                                 /* tp_dealloc */
    0,                                                 /* tp_print */
    0,                                                 /* tp_getattr */
    0,                                                 /* tp_setattr */
    0,                                                 /* tp_reserved */
    0,                                                 /* tp_repr */
    0,                                                 /* tp_as_number */
    0,                                                 /* tp_as_sequence */
    0,                                                 /* tp_as_mapping */
    0,                                                 /* tp_hash  */
    0,                                                 /* tp_call */
    0,                                                 /* tp_str */
    0,                                                 /* tp_getattro */
    0,                                                 /* tp_setattro */
    0,                                                 /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,          /* tp_flags */
    ChainingIterator_doc,                              /* tp_doc */
    0,                                                 /* tp_traverse */
    0,                                                 /* tp_clear */
    0,                                                 /* tp_richcompare */
    0,                                                 /* tp_weaklistoffset */
    0,                                                 /* tp_iter */
    0,                                                 /* tp_iternext */
    BPy_ChainingIterator_methods,                      /* tp_methods */
    0,                                                 /* tp_members */
    BPy_ChainingIterator_getseters,                    /* tp_getset */
    &ViewEdgeIterator_Type,                            /* tp_base */
    0,                                                 /* tp_dict */
    0,                                                 /* tp_descr_get */
    0,                                                 /* tp_descr_set */
    0,                                                 /* tp_dictoffset */
    (initproc)ChainingIterator___init__,               /* tp_init */
    0,                                                 /* tp_alloc */
    0,                                                 /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
