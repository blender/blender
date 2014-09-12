/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bmesh_py_utils.c
 *  \ingroup pybmesh
 *
 * This file defines the 'bmesh.utils' module.
 * Utility functions for operating on 'bmesh.types'
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "../mathutils/mathutils.h"

#include "bmesh.h"
#include "bmesh_py_types.h"
#include "bmesh_py_utils.h" /* own include */


PyDoc_STRVAR(bpy_bm_utils_vert_collapse_edge_doc,
".. method:: vert_collapse_edge(vert, edge)\n"
"\n"
"   Collapse a vertex into an edge.\n"
"\n"
"   :arg vert: The vert that will be collapsed.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :arg edge: The edge to collapse into.\n"
"   :type edge: :class:`bmesh.types.BMEdge`\n"
"   :return: The resulting edge from the collapse operation.\n"
"   :rtype: :class:`bmesh.types.BMEdge`\n"
);
static PyObject *bpy_bm_utils_vert_collapse_edge(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMEdge *py_edge;
	BPy_BMVert *py_vert;

	BMesh *bm;
	BMEdge *e_new = NULL;

	if (!PyArg_ParseTuple(args, "O!O!:vert_collapse_edge",
	                      &BPy_BMVert_Type, &py_vert,
	                      &BPy_BMEdge_Type, &py_edge))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_edge);
	BPY_BM_CHECK_OBJ(py_vert);

	/* this doubles for checking that the verts are in the same mesh */
	if (!(py_edge->e->v1 == py_vert->v ||
	      py_edge->e->v2 == py_vert->v))
	{
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_edge(vert, edge): the vertex is not found in the edge");
		return NULL;
	}

	if (BM_vert_edge_count(py_vert->v) > 2) {
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_edge(vert, edge): vert has more than 2 connected edges");
		return NULL;
	}

	bm = py_edge->bm;

	e_new = BM_vert_collapse_edge(bm, py_edge->e, py_vert->v, true, true);

	if (e_new) {
		return BPy_BMEdge_CreatePyObject(bm, e_new);
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_edge(vert, edge): no new edge created, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bm_utils_vert_collapse_faces_doc,
".. method:: vert_collapse_faces(vert, edge, fac, join_faces)\n"
"\n"
"   Collapses a vertex that has only two manifold edges onto a vertex it shares an edge with.\n"
"\n"
"   :arg vert: The vert that will be collapsed.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :arg edge: The edge to collapse into.\n"
"   :type edge: :class:`bmesh.types.BMEdge`\n"
"   :arg fac: The factor to use when merging customdata [0 - 1].\n"
"   :type fac: float\n"
"   :return: The resulting edge from the collapse operation.\n"
"   :rtype: :class:`bmesh.types.BMEdge`\n"
);
static PyObject *bpy_bm_utils_vert_collapse_faces(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMEdge *py_edge;
	BPy_BMVert *py_vert;

	float fac;
	int do_join_faces;

	BMesh *bm;
	BMEdge *e_new = NULL;

	if (!PyArg_ParseTuple(args, "O!O!fi:vert_collapse_faces",
	                      &BPy_BMVert_Type, &py_vert,
	                      &BPy_BMEdge_Type, &py_edge,
	                      &fac, &do_join_faces))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_edge);
	BPY_BM_CHECK_OBJ(py_vert);

	/* this doubles for checking that the verts are in the same mesh */
	if (!(py_edge->e->v1 == py_vert->v ||
	      py_edge->e->v2 == py_vert->v))
	{
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_faces(vert, edge): the vertex is not found in the edge");
		return NULL;
	}

	if (BM_vert_edge_count(py_vert->v) > 2) {
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_faces(vert, edge): vert has more than 2 connected edges");
		return NULL;
	}

	bm = py_edge->bm;

	e_new = BM_vert_collapse_faces(bm, py_edge->e, py_vert->v, CLAMPIS(fac, 0.0f, 1.0f), true, do_join_faces, true);

	if (e_new) {
		return BPy_BMEdge_CreatePyObject(bm, e_new);
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "vert_collapse_faces(vert, edge): no new edge created, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bm_utils_vert_dissolve_doc,
".. method:: vert_dissolve(vert)\n"
"\n"
"   Dissolve this vertex (will be removed).\n"
"\n"
"   :arg vert: The vert to be dissolved.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :return: True when the vertex dissolve is successful.\n"
"   :rtype: boolean\n"
);
static PyObject *bpy_bm_utils_vert_dissolve(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMVert *py_vert;

	BMesh *bm;

	if (!PyArg_ParseTuple(args, "O!:vert_dissolve",
	                      &BPy_BMVert_Type, &py_vert))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_vert);

	bm = py_vert->bm;

	return PyBool_FromLong((BM_vert_dissolve(bm, py_vert->v)));
}

