/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_sort.h"
#include "BLI_string_utils.hh"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "bmesh.hh"

#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "../mathutils/mathutils.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_utildefines.hh"

#include "bmesh_py_types.hh" /* own include */
#include "bmesh_py_types_customdata.hh"
#include "bmesh_py_types_meshdata.hh"
#include "bmesh_py_types_select.hh"

static void bm_dealloc_editmode_warn(BPy_BMesh *self);

/* Common Flags
 * ************ */

/* scene does not use BM_* flags. */
PyC_FlagSet bpy_bm_scene_vert_edge_face_flags[] = {
    {1, "VERT"},
    {2, "EDGE"},
    {4, "FACE"},
    {0, nullptr},
};

PyC_FlagSet bpy_bm_htype_vert_edge_face_flags[] = {
    {BM_VERT, "VERT"},
    {BM_EDGE, "EDGE"},
    {BM_FACE, "FACE"},
    {0, nullptr},
};

PyC_FlagSet bpy_bm_htype_all_flags[] = {
    {BM_VERT, "VERT"},
    {BM_LOOP, "EDGE"},
    {BM_FACE, "FACE"},
    {BM_LOOP, "LOOP"},
    {0, nullptr},
};

/** This may be used with a `Literal[...]` typing expression. */
#define BPY_BM_HTYPE_NOLOOP "'VERT', 'EDGE', 'FACE'"

/** This may be used with a `Literal[...]` typing expression. */
#define BPY_BM_HFLAG_ALL_STR "'SELECT', 'HIDE', 'SEAM', 'SMOOTH', 'TAG'"

PyC_FlagSet bpy_bm_hflag_all_flags[] = {
    {BM_ELEM_SELECT, "SELECT"},
    {BM_ELEM_HIDDEN, "HIDE"},
    {BM_ELEM_SEAM, "SEAM"},
    {BM_ELEM_SMOOTH, "SMOOTH"},
    {BM_ELEM_TAG, "TAG"},
    {0, nullptr},
};

/* This could/should be shared with `scene.toolsettings.uv_sticky_select_mode`.
 * however it relies on using the RNA API. */
static PyC_StringEnumItems bpy_bm_uv_select_sticky_items[] = {
    {UV_STICKY_LOCATION, "SHARED_LOCATION"},
    {UV_STICKY_DISABLE, "DISABLED"},
    {UV_STICKY_VERT, "SHARED_VERTEX"},
    {0, nullptr},
};

/* py-type definitions
 * ******************* */

/* getseters
 * ========= */

/* bmesh elems
 * ----------- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_select_doc,
    "Selected state of this element.\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_hide_doc,
    "Hidden state of this element.\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_tag_doc,
    "Generic attribute scripts can use for own logic\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_smooth_doc,
    "Smooth state of this element.\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_seam_doc,
    "Seam for UV unwrapping.\n"
    "\n"
    ":type: bool\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_uv_select_doc,
    "UV selected state of this element.\n"
    "\n"
    ":type: bool\n");

static PyObject *bpy_bm_elem_hflag_get(BPy_BMElem *self, void *flag)
{
  const char hflag = char(POINTER_AS_INT(flag));

  BPY_BM_CHECK_OBJ(self);

  return PyBool_FromLong(BM_elem_flag_test(self->ele, hflag));
}

static int bpy_bm_elem_hflag_set(BPy_BMElem *self, PyObject *value, void *flag)
{
  const char hflag = char(POINTER_AS_INT(flag));
  int param;

  BPY_BM_CHECK_INT(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return -1;
  }

  if (hflag == BM_ELEM_SELECT) {
    BM_elem_select_set(self->bm, self->ele, param);
  }
  else {
    BM_elem_flag_set(self->ele, hflag, param);
  }
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_index_doc,
    "Index of this element.\n"
    "\n"
    ":type: int\n"
    "\n"
    ".. note::\n"
    "\n"
    "   This value is not necessarily valid, while editing the mesh it can become *dirty*.\n"
    "\n"
    "   It's also possible to assign any number to this attribute for a scripts internal logic.\n"
    "\n"
    "   To ensure the value is up to date - see :class:`bmesh.types.BMElemSeq.index_update`.\n");
static PyObject *bpy_bm_elem_index_get(BPy_BMElem *self, void * /*flag*/)
{
  BPY_BM_CHECK_OBJ(self);

  return PyLong_FromLong(BM_elem_index_get(self->ele));
}

static int bpy_bm_elem_index_set(BPy_BMElem *self, PyObject *value, void * /*flag*/)
{
  int param;

  BPY_BM_CHECK_INT(self);

  if (((param = PyC_Long_AsI32(value)) == -1) && PyErr_Occurred()) {
    /* error is set */
    return -1;
  }

  BM_elem_index_set(self->ele, param); /* set_dirty! */

  /* when setting the index assume its set invalid */
  self->bm->elem_index_dirty |= self->ele->head.htype;

  return 0;
}

/* type specific get/sets
 * ---------------------- */

/* Mesh
 * ^^^^ */

/* doc-strings for all uses of this function */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertseq_doc,
    "This meshes vert sequence (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMVertSeq`\n");
static PyObject *bpy_bmvertseq_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMVertSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedgeseq_doc,
    "This meshes edge sequence (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMEdgeSeq`\n");
static PyObject *bpy_bmedgeseq_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMEdgeSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmfaceseq_doc,
    "This meshes face sequence (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMFaceSeq`\n");
static PyObject *bpy_bmfaceseq_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMFaceSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloopseq_doc,
    "This meshes loops (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLoopSeq`\n"
    "\n"
    ".. note::\n"
    "\n"
    "   Loops must be accessed via faces, this is only exposed for layer access.\n");
static PyObject *bpy_bmloopseq_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMLoopSeq_CreatePyObject(self->bm);
}

/* vert */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_link_edges_doc,
    "Edges connected to this vertex (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMEdge`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_link_faces_doc,
    "Faces connected to this vertex (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMFace`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_link_loops_doc,
    "Loops that use this vertex (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMLoop`\n");
/* edge */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_verts_doc,
    "Verts this edge uses (always 2), (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of "
    ":class:`bmesh.types.BMVert`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_link_faces_doc,
    "Faces connected to this edge, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMFace`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_link_loops_doc,
    "Loops connected to this edge, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMLoop`\n");
/* face */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_verts_doc,
    "Verts of this face, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMVert`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_edges_doc,
    "Edges of this face, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMEdge`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_loops_doc,
    "Loops of this face, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMLoop`\n");
/* loop */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloops_link_loops_doc,
    "Loops connected to this loop, (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMElemSeq` of :class:`bmesh.types.BMLoop`\n");

static PyObject *bpy_bmelemseq_elem_get(BPy_BMElem *self, void *itype)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMElemSeq_CreatePyObject(self->bm, self, POINTER_AS_INT(itype));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_is_valid_doc,
    "True when this element is valid (hasn't been removed).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bm_is_valid_get(BPy_BMGeneric *self, void * /*closure*/)
{
  return PyBool_FromLong(BPY_BM_IS_VALID(self));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_is_wrapped_doc,
    "True when this mesh is owned by blender (typically the editmode BMesh).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmesh_is_wrapped_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);

  return PyBool_FromLong(self->flag & BPY_BMFLAG_IS_WRAPPED);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_select_mode_doc,
    "The selection mode, cannot be assigned an empty set.\n"
    "\n"
    ":type: set[Literal[" BPY_BM_HTYPE_NOLOOP "]]\n");
static PyObject *bpy_bmesh_select_mode_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);

  return PyC_FlagSet_FromBitfield(bpy_bm_scene_vert_edge_face_flags, self->bm->selectmode);
}

static int bpy_bmesh_select_mode_set(BPy_BMesh *self, PyObject *value, void * /*closure*/)
{
  int flag = 0;
  BPY_BM_CHECK_INT(self);

  if (PyC_FlagSet_ToBitfield(bpy_bm_scene_vert_edge_face_flags, value, &flag, "bm.select_mode") ==
      -1)
  {
    return -1;
  }
  if (flag == 0) {
    PyErr_SetString(PyExc_TypeError, "bm.select_mode: cannot assign an empty value");
    return -1;
  }

  self->bm->selectmode = flag;
  return 0;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_select_history_doc,
    "Sequence of selected items (the last is displayed as active).\n"
    "\n"
    ":type: "
    ":class:`bmesh.types.BMEditSelSeq`\n");
static PyObject *bpy_bmesh_select_history_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);

  return BPy_BMEditSel_CreatePyObject(self->bm);
}

static int bpy_bmesh_select_history_set(BPy_BMesh *self, PyObject *value, void * /*closure*/)
{
  BPY_BM_CHECK_INT(self);

  return BPy_BMEditSel_Assign(self, value);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_sync_valid_doc,
    "When true, the UV selection has been synchronized. "
    "Setting to False means the UV selection will be ignored. "
    "While setting to true is supported it is up to the script author to "
    "ensure a correct selection state before doing so.\n"
    ":type: "
    "bool\n");
static PyObject *bpy_bmesh_uv_select_sync_valid_get(BPy_BMesh *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);

  return PyBool_FromLong(self->bm->uv_select_sync_valid);
}

static int bpy_bmesh_uv_select_sync_valid_set(BPy_BMesh *self, PyObject *value, void * /*closure*/)
{
  BPY_BM_CHECK_INT(self);

  int param;
  if ((param = PyC_Long_AsBool(value)) == -1) {
    return -1;
  }
  self->bm->uv_select_sync_valid = param;
  return 0;
}

/* Vert
 * ^^^^ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_co_doc,
    "The coordinates for this vertex as a 3D, wrapped vector.\n"
    "\n"
    ":type: "
    ":class:`mathutils.Vector`\n");
static PyObject *bpy_bmvert_co_get(BPy_BMVert *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return Vector_CreatePyObject_wrap(self->v->co, 3, nullptr);
}

static int bpy_bmvert_co_set(BPy_BMVert *self, PyObject *value, void * /*closure*/)
{
  BPY_BM_CHECK_INT(self);

  if (mathutils_array_parse(self->v->co, 3, 3, value, "BMVert.co") != -1) {
    return 0;
  }

  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_normal_doc,
    "The normal for this vertex as a 3D, wrapped vector.\n"
    "\n"
    ":type: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmvert_normal_get(BPy_BMVert *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return Vector_CreatePyObject_wrap(self->v->no, 3, nullptr);
}

static int bpy_bmvert_normal_set(BPy_BMVert *self, PyObject *value, void * /*closure*/)
{
  BPY_BM_CHECK_INT(self);

  if (mathutils_array_parse(self->v->no, 3, 3, value, "BMVert.normal") != -1) {
    return 0;
  }

  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_is_manifold_doc,
    "True when this vertex is manifold (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmvert_is_manifold_get(BPy_BMVert *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_vert_is_manifold(self->v));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_is_wire_doc,
    "True when this vertex is not connected to any faces (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmvert_is_wire_get(BPy_BMVert *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_vert_is_wire(self->v));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_is_boundary_doc,
    "True when this vertex is connected to boundary edges (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmvert_is_boundary_get(BPy_BMVert *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_vert_is_boundary(self->v));
}

/* Edge
 * ^^^^ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_is_manifold_doc,
    "True when this edge is manifold (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmedge_is_manifold_get(BPy_BMEdge *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_edge_is_manifold(self->e));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_is_contiguous_doc,
    "True when this edge is manifold, between two faces with the same winding "
    "(read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmedge_is_contiguous_get(BPy_BMEdge *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_edge_is_contiguous(self->e));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_is_convex_doc,
    "True when this edge joins two convex faces, depends on a valid face normal (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmedge_is_convex_get(BPy_BMEdge *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_edge_is_convex(self->e));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_is_wire_doc,
    "True when this edge is not connected to any faces (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmedge_is_wire_get(BPy_BMEdge *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_edge_is_wire(self->e));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_is_boundary_doc,
    "True when this edge is at the boundary of a face (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmedge_is_boundary_get(BPy_BMEdge *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_edge_is_boundary(self->e));
}

/* Face
 * ^^^^ */

PyDoc_STRVAR(
    /* Warp. */
    bpy_bmface_normal_doc,
    "The normal for this face as a 3D, wrapped vector.\n"
    "\n"
    ":type: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_normal_get(BPy_BMFace *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return Vector_CreatePyObject_wrap(self->f->no, 3, nullptr);
}

static int bpy_bmface_normal_set(BPy_BMFace *self, PyObject *value, void * /*closure*/)
{
  BPY_BM_CHECK_INT(self);

  if (mathutils_array_parse(self->f->no, 3, 3, value, "BMFace.normal") != -1) {
    return 0;
  }

  return -1;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_material_index_doc,
    "The face's material index.\n"
    "\n"
    ":type: int\n");
static PyObject *bpy_bmface_material_index_get(BPy_BMFace *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyLong_FromLong(self->f->mat_nr);
}

static int bpy_bmface_material_index_set(BPy_BMFace *self, PyObject *value, void * /*closure*/)
{
  int param;

  BPY_BM_CHECK_INT(self);

  if (((param = PyC_Long_AsI32(value)) == -1) && PyErr_Occurred()) {
    /* error is set */
    return -1;
  }

  if ((param < 0) || (param > MAXMAT)) {
    /* normally we clamp but in this case raise an error */
    PyErr_SetString(PyExc_ValueError, "material index outside of usable range (0 - 32766)");
    return -1;
  }

  self->f->mat_nr = short(param);
  return 0;
}

/* Loop
 * ^^^^ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_vert_doc,
    "The loop's vertex (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMVert`\n");
static PyObject *bpy_bmloop_vert_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMVert_CreatePyObject(self->bm, self->l->v);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_edge_doc,
    "The loop's edge (between this loop and the next), (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bmloop_edge_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMEdge_CreatePyObject(self->bm, self->l->e);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_face_doc,
    "The face this loop makes (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmloop_face_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMFace_CreatePyObject(self->bm, self->l->f);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_link_loop_next_doc,
    "The next face corner (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLoop`\n");
static PyObject *bpy_bmloop_link_loop_next_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMLoop_CreatePyObject(self->bm, self->l->next);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_link_loop_prev_doc,
    "The previous face corner (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLoop`\n");
static PyObject *bpy_bmloop_link_loop_prev_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMLoop_CreatePyObject(self->bm, self->l->prev);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_link_loop_radial_next_doc,
    "The next loop around the edge (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLoop`\n");
static PyObject *bpy_bmloop_link_loop_radial_next_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMLoop_CreatePyObject(self->bm, self->l->radial_next);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_link_loop_radial_prev_doc,
    "The previous loop around the edge (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLoop`\n");
static PyObject *bpy_bmloop_link_loop_radial_prev_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return BPy_BMLoop_CreatePyObject(self->bm, self->l->radial_prev);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_is_convex_doc,
    "True when this loop is at the convex corner of a face, depends on a valid face "
    "normal (read-only).\n"
    "\n"
    ":type: bool\n");
static PyObject *bpy_bmloop_is_convex_get(BPy_BMLoop *self, void * /*closure*/)
{
  BPY_BM_CHECK_OBJ(self);
  return PyBool_FromLong(BM_loop_is_convex(self->l));
}

/* ElemSeq
 * ^^^^^^^ */

/* NOTE: use for bmvert/edge/face/loop seq's use these, not bmelemseq directly. */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_layers_vert_doc,
    "custom-data layers (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerAccessVert`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_layers_edge_doc,
    "custom-data layers (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerAccessEdge`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_layers_face_doc,
    "custom-data layers (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerAccessFace`\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_layers_loop_doc,
    "custom-data layers (read-only).\n"
    "\n"
    ":type: :class:`bmesh.types.BMLayerAccessLoop`\n");
static PyObject *bpy_bmelemseq_layers_get(BPy_BMElemSeq *self, void *htype)
{
  BPY_BM_CHECK_OBJ(self);

  return BPy_BMLayerAccess_CreatePyObject(self->bm, POINTER_AS_INT(htype));
}

/* FaceSeq
 * ^^^^^^^ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmfaceseq_active_doc,
    "active face.\n"
    "\n"
    ":type: :class:`bmesh.types.BMFace` | None\n");
static PyObject *bpy_bmfaceseq_active_get(BPy_BMElemSeq *self, void * /*closure*/)
{
  BMesh *bm = self->bm;
  BPY_BM_CHECK_OBJ(self);

  if (bm->act_face) {
    return BPy_BMElem_CreatePyObject(bm, (BMHeader *)bm->act_face);
  }

  Py_RETURN_NONE;
}

static int bpy_bmfaceseq_active_set(BPy_BMElem *self, PyObject *value, void * /*closure*/)
{
  const char *error_prefix = "faces.active = f";
  BMesh *bm = self->bm;
  if (value == Py_None) {
    bm->act_face = nullptr;
    return 0;
  }
  if (BPy_BMFace_Check(value)) {
    BPY_BM_CHECK_SOURCE_INT(bm, error_prefix, value);

    bm->act_face = ((BPy_BMFace *)value)->f;
    return 0;
  }

  PyErr_Format(PyExc_TypeError,
               "%s: expected BMFace or None, not %.200s",
               error_prefix,
               Py_TYPE(value)->tp_name);
  return -1;
}

