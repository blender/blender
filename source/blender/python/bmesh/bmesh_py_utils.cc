/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 *
 * This file defines the 'bmesh.utils' module.
 * Utility functions for operating on 'bmesh.types'
 */

#include <Python.h>

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "../mathutils/mathutils.hh"

#include "bmesh.hh"
#include "bmesh_py_types.hh"
#include "bmesh_py_utils.hh" /* own include */

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh"
#include "../generic/python_utildefines.hh"

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_vert_collapse_edge_doc,
    ".. method:: vert_collapse_edge(vert, edge)\n"
    "\n"
    "   Collapse a vertex into an edge.\n"
    "\n"
    "   :arg vert: The vert that will be collapsed.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :arg edge: The edge to collapse into.\n"
    "   :type edge: :class:`bmesh.types.BMEdge`\n"
    "   :return: The resulting edge from the collapse operation.\n"
    "   :rtype: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bm_utils_vert_collapse_edge(PyObject * /*self*/, PyObject *args)
{
  BPy_BMEdge *py_edge;
  BPy_BMVert *py_vert;

  BMesh *bm;
  BMEdge *e_new = nullptr;

  if (!PyArg_ParseTuple(
          args, "O!O!:vert_collapse_edge", &BPy_BMVert_Type, &py_vert, &BPy_BMEdge_Type, &py_edge))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_edge);
  BPY_BM_CHECK_OBJ(py_vert);

  /* this doubles for checking that the verts are in the same mesh */
  if (!(py_edge->e->v1 == py_vert->v || py_edge->e->v2 == py_vert->v)) {
    PyErr_SetString(PyExc_ValueError,
                    "vert_collapse_edge(vert, edge): the vertex is not found in the edge");
    return nullptr;
  }

  if (BM_vert_edge_count_is_over(py_vert->v, 2)) {
    PyErr_SetString(PyExc_ValueError,
                    "vert_collapse_edge(vert, edge): vert has more than 2 connected edges");
    return nullptr;
  }

  bm = py_edge->bm;

  e_new = BM_vert_collapse_edge(bm, py_edge->e, py_vert->v, true, true, true);

  if (e_new) {
    return BPy_BMEdge_CreatePyObject(bm, e_new);
  }

  PyErr_SetString(PyExc_ValueError,
                  "vert_collapse_edge(vert, edge): no new edge created, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_vert_collapse_faces_doc,
    ".. method:: vert_collapse_faces(vert, edge, fac, join_faces)\n"
    "\n"
    "   Collapses a vertex that has only two manifold edges onto a vertex it shares an "
    "edge with.\n"
    "\n"
    "   :arg vert: The vert that will be collapsed.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :arg edge: The edge to collapse into.\n"
    "   :type edge: :class:`bmesh.types.BMEdge`\n"
    "   :arg fac: The factor to use when merging customdata [0 - 1].\n"
    "   :type fac: float\n"
    "   :arg join_faces: When true the faces around the vertex will be joined otherwise "
    "collapse the vertex by merging the 2 edges this vertex connects to into one.\n"
    "   :type join_faces: bool\n"
    "   :return: The resulting edge from the collapse operation.\n"
    "   :rtype: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bm_utils_vert_collapse_faces(PyObject * /*self*/, PyObject *args)
{
  BPy_BMEdge *py_edge;
  BPy_BMVert *py_vert;

  float fac;
  int do_join_faces;

  BMesh *bm;
  BMEdge *e_new = nullptr;

  if (!PyArg_ParseTuple(args,
                        "O!O!fi:vert_collapse_faces",
                        &BPy_BMVert_Type,
                        &py_vert,
                        &BPy_BMEdge_Type,
                        &py_edge,
                        &fac,
                        &do_join_faces))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_edge);
  BPY_BM_CHECK_OBJ(py_vert);

  /* this doubles for checking that the verts are in the same mesh */
  if (!(py_edge->e->v1 == py_vert->v || py_edge->e->v2 == py_vert->v)) {
    PyErr_SetString(PyExc_ValueError,
                    "vert_collapse_faces(vert, edge): the vertex is not found in the edge");
    return nullptr;
  }

  if (BM_vert_edge_count_is_over(py_vert->v, 2)) {
    PyErr_SetString(PyExc_ValueError,
                    "vert_collapse_faces(vert, edge): vert has more than 2 connected edges");
    return nullptr;
  }

  bm = py_edge->bm;

  e_new = BM_vert_collapse_faces(
      bm, py_edge->e, py_vert->v, clamp_f(fac, 0.0f, 1.0f), true, do_join_faces, true, true);

  if (e_new) {
    return BPy_BMEdge_CreatePyObject(bm, e_new);
  }

  PyErr_SetString(PyExc_ValueError,
                  "vert_collapse_faces(vert, edge): no new edge created, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_vert_dissolve_doc,
    ".. method:: vert_dissolve(vert)\n"
    "\n"
    "   Dissolve this vertex (will be removed).\n"
    "\n"
    "   :arg vert: The vert to be dissolved.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :return: True when the vertex dissolve is successful.\n"
    "   :rtype: bool\n");
static PyObject *bpy_bm_utils_vert_dissolve(PyObject * /*self*/, PyObject *args)
{
  BPy_BMVert *py_vert;

  BMesh *bm;

  if (!PyArg_ParseTuple(args, "O!:vert_dissolve", &BPy_BMVert_Type, &py_vert)) {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_vert);

  bm = py_vert->bm;

  return PyBool_FromLong(BM_vert_dissolve(bm, py_vert->v));
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_vert_splice_doc,
    ".. method:: vert_splice(vert, vert_target)\n"
    "\n"
    "   Splice vert into vert_target.\n"
    "\n"
    "   :arg vert: The vertex to be removed.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :arg vert_target: The vertex to use.\n"
    "   :type vert_target: :class:`bmesh.types.BMVert`\n"
    "\n"
    "   .. note:: The verts mustn't share an edge or face.\n");
static PyObject *bpy_bm_utils_vert_splice(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "vert_splice(...)";
  BPy_BMVert *py_vert;
  BPy_BMVert *py_vert_target;

  BMesh *bm;

  bool ok;

  if (!PyArg_ParseTuple(
          args, "O!O!:vert_splice", &BPy_BMVert_Type, &py_vert, &BPy_BMVert_Type, &py_vert_target))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_vert);
  BPY_BM_CHECK_OBJ(py_vert_target);

  bm = py_vert->bm;
  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, py_vert_target);

  if (py_vert->v == py_vert_target->v) {
    PyErr_Format(PyExc_ValueError, "%s: vert arguments match", error_prefix);
    return nullptr;
  }

  if (BM_edge_exists(py_vert->v, py_vert_target->v)) {
    PyErr_Format(PyExc_ValueError, "%s: verts cannot share an edge", error_prefix);
    return nullptr;
  }

  if (BM_vert_pair_share_face_check(py_vert->v, py_vert_target->v)) {
    PyErr_Format(PyExc_ValueError, "%s: verts cannot share a face", error_prefix);
    return nullptr;
  }

  /* should always succeed */
  ok = BM_vert_splice(bm, py_vert_target->v, py_vert->v);
  BLI_assert(ok == true);
  UNUSED_VARS_NDEBUG(ok);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_vert_separate_doc,
    ".. method:: vert_separate(vert, edges)\n"
    "\n"
    "   Separate this vertex at every edge.\n"
    "\n"
    "   :arg vert: The vert to be separated.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :arg edges: The edges to separated.\n"
    "   :type edges: :class:`bmesh.types.BMEdge`\n"
    "   :return: The newly separated verts (including the vertex passed).\n"
    "   :rtype: tuple[:class:`bmesh.types.BMVert`, ...]\n");
