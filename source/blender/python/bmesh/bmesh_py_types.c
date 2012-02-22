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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/bmesh/bme_types.c
 *  \ingroup pybmesh
 */

#include <Python.h>

#include "BLI_math.h"

#include "bmesh.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_types.h" /* own include */

/* Common Flags
 * ************ */

/* scene does not use BM_* flags. */
PyC_FlagSet bpy_bm_scene_vert_edge_face_flags[] = {
	{1, "VERT"},
	{2, "EDGE"},
	{4, "FACE"},
	{0, NULL}
};

PyC_FlagSet bpy_bm_htype_vert_edge_face_flags[] = {
	{BM_VERT, "VERT"},
	{BM_EDGE, "EDGE"},
	{BM_FACE, "FACE"},
	{0, NULL}
};

PyC_FlagSet bpy_bm_htype_all_flags[] = {
	{BM_VERT, "VERT"},
	{BM_LOOP, "EDGE"},
	{BM_FACE, "FACE"},
	{BM_LOOP, "LOOP"},
	{0, NULL}
};

PyC_FlagSet bpy_bm_hflag_all_flags[] = {
	{BM_ELEM_SELECT,  "SELECT"},
	{BM_ELEM_HIDDEN,  "HIDE"},
	{BM_ELEM_SEAM,    "SEAM"},
	{BM_ELEM_SMOOTH,  "SMOOTH"},
	{BM_ELEM_TAG,     "TAG"},
	{0, NULL}
};

/* py-type definitions
 * ******************* */

/* getseters
 * ========= */


/* bmesh elems
 * ----------- */

PyDoc_STRVAR(bpy_bm_elem_select_doc, "Selected state of this element (boolean)");
PyDoc_STRVAR(bpy_bm_elem_hide_doc, "Hidden state of this element (boolean)");
PyDoc_STRVAR(bpy_bm_elem_tag_doc, "Tag state of this element (boolean)");
PyDoc_STRVAR(bpy_bm_elem_smooth_doc, "Smooth state of this element (boolean)");
PyDoc_STRVAR(bpy_bm_elem_index_doc, "Index of this element");


static PyObject *bpy_bm_elem_hflag_get(BPy_BMElem *self, void *flag)
{
	const char hflag = (char)GET_INT_FROM_POINTER(flag);

	BPY_BM_CHECK_OBJ(self);

	return PyBool_FromLong(BM_elem_flag_test(self->ele, hflag));
}

static int bpy_bm_elem_hflag_set(BPy_BMElem *self, PyObject *value, void *flag)
{
	const char hflag = (char)GET_INT_FROM_POINTER(flag);
	int param;

	BPY_BM_CHECK_INT(self);

	param = PyLong_AsLong(value);

	if (param == TRUE) {
		BM_elem_flag_enable(self->ele, hflag);
		return 0;
	}
	else if (param == FALSE) {
		BM_elem_flag_disable(self->ele, hflag);
		return 0;
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "expected True/False or 0/1, not %.200s",
		             Py_TYPE(value)->tp_name);
		return -1;
	}
}

static PyObject *bpy_bm_elem_index_get(BPy_BMElem *self, void *UNUSED(flag))
{
	BPY_BM_CHECK_OBJ(self);

	return PyLong_FromLong(BM_elem_index_get(self->ele));
}

static int bpy_bm_elem_index_set(BPy_BMElem *self, PyObject *value, void *UNUSED(flag))
{
	int param;

	BPY_BM_CHECK_INT(self);

	param = PyLong_AsLong(value);

	if (param == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "expected an int type");
		return -1;
	}
	else {
		BM_elem_index_set(self->ele, param);

		/* when setting the index assume its set invalid */
		if (self->ele->htype & (BM_VERT | BM_EDGE | BM_FACE)) {
			self->bm->elem_index_dirty |= self->ele->htype;
		}

		return 0;
	}
}

/* type spesific get/sets
 * ---------------------- */


/* Mesh
 * ^^^^ */

PyDoc_STRVAR(bpy_bmesh_verts_doc,
"The :class:`bme.types.BMVertSeq` object this mesh"
);
static PyObject *bpy_bmesh_verts_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMVertSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmesh_edges_doc,
"The :class:`bme.types.BMEdgeSeq` object this mesh"
);
static PyObject *bpy_bmesh_edges_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMEdgeSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmesh_faces_doc,
"The :class:`bme.types.BMFaceSeq` object this mesh"
);
static PyObject *bpy_bmesh_faces_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMFaceSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmesh_select_mode_doc,
"The selection mode for this mesh"
);
static PyObject *bpy_bmesh_select_mode_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);

	return PyC_FlagSet_FromBitfield(bpy_bm_scene_vert_edge_face_flags, self->bm->selectmode);
}

static int bpy_bmesh_select_mode_set(BPy_BMesh *self, PyObject *value)
{
	int flag = 0;
	BPY_BM_CHECK_INT(self);

	if (PyC_FlagSet_ToBitfield(bpy_bm_scene_vert_edge_face_flags, value, &flag, "bm.select_mode") == -1) {
		return -1;
	}
	else if (flag == 0) {
		PyErr_SetString(PyExc_TypeError, "bm.select_mode: cant assignt an empty value");
		return -1;
	}
	else {
		self->bm->selectmode = flag;
		return 0;
	}
}