static PyGetSetDef bpy_bmesh_getseters[] = {
    {"verts", (getter)bpy_bmvertseq_get, (setter) nullptr, bpy_bmvertseq_doc, nullptr},
    {"edges", (getter)bpy_bmedgeseq_get, (setter) nullptr, bpy_bmedgeseq_doc, nullptr},
    {"faces", (getter)bpy_bmfaceseq_get, (setter) nullptr, bpy_bmfaceseq_doc, nullptr},
    {"loops", (getter)bpy_bmloopseq_get, (setter) nullptr, bpy_bmloopseq_doc, nullptr},
    {"select_mode",
     (getter)bpy_bmesh_select_mode_get,
     (setter)bpy_bmesh_select_mode_set,
     bpy_bmesh_select_mode_doc,
     nullptr},

    {"select_history",
     (getter)bpy_bmesh_select_history_get,
     (setter)bpy_bmesh_select_history_set,
     bpy_bmesh_select_history_doc,
     nullptr},

    {"uv_select_sync_valid",
     (getter)bpy_bmesh_uv_select_sync_valid_get,
     (setter)bpy_bmesh_uv_select_sync_valid_set,
     bpy_bmesh_uv_select_sync_valid_doc,
     nullptr},

    /* readonly checks */
    {"is_wrapped",
     (getter)bpy_bmesh_is_wrapped_get,
     (setter) nullptr,
     bpy_bmesh_is_wrapped_doc,
     nullptr}, /* as with mathutils */
    {"is_valid", (getter)bpy_bm_is_valid_get, (setter) nullptr, bpy_bm_is_valid_doc, nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmvert_getseters[] = {
    /* generic */
    {"select",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_select_doc,
     (void *)BM_ELEM_SELECT},
    {"hide",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_hide_doc,
     (void *)BM_ELEM_HIDDEN},
    {"tag",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_tag_doc,
     (void *)BM_ELEM_TAG},
    {"index",
     (getter)bpy_bm_elem_index_get,
     (setter)bpy_bm_elem_index_set,
     bpy_bm_elem_index_doc,
     nullptr},

    {"co", (getter)bpy_bmvert_co_get, (setter)bpy_bmvert_co_set, bpy_bmvert_co_doc, nullptr},
    {"normal",
     (getter)bpy_bmvert_normal_get,
     (setter)bpy_bmvert_normal_set,
     bpy_bmvert_normal_doc,
     nullptr},

    /* connectivity data */
    {"link_edges",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmvert_link_edges_doc,
     (void *)BM_EDGES_OF_VERT},
    {"link_faces",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmvert_link_faces_doc,
     (void *)BM_FACES_OF_VERT},
    {"link_loops",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmvert_link_loops_doc,
     (void *)BM_LOOPS_OF_VERT},

    /* readonly checks */
    {"is_manifold",
     (getter)bpy_bmvert_is_manifold_get,
     (setter) nullptr,
     bpy_bmvert_is_manifold_doc,
     nullptr},
    {"is_wire", (getter)bpy_bmvert_is_wire_get, (setter) nullptr, bpy_bmvert_is_wire_doc, nullptr},
    {"is_boundary",
     (getter)bpy_bmvert_is_boundary_get,
     (setter) nullptr,
     bpy_bmvert_is_boundary_doc,
     nullptr},
    {"is_valid", (getter)bpy_bm_is_valid_get, (setter) nullptr, bpy_bm_is_valid_doc, nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmedge_getseters[] = {
    /* generic */
    {"select",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_select_doc,
     (void *)BM_ELEM_SELECT},
    {"hide",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_hide_doc,
     (void *)BM_ELEM_HIDDEN},
    {"tag",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_tag_doc,
     (void *)BM_ELEM_TAG},
    {"index",
     (getter)bpy_bm_elem_index_get,
     (setter)bpy_bm_elem_index_set,
     bpy_bm_elem_index_doc,
     nullptr},

    {"smooth",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_smooth_doc,
     (void *)BM_ELEM_SMOOTH},
    {"seam",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_seam_doc,
     (void *)BM_ELEM_SEAM},

    /* connectivity data */
    {"verts",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmedge_verts_doc,
     (void *)BM_VERTS_OF_EDGE},

    {"link_faces",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmedge_link_faces_doc,
     (void *)BM_FACES_OF_EDGE},
    {"link_loops",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmedge_link_loops_doc,
     (void *)BM_LOOPS_OF_EDGE},

    /* readonly checks */
    {"is_manifold",
     (getter)bpy_bmedge_is_manifold_get,
     (setter) nullptr,
     bpy_bmedge_is_manifold_doc,
     nullptr},
    {"is_contiguous",
     (getter)bpy_bmedge_is_contiguous_get,
     (setter) nullptr,
     bpy_bmedge_is_contiguous_doc,
     nullptr},
    {"is_convex",
     (getter)bpy_bmedge_is_convex_get,
     (setter) nullptr,
     bpy_bmedge_is_convex_doc,
     nullptr},
    {"is_wire", (getter)bpy_bmedge_is_wire_get, (setter) nullptr, bpy_bmedge_is_wire_doc, nullptr},
    {"is_boundary",
     (getter)bpy_bmedge_is_boundary_get,
     (setter) nullptr,
     bpy_bmedge_is_boundary_doc,
     nullptr},
    {"is_valid", (getter)bpy_bm_is_valid_get, (setter) nullptr, bpy_bm_is_valid_doc, nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmface_getseters[] = {
    /* generic */
    {"select",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_select_doc,
     (void *)BM_ELEM_SELECT},
    {"hide",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_hide_doc,
     (void *)BM_ELEM_HIDDEN},
    {"tag",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_tag_doc,
     (void *)BM_ELEM_TAG},
    {"uv_select",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_uv_select_doc,
     (void *)BM_ELEM_SELECT_UV},
    {"index",
     (getter)bpy_bm_elem_index_get,
     (setter)bpy_bm_elem_index_set,
     bpy_bm_elem_index_doc,
     nullptr},

    {"smooth",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_smooth_doc,
     (void *)BM_ELEM_SMOOTH},

    {"normal",
     (getter)bpy_bmface_normal_get,
     (setter)bpy_bmface_normal_set,
     bpy_bmface_normal_doc,
     nullptr},

    {"material_index",
     (getter)bpy_bmface_material_index_get,
     (setter)bpy_bmface_material_index_set,
     bpy_bmface_material_index_doc,
     nullptr},

    /* connectivity data */
    {"verts",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmface_verts_doc,
     (void *)BM_VERTS_OF_FACE},
    {"edges",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmface_edges_doc,
     (void *)BM_EDGES_OF_FACE},
    {"loops",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmface_loops_doc,
     (void *)BM_LOOPS_OF_FACE},

    /* readonly checks */
    {"is_valid", (getter)bpy_bm_is_valid_get, (setter) nullptr, bpy_bm_is_valid_doc, nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmloop_getseters[] = {
/* generic */
/* flags are available but not used for loops. */
#if 0
    {"select",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_select_doc,
     (void *)BM_ELEM_SELECT},
    {"hide",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_hide_doc,
     (void *)BM_ELEM_HIDDEN},
#endif
    {"tag",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_tag_doc,
     (void *)BM_ELEM_TAG},
    {"uv_select_vert",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_uv_select_doc,
     (void *)BM_ELEM_SELECT_UV},
    {"uv_select_edge",
     (getter)bpy_bm_elem_hflag_get,
     (setter)bpy_bm_elem_hflag_set,
     bpy_bm_elem_uv_select_doc,
     (void *)BM_ELEM_SELECT_UV_EDGE},
    {"index",
     (getter)bpy_bm_elem_index_get,
     (setter)bpy_bm_elem_index_set,
     bpy_bm_elem_index_doc,
     nullptr},

    {"vert", (getter)bpy_bmloop_vert_get, (setter) nullptr, bpy_bmloop_vert_doc, nullptr},
    {"edge", (getter)bpy_bmloop_edge_get, (setter) nullptr, bpy_bmloop_edge_doc, nullptr},
    {"face", (getter)bpy_bmloop_face_get, (setter) nullptr, bpy_bmloop_face_doc, nullptr},

    /* connectivity data */
    {"link_loops",
     (getter)bpy_bmelemseq_elem_get,
     (setter) nullptr,
     bpy_bmloops_link_loops_doc,
     (void *)BM_LOOPS_OF_LOOP},
    {"link_loop_next",
     (getter)bpy_bmloop_link_loop_next_get,
     (setter) nullptr,
     bpy_bmloop_link_loop_next_doc,
     nullptr},
    {"link_loop_prev",
     (getter)bpy_bmloop_link_loop_prev_get,
     (setter) nullptr,
     bpy_bmloop_link_loop_prev_doc,
     nullptr},
    {"link_loop_radial_next",
     (getter)bpy_bmloop_link_loop_radial_next_get,
     (setter) nullptr,
     bpy_bmloop_link_loop_radial_next_doc,
     nullptr},
    {"link_loop_radial_prev",
     (getter)bpy_bmloop_link_loop_radial_prev_get,
     (setter) nullptr,
     bpy_bmloop_link_loop_radial_prev_doc,
     nullptr},

    /* readonly checks */
    {"is_convex",
     (getter)bpy_bmloop_is_convex_get,
     (setter) nullptr,
     bpy_bmloop_is_convex_doc,
     nullptr},
    {"is_valid", (getter)bpy_bm_is_valid_get, (setter) nullptr, bpy_bm_is_valid_doc, nullptr},

    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

static PyGetSetDef bpy_bmvertseq_getseters[] = {
    {"layers",
     (getter)bpy_bmelemseq_layers_get,
     (setter) nullptr,
     bpy_bmelemseq_layers_vert_doc,
     (void *)BM_VERT},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};
static PyGetSetDef bpy_bmedgeseq_getseters[] = {
    {"layers",
     (getter)bpy_bmelemseq_layers_get,
     (setter) nullptr,
     bpy_bmelemseq_layers_edge_doc,
     (void *)BM_EDGE},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};
static PyGetSetDef bpy_bmfaceseq_getseters[] = {
    {"layers",
     (getter)bpy_bmelemseq_layers_get,
     (setter) nullptr,
     bpy_bmelemseq_layers_face_doc,
     (void *)BM_FACE},
    /* face only */
    {"active",
     (getter)bpy_bmfaceseq_active_get,
     (setter)bpy_bmfaceseq_active_set,
     bpy_bmfaceseq_active_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};
static PyGetSetDef bpy_bmloopseq_getseters[] = {
    {"layers",
     (getter)bpy_bmelemseq_layers_get,
     (setter) nullptr,
     bpy_bmelemseq_layers_loop_doc,
     (void *)BM_LOOP},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/* Methods
 * ======= */

/* Mesh
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_copy_doc,
    ".. method:: copy()\n"
    "\n"
    "   :return: A copy of this BMesh.\n"
    "   :rtype: :class:`bmesh.types.BMesh`\n");
static PyObject *bpy_bmesh_copy(BPy_BMesh *self)
{
  BPY_BM_CHECK_OBJ(self);

  BMesh *bm = self->bm;
  BMesh *bm_copy = BM_mesh_copy(bm);

  if (bm_copy) {
    return BPy_BMesh_CreatePyObject(bm_copy, BPY_BMFLAG_NOP);
  }

  PyErr_SetString(PyExc_SystemError, "Unable to copy BMesh, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_clear_doc,
    ".. method:: clear()\n"
    "\n"
    "   Clear all mesh data.\n");
static PyObject *bpy_bmesh_clear(BPy_BMesh *self)
{
  BPY_BM_CHECK_OBJ(self);

  BMesh *bm = self->bm;

  BM_mesh_clear(bm);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_free_doc,
    ".. method:: free()\n"
    "\n"
    "   Explicitly free the BMesh data from memory, causing exceptions on further access.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      The BMesh is freed automatically, typically when the script finishes executing.\n"
    "      However in some cases its hard to predict when this will be and its useful to\n"
    "      explicitly free the data.\n");
static PyObject *bpy_bmesh_free(BPy_BMesh *self)
{
  if (self->bm) {
    BMesh *bm = self->bm;

    bm_dealloc_editmode_warn(self);

    if (self->flag & BPY_BMFLAG_IS_WRAPPED) {
      /* Ensure further access doesn't return this invalid object, see: #105715. */
      bm->py_handle = nullptr;
    }
    else {
      BM_mesh_free(bm);
    }

    bpy_bm_generic_invalidate((BPy_BMGeneric *)self);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_to_mesh_doc,
    ".. method:: to_mesh(mesh)\n"
    "\n"
    "   Writes this BMesh data into an existing Mesh data-block.\n"
    "\n"
    "   :arg mesh: The mesh data to write into.\n"
    "   :type mesh: :class:`bpy.types.Mesh`\n");
static PyObject *bpy_bmesh_to_mesh(BPy_BMesh *self, PyObject *args)
{
  PyObject *py_mesh;
  Mesh *mesh;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O:to_mesh", &py_mesh) ||
      !(mesh = static_cast<Mesh *>(PyC_RNA_AsPointer(py_mesh, "Mesh"))))
  {
    return nullptr;
  }

  /* we could allow this but its almost certainly _not_ what script authors want */
  if (mesh->runtime->edit_mesh) {
    PyErr_Format(PyExc_ValueError, "to_mesh(): Mesh '%s' is in editmode", mesh->id.name + 2);
    return nullptr;
  }

  BMesh *bm = self->bm;

  Main *bmain = nullptr;
  BMeshToMeshParams params{};
  params.update_shapekey_indices = true;
  if (mesh->id.tag & ID_TAG_NO_MAIN) {
    /* Mesh might be coming from a self-contained source like object.to_mesh(). No need to remap
     * anything in this case. */
  }
  else {
    BLI_assert(BKE_id_is_in_global_main(&mesh->id));
    bmain = G_MAIN; /* XXX UGLY! */
    params.calc_object_remap = true;
  }

  BM_mesh_bm_to_me(bmain, bm, mesh, &params);

  /* We could have the user do this but if they forget blender can easy crash
   * since the references arrays for the objects evaluated meshes are now invalid. */
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY_ALL_MODES);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_from_object_doc,
    ".. method:: from_object(object, depsgraph, *, "
    "cage=False, face_normals=True, vertex_normals=True)\n"
    "\n"
    "   Initialize this bmesh from existing object data-block (only meshes are currently "
    "supported).\n"
    "\n"
    "   :arg object: The object data to load.\n"
    "   :type object: :class:`bpy.types.Object`\n"
    "   :type depsgraph: :class:`bpy.types.Depsgraph`\n"
    "   :arg cage: Get the mesh as a deformed cage.\n"
    "   :type cage: bool\n"
    "   :arg face_normals: Calculate face normals.\n"
    "   :type face_normals: bool\n"
    "   :arg vertex_normals: Calculate vertex normals.\n"
    "   :type vertex_normals: bool\n");
static PyObject *bpy_bmesh_from_object(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {
      "object", "depsgraph", "cage", "face_normals", "vertex_normals", nullptr};
  PyObject *py_object;
  PyObject *py_depsgraph;
  Object *ob, *ob_eval;
  Depsgraph *depsgraph;
  Scene *scene_eval;
  const Mesh *mesh_eval;
  bool use_cage = false;
  bool use_fnorm = true;
  bool use_vert_normal = true;
  const CustomData_MeshMasks data_masks = CD_MASK_BMESH;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "OO|$O&O&O&:from_object",
                                   (char **)kwlist,
                                   &py_object,
                                   &py_depsgraph,
                                   PyC_ParseBool,
                                   &use_cage,
                                   PyC_ParseBool,
                                   &use_fnorm,
                                   PyC_ParseBool,
                                   &use_vert_normal) ||
      !(ob = static_cast<Object *>(PyC_RNA_AsPointer(py_object, "Object"))) ||
      !(depsgraph = static_cast<Depsgraph *>(PyC_RNA_AsPointer(py_depsgraph, "Depsgraph"))))
  {
    return nullptr;
  }

  if (ob->type != OB_MESH) {
    PyErr_SetString(PyExc_ValueError,
                    "from_object(...): currently only mesh objects are supported");
    return nullptr;
  }

  const bool use_render = DEG_get_mode(depsgraph) == DAG_EVAL_RENDER;
  scene_eval = DEG_get_evaluated_scene(depsgraph);
  ob_eval = DEG_get_evaluated(depsgraph, ob);
  bool need_free = false;

  /* Write the display mesh into the dummy mesh */
  if (use_render) {
    if (use_cage) {
      PyErr_SetString(PyExc_ValueError,
                      "from_object(...): cage arg is unsupported when dependency graph "
                      "evaluation mode is RENDER");
      return nullptr;
    }

    mesh_eval = BKE_mesh_new_from_object(depsgraph, ob_eval, true, false, true);
    need_free = true;
  }
  else {
    if (use_cage) {
      mesh_eval = blender::bke::mesh_get_eval_deform(depsgraph, scene_eval, ob_eval, &data_masks);
    }
    else {
      mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
    }
  }

  if (mesh_eval == nullptr) {
    PyErr_Format(PyExc_ValueError,
                 "from_object(...): Object '%s' has no usable mesh data",
                 ob->id.name + 2);
    return nullptr;
  }

  BMesh *bm = self->bm;

  BMeshFromMeshParams params{};
  params.calc_face_normal = use_fnorm;
  params.calc_vert_normal = use_vert_normal;
  BM_mesh_bm_from_me(bm, mesh_eval, &params);

  if (need_free) {
    BKE_id_free(nullptr, (Mesh *)mesh_eval);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_from_mesh_doc,
    ".. method:: from_mesh(mesh, *, "
    "face_normals=True, vertex_normals=True, use_shape_key=False, shape_key_index=0)\n"
    "\n"
    "   Initialize this bmesh from existing mesh data-block.\n"
    "\n"
    "   :arg mesh: The mesh data to load.\n"
    "   :type mesh: :class:`bpy.types.Mesh`\n"
    "   :type face_normals: bool\n"
    "   :type vertex_normals: bool\n"
    "   :arg use_shape_key: Use the locations from a shape key.\n"
    "   :type use_shape_key: bool\n"
    "   :arg shape_key_index: The shape key index to use.\n"
    "   :type shape_key_index: int\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Multiple calls can be used to join multiple meshes.\n"
    "\n"
    "      Custom-data layers are only copied from ``mesh`` on initialization.\n"
    "      Further calls will copy custom-data to matching layers, layers missing on the target "
    "mesh won't be added.\n");
static PyObject *bpy_bmesh_from_mesh(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {
      "mesh", "face_normals", "vertex_normals", "use_shape_key", "shape_key_index", nullptr};
  PyObject *py_mesh;
  Mesh *mesh;
  bool use_fnorm = true;
  bool use_vert_normal = true;
  bool use_shape_key = false;
  int shape_key_index = 0;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O|$O&O&O&i:from_mesh",
                                   (char **)kwlist,
                                   &py_mesh,
                                   PyC_ParseBool,
                                   &use_fnorm,
                                   PyC_ParseBool,
                                   &use_vert_normal,
                                   PyC_ParseBool,
                                   &use_shape_key,
                                   &shape_key_index) ||
      !(mesh = static_cast<Mesh *>(PyC_RNA_AsPointer(py_mesh, "Mesh"))))
  {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BMeshFromMeshParams params{};
  params.calc_face_normal = use_fnorm;
  params.calc_vert_normal = use_vert_normal;
  params.use_shapekey = use_shape_key;
  params.active_shapekey = shape_key_index + 1;
  BM_mesh_bm_from_me(bm, mesh, &params);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_select_flush_mode_doc,
    ".. method:: select_flush_mode(*, flush_down=False)\n"
    "\n"
    "   Flush selection based on the current mode current "
    ":class:`bmesh.types.BMesh.select_mode`.\n"
    "\n"
    "   :arg flush_down: Flush selection down from faces to edges & verts or from edges to verts. "
    "This option is ignored when vertex selection mode is enabled.\n"
    "   :type flush_down: bool\n");
static PyObject *bpy_bmesh_select_flush_mode(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  BPY_BM_CHECK_OBJ(self);

  bool flush_down = false;
  BMSelectFlushFlag flag = BMSelectFlushFlag_Default;

  static const char *kwlist[] = {
      "flush_down",
      nullptr,
  };
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "|$"
                                   "O&" /* `flush_down` */
                                   ":select_flush_mode",
                                   (char **)kwlist,
                                   PyC_ParseBool,
                                   &flush_down))
  {
    return nullptr;
  }

  if (flush_down) {
    flag |= BMSelectFlushFlag::Down;
  }

  BM_mesh_select_mode_flush_ex(self->bm, self->bm->selectmode, flag);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_select_flush_doc,
    ".. method:: select_flush(select)\n"
    "\n"
    "   Flush selection from vertices, independent of the current selection mode.\n"
    "\n"
    "   :arg select: flush selection or de-selected elements.\n"
    "   :type select: bool\n");
static PyObject *bpy_bmesh_select_flush(BPy_BMesh *self, PyObject *value)
{
  int param;

  BPY_BM_CHECK_OBJ(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }
  BM_mesh_select_flush_from_verts(self->bm, param);
  Py_RETURN_NONE;
}

/* ---------------------------------------------------------------------- */
/** \name UV Sync Selection
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_flush_mode_doc,
    ".. method:: uv_select_flush_mode(*, flush_down=False)\n"
    "\n"
    "   Flush selection based on the current mode current :class:`BMesh.select_mode`.\n"
    "\n"
    "   :arg flush_down: Flush selection down from faces to edges & verts or from edges to verts. "
    "This option is ignored when vertex selection mode is enabled.\n"
    "   :type flush_down: bool\n");
static PyObject *bpy_bmesh_uv_select_flush_mode(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  BPY_BM_CHECK_OBJ(self);
  BMesh *bm = self->bm;

  bool flush_down = false;
  static const char *kwlist[] = {
      "flush_down",
      nullptr,
  };
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "|$"
                                   "O&" /* `flush_down` */
                                   ":uv_select_flush_mode",
                                   (char **)kwlist,
                                   PyC_ParseBool,
                                   &flush_down))
  {
    return nullptr;
  }

  BM_mesh_uvselect_mode_flush_ex(bm, bm->selectmode, flush_down);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_flush_doc,
    ".. method:: uv_select_flush(select)\n"
    "\n"
    "   Flush selection from UV vertices to edges & faces independent of the selection mode.\n"
    "\n"
    "   :arg select: Flush selection or de-selected elements.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      - |UV_SELECT_SYNC_TO_MESH_NEEDED|\n");
static PyObject *bpy_bmesh_uv_select_flush(BPy_BMesh *self, PyObject *value)
{
  const char *error_prefix = "uv_select_flush(...)";
  int param;

  BPY_BM_CHECK_OBJ(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }
  BMesh *bm = self->bm;
  /* While sync doesn't need to be valid,
   * failing to make it valid causes selection functions to assert, so require it to be valid. */
  if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
    return nullptr;
  }
  BM_mesh_uvselect_flush_from_verts(bm, param);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_flush_shared_doc,
    ".. method:: uv_select_flush_shared(select)\n"
    "\n"
    "   Flush selection from UV vertices to contiguous UV's independent of the selection mode.\n"
    "\n"
    "   :arg select: Flush selection or de-selected elements.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      - |UV_SELECT_SYNC_TO_MESH_NEEDED|\n");
