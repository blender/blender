/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Interface1D.h"

#include "BPy_Convert.h"
#include "Interface1D/BPy_FEdge.h"
#include "Interface1D/BPy_FrsCurve.h"
#include "Interface1D/BPy_Stroke.h"
#include "Interface1D/BPy_ViewEdge.h"
#include "Interface1D/Curve/BPy_Chain.h"
#include "Interface1D/FEdge/BPy_FEdgeSharp.h"
#include "Interface1D/FEdge/BPy_FEdgeSmooth.h"

#include "BPy_MediumType.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Interface1D_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Interface1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Interface1D_Type);
  PyModule_AddObject(module, "Interface1D", (PyObject *)&Interface1D_Type);

  if (PyType_Ready(&FrsCurve_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FrsCurve_Type);
  PyModule_AddObject(module, "Curve", (PyObject *)&FrsCurve_Type);

  if (PyType_Ready(&Chain_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Chain_Type);
  PyModule_AddObject(module, "Chain", (PyObject *)&Chain_Type);

  if (PyType_Ready(&FEdge_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FEdge_Type);
  PyModule_AddObject(module, "FEdge", (PyObject *)&FEdge_Type);

  if (PyType_Ready(&FEdgeSharp_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FEdgeSharp_Type);
  PyModule_AddObject(module, "FEdgeSharp", (PyObject *)&FEdgeSharp_Type);

  if (PyType_Ready(&FEdgeSmooth_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FEdgeSmooth_Type);
  PyModule_AddObject(module, "FEdgeSmooth", (PyObject *)&FEdgeSmooth_Type);

  if (PyType_Ready(&Stroke_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Stroke_Type);
  PyModule_AddObject(module, "Stroke", (PyObject *)&Stroke_Type);

  PyDict_SetItemString(Stroke_Type.tp_dict, "DRY_MEDIUM", BPy_MediumType_DRY_MEDIUM);
  PyDict_SetItemString(Stroke_Type.tp_dict, "HUMID_MEDIUM", BPy_MediumType_HUMID_MEDIUM);
  PyDict_SetItemString(Stroke_Type.tp_dict, "OPAQUE_MEDIUM", BPy_MediumType_OPAQUE_MEDIUM);

  if (PyType_Ready(&ViewEdge_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ViewEdge_Type);
  PyModule_AddObject(module, "ViewEdge", (PyObject *)&ViewEdge_Type);

  FEdgeSharp_mathutils_register_callback();
  FEdgeSmooth_mathutils_register_callback();

  return 0;
}

/*----------------------Interface1D methods ----------------------------*/

PyDoc_STRVAR(Interface1D_doc,
             "Base class for any 1D element.\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Default constructor.");

static int Interface1D_init(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->if1D = new Interface1D();
  self->borrowed = false;
  return 0;
}

static void Interface1D_dealloc(BPy_Interface1D *self)
{
  if (self->if1D && !self->borrowed) {
    delete self->if1D;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Interface1D_repr(BPy_Interface1D *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", self->if1D->getExactTypeName().c_str(), self->if1D);
}

PyDoc_STRVAR(Interface1D_vertices_begin_doc,
             ".. method:: vertices_begin()\n"
             "\n"
             "   Returns an iterator over the Interface1D vertices, pointing to the\n"
             "   first vertex.\n"
             "\n"
             "   :return: An Interface0DIterator pointing to the first vertex.\n"
             "   :rtype: :class:`Interface0DIterator`");

static PyObject *Interface1D_vertices_begin(BPy_Interface1D *self)
{
  Interface0DIterator if0D_it(self->if1D->verticesBegin());
  return BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, false);
}

PyDoc_STRVAR(Interface1D_vertices_end_doc,
             ".. method:: vertices_end()\n"
             "\n"
             "   Returns an iterator over the Interface1D vertices, pointing after\n"
             "   the last vertex.\n"
             "\n"
             "   :return: An Interface0DIterator pointing after the last vertex.\n"
             "   :rtype: :class:`Interface0DIterator`");

static PyObject *Interface1D_vertices_end(BPy_Interface1D *self)
{
  Interface0DIterator if0D_it(self->if1D->verticesEnd());
  return BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, true);
}

PyDoc_STRVAR(Interface1D_points_begin_doc,
             ".. method:: points_begin(t=0.0)\n"
             "\n"
             "   Returns an iterator over the Interface1D points, pointing to the\n"
             "   first point. The difference with vertices_begin() is that here we can\n"
             "   iterate over points of the 1D element at a any given sampling.\n"
             "   Indeed, for each iteration, a virtual point is created.\n"
             "\n"
             "   :arg t: A sampling with which we want to iterate over points of\n"
             "      this 1D element.\n"
             "   :type t: float\n"
             "   :return: An Interface0DIterator pointing to the first point.\n"
             "   :rtype: :class:`Interface0DIterator`");

static PyObject *Interface1D_points_begin(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"t", nullptr};
  float f = 0.0f;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", (char **)kwlist, &f)) {
    return nullptr;
  }
  Interface0DIterator if0D_it(self->if1D->pointsBegin(f));
  return BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, false);
}

PyDoc_STRVAR(Interface1D_points_end_doc,
             ".. method:: points_end(t=0.0)\n"
             "\n"
             "   Returns an iterator over the Interface1D points, pointing after the\n"
             "   last point. The difference with vertices_end() is that here we can\n"
             "   iterate over points of the 1D element at a given sampling.  Indeed,\n"
             "   for each iteration, a virtual point is created.\n"
             "\n"
             "   :arg t: A sampling with which we want to iterate over points of\n"
             "      this 1D element.\n"
             "   :type t: float\n"
             "   :return: An Interface0DIterator pointing after the last point.\n"
             "   :rtype: :class:`Interface0DIterator`");

static PyObject *Interface1D_points_end(BPy_Interface1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"t", nullptr};
  float f = 0.0f;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|f", (char **)kwlist, &f)) {
    return nullptr;
  }
  Interface0DIterator if0D_it(self->if1D->pointsEnd(f));
  return BPy_Interface0DIterator_from_Interface0DIterator(if0D_it, true);
}

static PyMethodDef BPy_Interface1D_methods[] = {
    {"vertices_begin",
     (PyCFunction)Interface1D_vertices_begin,
     METH_NOARGS,
     Interface1D_vertices_begin_doc},
    {"vertices_end",
     (PyCFunction)Interface1D_vertices_end,
     METH_NOARGS,
     Interface1D_vertices_end_doc},
    {"points_begin",
     (PyCFunction)Interface1D_points_begin,
     METH_VARARGS | METH_KEYWORDS,
     Interface1D_points_begin_doc},
    {"points_end",
     (PyCFunction)Interface1D_points_end,
     METH_VARARGS | METH_KEYWORDS,
     Interface1D_points_end_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------Interface1D get/setters ----------------------------*/

PyDoc_STRVAR(Interface1D_name_doc,
             "The string of the name of the 1D element.\n"
             "\n"
             ":type: str");

static PyObject *Interface1D_name_get(BPy_Interface1D *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

PyDoc_STRVAR(Interface1D_id_doc,
             "The Id of this Interface1D.\n"
             "\n"
             ":type: :class:`Id`");

static PyObject *Interface1D_id_get(BPy_Interface1D *self, void * /*closure*/)
{
  Id id(self->if1D->getId());
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return BPy_Id_from_Id(id);  // return a copy
}

PyDoc_STRVAR(Interface1D_nature_doc,
             "The nature of this Interface1D.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *Interface1D_nature_get(BPy_Interface1D *self, void * /*closure*/)
{
  Nature::VertexNature nature = self->if1D->getNature();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return BPy_Nature_from_Nature(nature);
}

PyDoc_STRVAR(Interface1D_length_2d_doc,
             "The 2D length of this Interface1D.\n"
             "\n"
             ":type: float");

static PyObject *Interface1D_length_2d_get(BPy_Interface1D *self, void * /*closure*/)
{
  real length = self->if1D->getLength2D();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return PyFloat_FromDouble(double(length));
}

PyDoc_STRVAR(Interface1D_time_stamp_doc,
             "The time stamp of the 1D element, mainly used for selection.\n"
             "\n"
             ":type: int");

static PyObject *Interface1D_time_stamp_get(BPy_Interface1D *self, void * /*closure*/)
{
  return PyLong_FromLong(self->if1D->getTimeStamp());
}

static int Interface1D_time_stamp_set(BPy_Interface1D *self, PyObject *value, void * /*closure*/)
{
  int timestamp;

  if ((timestamp = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "value must be a number");
    return -1;
  }
  self->if1D->setTimeStamp(timestamp);
  return 0;
}

static PyGetSetDef BPy_Interface1D_getseters[] = {
    {"name", (getter)Interface1D_name_get, (setter) nullptr, Interface1D_name_doc, nullptr},
    {"id", (getter)Interface1D_id_get, (setter) nullptr, Interface1D_id_doc, nullptr},
    {"nature", (getter)Interface1D_nature_get, (setter) nullptr, Interface1D_nature_doc, nullptr},
    {"length_2d",
     (getter)Interface1D_length_2d_get,
     (setter) nullptr,
     Interface1D_length_2d_doc,
     nullptr},
    {"time_stamp",
     (getter)Interface1D_time_stamp_get,
     (setter)Interface1D_time_stamp_set,
     Interface1D_time_stamp_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Interface1D type definition ------------------------------*/

PyTypeObject Interface1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Interface1D",
    /*tp_basicsize*/ sizeof(BPy_Interface1D),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)Interface1D_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Interface1D_repr,
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
    /*tp_doc*/ Interface1D_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_Interface1D_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Interface1D_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Interface1D_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
