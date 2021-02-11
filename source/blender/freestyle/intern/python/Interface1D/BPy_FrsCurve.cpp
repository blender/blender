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

#include "BPy_FrsCurve.h"

#include "../BPy_Convert.h"
#include "../BPy_Id.h"
#include "../Interface0D/BPy_CurvePoint.h"
#include "../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------CurvePoint methods ----------------------------*/

PyDoc_STRVAR(FrsCurve_doc,
             "Class hierarchy: :class:`Interface1D` > :class:`Curve`\n"
             "\n"
             "Base class for curves made of CurvePoints.  :class:`SVertex` is the\n"
             "type of the initial curve vertices.  A :class:`Chain` is a\n"
             "specialization of a Curve.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(id)\n"
             "\n"
             "   Builds a :class:`FrsCurve` using a default constructor,\n"
             "   copy constructor or from an :class:`Id`.\n"
             "\n"
             "   :arg brother: A Curve object.\n"
             "   :type brother: :class:`Curve`\n"
             "   :arg id: An Id object.\n"
             "   :type id: :class:`Id`");

static int FrsCurve_init(BPy_FrsCurve *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"id", nullptr};
  PyObject *obj = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FrsCurve_Type, &obj)) {
    if (!obj) {
      self->c = new Curve();
    }
    else {
      self->c = new Curve(*(((BPy_FrsCurve *)obj)->c));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist_2, &Id_Type, &obj)) {
    self->c = new Curve(*(((BPy_Id *)obj)->id));
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_if1D.if1D = self->c;
  self->py_if1D.borrowed = false;
  return 0;
}

PyDoc_STRVAR(FrsCurve_push_vertex_back_doc,
             ".. method:: push_vertex_back(vertex)\n"
             "\n"
             "   Adds a single vertex at the end of the Curve.\n"
             "\n"
             "   :arg vertex: A vertex object.\n"
             "   :type vertex: :class:`SVertex` or :class:`CurvePoint`");

static PyObject *FrsCurve_push_vertex_back(BPy_FrsCurve *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"vertex", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &obj)) {
    return nullptr;
  }

  if (BPy_CurvePoint_Check(obj)) {
    self->c->push_vertex_back(((BPy_CurvePoint *)obj)->cp);
  }
  else if (BPy_SVertex_Check(obj)) {
    self->c->push_vertex_back(((BPy_SVertex *)obj)->sv);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument");
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(FrsCurve_push_vertex_front_doc,
             ".. method:: push_vertex_front(vertex)\n"
             "\n"
             "   Adds a single vertex at the front of the Curve.\n"
             "\n"
             "   :arg vertex: A vertex object.\n"
             "   :type vertex: :class:`SVertex` or :class:`CurvePoint`");

static PyObject *FrsCurve_push_vertex_front(BPy_FrsCurve *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"vertex", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &obj)) {
    return nullptr;
  }

  if (BPy_CurvePoint_Check(obj)) {
    self->c->push_vertex_front(((BPy_CurvePoint *)obj)->cp);
  }
  else if (BPy_SVertex_Check(obj)) {
    self->c->push_vertex_front(((BPy_SVertex *)obj)->sv);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument");
    return nullptr;
  }
  Py_RETURN_NONE;
}

static PyMethodDef BPy_FrsCurve_methods[] = {
    {"push_vertex_back",
     (PyCFunction)FrsCurve_push_vertex_back,
     METH_VARARGS | METH_KEYWORDS,
     FrsCurve_push_vertex_back_doc},
    {"push_vertex_front",
     (PyCFunction)FrsCurve_push_vertex_front,
     METH_VARARGS | METH_KEYWORDS,
     FrsCurve_push_vertex_front_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------CurvePoint get/setters ----------------------------*/

PyDoc_STRVAR(FrsCurve_is_empty_doc,
             "True if the Curve doesn't have any Vertex yet.\n"
             "\n"
             ":type: bool");

static PyObject *FrsCurve_is_empty_get(BPy_FrsCurve *self, void *UNUSED(closure))
{
  return PyBool_from_bool(self->c->empty());
}

PyDoc_STRVAR(FrsCurve_segments_size_doc,
             "The number of segments in the polyline constituting the Curve.\n"
             "\n"
             ":type: int");

static PyObject *FrsCurve_segments_size_get(BPy_FrsCurve *self, void *UNUSED(closure))
{
  return PyLong_FromLong(self->c->nSegments());
}

static PyGetSetDef BPy_FrsCurve_getseters[] = {
    {"is_empty", (getter)FrsCurve_is_empty_get, (setter) nullptr, FrsCurve_is_empty_doc, nullptr},
    {"segments_size",
     (getter)FrsCurve_segments_size_get,
     (setter) nullptr,
     FrsCurve_segments_size_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_FrsCurve type definition ------------------------------*/

PyTypeObject FrsCurve_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "Curve", /* tp_name */
    sizeof(BPy_FrsCurve),                      /* tp_basicsize */
    0,                                         /* tp_itemsize */
    nullptr,                                   /* tp_dealloc */
    0,                                         /* tp_vectorcall_offset */
    nullptr,                                   /* tp_getattr */
    nullptr,                                   /* tp_setattr */
    nullptr,                                   /* tp_reserved */
    nullptr,                                   /* tp_repr */
    nullptr,                                   /* tp_as_number */
    nullptr,                                   /* tp_as_sequence */
    nullptr,                                   /* tp_as_mapping */
    nullptr,                                   /* tp_hash  */
    nullptr,                                   /* tp_call */
    nullptr,                                   /* tp_str */
    nullptr,                                   /* tp_getattro */
    nullptr,                                   /* tp_setattro */
    nullptr,                                   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags */
    FrsCurve_doc,                              /* tp_doc */
    nullptr,                                   /* tp_traverse */
    nullptr,                                   /* tp_clear */
    nullptr,                                   /* tp_richcompare */
    0,                                         /* tp_weaklistoffset */
    nullptr,                                   /* tp_iter */
    nullptr,                                   /* tp_iternext */
    BPy_FrsCurve_methods,                      /* tp_methods */
    nullptr,                                   /* tp_members */
    BPy_FrsCurve_getseters,                    /* tp_getset */
    &Interface1D_Type,                         /* tp_base */
    nullptr,                                   /* tp_dict */
    nullptr,                                   /* tp_descr_get */
    nullptr,                                   /* tp_descr_set */
    0,                                         /* tp_dictoffset */
    (initproc)FrsCurve_init,                   /* tp_init */
    nullptr,                                   /* tp_alloc */
    nullptr,                                   /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