static PyObject *bpy_bm_utils_vert_separate(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "vert_separate(...)";
  BPy_BMVert *py_vert;
  PyObject *edge_seq;

  BMesh *bm;
  BMVert **elem;
  int elem_len;

  PyObject *ret;

  if (!PyArg_ParseTuple(args, "O!O:vert_separate", &BPy_BMVert_Type, &py_vert, &edge_seq)) {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_vert);

  bm = py_vert->bm;

  /* Edges to split. */
  Py_ssize_t edge_array_num;
  BMEdge **edge_array = BPy_BMEdge_PySeq_As_Array(
      &bm, edge_seq, 0, PY_SSIZE_T_MAX, &edge_array_num, true, true, error_prefix);

  if (edge_array == nullptr) {
    return nullptr;
  }

  BM_vert_separate(bm, py_vert->v, edge_array, edge_array_num, false, &elem, &elem_len);
  /* return collected verts */
  ret = BPy_BMVert_Array_As_Tuple(bm, elem, elem_len);
  MEM_freeN(elem);

  PyMem_FREE(edge_array);

  return ret;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_edge_split_doc,
    ".. method:: edge_split(edge, vert, fac)\n"
    "\n"
    "   Split an edge, return the newly created data.\n"
    "\n"
    "   :arg edge: The edge to split.\n"
    "   :type edge: :class:`bmesh.types.BMEdge`\n"
    "   :arg vert: One of the verts on the edge, defines the split direction.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :arg fac: The point on the edge where the new vert will be created [0 - 1].\n"
    "   :type fac: float\n"
    "   :return: The newly created (edge, vert) pair.\n"
    "   :rtype: tuple[:class:`bmesh.types.BMEdge`, :class:`bmesh.types.BMVert`]\n");
