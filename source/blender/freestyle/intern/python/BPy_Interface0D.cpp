/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Interface0D.h"

#include "BPy_Convert.h"
#include "BPy_Nature.h"
#include "Interface0D/BPy_CurvePoint.h"
#include "Interface0D/BPy_SVertex.h"
#include "Interface0D/BPy_ViewVertex.h"
#include "Interface0D/CurvePoint/BPy_StrokeVertex.h"
#include "Interface0D/ViewVertex/BPy_NonTVertex.h"
#include "Interface0D/ViewVertex/BPy_TVertex.h"
#include "Interface1D/BPy_FEdge.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Interface0D_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Interface0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&Interface0D_Type);
  PyModule_AddObject(module, "Interface0D", (PyObject *)&Interface0D_Type);

  if (PyType_Ready(&CurvePoint_Type) < 0) {
    return -1;
  }
  Py_INCREF(&CurvePoint_Type);
  PyModule_AddObject(module, "CurvePoint", (PyObject *)&CurvePoint_Type);

  if (PyType_Ready(&SVertex_Type) < 0) {
    return -1;
  }
  Py_INCREF(&SVertex_Type);
  PyModule_AddObject(module, "SVertex", (PyObject *)&SVertex_Type);

  if (PyType_Ready(&ViewVertex_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ViewVertex_Type);
  PyModule_AddObject(module, "ViewVertex", (PyObject *)&ViewVertex_Type);

  if (PyType_Ready(&StrokeVertex_Type) < 0) {
    return -1;
  }
  Py_INCREF(&StrokeVertex_Type);
  PyModule_AddObject(module, "StrokeVertex", (PyObject *)&StrokeVertex_Type);

  if (PyType_Ready(&NonTVertex_Type) < 0) {
    return -1;
  }
  Py_INCREF(&NonTVertex_Type);
  PyModule_AddObject(module, "NonTVertex", (PyObject *)&NonTVertex_Type);

  if (PyType_Ready(&TVertex_Type) < 0) {
    return -1;
  }
  Py_INCREF(&TVertex_Type);
  PyModule_AddObject(module, "TVertex", (PyObject *)&TVertex_Type);

  SVertex_mathutils_register_callback();
  StrokeVertex_mathutils_register_callback();

  return 0;
}

/*----------------------Interface1D methods ----------------------------*/

PyDoc_STRVAR(Interface0D_doc,
             "Base class for any 0D element.\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Default constructor.");

static int Interface0D_init(BPy_Interface0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->if0D = new Interface0D();
  self->borrowed = false;
  return 0;
}

static void Interface0D_dealloc(BPy_Interface0D *self)
{
  if (self->if0D && !self->borrowed) {
    delete self->if0D;
  }
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Interface0D_repr(BPy_Interface0D *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", self->if0D->getExactTypeName().c_str(), self->if0D);
}

PyDoc_STRVAR(Interface0D_get_fedge_doc,
             ".. method:: get_fedge(inter)\n"
             "\n"
             "   Returns the FEdge that lies between this 0D element and the 0D\n"
             "   element given as the argument.\n"
             "\n"
             "   :arg inter: A 0D element.\n"
             "   :type inter: :class:`Interface0D`\n"
             "   :return: The FEdge lying between the two 0D elements.\n"
             "   :rtype: :class:`FEdge`");

static PyObject *Interface0D_get_fedge(BPy_Interface0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"inter", nullptr};
  PyObject *py_if0D;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface0D_Type, &py_if0D))
  {
    return nullptr;
  }
  FEdge *fe = self->if0D->getFEdge(*(((BPy_Interface0D *)py_if0D)->if0D));
  if (PyErr_Occurred()) {
    return nullptr;
  }
  if (fe) {
    return Any_BPy_FEdge_from_FEdge(*fe);
  }
  Py_RETURN_NONE;
}

static PyMethodDef BPy_Interface0D_methods[] = {
    {"get_fedge",
     (PyCFunction)Interface0D_get_fedge,
     METH_VARARGS | METH_KEYWORDS,
     Interface0D_get_fedge_doc},
    {nullptr, nullptr, 0, nullptr},
};

/*----------------------Interface1D get/setters ----------------------------*/