/* Vert
 * ^^^^ */

PyDoc_STRVAR(bpy_bmvert_co_doc,
"The coordinates for this vertex"
);
static PyObject *bpy_bmvert_co_get(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);
	return Vector_CreatePyObject(self->v->co, 3, Py_WRAP, NULL);
}

static int bpy_bmvert_co_set(BPy_BMVert *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (mathutils_array_parse(self->v->co, 3, 3, value, "BMVert.co") != -1) {
		return 0;
	}
	else {
		return -1;
	}
}

PyDoc_STRVAR(bpy_bmvert_normal_doc,
"The normal for this vertex"
);
static PyObject *bpy_bmvert_normal_get(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);
	return Vector_CreatePyObject(self->v->no, 3, Py_WRAP, NULL);
}

static int bpy_bmvert_normal_set(BPy_BMVert *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (mathutils_array_parse(self->v->no, 3, 3, value, "BMVert.normal") != -1) {
		return 0;
	}
	else {
		return -1;
	}
}

/* Face
 * ^^^^ */

PyDoc_STRVAR(bpy_bmface_normal_doc,
"The normal for this face"
);
static PyObject *bpy_bmface_normal_get(BPy_BMFace *self)
{
	BPY_BM_CHECK_OBJ(self);
	return Vector_CreatePyObject(self->f->no, 3, Py_WRAP, NULL);
}

static int bpy_bmface_normal_set(BPy_BMFace *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (mathutils_array_parse(self->f->no, 3, 3, value, "BMFace.normal") != -1) {
		return 0;
	}
	else {
		return -1;
	}
}

