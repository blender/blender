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

#include "BPy_FEdgeSharp.h"

#include "../../BPy_Convert.h"
#include "../../Interface0D/BPy_SVertex.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

/*----------------------FEdgeSharp methods ----------------------------*/

PyDoc_STRVAR(FEdgeSharp_doc,
             "Class hierarchy: :class:`Interface1D` > :class:`FEdge` > :class:`FEdgeSharp`\n"
             "\n"
             "Class defining a sharp FEdge.  A Sharp FEdge corresponds to an initial\n"
             "edge of the input mesh.  It can be a silhouette, a crease or a border.\n"
             "If it is a crease edge, then it is bordered by two faces of the mesh.\n"
             "Face a lies on its right whereas Face b lies on its left.  If it is a\n"
             "border edge, then it doesn't have any face on its right, and thus Face\n"
             "a is None.\n"
             "\n"
             ".. method:: __init__()\n"
             "            __init__(brother)\n"
             "            __init__(first_vertex, second_vertex)\n"
             "\n"
             "   Builds an :class:`FEdgeSharp` using the default constructor,\n"
             "   copy constructor, or between two :class:`SVertex` objects.\n"
             "\n"
             "   :arg brother: An FEdgeSharp object.\n"
             "   :type brother: :class:`FEdgeSharp`\n"
             "   :arg first_vertex: The first SVertex object.\n"
             "   :type first_vertex: :class:`SVertex`\n"
             "   :arg second_vertex: The second SVertex object.\n"
             "   :type second_vertex: :class:`SVertex`");

static int FEdgeSharp_init(BPy_FEdgeSharp *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"first_vertex", "second_vertex", nullptr};
  PyObject *obj1 = nullptr, *obj2 = nullptr;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "|O!", (char **)kwlist_1, &FEdgeSharp_Type, &obj1)) {
    if (!obj1) {
      self->fes = new FEdgeSharp();
    }
    else {
      self->fes = new FEdgeSharp(*(((BPy_FEdgeSharp *)obj1)->fes));
    }
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args,
                                       kwds,
                                       "O!O!",
                                       (char **)kwlist_2,
                                       &SVertex_Type,
                                       &obj1,
                                       &SVertex_Type,
                                       &obj2)) {
    self->fes = new FEdgeSharp(((BPy_SVertex *)obj1)->sv, ((BPy_SVertex *)obj2)->sv);
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

/* subtype */
#define MATHUTILS_SUBTYPE_NORMAL_A 1
#define MATHUTILS_SUBTYPE_NORMAL_B 2

static int FEdgeSharp_mathutils_check(BaseMathObject *bmo)
{
  if (!BPy_FEdgeSharp_Check(bmo->cb_user)) {
    return -1;
  }
  return 0;
}

static int FEdgeSharp_mathutils_get(BaseMathObject *bmo, int subtype)
{
  BPy_FEdgeSharp *self = (BPy_FEdgeSharp *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_NORMAL_A: {
      Vec3r p(self->fes->normalA());
      bmo->data[0] = p[0];
      bmo->data[1] = p[1];
      bmo->data[2] = p[2];
    } break;
    case MATHUTILS_SUBTYPE_NORMAL_B: {
      Vec3r p(self->fes->normalB());
      bmo->data[0] = p[0];
      bmo->data[1] = p[1];
      bmo->data[2] = p[2];
    } break;
    default:
      return -1;
  }
  return 0;
}

static int FEdgeSharp_mathutils_set(BaseMathObject *bmo, int subtype)
{
  BPy_FEdgeSharp *self = (BPy_FEdgeSharp *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_NORMAL_A: {
      Vec3r p(bmo->data[0], bmo->data[1], bmo->data[2]);
      self->fes->setNormalA(p);
    } break;
    case MATHUTILS_SUBTYPE_NORMAL_B: {
      Vec3r p(bmo->data[0], bmo->data[1], bmo->data[2]);
      self->fes->setNormalB(p);
    } break;
    default:
      return -1;
  }
  return 0;
}

static int FEdgeSharp_mathutils_get_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_FEdgeSharp *self = (BPy_FEdgeSharp *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_NORMAL_A: {
      Vec3r p(self->fes->normalA());
      bmo->data[index] = p[index];
    } break;
    case MATHUTILS_SUBTYPE_NORMAL_B: {
      Vec3r p(self->fes->normalB());
      bmo->data[index] = p[index];
    } break;
    default:
      return -1;
  }
  return 0;
}

static int FEdgeSharp_mathutils_set_index(BaseMathObject *bmo, int subtype, int index)
{
  BPy_FEdgeSharp *self = (BPy_FEdgeSharp *)bmo->cb_user;
  switch (subtype) {
    case MATHUTILS_SUBTYPE_NORMAL_A: {
      Vec3r p(self->fes->normalA());
      p[index] = bmo->data[index];
      self->fes->setNormalA(p);
    } break;
    case MATHUTILS_SUBTYPE_NORMAL_B: {
      Vec3r p(self->fes->normalB());
      p[index] = bmo->data[index];
      self->fes->setNormalB(p);
    } break;
    default:
      return -1;
  }
  return 0;
}