static PyObject *bpy_bm_utils_edge_split(PyObject * /*self*/, PyObject *args)
{
  BPy_BMEdge *py_edge;
  BPy_BMVert *py_vert;
  float fac;

  BMesh *bm;
  BMVert *v_new = nullptr;
  BMEdge *e_new = nullptr;

  if (!PyArg_ParseTuple(
          args, "O!O!f:edge_split", &BPy_BMEdge_Type, &py_edge, &BPy_BMVert_Type, &py_vert, &fac))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_edge);
  BPY_BM_CHECK_OBJ(py_vert);

  /* this doubles for checking that the verts are in the same mesh */
  if (!(py_edge->e->v1 == py_vert->v || py_edge->e->v2 == py_vert->v)) {
    PyErr_SetString(PyExc_ValueError,
                    "edge_split(edge, vert): the vertex is not found in the edge");
    return nullptr;
  }

  bm = py_edge->bm;

  v_new = BM_edge_split(bm, py_edge->e, py_vert->v, &e_new, clamp_f(fac, 0.0f, 1.0f));

  if (v_new && e_new) {
    PyObject *ret = PyTuple_New(2);
    PyTuple_SET_ITEMS(
        ret, BPy_BMEdge_CreatePyObject(bm, e_new), BPy_BMVert_CreatePyObject(bm, v_new));
    return ret;
  }

  PyErr_SetString(PyExc_ValueError,
                  "edge_split(edge, vert): couldn't split the edge, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_edge_rotate_doc,
    ".. method:: edge_rotate(edge, ccw=False)\n"
    "\n"
    "   Rotate the edge and return the newly created edge.\n"
    "   If rotating the edge fails, None will be returned.\n"
    "\n"
    "   :arg edge: The edge to rotate.\n"
    "   :type edge: :class:`bmesh.types.BMEdge`\n"
    "   :arg ccw: When True the edge will be rotated counter clockwise.\n"
    "   :type ccw: bool\n"
    "   :return: The newly rotated edge.\n"
    "   :rtype: :class:`bmesh.types.BMEdge`\n");