static PyGetSetDef bpy_bmesh_getseters[] = {
	{(char *)"verts", (getter)bpy_bmesh_verts_get, (setter)NULL, (char *)bpy_bmesh_verts_doc, NULL},
	{(char *)"edges", (getter)bpy_bmesh_edges_get, (setter)NULL, (char *)bpy_bmesh_edges_doc, NULL},
	{(char *)"faces", (getter)bpy_bmesh_faces_get, (setter)NULL, (char *)bpy_bmesh_faces_doc, NULL},
    {(char *)"select_mode", (getter)bpy_bmesh_select_mode_get, (setter)bpy_bmesh_select_mode_set, (char *)bpy_bmesh_select_mode_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmvert_getseters[] = {
	/* generic */
	{(char *)"select", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_select_doc, (void *)BM_ELEM_SELECT},
	{(char *)"hide",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_hide_doc,   (void *)BM_ELEM_SELECT},
	{(char *)"tag",    (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_tag_doc,    (void *)BM_ELEM_TAG},
	{(char *)"index",  (getter)bpy_bm_elem_index_get, (setter)bpy_bm_elem_index_set, (char *)bpy_bm_elem_index_doc,  NULL},

	{(char *)"co",     (getter)bpy_bmvert_co_get,     (setter)bpy_bmvert_co_set,     (char *)bpy_bmvert_co_doc, NULL},
	{(char *)"normal", (getter)bpy_bmvert_normal_get, (setter)bpy_bmvert_normal_set, (char *)bpy_bmvert_normal_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmedge_getseters[] = {
	/* generic */
	{(char *)"select", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_select_doc, (void *)BM_ELEM_SELECT},
	{(char *)"hide",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_hide_doc,   (void *)BM_ELEM_SELECT},
	{(char *)"tag",    (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_tag_doc,    (void *)BM_ELEM_TAG},
	{(char *)"index",  (getter)bpy_bm_elem_index_get, (setter)bpy_bm_elem_index_set, (char *)bpy_bm_elem_index_doc,  NULL},

	{(char *)"smooth", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_smooth_doc, (void *)BM_ELEM_SMOOTH},
	{(char *)"seam",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_smooth_doc, (void *)BM_ELEM_SEAM},

	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmface_getseters[] = {
	/* generic */
	{(char *)"select", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_select_doc, (void *)BM_ELEM_SELECT},
	{(char *)"hide",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_hide_doc,   (void *)BM_ELEM_SELECT},
	{(char *)"tag",    (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_tag_doc,    (void *)BM_ELEM_TAG},
	{(char *)"index",  (getter)bpy_bm_elem_index_get, (setter)bpy_bm_elem_index_set, (char *)bpy_bm_elem_index_doc,  NULL},

	{(char *)"smooth", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_smooth_doc, (void *)BM_ELEM_SMOOTH},

	{(char *)"normal", (getter)bpy_bmface_normal_get, (setter)bpy_bmface_normal_set, (char *)bpy_bmface_normal_doc, NULL},

	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmloop_getseters[] = {
	/* generic */
	{(char *)"select", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_select_doc, (void *)BM_ELEM_SELECT},
	{(char *)"hide",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_hide_doc,   (void *)BM_ELEM_SELECT},
	{(char *)"tag",    (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_tag_doc,    (void *)BM_ELEM_TAG},
	{(char *)"index",  (getter)bpy_bm_elem_index_get, (setter)bpy_bm_elem_index_set, (char *)bpy_bm_elem_index_doc,  NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


/* Methods
 * ======= */

/* Mesh
 * ---- */

PyDoc_STRVAR(bpy_bmesh_select_flush_mode_doc,
".. method:: select_flush_mode()\n"
"\n"
"   todo.\n"
);
static PyObject *bpy_bmesh_select_flush_mode(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);

	BM_mesh_select_mode_flush(self->bm);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmesh_select_flush_doc,
".. method:: select_flush(select)\n"
"\n"
"   todo.\n"
);
static PyObject *bpy_bmesh_select_flush(BPy_BMesh *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_OBJ(self);

	param = PyLong_AsLong(value);
	if (param != FALSE && param != TRUE) {
		PyErr_SetString(PyExc_TypeError, "expected a boolean type 0/1");
		return NULL;
	}

	if (param)  BM_mesh_select_flush(self->bm);
	else        BM_mesh_deselect_flush(self->bm);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmesh_update_doc,
".. method:: update(index=False, normals=False)\n"
"\n"
"   Update mesh data.\n"
);
static PyObject *bpy_bmesh_update(BPy_BMElem *self, PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"normals", "index", NULL};

	int do_normals = FALSE;

	PyObject *index_flags = NULL;
	int do_index_hflag = 0;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(args, kw,
	                                 "|iO:update",
	                                 (char **)kwlist,
	                                 &do_normals, &index_flags))
	{
		return NULL;
	}

	if (index_flags) {
		if (PyC_FlagSet_ToBitfield(bpy_bm_htype_vert_edge_face_flags, index_flags,
		                           &do_index_hflag, "bm.update(index=...)") == -1)
		{
			return NULL;
		}
	}

	if (do_normals) {
		BM_mesh_normals_update(self->bm);
	}

	if (do_index_hflag) {
		BM_mesh_elem_index_ensure(self->bm, (char)do_index_hflag);
	}

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bmesh_transform_doc,
".. method:: transform(matrix, filter=None)\n"
"\n"
"   Transform the mesh (optionally filtering flagged data only).\n"
"\n"
"   :arg matrix: transform matrix.\n"
"   :type matrix: 4x4 :class:`mathutils.Matrix`"
"   :arg filter: set of values in ('SELECT', 'HIDE', 'SEAM', 'SMOOTH', 'TAG').\n"
"   :type filter: set\n"
);
static PyObject *bpy_bmesh_transform(BPy_BMElem *self, PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"matrix", "filter", NULL};

	MatrixObject *mat;
	PyObject *filter = NULL;
	int filter_flags = 0;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(args, kw,
	                                 "O!|O!:transform",
	                                 (char **)kwlist,
	                                 &matrix_Type, &mat,
	                                 &PySet_Type,  &filter))
	{
		return NULL;
	}
	else {
		BMVert *eve;
		BMIter iter;
		void *mat_ptr;

		if (BaseMath_ReadCallback(mat) == -1) {
			return NULL;
		}
		else if (mat->num_col != 4 || mat->num_row != 4) {
			PyErr_SetString(PyExc_ValueError, "expected a 4x4 matrix");
			return NULL;
		}

		if (filter != NULL && PyC_FlagSet_ToBitfield(bpy_bm_hflag_all_flags, filter,
		                                             &filter_flags, "bm.transform") == -1)
		{
			return NULL;
		}

		mat_ptr = mat->matrix;

		if (!filter_flags) {
			BM_ITER(eve, &iter, self->bm, BM_VERTS_OF_MESH, NULL) {
				mul_m4_v3((float (*)[4])mat_ptr, eve->co);
			}
		}
		else {
			char filter_flags_ch = (char)filter_flags;
			BM_ITER(eve, &iter, self->bm, BM_VERTS_OF_MESH, NULL) {
				if (eve->head.hflag & filter_flags_ch) {
					mul_m4_v3((float (*)[4])mat_ptr, eve->co);
				}
			}
		}
	}

	Py_RETURN_NONE;
}

/* Elem
 * ---- */

PyDoc_STRVAR(bpy_bm_elem_select_set_doc,
".. method:: select_set(select)\n"
"\n"
"   Set the selection and update assosiated geometry.\n"
);
static PyObject *bpy_bm_elem_select_set(BPy_BMElem *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_OBJ(self);

	param = PyLong_AsLong(value);
	if (param != FALSE && param != TRUE) {
		PyErr_SetString(PyExc_TypeError, "expected a boolean type 0/1");
		return NULL;
	}

	BM_elem_select_set(self->bm, self->ele, param);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bm_elem_copy_from_doc,
".. method:: copy_from(select)\n"
"\n"
"   Copy values from another element.\n"
);
static PyObject *bpy_bm_elem_copy_from(BPy_BMElem *self, BPy_BMElem *value)
{
	BPY_BM_CHECK_OBJ(self);

	if (Py_TYPE(self) != Py_TYPE(value)) {
		PyErr_Format(PyExc_TypeError,
		             "expected element of type '%.200s' not '%.200s'",
		             Py_TYPE(self)->tp_name, Py_TYPE(value)->tp_name);
		return NULL;
	}

	BM_elem_attrs_copy(value->bm, self->bm, value->ele, self->ele);

	Py_RETURN_NONE;
}

/* Vert Seq
 * -------- */

PyDoc_STRVAR(bpy_bmvert_seq_new_doc,
".. method:: new()\n"
"\n"
"   Create a new vertex.\n"
);
static PyObject *bpy_bmvert_seq_new(BPy_BMGeneric *self, PyObject *args)
{
	PyObject *py_co = NULL;

	BPY_BM_CHECK_OBJ(self);

	if(!PyArg_ParseTuple(args, "|O:verts.new", py_co)) {
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMVert *v;
		float co[3] = {0.0f, 0.0f, 0.0f};

		if (py_co && mathutils_array_parse(co, 3, 3, py_co, "verts.new(co)") != -1) {
			return NULL;
		}

		v = BM_vert_create(bm, co, NULL);

		if (v == NULL) {
			PyErr_SetString(PyExc_ValueError,
							"faces.new(verts): couldn't create the new face, internal error");
			return NULL;
		}

		return BPy_BMVert_CreatePyObject(bm, v);
	}
}

/* Edge Seq
 * -------- */

PyDoc_STRVAR(bpy_bmedge_seq_new_doc,
".. method:: new()\n"
"\n"
"   Create a new edge.\n"
);
static PyObject *bpy_bmedge_seq_new(BPy_BMGeneric *self, PyObject *args)
{
	BPy_BMVert *v1;
    BPy_BMVert *v2;

	BPY_BM_CHECK_OBJ(self);

	if(!PyArg_ParseTuple(args, "O!O!:edges.new",
	                     &BPy_BMVert_Type, &v1,
	                     &BPy_BMVert_Type, &v2))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMEdge *e;

		if (v1->v == v2->v) {
			PyErr_SetString(PyExc_ValueError,
							"edges.new(): both verts are the same");
		}

		if (!(bm == v1->bm && bm == v2->bm)) {
			PyErr_SetString(PyExc_ValueError,
							"edges.new(): both verts must be from this mesh");
		}

		if (BM_edge_exists(v1->v, v2->v)) {
			PyErr_SetString(PyExc_ValueError,
							"edges.new(): this edge exists");
		}

		e = BM_edge_create(bm, v1->v, v2->v, NULL, FALSE);

		if (e == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): couldn't create the new face, internal error");
			return NULL;
		}

		return BPy_BMEdge_CreatePyObject(bm, e);
	}
}

/* Edge Seq
 * -------- */

PyDoc_STRVAR(bpy_bmface_seq_new_doc,
".. method:: new()\n"
"\n"
"   Create a new face.\n"
);
static PyObject *bpy_bmface_seq_new(BPy_BMGeneric *self, PyObject *args)
{
	PyObject *vert_seq;

	BPY_BM_CHECK_OBJ(self);

	if(!PyArg_ParseTuple(args, "O:faces.new", &vert_seq)) {
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		PyObject *vert_seq_fast;
		Py_ssize_t vert_seq_len;
		Py_ssize_t i, i_prev;

		void *alloc;
		BMVert **vert_array;
		BMEdge **edge_array;

		BPy_BMVert *item;
		PyObject *ret = NULL;
		int ok;

		BMFace *f;

		if (!(vert_seq_fast=PySequence_Fast(vert_seq, "faces.new(...)"))) {
			return NULL;
		}

		vert_seq_len = PySequence_Fast_GET_SIZE(vert_seq_fast);

		alloc = PyMem_MALLOC(vert_seq_len * sizeof(BMVert **) + vert_seq_len * sizeof(BMEdge **));

		vert_array = (BMVert **) alloc;
		edge_array = (BMEdge **) &(((BMVert **)alloc)[vert_seq_len]);

		/* --- */
		for (i = 0; i < vert_seq_len; i++) {
			item = (BPy_BMVert *)PySequence_Fast_GET_ITEM(vert_seq_fast, i);

			if (!BPy_BMVert_Check(item)) {
				PyErr_Format(PyExc_TypeError,
				             "faces.new(verts): expected BMVert sequence, not '%.200s'",
				             Py_TYPE(item)->tp_name);
			}
			else if (item->bm != bm) {
				PyErr_Format(PyExc_TypeError,
				             "faces.new(verts): %d vertex is from another mesh", i);
			}

			vert_array[i] = item->v;

			BM_elem_flag_enable(item->v, BM_ELEM_TAG);
		}

		/* check for double verts! */
		ok = TRUE;
		for (i = 0; i < vert_seq_len; i++) {
			if (UNLIKELY(BM_elem_flag_test(vert_array[i], BM_ELEM_TAG) == FALSE)) {
				ok = FALSE;
			}
			BM_elem_flag_disable(item->v, BM_ELEM_TAG);
		}

		if (ok == FALSE) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): found the same vertex used multiple times");
			goto cleanup;
		}

		/* check if the face exists */
		if (BM_face_exists(bm, vert_array, vert_seq_len, NULL)) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): face already exists");
			goto cleanup;
		}


		/* Go ahead and make the face!
		 * --------------------------- */

		/* ensure edges */
		ok = TRUE;
		for (i = 0, i_prev = vert_seq_len - 1; i < vert_seq_len; (i_prev=i++)) {
			edge_array[i] = BM_edge_create(bm, vert_array[i], vert_array[i_prev], NULL, TRUE);
		}

		f = BM_face_create(bm, vert_array, edge_array, vert_seq_len, FALSE);

		if (f == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): couldn't create the new face, internal error");
			goto cleanup;
		}

		ret = BPy_BMFace_CreatePyObject(bm, f);

		/* pass through */
cleanup:
		Py_DECREF(vert_seq_fast);
		PyMem_FREE(alloc);
		return ret;
	}
}




