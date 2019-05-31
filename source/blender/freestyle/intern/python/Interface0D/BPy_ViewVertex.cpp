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

#include "BPy_ViewVertex.h"

#include "../BPy_Convert.h"
#include "../Interface1D/BPy_ViewEdge.h"
#include "../BPy_Nature.h"

#ifdef __cplusplus
extern "C" {
#endif

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
  return NULL;
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
  static const char *kwlist[] = {"edge", NULL};
  PyObject *py_ve;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &ViewEdge_Type, &py_ve)) {
    return NULL;
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
    {NULL, NULL, 0, NULL},
};

/*----------------------ViewVertex get/setters ----------------------------*/

PyDoc_STRVAR(ViewVertex_nature_doc,
             "The nature of this ViewVertex.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *ViewVertex_nature_get(BPy_ViewVertex *self, void *UNUSED(closure))
{
  Nature::VertexNature nature = self->vv->getNature();
  if (PyErr_Occurred()) {
    return NULL;
  }
  return BPy_Nature_from_Nature(nature);  // return a copy
}

static int ViewVertex_nature_set(BPy_ViewVertex *self, PyObject *value, void *UNUSED(closure))
{
  if (!BPy_Nature_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a Nature");
    return -1;
  }
  self->vv->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
  return 0;
}

static PyGetSetDef BPy_ViewVertex_getseters[] = {
    {(char *)"nature",
     (getter)ViewVertex_nature_get,
     (setter)ViewVertex_nature_set,
     (char *)ViewVertex_nature_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_ViewVertex type definition ------------------------------*/
PyTypeObject ViewVertex_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "ViewVertex", /* tp_name */
    sizeof(BPy_ViewVertex),                      /* tp_basicsize */
    0,                                           /* tp_itemsize */
    0,                                           /* tp_dealloc */
    0,                                           /* tp_print */
    0,                                           /* tp_getattr */
    0,                                           /* tp_setattr */
    0,                                           /* tp_reserved */
    0,                                           /* tp_repr */
    0,                                           /* tp_as_number */
    0,                                           /* tp_as_sequence */
    0,                                           /* tp_as_mapping */
    0,                                           /* tp_hash  */
    0,                                           /* tp_call */
    0,                                           /* tp_str */
    0,                                           /* tp_getattro */
    0,                                           /* tp_setattro */
    0,                                           /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,    /* tp_flags */
    ViewVertex_doc,                              /* tp_doc */
    0,                                           /* tp_traverse */
    0,                                           /* tp_clear */
    0,                                           /* tp_richcompare */
    0,                                           /* tp_weaklistoffset */
    0,                                           /* tp_iter */
    0,                                           /* tp_iternext */
    BPy_ViewVertex_methods,                      /* tp_methods */
    0,                                           /* tp_members */
    BPy_ViewVertex_getseters,                    /* tp_getset */
    &Interface0D_Type,                           /* tp_base */
    0,                                           /* tp_dict */
    0,                                           /* tp_descr_get */
    0,                                           /* tp_descr_set */
    0,                                           /* tp_dictoffset */
    (initproc)ViewVertex_init,                   /* tp_init */
    0,                                           /* tp_alloc */
    0,                                           /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
