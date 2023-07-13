/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_ViewEdge.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../BPy_Nature.h"
#include "../BPy_ViewShape.h"
#include "../Interface0D/BPy_ViewVertex.h"
#include "../Interface1D/BPy_FEdge.h"
#include "../Interface1D/BPy_ViewEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------ViewEdge methods ----------------------------*/

PyDoc_STRVAR(
    ViewEdge_doc,
    "Class hierarchy: :class:`Interface1D` > :class:`ViewEdge`\n"
    "\n"
    "Class defining a ViewEdge. A ViewEdge in an edge of the image graph.\n"
    "it connects two :class:`ViewVertex` objects. It is made by connecting\n"
    "a set of FEdges.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(brother)\n"
    "\n"
    "   Builds a :class:`ViewEdge` using the default constructor or the copy constructor.\n"
    "\n"
    "   :arg brother: A ViewEdge object.\n"
    "   :type brother: :class:`ViewEdge`");

static int ViewEdge_init(BPy_ViewEdge *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"brother", nullptr};
  PyObject *brother = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist, &ViewEdge_Type, &brother)) {
    return -1;
  }
  if (!brother) {
    self->ve = new ViewEdge();
  }
  else {
    self->ve = new ViewEdge(*(((BPy_ViewEdge *)brother)->ve));
  }
  self->py_if1D.if1D = self->ve;
  self->py_if1D.borrowed = false;
  return 0;
}

PyDoc_STRVAR(ViewEdge_update_fedges_doc,
             ".. method:: update_fedges()\n"
             "\n"
             "   Sets Viewedge to this for all embedded fedges.\n");

static PyObject *ViewEdge_update_fedges(BPy_ViewEdge *self)
{
  self->ve->UpdateFEdges();
  Py_RETURN_NONE;
}

static PyMethodDef BPy_ViewEdge_methods[] = {
    {"update_fedges",
     (PyCFunction)ViewEdge_update_fedges,
     METH_NOARGS,
     ViewEdge_update_fedges_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------ViewEdge get/setters ----------------------------*/

PyDoc_STRVAR(ViewEdge_first_viewvertex_doc,
             "The first ViewVertex.\n"
             "\n"
             ":type: :class:`ViewVertex`");

static PyObject *ViewEdge_first_viewvertex_get(BPy_ViewEdge *self, void * /*closure*/)
{
  ViewVertex *v = self->ve->A();
  if (v) {
    return Any_BPy_ViewVertex_from_ViewVertex(*v);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_first_viewvertex_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_ViewVertex_Check(value)) {
    return -1;
  }
  self->ve->setA(((BPy_ViewVertex *)value)->vv);
  return 0;
}

PyDoc_STRVAR(ViewEdge_last_viewvertex_doc,
             "The second ViewVertex.\n"
             "\n"
             ":type: :class:`ViewVertex`");

static PyObject *ViewEdge_last_viewvertex_get(BPy_ViewEdge *self, void * /*closure*/)
{
  ViewVertex *v = self->ve->B();
  if (v) {
    return Any_BPy_ViewVertex_from_ViewVertex(*v);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_last_viewvertex_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_ViewVertex_Check(value)) {
    return -1;
  }
  self->ve->setB(((BPy_ViewVertex *)value)->vv);
  return 0;
}

PyDoc_STRVAR(ViewEdge_first_fedge_doc,
             "The first FEdge that constitutes this ViewEdge.\n"
             "\n"
             ":type: :class:`FEdge`");

static PyObject *ViewEdge_first_fedge_get(BPy_ViewEdge *self, void * /*closure*/)
{
  FEdge *fe = self->ve->fedgeA();
  if (fe) {
    return Any_BPy_FEdge_from_FEdge(*fe);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_first_fedge_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_FEdge_Check(value)) {
    return -1;
  }
  self->ve->setFEdgeA(((BPy_FEdge *)value)->fe);
  return 0;
}

PyDoc_STRVAR(ViewEdge_last_fedge_doc,
             "The last FEdge that constitutes this ViewEdge.\n"
             "\n"
             ":type: :class:`FEdge`");

static PyObject *ViewEdge_last_fedge_get(BPy_ViewEdge *self, void * /*closure*/)
{
  FEdge *fe = self->ve->fedgeB();
  if (fe) {
    return Any_BPy_FEdge_from_FEdge(*fe);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_last_fedge_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_FEdge_Check(value)) {
    return -1;
  }
  self->ve->setFEdgeB(((BPy_FEdge *)value)->fe);
  return 0;
}

PyDoc_STRVAR(ViewEdge_viewshape_doc,
             "The ViewShape to which this ViewEdge belongs to.\n"
             "\n"
             ":type: :class:`ViewShape`");

static PyObject *ViewEdge_viewshape_get(BPy_ViewEdge *self, void * /*closure*/)
{
  ViewShape *vs = self->ve->viewShape();
  if (vs) {
    return BPy_ViewShape_from_ViewShape(*vs);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_viewshape_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_ViewShape_Check(value)) {
    return -1;
  }
  self->ve->setShape(((BPy_ViewShape *)value)->vs);
  return 0;
}

PyDoc_STRVAR(ViewEdge_occludee_doc,
             "The shape that is occluded by the ViewShape to which this ViewEdge\n"
             "belongs to. If no object is occluded, this property is set to None.\n"
             "\n"
             ":type: :class:`ViewShape`");

static PyObject *ViewEdge_occludee_get(BPy_ViewEdge *self, void * /*closure*/)
{
  ViewShape *vs = self->ve->aShape();
  if (vs) {
    return BPy_ViewShape_from_ViewShape(*vs);
  }
  Py_RETURN_NONE;
}

static int ViewEdge_occludee_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_ViewShape_Check(value)) {
    return -1;
  }
  self->ve->setaShape(((BPy_ViewShape *)value)->vs);
  return 0;
}