static PyObject *bpy_bmesh_uv_select_flush_shared(BPy_BMesh *self, PyObject *value)
{
  const char *error_prefix = "uv_select_flush_shared(...)";
  int param;

  BPY_BM_CHECK_OBJ(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }
  BMesh *bm = self->bm;
  /* While sync doesn't need to be valid,
   * failing to make it valid causes selection functions to assert, so require it to be valid. */
  if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
    return nullptr;
  }
  if (param) {
    BM_mesh_uvselect_flush_shared_only_select(bm, param);
  }
  else {
    BM_mesh_uvselect_flush_shared_only_deselect(bm, param);
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_sync_from_mesh_doc,
    ".. method:: uv_select_sync_from_mesh(*, "
    "sticky_select_mode='SHARED_LOCATION')\n"
    "\n"
    "   Sync selection from mesh to UVs.\n"
    "\n"
    "   :arg sticky_select_mode: Behavior when flushing from the mesh to UV selection "
    "|UV_STICKY_SELECT_MODE_REF|. "
    "This should only be used when preparing to create a UV selection.\n"
    "   :type sticky_select_mode: |UV_STICKY_SELECT_MODE_TYPE|\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      - |UV_SELECT_SYNC_TO_MESH_NEEDED|\n");
static PyObject *bpy_bmesh_uv_select_sync_from_mesh(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {
      "sticky_select_mode",
      nullptr,
  };

  BPY_BM_CHECK_OBJ(self);

  PyC_StringEnum uv_sticky_select_mode = {bpy_bm_uv_select_sticky_items, UV_STICKY_LOCATION};

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "|$" /* Optional keyword only arguments. */
                                   "O&" /* `sticky_select_mode` */
                                   ":uv_select_sync_from_mesh",
                                   (char **)kwlist,
                                   PyC_ParseStringEnum,
                                   &uv_sticky_select_mode))
  {
    return nullptr;
  }

  BMesh *bm = self->bm;
  switch (uv_sticky_select_mode.value_found) {
    case UV_STICKY_LOCATION: {
      const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);
      if (cd_loop_uv_offset == -1) {
        PyErr_SetString(PyExc_ValueError, "sticky_select_mode='SHARED_LOCATION' requires UV's");
        return nullptr;
      }
      BM_mesh_uvselect_sync_from_mesh_sticky_location(bm, cd_loop_uv_offset);
      break;
    }
    case UV_STICKY_DISABLE: {
      BM_mesh_uvselect_sync_from_mesh_sticky_disabled(bm);
      break;
    }
    case UV_STICKY_VERT: {
      BM_mesh_uvselect_sync_from_mesh_sticky_vert(bm);
      break;
    }
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_sync_to_mesh_doc,
    ".. method:: uv_select_sync_to_mesh()\n"
    "\n"
    "   Sync selection from UVs to the mesh.\n");
static PyObject *bpy_bmesh_uv_select_sync_to_mesh(BPy_BMesh *self)
{
  const char *error_prefix = "uv_select_sync_to_mesh(...)";

  BPY_BM_CHECK_OBJ(self);

  BMesh *bm = self->bm;
  if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
    return nullptr;
  }
  BM_mesh_uvselect_sync_to_mesh(bm);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_foreach_set_doc,
    ".. method:: uv_select_foreach_set(select, /, *, "
    "loop_verts=(), loop_edges=(), faces=(), sticky_select_mode='SHARED_LOCATION')\n"
    "\n"
    "   Set the UV selection state for loop-vertices, loop-edges & faces.\n"
    "\n"
    "   This is a close equivalent to selecting in the UV editor.\n"
    "\n"
    "   :arg select: The selection state to set.\n"
    "   :type select: bool\n"
    "   :arg loop_verts: Loop verts to operate on.\n"
    "   :type loop_verts: Iterable[:class:`bmesh.types.BMLoop`]\n"
    "   :arg loop_edges: Loop edges to operate on.\n"
    "   :type loop_edges: Iterable[:class:`bmesh.types.BMLoop`]\n"
    "   :arg faces: Faces to operate on.\n"
    "   :type faces: Iterable[:class:`bmesh.types.BMFace`]\n"
    "   :arg sticky_select_mode: See |UV_STICKY_SELECT_MODE_REF|.\n"
    "   :type sticky_select_mode: |UV_STICKY_SELECT_MODE_TYPE|\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      - |UV_SELECT_FLUSH_MODE_NEEDED|\n"
    "      - |UV_SELECT_SYNC_TO_MESH_NEEDED|\n");