static Mathutils_Callback FEdgeSharp_mathutils_cb = {
    FEdgeSharp_mathutils_check,
    FEdgeSharp_mathutils_get,
    FEdgeSharp_mathutils_set,
    FEdgeSharp_mathutils_get_index,
    FEdgeSharp_mathutils_set_index,
};

static unsigned char FEdgeSharp_mathutils_cb_index = -1;

void FEdgeSharp_mathutils_register_callback()
{
  FEdgeSharp_mathutils_cb_index = Mathutils_RegisterCallback(&FEdgeSharp_mathutils_cb);
}

/*----------------------FEdgeSharp get/setters ----------------------------*/

PyDoc_STRVAR(FEdgeSharp_normal_right_doc,
             "The normal to the face lying on the right of the FEdge.  If this FEdge\n"
             "is a border, it has no Face on its right and therefore no normal.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FEdgeSharp_normal_right_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 3, FEdgeSharp_mathutils_cb_index, MATHUTILS_SUBTYPE_NORMAL_A);
}

static int FEdgeSharp_normal_right_set(BPy_FEdgeSharp *self,
                                       PyObject *value,
                                       void *UNUSED(closure))
{
  float v[3];
  if (mathutils_array_parse(v, 3, 3, value, "value must be a 3-dimensional vector") == -1) {
    return -1;
  }
  Vec3r p(v[0], v[1], v[2]);
  self->fes->setNormalA(p);
  return 0;
}

PyDoc_STRVAR(FEdgeSharp_normal_left_doc,
             "The normal to the face lying on the left of the FEdge.\n"
             "\n"
             ":type: :class:`mathutils.Vector`");

static PyObject *FEdgeSharp_normal_left_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return Vector_CreatePyObject_cb(
      (PyObject *)self, 3, FEdgeSharp_mathutils_cb_index, MATHUTILS_SUBTYPE_NORMAL_B);
}

static int FEdgeSharp_normal_left_set(BPy_FEdgeSharp *self, PyObject *value, void *UNUSED(closure))
{
  float v[3];
  if (mathutils_array_parse(v, 3, 3, value, "value must be a 3-dimensional vector") == -1) {
    return -1;
  }
  Vec3r p(v[0], v[1], v[2]);
  self->fes->setNormalB(p);
  return 0;
}

PyDoc_STRVAR(FEdgeSharp_material_index_right_doc,
             "The index of the material of the face lying on the right of the FEdge.\n"
             "If this FEdge is a border, it has no Face on its right and therefore\n"
             "no material.\n"
             "\n"
             ":type: int");

static PyObject *FEdgeSharp_material_index_right_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return PyLong_FromLong(self->fes->aFrsMaterialIndex());
}

static int FEdgeSharp_material_index_right_set(BPy_FEdgeSharp *self,
                                               PyObject *value,
                                               void *UNUSED(closure))
{
  unsigned int i = PyLong_AsUnsignedLong(value);
  if (PyErr_Occurred()) {
    return -1;
  }
  self->fes->setaFrsMaterialIndex(i);
  return 0;
}

PyDoc_STRVAR(FEdgeSharp_material_index_left_doc,
             "The index of the material of the face lying on the left of the FEdge.\n"
             "\n"
             ":type: int");

static PyObject *FEdgeSharp_material_index_left_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return PyLong_FromLong(self->fes->bFrsMaterialIndex());
}

static int FEdgeSharp_material_index_left_set(BPy_FEdgeSharp *self,
                                              PyObject *value,
                                              void *UNUSED(closure))
{
  unsigned int i = PyLong_AsUnsignedLong(value);
  if (PyErr_Occurred()) {
    return -1;
  }
  self->fes->setbFrsMaterialIndex(i);
  return 0;
}

PyDoc_STRVAR(FEdgeSharp_material_right_doc,
             "The material of the face lying on the right of the FEdge.  If this FEdge\n"
             "is a border, it has no Face on its right and therefore no material.\n"
             "\n"
             ":type: :class:`Material`");

static PyObject *FEdgeSharp_material_right_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return BPy_FrsMaterial_from_FrsMaterial(self->fes->aFrsMaterial());
}

PyDoc_STRVAR(FEdgeSharp_material_left_doc,
             "The material of the face lying on the left of the FEdge.\n"
             "\n"
             ":type: :class:`Material`");

static PyObject *FEdgeSharp_material_left_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return BPy_FrsMaterial_from_FrsMaterial(self->fes->bFrsMaterial());
}