PyDoc_STRVAR(ViewEdge_is_closed_doc,
             "True if this ViewEdge forms a closed loop.\n"
             "\n"
             ":type: bool");

static PyObject *ViewEdge_is_closed_get(BPy_ViewEdge *self, void * /*closure*/)
{
  return PyBool_from_bool(self->ve->isClosed());
}

PyDoc_STRVAR(ViewEdge_id_doc,
             "The Id of this ViewEdge.\n"
             "\n"
             ":type: :class:`Id`");

static PyObject *ViewEdge_id_get(BPy_ViewEdge *self, void * /*closure*/)
{
  Id id(self->ve->getId());
  return BPy_Id_from_Id(id);  // return a copy
}

static int ViewEdge_id_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Id_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an Id");
    return -1;
  }
  self->ve->setId(*(((BPy_Id *)value)->id));
  return 0;
}

PyDoc_STRVAR(ViewEdge_nature_doc,
             "The nature of this ViewEdge.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *ViewEdge_nature_get(BPy_ViewEdge *self, void * /*closure*/)
{
  return BPy_Nature_from_Nature(self->ve->getNature());
}

static int ViewEdge_nature_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  if (!BPy_Nature_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be a Nature");
    return -1;
  }
  self->ve->setNature(PyLong_AsLong((PyObject *)&((BPy_Nature *)value)->i));
  return 0;
}

PyDoc_STRVAR(ViewEdge_qi_doc,
             "The quantitative invisibility.\n"
             "\n"
             ":type: int");

static PyObject *ViewEdge_qi_get(BPy_ViewEdge *self, void * /*closure*/)
{
  return PyLong_FromLong(self->ve->qi());
}

static int ViewEdge_qi_set(BPy_ViewEdge *self, PyObject *value, void * /*closure*/)
{
  int qi;

  if ((qi = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    return -1;
  }
  self->ve->setQI(qi);
  return 0;
}

PyDoc_STRVAR(ViewEdge_chaining_time_stamp_doc,
             "The time stamp of this ViewEdge.\n"
             "\n"
             ":type: int");

static PyObject *ViewEdge_chaining_time_stamp_get(BPy_ViewEdge *self, void * /*closure*/)
{
  return PyLong_FromLong(self->ve->getChainingTimeStamp());
}

static int ViewEdge_chaining_time_stamp_set(BPy_ViewEdge *self,
                                            PyObject *value,
                                            void * /*closure*/)
{
  int timestamp;

  if ((timestamp = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    return -1;
  }
  self->ve->setChainingTimeStamp(timestamp);
  return 0;
}

static PyGetSetDef BPy_ViewEdge_getseters[] = {
    {"first_viewvertex",
     (getter)ViewEdge_first_viewvertex_get,
     (setter)ViewEdge_first_viewvertex_set,
     ViewEdge_first_viewvertex_doc,
     nullptr},
    {"last_viewvertex",
     (getter)ViewEdge_last_viewvertex_get,
     (setter)ViewEdge_last_viewvertex_set,
     ViewEdge_last_viewvertex_doc,
     nullptr},
    {"first_fedge",
     (getter)ViewEdge_first_fedge_get,
     (setter)ViewEdge_first_fedge_set,
     ViewEdge_first_fedge_doc,
     nullptr},
    {"last_fedge",
     (getter)ViewEdge_last_fedge_get,
     (setter)ViewEdge_last_fedge_set,
     ViewEdge_last_fedge_doc,
     nullptr},
    {"viewshape",
     (getter)ViewEdge_viewshape_get,
     (setter)ViewEdge_viewshape_set,
     ViewEdge_viewshape_doc,
     nullptr},
    {"occludee",
     (getter)ViewEdge_occludee_get,
     (setter)ViewEdge_occludee_set,
     ViewEdge_occludee_doc,
     nullptr},
    {"is_closed",
     (getter)ViewEdge_is_closed_get,
     (setter) nullptr,
     ViewEdge_is_closed_doc,
     nullptr},
    {"id", (getter)ViewEdge_id_get, (setter)ViewEdge_id_set, ViewEdge_id_doc, nullptr},
    {"nature",
     (getter)ViewEdge_nature_get,
     (setter)ViewEdge_nature_set,
     ViewEdge_nature_doc,
     nullptr},
    {"qi", (getter)ViewEdge_qi_get, (setter)ViewEdge_qi_set, ViewEdge_qi_doc, nullptr},
    {"chaining_time_stamp",
     (getter)ViewEdge_chaining_time_stamp_get,
     (setter)ViewEdge_chaining_time_stamp_set,
     ViewEdge_chaining_time_stamp_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_ViewEdge type definition ------------------------------*/

PyTypeObject ViewEdge_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "ViewEdge",
    /*tp_basicsize*/ sizeof(BPy_ViewEdge),
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
    /*tp_doc*/ ViewEdge_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_ViewEdge_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_ViewEdge_getseters,
    /*tp_base*/ &Interface1D_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)ViewEdge_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