static PyObject *bpy_bmesh_uv_select_foreach_set(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
  const char *error_prefix = "uv_select_foreach_set(...)";
  static const char *kwlist[] = {
      "", /* `select` */
      "loop_verts",
      "loop_edges",
      "faces",
      "sticky_select_mode",
      nullptr,
  };
  BMesh *bm;
  bool use_select = false;
  PyObject *py_loop_verts = nullptr;
  PyObject *py_loop_edges = nullptr;
  PyObject *py_faces = nullptr;
  PyC_StringEnum uv_sticky_select_mode = {bpy_bm_uv_select_sticky_items, UV_STICKY_LOCATION};

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O&" /* `select` */
                                   "|$" /* Optional keyword only arguments. */
                                   "O"  /* `loop_verts` */
                                   "O"  /* `loop_edges` */
                                   "O"  /* `faces` */
                                   "O&" /* `sticky_select_mode` */
                                   ":uv_select_foreach_set",
                                   (char **)kwlist,
                                   PyC_ParseBool,
                                   &use_select,
                                   &py_loop_verts,
                                   &py_loop_edges,
                                   &py_faces,
                                   PyC_ParseStringEnum,
                                   &uv_sticky_select_mode))
  {
    return nullptr;
  }

  bm = self->bm;
  if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
    return nullptr;
  }
  const bool shared = uv_sticky_select_mode.value_found == UV_STICKY_LOCATION;
  const int cd_loop_uv_offset = shared ? bpy_bm_uv_layer_offset_or_error(bm, error_prefix) : -1;
  if (shared && (cd_loop_uv_offset == -1)) {
    return nullptr;
  }

  Py_ssize_t loop_vert_array_num = 0;
  Py_ssize_t loop_edge_array_num = 0;
  Py_ssize_t face_array_num = 0;
  BMLoop **loop_vert_array = nullptr;
  BMLoop **loop_edge_array = nullptr;
  BMFace **face_array = nullptr;

  bool ok = true;
  if (ok && py_loop_verts) {
    BMesh *bm_test = nullptr;
    if (!(loop_vert_array = BPy_BMLoop_PySeq_As_Array(&bm_test,
                                                      py_loop_verts,
                                                      0,
                                                      PY_SSIZE_T_MAX,
                                                      &loop_vert_array_num,
                                                      true,
                                                      true,
                                                      error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }
  if (ok && py_loop_edges) {
    BMesh *bm_test = nullptr;
    if (!(loop_edge_array = BPy_BMLoop_PySeq_As_Array(&bm_test,
                                                      py_loop_edges,
                                                      0,
                                                      PY_SSIZE_T_MAX,
                                                      &loop_edge_array_num,
                                                      true,
                                                      true,
                                                      error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }
  if (ok && py_faces) {
    BMesh *bm_test = nullptr;
    if (!(face_array = BPy_BMFace_PySeq_As_Array(
              &bm_test, py_faces, 0, PY_SSIZE_T_MAX, &face_array_num, true, true, error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }

  /* TODO: support different "sticky" modes. */
  if (ok) {
    BM_mesh_uvselect_set_elem_shared(bm,
                                     use_select,
                                     cd_loop_uv_offset,
                                     blender::Span(loop_vert_array, loop_vert_array_num),
                                     blender::Span(loop_edge_array, loop_edge_array_num),
                                     blender::Span(face_array, face_array_num));
  }

  PyMem_FREE(loop_vert_array);
  PyMem_FREE(loop_edge_array);
  PyMem_FREE(face_array);

  if (ok == false) {
    /* The error has been raised. */
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_uv_select_foreach_set_from_mesh_doc,
    ".. method:: uv_select_foreach_set_from_mesh(select, /, *, "
    "verts=(), edges=(), faces=(), sticky_select_mode='SHARED_LOCATION')\n"
    "\n"
    "   Select or de-select mesh elements, updating the UV selection.\n"
    "\n"
    "   An equivalent to selecting from the 3D viewport "
    "for selection operations that support maintaining a synchronized UV selection.\n"
    "\n"
    "   :arg select: The selection state to set.\n"
    "   :type select: bool\n"
    "   :arg verts: Verts to operate on.\n"
    "   :type verts: Iterable[:class:`bmesh.types.BMVert`]\n"
    "   :arg edges: Edges to operate on.\n"
    "   :type edges: Iterable[:class:`bmesh.types.BMEdge`]\n"
    "   :arg faces: Faces to operate on.\n"
    "   :type faces: Iterable[:class:`bmesh.types.BMFace`]\n"
    "   :arg sticky_select_mode: See |UV_STICKY_SELECT_MODE_REF|.\n"
    "   :type sticky_select_mode: |UV_STICKY_SELECT_MODE_TYPE|\n");
static PyObject *bpy_bmesh_uv_select_foreach_set_from_mesh(BPy_BMesh *self,
                                                           PyObject *args,
                                                           PyObject *kw)
{
  const char *error_prefix = "uv_select_foreach_set_from_mesh(...)";
  static const char *kwlist[] = {
      "", /* `select` */
      "verts",
      "edges",
      "faces",
      "sticky_select_mode",
      nullptr,
  };
  bool use_select = false;
  PyObject *py_verts = nullptr;
  PyObject *py_edges = nullptr;
  PyObject *py_faces = nullptr;
  PyC_StringEnum uv_sticky_select_mode = {bpy_bm_uv_select_sticky_items, UV_STICKY_LOCATION};

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O&" /* `select` */
                                   "|$" /* Optional keyword only arguments. */
                                   "O"  /* `verts` */
                                   "O"  /* `edges` */
                                   "O"  /* `faces` */
                                   "O&" /* `sticky_select_mode` */
                                   ":uv_select_foreach_set_from_mesh",
                                   (char **)kwlist,
                                   PyC_ParseBool,
                                   &use_select,
                                   &py_verts,
                                   &py_edges,
                                   &py_faces,
                                   PyC_ParseStringEnum,
                                   &uv_sticky_select_mode))
  {
    return nullptr;
  }

  BMesh *bm = self->bm;
  if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
    return nullptr;
  }
  const bool shared = uv_sticky_select_mode.value_found == UV_STICKY_LOCATION;
  const int cd_loop_uv_offset = shared ? bpy_bm_uv_layer_offset_or_error(bm, error_prefix) : -1;
  if (shared && (cd_loop_uv_offset == -1)) {
    return nullptr;
  }

  Py_ssize_t vert_array_num = 0;
  Py_ssize_t edge_array_num = 0;
  Py_ssize_t face_array_num = 0;
  BMVert **vert_array = nullptr;
  BMEdge **edge_array = nullptr;
  BMFace **face_array = nullptr;

  bool ok = true;
  if (ok && py_verts) {
    BMesh *bm_test = nullptr;
    if (!(vert_array = BPy_BMVert_PySeq_As_Array(
              &bm_test, py_verts, 0, PY_SSIZE_T_MAX, &vert_array_num, true, true, error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }
  if (ok && py_edges) {
    BMesh *bm_test = nullptr;
    if (!(edge_array = BPy_BMEdge_PySeq_As_Array(
              &bm_test, py_edges, 0, PY_SSIZE_T_MAX, &edge_array_num, true, true, error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }
  if (ok && py_faces) {
    BMesh *bm_test = nullptr;
    if (!(face_array = BPy_BMFace_PySeq_As_Array(
              &bm_test, py_faces, 0, PY_SSIZE_T_MAX, &face_array_num, true, true, error_prefix)))
    {
      ok = false;
    }
    else if (bm_test && bpy_bm_check_bm_match_or_error(bm, bm_test, error_prefix) == -1) {
      ok = false;
    }
  }

  if (ok) {
    const BMUVSelectPickParams uv_pick_params = {
        /*cd_loop_uv_offset*/ cd_loop_uv_offset,
        /*shared*/ shared,
    };
    BM_mesh_uvselect_set_elem_from_mesh(bm,
                                        use_select,
                                        uv_pick_params,
                                        blender::Span(vert_array, vert_array_num),
                                        blender::Span(edge_array, edge_array_num),
                                        blender::Span(face_array, face_array_num));
  }

  PyMem_FREE(vert_array);
  PyMem_FREE(edge_array);
  PyMem_FREE(face_array);

  if (ok == false) {
    /* The error has been raised. */
    return nullptr;
  }
  Py_RETURN_NONE;
}

/** \} */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_normal_update_doc,
    ".. method:: normal_update()\n"
    "\n"
    "   Update normals of mesh faces and verts.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      The normal of any vertex where :attr:`is_wire` is True will be a zero vector.\n");
static PyObject *bpy_bmesh_normal_update(BPy_BMesh *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_mesh_normals_update(self->bm);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_transform_doc,
    ".. method:: transform(matrix, *, filter=None)\n"
    "\n"
    "   Transform the mesh (optionally filtering flagged data only).\n"
    "\n"
    "   :arg matrix: 4x4x transform matrix.\n"
    "   :type matrix: :class:`mathutils.Matrix`\n"
    "   :arg filter: Flag to filter vertices."
    ".\n"
    "   :type filter: set[Literal[" BPY_BM_HFLAG_ALL_STR "]]\n");
static PyObject *bpy_bmesh_transform(BPy_BMElem *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"matrix", "filter", nullptr};

  MatrixObject *mat;
  PyObject *filter = nullptr;
  int filter_flags = 0;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(
          args, kw, "O!|$O!:transform", (char **)kwlist, &matrix_Type, &mat, &PySet_Type, &filter))
  {
    return nullptr;
  }

  BMVert *eve;
  BMIter iter;
  void *mat_ptr;

  if (BaseMath_ReadCallback(mat) == -1) {
    return nullptr;
  }
  if (mat->col_num != 4 || mat->row_num != 4) {
    PyErr_SetString(PyExc_ValueError, "expected a 4x4 matrix");
    return nullptr;
  }

  if (filter != nullptr &&
      PyC_FlagSet_ToBitfield(bpy_bm_hflag_all_flags, filter, &filter_flags, "bm.transform") == -1)
  {
    return nullptr;
  }

  mat_ptr = mat->matrix;

  if (!filter_flags) {
    BM_ITER_MESH (eve, &iter, self->bm, BM_VERTS_OF_MESH) {
      mul_m4_v3((float (*)[4])mat_ptr, eve->co);
    }
  }
  else {
    const char filter_flags_ch = char(filter_flags);
    BM_ITER_MESH (eve, &iter, self->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(eve, filter_flags_ch)) {
        mul_m4_v3((float (*)[4])mat_ptr, eve->co);
      }
    }
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_calc_volume_doc,
    ".. method:: calc_volume(*, signed=False)\n"
    "\n"
    "   Calculate mesh volume based on face normals.\n"
    "\n"
    "   :arg signed: when signed is true, negative values may be returned.\n"
    "   :type signed: bool\n"
    "   :return: The volume of the mesh.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmesh_calc_volume(BPy_BMElem *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"signed", nullptr};
  PyObject *is_signed = Py_False;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(
          args, kw, "|$O!:calc_volume", (char **)kwlist, &PyBool_Type, &is_signed))
  {
    return nullptr;
  }

  return PyFloat_FromDouble(BM_mesh_calc_volume(self->bm, is_signed != Py_False));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_calc_loop_triangles_doc,
    ".. method:: calc_loop_triangles()\n"
    "\n"
    "   Calculate triangle tessellation from quads/ngons.\n"
    "\n"
    "   :return: The triangulated faces.\n"
    "   :rtype: list[tuple[:class:`bmesh.types.BMLoop`, "
    ":class:`bmesh.types.BMLoop`, "
    ":class:`bmesh.types.BMLoop`]]\n");
static PyObject *bpy_bmesh_calc_loop_triangles(BPy_BMElem *self)
{
  int corner_tris_tot;

  PyObject *ret;

  BPY_BM_CHECK_OBJ(self);

  BMesh *bm = self->bm;

  corner_tris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  blender::Array<std::array<BMLoop *, 3>> corner_tris(corner_tris_tot);
  BM_mesh_calc_tessellation(bm, corner_tris);

  ret = PyList_New(corner_tris_tot);
  for (int i = 0; i < corner_tris_tot; i++) {
    PyList_SET_ITEM(ret, i, BPy_BMLoop_Array_As_Tuple(bm, corner_tris[i].data(), 3));
  }

  return ret;
}

/* Elem
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_select_set_doc,
    ".. method:: select_set(select)\n"
    "\n"
    "   Set the selection.\n"
    "   This is different from the *select* attribute because it updates the selection "
    "state of associated geometry.\n"
    "\n"
    "   :arg select: Select or de-select.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      This only flushes down, so selecting a face will select all its "
    "vertices but de-selecting a vertex "
    "      won't de-select all the faces that use it, before finishing with a mesh "
    "typically flushing is still needed.\n");
static PyObject *bpy_bm_elem_select_set(BPy_BMElem *self, PyObject *value)
{
  int param;

  BPY_BM_CHECK_OBJ(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }

  BM_elem_select_set(self->bm, self->ele, param);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_hide_set_doc,
    ".. method:: hide_set(hide)\n"
    "\n"
    "   Set the hide state.\n"
    "   This is different from the *hide* attribute because it updates the selection and "
    "hide state of associated geometry.\n"
    "\n"
    "   :arg hide: Hidden or visible.\n"
    "   :type hide: bool\n");
static PyObject *bpy_bm_elem_hide_set(BPy_BMElem *self, PyObject *value)
{
  int param;

  BPY_BM_CHECK_OBJ(self);

  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }

  BM_elem_hide_set(self->bm, self->ele, param);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_elem_copy_from_doc,
    ".. method:: copy_from(other)\n"
    "\n"
    "   Copy values from another element of matching type.\n");
static PyObject *bpy_bm_elem_copy_from(BPy_BMElem *self, BPy_BMElem *value)
{
  BPY_BM_CHECK_OBJ(self);

  if (Py_TYPE(self) != Py_TYPE(value)) {
    PyErr_Format(PyExc_TypeError,
                 "expected element of type '%.200s' not '%.200s'",
                 Py_TYPE(self)->tp_name,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  if (value->ele != self->ele) {
    switch (self->ele->head.htype) {
      case BM_VERT: {
        const BMCustomDataCopyMap cd_vert_map = CustomData_bmesh_copy_map_calc(
            value->bm->vdata, self->bm->vdata, CD_MASK_BM_ELEM_PYPTR);
        BM_elem_attrs_copy(self->bm,
                           cd_vert_map,
                           reinterpret_cast<const BMVert *>(value->ele),
                           reinterpret_cast<BMVert *>(self->ele));
        break;
      }
      case BM_EDGE: {
        const BMCustomDataCopyMap cd_edge_map = CustomData_bmesh_copy_map_calc(
            value->bm->edata, self->bm->edata, CD_MASK_BM_ELEM_PYPTR);
        BM_elem_attrs_copy(self->bm,
                           cd_edge_map,
                           reinterpret_cast<const BMEdge *>(value->ele),
                           reinterpret_cast<BMEdge *>(self->ele));
        break;
      }
      case BM_FACE: {
        const BMCustomDataCopyMap cd_face_map = CustomData_bmesh_copy_map_calc(
            value->bm->pdata, self->bm->pdata, CD_MASK_BM_ELEM_PYPTR);
        BM_elem_attrs_copy(self->bm,
                           cd_face_map,
                           reinterpret_cast<const BMFace *>(value->ele),
                           reinterpret_cast<BMFace *>(self->ele));
        break;
      }
      case BM_LOOP: {
        const BMCustomDataCopyMap cd_loop_map = CustomData_bmesh_copy_map_calc(
            value->bm->ldata, self->bm->ldata, CD_MASK_BM_ELEM_PYPTR);
        BM_elem_attrs_copy(self->bm,
                           cd_loop_map,
                           reinterpret_cast<const BMLoop *>(value->ele),
                           reinterpret_cast<BMLoop *>(self->ele));
        break;
      }
    }
  }

  Py_RETURN_NONE;
}

/* Vert
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_copy_from_vert_interp_doc,
    ".. method:: copy_from_vert_interp(vert_pair, fac)\n"
    "\n"
    "   Interpolate the customdata from a vert between 2 other verts.\n"
    "\n"
    "   :arg vert_pair: The verts between which to interpolate data from.\n"
    "   :type vert_pair: Sequence[:class:`bmesh.types.BMVert`]\n"
    "   :type fac: float\n");
static PyObject *bpy_bmvert_copy_from_vert_interp(BPy_BMVert *self, PyObject *args)
{
  const char *error_prefix = "BMVert.copy_from_vert_interp(...)";
  PyObject *vert_seq;
  float fac;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "Of:BMVert.copy_from_vert_interp", &vert_seq, &fac)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  Py_ssize_t vert_seq_num; /* Always 2. */
  BMVert **vert_array = BPy_BMVert_PySeq_As_Array(
      &bm, vert_seq, 2, 2, &vert_seq_num, true, true, error_prefix);

  if (vert_array == nullptr) {
    return nullptr;
  }

  BM_data_interp_from_verts(bm, vert_array[0], vert_array[1], self->v, clamp_f(fac, 0.0f, 1.0f));

  PyMem_FREE(vert_array);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_copy_from_face_interp_doc,
    ".. method:: copy_from_face_interp(face)\n"
    "\n"
    "   Interpolate the customdata from a face onto this loop (the loops vert should "
    "overlap the face).\n"
    "\n"
    "   :arg face: The face to interpolate data from.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmvert_copy_from_face_interp(BPy_BMVert *self, PyObject *args)
{
  const char *error_prefix = "copy_from_face_interp(...)";
  BPy_BMFace *py_face = nullptr;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O!:BMVert.copy_from_face_interp", &BPy_BMFace_Type, &py_face)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, py_face);

  BM_vert_interp_from_face(bm, self->v, py_face->f);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_calc_edge_angle_doc,
    ".. method:: calc_edge_angle(fallback=None)\n"
    "\n"
    "   Return the angle between this vert's two connected edges.\n"
    "\n"
    "   :arg fallback: return this when the vert doesn't have 2 edges\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: Angle between edges in radians.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmvert_calc_edge_angle(BPy_BMVert *self, PyObject *args)
{
  const float angle_invalid = -1.0f;
  float angle;
  PyObject *fallback = nullptr;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|O:calc_edge_angle", &fallback)) {
    return nullptr;
  }

  angle = BM_vert_calc_edge_angle_ex(self->v, angle_invalid);

  if (angle == angle_invalid) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "BMVert.calc_edge_angle(): "
                    "vert must connect to exactly 2 edges");
    return nullptr;
  }

  return PyFloat_FromDouble(angle);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_calc_shell_factor_doc,
    ".. method:: calc_shell_factor()\n"
    "\n"
    "   Return a multiplier calculated based on the sharpness of the vertex.\n"
    "   Where a flat surface gives 1.0, and higher values sharper edges.\n"
    "   This is used to maintain shell thickness when offsetting verts along their normals.\n"
    "\n"
    "   :return: offset multiplier\n"
    "   :rtype: float\n");
static PyObject *bpy_bmvert_calc_shell_factor(BPy_BMVert *self)
{
  BPY_BM_CHECK_OBJ(self);
  return PyFloat_FromDouble(BM_vert_calc_shell_factor(self->v));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_normal_update_doc,
    ".. method:: normal_update()\n"
    "\n"
    "   Update vertex normal.\n"
    "   This does not update the normals of adjoining faces.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      The vertex normal will be a zero vector if vertex :attr:`is_wire` is True.\n");
static PyObject *bpy_bmvert_normal_update(BPy_BMVert *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_vert_normal_update(self->v);

  Py_RETURN_NONE;
}

/* Edge
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_calc_length_doc,
    ".. method:: calc_length()\n"
    "\n"
    "   :return: The length between both verts.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmedge_calc_length(BPy_BMEdge *self)
{
  BPY_BM_CHECK_OBJ(self);
  return PyFloat_FromDouble(len_v3v3(self->e->v1->co, self->e->v2->co));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_calc_face_angle_doc,
    ".. method:: calc_face_angle(fallback=None)\n"
    "\n"
    "   :arg fallback: return this when the edge doesn't have 2 faces\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: The angle between 2 connected faces in radians.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmedge_calc_face_angle(BPy_BMEdge *self, PyObject *args)
{
  const float angle_invalid = -1.0f;
  float angle;
  PyObject *fallback = nullptr;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|O:calc_face_angle", &fallback)) {
    return nullptr;
  }

  angle = BM_edge_calc_face_angle_ex(self->e, angle_invalid);

  if (angle == angle_invalid) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "BMEdge.calc_face_angle(): "
                    "edge doesn't use 2 faces");
    return nullptr;
  }

  return PyFloat_FromDouble(angle);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_calc_face_angle_signed_doc,
    ".. method:: calc_face_angle_signed(fallback=None)\n"
    "\n"
    "   :arg fallback: return this when the edge doesn't have 2 faces\n"
    "      (instead of raising a :exc:`ValueError`).\n"
    "   :type fallback: Any\n"
    "   :return: The angle between 2 connected faces in radians (negative for concave join).\n"
    "   :rtype: float\n");
static PyObject *bpy_bmedge_calc_face_angle_signed(BPy_BMEdge *self, PyObject *args)
{
  const float angle_invalid = -FLT_MAX;
  float angle;
  PyObject *fallback = nullptr;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|O:calc_face_angle_signed", &fallback)) {
    return nullptr;
  }

  angle = BM_edge_calc_face_angle_signed_ex(self->e, angle_invalid);

  if (angle == angle_invalid) {
    /* avoid exception */
    if (fallback) {
      Py_INCREF(fallback);
      return fallback;
    }

    PyErr_SetString(PyExc_ValueError,
                    "BMEdge.calc_face_angle_signed(): "
                    "edge doesn't use 2 faces");
    return nullptr;
  }

  return PyFloat_FromDouble(angle);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_calc_tangent_doc,
    ".. method:: calc_tangent(loop)\n"
    "\n"
    "   Return the tangent at this edge relative to a face (pointing inward into the face).\n"
    "   This uses the face normal for calculation.\n"
    "\n"
    "   :arg loop: The loop used for tangent calculation.\n"
    "   :type loop: :class:`bmesh.types.BMLoop`\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmedge_calc_tangent(BPy_BMEdge *self, PyObject *args)
{
  BPy_BMLoop *py_loop;
  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O!:BMEdge.calc_face_tangent", &BPy_BMLoop_Type, &py_loop)) {
    return nullptr;
  }

  float vec[3];
  BPY_BM_CHECK_OBJ(py_loop);
  /* no need to check if they are from the same mesh or even connected */
  BM_edge_calc_face_tangent(self->e, py_loop->l, vec);
  return Vector_CreatePyObject(vec, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_other_vert_doc,
    ".. method:: other_vert(vert)\n"
    "\n"
    "   Return the other vertex on this edge or None if the vertex is not used by this edge.\n"
    "\n"
    "   :arg vert: a vert in this edge.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :return: The edges other vert.\n"
    "   :rtype: :class:`bmesh.types.BMVert` | None\n");
static PyObject *bpy_bmedge_other_vert(BPy_BMEdge *self, BPy_BMVert *value)
{
  const char *error_prefix = "BMEdge.other_vert(...)";
  BMVert *other;
  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMVert_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "%s: BMVert expected, not '%.200s'",
                 error_prefix,
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_SOURCE_OBJ(self->bm, error_prefix, value);

  other = BM_edge_other_vert(self->e, value->v);

  if (other) {
    return BPy_BMVert_CreatePyObject(self->bm, other);
  }

  /* could raise an exception here */
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_normal_update_doc,
    ".. method:: normal_update()\n"
    "\n"
    "   Update normals of all connected faces and the edge verts.\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      The normal of edge vertex will be a zero vector if vertex :attr:`is_wire` is True.\n");
static PyObject *bpy_bmedge_normal_update(BPy_BMEdge *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_edge_normals_update(self->e);

  Py_RETURN_NONE;
}

/* Face
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_copy_from_face_interp_doc,
    ".. method:: copy_from_face_interp(face, vert=True)\n"
    "\n"
    "   Interpolate the customdata from another face onto this one (faces should overlap).\n"
    "\n"
    "   :arg face: The face to interpolate data from.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg vert: When True, also copy vertex data.\n"
    "   :type vert: bool\n");
static PyObject *bpy_bmface_copy_from_face_interp(BPy_BMFace *self, PyObject *args)
{
  const char *error_prefix = "BMFace.copy_from_face_interp(...)";
  BPy_BMFace *py_face = nullptr;
  bool do_vertex = true;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args,
                        "O!|O&:BMFace.copy_from_face_interp",
                        &BPy_BMFace_Type,
                        &py_face,
                        PyC_ParseBool,
                        &do_vertex))
  {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, py_face);

  BM_face_interp_from_face(bm, self->f, py_face->f, do_vertex);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_copy_doc,
    ".. method:: copy(*, verts=True, edges=True)\n"
    "\n"
    "   Make a copy of this face.\n"
    "\n"
    "   :arg verts: When set, the faces verts will be duplicated too.\n"
    "   :type verts: bool\n"
    "   :arg edges: When set, the faces edges will be duplicated too.\n"
    "   :type edges: bool\n"
    "   :return: The newly created face.\n"
    "   :rtype: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmface_copy(BPy_BMFace *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"verts", "edges", nullptr};

  BMesh *bm = self->bm;
  bool do_verts = true;
  bool do_edges = true;

  BMFace *f_cpy;
  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "|$O&O&:BMFace.copy",
                                   (char **)kwlist,
                                   PyC_ParseBool,
                                   &do_verts,
                                   PyC_ParseBool,
                                   &do_edges))
  {
    return nullptr;
  }

  f_cpy = BM_face_copy(bm, self->f, do_verts, do_edges);

  if (f_cpy) {
    return BPy_BMFace_CreatePyObject(bm, f_cpy);
  }

  PyErr_SetString(PyExc_ValueError, "BMFace.copy(): couldn't create the new face, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_uv_select_set_doc,
    ".. method:: uv_select_set(select)\n"
    "\n"
    "   Select the face.\n"
    "\n"
    "   :arg select: Select or de-select.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Currently this only flushes down, so selecting a face will select all its "
    "vertices but de-selecting a vertex "
    "      won't de-select all the faces that use it, before finishing with a mesh "
    "typically flushing is still needed.\n");
static PyObject *bpy_bmface_uv_select_set(BPy_BMFace *self, PyObject *value)
{
  BMesh *bm = self->bm;
  BPY_BM_CHECK_OBJ(self);
  int param;
  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }
  BM_face_uvselect_set(bm, self->f, param);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_area_doc,
    ".. method:: calc_area()\n"
    "\n"
    "   Return the area of the face.\n"
    "\n"
    "   :return: Return the area of the face.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmface_calc_area(BPy_BMFace *self)
{
  BPY_BM_CHECK_OBJ(self);
  return PyFloat_FromDouble(BM_face_calc_area(self->f));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_perimeter_doc,
    ".. method:: calc_perimeter()\n"
    "\n"
    "   Return the perimeter of the face.\n"
    "\n"
    "   :return: Return the perimeter of the face.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmface_calc_perimeter(BPy_BMFace *self)
{
  BPY_BM_CHECK_OBJ(self);
  return PyFloat_FromDouble(BM_face_calc_perimeter(self->f));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_tangent_edge_doc,
    ".. method:: calc_tangent_edge()\n"
    "\n"
    "   Return face tangent based on longest edge.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_tangent_edge(BPy_BMFace *self)
{
  float tangent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_tangent_from_edge(self->f, tangent);
  return Vector_CreatePyObject(tangent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_tangent_edge_pair_doc,
    ".. method:: calc_tangent_edge_pair()\n"
    "\n"
    "   Return face tangent based on the two longest disconnected edges.\n"
    "\n"
    "   - Tris: Use the edge pair with the most similar lengths.\n"
    "   - Quads: Use the longest edge pair.\n"
    "   - NGons: Use the two longest disconnected edges.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_tangent_edge_pair(BPy_BMFace *self)
{
  float tangent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_tangent_from_edge_pair(self->f, tangent);
  return Vector_CreatePyObject(tangent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_tangent_edge_diagonal_doc,
    ".. method:: calc_tangent_edge_diagonal()\n"
    "\n"
    "   Return face tangent based on the edge farthest from any vertex.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_tangent_edge_diagonal(BPy_BMFace *self)
{
  float tangent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_tangent_from_edge_diagonal(self->f, tangent);
  return Vector_CreatePyObject(tangent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_tangent_vert_diagonal_doc,
    ".. method:: calc_tangent_vert_diagonal()\n"
    "\n"
    "   Return face tangent based on the two most distant vertices.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_tangent_vert_diagonal(BPy_BMFace *self)
{
  float tangent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_tangent_from_vert_diagonal(self->f, tangent);
  return Vector_CreatePyObject(tangent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_center_median_doc,
    ".. method:: calc_center_median()\n"
    "\n"
    "   Return median center of the face.\n"
    "\n"
    "   :return: a 3D vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_center_mean(BPy_BMFace *self)
{
  float cent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_center_median(self->f, cent);
  return Vector_CreatePyObject(cent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_center_median_weighted_doc,
    ".. method:: calc_center_median_weighted()\n"
    "\n"
    "   Return median center of the face weighted by edge lengths.\n"
    "\n"
    "   :return: a 3D vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_center_median_weighted(BPy_BMFace *self)
{
  float cent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_center_median_weighted(self->f, cent);
  return Vector_CreatePyObject(cent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_calc_center_bounds_doc,
    ".. method:: calc_center_bounds()\n"
    "\n"
    "   Return bounds center of the face.\n"
    "\n"
    "   :return: a 3D vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmface_calc_center_bounds(BPy_BMFace *self)
{
  float cent[3];

  BPY_BM_CHECK_OBJ(self);
  BM_face_calc_center_bounds(self->f, cent);
  return Vector_CreatePyObject(cent, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_normal_update_doc,
    ".. method:: normal_update()\n"
    "\n"
    "   Update face normal based on the positions of the face verts.\n"
    "   This does not update the normals of face verts.\n");
static PyObject *bpy_bmface_normal_update(BPy_BMFace *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_face_normal_update(self->f);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_normal_flip_doc,
    ".. method:: normal_flip()\n"
    "\n"
    "   Reverses winding of a face, which flips its normal.\n");
static PyObject *bpy_bmface_normal_flip(BPy_BMFace *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_face_normal_flip(self->bm, self->f);

  Py_RETURN_NONE;
}

/* Loop
 * ---- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_copy_from_face_interp_doc,
    ".. method:: copy_from_face_interp(face, vert=True, multires=True)\n"
    "\n"
    "   Interpolate the customdata from a face onto this loop (the loops vert should "
    "overlap the face).\n"
    "\n"
    "   :arg face: The face to interpolate data from.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg vert: When enabled, interpolate the loops vertex data (optional).\n"
    "   :type vert: bool\n"
    "   :arg multires: When enabled, interpolate the loops multires data (optional).\n"
    "   :type multires: bool\n");
static PyObject *bpy_bmloop_copy_from_face_interp(BPy_BMLoop *self, PyObject *args)
{
  const char *error_prefix = "BMLoop.copy_from_face_interp(face)";
  BPy_BMFace *py_face = nullptr;
  bool do_vertex = true;
  bool do_multires = true;

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args,
                        "O!|O&O&:BMLoop.copy_from_face_interp",
                        &BPy_BMFace_Type,
                        &py_face,
                        PyC_ParseBool,
                        &do_vertex,
                        PyC_ParseBool,
                        &do_multires))
  {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, py_face);

  BM_loop_interp_from_face(bm, self->l, py_face->f, do_vertex, do_multires);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_uv_select_vert_set_doc,
    ".. method:: uv_select_vert_set(select)\n"
    "\n"
    "   Select the UV vertex.\n"
    "\n"
    "   :arg select: Select or de-select.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Currently this only flushes down, so selecting an edge will select all its "
    "vertices but de-selecting a vertex "
    "      won't de-select the edges & faces that use it, before finishing with a mesh "
    "typically flushing with :class:`bmesh.types.BMesh.uv_select_flush_mode` is still needed.\n");
static PyObject *bpy_bmloop_uv_select_vert_set(BPy_BMLoop *self, PyObject *value)
{
  BMesh *bm = self->bm;
  BPY_BM_CHECK_OBJ(self);
  int param;
  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }

  /* There is no flushing version of this function. */
  BM_loop_vert_uvselect_set_noflush(bm, self->l, param);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_uv_select_edge_set_doc,
    ".. method:: uv_select_edge_set(select)\n"
    "\n"
    "   Set the UV edge selection state.\n"
    "\n"
    "   :arg select: Select or de-select.\n"
    "   :type select: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      This only flushes down, so selecting an edge will select all its "
    "vertices but de-selecting a vertex "
    "won't de-select the faces that use it, before finishing with a mesh "
    "typically flushing with :class:`bmesh.types.BMesh.uv_select_flush_mode` is still needed.\n");
static PyObject *bpy_bmloop_uv_select_edge_set(BPy_BMLoop *self, PyObject *value)
{
  BMesh *bm = self->bm;
  BPY_BM_CHECK_OBJ(self);
  int param;
  if ((param = PyC_Long_AsBool(value)) == -1) {
    return nullptr;
  }
  BM_loop_edge_uvselect_set(bm, self->l, param);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_calc_angle_doc,
    ".. method:: calc_angle()\n"
    "\n"
    "   Return the angle at this loops corner of the face.\n"
    "   This is calculated so sharper corners give lower angles.\n"
    "\n"
    "   :return: The angle in radians.\n"
    "   :rtype: float\n");
static PyObject *bpy_bmloop_calc_angle(BPy_BMLoop *self)
{
  BPY_BM_CHECK_OBJ(self);
  return PyFloat_FromDouble(BM_loop_calc_face_angle(self->l));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_calc_normal_doc,
    ".. method:: calc_normal()\n"
    "\n"
    "   Return normal at this loops corner of the face.\n"
    "   Falls back to the face normal for straight lines.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmloop_calc_normal(BPy_BMLoop *self)
{
  float vec[3];
  BPY_BM_CHECK_OBJ(self);
  BM_loop_calc_face_normal(self->l, vec);
  return Vector_CreatePyObject(vec, 3, nullptr);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_calc_tangent_doc,
    ".. method:: calc_tangent()\n"
    "\n"
    "   Return the tangent at this loops corner of the face (pointing inward into the face).\n"
    "   Falls back to the face normal for straight lines.\n"
    "\n"
    "   :return: a normalized vector.\n"
    "   :rtype: :class:`mathutils.Vector`\n");
static PyObject *bpy_bmloop_calc_tangent(BPy_BMLoop *self)
{
  float vec[3];
  BPY_BM_CHECK_OBJ(self);
  BM_loop_calc_face_tangent(self->l, vec);
  return Vector_CreatePyObject(vec, 3, nullptr);
}

/* Vert Seq
 * -------- */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertseq_new_doc,
    ".. method:: new(co=(0.0, 0.0, 0.0), example=None)\n"
    "\n"
    "   Create a new vertex.\n"
    "\n"
    "   :arg co: The initial location of the vertex (optional argument).\n"
    "   :type co: float triplet\n"
    "   :arg example: Existing vert to initialize settings.\n"
    "   :type example: :class:`bmesh.types.BMVert`\n"
    "   :return: The newly created vertex.\n"
    "   :rtype: :class:`bmesh.types.BMVert`\n");
static PyObject *bpy_bmvertseq_new(BPy_BMElemSeq *self, PyObject *args)
{
  PyObject *py_co = nullptr;
  BPy_BMVert *py_vert_example = nullptr; /* optional */

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "|OO!:verts.new", &py_co, &BPy_BMVert_Type, &py_vert_example)) {
    return nullptr;
  }

  BMesh *bm = self->bm;
  BMVert *v;
  float co[3] = {0.0f, 0.0f, 0.0f};

  if (py_vert_example) {
    BPY_BM_CHECK_OBJ(py_vert_example);
  }

  if (py_co && mathutils_array_parse(co, 3, 3, py_co, "verts.new(co)") == -1) {
    return nullptr;
  }

  v = BM_vert_create(bm, co, nullptr, BM_CREATE_NOP);

  if (v == nullptr) {
    PyErr_SetString(PyExc_ValueError,
                    "faces.new(verts): couldn't create the new face, internal error");
    return nullptr;
  }

  if (py_vert_example) {
    if (py_vert_example->bm == bm) {
      BM_elem_attrs_copy(bm, py_vert_example->v, v);
    }
    else {
      const BMCustomDataCopyMap cd_vert_map = CustomData_bmesh_copy_map_calc(
          py_vert_example->bm->vdata, bm->vdata);
      BM_elem_attrs_copy(bm, cd_vert_map, py_vert_example->v, v);
    }
  }

  return BPy_BMVert_CreatePyObject(bm, v);
}

/* Edge Seq
 * -------- */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedgeseq_new_doc,
    ".. method:: new(verts, example=None)\n"
    "\n"
    "   Create a new edge from a given pair of verts.\n"
    "\n"
    "   :arg verts: Vertex pair.\n"
    "   :type verts: Sequence[:class:`bmesh.types.BMVert`]\n"
    "   :arg example: Existing edge to initialize settings (optional argument).\n"
    "   :type example: :class:`bmesh.types.BMEdge`\n"
    "   :return: The newly created edge.\n"
    "   :rtype: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bmedgeseq_new(BPy_BMElemSeq *self, PyObject *args)
{
  const char *error_prefix = "edges.new(...)";
  PyObject *vert_seq;
  BPy_BMEdge *py_edge_example = nullptr; /* optional */

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O!:edges.new", &vert_seq, &BPy_BMEdge_Type, &py_edge_example)) {
    return nullptr;
  }

  BMesh *bm = self->bm;
  BMEdge *e;
  PyObject *ret = nullptr;

  if (py_edge_example) {
    BPY_BM_CHECK_OBJ(py_edge_example);
  }

  Py_ssize_t vert_seq_num; /* Always 2. */
  BMVert **vert_array = BPy_BMVert_PySeq_As_Array(
      &bm, vert_seq, 2, 2, &vert_seq_num, true, true, error_prefix);
  if (vert_array == nullptr) {
    return nullptr;
  }

  if (BM_edge_exists(vert_array[0], vert_array[1])) {
    PyErr_SetString(PyExc_ValueError, "edges.new(): this edge exists");
    goto cleanup;
  }

  e = BM_edge_create(bm, vert_array[0], vert_array[1], nullptr, BM_CREATE_NOP);

  if (e == nullptr) {
    PyErr_SetString(PyExc_ValueError,
                    "faces.new(verts): couldn't create the new face, internal error");
    goto cleanup;
  }

  if (py_edge_example) {
    if (py_edge_example->bm == bm) {
      BM_elem_attrs_copy(bm, py_edge_example->e, e);
    }
    else {
      const BMCustomDataCopyMap cd_edge_map = CustomData_bmesh_copy_map_calc(
          py_edge_example->bm->edata, bm->edata);
      BM_elem_attrs_copy(bm, cd_edge_map, py_edge_example->e, e);
    }
  }

  ret = BPy_BMEdge_CreatePyObject(bm, e);

cleanup:
  if (vert_array) {
    PyMem_FREE(vert_array);
  }
  return ret;
}

/* Face Seq
 * -------- */
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmfaceseq_new_doc,
    ".. method:: new(verts, example=None)\n"
    "\n"
    "   Create a new face from a given set of verts.\n"
    "\n"
    "   :arg verts: Sequence of 3 or more verts.\n"
    "   :type verts: Sequence[:class:`bmesh.types.BMVert`]\n"
    "   :arg example: Existing face to initialize settings (optional argument).\n"
    "   :type example: :class:`bmesh.types.BMFace`\n"
    "   :return: The newly created face.\n"
    "   :rtype: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmfaceseq_new(BPy_BMElemSeq *self, PyObject *args)
{
  const char *error_prefix = "faces.new(...)";
  PyObject *vert_seq;
  BPy_BMFace *py_face_example = nullptr; /* optional */

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O!:faces.new", &vert_seq, &BPy_BMFace_Type, &py_face_example)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  PyObject *ret = nullptr;

  BMFace *f_new;

  if (py_face_example) {
    BPY_BM_CHECK_OBJ(py_face_example);
  }

  Py_ssize_t vert_seq_num;
  BMVert **vert_array = BPy_BMVert_PySeq_As_Array(
      &bm, vert_seq, 3, PY_SSIZE_T_MAX, &vert_seq_num, true, true, error_prefix);
  if (vert_array == nullptr) {
    return nullptr;
  }

  /* check if the face exists */
  if (BM_face_exists(vert_array, vert_seq_num) != nullptr) {
    PyErr_Format(PyExc_ValueError, "%s: face already exists", error_prefix);
    goto cleanup;
  }

  /* Go ahead and make the face!
   * --------------------------- */

  f_new = BM_face_create_verts(bm,
                               vert_array,
                               vert_seq_num,
                               py_face_example ? py_face_example->f : nullptr,
                               BM_CREATE_NOP,
                               true);

  if (UNLIKELY(f_new == nullptr)) {
    PyErr_Format(
        PyExc_ValueError, "%s: couldn't create the new face, internal error", error_prefix);
    goto cleanup;
  }

  ret = BPy_BMFace_CreatePyObject(bm, f_new);

/* pass through */
cleanup:
  if (vert_array) {
    PyMem_FREE(vert_array);
  }
  return ret;
}

/* Elem Seq
 * -------- */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvertseq_remove_doc,
    ".. method:: remove(vert)\n"
    "\n"
    "   Remove a vert.\n"
    "\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n");