PyDoc_STRVAR(bpy_bm_utils_vert_splice_doc,
".. method:: vert_splice(vert, vert_target)\n"
"\n"
"   Splice vert into vert_target.\n"
"\n"
"   :arg vert: The vertex to be removed.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :arg vert_target: The vertex to use.\n"
"   :type vert_target: :class:`bmesh.types.BMVert`\n"
"\n"
"   .. note:: The verts mustn't share an edge or face.\n"
);
static PyObject *bpy_bm_utils_vert_splice(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMVert *py_vert;
	BPy_BMVert *py_vert_target;

	BMesh *bm;

	bool ok;

	if (!PyArg_ParseTuple(args, "O!O!:vert_splice",
	                      &BPy_BMVert_Type, &py_vert,
	                      &BPy_BMVert_Type, &py_vert_target))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_vert);
	BPY_BM_CHECK_OBJ(py_vert_target);

	bm = py_vert->bm;
	BPY_BM_CHECK_SOURCE_OBJ(bm, "vert_splice", py_vert_target);

	if (py_vert->v == py_vert_target->v) {
		PyErr_SetString(PyExc_ValueError,
		                "vert_splice(...): vert arguments match");
		return NULL;
	}

	if (BM_edge_exists(py_vert->v, py_vert_target->v)) {
		PyErr_SetString(PyExc_ValueError,
		                "vert_splice(...): verts can't share an edge");
		return NULL;
	}

	if (BM_vert_pair_share_face_check(py_vert->v, py_vert_target->v)) {
		PyErr_SetString(PyExc_ValueError,
		                "vert_splice(...): verts can't share a face");
		return NULL;
	}

	/* should always succeed */
	ok = BM_vert_splice(bm, py_vert->v, py_vert_target->v);
	BLI_assert(ok == true);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bm_utils_vert_separate_doc,
".. method:: vert_separate(vert, edges)\n"
"\n"
"   Separate this vertex at every edge.\n"
"\n"
"   :arg vert: The vert to be separated.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :arg edges: The edges to separated.\n"
"   :type edges: :class:`bmesh.types.BMEdge`\n"
"   :return: The newly separated verts (including the vertex passed).\n"
"   :rtype: tuple of :class:`bmesh.types.BMVert`\n"
);
static PyObject *bpy_bm_utils_vert_separate(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMVert *py_vert;
	PyObject *edge_seq;

	BMesh *bm;
	BMVert **elem;
	int elem_len;

	/* edges to split */
	BMEdge **edge_array;
	Py_ssize_t edge_array_len;

	PyObject *ret;


	if (!PyArg_ParseTuple(args, "O!O:vert_separate",
	                      &BPy_BMVert_Type, &py_vert,
	                      &edge_seq))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_vert);

	bm = py_vert->bm;

	edge_array = BPy_BMElem_PySeq_As_Array(&bm, edge_seq, 0, PY_SSIZE_T_MAX,
	                                       &edge_array_len, BM_EDGE,
	                                       true, true, "vert_separate(...)");

	if (edge_array == NULL) {
		return NULL;
	}

	BM_vert_separate(bm, py_vert->v, &elem, &elem_len, edge_array, edge_array_len);
	/* return collected verts */
	ret = BPy_BMVert_Array_As_Tuple(bm, elem, elem_len);
	MEM_freeN(elem);

	PyMem_FREE(edge_array);

	return ret;
}


