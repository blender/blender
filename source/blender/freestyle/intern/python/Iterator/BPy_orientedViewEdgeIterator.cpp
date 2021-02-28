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

#include "BPy_orientedViewEdgeIterator.h"

#include "../BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(orientedViewEdgeIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`orientedViewEdgeIterator`\n"
             "\n"
             "Class representing an iterator over oriented ViewEdges around a\n"
             ":class:`ViewVertex`.  This iterator allows a CCW iteration (in the image\n"
             "plane).  An instance of an orientedViewEdgeIterator can only be\n"
             "obtained from a ViewVertex by calling edges_begin() or edges_end().\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(iBrother)\n"
             "\n"
             "   Creates an :class:`orientedViewEdgeIterator` using either the\n"
             "   default constructor or the copy constructor.\n"
             "\n"
             "   :arg iBrother: An orientedViewEdgeIterator object.\n"
             "   :type iBrother: :class:`orientedViewEdgeIterator`");

static int orientedViewEdgeIterator_init(BPy_orientedViewEdgeIterator *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"brother", nullptr};
  PyObject *brother = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &orientedViewEdgeIterator_Type, &brother)) {
    return -1;
  }
  if (!brother) {
    self->ove_it = new ViewVertexInternal::orientedViewEdgeIterator();
    self->at_start = true;
    self->reversed = false;
  }
  else {
    self->ove_it = new ViewVertexInternal::orientedViewEdgeIterator(
        *(((BPy_orientedViewEdgeIterator *)brother)->ove_it));
    self->at_start = ((BPy_orientedViewEdgeIterator *)brother)->at_start;
    self->reversed = ((BPy_orientedViewEdgeIterator *)brother)->reversed;
  }
  self->py_it.it = self->ove_it;
  return 0;
}

static PyObject *orientedViewEdgeIterator_iter(BPy_orientedViewEdgeIterator *self)
{
  Py_INCREF(self);
  self->at_start = true;
  return (PyObject *)self;
}

static PyObject *orientedViewEdgeIterator_iternext(BPy_orientedViewEdgeIterator *self)
{
  if (self->reversed) {
    if (self->ove_it->isBegin()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    self->ove_it->decrement();
  }
  else {
    if (self->ove_it->isEnd()) {
      PyErr_SetNone(PyExc_StopIteration);
      return nullptr;
    }
    if (self->at_start) {
      self->at_start = false;
    }
    else {
      self->ove_it->increment();
      if (self->ove_it->isEnd()) {
        PyErr_SetNone(PyExc_StopIteration);
        return nullptr;
      }
    }
  }
  ViewVertex::directedViewEdge *dve = self->ove_it->operator->();
  return BPy_directedViewEdge_from_directedViewEdge(*dve);
}

/*----------------------orientedViewEdgeIterator get/setters ----------------------------*/

PyDoc_STRVAR(orientedViewEdgeIterator_object_doc,
             "The oriented ViewEdge (i.e., a tuple of the pointed ViewEdge and a boolean\n"
             "value) currently pointed to by this iterator. If the boolean value is true,\n"
             "the ViewEdge is incoming.\n"
             "\n"
             ":type: (:class:`ViewEdge`, bool)");

static PyObject *orientedViewEdgeIterator_object_get(BPy_orientedViewEdgeIterator *self,
                                                     void *UNUSED(closure))
{
  if (self->ove_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
  }
  return BPy_directedViewEdge_from_directedViewEdge(self->ove_it->operator*());
}

static PyGetSetDef BPy_orientedViewEdgeIterator_getseters[] = {
    {"object",
     (getter)orientedViewEdgeIterator_object_get,
     (setter) nullptr,
     orientedViewEdgeIterator_object_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_orientedViewEdgeIterator type definition ---------------------------*/

PyTypeObject orientedViewEdgeIterator_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "orientedViewEdgeIterator", /* tp_name */
    sizeof(BPy_orientedViewEdgeIterator),                         /* tp_basicsize */
    0,                                                            /* tp_itemsize */
    nullptr,                                                      /* tp_dealloc */
    0,                                                            /* tp_vectorcall_offset */
    nullptr,                                                      /* tp_getattr */
    nullptr,                                                      /* tp_setattr */
    nullptr,                                                      /* tp_reserved */
    nullptr,                                                      /* tp_repr */
    nullptr,                                                      /* tp_as_number */
    nullptr,                                                      /* tp_as_sequence */
    nullptr,                                                      /* tp_as_mapping */
    nullptr,                                                      /* tp_hash  */
    nullptr,                                                      /* tp_call */
    nullptr,                                                      /* tp_str */
    nullptr,                                                      /* tp_getattro */
    nullptr,                                                      /* tp_setattro */
    nullptr,                                                      /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                     /* tp_flags */
    orientedViewEdgeIterator_doc,                                 /* tp_doc */
    nullptr,                                                      /* tp_traverse */
    nullptr,                                                      /* tp_clear */
    nullptr,                                                      /* tp_richcompare */
    0,                                                            /* tp_weaklistoffset */
    (getiterfunc)orientedViewEdgeIterator_iter,                   /* tp_iter */
    (iternextfunc)orientedViewEdgeIterator_iternext,              /* tp_iternext */
    nullptr,                                                      /* tp_methods */
    nullptr,                                                      /* tp_members */
    BPy_orientedViewEdgeIterator_getseters,                       /* tp_getset */
    &Iterator_Type,                                               /* tp_base */
    nullptr,                                                      /* tp_dict */
    nullptr,                                                      /* tp_descr_get */
    nullptr,                                                      /* tp_descr_set */
    0,                                                            /* tp_dictoffset */
    (initproc)orientedViewEdgeIterator_init,                      /* tp_init */
    nullptr,                                                      /* tp_alloc */
    nullptr,                                                      /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