static PyObject *bpy_bmvertseq_remove(BPy_BMElemSeq *self, BPy_BMVert *value)
{
  const char *error_prefix = "verts.remove(vert)";
  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMVert_Check(value)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, value);

  BM_vert_kill(bm, value->v);
  bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedgeseq_remove_doc,
    ".. method:: remove(edge)\n"
    "\n"
    "   Remove an edge.\n"
    "\n"
    "   :type edge: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bmedgeseq_remove(BPy_BMElemSeq *self, BPy_BMEdge *value)
{
  const char *error_prefix = "edges.remove(...)";
  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMEdge_Check(value)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, value);

  BM_edge_kill(bm, value->e);
  bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmfaceseq_remove_doc,
    ".. method:: remove(face)\n"
    "\n"
    "   Remove a face.\n"
    "\n"
    "   :type face: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmfaceseq_remove(BPy_BMElemSeq *self, BPy_BMFace *value)
{
  const char *error_prefix = "faces.remove(...)";
  BPY_BM_CHECK_OBJ(self);

  if (!BPy_BMFace_Check(value)) {
    return nullptr;
  }

  BMesh *bm = self->bm;

  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, value);

  BM_face_kill(bm, value->f);
  bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedgeseq_get__method_doc,
    ".. method:: get(verts, fallback=None)\n"
    "\n"
    "   Return an edge which uses the **verts** passed.\n"
    "\n"
    "   :arg verts: Sequence of verts.\n"
    "   :type verts: Sequence[:class:`bmesh.types.BMVert`]\n"
    "   :arg fallback: Return this value if nothing is found.\n"
    "   :return: The edge found or None\n"
    "   :rtype: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bmedgeseq_get__method(BPy_BMElemSeq *self, PyObject *args)
{
  const char *error_prefix = "edges.get(...)";
  PyObject *vert_seq;
  PyObject *fallback = Py_None; /* optional */

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O:edges.get", &vert_seq, &fallback)) {
    return nullptr;
  }

  BMesh *bm = self->bm;
  BMEdge *e;
  PyObject *ret = nullptr;

  Py_ssize_t vert_seq_num; /* Always 2. */
  BMVert **vert_array = BPy_BMVert_PySeq_As_Array(
      &bm, vert_seq, 2, 2, &vert_seq_num, true, true, error_prefix);

  if (vert_array == nullptr) {
    return nullptr;
  }

  if ((e = BM_edge_exists(vert_array[0], vert_array[1]))) {
    ret = BPy_BMEdge_CreatePyObject(bm, e);
  }
  else {
    ret = fallback;
    Py_INCREF(ret);
  }

  PyMem_FREE(vert_array);
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmfaceseq_get__method_doc,
    ".. method:: get(verts, fallback=None)\n"
    "\n"
    "   Return a face which uses the **verts** passed.\n"
    "\n"
    "   :arg verts: Sequence of verts.\n"
    "   :type verts: Sequence[:class:`bmesh.types.BMVert`]\n"
    "   :arg fallback: Return this value if nothing is found.\n"
    "   :return: The face found or None\n"
    "   :rtype: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bmfaceseq_get__method(BPy_BMElemSeq *self, PyObject *args)
{
  const char *error_prefix = "faces.get(...)";
  PyObject *vert_seq;
  PyObject *fallback = Py_None; /* optional */

  BPY_BM_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "O|O:faces.get", &vert_seq, &fallback)) {
    return nullptr;
  }

  BMesh *bm = self->bm;
  BMFace *f = nullptr;
  PyObject *ret = nullptr;

  Py_ssize_t vert_seq_num;
  BMVert **vert_array = BPy_BMVert_PySeq_As_Array(
      &bm, vert_seq, 1, PY_SSIZE_T_MAX, &vert_seq_num, true, true, error_prefix);

  if (vert_array == nullptr) {
    return nullptr;
  }

  f = BM_face_exists(vert_array, vert_seq_num);
  if (f != nullptr) {
    ret = BPy_BMFace_CreatePyObject(bm, f);
  }
  else {
    ret = fallback;
    Py_INCREF(ret);
  }

  PyMem_FREE(vert_array);
  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_index_update_doc,
    ".. method:: index_update()\n"
    "\n"
    "   Initialize the index values of this sequence.\n"
    "\n"
    "   This is the equivalent of looping over all elements and assigning the index values.\n"
    "\n"
    "   .. code-block:: python\n"
    "\n"
    "      for index, ele in enumerate(sequence):\n"
    "          ele.index = index\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Running this on sequences besides "
    ":class:`bmesh.types.BMesh.verts`, "
    ":class:`bmesh.types.BMesh.edges`, "
    ":class:`bmesh.types.BMesh.faces`\n"
    "      works but won't result in each element having a valid index, instead its order in the "
    "sequence will be set.\n");