static PyObject *bpy_bm_utils_edge_rotate(PyObject * /*self*/, PyObject *args)
{
  BPy_BMEdge *py_edge;
  bool do_ccw = false;

  BMesh *bm;
  BMEdge *e_new = nullptr;

  if (!PyArg_ParseTuple(
          args, "O!|O&:edge_rotate", &BPy_BMEdge_Type, &py_edge, PyC_ParseBool, &do_ccw))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_edge);

  bm = py_edge->bm;

  e_new = BM_edge_rotate(bm, py_edge->e, do_ccw, 0);

  if (e_new) {
    return BPy_BMEdge_CreatePyObject(bm, e_new);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_face_split_doc,
    ".. method:: face_split(face, vert_a, vert_b, *, coords=(), use_exist=True, example=None)\n"
    "\n"
    "   Face split with optional intermediate points.\n"
    "\n"
    "   :arg face: The face to cut.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg vert_a: First vertex to cut in the face (face must contain the vert).\n"
    "   :type vert_a: :class:`bmesh.types.BMVert`\n"
    "   :arg vert_b: Second vertex to cut in the face (face must contain the vert).\n"
    "   :type vert_b: :class:`bmesh.types.BMVert`\n"
    "   :arg coords: Optional sequence of 3D points in between *vert_a* and *vert_b*.\n"
    "   :type coords: Sequence[Sequence[float]]\n"
    "   :arg use_exist: .Use an existing edge if it exists (Only used when *coords* argument is "
    "empty or omitted)\n"
    "   :type use_exist: bool\n"
    "   :arg example: Newly created edge will copy settings from this one.\n"
    "   :type example: :class:`bmesh.types.BMEdge`\n"
    "   :return: The newly created face or None on failure.\n"
    "   :rtype: tuple[:class:`bmesh.types.BMFace`, :class:`bmesh.types.BMLoop`]\n");
static PyObject *bpy_bm_utils_face_split(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  static const char *kwlist[] = {
      "face", "vert_a", "vert_b", "coords", "use_exist", "example", nullptr};

  BPy_BMFace *py_face;
  BPy_BMVert *py_vert_a;
  BPy_BMVert *py_vert_b;

  /* optional */
  PyObject *py_coords = nullptr;
  bool edge_exists = true;
  BPy_BMEdge *py_edge_example = nullptr;

  float *coords;
  int ncoords = 0;

  BMesh *bm;
  BMFace *f_new = nullptr;
  BMLoop *l_new = nullptr;
  BMLoop *l_a, *l_b;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O!O!O!|$OO&O!:face_split",
                                   (char **)kwlist,
                                   &BPy_BMFace_Type,
                                   &py_face,
                                   &BPy_BMVert_Type,
                                   &py_vert_a,
                                   &BPy_BMVert_Type,
                                   &py_vert_b,
                                   &py_coords,
                                   PyC_ParseBool,
                                   &edge_exists,
                                   &BPy_BMEdge_Type,
                                   &py_edge_example))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_face);
  BPY_BM_CHECK_OBJ(py_vert_a);
  BPY_BM_CHECK_OBJ(py_vert_b);

  if (py_edge_example) {
    BPY_BM_CHECK_OBJ(py_edge_example);
  }

  /* this doubles for checking that the verts are in the same mesh */
  if ((l_a = BM_face_vert_share_loop(py_face->f, py_vert_a->v)) &&
      (l_b = BM_face_vert_share_loop(py_face->f, py_vert_b->v)))
  {
    /* pass */
  }
  else {
    PyErr_SetString(PyExc_ValueError,
                    "face_split(...): one of the verts passed is not found in the face");
    return nullptr;
  }

  if (py_vert_a->v == py_vert_b->v) {
    PyErr_SetString(PyExc_ValueError, "face_split(...): vert arguments must differ");
    return nullptr;
  }

  if (py_coords) {
    ncoords = mathutils_array_parse_alloc_v(&coords, 3, py_coords, "face_split(...): ");
    if (ncoords == -1) {
      return nullptr;
    }
  }
  else {
    if (BM_loop_is_adjacent(l_a, l_b)) {
      PyErr_SetString(PyExc_ValueError, "face_split(...): verts are adjacent in the face");
      return nullptr;
    }
  }

  /* --- main function body --- */
  bm = py_face->bm;

  if (ncoords) {
    f_new = BM_face_split_n(bm,
                            py_face->f,
                            l_a,
                            l_b,
                            (float (*)[3])coords,
                            ncoords,
                            &l_new,
                            py_edge_example ? py_edge_example->e : nullptr);
    PyMem_Free(coords);
  }
  else {
    f_new = BM_face_split(bm,
                          py_face->f,
                          l_a,
                          l_b,
                          &l_new,
                          py_edge_example ? py_edge_example->e : nullptr,
                          edge_exists);
  }

  if (f_new && l_new) {
    PyObject *ret = PyTuple_New(2);
    PyTuple_SET_ITEMS(
        ret, BPy_BMFace_CreatePyObject(bm, f_new), BPy_BMLoop_CreatePyObject(bm, l_new));
    return ret;
  }

  PyErr_SetString(PyExc_ValueError, "face_split(...): couldn't split the face, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_face_split_edgenet_doc,
    ".. method:: face_split_edgenet(face, edgenet)\n"
    "\n"
    "   Splits a face into any number of regions defined by an edgenet.\n"
    "\n"
    "   :arg face: The face to split.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg face: The face to split.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg edgenet: Sequence of edges.\n"
    "   :type edgenet: Sequence[:class:`bmesh.types.BMEdge`]\n"
    "   :return: The newly created faces.\n"
    "   :rtype: tuple[:class:`bmesh.types.BMFace`, ...]\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      Regions defined by edges need to connect to the face, otherwise they're "
    "ignored as loose edges.\n");
