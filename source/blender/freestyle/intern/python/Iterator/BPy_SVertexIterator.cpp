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

#include "BPy_SVertexIterator.h"

#include "../BPy_Convert.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(SVertexIterator_doc,
             "Class hierarchy: :class:`Iterator` > :class:`SVertexIterator`\n"
             "\n"
             "Class representing an iterator over :class:`SVertex` of a\n"
             ":class:`ViewEdge`.  An instance of an SVertexIterator can be obtained\n"
             "from a ViewEdge by calling verticesBegin() or verticesEnd().\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Default constructor.\n"
             "\n"
             ".. method:: __init__(brother)\n"
             "\n"
             "   Copy constructor.\n"
             "\n"
             "   :arg brother: An SVertexIterator object.\n"
             "   :type brother: :class:`SVertexIterator`\n"
             "\n"
             ".. method:: __init__(vertex, begin, previous_edge, next_edge, t)\n"
             "\n"
             "   Build an SVertexIterator that starts iteration from an SVertex\n"
             "   object v.\n"
             "\n"
             "   :arg vertex: The SVertex from which the iterator starts iteration.\n"
             "   :type vertex: :class:`SVertex`\n"
             "   :arg begin: The first SVertex of a ViewEdge.\n"
             "   :type begin: :class:`SVertex`\n"
             "   :arg previous_edge: The previous FEdge coming to vertex.\n"
             "   :type previous_edge: :class:`FEdge`\n"
             "   :arg next_edge: The next FEdge going out from vertex.\n"
             "   :type next_edge: :class:`FEdge`\n"
             "   :arg t: The curvilinear abscissa at vertex.\n"
             "   :type t: float");

static int SVertexIterator_init(BPy_SVertexIterator *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", NULL};
  static const char *kwlist_2[] = {"vertex", "begin", "previous_edge", "next_edge", "t", NULL};
  PyObject *obj1 = 0, *obj2 = 0, *obj3 = 0, *obj4 = 0;
  float t;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &SVertexIterator_Type, &obj1)) {
    if (!obj1) {
      self->sv_it = new ViewEdgeInternal::SVertexIterator();
    }
    else {
      self->sv_it = new ViewEdgeInternal::SVertexIterator(*(((BPy_SVertexIterator *)obj1)->sv_it));
    }
  }
  else if (PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!O!O!f",
                                       (char **)kwlist_2,
                                       &SVertex_Type,
                                       &obj1,
                                       &SVertex_Type,
                                       &obj2,
                                       &FEdge_Type,
                                       &obj3,
                                       &FEdge_Type,
                                       &obj4,
                                       &t)) {
    self->sv_it = new ViewEdgeInternal::SVertexIterator(((BPy_SVertex *)obj1)->sv,
                                                        ((BPy_SVertex *)obj2)->sv,
                                                        ((BPy_FEdge *)obj3)->fe,
                                                        ((BPy_FEdge *)obj4)->fe,
                                                        t);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_it.it = self->sv_it;
  return 0;
}

/*----------------------SVertexIterator get/setters ----------------------------*/

PyDoc_STRVAR(SVertexIterator_object_doc,
             "The SVertex object currently pointed by this iterator.\n"
             "\n"
             ":type: :class:`SVertex`");

static PyObject *SVertexIterator_object_get(BPy_SVertexIterator *self, void *UNUSED(closure))
{
  if (self->sv_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return NULL;
  }
  SVertex *sv = self->sv_it->operator->();
  if (sv) {
    return BPy_SVertex_from_SVertex(*sv);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(SVertexIterator_t_doc,
             "The curvilinear abscissa of the current point.\n"
             "\n"
             ":type: float");

static PyObject *SVertexIterator_t_get(BPy_SVertexIterator *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->sv_it->t());
}

PyDoc_STRVAR(SVertexIterator_u_doc,
             "The point parameter at the current point in the 1D element (0 <= u <= 1).\n"
             "\n"
             ":type: float");

static PyObject *SVertexIterator_u_get(BPy_SVertexIterator *self, void *UNUSED(closure))
{
  return PyFloat_FromDouble(self->sv_it->u());
}

static PyGetSetDef BPy_SVertexIterator_getseters[] = {
    {(char *)"object",
     (getter)SVertexIterator_object_get,
     (setter)NULL,
     (char *)SVertexIterator_object_doc,
     NULL},
    {(char *)"t",
     (getter)SVertexIterator_t_get,
     (setter)NULL,
     (char *)SVertexIterator_t_doc,
     NULL},
    {(char *)"u",
     (getter)SVertexIterator_u_get,
     (setter)NULL,
     (char *)SVertexIterator_u_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_SVertexIterator type definition ------------------------------*/

PyTypeObject SVertexIterator_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "SVertexIterator", /* tp_name */
    sizeof(BPy_SVertexIterator),                      /* tp_basicsize */
    0,                                                /* tp_itemsize */
    0,                                                /* tp_dealloc */
    0,                                                /* tp_print */
    0,                                                /* tp_getattr */
    0,                                                /* tp_setattr */
    0,                                                /* tp_reserved */
    0,                                                /* tp_repr */
    0,                                                /* tp_as_number */
    0,                                                /* tp_as_sequence */
    0,                                                /* tp_as_mapping */
    0,                                                /* tp_hash  */
    0,                                                /* tp_call */
    0,                                                /* tp_str */
    0,                                                /* tp_getattro */
    0,                                                /* tp_setattro */
    0,                                                /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* tp_flags */
    SVertexIterator_doc,                              /* tp_doc */
    0,                                                /* tp_traverse */
    0,                                                /* tp_clear */
    0,                                                /* tp_richcompare */
    0,                                                /* tp_weaklistoffset */
    0,                                                /* tp_iter */
    0,                                                /* tp_iternext */
    0,                                                /* tp_methods */
    0,                                                /* tp_members */
    BPy_SVertexIterator_getseters,                    /* tp_getset */
    &Iterator_Type,                                   /* tp_base */
    0,                                                /* tp_dict */
    0,                                                /* tp_descr_get */
    0,                                                /* tp_descr_set */
    0,                                                /* tp_dictoffset */
    (initproc)SVertexIterator_init,                   /* tp_init */
    0,                                                /* tp_alloc */
    0,                                                /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
