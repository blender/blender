/* SPDX-FileCopyrightText: 2004-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_FEdgeSmooth.h"

#include "../../BPy_Convert.h"
#include "../../Interface0D/BPy_SVertex.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------FEdgeSmooth methods ----------------------------*/

PyDoc_STRVAR(FEdgeSmooth_doc,
             "Class hierarchy: :class:`Interface1D` > :class:`FEdge` > :class:`FEdgeSmooth`\n"
             "\n"
             "Class defining a smooth edge. This kind of edge typically runs across\n"
             "a face of the input mesh. It can be a silhouette, a ridge or valley,\n"
             "a suggestive contour.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(first_vertex, second_vertex)\n"
             "\n"
             "   Builds an :class:`FEdgeSmooth` using the default constructor,\n"
             "   copy constructor, or between two :class:`SVertex`.\n"
             "\n"
             "   :arg brother: An FEdgeSmooth object.\n"
             "   :type brother: :class:`FEdgeSmooth`\n"
             "   :arg first_vertex: The first SVertex object.\n"
             "   :type first_vertex: :class:`SVertex`\n"
             "   :arg second_vertex: The second SVertex object.\n"
             "   :type second_vertex: :class:`SVertex`");

static int FEdgeSmooth_init(BPy_FEdgeSmooth *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"first_vertex", "second_vertex", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FEdgeSmooth_Type, &obj1))
  {
    if (!obj1) {
      self->fes = new FEdgeSmooth();
    }
    else {
      self->fes = new FEdgeSmooth(*(((BPy_FEdgeSmooth *)obj1)->fes));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(
               args, kwds, "O!O!", (char **)kwlist_2, &SVertex_Type, &obj1, &SVertex_Type, &obj2))
  {
    self->fes = new FEdgeSmooth(((BPy_SVertex *)obj1)->sv, ((BPy_SVertex *)obj2)->sv);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  self->py_fe.fe = self->fes;
  self->py_fe.py_if1D.if1D = self->fes;
  self->py_fe.py_if1D.borrowed = false;
  return 0;
}

/*----------------------mathutils callbacks ----------------------------*/

static int FEdgeSmooth_mathutils_check(BaseMathObject *bmo)
{
  if (!BPy_FEdgeSmooth_Check(bmo->cb_user)) {
    return -1;
  }
  return 0;
}

static int FEdgeSmooth_mathutils_get(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_FEdgeSmooth *self = (BPy_FEdgeSmooth *)bmo->cb_user;
  Vec3r p(self->fes->normal());
  bmo->data[0] = p[0];
  bmo->data[1] = p[1];
  bmo->data[2] = p[2];
  return 0;
}

static int FEdgeSmooth_mathutils_set(BaseMathObject *bmo, int /*subtype*/)
{
  BPy_FEdgeSmooth *self = (BPy_FEdgeSmooth *)bmo->cb_user;
  Vec3r p(bmo->data[0], bmo->data[1], bmo->data[2]);
  self->fes->setNormal(p);
  return 0;
}

static int FEdgeSmooth_mathutils_get_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_FEdgeSmooth *self = (BPy_FEdgeSmooth *)bmo->cb_user;
  Vec3r p(self->fes->normal());
  bmo->data[index] = p[index];
  return 0;
}

static int FEdgeSmooth_mathutils_set_index(BaseMathObject *bmo, int /*subtype*/, int index)
{
  BPy_FEdgeSmooth *self = (BPy_FEdgeSmooth *)bmo->cb_user;
  Vec3r p(self->fes->normal());
  p[index] = bmo->data[index];
  self->fes->setNormal(p);
  return 0;
}

static Mathutils_Callback FEdgeSmooth_mathutils_cb = {
    FEdgeSmooth_mathutils_check,
    FEdgeSmooth_mathutils_get,
    FEdgeSmooth_mathutils_set,
    FEdgeSmooth_mathutils_get_index,
    FEdgeSmooth_mathutils_set_index,
};

static uchar FEdgeSmooth_mathutils_cb_index = -1;

