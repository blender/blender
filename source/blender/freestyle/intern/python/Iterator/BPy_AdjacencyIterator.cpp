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

#include "BPy_AdjacencyIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_ViewVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    AdjacencyIterator_doc,
    "Class hierarchy: :class:`Iterator` > :class:`AdjacencyIterator`\n"
    "\n"
    "Class for representing adjacency iterators used in the chaining\n"
    "process.  An AdjacencyIterator is created in the increment() and\n"
    "decrement() methods of a :class:`ChainingIterator` and passed to the\n"
    "traverse() method of the ChainingIterator.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(brother)\n"
    "            __init__(vertex, restrict_to_selection=True, restrict_to_unvisited=True)\n"
    "\n"
    "   Builds an :class:`AdjacencyIterator` using the default constructor,\n"
    "   copy constructor or the overloaded constructor.\n"
    "\n"
    "   :arg brother: An AdjacencyIterator object.\n"
    "   :type brother: :class:`AdjacencyIterator`\n"
    "   :arg vertex: The vertex which is the next crossing.\n"
    "   :type vertex: :class:`ViewVertex`\n"
    "   :arg restrict_to_selection: Indicates whether to force the chaining\n"
    "      to stay within the set of selected ViewEdges or not.\n"
    "   :type restrict_to_selection: bool\n"
    "   :arg restrict_to_unvisited: Indicates whether a ViewEdge that has\n"
    "      already been chained must be ignored ot not.\n"
    "   :type restrict_to_unvisited: bool");

static int AdjacencyIterator_init(BPy_AdjacencyIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {
      "vertex", "restrict_to_selection", "restrict_to_unvisited", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &AdjacencyIterator_Type, &obj1)) {
    if (!obj1) {
      self->a_it = new AdjacencyIterator();
      self->at_start = true;
    }
    else {
      self->a_it = new AdjacencyIterator(*(((BPy_AdjacencyIterator *)obj1)->a_it));
      self->at_start = ((BPy_AdjacencyIterator *)obj1)->at_start;
    }
  }
  else if ((void)PyErr_Clear(),
           (void)(obj2 = obj3 = nullptr),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!|O!O!",
                                       (char **)kwlist_2,
                                       &ViewVertex_Type,
                                       &obj1,
                                       &PyBool_Type,
                                       &obj2,
                                       &PyBool_Type,
                                       &obj3)) {
    bool restrictToSelection = (!obj2) ? true : bool_from_PyBool(obj2);
    bool restrictToUnvisited = (!obj3) ? true : bool_from_PyBool(obj3);
    self->a_it = new AdjacencyIterator(
        ((BPy_ViewVertex *)obj1)->vv, restrictToSelection, restrictToUnvisited);
    self->at_start = ((BPy_AdjacencyIterator *)obj1)->at_start;
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_it.it = self->a_it;
  return 0;
}

static PyObject *AdjacencyIterator_iter(BPy_AdjacencyIterator *self)
{
  Py_INCREF(self);
  self->at_start = true;
  return (PyObject *)self;
}

static PyObject *AdjacencyIterator_iternext(BPy_AdjacencyIterator *self)
{
  if (self->a_it->isEnd()) {
    PyErr_SetNone(PyExc_StopIteration);
    return nullptr;
  }
  if (self->at_start) {
    self->at_start = false;
  }
  else {
    self->a_it->increment();
    if (self->a_it->isEnd()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
  }
  ViewEdge *ve = self->a_it->operator->();
  return BPy_ViewEdge_from_ViewEdge(*ve);
}

/*----------------------AdjacencyIterator get/setters ----------------------------*/

PyDoc_STRVAR(AdjacencyIterator_object_doc,
             "The ViewEdge object currently pointed to by this iterator.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *AdjacencyIterator_object_get(BPy_AdjacencyIterator *self, void *UNUSED(closure))
{
  if (self->a_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  ViewEdge *ve = self->a_it->operator*();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(AdjacencyIterator_is_incoming_doc,
             "True if the current ViewEdge is coming towards the iteration vertex, and\n"
             "False otherwise.\n"
             "\n"
             ":type: bool");

static PyObject *AdjacencyIterator_is_incoming_get(BPy_AdjacencyIterator *self,
                                                   void *UNUSED(closure))
{
  if (self->a_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  return PyBool_from_bool(self->a_it->isIncoming());
}

static PyGetSetDef BPy_AdjacencyIterator_getseters[] = {
    {"is_incoming",
     (getter)AdjacencyIterator_is_incoming_get,
     (setter)nullptr,
     AdjacencyIterator_is_incoming_doc,
     nullptr},
    {"object",
     (getter)AdjacencyIterator_object_get,
     (setter)nullptr,
     AdjacencyIterator_object_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_AdjacencyIterator type definition ------------------------------*/

PyTypeObject AdjacencyIterator_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "AdjacencyIterator", /* tp_name */
    sizeof(BPy_AdjacencyIterator),                      /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    nullptr,                                                  /* tp_dealloc */
    nullptr,                                                  /* tp_print */
    nullptr,                                                  /* tp_getattr */
    nullptr,                                                  /* tp_setattr */
    nullptr,                                                  /* tp_reserved */
    nullptr,                                                  /* tp_repr */
    nullptr,                                                  /* tp_as_number */
    nullptr,                                                  /* tp_as_sequence */
    nullptr,                                                  /* tp_as_mapping */
    nullptr,                                                  /* tp_hash  */
    nullptr,                                                  /* tp_call */
    nullptr,                                                  /* tp_str */
    nullptr,                                                  /* tp_getattro */
    nullptr,                                                  /* tp_setattro */
    nullptr,                                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,           /* tp_flags */
    AdjacencyIterator_doc,                              /* tp_doc */
    nullptr,                                                  /* tp_traverse */
    nullptr,                                                  /* tp_clear */
    nullptr,                                                  /* tp_richcompare */
    0,                                                  /* tp_weaklistoffset */
    (getiterfunc)AdjacencyIterator_iter,                /* tp_iter */
    (iternextfunc)AdjacencyIterator_iternext,           /* tp_iternext */
    nullptr,                                                  /* tp_methods */
    nullptr,                                                  /* tp_members */
    BPy_AdjacencyIterator_getseters,                    /* tp_getset */
    &Iterator_Type,                                     /* tp_base */
    nullptr,                                                  /* tp_dict */
    nullptr,                                                  /* tp_descr_get */
    nullptr,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    (initproc)AdjacencyIterator_init,                   /* tp_init */
    nullptr,                                                  /* tp_alloc */
    nullptr,                                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