static struct PyMethodDef bpy_bmesh_methods[] = {
	{"select_flush_mode", (PyCFunction)bpy_bmesh_select_flush_mode, METH_NOARGS, bpy_bmesh_select_flush_mode_doc},
	{"select_flush", (PyCFunction)bpy_bmesh_select_flush, METH_O, bpy_bmesh_select_flush_doc},
	{"update", (PyCFunction)bpy_bmesh_update, METH_VARARGS|METH_KEYWORDS, bpy_bmesh_update_doc},
	{"transform", (PyCFunction)bpy_bmesh_transform, METH_VARARGS|METH_KEYWORDS, bpy_bmesh_transform_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmvert_methods[] = {
	{"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
	{"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmedge_methods[] = {
	{"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
	{"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmface_methods[] = {
	{"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
	{"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmloop_methods[] = {
	{"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmvert_seq_methods[] = {
	{"new", (PyCFunction)bpy_bmvert_seq_new, METH_VARARGS, bpy_bmvert_seq_new_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmedge_seq_methods[] = {
	{"new", (PyCFunction)bpy_bmedge_seq_new, METH_VARARGS, bpy_bmedge_seq_new_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmface_seq_methods[] = {
	{"new", (PyCFunction)bpy_bmface_seq_new, METH_VARARGS, bpy_bmface_seq_new_doc},
	{NULL, NULL, 0, NULL}
};

/* Sequences
 * ========= */

static Py_ssize_t bpy_bmvert_seq_length(BPy_BMGeneric *self)
{
	BPY_BM_CHECK_INT(self);
	return self->bm->totvert;
}
static Py_ssize_t bpy_bmedge_seq_length(BPy_BMGeneric *self)
{
	BPY_BM_CHECK_INT(self);
	return self->bm->totedge;
}
static Py_ssize_t bpy_bmface_seq_length(BPy_BMGeneric *self)
{
	BPY_BM_CHECK_INT(self);
	return self->bm->totface;
}

static PyObject *bpy_bmvert_seq_subscript_int(BPy_BMGeneric *self, int keynum)
{
	int len;

	BPY_BM_CHECK_OBJ(self);

	len = self->bm->totvert;
	if (keynum < 0) keynum += len;
	if (keynum >= 0 && keynum < len) {
		return BPy_BMVert_CreatePyObject(self->bm, BM_iter_at_index(self->bm, BM_VERTS_OF_MESH, NULL, keynum));
	}
	PyErr_Format(PyExc_IndexError,
	             "bm.verts[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bmedge_seq_subscript_int(BPy_BMGeneric *self, int keynum)
{
	int len;

	BPY_BM_CHECK_OBJ(self);

	len = self->bm->totedge;
	if (keynum < 0) keynum += len;
	if (keynum >= 0 && keynum < len) {
		return BPy_BMEdge_CreatePyObject(self->bm, BM_iter_at_index(self->bm, BM_EDGES_OF_MESH, NULL, keynum));
	}
	PyErr_Format(PyExc_IndexError,
	             "bm.edges[index]: index %d out of range", keynum);
	return NULL;
}


static PyObject *bpy_bmface_seq_subscript_int(BPy_BMGeneric *self, int keynum)
{
	int len;

	BPY_BM_CHECK_OBJ(self);

	len = self->bm->totface;
	if (keynum < 0) keynum += len;
	if (keynum >= 0 && keynum < len) {
		return BPy_BMFace_CreatePyObject(self->bm, BM_iter_at_index(self->bm, BM_FACES_OF_MESH, NULL, keynum));
	}
	PyErr_Format(PyExc_IndexError,
	             "bm.faces[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bmvert_seq_subscript(BPy_BMGeneric *self, PyObject *key)
{
	/* dont need error check here */
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmvert_seq_subscript_int(self, i);
	}
	/* TODO, slice */
	else {
		PyErr_SetString(PyExc_AttributeError, "bm.verts[key]: invalid key, key must be an int");
		return NULL;
	}
}

static PyObject *bpy_bmedge_seq_subscript(BPy_BMGeneric *self, PyObject *key)
{
	/* dont need error check here */
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmedge_seq_subscript_int(self, i);
	}
	/* TODO, slice */
	else {
		PyErr_SetString(PyExc_AttributeError, "bm.edges[key]: invalid key, key must be an int");
		return NULL;
	}
}

static PyObject *bpy_bmface_seq_subscript(BPy_BMGeneric *self, PyObject *key)
{
	/* dont need error check here */
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmface_seq_subscript_int(self, i);
	}
	/* TODO, slice */
	else {
		PyErr_SetString(PyExc_AttributeError, "bm.faces[key]: invalid key, key must be an int");
		return NULL;
	}
}

static int bpy_bmvert_seq_contains(BPy_BMGeneric *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (BPy_BMVert_Check(value)) {
		BPy_BMVert *value_bmvert = (BPy_BMVert *)value;
		if (value_bmvert->bm == self->bm) {
			BMVert *v, *v_test = value_bmvert->v;
			BMIter iter;
			BM_ITER(v, &iter, self->bm, BM_VERTS_OF_MESH, NULL) {
				if (v == v_test) {
					return 1;
				}
			}
		}
	}
	return 0;
}

static int bpy_bmedge_seq_contains(BPy_BMGeneric *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (BPy_BMEdge_Check(value)) {
		BPy_BMEdge *value_bmedge = (BPy_BMEdge *)value;
		if (value_bmedge->bm == self->bm) {
			BMEdge *e, *e_test = value_bmedge->e;
			BMIter iter;
			BM_ITER(e, &iter, self->bm, BM_EDGES_OF_MESH, NULL) {
				if (e == e_test) {
					return 1;
				}
			}
		}
	}
	return 0;
}

static int bpy_bmface_seq_contains(BPy_BMGeneric *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (BPy_BMFace_Check(value)) {
		BPy_BMFace *value_bmface = (BPy_BMFace *)value;
		if (value_bmface->bm == self->bm) {
			BMFace *f, *f_test = value_bmface->f;
			BMIter iter;
			BM_ITER(f, &iter, self->bm, BM_FACES_OF_MESH, NULL) {
				if (f == f_test) {
					return 1;
				}
			}
		}
	}
	return 0;
}

static PySequenceMethods bpy_bmvert_seq_as_sequence = {
	(lenfunc)bpy_bmvert_seq_length,              /* sq_length */
	NULL,                                        /* sq_concat */
	NULL,                                        /* sq_repeat */
	(ssizeargfunc)bpy_bmvert_seq_subscript_int,  /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                        /* sq_slice */
	(ssizeobjargproc)NULL,                       /* sq_ass_item */
	NULL,                                        /* *was* sq_ass_slice */
	(objobjproc)bpy_bmvert_seq_contains,         /* sq_contains */
	(binaryfunc) NULL,                           /* sq_inplace_concat */
	(ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PySequenceMethods bpy_bmedge_seq_as_sequence = {
	(lenfunc)bpy_bmedge_seq_length,              /* sq_length */
	NULL,                                        /* sq_concat */
	NULL,                                        /* sq_repeat */
	(ssizeargfunc)bpy_bmedge_seq_subscript_int,  /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                        /* sq_slice */
	(ssizeobjargproc)NULL,                       /* sq_ass_item */
	NULL,                                        /* *was* sq_ass_slice */
	(objobjproc)bpy_bmedge_seq_contains,         /* sq_contains */
	(binaryfunc) NULL,                           /* sq_inplace_concat */
	(ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PySequenceMethods bpy_bmface_seq_as_sequence = {
	(lenfunc)bpy_bmface_seq_length,              /* sq_length */
	NULL,                                        /* sq_concat */
	NULL,                                        /* sq_repeat */
	(ssizeargfunc)bpy_bmface_seq_subscript_int,  /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,                                        /* sq_slice */
	(ssizeobjargproc)NULL,                       /* sq_ass_item */
	NULL,                                        /* *was* sq_ass_slice */
	(objobjproc)bpy_bmface_seq_contains,         /* sq_contains */
	(binaryfunc) NULL,                           /* sq_inplace_concat */
	(ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};


static PyMappingMethods bpy_bmvert_seq_as_mapping = {
	(lenfunc)bpy_bmvert_seq_length,              /* mp_length */
	(binaryfunc)bpy_bmvert_seq_subscript,        /* mp_subscript */
	(objobjargproc)NULL,                         /* mp_ass_subscript */
};

static PyMappingMethods bpy_bmedge_seq_as_mapping = {
	(lenfunc)bpy_bmedge_seq_length,              /* mp_length */
	(binaryfunc)bpy_bmedge_seq_subscript,        /* mp_subscript */
	(objobjargproc)NULL,                         /* mp_ass_subscript */
};

static PyMappingMethods bpy_bmface_seq_as_mapping = {
	(lenfunc)bpy_bmface_seq_length,              /* mp_length */
	(binaryfunc)bpy_bmface_seq_subscript,        /* mp_subscript */
	(objobjargproc)NULL,                         /* mp_ass_subscript */
};

/* Iterator
 * -------- */

static PyObject *bpy_bmvert_seq_iter(BPy_BMGeneric *self)
{
	BPy_BMIter *py_iter;

	BPY_BM_CHECK_OBJ(self);
	py_iter = (BPy_BMIter *)BPy_BMIter_CreatePyObject(self->bm);
	BM_iter_init(&py_iter->iter, self->bm, BM_VERTS_OF_MESH, NULL);
	return (PyObject *)py_iter;
}

static PyObject *bpy_bmedge_seq_iter(BPy_BMGeneric *self)
{
	BPy_BMIter *py_iter;

	BPY_BM_CHECK_OBJ(self);
	py_iter = (BPy_BMIter *)BPy_BMIter_CreatePyObject(self->bm);
	BM_iter_init(&py_iter->iter, self->bm, BM_EDGES_OF_MESH, NULL);
	return (PyObject *)py_iter;
}

static PyObject *bpy_bmface_seq_iter(BPy_BMGeneric *self)
{
	BPy_BMIter *py_iter;

	BPY_BM_CHECK_OBJ(self);
	py_iter = (BPy_BMIter *)BPy_BMIter_CreatePyObject(self->bm);
	BM_iter_init(&py_iter->iter, self->bm, BM_FACES_OF_MESH, NULL);
	return (PyObject *)py_iter;
}


static PyObject *bpy_bm_iter_next(BPy_BMIter *self)
{
	BMHeader *ele = BM_iter_step(&self->iter);
	if (ele == NULL) {
		PyErr_SetString(PyExc_StopIteration, "bpy_bm_iter_next stop");
		return NULL;
	}
	else {
		return (PyObject *)BPy_BMElem_CreatePyObject(self->bm, ele);
	}
}


/* not sure where this should go */
static long bpy_bm_elem_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_BMElem *)self)->ele);
}

PyTypeObject BPy_BMesh_Type     = {{{0}}};
PyTypeObject BPy_BMVert_Type    = {{{0}}};
PyTypeObject BPy_BMEdge_Type    = {{{0}}};
PyTypeObject BPy_BMFace_Type    = {{{0}}};
PyTypeObject BPy_BMLoop_Type    = {{{0}}};
PyTypeObject BPy_BMVertSeq_Type = {{{0}}};
PyTypeObject BPy_BMEdgeSeq_Type = {{{0}}};
PyTypeObject BPy_BMFaceSeq_Type = {{{0}}};
PyTypeObject BPy_BMIter_Type    = {{{0}}};



void BPy_BM_init_types(void)
{
	BPy_BMesh_Type.tp_basicsize     = sizeof(BPy_BMesh);
	BPy_BMVert_Type.tp_basicsize    = sizeof(BPy_BMVert);
	BPy_BMEdge_Type.tp_basicsize    = sizeof(BPy_BMEdge);
	BPy_BMFace_Type.tp_basicsize    = sizeof(BPy_BMFace);
	BPy_BMLoop_Type.tp_basicsize    = sizeof(BPy_BMLoop);
	BPy_BMVertSeq_Type.tp_basicsize = sizeof(BPy_BMGeneric);
	BPy_BMEdgeSeq_Type.tp_basicsize = sizeof(BPy_BMGeneric);
	BPy_BMFaceSeq_Type.tp_basicsize = sizeof(BPy_BMGeneric);
	BPy_BMIter_Type.tp_basicsize    = sizeof(BPy_BMIter);


	BPy_BMesh_Type.tp_name     = "BMesh";
	BPy_BMVert_Type.tp_name    = "BMVert";
	BPy_BMEdge_Type.tp_name    = "BMEdge";
	BPy_BMFace_Type.tp_name    = "BMFace";
	BPy_BMLoop_Type.tp_name    = "BMLoop";
	BPy_BMVertSeq_Type.tp_name = "BMVertSeq";
	BPy_BMEdgeSeq_Type.tp_name = "BMEdgeSeq";
	BPy_BMFaceSeq_Type.tp_name = "BMFaceSeq";
	BPy_BMIter_Type.tp_name    = "BMIter";


	BPy_BMesh_Type.tp_getset     = bpy_bmesh_getseters;
	BPy_BMVert_Type.tp_getset    = bpy_bmvert_getseters;
	BPy_BMEdge_Type.tp_getset    = bpy_bmedge_getseters;
	BPy_BMFace_Type.tp_getset    = bpy_bmface_getseters;
	BPy_BMLoop_Type.tp_getset    = bpy_bmloop_getseters;
	BPy_BMVertSeq_Type.tp_getset = NULL;
	BPy_BMEdgeSeq_Type.tp_getset = NULL;
	BPy_BMFaceSeq_Type.tp_getset = NULL;
	BPy_BMIter_Type.tp_getset    = NULL;


	BPy_BMesh_Type.tp_methods     = bpy_bmesh_methods;
	BPy_BMVert_Type.tp_methods    = bpy_bmvert_methods;
	BPy_BMEdge_Type.tp_methods    = bpy_bmedge_methods;
	BPy_BMFace_Type.tp_methods    = bpy_bmface_methods;
	BPy_BMLoop_Type.tp_methods    = bpy_bmloop_methods;
	BPy_BMVertSeq_Type.tp_methods = bpy_bmvert_seq_methods;
	BPy_BMEdgeSeq_Type.tp_methods = bpy_bmedge_seq_methods;
	BPy_BMFaceSeq_Type.tp_methods = bpy_bmface_seq_methods;
	BPy_BMIter_Type.tp_methods    = NULL;


	BPy_BMesh_Type.tp_hash     = NULL;
	BPy_BMVert_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMEdge_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMFace_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMLoop_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMVertSeq_Type.tp_hash = NULL;
	BPy_BMEdgeSeq_Type.tp_hash = NULL;
	BPy_BMFaceSeq_Type.tp_hash = NULL;
	BPy_BMIter_Type.tp_hash    = NULL;


	BPy_BMVertSeq_Type.tp_as_sequence = &bpy_bmvert_seq_as_sequence;
	BPy_BMEdgeSeq_Type.tp_as_sequence = &bpy_bmedge_seq_as_sequence;
	BPy_BMFaceSeq_Type.tp_as_sequence = &bpy_bmface_seq_as_sequence;

	BPy_BMVertSeq_Type.tp_as_mapping = &bpy_bmvert_seq_as_mapping;
	BPy_BMEdgeSeq_Type.tp_as_mapping = &bpy_bmedge_seq_as_mapping;
	BPy_BMFaceSeq_Type.tp_as_mapping = &bpy_bmface_seq_as_mapping;

	BPy_BMVertSeq_Type.tp_iter = (getiterfunc)bpy_bmvert_seq_iter;
	BPy_BMEdgeSeq_Type.tp_iter = (getiterfunc)bpy_bmedge_seq_iter;
	BPy_BMFaceSeq_Type.tp_iter = (getiterfunc)bpy_bmface_seq_iter;

	/* only 1 iteratir so far */
	BPy_BMIter_Type.tp_iternext = (iternextfunc)bpy_bm_iter_next;

	/*
	BPy_BMesh_Type.
	BPy_BMVert_Type.
	BPy_BMEdge_Type.
	BPy_BMFace_Type.
	BPy_BMLoop_Type.
	BPy_BMVertSeq_Type.
	BPy_BMEdgeSeq_Type.
	BPy_BMFaceSeq_Type.
	BPy_BMIter_Type.
	*/

	BPy_BMesh_Type.tp_flags     = Py_TPFLAGS_DEFAULT;
	BPy_BMVert_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMEdge_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMFace_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMLoop_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMVertSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMEdgeSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMFaceSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMIter_Type.tp_flags    = Py_TPFLAGS_DEFAULT;


	PyType_Ready(&BPy_BMesh_Type);
	PyType_Ready(&BPy_BMVert_Type);
	PyType_Ready(&BPy_BMEdge_Type);
	PyType_Ready(&BPy_BMFace_Type);
	PyType_Ready(&BPy_BMLoop_Type);
	PyType_Ready(&BPy_BMVertSeq_Type);
	PyType_Ready(&BPy_BMEdgeSeq_Type);
	PyType_Ready(&BPy_BMFaceSeq_Type);
	PyType_Ready(&BPy_BMIter_Type);
}


/* Utility Functions
 * ***************** */

PyObject *BPy_BMesh_CreatePyObject(BMesh *bm)
{
	BPy_BMesh *self = PyObject_New(BPy_BMesh, &BPy_BMesh_Type);
	self->bm = bm;
	return (PyObject *)self;
}

PyObject *BPy_BMVert_CreatePyObject(BMesh *bm, BMVert *v)
{
	BPy_BMVert *self = PyObject_New(BPy_BMVert, &BPy_BMVert_Type);
	BLI_assert(v != NULL);
	self->bm = bm;
	self->v  = v;
	return (PyObject *)self;
}

PyObject *BPy_BMEdge_CreatePyObject(BMesh *bm, BMEdge *e)
{
	BPy_BMEdge *self = PyObject_New(BPy_BMEdge, &BPy_BMEdge_Type);
	BLI_assert(e != NULL);
	self->bm = bm;
	self->e  = e;
	return (PyObject *)self;
}

PyObject *BPy_BMFace_CreatePyObject(BMesh *bm, BMFace *f)
{
	BPy_BMFace *self = PyObject_New(BPy_BMFace, &BPy_BMFace_Type);
	BLI_assert(f != NULL);
	self->bm = bm;
	self->f  = f;
	return (PyObject *)self;
}

PyObject *BPy_BMLoop_CreatePyObject(BMesh *bm, BMLoop *l)
{
	BPy_BMLoop *self = PyObject_New(BPy_BMLoop, &BPy_BMLoop_Type);
	BLI_assert(l != NULL);
	self->bm = bm;
	self->l  = l;
	return (PyObject *)self;
}

PyObject *BPy_BMVertSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMGeneric *self = PyObject_New(BPy_BMGeneric, &BPy_BMVertSeq_Type);
	self->bm = bm;
	return (PyObject *)self;
}

PyObject *BPy_BMEdgeSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMGeneric *self = PyObject_New(BPy_BMGeneric, &BPy_BMEdgeSeq_Type);
	self->bm = bm;
	return (PyObject *)self;
}

PyObject *BPy_BMFaceSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMGeneric *self = PyObject_New(BPy_BMGeneric, &BPy_BMFaceSeq_Type);
	self->bm = bm;
	return (PyObject *)self;
}

PyObject *BPy_BMIter_CreatePyObject(BMesh *bm)
{
	BPy_BMIter *self = PyObject_New(BPy_BMIter, &BPy_BMIter_Type);
	self->bm = bm;
	/* caller must initialize 'iter' member */
	return (PyObject *)self;
}

/* this is just a helper func */
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
			PyErr_SetString(PyExc_SystemError, "internal error");
			return NULL;
	}
}

int bpy_bm_generic_valid_check(BPy_BMGeneric *self)
{
	if (self->bm) {
		return 0;
	}
	PyErr_Format(PyExc_ReferenceError,
	             "BMesh data of type %.200s has been removed",
	             Py_TYPE(self)->tp_name);
	return -1;
}

void bpy_bm_generic_invalidate(BPy_BMGeneric *self)
{
	self->bm = NULL;
}
