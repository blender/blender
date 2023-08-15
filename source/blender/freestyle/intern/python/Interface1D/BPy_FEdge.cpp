/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_FEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../BPy_Nature.h"
#include "../Interface0D/BPy_SVertex.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------FEdge methods ----------------------------*/

PyDoc_STRVAR(FEdge_doc,
             "Class hierarchy: :class:`Interface1D` > :class:`FEdge`\n"
             "\n"
             "Base Class for feature edges. This FEdge can represent a silhouette,\n"
             "a crease, a ridge/valley, a border or a suggestive contour. For\n"
             "silhouettes, the FEdge is oriented so that the visible face lies on\n"
             "the left of the edge. For borders, the FEdge is oriented so that the\n"
             "face lies on the left of the edge. An FEdge can represent an initial\n"
             "edge of the mesh or runs across a face of the initial mesh depending\n"
             "on the smoothness or sharpness of the mesh. This class is specialized\n"
             "into a smooth and a sharp version since their properties slightly vary\n"
             "from one to the other.\n"
             "\n"
             ".. method:: FEdge()\n"
             "            FEdge(brother)\n"
             "\n"
             "   Builds an :class:`FEdge` using the default constructor,\n"
             "   copy constructor, or between two :class:`SVertex` objects.\n"
             "\n"
             "   :arg brother: An FEdge object.\n"
             "   :type brother: :class:`FEdge`\n"
             "   :arg first_vertex: The first SVertex.\n"
             "   :type first_vertex: :class:`SVertex`\n"
             "   :arg second_vertex: The second SVertex.\n"
             "   :type second_vertex: :class:`SVertex`");

static int FEdge_init(BPy_FEdge *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"first_vertex", "second_vertex", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FEdge_Type, &obj1)) {
    if (!obj1) {
      self->fe = new FEdge();
    }
    else {
      self->fe = new FEdge(*(((BPy_FEdge *)obj1)->fe));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "O!O!", (char **)kwlist_2, &SVertex_Type, &obj1, &SVertex_Type, &obj2))
  {
    self->fe = new FEdge(((BPy_SVertex *)obj1)->sv, ((BPy_SVertex *)obj2)->sv);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_if1D.if1D = self->fe;
  self->py_if1D.borrowed = false;
  return 0;
}

/*----------------------FEdge sequence protocol ----------------------------*/

static Py_ssize_t FEdge_sq_length(BPy_FEdge * /*self*/)
{
  return 2;
}

static PyObject *FEdge_sq_item(BPy_FEdge *self, Py_ssize_t keynum)
{
  if (keynum < 0) {
    keynum += FEdge_sq_length(self);
  }
  if (ELEM(keynum, 0, 1)) {
    SVertex *v = self->fe->operator[](keynum);
    if (v) {
      return BPy_SVertex_from_SVertex(*v);
    }
    Py_RETURN_NONE;
  }
  PyErr_Format(PyExc_IndexError, "FEdge[index]: index %d out of range", keynum);
  return nullptr;
}

static PySequenceMethods BPy_FEdge_as_sequence = {
    /*sq_length*/ (lenfunc)FEdge_sq_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /*sq_item*/ (ssizeargfunc)FEdge_sq_item,
    /*was_sq_slice*/ nullptr, /* DEPRECATED. */
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr, /* DEPRECATED. */
    /*sq_contains*/ nullptr,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

/*----------------------FEdge get/setters ----------------------------*/

PyDoc_STRVAR(FEdge_first_svertex_doc,
             "The first SVertex constituting this FEdge.\n"
             "\n"
             ":type: :class:`SVertex`");

static PyObject *FEdge_first_svertex_get(BPy_FEdge *self, void * /*closure*/)
{
  SVertex *A = self->fe->vertexA();
  if (A) {
    return BPy_SVertex_from_SVertex(*A);
  }
  Py_RETURN_NONE;
}

static int FEdge_first_svertex_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_SVertex_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
    return -1;
  }
  self->fe->setVertexA(((BPy_SVertex *)value)->sv);
  return 0;
}

PyDoc_STRVAR(FEdge_second_svertex_doc,
             "The second SVertex constituting this FEdge.\n"
             "\n"
             ":type: :class:`SVertex`");

static PyObject *FEdge_second_svertex_get(BPy_FEdge *self, void * /*closure*/)
{
  SVertex *B = self->fe->vertexB();
  if (B) {
    return BPy_SVertex_from_SVertex(*B);
  }
  Py_RETURN_NONE;
}

static int FEdge_second_svertex_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_SVertex_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an SVertex");
    return -1;
  }
  self->fe->setVertexB(((BPy_SVertex *)value)->sv);
  return 0;
}

PyDoc_STRVAR(FEdge_next_fedge_doc,
             "The FEdge following this one in the ViewEdge. The value is None if\n"
             "this FEdge is the last of the ViewEdge.\n"
             "\n"
             ":type: :class:`FEdge`");