void FEdgeSmooth_mathutils_register_callback()
{
  FEdgeSmooth_mathutils_cb_index = Mathutils_RegisterCallback(&FEdgeSmooth_mathutils_cb);
}

/*----------------------FEdgeSmooth get/setters ----------------------------*/

PyDoc_STRVAR(FEdgeSmooth_normal_doc,
             "The normal of the face that this FEdge is running across.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FEdgeSmooth_normal_get(BPy_FEdgeSmooth *self, void * /*closure*/)
{
  return Vector_CreatePyObject_cb((PyObject *)self, 3, FEdgeSmooth_mathutils_cb_index, 0);
}

static int FEdgeSmooth_normal_set(BPy_FEdgeSmooth *self, PyObject *value, void * /*closure*/)
{
  float v[3];
  if (mathutils_array_parse(v, 3, 3, value, "value must be a 3-dimensional vector") == -1) {
    return -1;
  }
  Vec3r p(v[0], v[1], v[2]);
  self->fes->setNormal(p);
  return 0;
}

PyDoc_STRVAR(FEdgeSmooth_material_index_doc,
             "The index of the material of the face that this FEdge is running across.\n"
             "\n"
             ":type: int");

static PyObject *FEdgeSmooth_material_index_get(BPy_FEdgeSmooth *self, void * /*closure*/)
{
  return PyLong_FromLong(self->fes->frs_materialIndex());
}

static int FEdgeSmooth_material_index_set(BPy_FEdgeSmooth *self,
                                          PyObject *value,
                                          void * /*closure*/)
{
  uint i = PyLong_AsUnsignedLong(value);
  if (PyErr_Occurred()) {
    return -1;
  }
  self->fes->setFrsMaterialIndex(i);
  return 0;
}

PyDoc_STRVAR(FEdgeSmooth_material_doc,
             "The material of the face that this FEdge is running across.\n"
             "\n"
             ":type: :class:`Material`");

static PyObject *FEdgeSmooth_material_get(BPy_FEdgeSmooth *self, void * /*closure*/)
{
  return BPy_FrsMaterial_from_FrsMaterial(self->fes->frs_material());
}

PyDoc_STRVAR(FEdgeSmooth_face_mark_doc,
             "The face mark of the face that this FEdge is running across.\n"
             "\n"
             ":type: bool");

static PyObject *FEdgeSmooth_face_mark_get(BPy_FEdgeSmooth *self, void * /*closure*/)
{
  return PyBool_from_bool(self->fes->faceMark());
}

static int FEdgeSmooth_face_mark_set(BPy_FEdgeSmooth *self, PyObject *value, void * /*closure*/)
{
  if (!PyBool_Check(value)) {
    return -1;
  }
  self->fes->setFaceMark(bool_from_PyBool(value));
  return 0;
}

static PyGetSetDef BPy_FEdgeSmooth_getseters[] = {
    {"normal",
     (getter)FEdgeSmooth_normal_get,
     (setter)FEdgeSmooth_normal_set,
     FEdgeSmooth_normal_doc,
     nullptr},
    {"material_index",
     (getter)FEdgeSmooth_material_index_get,
     (setter)FEdgeSmooth_material_index_set,
     FEdgeSmooth_material_index_doc,
     nullptr},
    {"material",
     (getter)FEdgeSmooth_material_get,
     (setter) nullptr,
     FEdgeSmooth_material_doc,
     nullptr},
    {"face_mark",
     (getter)FEdgeSmooth_face_mark_get,
     (setter)FEdgeSmooth_face_mark_set,
     FEdgeSmooth_face_mark_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_FEdgeSmooth type definition ------------------------------*/

PyTypeObject FEdgeSmooth_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "FEdgeSmooth",
    /*tp_basicsize*/ sizeof(BPy_FEdgeSmooth),
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
    /*tp_doc*/ FEdgeSmooth_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_FEdgeSmooth_getseters,
    /*tp_base*/ &FEdge_Type,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)FEdgeSmooth_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
