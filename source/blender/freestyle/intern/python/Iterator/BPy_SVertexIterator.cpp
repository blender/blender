/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    SVertexIterator_doc,
    "Class hierarchy: :class:`Iterator` > :class:`SVertexIterator`\n"
    "\n"
    "Class representing an iterator over :class:`SVertex` of a\n"
    ":class:`ViewEdge`.  An instance of an SVertexIterator can be obtained\n"
    "from a ViewEdge by calling verticesBegin() or verticesEnd().\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(brother)\n"
    "            __init__(vertex, begin, previous_edge, next_edge, t)"
    "\n"
    "   Build an SVertexIterator using either the default constructor, copy constructor,\n"
    "   or the overloaded constructor that starts iteration from an SVertex object vertex.\n"
    "\n"
    "   :arg brother: An SVertexIterator object.\n"
    "   :type brother: :class:`SVertexIterator`\n"
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
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"vertex", "begin", "previous_edge", "next_edge", "t", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr, *obj3 = nullptr, *obj4 = nullptr;
  float t;

  if (PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist_1, &SVertexIterator_Type, &obj1))
  {
    if (!obj1) {
      self->sv_it = new ViewEdgeInternal::SVertexIterator();
    }
    else {
      self->sv_it = new ViewEdgeInternal::SVertexIterator(*(((BPy_SVertexIterator *)obj1)->sv_it));
    }
  }
  else if ((void)PyErr_Clear(),
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
                                       &t))
  {
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

static PyObject *SVertexIterator_object_get(BPy_SVertexIterator *self, void * /*closure*/)
{
  if (self->sv_it->isEnd()) {
    PyErr_SetString(PyExc_RuntimeError, "iteration has stopped");
    return nullptr;
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

static PyObject *SVertexIterator_t_get(BPy_SVertexIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->sv_it->t());
}

PyDoc_STRVAR(SVertexIterator_u_doc,
             "The point parameter at the current point in the 1D element (0 <= u <= 1).\n"
             "\n"
             ":type: float");

static PyObject *SVertexIterator_u_get(BPy_SVertexIterator *self, void * /*closure*/)
{
  return PyFloat_FromDouble(self->sv_it->u());
}

static PyGetSetDef BPy_SVertexIterator_getseters[] = {
    {"object",
     (getter)SVertexIterator_object_get,
     (setter) nullptr,
     SVertexIterator_object_doc,
     nullptr},
    {"t", (getter)SVertexIterator_t_get, (setter) nullptr, SVertexIterator_t_doc, nullptr},
    {"u", (getter)SVertexIterator_u_get, (setter) nullptr, SVertexIterator_u_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_SVertexIterator type definition ------------------------------*/

PyTypeObject SVertexIterator_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "SVertexIterator",
    /*tp_basicsize*/ sizeof(BPy_SVertexIterator),
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
    /*tp_doc*/ SVertexIterator_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_SVertexIterator_getseters,
    /*tp_base*/ &Iterator_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)SVertexIterator_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