static PyObject *FEdge_next_fedge_get(BPy_FEdge *self, void * /*closure*/)
{
  FEdge *fe = self->fe->nextEdge();
  if (fe) {
    return Any_BPy_FEdge_from_FEdge(*fe);
  }
  Py_RETURN_NONE;
}

static int FEdge_next_fedge_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_FEdge_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an FEdge");
    return -1;
  }
  self->fe->setNextEdge(((BPy_FEdge *)value)->fe);
  return 0;
}

PyDoc_STRVAR(FEdge_previous_fedge_doc,
             "The FEdge preceding this one in the ViewEdge. The value is None if\n"
             "this FEdge is the first one of the ViewEdge.\n"
             "\n"
             ":type: :class:`FEdge`");

static PyObject *FEdge_previous_fedge_get(BPy_FEdge *self, void * /*closure*/)
{
  FEdge *fe = self->fe->previousEdge();
  if (fe) {
    return Any_BPy_FEdge_from_FEdge(*fe);
  }
  Py_RETURN_NONE;
}

static int FEdge_previous_fedge_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_FEdge_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an FEdge");
    return -1;
  }
  self->fe->setPreviousEdge(((BPy_FEdge *)value)->fe);
  return 0;
}

PyDoc_STRVAR(FEdge_viewedge_doc,
             "The ViewEdge to which this FEdge belongs to.\n"
             "\n"
             ":type: :class:`ViewEdge`");

static PyObject *FEdge_viewedge_get(BPy_FEdge *self, void * /*closure*/)
{
  ViewEdge *ve = self->fe->viewedge();
  if (ve) {
    return BPy_ViewEdge_from_ViewEdge(*ve);
  }
  Py_RETURN_NONE;
}

static int FEdge_viewedge_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_ViewEdge_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an ViewEdge");
    return -1;
  }
  self->fe->setViewEdge(((BPy_ViewEdge *)value)->ve);
  return 0;
}

PyDoc_STRVAR(FEdge_is_smooth_doc,
             "True if this FEdge is a smooth FEdge.\n"
             "\n"
             ":type: bool");

static PyObject *FEdge_is_smooth_get(BPy_FEdge *self, void * /*closure*/)
{
  return PyBool_from_bool(self->fe->isSmooth());
}

static int FEdge_is_smooth_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!PyBool_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be boolean");
    return -1;
  }
  self->fe->setSmooth(bool_from_PyBool(value));
  return 0;
}

PyDoc_STRVAR(FEdge_id_doc,
             "The Id of this FEdge.\n"
             "\n"
             ":type: :class:`Id`");

static PyObject *FEdge_id_get(BPy_FEdge *self, void * /*closure*/)
{
  Id id(self->fe->getId());
  return BPy_Id_from_Id(id);  // return a copy
}

static int FEdge_id_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Id_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an Id");
    return -1;
  }
  self->fe->setId(*(((BPy_Id *)value)->id));
  return 0;
}

PyDoc_STRVAR(FEdge_nature_doc,
             "The nature of this FEdge.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *FEdge_nature_get(BPy_FEdge *self, void * /*closure*/)
{
  return BPy_Nature_from_Nature(self->fe->getNature());
}

static int FEdge_nature_set(BPy_FEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Nature_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a Nature");
    return -1;
  }
  self->fe->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
  return 0;
}

static PyGetSetDef BPy_FEdge_getseters[] = {
    {"first_svertex",
     (getter)FEdge_first_svertex_get,
     (setter)FEdge_first_svertex_set,
     FEdge_first_svertex_doc,
     nullptr},
    {"second_svertex",
     (getter)FEdge_second_svertex_get,
     (setter)FEdge_second_svertex_set,
     FEdge_second_svertex_doc,
     nullptr},
    {"next_fedge",
     (getter)FEdge_next_fedge_get,
     (setter)FEdge_next_fedge_set,
     FEdge_next_fedge_doc,
     nullptr},
    {"previous_fedge",
     (getter)FEdge_previous_fedge_get,
     (setter)FEdge_previous_fedge_set,
     FEdge_previous_fedge_doc,
     nullptr},
    {"viewedge",
     (getter)FEdge_viewedge_get,
     (setter)FEdge_viewedge_set,
     FEdge_viewedge_doc,
     nullptr},
    {"is_smooth",
     (getter)FEdge_is_smooth_get,
     (setter)FEdge_is_smooth_set,
     FEdge_is_smooth_doc,
     nullptr},
    {"id", (getter)FEdge_id_get, (setter)FEdge_id_set, FEdge_id_doc, nullptr},
    {"nature", (getter)FEdge_nature_get, (setter)FEdge_nature_set, FEdge_nature_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_FEdge type definition ------------------------------*/

PyTypeObject FEdge_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "FEdge",
    /*tp_basicsize*/ sizeof(BPy_FEdge),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ nullptr,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ &BPy_FEdge_as_sequence,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ FEdge_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_FEdge_getseters,
    /*tp_base*/ &Interface1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)FEdge_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
