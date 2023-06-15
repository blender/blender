/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ViewVertex.h"

#include "../BPy_Convert.h"
#include "../BPy_Nature.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------ViewVertex methods----------------------------*/

PyDoc_STRVAR(ViewVertex_doc,
             "Class hierarchy: :class:`Interface0D` > :class:`ViewVertex`\n"
             "\n"
             "Class to define a view vertex.  A view vertex is a feature vertex\n"
             "corresponding to a point of the image graph, where the characteristics\n"
             "of an edge (e.g., nature and visibility) might change.  A\n"
             ":class:`ViewVertex` can be of two kinds: A :class:`TVertex` when it\n"
             "corresponds to the intersection between two ViewEdges or a\n"
             ":class:`NonTVertex` when it corresponds to a vertex of the initial\n"
             "input mesh (it is the case for vertices such as corners for example).\n"
             "Thus, this class can be specialized into two classes, the\n"
             ":class:`TVertex` class and the :class:`NonTVertex` class.");

static int ViewVertex_init(BPy_ViewVertex * /*self*/, PyObject * /*args*/, PyObject * /*kwds*/)
{
  PyErr_SetString(PyExc_TypeError, "cannot instantiate abstract class");
  return -1;
}

PyDoc_STRVAR(ViewVertex_edges_begin_doc,
             ".. method:: edges_begin()\n"
             "\n"
             "   Returns an iterator over the ViewEdges that goes to or comes from\n"
             "   this ViewVertex pointing to the first ViewEdge of the list. The\n"
             "   orientedViewEdgeIterator allows to iterate in CCW order over these\n"
             "   ViewEdges and to get the orientation for each ViewEdge\n"
             "   (incoming/outgoing).\n"
             "\n"
             "   :return: An orientedViewEdgeIterator pointing to the first ViewEdge.\n"
             "   :rtype: :class:`orientedViewEdgeIterator`");

static PyObject *ViewVertex_edges_begin(BPy_ViewVertex *self)
{
  ViewVertexInternal::orientedViewEdgeIterator ove_it(self->vv->edgesBegin());
  return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator(ove_it, false);
}

PyDoc_STRVAR(ViewVertex_edges_end_doc,
             ".. method:: edges_end()\n"
             "\n"
             "   Returns an orientedViewEdgeIterator over the ViewEdges around this\n"
             "   ViewVertex, pointing after the last ViewEdge.\n"
             "\n"
             "   :return: An orientedViewEdgeIterator pointing after the last ViewEdge.\n"
             "   :rtype: :class:`orientedViewEdgeIterator`");

static PyObject *ViewVertex_edges_end(BPy_ViewVertex * /*self*/)
{
#if 0
  ViewVertexInternal::orientedViewEdgeIterator ove_it(self->vv->edgesEnd());
  return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator(ove_it, 1);
#else
  PyErr_SetString(PyExc_NotImplementedError, "edges_end method currently disabled");
  return nullptr;
#endif
}

PyDoc_STRVAR(ViewVertex_edges_iterator_doc,
             ".. method:: edges_iterator(edge)\n"
             "\n"
             "   Returns an orientedViewEdgeIterator pointing to the ViewEdge given\n"
             "   as argument.\n"
             "\n"
             "   :arg edge: A ViewEdge object.\n"
             "   :type edge: :class:`ViewEdge`\n"
             "   :return: An orientedViewEdgeIterator pointing to the given ViewEdge.\n"
             "   :rtype: :class:`orientedViewEdgeIterator`");

static PyObject *ViewVertex_edges_iterator(BPy_ViewVertex *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"edge", nullptr};
  PyObject *py_ve;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &ViewEdge_Type, &py_ve)) {
    return nullptr;
  }
  ViewEdge *ve = ((BPy_ViewEdge *)py_ve)->ve;
  ViewVertexInternal::orientedViewEdgeIterator ove_it(self->vv->edgesIterator(ve));
  return BPy_orientedViewEdgeIterator_from_orientedViewEdgeIterator(ove_it, false);
}

static PyMethodDef BPy_ViewVertex_methods[] = {
    {"edges_begin", (PyCFunction)ViewVertex_edges_begin, METH_NOARGS, ViewVertex_edges_begin_doc},
    {"edges_end", (PyCFunction)ViewVertex_edges_end, METH_NOARGS, ViewVertex_edges_end_doc},
    {"edges_iterator",
     (PyCFunction)ViewVertex_edges_iterator,
     METH_VARARGS | METH_KEYWORDS,
     ViewVertex_edges_iterator_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------ViewVertex get/setters ----------------------------*/

PyDoc_STRVAR(ViewVertex_nature_doc,
             "The nature of this ViewVertex.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *ViewVertex_nature_get(BPy_ViewVertex *self, void * /*closure*/)
{
  Nature::VertexNature nature = self->vv->getNature();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return BPy_Nature_from_Nature(nature);  // return a copy
}

static int ViewVertex_nature_set(BPy_ViewVertex *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Nature_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a Nature");
    return -1;
  }
  self->vv->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
  return 0;
}

static PyGetSetDef BPy_ViewVertex_getseters[] = {
    {"nature",
     (getter)ViewVertex_nature_get,
     (setter)ViewVertex_nature_set,
     ViewVertex_nature_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_ViewVertex type definition ------------------------------*/

PyTypeObject ViewVertex_Type = {
    /*tp_name*/ PyVarObject_HEAD_INIT(nullptr, 0) "ViewVertex",
    /*tp_basicsize*/ sizeof(BPy_ViewVertex),
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
    /*tp_doc*/ ViewVertex_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_ViewVertex_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_ViewVertex_getseters,
    /*tp_base*/ &Interface0D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ViewVertex_init,
    /*tp_alloc*/ nullptr,
    nullptr, /*tp_new*/
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