PyDoc_STRVAR(FEdgeSharp_face_mark_right_doc,
             "The face mark of the face lying on the right of the FEdge.  If this FEdge\n"
             "is a border, it has no face on the right and thus this property is set to\n"
             "false.\n"
             "\n"
             ":type: bool");

static PyObject *FEdgeSharp_face_mark_right_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return PyBool_from_bool(self->fes->aFaceMark());
}

static int FEdgeSharp_face_mark_right_set(BPy_FEdgeSharp *self,
                                          PyObject *value,
                                          void *UNUSED(closure))
{
  if (!PyBool_Check(value)) {
    return -1;
  }
  self->fes->setaFaceMark(bool_from_PyBool(value));
  return 0;
}

PyDoc_STRVAR(FEdgeSharp_face_mark_left_doc,
             "The face mark of the face lying on the left of the FEdge.\n"
             "\n"
             ":type: bool");

static PyObject *FEdgeSharp_face_mark_left_get(BPy_FEdgeSharp *self, void *UNUSED(closure))
{
  return PyBool_from_bool(self->fes->bFaceMark());
}

static int FEdgeSharp_face_mark_left_set(BPy_FEdgeSharp *self,
                                         PyObject *value,
                                         void *UNUSED(closure))
{
  if (!PyBool_Check(value)) {
    return -1;
  }
  self->fes->setbFaceMark(bool_from_PyBool(value));
  return 0;
}

static PyGetSetDef BPy_FEdgeSharp_getseters[] = {
    {"normal_right",
     (getter)FEdgeSharp_normal_right_get,
     (setter)FEdgeSharp_normal_right_set,
     FEdgeSharp_normal_right_doc,
     nullptr},
    {"normal_left",
     (getter)FEdgeSharp_normal_left_get,
     (setter)FEdgeSharp_normal_left_set,
     FEdgeSharp_normal_left_doc,
     nullptr},
    {"material_index_right",
     (getter)FEdgeSharp_material_index_right_get,
     (setter)FEdgeSharp_material_index_right_set,
     FEdgeSharp_material_index_right_doc,
     nullptr},
    {"material_index_left",
     (getter)FEdgeSharp_material_index_left_get,
     (setter)FEdgeSharp_material_index_left_set,
     FEdgeSharp_material_index_left_doc,
     nullptr},
    {"material_right",
     (getter)FEdgeSharp_material_right_get,
     (setter) nullptr,
     FEdgeSharp_material_right_doc,
     nullptr},
    {"material_left",
     (getter)FEdgeSharp_material_left_get,
     (setter) nullptr,
     FEdgeSharp_material_left_doc,
     nullptr},
    {"face_mark_right",
     (getter)FEdgeSharp_face_mark_right_get,
     (setter)FEdgeSharp_face_mark_right_set,
     FEdgeSharp_face_mark_right_doc,
     nullptr},
    {"face_mark_left",
     (getter)FEdgeSharp_face_mark_left_get,
     (setter)FEdgeSharp_face_mark_left_set,
     FEdgeSharp_face_mark_left_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_FEdgeSharp type definition ------------------------------*/

PyTypeObject FEdgeSharp_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "FEdgeSharp", /* tp_name */
    sizeof(BPy_FEdgeSharp),                         /* tp_basicsize */
    0,                                              /* tp_itemsize */
    nullptr,                                        /* tp_dealloc */
    0,                                              /* tp_vectorcall_offset */
    nullptr,                                        /* tp_getattr */
    nullptr,                                        /* tp_setattr */
    nullptr,                                        /* tp_reserved */
    nullptr,                                        /* tp_repr */
    nullptr,                                        /* tp_as_number */
    nullptr,                                        /* tp_as_sequence */
    nullptr,                                        /* tp_as_mapping */
    nullptr,                                        /* tp_hash  */
    nullptr,                                        /* tp_call */
    nullptr,                                        /* tp_str */
    nullptr,                                        /* tp_getattro */
    nullptr,                                        /* tp_setattro */
    nullptr,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,       /* tp_flags */
    FEdgeSharp_doc,                                 /* tp_doc */
    nullptr,                                        /* tp_traverse */
    nullptr,                                        /* tp_clear */
    nullptr,                                        /* tp_richcompare */
    0,                                              /* tp_weaklistoffset */
    nullptr,                                        /* tp_iter */
    nullptr,                                        /* tp_iternext */
    nullptr,                                        /* tp_methods */
    nullptr,                                        /* tp_members */
    BPy_FEdgeSharp_getseters,                       /* tp_getset */
    &FEdge_Type,                                    /* tp_base */
    nullptr,                                        /* tp_dict */
    nullptr,                                        /* tp_descr_get */
    nullptr,                                        /* tp_descr_set */
    0,                                              /* tp_dictoffset */
    (initproc)FEdgeSharp_init,                      /* tp_init */
    nullptr,                                        /* tp_alloc */
    nullptr,                                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