PyDoc_STRVAR(Interface0D_name_doc,
             "The string of the name of this 0D element.\n"
             "\n"
             ":type: str");

static PyObject *Interface0D_name_get(BPy_Interface0D *self, void * /*closure*/)
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

PyDoc_STRVAR(Interface0D_point_3d_doc,
             "The 3D point of this 0D element.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *Interface0D_point_3d_get(BPy_Interface0D *self, void * /*closure*/)
{
  Vec3f p(self->if0D->getPoint3D());
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return Vector_from_Vec3f(p);
}

PyDoc_STRVAR(Interface0D_projected_x_doc,
             "The X coordinate of the projected 3D point of this 0D element.\n"
             "\n"
             ":type: float");

static PyObject *Interface0D_projected_x_get(BPy_Interface0D *self, void * /*closure*/)
{
  real x = self->if0D->getProjectedX();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return PyFloat_FromDouble(x);
}

PyDoc_STRVAR(Interface0D_projected_y_doc,
             "The Y coordinate of the projected 3D point of this 0D element.\n"
             "\n"
             ":type: float");

static PyObject *Interface0D_projected_y_get(BPy_Interface0D *self, void * /*closure*/)
{
  real y = self->if0D->getProjectedY();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return PyFloat_FromDouble(y);
}

PyDoc_STRVAR(Interface0D_projected_z_doc,
             "The Z coordinate of the projected 3D point of this 0D element.\n"
             "\n"
             ":type: float");

static PyObject *Interface0D_projected_z_get(BPy_Interface0D *self, void * /*closure*/)
{
  real z = self->if0D->getProjectedZ();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return PyFloat_FromDouble(z);
}

PyDoc_STRVAR(Interface0D_point_2d_doc,
             "The 2D point of this 0D element.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *Interface0D_point_2d_get(BPy_Interface0D *self, void * /*closure*/)
{
  Vec2f p(self->if0D->getPoint2D());
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return Vector_from_Vec2f(p);
}

PyDoc_STRVAR(Interface0D_id_doc,
             "The Id of this 0D element.\n"
             "\n"
             ":type: :class:`Id`");

static PyObject *Interface0D_id_get(BPy_Interface0D *self, void * /*closure*/)
{
  Id id(self->if0D->getId());
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return BPy_Id_from_Id(id);  // return a copy
}

PyDoc_STRVAR(Interface0D_nature_doc,
             "The nature of this 0D element.\n"
             "\n"
             ":type: :class:`Nature`");

static PyObject *Interface0D_nature_get(BPy_Interface0D *self, void * /*closure*/)
{
  Nature::VertexNature nature = self->if0D->getNature();
  if (PyErr_Occurred()) {
    return nullptr;
  }
  return BPy_Nature_from_Nature(nature);
}

static PyGetSetDef BPy_Interface0D_getseters[] = {
    {"name", (getter)Interface0D_name_get, (setter) nullptr, Interface0D_name_doc, nullptr},
    {"point_3d",
     (getter)Interface0D_point_3d_get,
     (setter) nullptr,
     Interface0D_point_3d_doc,
     nullptr},
    {"projected_x",
     (getter)Interface0D_projected_x_get,
     (setter) nullptr,
     Interface0D_projected_x_doc,
     nullptr},
    {"projected_y",
     (getter)Interface0D_projected_y_get,
     (setter) nullptr,
     Interface0D_projected_y_doc,
     nullptr},
    {"projected_z",
     (getter)Interface0D_projected_z_get,
     (setter) nullptr,
     Interface0D_projected_z_doc,
     nullptr},
    {"point_2d",
     (getter)Interface0D_point_2d_get,
     (setter) nullptr,
     Interface0D_point_2d_doc,
     nullptr},
    {"id", (getter)Interface0D_id_get, (setter) nullptr, Interface0D_id_doc, nullptr},
    {"nature", (getter)Interface0D_nature_get, (setter) nullptr, Interface0D_nature_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Interface0D type definition ------------------------------*/

PyTypeObject Interface0D_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Interface0D",
    /*tp_basicsize*/ sizeof(BPy_Interface0D),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)Interface0D_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Interface0D_repr,
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
    /*tp_doc*/ Interface0D_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_Interface0D_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Interface0D_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Interface0D_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