static PyObject *bpy_bm_utils_face_split_edgenet(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const char *error_prefix = "face_split_edgenet(...)";
  static const char *kwlist[] = {"face", "edgenet", nullptr};

  BPy_BMFace *py_face;
  PyObject *edge_seq;

  BMesh *bm;

  bool ok;

  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   "O!O:face_split_edgenet",
                                   (char **)kwlist,
                                   &BPy_BMFace_Type,
                                   &py_face,
                                   &edge_seq))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_face);

  bm = py_face->bm;

  Py_ssize_t edge_array_num;
  BMEdge **edge_array = BPy_BMEdge_PySeq_As_Array(
      &bm, edge_seq, 1, PY_SSIZE_T_MAX, &edge_array_num, true, true, error_prefix);

  if (edge_array == nullptr) {
    return nullptr;
  }

  /* --- main function body --- */
  blender::Vector<BMFace *> face_arr;
  ok = BM_face_split_edgenet(bm, py_face->f, edge_array, edge_array_num, &face_arr);

  PyMem_FREE(edge_array);

  if (ok) {
    PyObject *ret = BPy_BMFace_Array_As_Tuple(bm, face_arr.data(), face_arr.size());
    return ret;
  }

  PyErr_SetString(PyExc_ValueError,
                  "face_split_edgenet(...): couldn't split the face, internal error");
  return nullptr;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_face_join_doc,
    ".. method:: face_join(faces, remove=True)\n"
    "\n"
    "   Joins a sequence of faces.\n"
    "\n"
    "   :arg faces: Sequence of faces.\n"
    "   :type faces: :class:`bmesh.types.BMFace`\n"
    "   :arg remove: Remove the edges and vertices between the faces.\n"
    "   :type remove: bool\n"
    "   :return: The newly created face or None on failure.\n"
    "   :rtype: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bm_utils_face_join(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "face_join(...)";
  BMesh *bm = nullptr;
  PyObject *py_face_array;
  BMFace *f_new;
  bool do_remove = true;

  if (!PyArg_ParseTuple(args, "O|O&:face_join", &py_face_array, PyC_ParseBool, &do_remove)) {
    return nullptr;
  }

  Py_ssize_t face_seq_len = 0;
  BMFace **face_array = BPy_BMFace_PySeq_As_Array(
      &bm, py_face_array, 2, PY_SSIZE_T_MAX, &face_seq_len, true, true, error_prefix);
  if (face_array == nullptr) {
    return nullptr; /* error will be set */
  }

  /* Go ahead and join the face!
   * --------------------------- */
  BMFace *f_double;
  f_new = BM_faces_join(bm, face_array, int(face_seq_len), do_remove, &f_double);
  /* See #BM_faces_join note on callers asserting when `r_double` is non-null. */
  BLI_assert_msg(f_double == nullptr,
                 "Doubled face detected at " AT ". Resulting mesh may be corrupt.");

  PyMem_FREE(face_array);

  if (f_new) {
    return BPy_BMFace_CreatePyObject(bm, f_new);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_face_vert_separate_doc,
    ".. method:: face_vert_separate(face, vert)\n"
    "\n"
    "   Rip a vertex in a face away and add a new vertex.\n"
    "\n"
    "   :arg face: The face to separate.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n"
    "   :arg vert: A vertex in the face to separate.\n"
    "   :type vert: :class:`bmesh.types.BMVert`\n"
    "   :return vert: The newly created vertex or None on failure.\n"
    "   :rtype vert: :class:`bmesh.types.BMVert`\n"
    "\n"
    "   .. note::\n"
    "\n"
    "      This is the same as loop_separate, and has only been added for convenience.\n");