PyDoc_STRVAR(bpy_bm_utils_edge_split_doc,
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
"   :rtype: tuple\n"
);
static PyObject *bpy_bm_utils_edge_split(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMEdge *py_edge;
	BPy_BMVert *py_vert;
	float fac;

	BMesh *bm;
	BMVert *v_new = NULL;
	BMEdge *e_new = NULL;

	if (!PyArg_ParseTuple(args, "O!O!f:edge_split",
	                      &BPy_BMEdge_Type, &py_edge,
	                      &BPy_BMVert_Type, &py_vert,
	                      &fac))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_edge);
	BPY_BM_CHECK_OBJ(py_vert);

	/* this doubles for checking that the verts are in the same mesh */
	if (!(py_edge->e->v1 == py_vert->v ||
	      py_edge->e->v2 == py_vert->v))
	{
		PyErr_SetString(PyExc_ValueError,
		                "edge_split(edge, vert): the vertex is not found in the edge");
		return NULL;
	}

	bm = py_edge->bm;

	v_new = BM_edge_split(bm, py_edge->e, py_vert->v, &e_new, CLAMPIS(fac, 0.0f, 1.0f));

	if (v_new && e_new) {
		PyObject *ret = PyTuple_New(2);
		PyTuple_SET_ITEM(ret, 0, BPy_BMEdge_CreatePyObject(bm, e_new));
		PyTuple_SET_ITEM(ret, 1, BPy_BMVert_CreatePyObject(bm, v_new));
		return ret;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "edge_split(edge, vert): couldn't split the edge, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bm_utils_edge_rotate_doc,
".. method:: edge_rotate(edge, ccw=False)\n"
"\n"
"   Rotate the edge and return the newly created edge.\n"
"   If rotating the edge fails, None will be returned.\n"
"\n"
"   :arg edge: The edge to rotate.\n"
"   :type edge: :class:`bmesh.types.BMEdge`\n"
"   :arg ccw: When True the edge will be rotated counter clockwise.\n"
"   :type ccw: boolean\n"
"   :return: The newly rotated edge.\n"
"   :rtype: :class:`bmesh.types.BMEdge`\n"
);
static PyObject *bpy_bm_utils_edge_rotate(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMEdge *py_edge;
	int do_ccw = false;

	BMesh *bm;
	BMEdge *e_new = NULL;

	if (!PyArg_ParseTuple(args, "O!|i:edge_rotate",
	                      &BPy_BMEdge_Type, &py_edge,
	                      &do_ccw))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_edge);

	bm = py_edge->bm;

	e_new = BM_edge_rotate(bm, py_edge->e, do_ccw, 0);

	if (e_new) {
		return BPy_BMEdge_CreatePyObject(bm, e_new);
	}
	else {
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bm_utils_face_split_doc,
".. method:: face_split(face, vert_a, vert_b, coords=(), use_exist=True, example=None)\n"
"\n"
"   Face split with optional intermediate points.\n"
"\n"
"   :arg face: The face to cut.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
"   :arg vert_a: First vertex to cut in the face (face must contain the vert).\n"
"   :type vert_a: :class:`bmesh.types.BMVert`\n"
"   :arg vert_b: Second vertex to cut in the face (face must contain the vert).\n"
"   :type vert_b: :class:`bmesh.types.BMVert`\n"
"   :arg coords: Optional argument to define points inbetween *vert_a* and *vert_b*.\n"
"   :type coords: sequence of float triplets\n"
"   :arg use_exist: .Use an existing edge if it exists (Only used when *coords* argument is empty or omitted)\n"
"   :type use_exist: boolean\n"
"   :arg example: Newly created edge will copy settings from this one.\n"
"   :type example: :class:`bmesh.types.BMEdge`\n"
"   :return: The newly created face or None on failure.\n"
"   :rtype: (:class:`bmesh.types.BMFace`, :class:`bmesh.types.BMLoop`) pair\n"
);
static PyObject *bpy_bm_utils_face_split(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"face", "vert_a", "vert_b",
	                               "coords", "use_exist", "example", NULL};

	BPy_BMFace *py_face;
	BPy_BMVert *py_vert_a;
	BPy_BMVert *py_vert_b;

	/* optional */
	PyObject *py_coords = NULL;
	int edge_exists = true;
	BPy_BMEdge *py_edge_example = NULL;

	float *coords;
	int ncoords = 0;

	BMesh *bm;
	BMFace *f_new = NULL;
	BMLoop *l_new = NULL;
	BMLoop *l_a, *l_b;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "O!O!O!|OiO!:face_split", (char **)kwlist,
	                                 &BPy_BMFace_Type, &py_face,
	                                 &BPy_BMVert_Type, &py_vert_a,
	                                 &BPy_BMVert_Type, &py_vert_b,
	                                 &py_coords,
	                                 &edge_exists,
	                                 &BPy_BMEdge_Type, &py_edge_example))
	{
		return NULL;
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
		return NULL;
	}

	if (py_vert_a->v == py_vert_b->v) {
		PyErr_SetString(PyExc_ValueError,
		                "face_split(...): vert arguments must differ");
		return NULL;
	}

	if (py_coords) {
		ncoords = mathutils_array_parse_alloc_v(&coords, 3, py_coords, "face_split(...): ");
		if (ncoords == -1) {
			return NULL;
		}
	}

	/* --- main function body --- */
	bm = py_face->bm;

	if (ncoords) {
		f_new = BM_face_split_n(bm, py_face->f,
		                        l_a, l_b,
		                        (float (*)[3])coords, ncoords,
		                        &l_new, py_edge_example ? py_edge_example->e : NULL);
		PyMem_Free(coords);
	}
	else {
		f_new = BM_face_split(bm, py_face->f,
		                      l_a, l_b,
		                      &l_new, py_edge_example ? py_edge_example->e : NULL, edge_exists);
	}

	if (f_new && l_new) {
		PyObject *ret = PyTuple_New(2);
		PyTuple_SET_ITEM(ret, 0, BPy_BMFace_CreatePyObject(bm, f_new));
		PyTuple_SET_ITEM(ret, 1, BPy_BMLoop_CreatePyObject(bm, l_new));
		return ret;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "face_split(...): couldn't split the face, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bm_utils_face_split_edgenet_doc,
".. method:: face_split_edgenet(face, edgenet)\n"
"\n"
"   Splits a face into any number of regions defined by an edgenet.\n"
"\n"
"   :arg face: The face to split.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
"   :arg face: The face to split.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
"   :arg edgenet: Sequence of edges.\n"
"   :type edgenet: :class:`bmesh.types.BMEdge`\n"
"   :return: The newly created faces.\n"
"   :rtype: tuple of (:class:`bmesh.types.BMFace`)\n"
);
static PyObject *bpy_bm_utils_face_split_edgenet(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"face", "edgenet", NULL};

	BPy_BMFace *py_face;
	PyObject *edge_seq;

	BMEdge **edge_array;
	Py_ssize_t edge_array_len;

	BMesh *bm;

	BMFace **face_arr;
	int face_arr_len;
	bool ok;


	if (!PyArg_ParseTupleAndKeywords(args, kw, "O!O:face_split_edgenet", (char **)kwlist,
	                                 &BPy_BMFace_Type, &py_face,
	                                 &edge_seq))
	{
		return NULL;
	}

	BPY_BM_CHECK_OBJ(py_face);

	bm = py_face->bm;

	edge_array = BPy_BMElem_PySeq_As_Array(&bm, edge_seq, 1, PY_SSIZE_T_MAX,
	                                       &edge_array_len, BM_EDGE,
	                                       true, true, "face_split_edgenet(...)");

	if (edge_array == NULL) {
		return NULL;
	}

	/* --- main function body --- */

	ok = BM_face_split_edgenet(bm, py_face->f, edge_array, edge_array_len,
	                           &face_arr, &face_arr_len);

	PyMem_FREE(edge_array);

	if (ok) {
		PyObject *ret = BPy_BMFace_Array_As_Tuple(bm, face_arr, face_arr_len);
		if (face_arr) {
			MEM_freeN(face_arr);
		}
		return ret;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "face_split_edgenet(...): couldn't split the face, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bm_utils_face_join_doc,
".. method:: face_join(faces, remove=True)\n"
"\n"
"   Joins a sequence of faces.\n"
"\n"
"   :arg faces: Sequence of faces.\n"
"   :type faces: :class:`bmesh.types.BMFace`\n"
"   :arg remove: Remove the edges and vertices between the faces.\n"
"   :type remove: boolean\n"
"   :return: The newly created face or None on failure.\n"
"   :rtype: :class:`bmesh.types.BMFace`\n"
);
static PyObject *bpy_bm_utils_face_join(PyObject *UNUSED(self), PyObject *args)
{
	BMesh *bm = NULL;
	PyObject *py_face_array;
	BMFace **face_array;
	Py_ssize_t face_seq_len = 0;
	BMFace *f_new;
	int do_remove = true;

	if (!PyArg_ParseTuple(args, "O|i:face_join", &py_face_array, &do_remove)) {
		return NULL;
	}

	face_array = BPy_BMElem_PySeq_As_Array(&bm, py_face_array, 2, PY_SSIZE_T_MAX,
	                                       &face_seq_len, BM_FACE,
	                                       true, true, "face_join(...)");

	if (face_array == NULL) {
		return NULL; /* error will be set */
	}

	/* Go ahead and join the face!
	 * --------------------------- */
	f_new = BM_faces_join(bm, face_array, (int)face_seq_len, do_remove);

	PyMem_FREE(face_array);

	if (f_new) {
		return BPy_BMFace_CreatePyObject(bm, f_new);
	}
	else {
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bm_utils_face_vert_separate_doc,
".. method:: face_vert_separate(face, vert)\n"
"\n"
"   Rip a vertex in a face away and add a new vertex.\n"
"\n"
"   :arg face: The face to separate.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
"   :arg vert: A vertex in the face to separate.\n"
"   :type vert: :class:`bmesh.types.BMVert`\n"
"   :return vert: The newly created vertex or None of failure.\n"
"   :rtype vert: :class:`bmesh.types.BMVert`\n"
"\n"
"   .. note::\n"
"\n"
"      This is the same as loop_separate, and has only been added for convenience.\n"
);
static PyObject *bpy_bm_utils_face_vert_separate(PyObject *UNUSED(self), PyObject *args)
{
	BPy_BMFace *py_face;
	BPy_BMVert *py_vert;

	BMesh *bm;
	BMLoop *l;
	BMVert *v_old, *v_new;

	if (!PyArg_ParseTuple(args, "O!O!:face_vert_separate",
	                      &BPy_BMFace_Type, &py_face,
	                      &BPy_BMVert_Type, &py_vert))
	{
		return NULL;
	}

	bm = py_face->bm;

	BPY_BM_CHECK_OBJ(py_face);
	BPY_BM_CHECK_SOURCE_OBJ(bm, "face_vert_separate()", py_vert);

	l = BM_face_vert_share_loop(py_face->f, py_vert->v);

	if (l == NULL) {
		PyErr_SetString(PyExc_ValueError,
		                "vertex not found in face");
		return NULL;
	}

	v_old = l->v;
	v_new = BM_face_loop_separate(bm, l);

	if (v_new != v_old) {
		return BPy_BMVert_CreatePyObject(bm, v_new);
	}
	else {
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bm_utils_face_flip_doc,
".. method:: face_flip(faces)\n"
"\n"
"   Flip the faces direction.\n"
"\n"
"   :arg face: Face to flip.\n"
"   :type face: :class:`bmesh.types.BMFace`\n"
);
static PyObject *bpy_bm_utils_face_flip(PyObject *UNUSED(self), BPy_BMFace *value)
{
	if (!BPy_BMFace_Check(value)) {
		PyErr_Format(PyExc_TypeError,
		             "face_flip(face): BMFace expected, not '%.200s'",
		             Py_TYPE(value)->tp_name);
		return NULL;
	}

	BPY_BM_CHECK_OBJ(value);

	BM_face_normal_flip(value->bm, value->f);

	Py_RETURN_NONE;
}



PyDoc_STRVAR(bpy_bm_utils_loop_separate_doc,
".. method:: loop_separate(loop)\n"
"\n"
"   Rip a vertex in a face away and add a new vertex.\n"
"\n"
"   :arg loop: The to separate.\n"
"   :type loop: :class:`bmesh.types.BMFace`\n"
"   :return vert: The newly created vertex or None of failure.\n"
"   :rtype vert: :class:`bmesh.types.BMVert`\n"
);
static PyObject *bpy_bm_utils_loop_separate(PyObject *UNUSED(self), BPy_BMLoop *value)
{
	BMesh *bm;
	BMLoop *l;
	BMVert *v_old, *v_new;

	if (!BPy_BMLoop_Check(value)) {
		PyErr_Format(PyExc_TypeError,
		             "loop_separate(loop): BMLoop expected, not '%.200s'",
		             Py_TYPE(value)->tp_name);
		return NULL;
	}

	BPY_BM_CHECK_OBJ(value);

	bm = value->bm;
	l = value->l;

	v_old = l->v;
	v_new = BM_face_loop_separate(bm, l);

	if (v_new != v_old) {
		return BPy_BMVert_CreatePyObject(bm, v_new);
	}
	else {
		Py_RETURN_NONE;
	}
}


static struct PyMethodDef BPy_BM_utils_methods[] = {
	{"vert_collapse_edge",  (PyCFunction)bpy_bm_utils_vert_collapse_edge,  METH_VARARGS, bpy_bm_utils_vert_collapse_edge_doc},
	{"vert_collapse_faces", (PyCFunction)bpy_bm_utils_vert_collapse_faces, METH_VARARGS, bpy_bm_utils_vert_collapse_faces_doc},
	{"vert_dissolve",       (PyCFunction)bpy_bm_utils_vert_dissolve,       METH_VARARGS, bpy_bm_utils_vert_dissolve_doc}, /* could use METH_O */
	{"vert_splice",         (PyCFunction)bpy_bm_utils_vert_splice,         METH_VARARGS, bpy_bm_utils_vert_splice_doc},
	{"vert_separate",       (PyCFunction)bpy_bm_utils_vert_separate,       METH_VARARGS, bpy_bm_utils_vert_separate_doc},
	{"edge_split",          (PyCFunction)bpy_bm_utils_edge_split,          METH_VARARGS, bpy_bm_utils_edge_split_doc},
	{"edge_rotate",         (PyCFunction)bpy_bm_utils_edge_rotate,         METH_VARARGS, bpy_bm_utils_edge_rotate_doc},
	{"face_split",          (PyCFunction)bpy_bm_utils_face_split,          METH_VARARGS | METH_KEYWORDS, bpy_bm_utils_face_split_doc},
	{"face_split_edgenet",  (PyCFunction)bpy_bm_utils_face_split_edgenet,  METH_VARARGS | METH_KEYWORDS, bpy_bm_utils_face_split_edgenet_doc},
	{"face_join",           (PyCFunction)bpy_bm_utils_face_join,           METH_VARARGS, bpy_bm_utils_face_join_doc},
	{"face_vert_separate",  (PyCFunction)bpy_bm_utils_face_vert_separate,  METH_VARARGS, bpy_bm_utils_face_vert_separate_doc},
	{"face_flip",           (PyCFunction)bpy_bm_utils_face_flip,           METH_O,       bpy_bm_utils_face_flip_doc},
	{"loop_separate",       (PyCFunction)bpy_bm_utils_loop_separate,       METH_O,       bpy_bm_utils_loop_separate_doc},
	{NULL, NULL, 0, NULL}
};


PyDoc_STRVAR(BPy_BM_utils_doc,
"This module provides access to blenders bmesh data structures."
);
static struct PyModuleDef BPy_BM_utils_module_def = {
	PyModuleDef_HEAD_INIT,
	"bmesh.utils",  /* m_name */
	BPy_BM_utils_doc,  /* m_doc */
	0,  /* m_size */
	BPy_BM_utils_methods,  /* m_methods */
	NULL,  /* m_reload */
	NULL,  /* m_traverse */
	NULL,  /* m_clear */
	NULL,  /* m_free */
};


PyObject *BPyInit_bmesh_utils(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_utils_module_def);

	return submodule;
}