static PyObject *bpy_bmelemseq_index_update(BPy_BMElemSeq *self)
{
  BMesh *bm = self->bm;

  BPY_BM_CHECK_OBJ(self);

  switch ((BMIterType)self->itype) {
    case BM_VERTS_OF_MESH:
      BM_mesh_elem_index_ensure(self->bm, BM_VERT);
      break;
    case BM_EDGES_OF_MESH:
      BM_mesh_elem_index_ensure(self->bm, BM_EDGE);
      break;
    case BM_FACES_OF_MESH:
      BM_mesh_elem_index_ensure(self->bm, BM_FACE);
      break;
    default: {
      BMIter iter;
      BMElem *ele;
      int index = 0;
      const char htype = bm_iter_itype_htype_map[self->itype];

      BM_ITER_BPY_BM_SEQ (ele, &iter, self) {
        BM_elem_index_set(ele, index); /* set_dirty! */
        index++;
      }

      /* since this isn't the normal vert/edge/face loops,
       * we're setting dirty values here. so tag as dirty. */
      bm->elem_index_dirty |= htype;

      break;
    }
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_ensure_lookup_table_doc,
    ".. method:: ensure_lookup_table()\n"
    "\n"
    "   Ensure internal data needed for int subscription is initialized with "
    "verts/edges/faces, eg ``bm.verts[index]``.\n"
    "\n"
    "   This needs to be called again after adding/removing data in this sequence.\n");
static PyObject *bpy_bmelemseq_ensure_lookup_table(BPy_BMElemSeq *self)
{
  BPY_BM_CHECK_OBJ(self);

  BM_mesh_elem_table_ensure(self->bm, bm_iter_itype_htype_map[self->itype]);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_sort_doc,
    ".. method:: sort(*, key=None, reverse=False)\n"
    "\n"
    "   Sort the elements of this sequence, using an optional custom sort key.\n"
    "   Indices of elements are not changed, :class:`bmesh.types.BMElemSeq.index_update` "
    "can be used for that.\n"
    "\n"
    "   :arg key: The key that sets the ordering of the elements.\n"
    "   :type key: Callable[["
    ":class:`bmesh.types.BMVert` | "
    ":class:`bmesh.types.BMEdge` | "
    ":class:`bmesh.types.BMFace`], int] | None\n"
    "   :arg reverse: Reverse the order of the elements\n"
    "   :type reverse: bool\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      When the 'key' argument is not provided, the elements are reordered following their "
    "current index value.\n"
    "      In particular this can be used by setting indices manually before calling this "
    "method.\n"
    "\n"
    "   .. warning::\n"
    "\n"
    "      Existing references to the N'th element, will continue to point the data at that "
    "index.\n");

/* Use a static variable here because there is the need to sort some array
 * doing comparisons on elements of another array, qsort_r would have been
 * wonderful to use here, but unfortunately it is not standard and it's not
 * portable across different platforms.
 *
 * If a portable alternative to qsort_r becomes available, remove this static
 * var hack!
 *
 * NOTE: the functions below assumes the keys array has been allocated and it
 * has enough elements to complete the task.
 */

static int bpy_bmelemseq_sort_cmp_by_keys_ascending(const void *index1_v,
                                                    const void *index2_v,
                                                    void *keys_v)
{
  const double *keys = static_cast<const double *>(keys_v);
  const int *index1 = (int *)index1_v;
  const int *index2 = (int *)index2_v;

  if (keys[*index1] < keys[*index2]) {
    return -1;
  }
  if (keys[*index1] > keys[*index2]) {
    return 1;
  }

  return 0;
}

static int bpy_bmelemseq_sort_cmp_by_keys_descending(const void *index1_v,
                                                     const void *index2_v,
                                                     void *keys_v)
{
  return -bpy_bmelemseq_sort_cmp_by_keys_ascending(index1_v, index2_v, keys_v);
}

static PyObject *bpy_bmelemseq_sort(BPy_BMElemSeq *self, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {"key", "reverse", nullptr};
  PyObject *keyfunc = nullptr; /* optional */
  bool do_reverse = false;     /* optional */

  const char htype = bm_iter_itype_htype_map[self->itype];
  int n_elem;

  BMIter iter;
  BMElem *ele;

  double *keys;
  int *elem_idx;
  uint *elem_map_idx;
  int (*elem_idx_compare_by_keys)(const void *, const void *, void *);

  uint *vert_idx = nullptr;
  uint *edge_idx = nullptr;
  uint *face_idx = nullptr;
  int i;

  BMesh *bm = self->bm;

  BPY_BM_CHECK_OBJ(self);

  if (args != nullptr) {
    if (!PyArg_ParseTupleAndKeywords(args,
                                     kw,
                                     "|$OO&:BMElemSeq.sort",
                                     (char **)kwlist,
                                     &keyfunc,
                                     PyC_ParseBool,
                                     &do_reverse))
    {
      return nullptr;
    }
    if (keyfunc == Py_None) {
      keyfunc = nullptr;
    }
  }

  if (keyfunc != nullptr && !PyCallable_Check(keyfunc)) {
    PyErr_SetString(PyExc_TypeError, "the 'key' argument is not a callable object");
    return nullptr;
  }

  n_elem = BM_mesh_elem_count(bm, htype);
  if (n_elem <= 1) {
    /* 0 or 1 elements: sorted already */
    Py_RETURN_NONE;
  }

  keys = static_cast<double *>(PyMem_MALLOC(sizeof(*keys) * n_elem));
  if (keys == nullptr) {
    PyErr_NoMemory();
    return nullptr;
  }

  i = 0;
  BM_ITER_BPY_BM_SEQ (ele, &iter, self) {
    if (keyfunc != nullptr) {
      PyObject *py_elem;
      PyObject *index;

      py_elem = BPy_BMElem_CreatePyObject(self->bm, (BMHeader *)ele);
      index = PyObject_CallFunctionObjArgs(keyfunc, py_elem, nullptr);
      Py_DECREF(py_elem);
      if (index == nullptr) {
        /* No need to set the exception here,
         * PyObject_CallFunctionObjArgs() does that */
        PyMem_FREE(keys);
        return nullptr;
      }

      if ((keys[i] = PyFloat_AsDouble(index)) == -1 && PyErr_Occurred()) {
        PyErr_SetString(PyExc_ValueError,
                        "the value returned by the 'key' function is not a number");
        Py_DECREF(index);
        PyMem_FREE(keys);
        return nullptr;
      }

      Py_DECREF(index);
    }
    else {
      /* If the 'key' function is not provided we sort
       * according to the current index values */
      keys[i] = ele->head.index;
    }

    i++;
  }

  elem_idx = static_cast<int *>(PyMem_MALLOC(sizeof(*elem_idx) * n_elem));
  if (elem_idx == nullptr) {
    PyErr_NoMemory();
    PyMem_FREE(keys);
    return nullptr;
  }

  /* Initialize the element index array */
  range_vn_i(elem_idx, n_elem, 0);

  /* Sort the index array according to the order of the 'keys' array */
  if (do_reverse) {
    elem_idx_compare_by_keys = bpy_bmelemseq_sort_cmp_by_keys_descending;
  }
  else {
    elem_idx_compare_by_keys = bpy_bmelemseq_sort_cmp_by_keys_ascending;
  }

  BLI_qsort_r(elem_idx, n_elem, sizeof(*elem_idx), elem_idx_compare_by_keys, keys);

  elem_map_idx = static_cast<uint *>(PyMem_MALLOC(sizeof(*elem_map_idx) * n_elem));
  if (elem_map_idx == nullptr) {
    PyErr_NoMemory();
    PyMem_FREE(elem_idx);
    PyMem_FREE(keys);
    return nullptr;
  }

  /* Initialize the map array
   *
   * We need to know the index such that if used as the new_index in
   * BM_mesh_remap() will give the order of the sorted keys like in
   * elem_idx */
  for (i = 0; i < n_elem; i++) {
    elem_map_idx[elem_idx[i]] = i;
  }

  switch ((BMIterType)self->itype) {
    case BM_VERTS_OF_MESH:
      vert_idx = elem_map_idx;
      break;
    case BM_EDGES_OF_MESH:
      edge_idx = elem_map_idx;
      break;
    case BM_FACES_OF_MESH:
      face_idx = elem_map_idx;
      break;
    default:
      PyErr_Format(PyExc_TypeError, "element type %d not supported", self->itype);
      PyMem_FREE(elem_map_idx);
      PyMem_FREE(elem_idx);
      PyMem_FREE(keys);
      return nullptr;
  }

  BM_mesh_remap(bm, vert_idx, edge_idx, face_idx);

  PyMem_FREE(elem_map_idx);
  PyMem_FREE(elem_idx);
  PyMem_FREE(keys);

  Py_RETURN_NONE;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_bmesh_methods[] = {
    /* utility */
    {"copy", (PyCFunction)bpy_bmesh_copy, METH_NOARGS, bpy_bmesh_copy_doc},
    {"clear", (PyCFunction)bpy_bmesh_clear, METH_NOARGS, bpy_bmesh_clear_doc},
    {"free", (PyCFunction)bpy_bmesh_free, METH_NOARGS, bpy_bmesh_free_doc},

    /* conversion */
    {"from_object",
     (PyCFunction)bpy_bmesh_from_object,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_from_object_doc},
    {"from_mesh",
     (PyCFunction)bpy_bmesh_from_mesh,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_from_mesh_doc},
    {"to_mesh", (PyCFunction)bpy_bmesh_to_mesh, METH_VARARGS, bpy_bmesh_to_mesh_doc},

    /* Mesh select methods. */
    {"select_flush_mode",
     (PyCFunction)bpy_bmesh_select_flush_mode,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_select_flush_mode_doc},
    {"select_flush", (PyCFunction)bpy_bmesh_select_flush, METH_O, bpy_bmesh_select_flush_doc},

    /* UV select methods. */
    {"uv_select_flush_mode",
     (PyCFunction)bpy_bmesh_uv_select_flush_mode,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_uv_select_flush_mode_doc},
    {"uv_select_flush",
     (PyCFunction)bpy_bmesh_uv_select_flush,
     METH_O,
     bpy_bmesh_uv_select_flush_doc},
    {"uv_select_flush_shared",
     (PyCFunction)bpy_bmesh_uv_select_flush_shared,
     METH_O,
     bpy_bmesh_uv_select_flush_shared_doc},

    {"uv_select_sync_from_mesh",
     (PyCFunction)bpy_bmesh_uv_select_sync_from_mesh,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_uv_select_sync_from_mesh_doc},
    {"uv_select_sync_to_mesh",
     (PyCFunction)bpy_bmesh_uv_select_sync_to_mesh,
     METH_NOARGS,
     bpy_bmesh_uv_select_sync_to_mesh_doc},
    {"uv_select_foreach_set",
     (PyCFunction)bpy_bmesh_uv_select_foreach_set,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_uv_select_foreach_set_doc},
    {"uv_select_foreach_set_from_mesh",
     (PyCFunction)bpy_bmesh_uv_select_foreach_set_from_mesh,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_uv_select_foreach_set_from_mesh_doc},

    /* meshdata */
    {"normal_update",
     (PyCFunction)bpy_bmesh_normal_update,
     METH_NOARGS,
     bpy_bmesh_normal_update_doc},
    {"transform",
     (PyCFunction)bpy_bmesh_transform,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_transform_doc},

    /* calculations */
    {"calc_volume",
     (PyCFunction)bpy_bmesh_calc_volume,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmesh_calc_volume_doc},
    {"calc_loop_triangles",
     (PyCFunction)bpy_bmesh_calc_loop_triangles,
     METH_NOARGS,
     bpy_bmesh_calc_loop_triangles_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmvert_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp",
     (PyCFunction)bpy_bmvert_copy_from_face_interp,
     METH_VARARGS,
     bpy_bmvert_copy_from_face_interp_doc},
    {"copy_from_vert_interp",
     (PyCFunction)bpy_bmvert_copy_from_vert_interp,
     METH_VARARGS,
     bpy_bmvert_copy_from_vert_interp_doc},

    {"calc_edge_angle",
     (PyCFunction)bpy_bmvert_calc_edge_angle,
     METH_VARARGS,
     bpy_bmvert_calc_edge_angle_doc},
    {"calc_shell_factor",
     (PyCFunction)bpy_bmvert_calc_shell_factor,
     METH_NOARGS,
     bpy_bmvert_calc_shell_factor_doc},

    {"normal_update",
     (PyCFunction)bpy_bmvert_normal_update,
     METH_NOARGS,
     bpy_bmvert_normal_update_doc},

    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmedge_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},

    {"other_vert", (PyCFunction)bpy_bmedge_other_vert, METH_O, bpy_bmedge_other_vert_doc},

    {"calc_length", (PyCFunction)bpy_bmedge_calc_length, METH_NOARGS, bpy_bmedge_calc_length_doc},
    {"calc_face_angle",
     (PyCFunction)bpy_bmedge_calc_face_angle,
     METH_VARARGS,
     bpy_bmedge_calc_face_angle_doc},
    {"calc_face_angle_signed",
     (PyCFunction)bpy_bmedge_calc_face_angle_signed,
     METH_VARARGS,
     bpy_bmedge_calc_face_angle_signed_doc},
    {"calc_tangent",
     (PyCFunction)bpy_bmedge_calc_tangent,
     METH_VARARGS,
     bpy_bmedge_calc_tangent_doc},

    {"normal_update",
     (PyCFunction)bpy_bmedge_normal_update,
     METH_NOARGS,
     bpy_bmedge_normal_update_doc},

    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmface_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},

    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp",
     (PyCFunction)bpy_bmface_copy_from_face_interp,
     METH_VARARGS,
     bpy_bmface_copy_from_face_interp_doc},

    {"copy", (PyCFunction)bpy_bmface_copy, METH_VARARGS | METH_KEYWORDS, bpy_bmface_copy_doc},

    {"uv_select_set", (PyCFunction)bpy_bmface_uv_select_set, METH_O, bpy_bmface_uv_select_set_doc},

    {"calc_area", (PyCFunction)bpy_bmface_calc_area, METH_NOARGS, bpy_bmface_calc_area_doc},
    {"calc_perimeter",
     (PyCFunction)bpy_bmface_calc_perimeter,
     METH_NOARGS,
     bpy_bmface_calc_perimeter_doc},
    {"calc_tangent_edge",
     (PyCFunction)bpy_bmface_calc_tangent_edge,
     METH_NOARGS,
     bpy_bmface_calc_tangent_edge_doc},
    {"calc_tangent_edge_pair",
     (PyCFunction)bpy_bmface_calc_tangent_edge_pair,
     METH_NOARGS,
     bpy_bmface_calc_tangent_edge_pair_doc},
    {"calc_tangent_edge_diagonal",
     (PyCFunction)bpy_bmface_calc_tangent_edge_diagonal,
     METH_NOARGS,
     bpy_bmface_calc_tangent_edge_diagonal_doc},
    {"calc_tangent_vert_diagonal",
     (PyCFunction)bpy_bmface_calc_tangent_vert_diagonal,
     METH_NOARGS,
     bpy_bmface_calc_tangent_vert_diagonal_doc},
    {"calc_center_median",
     (PyCFunction)bpy_bmface_calc_center_mean,
     METH_NOARGS,
     bpy_bmface_calc_center_median_doc},
    {"calc_center_median_weighted",
     (PyCFunction)bpy_bmface_calc_center_median_weighted,
     METH_NOARGS,
     bpy_bmface_calc_center_median_weighted_doc},
    {"calc_center_bounds",
     (PyCFunction)bpy_bmface_calc_center_bounds,
     METH_NOARGS,
     bpy_bmface_calc_center_bounds_doc},

    {"normal_update",
     (PyCFunction)bpy_bmface_normal_update,
     METH_NOARGS,
     bpy_bmface_normal_update_doc},
    {"normal_flip", (PyCFunction)bpy_bmface_normal_flip, METH_NOARGS, bpy_bmface_normal_flip_doc},

    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmloop_methods[] = {
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp",
     (PyCFunction)bpy_bmloop_copy_from_face_interp,
     METH_VARARGS,
     bpy_bmloop_copy_from_face_interp_doc},

    {"uv_select_vert_set",
     (PyCFunction)bpy_bmloop_uv_select_vert_set,
     METH_O,
     bpy_bmloop_uv_select_vert_set_doc},
    {"uv_select_edge_set",
     (PyCFunction)bpy_bmloop_uv_select_edge_set,
     METH_O,
     bpy_bmloop_uv_select_edge_set_doc},

    {"calc_angle", (PyCFunction)bpy_bmloop_calc_angle, METH_NOARGS, bpy_bmloop_calc_angle_doc},
    {"calc_normal", (PyCFunction)bpy_bmloop_calc_normal, METH_NOARGS, bpy_bmloop_calc_normal_doc},
    {"calc_tangent",
     (PyCFunction)bpy_bmloop_calc_tangent,
     METH_NOARGS,
     bpy_bmloop_calc_tangent_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmelemseq_methods[] = {
    /* odd function, initializes index values */
    {"index_update",
     (PyCFunction)bpy_bmelemseq_index_update,
     METH_NOARGS,
     bpy_bmelemseq_index_update_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmvertseq_methods[] = {
    {"new", (PyCFunction)bpy_bmvertseq_new, METH_VARARGS, bpy_bmvertseq_new_doc},
    {"remove", (PyCFunction)bpy_bmvertseq_remove, METH_O, bpy_bmvertseq_remove_doc},

    /* odd function, initializes index values */
    {"index_update",
     (PyCFunction)bpy_bmelemseq_index_update,
     METH_NOARGS,
     bpy_bmelemseq_index_update_doc},
    {"ensure_lookup_table",
     (PyCFunction)bpy_bmelemseq_ensure_lookup_table,
     METH_NOARGS,
     bpy_bmelemseq_ensure_lookup_table_doc},
    {"sort",
     (PyCFunction)bpy_bmelemseq_sort,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmelemseq_sort_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmedgeseq_methods[] = {
    {"new", (PyCFunction)bpy_bmedgeseq_new, METH_VARARGS, bpy_bmedgeseq_new_doc},
    {"remove", (PyCFunction)bpy_bmedgeseq_remove, METH_O, bpy_bmedgeseq_remove_doc},
    /* 'bpy_bmelemseq_get' for different purpose */
    {"get", (PyCFunction)bpy_bmedgeseq_get__method, METH_VARARGS, bpy_bmedgeseq_get__method_doc},

    /* odd function, initializes index values */
    {"index_update",
     (PyCFunction)bpy_bmelemseq_index_update,
     METH_NOARGS,
     bpy_bmelemseq_index_update_doc},
    {"ensure_lookup_table",
     (PyCFunction)bpy_bmelemseq_ensure_lookup_table,
     METH_NOARGS,
     bpy_bmelemseq_ensure_lookup_table_doc},
    {"sort",
     (PyCFunction)bpy_bmelemseq_sort,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmelemseq_sort_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmfaceseq_methods[] = {
    {"new", (PyCFunction)bpy_bmfaceseq_new, METH_VARARGS, bpy_bmfaceseq_new_doc},
    {"remove", (PyCFunction)bpy_bmfaceseq_remove, METH_O, bpy_bmfaceseq_remove_doc},
    /* 'bpy_bmelemseq_get' for different purpose */
    {"get", (PyCFunction)bpy_bmfaceseq_get__method, METH_VARARGS, bpy_bmfaceseq_get__method_doc},

    /* odd function, initializes index values */
    {"index_update",
     (PyCFunction)bpy_bmelemseq_index_update,
     METH_NOARGS,
     bpy_bmelemseq_index_update_doc},
    {"ensure_lookup_table",
     (PyCFunction)bpy_bmelemseq_ensure_lookup_table,
     METH_NOARGS,
     bpy_bmelemseq_ensure_lookup_table_doc},
    {"sort",
     (PyCFunction)bpy_bmelemseq_sort,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bmelemseq_sort_doc},
    {nullptr, nullptr, 0, nullptr},
};

static PyMethodDef bpy_bmloopseq_methods[] = {
    /* odd function, initializes index values */
    /* no: index_update() function since we can't iterate over loops */
    /* no: sort() function since we can't iterate over loops */
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

/* Sequences
 * ========= */

/* BMElemSeq / Iter
 * ---------------- */

static PyTypeObject *bpy_bm_itype_as_pytype(const char itype)
{
  /* should cover all types */
  switch ((BMIterType)itype) {
    case BM_VERTS_OF_MESH:
    case BM_VERTS_OF_FACE:
    case BM_VERTS_OF_EDGE:
      return &BPy_BMVert_Type;

    case BM_EDGES_OF_MESH:
    case BM_EDGES_OF_FACE:
    case BM_EDGES_OF_VERT:
      return &BPy_BMEdge_Type;

    case BM_FACES_OF_MESH:
    case BM_FACES_OF_EDGE:
    case BM_FACES_OF_VERT:
      return &BPy_BMFace_Type;

    // case BM_ALL_LOOPS_OF_FACE:
    case BM_LOOPS_OF_FACE:
    case BM_LOOPS_OF_EDGE:
    case BM_LOOPS_OF_VERT:
    case BM_LOOPS_OF_LOOP:
      return &BPy_BMLoop_Type;
  }

  return nullptr;
}

static Py_ssize_t bpy_bmelemseq_length(BPy_BMElemSeq *self)
{
  BPY_BM_CHECK_INT(self);

  /* first check if the size is known */
  switch ((BMIterType)self->itype) {
    /* main-types */
    case BM_VERTS_OF_MESH:
      return self->bm->totvert;
    case BM_EDGES_OF_MESH:
      return self->bm->totedge;
    case BM_FACES_OF_MESH:
      return self->bm->totface;

    /* sub-types */
    case BM_VERTS_OF_FACE:
    case BM_EDGES_OF_FACE:
    case BM_LOOPS_OF_FACE:
      BPY_BM_CHECK_INT(self->py_ele);
      return ((BMFace *)self->py_ele->ele)->len;

    case BM_VERTS_OF_EDGE:
      return 2;

    default:
      /* quiet compiler */
      break;
  }

  /* loop over all items, avoid this if we can */
  {
    BMIter iter;
    BMHeader *ele;
    Py_ssize_t tot = 0;

    BM_ITER_BPY_BM_SEQ (ele, &iter, self) {
      tot++;
    }
    return tot;
  }
}

static PyObject *bpy_bmelemseq_subscript_int(BPy_BMElemSeq *self, Py_ssize_t keynum)
{
  BPY_BM_CHECK_OBJ(self);

  if (keynum < 0) {
    /* only get length on negative value, may loop entire seq */
    keynum += bpy_bmelemseq_length(self);
  }
  if (keynum >= 0) {
    if (self->itype <= BM_FACES_OF_MESH) {
      if ((self->bm->elem_table_dirty & bm_iter_itype_htype_map[self->itype]) == 0) {
        BMHeader *ele = nullptr;
        switch (self->itype) {
          case BM_VERTS_OF_MESH:
            if (keynum < self->bm->totvert) {
              ele = (BMHeader *)self->bm->vtable[keynum];
            }
            break;
          case BM_EDGES_OF_MESH:
            if (keynum < self->bm->totedge) {
              ele = (BMHeader *)self->bm->etable[keynum];
            }
            break;
          case BM_FACES_OF_MESH:
            if (keynum < self->bm->totface) {
              ele = (BMHeader *)self->bm->ftable[keynum];
            }
            break;
        }
        if (ele) {
          return BPy_BMElem_CreatePyObject(self->bm, ele);
        }
        /* fall through to index error below */
      }
      else {
        PyErr_SetString(PyExc_IndexError,
                        "BMElemSeq[index]: outdated internal index table, "
                        "run ensure_lookup_table() first");
        return nullptr;
      }
    }
    else {
      BMHeader *ele = static_cast<BMHeader *>(BM_iter_at_index(
          self->bm, self->itype, self->py_ele ? self->py_ele->ele : nullptr, keynum));
      if (ele) {
        return BPy_BMElem_CreatePyObject(self->bm, ele);
      }
    }
  }

  PyErr_Format(PyExc_IndexError, "BMElemSeq[index]: index %d out of range", keynum);
  return nullptr;
}

static PyObject *bpy_bmelemseq_subscript_slice(BPy_BMElemSeq *self,
                                               Py_ssize_t start,
                                               Py_ssize_t stop)
{
  BMIter iter;
  int count = 0;
  bool ok;

  PyObject *list;
  BMHeader *ele;

  BPY_BM_CHECK_OBJ(self);

  list = PyList_New(0);

  ok = BM_iter_init(&iter, self->bm, self->itype, self->py_ele ? self->py_ele->ele : nullptr);

  BLI_assert(ok == true);

  if (UNLIKELY(ok == false)) {
    return list;
  }

  /* first loop up-until the start */
  for (ok = true; ok; ok = (BM_iter_step(&iter) != nullptr)) {
    if (count == start) {
      break;
    }
    count++;
  }

  /* add items until stop */
  while ((ele = static_cast<BMHeader *>(BM_iter_step(&iter)))) {
    PyList_APPEND(list, BPy_BMElem_CreatePyObject(self->bm, ele));

    count++;
    if (count == stop) {
      break;
    }
  }

  return list;
}

static PyObject *bpy_bmelemseq_subscript(BPy_BMElemSeq *self, PyObject *key)
{
  /* don't need error check here */
  if (PyIndex_Check(key)) {
    const Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
    if (i == -1 && PyErr_Occurred()) {
      return nullptr;
    }
    return bpy_bmelemseq_subscript_int(self, i);
  }
  if (PySlice_Check(key)) {
    PySliceObject *key_slice = (PySliceObject *)key;
    Py_ssize_t step = 1;

    if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
      return nullptr;
    }
    if (step != 1) {
      PyErr_SetString(PyExc_TypeError, "BMElemSeq[slice]: slice steps not supported");
      return nullptr;
    }
    if (key_slice->start == Py_None && key_slice->stop == Py_None) {
      return bpy_bmelemseq_subscript_slice(self, 0, PY_SSIZE_T_MAX);
    }

    Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

    /* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
    if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) {
      return nullptr;
    }
    if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop)) {
      return nullptr;
    }

    if (start < 0 || stop < 0) {
      /* only get the length for negative values */
      const Py_ssize_t len = bpy_bmelemseq_length(self);
      if (start < 0) {
        start += len;
        CLAMP_MIN(start, 0);
      }
      if (stop < 0) {
        stop += len;
        CLAMP_MIN(stop, 0);
      }
    }

    if (stop - start <= 0) {
      return PyList_New(0);
    }

    return bpy_bmelemseq_subscript_slice(self, start, stop);
  }

  PyErr_SetString(PyExc_AttributeError, "BMElemSeq[key]: invalid key, key must be an int");
  return nullptr;
}

static int bpy_bmelemseq_contains(BPy_BMElemSeq *self, PyObject *value)
{
  BPY_BM_CHECK_INT(self);

  if (Py_TYPE(value) == bpy_bm_itype_as_pytype(self->itype)) {
    BPy_BMElem *value_bm_ele = (BPy_BMElem *)value;
    if (value_bm_ele->bm == self->bm) {
      BMElem *ele, *ele_test = value_bm_ele->ele;
      BMIter iter;
      BM_ITER_BPY_BM_SEQ (ele, &iter, self) {
        if (ele == ele_test) {
          return 1;
        }
      }
    }
  }

  return 0;
}

/* BMElem (customdata)
 * ------------------- */

static PyObject *bpy_bmelem_subscript(BPy_BMElem *self, BPy_BMLayerItem *key)
{
  BPY_BM_CHECK_OBJ(self);

  return BPy_BMLayerItem_GetItem(self, key);
}

static int bpy_bmelem_ass_subscript(BPy_BMElem *self, BPy_BMLayerItem *key, PyObject *value)
{
  BPY_BM_CHECK_INT(self);

  return BPy_BMLayerItem_SetItem(self, key, value);
}

static PySequenceMethods bpy_bmelemseq_as_sequence = {
    /*sq_length*/ (lenfunc)bpy_bmelemseq_length,
    /*sq_concat*/ nullptr,
    /*sq_repeat*/ nullptr,
    /* Only set this so `PySequence_Check()` returns True. */
    /*sq_item*/ (ssizeargfunc)bpy_bmelemseq_subscript_int,
    /*was_sq_slice*/ nullptr,
    /*sq_ass_item*/ nullptr,
    /*was_sq_ass_slice*/ nullptr,
    /*sq_contains*/ (objobjproc)bpy_bmelemseq_contains,
    /*sq_inplace_concat*/ nullptr,
    /*sq_inplace_repeat*/ nullptr,
};

static PyMappingMethods bpy_bmelemseq_as_mapping = {
    /*mp_length*/ (lenfunc)bpy_bmelemseq_length,
    /*mp_subscript*/ (binaryfunc)bpy_bmelemseq_subscript,
    /*mp_ass_subscript*/ (objobjargproc) nullptr,
};

/* for customdata access */
static PyMappingMethods bpy_bm_elem_as_mapping = {
    /*mp_length*/ (lenfunc) nullptr, /* Keep this empty, messes up `if elem: ...` test. */
    /*mp_subscript*/ (binaryfunc)bpy_bmelem_subscript,
    /*mp_ass_subscript*/ (objobjargproc)bpy_bmelem_ass_subscript,
};

/* Iterator
 * -------- */

static PyObject *bpy_bmelemseq_iter(BPy_BMElemSeq *self)
{
  BPy_BMIter *py_iter;

  BPY_BM_CHECK_OBJ(self);
  py_iter = (BPy_BMIter *)BPy_BMIter_CreatePyObject(self->bm);
  BM_iter_init(
      &(py_iter->iter), self->bm, self->itype, self->py_ele ? self->py_ele->ele : nullptr);
  return (PyObject *)py_iter;
}

static PyObject *bpy_bmiter_next(BPy_BMIter *self)
{
  BMHeader *ele = static_cast<BMHeader *>(BM_iter_step(&self->iter));
  if (ele == nullptr) {
    PyErr_SetNone(PyExc_StopIteration);
    return nullptr;
  }

  return BPy_BMElem_CreatePyObject(self->bm, ele);
}

/* Deallocate Functions
 * ==================== */

static void bpy_bmesh_dealloc(BPy_BMesh *self)
{
  BMesh *bm = self->bm;

  /* The mesh has not been freed by #BMesh. */
  if (bm) {
    bm_dealloc_editmode_warn(self);

    if (CustomData_has_layer(&bm->vdata, CD_BM_ELEM_PYPTR)) {
      BM_data_layer_free(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
    }
    if (CustomData_has_layer(&bm->edata, CD_BM_ELEM_PYPTR)) {
      BM_data_layer_free(bm, &bm->edata, CD_BM_ELEM_PYPTR);
    }
    if (CustomData_has_layer(&bm->pdata, CD_BM_ELEM_PYPTR)) {
      BM_data_layer_free(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
    }
    if (CustomData_has_layer(&bm->ldata, CD_BM_ELEM_PYPTR)) {
      BM_data_layer_free(bm, &bm->ldata, CD_BM_ELEM_PYPTR);
    }

    bm->py_handle = nullptr;

    if ((self->flag & BPY_BMFLAG_IS_WRAPPED) == 0) {
      BM_mesh_free(bm);
    }
  }

  PyObject_DEL(self);
}

static void bpy_bmvert_dealloc(BPy_BMElem *self)
{
  BMesh *bm = self->bm;
  if (bm) {
    void **ptr = static_cast<void **>(
        CustomData_bmesh_get(&bm->vdata, self->ele->head.data, CD_BM_ELEM_PYPTR));
    if (ptr) {
      *ptr = nullptr;
    }
  }
  PyObject_DEL(self);
}

static void bpy_bmedge_dealloc(BPy_BMElem *self)
{
  BMesh *bm = self->bm;
  if (bm) {
    void **ptr = static_cast<void **>(
        CustomData_bmesh_get(&bm->edata, self->ele->head.data, CD_BM_ELEM_PYPTR));
    if (ptr) {
      *ptr = nullptr;
    }
  }
  PyObject_DEL(self);
}

static void bpy_bmface_dealloc(BPy_BMElem *self)
{
  BMesh *bm = self->bm;
  if (bm) {
    void **ptr = static_cast<void **>(
        CustomData_bmesh_get(&bm->pdata, self->ele->head.data, CD_BM_ELEM_PYPTR));
    if (ptr) {
      *ptr = nullptr;
    }
  }
  PyObject_DEL(self);
}

static void bpy_bmloop_dealloc(BPy_BMElem *self)
{
  BMesh *bm = self->bm;
  if (bm) {
    void **ptr = static_cast<void **>(
        CustomData_bmesh_get(&bm->ldata, self->ele->head.data, CD_BM_ELEM_PYPTR));
    if (ptr) {
      *ptr = nullptr;
    }
  }
  PyObject_DEL(self);
}

static void bpy_bmelemseq_dealloc(BPy_BMElemSeq *self)
{
  Py_XDECREF(self->py_ele);

  PyObject_DEL(self);
}

/* not sure where this should go */
static Py_hash_t bpy_bm_elem_hash(PyObject *self)
{
  return Py_HashPointer(((BPy_BMElem *)self)->ele);
}

static Py_hash_t bpy_bm_hash(PyObject *self)
{
  return Py_HashPointer(((BPy_BMesh *)self)->bm);
}

/* Type Doc-strings
 * ================ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmesh_doc,
    "The BMesh data structure\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmvert_doc,
    "The BMesh vertex type\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmedge_doc,
    "The BMesh edge connecting 2 verts\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmface_doc,
    "The BMesh face with 3 or more sides\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmloop_doc,
    "This is normally accessed from :class:`bmesh.types.BMFace.loops` where each face loop "
    "represents a corner of the face.\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmelemseq_doc,
    "General sequence type used for accessing any sequence of\n"
    ":class:`bmesh.types.BMVert`, "
    ":class:`bmesh.types.BMEdge`, "
    ":class:`bmesh.types.BMFace`, "
    ":class:`bmesh.types.BMLoop`.\n"
    "\n"
    "When accessed via "
    ":class:`bmesh.types.BMesh.verts`, "
    ":class:`bmesh.types.BMesh.edges`, "
    ":class:`bmesh.types.BMesh.faces`\n"
    "there are also functions to create/remove items.\n");
PyDoc_STRVAR(
    /* Wrap. */
    bpy_bmiter_doc,
    "Internal BMesh type for looping over verts/faces/edges,\n"
    "used for iterating over :class:`bmesh.types.BMElemSeq` types.\n");

static PyObject *bpy_bmesh_repr(BPy_BMesh *self)
{
  BMesh *bm = self->bm;

  if (bm) {
    return PyUnicode_FromFormat("<BMesh(%p), totvert=%d, totedge=%d, totface=%d, totloop=%d>",
                                bm,
                                bm->totvert,
                                bm->totedge,
                                bm->totface,
                                bm->totloop);
  }

  return PyUnicode_FromFormat("<BMesh dead at %p>", self);
}

static PyObject *bpy_bmvert_repr(BPy_BMVert *self)
{
  BMesh *bm = self->bm;

  if (bm) {
    BMVert *v = self->v;
    return PyUnicode_FromFormat("<BMVert(%p), index=%d>", v, BM_elem_index_get(v));
  }

  return PyUnicode_FromFormat("<BMVert dead at %p>", self);
}

static PyObject *bpy_bmedge_repr(BPy_BMEdge *self)
{
  BMesh *bm = self->bm;

  if (bm) {
    BMEdge *e = self->e;
    return PyUnicode_FromFormat("<BMEdge(%p), index=%d, verts=(%p/%d, %p/%d)>",
                                e,
                                BM_elem_index_get(e),
                                e->v1,
                                BM_elem_index_get(e->v1),
                                e->v2,
                                BM_elem_index_get(e->v2));
  }

  return PyUnicode_FromFormat("<BMEdge dead at %p>", self);
}

static PyObject *bpy_bmface_repr(BPy_BMFace *self)
{
  BMesh *bm = self->bm;

  if (bm) {
    BMFace *f = self->f;
    return PyUnicode_FromFormat(
        "<BMFace(%p), index=%d, totverts=%d>", f, BM_elem_index_get(f), f->len);
  }

  return PyUnicode_FromFormat("<BMFace dead at %p>", self);
}

static PyObject *bpy_bmloop_repr(BPy_BMLoop *self)
{
  BMesh *bm = self->bm;

  if (bm) {
    BMLoop *l = self->l;
    return PyUnicode_FromFormat("<BMLoop(%p), index=%d, vert=%p/%d, edge=%p/%d, face=%p/%d>",
                                l,
                                BM_elem_index_get(l),
                                l->v,
                                BM_elem_index_get(l->v),
                                l->e,
                                BM_elem_index_get(l->e),
                                l->f,
                                BM_elem_index_get(l->f));
  }

  return PyUnicode_FromFormat("<BMLoop dead at %p>", self);
}

/* Types
 * ===== */

PyTypeObject BPy_BMesh_Type;
PyTypeObject BPy_BMVert_Type;
PyTypeObject BPy_BMEdge_Type;
PyTypeObject BPy_BMFace_Type;
PyTypeObject BPy_BMLoop_Type;
PyTypeObject BPy_BMElemSeq_Type;
PyTypeObject BPy_BMVertSeq_Type;
PyTypeObject BPy_BMEdgeSeq_Type;
PyTypeObject BPy_BMFaceSeq_Type;
PyTypeObject BPy_BMLoopSeq_Type;
PyTypeObject BPy_BMIter_Type;

void BPy_BM_init_types()
{
  BPy_BMesh_Type.tp_basicsize = sizeof(BPy_BMesh);
  BPy_BMVert_Type.tp_basicsize = sizeof(BPy_BMVert);
  BPy_BMEdge_Type.tp_basicsize = sizeof(BPy_BMEdge);
  BPy_BMFace_Type.tp_basicsize = sizeof(BPy_BMFace);
  BPy_BMLoop_Type.tp_basicsize = sizeof(BPy_BMLoop);
  BPy_BMElemSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
  BPy_BMVertSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
  BPy_BMEdgeSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
  BPy_BMFaceSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
  BPy_BMLoopSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
  BPy_BMIter_Type.tp_basicsize = sizeof(BPy_BMIter);

  BPy_BMesh_Type.tp_name = "BMesh";
  BPy_BMVert_Type.tp_name = "BMVert";
  BPy_BMEdge_Type.tp_name = "BMEdge";
  BPy_BMFace_Type.tp_name = "BMFace";
  BPy_BMLoop_Type.tp_name = "BMLoop";
  BPy_BMElemSeq_Type.tp_name = "BMElemSeq";
  BPy_BMVertSeq_Type.tp_name = "BMVertSeq";
  BPy_BMEdgeSeq_Type.tp_name = "BMEdgeSeq";
  BPy_BMFaceSeq_Type.tp_name = "BMFaceSeq";
  BPy_BMLoopSeq_Type.tp_name = "BMLoopSeq";
  BPy_BMIter_Type.tp_name = "BMIter";

  BPy_BMesh_Type.tp_doc = bpy_bmesh_doc;
  BPy_BMVert_Type.tp_doc = bpy_bmvert_doc;
  BPy_BMEdge_Type.tp_doc = bpy_bmedge_doc;
  BPy_BMFace_Type.tp_doc = bpy_bmface_doc;
  BPy_BMLoop_Type.tp_doc = bpy_bmloop_doc;
  BPy_BMElemSeq_Type.tp_doc = bpy_bmelemseq_doc;
  BPy_BMVertSeq_Type.tp_doc = nullptr;
  BPy_BMEdgeSeq_Type.tp_doc = nullptr;
  BPy_BMFaceSeq_Type.tp_doc = nullptr;
  BPy_BMLoopSeq_Type.tp_doc = nullptr;
  BPy_BMIter_Type.tp_doc = bpy_bmiter_doc;

  BPy_BMesh_Type.tp_repr = (reprfunc)bpy_bmesh_repr;
  BPy_BMVert_Type.tp_repr = (reprfunc)bpy_bmvert_repr;
  BPy_BMEdge_Type.tp_repr = (reprfunc)bpy_bmedge_repr;
  BPy_BMFace_Type.tp_repr = (reprfunc)bpy_bmface_repr;
  BPy_BMLoop_Type.tp_repr = (reprfunc)bpy_bmloop_repr;
  BPy_BMElemSeq_Type.tp_repr = nullptr;
  BPy_BMVertSeq_Type.tp_repr = nullptr;
  BPy_BMEdgeSeq_Type.tp_repr = nullptr;
  BPy_BMFaceSeq_Type.tp_repr = nullptr;
  BPy_BMLoopSeq_Type.tp_repr = nullptr;
  BPy_BMIter_Type.tp_repr = nullptr;

  BPy_BMesh_Type.tp_getset = bpy_bmesh_getseters;
  BPy_BMVert_Type.tp_getset = bpy_bmvert_getseters;
  BPy_BMEdge_Type.tp_getset = bpy_bmedge_getseters;
  BPy_BMFace_Type.tp_getset = bpy_bmface_getseters;
  BPy_BMLoop_Type.tp_getset = bpy_bmloop_getseters;
  BPy_BMElemSeq_Type.tp_getset = nullptr;
  BPy_BMVertSeq_Type.tp_getset = bpy_bmvertseq_getseters;
  BPy_BMEdgeSeq_Type.tp_getset = bpy_bmedgeseq_getseters;
  BPy_BMFaceSeq_Type.tp_getset = bpy_bmfaceseq_getseters;
  BPy_BMLoopSeq_Type.tp_getset = bpy_bmloopseq_getseters;
  BPy_BMIter_Type.tp_getset = nullptr;

  BPy_BMesh_Type.tp_methods = bpy_bmesh_methods;
  BPy_BMVert_Type.tp_methods = bpy_bmvert_methods;
  BPy_BMEdge_Type.tp_methods = bpy_bmedge_methods;
  BPy_BMFace_Type.tp_methods = bpy_bmface_methods;
  BPy_BMLoop_Type.tp_methods = bpy_bmloop_methods;
  BPy_BMElemSeq_Type.tp_methods = bpy_bmelemseq_methods;
  BPy_BMVertSeq_Type.tp_methods = bpy_bmvertseq_methods;
  BPy_BMEdgeSeq_Type.tp_methods = bpy_bmedgeseq_methods;
  BPy_BMFaceSeq_Type.tp_methods = bpy_bmfaceseq_methods;
  BPy_BMLoopSeq_Type.tp_methods = bpy_bmloopseq_methods;
  BPy_BMIter_Type.tp_methods = nullptr;

  /* #BPy_BMElem_Check() uses #bpy_bm_elem_hash() to check types.
   * if this changes update the macro. */
  BPy_BMesh_Type.tp_hash = bpy_bm_hash;
  BPy_BMVert_Type.tp_hash = bpy_bm_elem_hash;
  BPy_BMEdge_Type.tp_hash = bpy_bm_elem_hash;
  BPy_BMFace_Type.tp_hash = bpy_bm_elem_hash;
  BPy_BMLoop_Type.tp_hash = bpy_bm_elem_hash;
  BPy_BMElemSeq_Type.tp_hash = nullptr;
  BPy_BMVertSeq_Type.tp_hash = nullptr;
  BPy_BMEdgeSeq_Type.tp_hash = nullptr;
  BPy_BMFaceSeq_Type.tp_hash = nullptr;
  BPy_BMLoopSeq_Type.tp_hash = nullptr;
  BPy_BMIter_Type.tp_hash = nullptr;

  BPy_BMElemSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
  BPy_BMVertSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
  BPy_BMEdgeSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
  BPy_BMFaceSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
  BPy_BMLoopSeq_Type.tp_as_sequence =
      nullptr; /* this is not a seq really, only for layer access */

  BPy_BMElemSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
  BPy_BMVertSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
  BPy_BMEdgeSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
  BPy_BMFaceSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
  BPy_BMLoopSeq_Type.tp_as_mapping = nullptr; /* this is not a seq really, only for layer access */

  /* layer access */
  BPy_BMVert_Type.tp_as_mapping = &bpy_bm_elem_as_mapping;
  BPy_BMEdge_Type.tp_as_mapping = &bpy_bm_elem_as_mapping;
  BPy_BMFace_Type.tp_as_mapping = &bpy_bm_elem_as_mapping;
  BPy_BMLoop_Type.tp_as_mapping = &bpy_bm_elem_as_mapping;

  BPy_BMElemSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
  BPy_BMVertSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
  BPy_BMEdgeSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
  BPy_BMFaceSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
  BPy_BMLoopSeq_Type.tp_iter = nullptr; /* no mapping */

  /* Only 1 iterator so far. */
  BPy_BMIter_Type.tp_iternext = (iternextfunc)bpy_bmiter_next;
  BPy_BMIter_Type.tp_iter = PyObject_SelfIter;

  BPy_BMesh_Type.tp_dealloc = (destructor)bpy_bmesh_dealloc;
  BPy_BMVert_Type.tp_dealloc = (destructor)bpy_bmvert_dealloc;
  BPy_BMEdge_Type.tp_dealloc = (destructor)bpy_bmedge_dealloc;
  BPy_BMFace_Type.tp_dealloc = (destructor)bpy_bmface_dealloc;
  BPy_BMLoop_Type.tp_dealloc = (destructor)bpy_bmloop_dealloc;
  BPy_BMElemSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
  BPy_BMVertSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
  BPy_BMEdgeSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
  BPy_BMFaceSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
  BPy_BMLoopSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
  BPy_BMIter_Type.tp_dealloc = nullptr;

  BPy_BMesh_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMVert_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMEdge_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMFace_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLoop_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMElemSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMVertSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMEdgeSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMFaceSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMLoopSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
  BPy_BMIter_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  PyType_Ready(&BPy_BMesh_Type);
  PyType_Ready(&BPy_BMVert_Type);
  PyType_Ready(&BPy_BMEdge_Type);
  PyType_Ready(&BPy_BMFace_Type);
  PyType_Ready(&BPy_BMLoop_Type);
  PyType_Ready(&BPy_BMElemSeq_Type);
  PyType_Ready(&BPy_BMVertSeq_Type);
  PyType_Ready(&BPy_BMEdgeSeq_Type);
  PyType_Ready(&BPy_BMFaceSeq_Type);
  PyType_Ready(&BPy_BMLoopSeq_Type);
  PyType_Ready(&BPy_BMIter_Type);
}

/* bmesh.types submodule
 * ********************* */

/* This exists to declare substitutions. */
PyDoc_STRVAR(
    /* Wrap. */
    BPy_BM_types_module_doc,
    "\n"
    ".. |UV_STICKY_SELECT_MODE_REF| replace:: "
    "(:class:`bpy.types.ToolSettings.uv_sticky_select_mode` which may be passed in directly).\n"
    "\n"
    ".. |UV_STICKY_SELECT_MODE_TYPE| replace:: "
    "Literal['SHARED_LOCATION', 'DISABLED', 'SHARED_VERTEX']\n"
    "\n"
    ".. |UV_SELECT_FLUSH_MODE_NEEDED| replace:: "
    "This function selection-mode independent, "
    "typically :class:`bmesh.types.BMesh.uv_select_flush_mode` should be called afterwards.\n"
    "\n"
    ".. |UV_SELECT_SYNC_TO_MESH_NEEDED| replace:: "
    "This function doesn't flush the selection to the mesh, "
    "typically :class:`bmesh.types.BMesh.uv_select_sync_to_mesh` should be called afterwards.\n");
static PyModuleDef BPy_BM_types_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh.types",
    /*m_doc*/ BPy_BM_types_module_doc,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_bmesh_types()
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPy_BM_types_module_def);

  /* `bmesh_py_types.cc` */
  PyModule_AddType(submodule, &BPy_BMesh_Type);
  PyModule_AddType(submodule, &BPy_BMVert_Type);
  PyModule_AddType(submodule, &BPy_BMEdge_Type);
  PyModule_AddType(submodule, &BPy_BMFace_Type);
  PyModule_AddType(submodule, &BPy_BMLoop_Type);
  PyModule_AddType(submodule, &BPy_BMElemSeq_Type);
  PyModule_AddType(submodule, &BPy_BMVertSeq_Type);
  PyModule_AddType(submodule, &BPy_BMEdgeSeq_Type);
  PyModule_AddType(submodule, &BPy_BMFaceSeq_Type);
  PyModule_AddType(submodule, &BPy_BMLoopSeq_Type);
  PyModule_AddType(submodule, &BPy_BMIter_Type);
  /* `bmesh_py_types_select.cc` */
  PyModule_AddType(submodule, &BPy_BMEditSelSeq_Type);
  PyModule_AddType(submodule, &BPy_BMEditSelIter_Type);
  /* `bmesh_py_types_customdata.cc` */
  PyModule_AddType(submodule, &BPy_BMLayerAccessVert_Type);
  PyModule_AddType(submodule, &BPy_BMLayerAccessEdge_Type);
  PyModule_AddType(submodule, &BPy_BMLayerAccessFace_Type);
  PyModule_AddType(submodule, &BPy_BMLayerAccessLoop_Type);
  PyModule_AddType(submodule, &BPy_BMLayerCollection_Type);
  PyModule_AddType(submodule, &BPy_BMLayerItem_Type);
  /* `bmesh_py_types_meshdata.cc` */
  PyModule_AddType(submodule, &BPy_BMLoopUV_Type);
  PyModule_AddType(submodule, &BPy_BMDeformVert_Type);

  return submodule;
}

/* Utility Functions
 * ***************** */

PyObject *BPy_BMesh_CreatePyObject(BMesh *bm, int flag)
{
  BPy_BMesh *self;

  if (bm->py_handle) {
    self = static_cast<BPy_BMesh *>(bm->py_handle);
    Py_INCREF(self);
  }
  else {
    self = PyObject_New(BPy_BMesh, &BPy_BMesh_Type);
    self->bm = bm;
    self->flag = flag;

    bm->py_handle = self; /* point back */

/* avoid allocating layers when we don't have to */
#if 0
    BM_data_layer_add(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
    BM_data_layer_add(bm, &bm->edata, CD_BM_ELEM_PYPTR);
    BM_data_layer_add(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
    BM_data_layer_add(bm, &bm->ldata, CD_BM_ELEM_PYPTR);
#endif
  }

  return (PyObject *)self;
}

PyObject *BPy_BMVert_CreatePyObject(BMesh *bm, BMVert *v)
{
  BPy_BMVert *self;

  void **ptr = static_cast<void **>(
      CustomData_bmesh_get(&bm->vdata, v->head.data, CD_BM_ELEM_PYPTR));

  /* bmesh may free layers, ensure we have one to store ourself */
  if (UNLIKELY(ptr == nullptr)) {
    BM_data_layer_add(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
    ptr = static_cast<void **>(CustomData_bmesh_get(&bm->vdata, v->head.data, CD_BM_ELEM_PYPTR));
  }

  if (*ptr != nullptr) {
    self = static_cast<BPy_BMVert *>(*ptr);
    Py_INCREF(self);
  }
  else {
    self = PyObject_New(BPy_BMVert, &BPy_BMVert_Type);
    BLI_assert(v != nullptr);
    self->bm = bm;
    self->v = v;
    *ptr = self;
  }
  return (PyObject *)self;
}

PyObject *BPy_BMEdge_CreatePyObject(BMesh *bm, BMEdge *e)
{
  BPy_BMEdge *self;

  void **ptr = static_cast<void **>(
      CustomData_bmesh_get(&bm->edata, e->head.data, CD_BM_ELEM_PYPTR));

  /* bmesh may free layers, ensure we have one to store ourself */
  if (UNLIKELY(ptr == nullptr)) {
    BM_data_layer_add(bm, &bm->edata, CD_BM_ELEM_PYPTR);
    ptr = static_cast<void **>(CustomData_bmesh_get(&bm->edata, e->head.data, CD_BM_ELEM_PYPTR));
  }

  if (*ptr != nullptr) {
    self = static_cast<BPy_BMEdge *>(*ptr);
    Py_INCREF(self);
  }
  else {
    self = PyObject_New(BPy_BMEdge, &BPy_BMEdge_Type);
    BLI_assert(e != nullptr);
    self->bm = bm;
    self->e = e;
    *ptr = self;
  }
  return (PyObject *)self;
}

PyObject *BPy_BMFace_CreatePyObject(BMesh *bm, BMFace *f)
{
  BPy_BMFace *self;

  void **ptr = static_cast<void **>(
      CustomData_bmesh_get(&bm->pdata, f->head.data, CD_BM_ELEM_PYPTR));

  /* bmesh may free layers, ensure we have one to store ourself */
  if (UNLIKELY(ptr == nullptr)) {
    BM_data_layer_add(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
    ptr = static_cast<void **>(CustomData_bmesh_get(&bm->pdata, f->head.data, CD_BM_ELEM_PYPTR));
  }

  if (*ptr != nullptr) {
    self = static_cast<BPy_BMFace *>(*ptr);
    Py_INCREF(self);
  }
  else {
    self = PyObject_New(BPy_BMFace, &BPy_BMFace_Type);
    BLI_assert(f != nullptr);
    self->bm = bm;
    self->f = f;
    *ptr = self;
  }
  return (PyObject *)self;
}

PyObject *BPy_BMLoop_CreatePyObject(BMesh *bm, BMLoop *l)
{
  BPy_BMLoop *self;

  void **ptr = static_cast<void **>(
      CustomData_bmesh_get(&bm->ldata, l->head.data, CD_BM_ELEM_PYPTR));

  /* bmesh may free layers, ensure we have one to store ourself */
  if (UNLIKELY(ptr == nullptr)) {
    BM_data_layer_add(bm, &bm->ldata, CD_BM_ELEM_PYPTR);
    ptr = static_cast<void **>(CustomData_bmesh_get(&bm->ldata, l->head.data, CD_BM_ELEM_PYPTR));
  }

  if (*ptr != nullptr) {
    self = static_cast<BPy_BMLoop *>(*ptr);
    Py_INCREF(self);
  }
  else {
    self = PyObject_New(BPy_BMLoop, &BPy_BMLoop_Type);
    BLI_assert(l != nullptr);
    self->bm = bm;
    self->l = l;
    *ptr = self;
  }
  return (PyObject *)self;
}

PyObject *BPy_BMElemSeq_CreatePyObject(BMesh *bm, BPy_BMElem *py_ele, const char itype)
{
  BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMElemSeq_Type);
  self->bm = bm;
  self->py_ele = py_ele; /* can be nullptr */
  self->itype = itype;
  Py_XINCREF(py_ele);
  return (PyObject *)self;
}

PyObject *BPy_BMVertSeq_CreatePyObject(BMesh *bm)
{
  BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMVertSeq_Type);
  self->bm = bm;
  self->py_ele = nullptr; /* unused */
  self->itype = BM_VERTS_OF_MESH;
  return (PyObject *)self;
}

PyObject *BPy_BMEdgeSeq_CreatePyObject(BMesh *bm)
{
  BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMEdgeSeq_Type);
  self->bm = bm;
  self->py_ele = nullptr; /* unused */
  self->itype = BM_EDGES_OF_MESH;
  return (PyObject *)self;
}

PyObject *BPy_BMFaceSeq_CreatePyObject(BMesh *bm)
{
  BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMFaceSeq_Type);
  self->bm = bm;
  self->py_ele = nullptr; /* unused */
  self->itype = BM_FACES_OF_MESH;
  return (PyObject *)self;
}

PyObject *BPy_BMLoopSeq_CreatePyObject(BMesh *bm)
{
  BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMLoopSeq_Type);
  self->bm = bm;
  self->py_ele = nullptr; /* unused */
  self->itype = 0;        /* should never be passed to the iterator function */
  return (PyObject *)self;
}

PyObject *BPy_BMIter_CreatePyObject(BMesh *bm)
{
  BPy_BMIter *self = PyObject_New(BPy_BMIter, &BPy_BMIter_Type);
  self->bm = bm;
  /* caller must initialize 'iter' member */
  return (PyObject *)self;
}

PyObject *BPy_BMElem_CreatePyObject(BMesh *bm, BMHeader *ele)
{
  switch (ele->htype) {
    case BM_VERT:
      return BPy_BMVert_CreatePyObject(bm, (BMVert *)ele);
    case BM_EDGE:
      return BPy_BMEdge_CreatePyObject(bm, (BMEdge *)ele);
    case BM_FACE:
      return BPy_BMFace_CreatePyObject(bm, (BMFace *)ele);
    case BM_LOOP:
      return BPy_BMLoop_CreatePyObject(bm, (BMLoop *)ele);
    default:
      BLI_assert_unreachable();
      PyErr_SetString(PyExc_SystemError, "internal error");
      return nullptr;
  }
}

int bpy_bm_generic_valid_check(BPy_BMGeneric *self)
{
  if (LIKELY(self->bm)) {

/* far too slow to enable by default but handy
 * to uncomment for debugging tricky errors,
 * note that this will throw error on entering a
 * function where the actual error will be caused by
 * the previous action. */
#if 0
    if (BM_mesh_is_valid(self->bm) == false) {
      PyErr_Format(
          PyExc_ReferenceError, "BMesh used by %.200s has become invalid", Py_TYPE(self)->tp_name);
      return -1;
    }
#endif

    return 0;
  }

  PyErr_Format(
      PyExc_ReferenceError, "BMesh data of type %.200s has been removed", Py_TYPE(self)->tp_name);
  return -1;
}

int bpy_bm_generic_valid_check_source(BMesh *bm_source,
                                      const char *error_prefix,
                                      void **args,
                                      uint args_tot)
{
  int ret = 0;

  while (args_tot--) {
    BPy_BMGeneric *py_bm_elem = static_cast<BPy_BMGeneric *>(args[args_tot]);
    if (py_bm_elem) {

      BLI_assert(BPy_BMesh_Check(py_bm_elem) || BPy_BMElem_Check(py_bm_elem));

      ret = bpy_bm_generic_valid_check(py_bm_elem);
      if (UNLIKELY(ret == -1)) {
        break;
      }

      if (UNLIKELY(py_bm_elem->bm != bm_source)) {
        /* could give more info here */
        PyErr_Format(PyExc_ValueError,
                     "%.200s: BMesh data of type %.200s is from another mesh",
                     error_prefix,
                     Py_TYPE(py_bm_elem)->tp_name);
        ret = -1;
        break;
      }
    }
  }

  return ret;
}

int bpy_bm_check_uv_select_sync_valid(BMesh *bm, const char *error_prefix)
{
  int ret = 0;
  if (bm->uv_select_sync_valid == false) {
    PyErr_Format(PyExc_ValueError, "%s: bm.uv_select_sync_valid: must be true", error_prefix);
    ret = -1;
  }
  return ret;
}

int bpy_bm_uv_layer_offset_or_error(BMesh *bm, const char *error_prefix)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);
  if (cd_loop_uv_offset == -1) {
    PyErr_Format(PyExc_ValueError, "%s: failed, no UV layer found", error_prefix);
  }
  return cd_loop_uv_offset;
}

int bpy_bm_check_bm_match_or_error(BMesh *bm_a, BMesh *bm_b, const char *error_prefix)
{
  if (bm_a != bm_b) {
    PyErr_Format(PyExc_ValueError, "%s: elements must be from a singe BMesh", error_prefix);
    return -1;
  }
  return 0;
}

void bpy_bm_generic_invalidate(BPy_BMGeneric *self)
{
  self->bm = nullptr;
}

void *BPy_BMElem_PySeq_As_Array_FAST(BMesh **r_bm,
                                     PyObject *seq_fast,
                                     Py_ssize_t min,
                                     Py_ssize_t max,
                                     Py_ssize_t *r_seq_num,
                                     const char htype,
                                     const bool do_unique_check,
                                     const bool do_bm_check,
                                     const char *error_prefix)
{
  BMesh *bm = (r_bm && *r_bm) ? *r_bm : nullptr;
  PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
  const Py_ssize_t seq_num = PySequence_Fast_GET_SIZE(seq_fast);
  Py_ssize_t i, i_last_dirty = PY_SSIZE_T_MAX;

  BPy_BMElem *item;
  BMElem **alloc;

  *r_seq_num = 0;

  if (seq_num < min || seq_num > max) {
    PyErr_Format(PyExc_TypeError,
                 "%s: sequence incorrect size, expected [%d - %d], given %d",
                 error_prefix,
                 min,
                 max,
                 seq_num);
    return nullptr;
  }

  /* from now on, use goto */
  alloc = static_cast<BMElem **>(PyMem_MALLOC(seq_num * sizeof(BPy_BMElem **)));

  for (i = 0; i < seq_num; i++) {
    item = (BPy_BMElem *)seq_fast_items[i];

    if (!BPy_BMElem_CheckHType(Py_TYPE(item), htype)) {
      PyErr_Format(PyExc_TypeError,
                   "%s: expected %.200s, not '%.200s'",
                   error_prefix,
                   BPy_BMElem_StringFromHType(htype),
                   Py_TYPE(item)->tp_name);
      goto err_cleanup;
    }
    else if (!BPY_BM_IS_VALID(item)) {
      PyErr_Format(
          PyExc_TypeError, "%s: %d %s has been removed", error_prefix, i, Py_TYPE(item)->tp_name);
      goto err_cleanup;
    }
    /* trick so we can ensure all items have the same mesh,
     * and allows us to pass the 'bm' as nullptr. */
    else if (do_bm_check && (bm && bm != item->bm)) {
      PyErr_Format(PyExc_ValueError,
                   "%s: %d %s is from another mesh",
                   error_prefix,
                   i,
                   BPy_BMElem_StringFromHType(htype));
      goto err_cleanup;
    }

    if (bm == nullptr) {
      bm = item->bm;
    }

    alloc[i] = item->ele;

    if (do_unique_check) {
      BM_elem_flag_enable(item->ele, BM_ELEM_INTERNAL_TAG);
      i_last_dirty = i;
    }
  }

  if (do_unique_check) {
    /* check for double verts! */
    bool ok = true;
    for (i = 0; i < seq_num; i++) {
      if (UNLIKELY(BM_elem_flag_test(alloc[i], BM_ELEM_INTERNAL_TAG) == false)) {
        ok = false;
      }

      /* ensure we don't leave this enabled */
      BM_elem_flag_disable(alloc[i], BM_ELEM_INTERNAL_TAG);
    }

    if (ok == false) {
      /* Cleared above. */
      i_last_dirty = PY_SSIZE_T_MAX;
      PyErr_Format(PyExc_ValueError,
                   "%s: found the same %.200s used multiple times",
                   error_prefix,
                   BPy_BMElem_StringFromHType(htype));
      goto err_cleanup;
    }
  }

  *r_seq_num = seq_num;
  if (r_bm) {
    *r_bm = bm;
  }
  return alloc;

err_cleanup:
  if (do_unique_check && (i_last_dirty != PY_SSIZE_T_MAX)) {
    for (i = 0; i <= i_last_dirty; i++) {
      BM_elem_flag_disable(alloc[i], BM_ELEM_INTERNAL_TAG);
    }
  }
  PyMem_FREE(alloc);
  return nullptr;
}

void *BPy_BMElem_PySeq_As_Array(BMesh **r_bm,
                                PyObject *seq,
                                Py_ssize_t min,
                                Py_ssize_t max,
                                Py_ssize_t *r_seq_num,
                                const char htype,
                                const bool do_unique_check,
                                const bool do_bm_check,
                                const char *error_prefix)
{
  PyObject *seq_fast;
  PyObject *ret;

  if (!(seq_fast = PySequence_Fast(seq, error_prefix))) {
    return nullptr;
  }

  ret = static_cast<PyObject *>(BPy_BMElem_PySeq_As_Array_FAST(
      r_bm, seq_fast, min, max, r_seq_num, htype, do_unique_check, do_bm_check, error_prefix));

  Py_DECREF(seq_fast);
  return ret;
}

BMVert **BPy_BMVert_PySeq_As_Array(BMesh **r_bm,
                                   PyObject *seq,
                                   Py_ssize_t min,
                                   Py_ssize_t max,
                                   Py_ssize_t *r_seq_num,
                                   bool do_unique_check,
                                   bool do_bm_check,
                                   const char *error_prefix)
{
  return static_cast<BMVert **>(BPy_BMElem_PySeq_As_Array(
      r_bm, seq, min, max, r_seq_num, BM_VERT, do_unique_check, do_bm_check, error_prefix));
}
BMEdge **BPy_BMEdge_PySeq_As_Array(BMesh **r_bm,
                                   PyObject *seq,
                                   Py_ssize_t min,
                                   Py_ssize_t max,
                                   Py_ssize_t *r_seq_num,
                                   bool do_unique_check,
                                   bool do_bm_check,
                                   const char *error_prefix)
{
  return static_cast<BMEdge **>(BPy_BMElem_PySeq_As_Array(
      r_bm, seq, min, max, r_seq_num, BM_EDGE, do_unique_check, do_bm_check, error_prefix));
}
BMFace **BPy_BMFace_PySeq_As_Array(BMesh **r_bm,
                                   PyObject *seq,
                                   Py_ssize_t min,
                                   Py_ssize_t max,
                                   Py_ssize_t *r_seq_num,
                                   bool do_unique_check,
                                   bool do_bm_check,
                                   const char *error_prefix)
{
  return static_cast<BMFace **>(BPy_BMElem_PySeq_As_Array(
      r_bm, seq, min, max, r_seq_num, BM_FACE, do_unique_check, do_bm_check, error_prefix));
}
BMLoop **BPy_BMLoop_PySeq_As_Array(BMesh **r_bm,
                                   PyObject *seq,
                                   Py_ssize_t min,
                                   Py_ssize_t max,
                                   Py_ssize_t *r_seq_num,
                                   bool do_unique_check,
                                   bool do_bm_check,
                                   const char *error_prefix)
{
  return static_cast<BMLoop **>(BPy_BMElem_PySeq_As_Array(
      r_bm, seq, min, max, r_seq_num, BM_LOOP, do_unique_check, do_bm_check, error_prefix));
}

PyObject *BPy_BMElem_Array_As_Tuple(BMesh *bm, BMHeader **elem, Py_ssize_t elem_num)
{
  Py_ssize_t i;
  PyObject *ret = PyTuple_New(elem_num);
  for (i = 0; i < elem_num; i++) {
    PyTuple_SET_ITEM(ret, i, BPy_BMElem_CreatePyObject(bm, elem[i]));
  }
  return ret;
}
PyObject *BPy_BMVert_Array_As_Tuple(BMesh *bm, BMVert **elem, Py_ssize_t elem_num)
{
  Py_ssize_t i;
  PyObject *ret = PyTuple_New(elem_num);
  for (i = 0; i < elem_num; i++) {
    PyTuple_SET_ITEM(ret, i, BPy_BMVert_CreatePyObject(bm, elem[i]));
  }
  return ret;
}
PyObject *BPy_BMEdge_Array_As_Tuple(BMesh *bm, BMEdge **elem, Py_ssize_t elem_num)
{
  Py_ssize_t i;
  PyObject *ret = PyTuple_New(elem_num);
  for (i = 0; i < elem_num; i++) {
    PyTuple_SET_ITEM(ret, i, BPy_BMEdge_CreatePyObject(bm, elem[i]));
  }

  return ret;
}
PyObject *BPy_BMFace_Array_As_Tuple(BMesh *bm, BMFace **elem, Py_ssize_t elem_num)
{
  Py_ssize_t i;
  PyObject *ret = PyTuple_New(elem_num);
  for (i = 0; i < elem_num; i++) {
    PyTuple_SET_ITEM(ret, i, BPy_BMFace_CreatePyObject(bm, elem[i]));
  }

  return ret;
}
PyObject *BPy_BMLoop_Array_As_Tuple(BMesh *bm, BMLoop *const *elem, Py_ssize_t elem_num)
{
  Py_ssize_t i;
  PyObject *ret = PyTuple_New(elem_num);
  for (i = 0; i < elem_num; i++) {
    PyTuple_SET_ITEM(ret, i, BPy_BMLoop_CreatePyObject(bm, elem[i]));
  }

  return ret;
}

int BPy_BMElem_CheckHType(PyTypeObject *type, const char htype)
{
  return (((htype & BM_VERT) && (type == &BPy_BMVert_Type)) ||
          ((htype & BM_EDGE) && (type == &BPy_BMEdge_Type)) ||
          ((htype & BM_FACE) && (type == &BPy_BMFace_Type)) ||
          ((htype & BM_LOOP) && (type == &BPy_BMLoop_Type)));
}

char *BPy_BMElem_StringFromHType_ex(const char htype, char ret[32])
{
  /* Zero to ensure string is always null terminated. */
  const char *ret_array[4];
  int i = 0;
  if (htype & BM_VERT) {
    ret_array[i++] = BPy_BMVert_Type.tp_name;
  }
  if (htype & BM_EDGE) {
    ret_array[i++] = BPy_BMEdge_Type.tp_name;
  }
  if (htype & BM_FACE) {
    ret_array[i++] = BPy_BMFace_Type.tp_name;
  }
  if (htype & BM_LOOP) {
    ret_array[i++] = BPy_BMLoop_Type.tp_name;
  }
  ret[0] = '(';
  int ret_ofs = BLI_string_join_array_by_sep_char(ret + 1, 30, '/', ret_array, i) + 1;
  ret[ret_ofs] = ')';
  ret[ret_ofs + 1] = '\0';
  return ret;
}
char *BPy_BMElem_StringFromHType(const char htype)
{
  /* Zero to ensure string is always null terminated. */
  static char ret[32];
  return BPy_BMElem_StringFromHType_ex(htype, ret);
}

/* -------------------------------------------------------------------- */
/* keep at bottom */

/* This function is called on free, it should stay quite fast */
static void bm_dealloc_editmode_warn(BPy_BMesh *self)
{
  if (self->flag & BPY_BMFLAG_IS_WRAPPED) {
    /* Currently NOP - this works without warnings now. */
  }
}