static PyObject *bpy_bm_utils_face_vert_separate(PyObject * /*self*/, PyObject *args)
{
  const char *error_prefix = "face_vert_separate()";
  BPy_BMFace *py_face;
  BPy_BMVert *py_vert;

  BMesh *bm;
  BMLoop *l;
  BMVert *v_old, *v_new;

  if (!PyArg_ParseTuple(
          args, "O!O!:face_vert_separate", &BPy_BMFace_Type, &py_face, &BPy_BMVert_Type, &py_vert))
  {
    return nullptr;
  }

  bm = py_face->bm;

  BPY_BM_CHECK_OBJ(py_face);
  BPY_BM_CHECK_SOURCE_OBJ(bm, error_prefix, py_vert);

  l = BM_face_vert_share_loop(py_face->f, py_vert->v);

  if (l == nullptr) {
    PyErr_Format(PyExc_ValueError, "%s: vertex not found in face", error_prefix);
    return nullptr;
  }

  v_old = l->v;
  v_new = BM_face_loop_separate(bm, l);

  if (v_new != v_old) {
    return BPy_BMVert_CreatePyObject(bm, v_new);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_face_flip_doc,
    ".. method:: face_flip(faces)\n"
    "\n"
    "   Flip the faces direction.\n"
    "\n"
    "   :arg face: Face to flip.\n"
    "   :type face: :class:`bmesh.types.BMFace`\n");
static PyObject *bpy_bm_utils_face_flip(PyObject * /*self*/, BPy_BMFace *value)
{
  if (!BPy_BMFace_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "face_flip(face): BMFace expected, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(value);

  BM_face_normal_flip(value->bm, value->f);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_loop_separate_doc,
    ".. method:: loop_separate(loop)\n"
    "\n"
    "   Rip a vertex in a face away and add a new vertex.\n"
    "\n"
    "   :arg loop: The loop to separate.\n"
    "   :type loop: :class:`bmesh.types.BMLoop`\n"
    "   :return vert: The newly created vertex or None on failure.\n"
    "   :rtype vert: :class:`bmesh.types.BMVert`\n");
static PyObject *bpy_bm_utils_loop_separate(PyObject * /*self*/, BPy_BMLoop *value)
{
  BMesh *bm;
  BMLoop *l;
  BMVert *v_old, *v_new;

  if (!BPy_BMLoop_Check(value)) {
    PyErr_Format(PyExc_TypeError,
                 "loop_separate(loop): BMLoop expected, not '%.200s'",
                 Py_TYPE(value)->tp_name);
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(value);

  bm = value->bm;
  l = value->l;

  v_old = l->v;
  v_new = BM_face_loop_separate(bm, l);

  if (v_new != v_old) {
    return BPy_BMVert_CreatePyObject(bm, v_new);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_bm_utils_uv_select_check_doc,
    ".. method:: uv_select_check(bm, /, *, sync=True, flush=False, contiguous=False)\n"
    "\n"
    "   Split an edge, return the newly created data.\n"
    "\n"
    "   :arg sync: Check the data is properly synchronized between UV's and the underlying mesh. "
    "Failure to synchronize with the mesh selection may cause tools not to behave properly.\n"
    "   :type sync: bool\n"
    "   :arg flush: Check the selection has been properly flushed between elements "
    "(based on the current :class:`BMesh.select_mode`).\n"
    "   :type flush: bool\n"
    "   :arg contiguous: Check connected UV's and edges have a matching selection state.\n"
    "   :type contiguous: bool\n"
    "   :return: An error dictionary or None when there are no errors found.\n"
    "   :rtype: dict[str, int] | None\n");
static PyObject *bpy_bm_utils_uv_select_check(PyObject * /*self*/, PyObject *args, PyObject *kwds)
{
  const char *error_prefix = "uv_select_check(...)";
  BPy_BMesh *py_bm;
  bool check_sync = true;
  bool check_contiguous = false;
  bool check_flush = false;

  static const char *_keywords[] = {
      "",
      "sync",
      "flush",
      "contiguous",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O!" /* `bm` */
      "|$" /* Optional keyword only arguments. */
      "O&" /* `sync` */
      "O&" /* `flush` */
      "O&" /* `contiguous` */
      ":uv_select_check",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &BPy_BMesh_Type,
                                        &py_bm,
                                        PyC_ParseBool,
                                        &check_sync,
                                        PyC_ParseBool,
                                        &check_flush,
                                        PyC_ParseBool,
                                        &check_contiguous))
  {
    return nullptr;
  }

  BPY_BM_CHECK_OBJ(py_bm);

  BMesh *bm = py_bm->bm;
  if (check_sync) {
    if (bpy_bm_check_uv_select_sync_valid(bm, error_prefix) == -1) {
      return nullptr;
    }
  }

  const int cd_loop_uv_offset = check_contiguous ?
                                    CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2) :
                                    -1;
  if (check_contiguous) {
    if (cd_loop_uv_offset == -1) {
      PyErr_SetString(PyExc_ValueError, "contiguous=True for a mesh without UV coordinates");
      return nullptr;
    }
  }

  UVSelectValidateInfo info = {};
  const bool is_valid = BM_mesh_uvselect_is_valid(
      bm, cd_loop_uv_offset, check_sync, check_flush, check_contiguous, &info);
  if (is_valid) {
    Py_RETURN_NONE;
  }

  PyObject *result = PyDict_New();

#define DICT_ADD_INT_MEMBER(info_struct, member) \
  PyDict_SetItemString(result, STRINGIFY(member), PyLong_FromLong(info_struct.member))

  {
    UVSelectValidateInfo_Sync &info_sub = info.sync;
    DICT_ADD_INT_MEMBER(info_sub, count_uv_vert_any_selected_with_vert_unselected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_vert_none_selected_with_vert_selected);

    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_any_selected_with_edge_unselected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_none_selected_with_edge_selected);
  }

  if (check_flush) {
    UVSelectValidateInfo_Flush &info_sub = info.flush;
    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_selected_with_any_verts_unselected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_unselected_with_all_verts_selected);

    DICT_ADD_INT_MEMBER(info_sub, count_uv_face_selected_with_any_verts_unselected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_face_unselected_with_all_verts_selected);

    DICT_ADD_INT_MEMBER(info_sub, count_uv_face_selected_with_any_edges_unselected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_face_unselected_with_all_edges_selected);
  }

  if (check_contiguous) {
    UVSelectValidateInfo_Contiguous &info_sub = info.contiguous;
    DICT_ADD_INT_MEMBER(info_sub, count_uv_vert_non_contiguous_selected);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_non_contiguous_selected);
  }

  if (check_flush && check_contiguous) {
    UVSelectValidateInfo_FlushAndContiguous &info_sub = info.flush_contiguous;
    DICT_ADD_INT_MEMBER(info_sub, count_uv_vert_isolated_in_edge_or_face_mode);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_vert_isolated_in_face_mode);
    DICT_ADD_INT_MEMBER(info_sub, count_uv_edge_isolated_in_face_mode);
  }

#undef DICT_ADD_INT_MEMBER

  return result;
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

static PyMethodDef BPy_BM_utils_methods[] = {
    {"vert_collapse_edge",
     (PyCFunction)bpy_bm_utils_vert_collapse_edge,
     METH_VARARGS,
     bpy_bm_utils_vert_collapse_edge_doc},
    {"vert_collapse_faces",
     (PyCFunction)bpy_bm_utils_vert_collapse_faces,
     METH_VARARGS,
     bpy_bm_utils_vert_collapse_faces_doc},
    {"vert_dissolve",
     (PyCFunction)bpy_bm_utils_vert_dissolve,
     METH_VARARGS,
     bpy_bm_utils_vert_dissolve_doc}, /* could use METH_O */
    {"vert_splice",
     (PyCFunction)bpy_bm_utils_vert_splice,
     METH_VARARGS,
     bpy_bm_utils_vert_splice_doc},
    {"vert_separate",
     (PyCFunction)bpy_bm_utils_vert_separate,
     METH_VARARGS,
     bpy_bm_utils_vert_separate_doc},
    {"edge_split",
     (PyCFunction)bpy_bm_utils_edge_split,
     METH_VARARGS,
     bpy_bm_utils_edge_split_doc},
    {"edge_rotate",
     (PyCFunction)bpy_bm_utils_edge_rotate,
     METH_VARARGS,
     bpy_bm_utils_edge_rotate_doc},
    {"face_split",
     (PyCFunction)bpy_bm_utils_face_split,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bm_utils_face_split_doc},
    {"face_split_edgenet",
     (PyCFunction)bpy_bm_utils_face_split_edgenet,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bm_utils_face_split_edgenet_doc},
    {"face_join", (PyCFunction)bpy_bm_utils_face_join, METH_VARARGS, bpy_bm_utils_face_join_doc},
    {"face_vert_separate",
     (PyCFunction)bpy_bm_utils_face_vert_separate,
     METH_VARARGS,
     bpy_bm_utils_face_vert_separate_doc},
    {"face_flip", (PyCFunction)bpy_bm_utils_face_flip, METH_O, bpy_bm_utils_face_flip_doc},
    {"loop_separate",
     (PyCFunction)bpy_bm_utils_loop_separate,
     METH_O,
     bpy_bm_utils_loop_separate_doc},
    {"uv_select_check",
     (PyCFunction)bpy_bm_utils_uv_select_check,
     METH_VARARGS | METH_KEYWORDS,
     bpy_bm_utils_uv_select_check_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

PyDoc_STRVAR(
    /* Wrap. */
    BPy_BM_utils_doc,
    "This module provides access to blenders bmesh data structures.");
static PyModuleDef BPy_BM_utils_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bmesh.utils",
    /*m_doc*/ BPy_BM_utils_doc,
    /*m_size*/ 0,
    /*m_methods*/ BPy_BM_utils_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPyInit_bmesh_utils()
{
  PyObject *submodule;

  submodule = PyModule_Create(&BPy_BM_utils_module_def);

  return submodule;
}
