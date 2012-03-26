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

/** \file blender/python/bmesh/bmesh_py_types.c
 *  \ingroup pybmesh
 */

#include <Python.h>

#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BKE_depsgraph.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"

#include "bmesh.h"

#include "../mathutils/mathutils.h"

#include "../generic/py_capi_utils.h"

#include "bmesh_py_types.h" /* own include */
#include "bmesh_py_types_select.h"
#include "bmesh_py_types_customdata.h"
#include "bmesh_py_types_meshdata.h"

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

PyDoc_STRVAR(bpy_bm_elem_select_doc,  "Selected state of this element.\n\n:type: boolean");
PyDoc_STRVAR(bpy_bm_elem_hide_doc,    "Hidden state of this element.\n\n:type: boolean");
PyDoc_STRVAR(bpy_bm_elem_tag_doc,     "Generic attribute scripts can use for own logic\n\n:type: boolean");
PyDoc_STRVAR(bpy_bm_elem_smooth_doc,  "Smooth state of this element.\n\n:type: boolean");


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


PyDoc_STRVAR(bpy_bm_elem_index_doc,
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
"   To ensure the value is up to date - see :class:`BMElemSeq.index_update`.\n"
);
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
		PyErr_SetString(PyExc_TypeError,
		                "expected an int type");
		return -1;
	}
	else {
		BM_elem_index_set(self->ele, param); /* set_dirty! */

		/* when setting the index assume its set invalid */
		if (self->ele->head.htype & (BM_VERT | BM_EDGE | BM_FACE)) {
			self->bm->elem_index_dirty |= self->ele->head.htype;
		}

		return 0;
	}
}

/* type specific get/sets
 * ---------------------- */


/* Mesh
 * ^^^^ */

/* doc-strings for all uses of this funcion */

PyDoc_STRVAR(bpy_bmvertseq_doc,
"This meshes vert sequence (read-only).\n\n:type: :class:`BMVertSeq`"
);
static PyObject *bpy_bmvertseq_get(BPy_BMesh *self, void *UNUSED(closure))
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMVertSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmedgeseq_doc,
"This meshes edge sequence (read-only).\n\n:type: :class:`BMEdgeSeq`"
);
static PyObject *bpy_bmedgeseq_get(BPy_BMesh *self, void *UNUSED(closure))
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMEdgeSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmfaceseq_doc,
"This meshes face sequence (read-only).\n\n:type: :class:`BMFaceSeq`"
);
static PyObject *bpy_bmfaceseq_get(BPy_BMesh *self, void *UNUSED(closure))
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMFaceSeq_CreatePyObject(self->bm);
}

PyDoc_STRVAR(bpy_bmloopseq_doc,
"This meshes face sequence (read-only).\n\n:type: :class:`BMLoopSeq`"
);
static PyObject *bpy_bmloopseq_get(BPy_BMesh *self, void *UNUSED(closure))
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMLoopSeq_CreatePyObject(self->bm);
}

/* vert */
PyDoc_STRVAR(bpy_bmvert_link_edges_doc,
"Edges connected to this vertex (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMVert`"
);
PyDoc_STRVAR(bpy_bmvert_link_faces_doc,
"Faces connected to this vertex (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMFace`"
);
PyDoc_STRVAR(bpy_bmvert_link_loops_doc,
"Loops that use this vertex (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMLoop`"
);
/* edge */
PyDoc_STRVAR(bpy_bmedge_verts_doc,
"Verts this edge uses (always 2), (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMVert`"
);
PyDoc_STRVAR(bpy_bmedge_link_faces_doc,
"Faces connected to this edge, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMFace`"
);
PyDoc_STRVAR(bpy_bmedge_link_loops_doc,
"Loops connected to this edge, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMLoop`"
);
/* face */
PyDoc_STRVAR(bpy_bmface_verts_doc,
"Verts of this face, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMVert`"
);
PyDoc_STRVAR(bpy_bmface_edges_doc,
"Edges of this face, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMEdge`"
);
PyDoc_STRVAR(bpy_bmface_loops_doc,
"Loops of this face, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMLoop`"
);
/* loop */
PyDoc_STRVAR(bpy_bmloops_link_loops_doc,
"Loops connected to this loop, (read-only).\n\n:type: :class:`BMElemSeq` of :class:`BMLoop`"
);

static PyObject *bpy_bmelemseq_elem_get(BPy_BMElem *self, void *itype)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMElemSeq_CreatePyObject(self->bm, self, GET_INT_FROM_POINTER(itype));
}


PyDoc_STRVAR(bpy_bm_is_valid_doc,
"True when this element is valid (hasn't been removed).\n\n:type: boolean"
);
static PyObject *bpy_bm_is_valid_get(BPy_BMGeneric *self)
{
	return PyBool_FromLong(BPY_BM_IS_VALID(self));
}

PyDoc_STRVAR(bpy_bmesh_is_wrapped_doc,
"True when this mesh is owned by blender (typically the editmode BMesh).\n\n:type: boolean"
);
static PyObject *bpy_bmesh_is_wrapped_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);

	return PyBool_FromLong(self->flag & BPY_BMFLAG_IS_WRAPPED);
}

PyDoc_STRVAR(bpy_bmesh_select_mode_doc,
"The selection mode, values can be {'VERT', 'EDGE', 'FACE'}, can't be assigned an empty set.\n\n:type: set"
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
		PyErr_SetString(PyExc_TypeError,
		                "bm.select_mode: cant assignt an empty value");
		return -1;
	}
	else {
		self->bm->selectmode = flag;
		return 0;
	}
}

PyDoc_STRVAR(bpy_bmesh_select_history_doc,
"Sequence of selected items (the last is displayed as active).\n\n:type: :class:`BMEditSelSeq`"
);
static PyObject *bpy_bmesh_select_history_get(BPy_BMesh *self)
{
	BPY_BM_CHECK_OBJ(self);

	return BPy_BMEditSel_CreatePyObject(self->bm);
}

static int bpy_bmesh_select_history_set(BPy_BMesh *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	return BPy_BMEditSel_Assign(self, value);
}

/* Vert
 * ^^^^ */

PyDoc_STRVAR(bpy_bmvert_co_doc,
"The coordinates for this vertex as a 3D, wrapped vector.\n\n:type: :class:`mathutils.Vector`"
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
"The normal for this vertex as a 3D, wrapped vector.\n\n:type: :class:`mathutils.Vector`"
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


PyDoc_STRVAR(bpy_bmvert_is_manifold_doc,
"True when this vertex is manifold (read-only).\n\n:type: boolean"
);
static PyObject *bpy_bmvert_is_manifold_get(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyBool_FromLong(BM_vert_is_manifold(self->v));
}


PyDoc_STRVAR(bpy_bmvert_is_wire_doc,
"True when this vertex is not connected to any faces (read-only).\n\n:type: boolean"
);
static PyObject *bpy_bmvert_is_wire_get(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyBool_FromLong(BM_vert_is_wire(self->v));
}


/* Edge
 * ^^^^ */

PyDoc_STRVAR(bpy_bmedge_is_manifold_doc,
"True when this edge is manifold (read-only).\n\n:type: boolean"
);
static PyObject *bpy_bmedge_is_manifold_get(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyBool_FromLong(BM_edge_is_manifold(self->e));
}


PyDoc_STRVAR(bpy_bmedge_is_wire_doc,
"True when this edge is not connected to any faces (read-only).\n\n:type: boolean"
);
static PyObject *bpy_bmedge_is_wire_get(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyBool_FromLong(BM_edge_is_wire(self->e));
}


PyDoc_STRVAR(bpy_bmedge_is_boundary_doc,
"True when this edge is at the boundary of a face (read-only).\n\n:type: boolean"
);
static PyObject *bpy_bmedge_is_boundary_get(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyBool_FromLong(BM_edge_is_boundary(self->e));
}


/* Face
 * ^^^^ */

PyDoc_STRVAR(bpy_bmface_normal_doc,
"The normal for this face as a 3D, wrapped vector.\n\n:type: :class:`mathutils.Vector`"
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

PyDoc_STRVAR(bpy_bmface_material_index_doc,
"The faces material index.\n\n:type: int"
);
static PyObject *bpy_bmface_material_index_get(BPy_BMFace *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyLong_FromLong(self->f->mat_nr);
}

static int bpy_bmface_material_index_set(BPy_BMFace *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_INT(self);

	param = PyLong_AsLong(value);

	if (param == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError,
		                "expected an int type");
		return -1;
	}
	else if ((param < 0) || (param > MAXMAT)) {
		/* normally we clamp but in this case raise an error */
		PyErr_SetString(PyExc_ValueError,
		                "material index outside of usable range (0 - 32766)");
		return -1;
	}
	else {
		self->f->mat_nr = (short)param;
		return 0;
	}
}

/* Loop
 * ^^^^ */

PyDoc_STRVAR(bpy_bmloop_vert_doc,
"The loops vertex (read-only).\n\n:type: :class:`BMVert`"
);
static PyObject *bpy_bmloop_vert_get(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMVert_CreatePyObject(self->bm, self->l->v);
}


PyDoc_STRVAR(bpy_bmloop_edge_doc,
"The loops edge (between this loop and the next), (read-only).\n\n:type: :class:`BMEdge`"
);
static PyObject *bpy_bmloop_edge_get(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMEdge_CreatePyObject(self->bm, self->l->e);
}


PyDoc_STRVAR(bpy_bmloop_face_doc,
"The face this loop makes (read-only).\n\n:type: :class:`BMFace`"
);
static PyObject *bpy_bmloop_face_get(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMFace_CreatePyObject(self->bm, self->l->f);
}

PyDoc_STRVAR(bpy_bmloop_link_loop_next_doc,
"The next face corner (read-only).\n\n:type: :class:`BMLoop`"
);
static PyObject *bpy_bmloop_link_loop_next_get(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMLoop_CreatePyObject(self->bm, self->l->next);
}

PyDoc_STRVAR(bpy_bmloop_link_loop_prev_doc,
"The previous face corner (read-only).\n\n:type: :class:`BMLoop`"
);
static PyObject *bpy_bmloop_link_loop_prev_get(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return BPy_BMLoop_CreatePyObject(self->bm, self->l->prev);
}

/* ElemSeq
 * ^^^^^^^ */

/* note: use for bmvert/edge/face/loop seq's use these, not bmelemseq directly */
PyDoc_STRVAR(bpy_bmelemseq_layers_doc,
"blah blah (read-only).\n\n:type: :class:`BMLayerAccess`"
);
static PyObject *bpy_bmelemseq_layers_get(BPy_BMElemSeq *self, void *htype)
{
	BPY_BM_CHECK_OBJ(self);

	return BPy_BMLayerAccess_CreatePyObject(self->bm, GET_INT_FROM_POINTER(htype));
}

static PyGetSetDef bpy_bmesh_getseters[] = {
    {(char *)"verts", (getter)bpy_bmvertseq_get, (setter)NULL, (char *)bpy_bmvertseq_doc, NULL},
    {(char *)"edges", (getter)bpy_bmedgeseq_get, (setter)NULL, (char *)bpy_bmedgeseq_doc, NULL},
    {(char *)"faces", (getter)bpy_bmfaceseq_get, (setter)NULL, (char *)bpy_bmfaceseq_doc, NULL},
    {(char *)"loops", (getter)bpy_bmloopseq_get, (setter)NULL, (char *)bpy_bmloopseq_doc, NULL},
    {(char *)"select_mode", (getter)bpy_bmesh_select_mode_get, (setter)bpy_bmesh_select_mode_set, (char *)bpy_bmesh_select_mode_doc, NULL},

    {(char *)"select_history", (getter)bpy_bmesh_select_history_get, (setter)bpy_bmesh_select_history_set, (char *)bpy_bmesh_select_history_doc, NULL},

    /* readonly checks */
    {(char *)"is_wrapped", (getter)bpy_bmesh_is_wrapped_get, (setter)NULL, (char *)bpy_bmesh_is_wrapped_doc, NULL}, /* as with mathutils */
    {(char *)"is_valid",   (getter)bpy_bm_is_valid_get,   (setter)NULL, (char *)bpy_bm_is_valid_doc, NULL},

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

    /* connectivity data */
    {(char *)"link_edges", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmvert_link_edges_doc, (void *)BM_EDGES_OF_VERT},
    {(char *)"link_faces", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmvert_link_faces_doc, (void *)BM_FACES_OF_VERT},
    {(char *)"link_loops", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmvert_link_loops_doc, (void *)BM_LOOPS_OF_VERT},

    /* readonly checks */
    {(char *)"is_manifold",  (getter)bpy_bmvert_is_manifold_get,  (setter)NULL, (char *)bpy_bmvert_is_manifold_doc, NULL},
    {(char *)"is_wire",      (getter)bpy_bmvert_is_wire_get,      (setter)NULL, (char *)bpy_bmvert_is_wire_doc, NULL},
    {(char *)"is_valid",     (getter)bpy_bm_is_valid_get,         (setter)NULL, (char *)bpy_bm_is_valid_doc, NULL},

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

    /* connectivity data */
    {(char *)"verts", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmedge_verts_doc, (void *)BM_VERTS_OF_EDGE},

    {(char *)"link_faces", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmedge_link_faces_doc, (void *)BM_FACES_OF_EDGE},
    {(char *)"link_loops", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmedge_link_loops_doc, (void *)BM_LOOPS_OF_EDGE},

    /* readonly checks */
    {(char *)"is_manifold",  (getter)bpy_bmedge_is_manifold_get,  (setter)NULL, (char *)bpy_bmedge_is_manifold_doc, NULL},
    {(char *)"is_wire",      (getter)bpy_bmedge_is_wire_get,      (setter)NULL, (char *)bpy_bmedge_is_wire_doc, NULL},
    {(char *)"is_boundary",   (getter)bpy_bmedge_is_boundary_get,   (setter)NULL, (char *)bpy_bmedge_is_boundary_doc, NULL},
    {(char *)"is_valid",     (getter)bpy_bm_is_valid_get,         (setter)NULL, (char *)bpy_bm_is_valid_doc, NULL},

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

    {(char *)"material_index",  (getter)bpy_bmface_material_index_get, (setter)bpy_bmface_material_index_set, (char *)bpy_bmface_material_index_doc,  NULL},

    /* connectivity data */
    {(char *)"verts", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmface_verts_doc, (void *)BM_VERTS_OF_FACE},
    {(char *)"edges", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmface_edges_doc, (void *)BM_EDGES_OF_FACE},
    {(char *)"loops", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmface_loops_doc, (void *)BM_LOOPS_OF_FACE},

    /* readonly checks */
    {(char *)"is_valid",   (getter)bpy_bm_is_valid_get, (setter)NULL, (char *)bpy_bm_is_valid_doc, NULL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmloop_getseters[] = {
    /* generic */
    // flags are available but not used for loops.
    // {(char *)"select", (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_select_doc, (void *)BM_ELEM_SELECT},
    // {(char *)"hide",   (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_hide_doc,   (void *)BM_ELEM_SELECT},
    {(char *)"tag",    (getter)bpy_bm_elem_hflag_get, (setter)bpy_bm_elem_hflag_set, (char *)bpy_bm_elem_tag_doc,    (void *)BM_ELEM_TAG},
    {(char *)"index",  (getter)bpy_bm_elem_index_get, (setter)bpy_bm_elem_index_set, (char *)bpy_bm_elem_index_doc,  NULL},

    {(char *)"vert", (getter)bpy_bmloop_vert_get, (setter)NULL, (char *)bpy_bmloop_vert_doc, NULL},
    {(char *)"edge", (getter)bpy_bmloop_edge_get, (setter)NULL, (char *)bpy_bmloop_edge_doc, NULL},
    {(char *)"face", (getter)bpy_bmloop_face_get, (setter)NULL, (char *)bpy_bmloop_face_doc, NULL},

    /* connectivity data */
    {(char *)"link_loops", (getter)bpy_bmelemseq_elem_get, (setter)NULL, (char *)bpy_bmloops_link_loops_doc, (void *)BM_LOOPS_OF_LOOP},
    {(char *)"link_loop_next", (getter)bpy_bmloop_link_loop_next_get, (setter)NULL, (char *)bpy_bmloop_link_loop_next_doc, NULL},
    {(char *)"link_loop_prev", (getter)bpy_bmloop_link_loop_prev_get, (setter)NULL, (char *)bpy_bmloop_link_loop_prev_doc, NULL},

    /* readonly checks */
    {(char *)"is_valid",   (getter)bpy_bm_is_valid_get, (setter)NULL, (char *)bpy_bm_is_valid_doc, NULL},

    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef bpy_bmvertseq_getseters[] = {
    {(char *)"layers",    (getter)bpy_bmelemseq_layers_get, (setter)NULL, (char *)bpy_bmelemseq_layers_doc, (void *)BM_VERT},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};
static PyGetSetDef bpy_bmedgeseq_getseters[] = {
    {(char *)"layers",    (getter)bpy_bmelemseq_layers_get, (setter)NULL, (char *)bpy_bmelemseq_layers_doc, (void *)BM_EDGE},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};
static PyGetSetDef bpy_bmfaceseq_getseters[] = {
    {(char *)"layers",    (getter)bpy_bmelemseq_layers_get, (setter)NULL, (char *)bpy_bmelemseq_layers_doc, (void *)BM_FACE},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};
static PyGetSetDef bpy_bmloopseq_getseters[] = {
    {(char *)"layers",    (getter)bpy_bmelemseq_layers_get, (setter)NULL, (char *)bpy_bmelemseq_layers_doc, (void *)BM_LOOP},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


/* Methods
 * ======= */


/* Mesh
 * ---- */

PyDoc_STRVAR(bpy_bmesh_copy_doc,
".. method:: copy()\n"
"\n"
"   :return: A copy of this BMesh.\n"
"   :rtype: :class:`BMesh`\n"
);
static PyObject *bpy_bmesh_copy(BPy_BMesh *self)
{
	BMesh *bm;
	BMesh *bm_copy;

	BPY_BM_CHECK_OBJ(self);

	bm = self->bm;

	bm_copy = BM_mesh_copy(bm);

	if (bm_copy) {
		return BPy_BMesh_CreatePyObject(bm_copy, BPY_BMFLAG_NOP);
	}
	else {
		PyErr_SetString(PyExc_SystemError, "Unable to copy BMesh, internal error");
		return NULL;
	}
}

PyDoc_STRVAR(bpy_bmesh_clear_doc,
".. method:: clear()\n"
"\n"
"   Clear all mesh data.\n"
);
static PyObject *bpy_bmesh_clear(BPy_BMesh *self)
{
	BMesh *bm;

	BPY_BM_CHECK_OBJ(self);

	bm = self->bm;

	BM_mesh_clear(bm);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmesh_free_doc,
".. method:: free()\n"
"\n"
"   Explicitly free the BMesh data from memory, causing exceptions on further access.\n"
"\n"
"   .. note::\n"
"\n"
"      The BMesh is freed automatically, typically when the script finishes executing.\n"
"      However in some cases its hard to predict when this will be and its useful to\n"
"      explicitly free the data.\n"
);
static PyObject *bpy_bmesh_free(BPy_BMesh *self)
{
	if (self->bm) {
		BMesh *bm = self->bm;

		if ((self->flag & BPY_BMFLAG_IS_WRAPPED) == 0) {
			BM_mesh_free(bm);
		}

		bpy_bm_generic_invalidate((BPy_BMGeneric *)self);
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_bmesh_to_mesh_doc,
".. method:: to_mesh(mesh)\n"
"\n"
"   Writes this BMesh data into an existing Mesh datablock.\n"
"\n"
"   :arg mesh: The mesh data to write into.\n"
"   :type mesh: :class:`Mesh`\n"
);
static PyObject *bpy_bmesh_to_mesh(BPy_BMesh *self, PyObject *args)
{
	PyObject *py_mesh;
	Mesh *me;
	BMesh *bm;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O:to_mesh", &py_mesh) ||
	    !(me = PyC_RNA_AsPointer(py_mesh, "Mesh")))
	{
		return NULL;
	}

	/* we could allow this but its almost certainly _not_ what script authors want */
	if (me->edit_btmesh) {
		PyErr_Format(PyExc_ValueError,
		             "to_mesh(): Mesh '%s' is in editmode", me->id.name + 2);
		return NULL;
	}

	bm = self->bm;

	BM_mesh_bm_to_me(bm, me, FALSE);

	/* we could have the user do this but if they forget blender can easy crash
	 * since the references arrays for the objects derived meshes are now invalid */
	DAG_id_tag_update(&me->id, OB_RECALC_DATA);

	Py_RETURN_NONE;
}

/* note: rna_Object_to_mesh() also has apply_modifiers arg that works the same way */
PyDoc_STRVAR(bpy_bmesh_from_object_doc,
".. method:: from_object(mesh, apply_modifiers=True)\n"
"\n"
"   Initialize this bmesh from existing object datablock.\n"
"\n"
"   :arg object: The object data to load.\n"
"   :type object: :class:`Object`\n"
"   :arg apply_modifiers: Use the final display mesh rather then the deformed cage.\n"
"   :type apply_modifiers: boolean\n"
);
static PyObject *bpy_bmesh_from_object(BPy_BMesh *self, PyObject *args)
{
	PyObject *py_object;
	Object *ob;
	BMesh *bm;
	int apply_modifiers = TRUE;
	DerivedMesh *dm;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|i:from_object", &py_object, &apply_modifiers) ||
	    !(ob = PyC_RNA_AsPointer(py_object, "Object")))
	{
		return NULL;
	}

	dm = apply_modifiers ? ob->derivedFinal : ob->derivedDeform;

	if (dm == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "from_object(...): Object '%s' has no usable mesh data", ob->id.name + 2);
		return NULL;
	}

	bm = self->bm;

	DM_to_bmesh_ex(dm, bm);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bmesh_from_mesh_doc,
".. method:: from_mesh(mesh, use_shape_key=False, shape_key_index=0)\n"
"\n"
"   Initialize this bmesh from existing mesh datablock.\n"
"\n"
"   :arg mesh: The mesh data to load.\n"
"   :type mesh: :class:`Mesh`\n"
"   :arg use_shape_key: Use the locations from a shape key.\n"
"   :type use_shape_key: boolean\n"
"   :arg shape_key_index: The shape key index to use.\n"
"   :type shape_key_index: int\n"
);
static PyObject *bpy_bmesh_from_mesh(BPy_BMesh *self, PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"mesh", "use_shape_key", "shape_key_index", NULL};
	BMesh *bm;
	PyObject *py_mesh;
	Mesh *me;
	int use_shape_key = FALSE;
	int shape_key_index = 0;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "O|ii:from_mesh", (char **)kwlist,
	                                 &py_mesh, &use_shape_key, &shape_key_index) ||
	    !(me = PyC_RNA_AsPointer(py_mesh, "Mesh")))
	{
		return NULL;
	}

	bm = self->bm;

	BM_mesh_bm_from_me(bm, me, use_shape_key, shape_key_index + 1);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bmesh_select_flush_mode_doc,
".. method:: select_flush_mode()\n"
"\n"
"   flush selection based on the current mode current :class:`BMesh.select_mode`.\n"
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
"   Flush selection, independent of the current selection mode.\n"
"\n"
"   :arg select: flush selection or de-selected elements.\n"
"   :type select: boolean\n"
);
static PyObject *bpy_bmesh_select_flush(BPy_BMesh *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_OBJ(self);

	param = PyLong_AsLong(value);
	if (param != FALSE && param != TRUE) {
		PyErr_SetString(PyExc_TypeError,
		                "expected a boolean type 0/1");
		return NULL;
	}

	if (param)  BM_mesh_select_flush(self->bm);
	else        BM_mesh_deselect_flush(self->bm);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bmesh_normal_update_doc,
".. method:: normal_update(skip_hidden=False)\n"
"\n"
"   Update mesh normals.\n"
"\n"
"   :arg skip_hidden: When True hidden elements are ignored.\n"
"   :type skip_hidden: boolean\n"
);
static PyObject *bpy_bmesh_normal_update(BPy_BMesh *self, PyObject *args)
{

	int skip_hidden = FALSE;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "|i:normal_update", &skip_hidden)) {
		return NULL;
	}

	BM_mesh_normals_update(self->bm, skip_hidden);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bmesh_transform_doc,
".. method:: transform(matrix, filter=None)\n"
"\n"
"   Transform the mesh (optionally filtering flagged data only).\n"
"\n"
"   :arg matrix: transform matrix.\n"
"   :type matrix: 4x4 :class:`mathutils.Matrix`\n"
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
			PyErr_SetString(PyExc_ValueError,
			                "expected a 4x4 matrix");
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
"   Set the selection.\n"
"   This is different from the *select* attribute because it updates the selection state of assosiated geometry.\n"
"\n"
"   :arg select: Select or de-select.\n"
"   :type select: boolean\n"
"\n"
"   .. note::\n"
"\n"
"      Currently this only flushes down, so selecting a face will select all its vertices but de-selecting a vertex "
"      won't de-select all the faces that use it, before finishing with a mesh typically flushing is still needed.\n"
);
static PyObject *bpy_bm_elem_select_set(BPy_BMElem *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_OBJ(self);

	param = PyLong_AsLong(value);
	if (param != FALSE && param != TRUE) {
		PyErr_SetString(PyExc_TypeError,
		                "expected a boolean type 0/1");
		return NULL;
	}

	BM_elem_select_set(self->bm, self->ele, param);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bm_elem_hide_set_doc,
".. method:: hide_set(hide)\n"
"\n"
"   Set the hide state.\n"
"   This is different from the *hide* attribute because it updates the selection and hide state of assosiated geometry.\n"
"\n"
"   :arg hide: Hidden or visible.\n"
"   :type hide: boolean\n"
);
static PyObject *bpy_bm_elem_hide_set(BPy_BMElem *self, PyObject *value)
{
	int param;

	BPY_BM_CHECK_OBJ(self);

	param = PyLong_AsLong(value);
	if (param != FALSE && param != TRUE) {
		PyErr_SetString(PyExc_TypeError,
		                "expected a boolean type 0/1");
		return NULL;
	}

	BM_elem_hide_set(self->bm, self->ele, param);

	Py_RETURN_NONE;
}


PyDoc_STRVAR(bpy_bm_elem_copy_from_doc,
".. method:: copy_from(other)\n"
"\n"
"   Copy values from another element of matching type.\n"
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

	if (value->ele != self->ele) {
		BM_elem_attrs_copy(value->bm, self->bm, value->ele, self->ele);
	}

	Py_RETURN_NONE;
}


/* Vert
 * ---- */


PyDoc_STRVAR(bpy_bmvert_copy_from_vert_interp_doc,
".. method:: copy_from_vert_interp(vert_pair, fac)\n"
"\n"
"   Interpolate the customdata from a vert between 2 other verts.\n"
"\n"
"   :arg vert_pair: The vert to interpolate data from.\n"
"   :type vert_pair: :class:`BMVert`\n"
);
static PyObject *bpy_bmvert_copy_from_vert_interp(BPy_BMVert *self, PyObject *args)
{
	PyObject *vert_seq;
	float fac;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "Of:BMVert.copy_from_vert_interp",
	                      &vert_seq, &fac))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMVert **vert_array = NULL;
		Py_ssize_t vert_seq_len; /* always 2 */

		vert_array = BPy_BMElem_PySeq_As_Array(&bm, vert_seq, 2, 2,
		                                       &vert_seq_len, BM_VERT,
		                                       TRUE, TRUE, "BMVert.copy_from_vert_interp(...)");

		if (vert_array == NULL) {
			return NULL;
		}

		BM_data_interp_from_verts(bm, vert_array[0], vert_array[1], self->v, CLAMPIS(fac, 0.0f, 1.0f));

		PyMem_FREE(vert_array);
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bmvert_copy_from_face_interp_doc,
".. method:: copy_from_face_interp(face)\n"
"\n"
"   Interpolate the customdata from a face onto this loop (the loops vert should overlap the face).\n"
"\n"
"   :arg face: The face to interpolate data from.\n"
"   :type face: :class:`BMFace`\n"
);
static PyObject *bpy_bmvert_copy_from_face_interp(BPy_BMVert *self, PyObject *args)
{
	BPy_BMFace *py_face = NULL;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O!:BMVert.copy_from_face_interp",
	                      &BPy_BMFace_Type, &py_face))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(py_face);

		if (py_face->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "BMVert.copy_from_face_interp(face): face is from another mesh");
			return NULL;
		}

		BM_vert_interp_from_face(bm, self->v, py_face->f);

		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bmvert_calc_edge_angle_doc,
".. method:: calc_edge_angle()\n"
"\n"
"   Return the angle between 2 connected edges.\n"
"\n"
"   :return: The angle between both edges in radians.\n"
"   :rtype: float\n"
);
static PyObject *bpy_bmvert_calc_edge_angle(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyFloat_FromDouble(BM_vert_edge_angle(self->v));
}


PyDoc_STRVAR(bpy_bmvert_normal_update_doc,
".. method:: normal_update()\n"
"\n"
"   Update vertex normal.\n"
);
static PyObject *bpy_bmvert_normal_update(BPy_BMVert *self)
{
	BPY_BM_CHECK_OBJ(self);

	BM_vert_normal_update(self->bm, self->v);

	Py_RETURN_NONE;
}


/* Edge
 * ---- */

PyDoc_STRVAR(bpy_bmedge_calc_length_doc,
".. method:: calc_length()\n"
"\n"
"   :return: The length between both verts.\n"
"   :rtype: float\n"
);
static PyObject *bpy_bmedge_calc_length(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyFloat_FromDouble(len_v3v3(self->e->v1->co, self->e->v2->co));
}

PyDoc_STRVAR(bpy_bmedge_calc_face_angle_doc,
".. method:: calc_face_angle()\n"
"\n"
"   :return: The angle between 2 connected faces in radians.\n"
"   :rtype: float\n"
);
static PyObject *bpy_bmedge_calc_face_angle(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyFloat_FromDouble(BM_edge_face_angle(self->e));
}


PyDoc_STRVAR(bpy_bmedge_other_vert_doc,
".. method:: other_vert(vert)\n"
"\n"
"   Return the other vertex on this edge or None if the vertex is not used by this edge.\n"
"\n"
"   :arg vert: a vert in this edge.\n"
"   :type vert: :class:`BMVert`\n"
"   :return: The edges other vert.\n"
"   :rtype: :class:`BMVert` or None\n"
);
static PyObject *bpy_bmedge_other_vert(BPy_BMEdge *self, BPy_BMVert *value)
{
	BMVert *other;
	BPY_BM_CHECK_OBJ(self);

	if (!BPy_BMVert_Check(value)) {
		PyErr_Format(PyExc_TypeError,
		             "BMEdge.other_vert(vert): BMVert expected, not '%.200s'",
		             Py_TYPE(value)->tp_name);
		return NULL;
	}

	BPY_BM_CHECK_OBJ(value);

	if (self->bm != value->bm) {
		PyErr_SetString(PyExc_ValueError,
		                "BMEdge.other_vert(vert): vert is from another mesh");
		return NULL;
	}

	other = BM_edge_other_vert(self->e, value->v);

	if (other) {
		return BPy_BMVert_CreatePyObject(self->bm, other);
	}
	else {
		/* could raise an exception here */
		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bmedge_normal_update_doc,
".. method:: normal_update()\n"
"\n"
"   Update edges vertex normals.\n"
);
static PyObject *bpy_bmedge_normal_update(BPy_BMEdge *self)
{
	BPY_BM_CHECK_OBJ(self);

	BM_edge_normals_update(self->bm, self->e);

	Py_RETURN_NONE;
}


/* Face
 * ---- */

PyDoc_STRVAR(bpy_bmface_copy_from_face_interp_doc,
".. method:: copy_from_face_interp(face)\n"
"\n"
"   Interpolate the customdata from another face onto this one (faces should overlap).\n"
"\n"
"   :arg face: The face to interpolate data from.\n"
"   :type face: :class:`BMFace`\n"
);
static PyObject *bpy_bmface_copy_from_face_interp(BPy_BMFace *self, PyObject *args)
{
	BPy_BMFace *py_face = NULL;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O!:BMFace.copy_from_face_interp",
	                      &BPy_BMFace_Type, &py_face))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(py_face);

		if (py_face->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "BMFace.copy_from_face_interp(face): face is from another mesh");
			return NULL;
		}

		BM_face_interp_from_face(bm, self->f, py_face->f);

		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bmface_copy_doc,
".. method:: copy(verts=True, edges=True)\n"
"\n"
"   Make a copy of this face.\n"
"\n"
"   :arg verts: When set, the faces verts will be duplicated too.\n"
"   :type verts: boolean\n"
"   :arg edges: When set, the faces edges will be duplicated too.\n"
"   :type edges: boolean\n"
"   :return: The newly created face.\n"
"   :rtype: :class:`BMFace`\n"
);
static PyObject *bpy_bmface_copy(BPy_BMFace *self, PyObject *args, PyObject *kw)
{
	static const char *kwlist[] = {"verts", "edges", NULL};

	BMesh *bm = self->bm;
	int do_verts = TRUE;
	int do_edges = TRUE;

	BMFace *f_cpy;
	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTupleAndKeywords(args, kw,
	                                 "|ii:BMFace.copy",
	                                 (char **)kwlist,
	                                 &do_verts, &do_edges))
	{
		return NULL;
	}

	f_cpy = BM_face_copy(bm, self->f, do_verts, do_edges);

	if (f_cpy) {
		return BPy_BMFace_CreatePyObject(bm, f_cpy);
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "BMFace.copy(): couldn't create the new face, internal error");
		return NULL;
	}
}


PyDoc_STRVAR(bpy_bmface_calc_area_doc,
".. method:: calc_area()\n"
"\n"
"   Return the area of the face.\n"
"\n"
"   :return: Return the area of the face.\n"
"   :rtype: float\n"
);
static PyObject *bpy_bmface_calc_area(BPy_BMFace *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyFloat_FromDouble(BM_face_area_calc(self->bm, self->f));
}


PyDoc_STRVAR(bpy_bmface_calc_center_mean_doc,
".. method:: calc_center_median()\n"
"\n"
"   Return median center of the face.\n"
"\n"
"   :return: a 3D vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *bpy_bmface_calc_center_mean(BPy_BMFace *self)
{
	float cent[3];

	BPY_BM_CHECK_OBJ(self);
	BM_face_center_mean_calc(self->bm, self->f, cent);
	return Vector_CreatePyObject(cent, 3, Py_NEW, NULL);
}


PyDoc_STRVAR(bpy_bmface_calc_center_bounds_doc,
".. method:: calc_center_bounds()\n"
"\n"
"   Return bounds center of the face.\n"
"\n"
"   :return: a 3D vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *bpy_bmface_calc_center_bounds(BPy_BMFace *self)
{
	float cent[3];

	BPY_BM_CHECK_OBJ(self);
	BM_face_center_bounds_calc(self->bm, self->f, cent);
	return Vector_CreatePyObject(cent, 3, Py_NEW, NULL);
}


PyDoc_STRVAR(bpy_bmface_normal_update_doc,
".. method:: normal_update()\n"
"\n"
"   Update faces normal.\n"
);
static PyObject *bpy_bmface_normal_update(BPy_BMFace *self)
{
	BPY_BM_CHECK_OBJ(self);

	BM_face_normal_update(self->bm, self->f);

	Py_RETURN_NONE;
}


/* Loop
 * ---- */

PyDoc_STRVAR(bpy_bmloop_copy_from_face_interp_doc,
".. method:: copy_from_face_interp(face, vert=True, multires=True)\n"
"\n"
"   Interpolate the customdata from a face onto this loop (the loops vert should overlap the face).\n"
"\n"
"   :arg face: The face to interpolate data from.\n"
"   :type face: :class:`BMFace`\n"
"   :arg vert: When enabled, interpolate the loops vertex data (optional).\n"
"   :type vert: boolean\n"
"   :arg multires: When enabled, interpolate the loops multires data (optional).\n"
"   :type multires: boolean\n"
);
static PyObject *bpy_bmloop_copy_from_face_interp(BPy_BMLoop *self, PyObject *args)
{
	BPy_BMFace *py_face = NULL;
	int do_vertex   = TRUE;
	int do_multires = TRUE;

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O!|ii:BMLoop.copy_from_face_interp",
	                      &BPy_BMFace_Type, &py_face,
	                      &do_vertex, &do_multires))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(py_face);

		if (py_face->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "BMLoop.copy_from_face_interp(face): face is from another mesh");
			return NULL;
		}

		BM_loop_interp_from_face(bm, self->l, py_face->f, do_vertex, do_multires);

		Py_RETURN_NONE;
	}
}


PyDoc_STRVAR(bpy_bmloop_calc_angle_doc,
".. method:: calc_angle()\n"
"\n"
"   Return the angle at this loops corner of the face.\n"
"   This is calculated so sharper corners give lower angles.\n"
"\n"
"   :return: The angle in radians.\n"
"   :rtype: float\n"
);
static PyObject *bpy_bmloop_calc_angle(BPy_BMLoop *self)
{
	BPY_BM_CHECK_OBJ(self);
	return PyFloat_FromDouble(BM_loop_face_angle(self->l));
}

PyDoc_STRVAR(bpy_bmloop_calc_normal_doc,
".. method:: calc_normal()\n"
"\n"
"   Return normal at this loops corner of the face.\n"
"   Falls back to the face normal for straignt lines.\n"
"\n"
"   :return: a normalized vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *bpy_bmloop_calc_normal(BPy_BMLoop *self)
{
	float vec[3];
	BPY_BM_CHECK_OBJ(self);
	BM_loop_face_normal(self->l, vec);
	return Vector_CreatePyObject(vec, 3, Py_NEW, NULL);
}

PyDoc_STRVAR(bpy_bmloop_calc_tangent_doc,
".. method:: calc_tangent()\n"
"\n"
"   Return the tangent at this loops corner of the face (pointing inward into the face).\n"
"   Falls back to the face normal for straignt lines.\n"
"\n"
"   :return: a normalized vector.\n"
"   :rtype: :class:`mathutils.Vector`\n"
);
static PyObject *bpy_bmloop_calc_tangent(BPy_BMLoop *self)
{
	float vec[3];
	BPY_BM_CHECK_OBJ(self);
	BM_loop_face_tangent(self->l, vec);
	return Vector_CreatePyObject(vec, 3, Py_NEW, NULL);
}

/* Vert Seq
 * -------- */
PyDoc_STRVAR(bpy_bmvertseq_new_doc,
".. method:: new(co=(0.0, 0.0, 0.0), example=None)\n"
"\n"
"   Create a new vertex.\n"
"\n"
"   :arg co: The initial location of the vertex (optional argument).\n"
"   :type co: float triplet\n"
"   :arg example: Existing vert to initialize settings.\n"
"   :type example: :class:`BMVert`\n"
"   :return: The newly created edge.\n"
"   :rtype: :class:`BMVert`\n"
);
static PyObject *bpy_bmvertseq_new(BPy_BMElemSeq *self, PyObject *args)
{
	PyObject *py_co = NULL;
	BPy_BMVert *py_vert_example = NULL; /* optional */

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "|OO!:verts.new",
	                      &py_co,
	                      &BPy_BMVert_Type, &py_vert_example))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMVert *v;
		float co[3] = {0.0f, 0.0f, 0.0f};

		if (py_vert_example) {
			BPY_BM_CHECK_OBJ(py_vert_example);
		}

		if (py_co && mathutils_array_parse(co, 3, 3, py_co, "verts.new(co)") == -1) {
			return NULL;
		}

		v = BM_vert_create(bm, co, NULL);

		if (v == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): couldn't create the new face, internal error");
			return NULL;
		}

		if (py_vert_example) {
			BM_elem_attrs_copy(py_vert_example->bm, bm, py_vert_example->v, v);
		}

		return BPy_BMVert_CreatePyObject(bm, v);
	}
}


/* Edge Seq
 * -------- */
PyDoc_STRVAR(bpy_bmedgeseq_new_doc,
".. method:: new(verts, example=None)\n"
"\n"
"   Create a new edge from a given pair of verts.\n"
"\n"
"   :arg verts: Vertex pair.\n"
"   :type verts: pair of :class:`BMVert`\n"
"   :arg example: Existing edge to initialize settings (optional argument).\n"
"   :type example: :class:`BMEdge`\n"
"   :return: The newly created edge.\n"
"   :rtype: :class:`BMEdge`\n"
);
static PyObject *bpy_bmedgeseq_new(BPy_BMElemSeq *self, PyObject *args)
{
	PyObject *vert_seq;
	BPy_BMEdge *py_edge_example = NULL; /* optional */

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|O!:edges.new",
	                      &vert_seq,
	                      &BPy_BMEdge_Type, &py_edge_example))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMEdge *e;
		BMVert **vert_array = NULL;
		Py_ssize_t vert_seq_len; /* always 2 */
		PyObject *ret = NULL;

		if (py_edge_example) {
			BPY_BM_CHECK_OBJ(py_edge_example);
		}

		vert_array = BPy_BMElem_PySeq_As_Array(&bm, vert_seq, 2, 2,
		                                       &vert_seq_len, BM_VERT,
		                                       TRUE, TRUE, "edges.new(...)");

		if (vert_array == NULL) {
			return NULL;
		}
		
		if (BM_edge_exists(vert_array[0], vert_array[1])) {
			PyErr_SetString(PyExc_ValueError,
			                "edges.new(): this edge exists");
			goto cleanup;
		}

		e = BM_edge_create(bm, vert_array[0], vert_array[1], NULL, FALSE);

		if (e == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): couldn't create the new face, internal error");
			goto cleanup;
		}

		if (py_edge_example) {
			BM_elem_attrs_copy(py_edge_example->bm, bm, py_edge_example->e, e);
		}

		ret = BPy_BMEdge_CreatePyObject(bm, e);

cleanup:
		if (vert_array) PyMem_FREE(vert_array);
		return ret;
	}
}


/* Face Seq
 * -------- */
PyDoc_STRVAR(bpy_bmfaceseq_new_doc,
".. method:: new(verts, example=None)\n"
"\n"
"   Create a new face from a given set of verts.\n"
"\n"
"   :arg verts: Sequence of 3 or more verts.\n"
"   :type verts: :class:`BMVert`\n"
"   :arg example: Existing face to initialize settings (optional argument).\n"
"   :type example: :class:`BMFace`\n"
"   :return: The newly created face.\n"
"   :rtype: :class:`BMFace`\n"
);
static PyObject *bpy_bmfaceseq_new(BPy_BMElemSeq *self, PyObject *args)
{
	PyObject *vert_seq;
	BPy_BMFace *py_face_example = NULL; /* optional */

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|O!:faces.new",
	                      &vert_seq,
	                      &BPy_BMFace_Type, &py_face_example))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		Py_ssize_t vert_seq_len;
		Py_ssize_t i, i_next;

		BMVert **vert_array = NULL;
		BMEdge **edge_array = NULL;

		PyObject *ret = NULL;

		BMFace *f_new;

		if (py_face_example) {
			BPY_BM_CHECK_OBJ(py_face_example);
		}

		vert_array = BPy_BMElem_PySeq_As_Array(&bm, vert_seq, 3, PY_SSIZE_T_MAX,
		                                       &vert_seq_len, BM_VERT,
		                                       TRUE, TRUE, "faces.new(...)");

		if (vert_array == NULL) {
			return NULL;
		}

		/* check if the face exists */
		if (BM_face_exists(bm, vert_array, vert_seq_len, NULL)) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): face already exists");
			goto cleanup;
		}

		/* Go ahead and make the face!
		 * --------------------------- */

		edge_array = (BMEdge **)PyMem_MALLOC(vert_seq_len * sizeof(BMEdge **));

		/* ensure edges */
		for (i = vert_seq_len - 1, i_next = 0; i_next < vert_seq_len; (i = i_next++)) {
			edge_array[i] = BM_edge_create(bm, vert_array[i], vert_array[i_next], NULL, TRUE);
		}

		f_new = BM_face_create(bm, vert_array, edge_array, vert_seq_len, FALSE);

		if (f_new == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.new(verts): couldn't create the new face, internal error");
			goto cleanup;
		}

		if (py_face_example) {
			BM_elem_attrs_copy(py_face_example->bm, bm, py_face_example->f, f_new);
		}

		ret = BPy_BMFace_CreatePyObject(bm, f_new);

		/* pass through */
cleanup:
		if (vert_array) PyMem_FREE(vert_array);
		if (edge_array) PyMem_FREE(edge_array);
		return ret;
	}
}

/* Elem Seq
 * -------- */

PyDoc_STRVAR(bpy_bmvertseq_remove_doc,
".. method:: remove(vert)\n"
"\n"
"   Remove a vert.\n"
);
static PyObject *bpy_bmvertseq_remove(BPy_BMElemSeq *self, BPy_BMVert *value)
{
	BPY_BM_CHECK_OBJ(self);

	if (!BPy_BMVert_Check(value)) {
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(value);

		if (value->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "verts.remove(vert): vert is from another mesh");
			return NULL;
		}

		BM_vert_kill(bm, value->v);
		bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(bpy_bmedgeseq_remove_doc,
".. method:: remove(edge)\n"
"\n"
"   Remove a edge.\n"
);
static PyObject *bpy_bmedgeseq_remove(BPy_BMElemSeq *self, BPy_BMEdge *value)
{
	BPY_BM_CHECK_OBJ(self);

	if (!BPy_BMEdge_Check(value)) {
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(value);

		if (value->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "edges.remove(edge): edge is from another mesh");
			return NULL;
		}

		BM_edge_kill(bm, value->e);
		bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(bpy_bmfaceseq_remove_doc,
".. method:: remove(face)\n"
"\n"
"   Remove a face.\n"
);
static PyObject *bpy_bmfaceseq_remove(BPy_BMElemSeq *self, BPy_BMFace *value)
{
	BPY_BM_CHECK_OBJ(self);

	if (!BPy_BMFace_Check(value)) {
		return NULL;
	}
	else {
		BMesh *bm = self->bm;

		BPY_BM_CHECK_OBJ(value);

		if (value->bm != bm) {
			PyErr_SetString(PyExc_ValueError,
			                "faces.remove(face): face is from another mesh");
			return NULL;
		}

		BM_face_kill(bm, value->f);
		bpy_bm_generic_invalidate((BPy_BMGeneric *)value);

		Py_RETURN_NONE;
	}
}

PyDoc_STRVAR(bpy_bmedgeseq_get__method_doc,
".. method:: get(verts, fallback=None)\n"
"\n"
"   Return a edge which uses the **verts** passed.\n"
"\n"
"   :arg verts: Sequence of verts.\n"
"   :type verts: :class:`BMVert`\n"
"   :arg fallback: Return this value if nothing is found.\n"
"   :return: The edge found or None\n"
"   :rtype: :class:`BMEdge`\n"
);
static PyObject *bpy_bmedgeseq_get__method(BPy_BMElemSeq *self, PyObject *args)
{
	PyObject *vert_seq;
	PyObject *fallback = Py_None; /* optional */

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|O:edges.get",
	                      &vert_seq,
	                      &fallback))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMEdge *e;
		BMVert **vert_array = NULL;
		Py_ssize_t vert_seq_len; /* always 2 */
		PyObject *ret = NULL;

		vert_array = BPy_BMElem_PySeq_As_Array(&bm, vert_seq, 2, 2,
		                                       &vert_seq_len, BM_VERT,
		                                       TRUE, TRUE, "edges.get(...)");

		if (vert_array == NULL) {
			return NULL;
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
}

PyDoc_STRVAR(bpy_bmfaceseq_get__method_doc,
".. method:: get(verts, fallback=None)\n"
"\n"
"   Return a face which uses the **verts** passed.\n"
"\n"
"   :arg verts: Sequence of verts.\n"
"   :type verts: :class:`BMVert`\n"
"   :arg fallback: Return this value if nothing is found.\n"
"   :return: The face found or None\n"
"   :rtype: :class:`BMFace`\n"
);
static PyObject *bpy_bmfaceseq_get__method(BPy_BMElemSeq *self, PyObject *args)
{
	PyObject *vert_seq;
	PyObject *fallback = Py_None; /* optional */

	BPY_BM_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|O:faces.get",
	                      &vert_seq,
	                      &fallback))
	{
		return NULL;
	}
	else {
		BMesh *bm = self->bm;
		BMFace *f = NULL;
		BMVert **vert_array = NULL;
		Py_ssize_t vert_seq_len;
		PyObject *ret = NULL;

		vert_array = BPy_BMElem_PySeq_As_Array(&bm, vert_seq, 1, PY_SSIZE_T_MAX,
		                                       &vert_seq_len, BM_VERT,
		                                       TRUE, TRUE, "faces.get(...)");

		if (vert_array == NULL) {
			return NULL;
		}

		if (BM_face_exists(bm, vert_array, vert_seq_len, &f)) {
			ret = BPy_BMFace_CreatePyObject(bm, f);
		}
		else {
			ret = fallback;
			Py_INCREF(ret);
		}

		PyMem_FREE(vert_array);
		return ret;
	}
}

PyDoc_STRVAR(bpy_bmelemseq_index_update_doc,
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
"      Running this on sequences besides :class:`BMesh.verts`, :class:`BMesh.edges`, :class:`BMesh.faces`\n"
"      works but wont result in each element having a valid index, insted its order in the sequence will be set.\n"
);
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
		default:
		{
			BMIter iter;
			BMElem *ele;
			int index = 0;
			const char htype = bm_iter_itype_htype_map[self->itype];

			BM_ITER_BPY_BM_SEQ(ele, &iter, self) {
				BM_elem_index_set(ele, index); /* set_dirty! */
				index++;
			}

			if (htype & (BM_VERT | BM_EDGE | BM_FACE)) {
				/* since this isn't the normal vert/edge/face loops,
				 * we're setting dirty values here. so tag as dirty. */
				bm->elem_index_dirty |= htype;
			}

			break;
		}
	}

	Py_RETURN_NONE;
}


static struct PyMethodDef bpy_bmesh_methods[] = {
    /* utility */
    {"copy",  (PyCFunction)bpy_bmesh_copy,  METH_NOARGS, bpy_bmesh_copy_doc},
    {"clear", (PyCFunction)bpy_bmesh_clear, METH_NOARGS, bpy_bmesh_clear_doc},
    {"free",  (PyCFunction)bpy_bmesh_free,  METH_NOARGS, bpy_bmesh_free_doc},

    /* conversion */
    {"from_object", (PyCFunction)bpy_bmesh_from_object, METH_VARARGS | METH_KEYWORDS, bpy_bmesh_from_object_doc},
    {"from_mesh",   (PyCFunction)bpy_bmesh_from_mesh,   METH_VARARGS | METH_KEYWORDS, bpy_bmesh_from_mesh_doc},
    {"to_mesh",     (PyCFunction)bpy_bmesh_to_mesh,     METH_VARARGS,                 bpy_bmesh_to_mesh_doc},

    /* meshdata */
    {"select_flush_mode", (PyCFunction)bpy_bmesh_select_flush_mode, METH_NOARGS, bpy_bmesh_select_flush_mode_doc},
    {"select_flush", (PyCFunction)bpy_bmesh_select_flush, METH_O, bpy_bmesh_select_flush_doc},
    {"normal_update", (PyCFunction)bpy_bmesh_normal_update, METH_VARARGS, bpy_bmesh_normal_update_doc},
    {"transform", (PyCFunction)bpy_bmesh_transform, METH_VARARGS|METH_KEYWORDS, bpy_bmesh_transform_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmvert_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp", (PyCFunction)bpy_bmvert_copy_from_face_interp, METH_VARARGS, bpy_bmvert_copy_from_face_interp_doc},
    {"copy_from_vert_interp", (PyCFunction)bpy_bmvert_copy_from_vert_interp, METH_VARARGS, bpy_bmvert_copy_from_vert_interp_doc},

    {"calc_vert_angle", (PyCFunction)bpy_bmvert_calc_edge_angle, METH_NOARGS, bpy_bmvert_calc_edge_angle_doc},

    {"normal_update",  (PyCFunction)bpy_bmvert_normal_update,  METH_NOARGS,  bpy_bmvert_normal_update_doc},

    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmedge_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},

    {"other_vert", (PyCFunction)bpy_bmedge_other_vert, METH_O, bpy_bmedge_other_vert_doc},

    {"calc_length",     (PyCFunction)bpy_bmedge_calc_length,     METH_NOARGS, bpy_bmedge_calc_length_doc},
    {"calc_face_angle", (PyCFunction)bpy_bmedge_calc_face_angle, METH_NOARGS, bpy_bmedge_calc_face_angle_doc},

    {"normal_update",  (PyCFunction)bpy_bmedge_normal_update,  METH_NOARGS,  bpy_bmedge_normal_update_doc},

    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmface_methods[] = {
    {"select_set", (PyCFunction)bpy_bm_elem_select_set, METH_O, bpy_bm_elem_select_set_doc},
    {"hide_set", (PyCFunction)bpy_bm_elem_hide_set, METH_O, bpy_bm_elem_hide_set_doc},

    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp", (PyCFunction)bpy_bmface_copy_from_face_interp, METH_O, bpy_bmface_copy_from_face_interp_doc},

    {"copy", (PyCFunction)bpy_bmface_copy, METH_VARARGS|METH_KEYWORDS, bpy_bmface_copy_doc},

    {"calc_area",          (PyCFunction)bpy_bmface_calc_area,          METH_NOARGS, bpy_bmface_calc_area_doc},
    {"calc_center_median", (PyCFunction)bpy_bmface_calc_center_mean,   METH_NOARGS, bpy_bmface_calc_center_mean_doc},
    {"calc_center_bounds", (PyCFunction)bpy_bmface_calc_center_bounds, METH_NOARGS, bpy_bmface_calc_center_bounds_doc},

    {"normal_update",  (PyCFunction)bpy_bmface_normal_update,  METH_NOARGS,  bpy_bmface_normal_update_doc},

    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmloop_methods[] = {
    {"copy_from", (PyCFunction)bpy_bm_elem_copy_from, METH_O, bpy_bm_elem_copy_from_doc},
    {"copy_from_face_interp", (PyCFunction)bpy_bmloop_copy_from_face_interp, METH_O, bpy_bmloop_copy_from_face_interp_doc},

    {"calc_angle",   (PyCFunction)bpy_bmloop_calc_angle,   METH_NOARGS, bpy_bmloop_calc_angle_doc},
    {"calc_normal",  (PyCFunction)bpy_bmloop_calc_normal,  METH_NOARGS, bpy_bmloop_calc_normal_doc},
    {"calc_tangent", (PyCFunction)bpy_bmloop_calc_tangent, METH_NOARGS, bpy_bmloop_calc_tangent_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmelemseq_methods[] = {
    /* odd function, initializes index values */
    {"index_update", (PyCFunction)bpy_bmelemseq_index_update, METH_NOARGS, bpy_bmelemseq_index_update_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmvertseq_methods[] = {
    {"new",     (PyCFunction)bpy_bmvertseq_new,         METH_VARARGS, bpy_bmvertseq_new_doc},
    {"remove",  (PyCFunction)bpy_bmvertseq_remove,      METH_O,       bpy_bmvertseq_remove_doc},

    /* odd function, initializes index values */
    {"index_update", (PyCFunction)bpy_bmelemseq_index_update, METH_NOARGS, bpy_bmelemseq_index_update_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmedgeseq_methods[] = {
    {"new",     (PyCFunction)bpy_bmedgeseq_new,         METH_VARARGS, bpy_bmedgeseq_new_doc},
    {"remove",  (PyCFunction)bpy_bmedgeseq_remove,      METH_O,       bpy_bmedgeseq_remove_doc},
    /* 'bpy_bmelemseq_get' for different purpose */
    {"get",     (PyCFunction)bpy_bmedgeseq_get__method, METH_VARARGS, bpy_bmedgeseq_get__method_doc},

    /* odd function, initializes index values */
    {"index_update", (PyCFunction)bpy_bmelemseq_index_update, METH_NOARGS, bpy_bmelemseq_index_update_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmfaceseq_methods[] = {
    {"new",     (PyCFunction)bpy_bmfaceseq_new,         METH_VARARGS, bpy_bmfaceseq_new_doc},
    {"remove",  (PyCFunction)bpy_bmfaceseq_remove,      METH_O,       bpy_bmfaceseq_remove_doc},
    /* 'bpy_bmelemseq_get' for different purpose */
    {"get",     (PyCFunction)bpy_bmfaceseq_get__method, METH_VARARGS, bpy_bmfaceseq_get__method_doc},

    /* odd function, initializes index values */
    {"index_update", (PyCFunction)bpy_bmelemseq_index_update, METH_NOARGS, bpy_bmelemseq_index_update_doc},
    {NULL, NULL, 0, NULL}
};

static struct PyMethodDef bpy_bmloopseq_methods[] = {
    /* odd function, initializes index values */
    {"index_update", (PyCFunction)bpy_bmelemseq_index_update, METH_NOARGS, bpy_bmelemseq_index_update_doc},
    {NULL, NULL, 0, NULL}
};

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

		case BM_ALL_LOOPS_OF_FACE:
		case BM_LOOPS_OF_FACE:
		case BM_LOOPS_OF_EDGE:
		case BM_LOOPS_OF_VERT:
		case BM_LOOPS_OF_LOOP:
			return &BPy_BMLoop_Type;
	}

	return NULL;
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

		BM_ITER_BPY_BM_SEQ(ele, &iter, self) {
			tot++;
		}
		return tot;
	}
}

static PyObject *bpy_bmelemseq_subscript_int(BPy_BMElemSeq *self, int keynum)
{
	BPY_BM_CHECK_OBJ(self);

	if (keynum < 0) keynum += bpy_bmelemseq_length(self); /* only get length on negative value, may loop entire seq */
	if (keynum >= 0) {
		BMHeader *ele = BM_iter_at_index(self->bm, self->itype, self->py_ele ? self->py_ele->ele : NULL, keynum);
		if (ele) {
			return BPy_BMElem_CreatePyObject(self->bm, ele);
		}
	}

	PyErr_Format(PyExc_IndexError,
	             "BMElemSeq[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *bpy_bmelemseq_subscript_slice(BPy_BMElemSeq *self, Py_ssize_t start, Py_ssize_t stop)
{
	BMIter iter;
	int count = 0;
	int ok;

	PyObject *list;
	PyObject *item;
	BMHeader *ele;

	BPY_BM_CHECK_OBJ(self);

	list = PyList_New(0);

	ok = BM_iter_init(&iter, self->bm, self->itype, self->py_ele ? self->py_ele->ele : NULL);

	BLI_assert(ok == TRUE);

	if (UNLIKELY(ok == FALSE)) {
		return list;
	}

	/* first loop up-until the start */
	for (ok = TRUE; ok; ok = (BM_iter_step(&iter) != NULL)) {
		if (count == start) {
			break;
		}
		count++;
	}

	/* add items until stop */
	while ((ele = BM_iter_step(&iter))) {
		item = BPy_BMElem_CreatePyObject(self->bm, ele);
		PyList_Append(list, item);
		Py_DECREF(item);

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
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return bpy_bmelemseq_subscript_int(self, i);
	}
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError,
			                "BMElemSeq[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return bpy_bmelemseq_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) return NULL;
			if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop))    return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = bpy_bmelemseq_length(self);
				if (start < 0) start += len;
				if (stop < 0) start += len;
			}

			if (stop - start <= 0) {
				return PyList_New(0);
			}
			else {
				return bpy_bmelemseq_subscript_slice(self, start, stop);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError,
		                "BMElemSeq[key]: invalid key, key must be an int");
		return NULL;
	}
}

static int bpy_bmelemseq_contains(BPy_BMElemSeq *self, PyObject *value)
{
	BPY_BM_CHECK_INT(self);

	if (Py_TYPE(value) == bpy_bm_itype_as_pytype(self->itype)) {
		BPy_BMElem *value_bm_ele = (BPy_BMElem *)value;
		if (value_bm_ele->bm == self->bm) {
			BMElem *ele, *ele_test = value_bm_ele->ele;
			BMIter iter;
			BM_ITER_BPY_BM_SEQ(ele, &iter, self) {
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
    (lenfunc)bpy_bmelemseq_length,                  /* sq_length */
    NULL,                                        /* sq_concat */
    NULL,                                        /* sq_repeat */
    (ssizeargfunc)bpy_bmelemseq_subscript_int,      /* sq_item */ /* Only set this so PySequence_Check() returns True */
    NULL,                                        /* sq_slice */
    (ssizeobjargproc)NULL,                       /* sq_ass_item */
    NULL,                                        /* *was* sq_ass_slice */
    (objobjproc)bpy_bmelemseq_contains,             /* sq_contains */
    (binaryfunc) NULL,                           /* sq_inplace_concat */
    (ssizeargfunc) NULL,                         /* sq_inplace_repeat */
};

static PyMappingMethods bpy_bmelemseq_as_mapping = {
    (lenfunc)bpy_bmelemseq_length,                  /* mp_length */
    (binaryfunc)bpy_bmelemseq_subscript,            /* mp_subscript */
    (objobjargproc)NULL,                         /* mp_ass_subscript */
};

/* for customdata access */
static PyMappingMethods bpy_bm_elem_as_mapping = {
    (lenfunc)NULL,                           /* mp_length */ /* keep this empty, messes up 'if elem: ...' test */
    (binaryfunc)bpy_bmelem_subscript,        /* mp_subscript */
    (objobjargproc)bpy_bmelem_ass_subscript, /* mp_ass_subscript */
};

/* Iterator
 * -------- */

static PyObject *bpy_bmelemseq_iter(BPy_BMElemSeq *self)
{
	BPy_BMIter *py_iter;

	BPY_BM_CHECK_OBJ(self);
	py_iter = (BPy_BMIter *)BPy_BMIter_CreatePyObject(self->bm);
	BM_iter_init(&(py_iter->iter), self->bm, self->itype, self->py_ele ? self->py_ele->ele : NULL);
	return (PyObject *)py_iter;
}

static PyObject *bpy_bmiter_next(BPy_BMIter *self)
{
	BMHeader *ele = BM_iter_step(&self->iter);
	if (ele == NULL) {
		PyErr_SetString(PyExc_StopIteration,
		                "bpy_bmiter_next stop");
		return NULL;
	}
	else {
		return (PyObject *)BPy_BMElem_CreatePyObject(self->bm, ele);
	}
}


/* Dealloc Functions
 * ================= */

static void bpy_bmesh_dealloc(BPy_BMesh *self)
{
	BMesh *bm = self->bm;

	/* have have been freed by bmesh */
	if (bm) {
		BM_data_layer_free(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
		BM_data_layer_free(bm, &bm->edata, CD_BM_ELEM_PYPTR);
		BM_data_layer_free(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
		BM_data_layer_free(bm, &bm->ldata, CD_BM_ELEM_PYPTR);

		bm->py_handle = NULL;

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
		void **ptr = CustomData_bmesh_get(&bm->vdata, self->ele->head.data, CD_BM_ELEM_PYPTR);
		*ptr = NULL;
	}
	PyObject_DEL(self);
}

static void bpy_bmedge_dealloc(BPy_BMElem *self)
{
	BMesh *bm = self->bm;
	if (bm) {
		void **ptr = CustomData_bmesh_get(&bm->edata, self->ele->head.data, CD_BM_ELEM_PYPTR);
		*ptr = NULL;
	}
	PyObject_DEL(self);
}

static void bpy_bmface_dealloc(BPy_BMElem *self)
{
	BMesh *bm = self->bm;
	if (bm) {
		void **ptr = CustomData_bmesh_get(&bm->pdata, self->ele->head.data, CD_BM_ELEM_PYPTR);
		*ptr = NULL;
	}
	PyObject_DEL(self);
}

static void bpy_bmloop_dealloc(BPy_BMElem *self)
{
	BMesh *bm = self->bm;
	if (bm) {
		void **ptr = CustomData_bmesh_get(&bm->ldata, self->ele->head.data, CD_BM_ELEM_PYPTR);
		*ptr = NULL;
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
	return _Py_HashPointer(((BPy_BMElem *)self)->ele);
}

static Py_hash_t bpy_bm_hash(PyObject *self)
{
	return _Py_HashPointer(((BPy_BMesh *)self)->bm);
}

/* Type Docstrings
 * =============== */

PyDoc_STRVAR(bpy_bmesh_doc,
"The BMesh data structure\n"
);
PyDoc_STRVAR(bpy_bmvert_doc,
"The BMesh vertex type\n"
);
PyDoc_STRVAR(bpy_bmedge_doc,
"The BMesh edge connecting 2 verts\n"
);
PyDoc_STRVAR(bpy_bmface_doc,
"The BMesh face with 3 or more sides\n"
);
PyDoc_STRVAR(bpy_bmloop_doc,
"This is normally accessed from :class:`BMFace.loops` where each face corner represents a corner of a face.\n"
);
PyDoc_STRVAR(bpy_bmelemseq_doc,
"General sequence type used for accessing any sequence of \n"
":class:`BMVert`, :class:`BMEdge`, :class:`BMFace`, :class:`BMLoop`.\n"
"\n"
"When accessed via :class:`BMesh.verts`, :class:`BMesh.edges`, :class:`BMesh.faces` \n"
"there are also functions to create/remomove items.\n"
);
PyDoc_STRVAR(bpy_bmiter_doc,
"Internal BMesh type for looping over verts/faces/edges,\n"
"used for iterating over :class:`BMElemSeq` types.\n"
);

static PyObject *bpy_bmesh_repr(BPy_BMesh *self)
{
	BMesh *bm = self->bm;

	if (bm) {
		return PyUnicode_FromFormat("<BMesh(%p), totvert=%d, totedge=%d, totface=%d, totloop=%d>",
		                            bm, bm->totvert, bm->totedge, bm->totface, bm->totloop);
	}
	else {
		return PyUnicode_FromFormat("<BMesh dead at %p>", self);
	}
}

static PyObject *bpy_bmvert_repr(BPy_BMVert *self)
{
	BMesh *bm = self->bm;

	if (bm) {
		BMVert *v = self->v;
		return PyUnicode_FromFormat("<BMVert(%p), index=%d>",
		                            v, BM_elem_index_get(v));
	}
	else {
		return PyUnicode_FromFormat("<BMVert dead at %p>", self);
	}
}

static PyObject *bpy_bmedge_repr(BPy_BMEdge *self)
{
	BMesh *bm = self->bm;

	if (bm) {
		BMEdge *e = self->e;
		return PyUnicode_FromFormat("<BMEdge(%p), index=%d, verts=(%p/%d, %p/%d)>",
		                            e, BM_elem_index_get(e),
		                            e->v1, BM_elem_index_get(e->v1),
		                            e->v2, BM_elem_index_get(e->v2));
	}
	else {
		return PyUnicode_FromFormat("<BMEdge dead at %p>", self);
	}
}

static PyObject *bpy_bmface_repr(BPy_BMFace *self)
{
	BMesh *bm = self->bm;

	if (bm) {
		BMFace *f = self->f;
		return PyUnicode_FromFormat("<BMFace(%p), index=%d, totverts=%d>",
		                            f, BM_elem_index_get(f),
		                            f->len);
	}
	else {
		return PyUnicode_FromFormat("<BMFace dead at %p>", self);
	}
}

static PyObject *bpy_bmloop_repr(BPy_BMLoop *self)
{
	BMesh *bm = self->bm;

	if (bm) {
		BMLoop *l = self->l;
		return PyUnicode_FromFormat("<BMLoop(%p), index=%d, vert=%p/%d, edge=%p/%d, face=%p/%d>",
		                            l, BM_elem_index_get(l),
		                            l->v, BM_elem_index_get(l->v),
		                            l->e, BM_elem_index_get(l->e),
		                            l->f, BM_elem_index_get(l->f));
	}
	else {
		return PyUnicode_FromFormat("<BMLoop dead at %p>", self);
	}
}

/* Types
 * ===== */

PyTypeObject BPy_BMesh_Type     = {{{0}}};
PyTypeObject BPy_BMVert_Type    = {{{0}}};
PyTypeObject BPy_BMEdge_Type    = {{{0}}};
PyTypeObject BPy_BMFace_Type    = {{{0}}};
PyTypeObject BPy_BMLoop_Type    = {{{0}}};
PyTypeObject BPy_BMElemSeq_Type = {{{0}}};
PyTypeObject BPy_BMVertSeq_Type = {{{0}}};
PyTypeObject BPy_BMEdgeSeq_Type = {{{0}}};
PyTypeObject BPy_BMFaceSeq_Type = {{{0}}};
PyTypeObject BPy_BMLoopSeq_Type = {{{0}}};
PyTypeObject BPy_BMIter_Type    = {{{0}}};



void BPy_BM_init_types(void)
{
	BPy_BMesh_Type.tp_basicsize     = sizeof(BPy_BMesh);
	BPy_BMVert_Type.tp_basicsize    = sizeof(BPy_BMVert);
	BPy_BMEdge_Type.tp_basicsize    = sizeof(BPy_BMEdge);
	BPy_BMFace_Type.tp_basicsize    = sizeof(BPy_BMFace);
	BPy_BMLoop_Type.tp_basicsize    = sizeof(BPy_BMLoop);
	BPy_BMElemSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
	BPy_BMVertSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
	BPy_BMEdgeSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
	BPy_BMFaceSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
	BPy_BMLoopSeq_Type.tp_basicsize = sizeof(BPy_BMElemSeq);
	BPy_BMIter_Type.tp_basicsize    = sizeof(BPy_BMIter);


	BPy_BMesh_Type.tp_name     = "BMesh";
	BPy_BMVert_Type.tp_name    = "BMVert";
	BPy_BMEdge_Type.tp_name    = "BMEdge";
	BPy_BMFace_Type.tp_name    = "BMFace";
	BPy_BMLoop_Type.tp_name    = "BMLoop";
	BPy_BMElemSeq_Type.tp_name = "BMElemSeq";
	BPy_BMVertSeq_Type.tp_name = "BMVertSeq";
	BPy_BMEdgeSeq_Type.tp_name = "BMEdgeSeq";
	BPy_BMFaceSeq_Type.tp_name = "BMFaceSeq";
	BPy_BMLoopSeq_Type.tp_name = "BMLoopSeq";
	BPy_BMIter_Type.tp_name    = "BMIter";


	BPy_BMesh_Type.tp_doc     = bpy_bmesh_doc;
	BPy_BMVert_Type.tp_doc    = bpy_bmvert_doc;
	BPy_BMEdge_Type.tp_doc    = bpy_bmedge_doc;
	BPy_BMFace_Type.tp_doc    = bpy_bmface_doc;
	BPy_BMLoop_Type.tp_doc    = bpy_bmloop_doc;
	BPy_BMElemSeq_Type.tp_doc = bpy_bmelemseq_doc;
	BPy_BMVertSeq_Type.tp_doc = NULL;
	BPy_BMEdgeSeq_Type.tp_doc = NULL;
	BPy_BMFaceSeq_Type.tp_doc = NULL;
	BPy_BMLoopSeq_Type.tp_doc = NULL;
	BPy_BMIter_Type.tp_doc    = bpy_bmiter_doc;


	BPy_BMesh_Type.tp_repr     = (reprfunc)bpy_bmesh_repr;
	BPy_BMVert_Type.tp_repr    = (reprfunc)bpy_bmvert_repr;
	BPy_BMEdge_Type.tp_repr    = (reprfunc)bpy_bmedge_repr;
	BPy_BMFace_Type.tp_repr    = (reprfunc)bpy_bmface_repr;
	BPy_BMLoop_Type.tp_repr    = (reprfunc)bpy_bmloop_repr;
	BPy_BMElemSeq_Type.tp_repr = NULL;
	BPy_BMVertSeq_Type.tp_repr = NULL;
	BPy_BMEdgeSeq_Type.tp_repr = NULL;
	BPy_BMFaceSeq_Type.tp_repr = NULL;
	BPy_BMLoopSeq_Type.tp_repr = NULL;
	BPy_BMIter_Type.tp_repr    = NULL;


	BPy_BMesh_Type.tp_getset     = bpy_bmesh_getseters;
	BPy_BMVert_Type.tp_getset    = bpy_bmvert_getseters;
	BPy_BMEdge_Type.tp_getset    = bpy_bmedge_getseters;
	BPy_BMFace_Type.tp_getset    = bpy_bmface_getseters;
	BPy_BMLoop_Type.tp_getset    = bpy_bmloop_getseters;
	BPy_BMElemSeq_Type.tp_getset = NULL;
	BPy_BMVertSeq_Type.tp_getset = bpy_bmvertseq_getseters;
	BPy_BMEdgeSeq_Type.tp_getset = bpy_bmedgeseq_getseters;
	BPy_BMFaceSeq_Type.tp_getset = bpy_bmfaceseq_getseters;
	BPy_BMLoopSeq_Type.tp_getset = bpy_bmloopseq_getseters;
	BPy_BMIter_Type.tp_getset    = NULL;


	BPy_BMesh_Type.tp_methods     = bpy_bmesh_methods;
	BPy_BMVert_Type.tp_methods    = bpy_bmvert_methods;
	BPy_BMEdge_Type.tp_methods    = bpy_bmedge_methods;
	BPy_BMFace_Type.tp_methods    = bpy_bmface_methods;
	BPy_BMLoop_Type.tp_methods    = bpy_bmloop_methods;
	BPy_BMElemSeq_Type.tp_methods = bpy_bmelemseq_methods;
	BPy_BMVertSeq_Type.tp_methods = bpy_bmvertseq_methods;
	BPy_BMEdgeSeq_Type.tp_methods = bpy_bmedgeseq_methods;
	BPy_BMFaceSeq_Type.tp_methods = bpy_bmfaceseq_methods;
	BPy_BMLoopSeq_Type.tp_methods = bpy_bmloopseq_methods;
	BPy_BMIter_Type.tp_methods    = NULL;


	BPy_BMesh_Type.tp_hash     = bpy_bm_hash;
	BPy_BMVert_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMEdge_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMFace_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMLoop_Type.tp_hash    = bpy_bm_elem_hash;
	BPy_BMElemSeq_Type.tp_hash = NULL;
	BPy_BMVertSeq_Type.tp_hash = NULL;
	BPy_BMEdgeSeq_Type.tp_hash = NULL;
	BPy_BMFaceSeq_Type.tp_hash = NULL;
	BPy_BMLoopSeq_Type.tp_hash = NULL;
	BPy_BMIter_Type.tp_hash    = NULL;

	BPy_BMElemSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
	BPy_BMVertSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
	BPy_BMEdgeSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
	BPy_BMFaceSeq_Type.tp_as_sequence = &bpy_bmelemseq_as_sequence;
	BPy_BMLoopSeq_Type.tp_as_sequence = NULL; /* this is not a seq really, only for layer access */

	BPy_BMElemSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
	BPy_BMVertSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
	BPy_BMEdgeSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
	BPy_BMFaceSeq_Type.tp_as_mapping = &bpy_bmelemseq_as_mapping;
	BPy_BMLoopSeq_Type.tp_as_mapping = NULL; /* this is not a seq really, only for layer access */

	/* layer access */
	BPy_BMVert_Type.tp_as_mapping    = &bpy_bm_elem_as_mapping;
	BPy_BMEdge_Type.tp_as_mapping    = &bpy_bm_elem_as_mapping;
	BPy_BMFace_Type.tp_as_mapping    = &bpy_bm_elem_as_mapping;
	BPy_BMLoop_Type.tp_as_mapping    = &bpy_bm_elem_as_mapping;

	BPy_BMElemSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
	BPy_BMVertSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
	BPy_BMEdgeSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
	BPy_BMFaceSeq_Type.tp_iter = (getiterfunc)bpy_bmelemseq_iter;
	BPy_BMLoopSeq_Type.tp_iter = NULL; /* no mapping */

	/* only 1 iteratir so far */
	BPy_BMIter_Type.tp_iternext = (iternextfunc)bpy_bmiter_next;
	BPy_BMIter_Type.tp_iter     = PyObject_SelfIter;

	BPy_BMesh_Type.tp_dealloc     = (destructor)bpy_bmesh_dealloc;
	BPy_BMVert_Type.tp_dealloc    = (destructor)bpy_bmvert_dealloc;
	BPy_BMEdge_Type.tp_dealloc    = (destructor)bpy_bmedge_dealloc;
	BPy_BMFace_Type.tp_dealloc    = (destructor)bpy_bmface_dealloc;
	BPy_BMLoop_Type.tp_dealloc    = (destructor)bpy_bmloop_dealloc;
	BPy_BMElemSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
	BPy_BMVertSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
	BPy_BMEdgeSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
	BPy_BMFaceSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
	BPy_BMLoopSeq_Type.tp_dealloc = (destructor)bpy_bmelemseq_dealloc;
	BPy_BMIter_Type.tp_dealloc    = NULL;

	BPy_BMesh_Type.tp_flags     = Py_TPFLAGS_DEFAULT;
	BPy_BMVert_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMEdge_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMFace_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMLoop_Type.tp_flags    = Py_TPFLAGS_DEFAULT;
	BPy_BMElemSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMVertSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMEdgeSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMFaceSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMLoopSeq_Type.tp_flags = Py_TPFLAGS_DEFAULT;
	BPy_BMIter_Type.tp_flags    = Py_TPFLAGS_DEFAULT;


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

static struct PyModuleDef BPy_BM_types_module_def = {
    PyModuleDef_HEAD_INIT,
    "bmesh.types",  /* m_name */
    NULL,  /* m_doc */
    0,  /* m_size */
    NULL,  /* m_methods */
    NULL,  /* m_reload */
    NULL,  /* m_traverse */
    NULL,  /* m_clear */
    NULL,  /* m_free */
};

PyObject *BPyInit_bmesh_types(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&BPy_BM_types_module_def);

#define mod_type_add(s, t) \
	PyModule_AddObject(s, t.tp_name, (PyObject *)&t); Py_INCREF((PyObject *)&t)

	/* bmesh_py_types.c */
	mod_type_add(submodule, BPy_BMesh_Type);
	mod_type_add(submodule, BPy_BMVert_Type);
	mod_type_add(submodule, BPy_BMEdge_Type);
	mod_type_add(submodule, BPy_BMFace_Type);
	mod_type_add(submodule, BPy_BMLoop_Type);
	mod_type_add(submodule, BPy_BMElemSeq_Type);
	mod_type_add(submodule, BPy_BMVertSeq_Type);
	mod_type_add(submodule, BPy_BMEdgeSeq_Type);
	mod_type_add(submodule, BPy_BMFaceSeq_Type);
	mod_type_add(submodule, BPy_BMLoopSeq_Type);
	mod_type_add(submodule, BPy_BMIter_Type);
	/* bmesh_py_types_select.c */
	mod_type_add(submodule, BPy_BMEditSelSeq_Type);
	mod_type_add(submodule, BPy_BMEditSelIter_Type);
	/* bmesh_py_types_customdata.c */
	mod_type_add(submodule, BPy_BMLayerAccessVert_Type);
	mod_type_add(submodule, BPy_BMLayerAccessEdge_Type);
	mod_type_add(submodule, BPy_BMLayerAccessFace_Type);
	mod_type_add(submodule, BPy_BMLayerAccessLoop_Type);
	mod_type_add(submodule, BPy_BMLayerCollection_Type);
	mod_type_add(submodule, BPy_BMLayerItem_Type);
	/* bmesh_py_types_meshdata.c */
	mod_type_add(submodule, BPy_BMLoopUV_Type);

#undef mod_type_add

	return submodule;
}

/* Utility Functions
 * ***************** */

PyObject *BPy_BMesh_CreatePyObject(BMesh *bm, int flag)
{
	BPy_BMesh *self;

	if (bm->py_handle) {
		self = bm->py_handle;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_BMesh, &BPy_BMesh_Type);
		self->bm = bm;
		self->flag = flag;

		bm->py_handle = self; /* point back */

		BM_data_layer_add(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
		BM_data_layer_add(bm, &bm->edata, CD_BM_ELEM_PYPTR);
		BM_data_layer_add(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
		BM_data_layer_add(bm, &bm->ldata, CD_BM_ELEM_PYPTR);
	}

	return (PyObject *)self;
}



PyObject *BPy_BMVert_CreatePyObject(BMesh *bm, BMVert *v)
{
	BPy_BMVert *self;

	void **ptr = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_BM_ELEM_PYPTR);

	/* bmesh may free layers, ensure we have one to store ourself */
	if (UNLIKELY(ptr == NULL)) {
		BM_data_layer_add(bm, &bm->vdata, CD_BM_ELEM_PYPTR);
		ptr = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_BM_ELEM_PYPTR);
	}

	if (*ptr != NULL) {
		self = *ptr;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_BMVert, &BPy_BMVert_Type);
		BLI_assert(v != NULL);
		self->bm = bm;
		self->v  = v;
		*ptr = self;
	}
	return (PyObject *)self;
}

PyObject *BPy_BMEdge_CreatePyObject(BMesh *bm, BMEdge *e)
{
	BPy_BMEdge *self;

	void **ptr = CustomData_bmesh_get(&bm->edata, e->head.data, CD_BM_ELEM_PYPTR);

	/* bmesh may free layers, ensure we have one to store ourself */
	if (UNLIKELY(ptr == NULL)) {
		BM_data_layer_add(bm, &bm->edata, CD_BM_ELEM_PYPTR);
		ptr = CustomData_bmesh_get(&bm->edata, e->head.data, CD_BM_ELEM_PYPTR);
	}

	if (*ptr != NULL) {
		self = *ptr;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_BMEdge, &BPy_BMEdge_Type);
		BLI_assert(e != NULL);
		self->bm = bm;
		self->e  = e;
		*ptr = self;
	}
	return (PyObject *)self;
}

PyObject *BPy_BMFace_CreatePyObject(BMesh *bm, BMFace *f)
{
	BPy_BMFace *self;

	void **ptr = CustomData_bmesh_get(&bm->pdata, f->head.data, CD_BM_ELEM_PYPTR);

	/* bmesh may free layers, ensure we have one to store ourself */
	if (UNLIKELY(ptr == NULL)) {
		BM_data_layer_add(bm, &bm->pdata, CD_BM_ELEM_PYPTR);
		ptr = CustomData_bmesh_get(&bm->pdata, f->head.data, CD_BM_ELEM_PYPTR);
	}

	if (*ptr != NULL) {
		self = *ptr;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_BMFace, &BPy_BMFace_Type);
		BLI_assert(f != NULL);
		self->bm = bm;
		self->f  = f;
		*ptr = self;
	}
	return (PyObject *)self;
}

PyObject *BPy_BMLoop_CreatePyObject(BMesh *bm, BMLoop *l)
{
	BPy_BMLoop *self;

	void **ptr = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_BM_ELEM_PYPTR);

	/* bmesh may free layers, ensure we have one to store ourself */
	if (UNLIKELY(ptr == NULL)) {
		BM_data_layer_add(bm, &bm->ldata, CD_BM_ELEM_PYPTR);
		ptr = CustomData_bmesh_get(&bm->ldata, l->head.data, CD_BM_ELEM_PYPTR);
	}

	if (*ptr != NULL) {
		self = *ptr;
		Py_INCREF(self);
	}
	else {
		self = PyObject_New(BPy_BMLoop, &BPy_BMLoop_Type);
		BLI_assert(l != NULL);
		self->bm = bm;
		self->l  = l;
		*ptr = self;
	}
	return (PyObject *)self;
}

PyObject *BPy_BMElemSeq_CreatePyObject(BMesh *bm, BPy_BMElem *py_ele, const char itype)
{
	BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMElemSeq_Type);
	self->bm = bm;
	self->py_ele = py_ele; /* can be NULL */
	self->itype = itype;
	Py_XINCREF(py_ele);
	return (PyObject *)self;
}

PyObject *BPy_BMVertSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMVertSeq_Type);
	self->bm = bm;
	self->py_ele = NULL; /* unused */
	self->itype = BM_VERTS_OF_MESH;
	return (PyObject *)self;
}

PyObject *BPy_BMEdgeSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMEdgeSeq_Type);
	self->bm = bm;
	self->py_ele = NULL; /* unused */
	self->itype = BM_EDGES_OF_MESH;
	return (PyObject *)self;
}

PyObject *BPy_BMFaceSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMFaceSeq_Type);
	self->bm = bm;
	self->py_ele = NULL; /* unused */
	self->itype = BM_FACES_OF_MESH;
	return (PyObject *)self;
}

PyObject *BPy_BMLoopSeq_CreatePyObject(BMesh *bm)
{
	BPy_BMElemSeq *self = PyObject_New(BPy_BMElemSeq, &BPy_BMLoopSeq_Type);
	self->bm = bm;
	self->py_ele = NULL; /* unused */
	self->itype = 0; /* should never be passed to the iterator function */
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
	if (LIKELY(self->bm)) {
		return 0;
	}
	else {
		PyErr_Format(PyExc_ReferenceError,
		             "BMesh data of type %.200s has been removed",
		             Py_TYPE(self)->tp_name);
		return -1;
	}
}

void bpy_bm_generic_invalidate(BPy_BMGeneric *self)
{
	self->bm = NULL;
}

/* generic python seq as BMVert/Edge/Face array,
 * return value must be freed with PyMem_FREE(...);
 *
 * The 'bm_r' value is assigned when empty, and used when set.
 */
void *BPy_BMElem_PySeq_As_Array(BMesh **r_bm, PyObject *seq, Py_ssize_t min, Py_ssize_t max, Py_ssize_t *r_size,
                                const char htype,
                                const char do_unique_check, const char do_bm_check,
                                const char *error_prefix)
{
	BMesh *bm = (r_bm && *r_bm) ? *r_bm : NULL;
	PyObject *seq_fast;
	*r_size = 0;

	if (!(seq_fast = PySequence_Fast(seq, error_prefix))) {
		return NULL;
	}
	else {
		Py_ssize_t seq_len;
		Py_ssize_t i;

		BPy_BMElem *item;
		BMElem **alloc;

		seq_len = PySequence_Fast_GET_SIZE(seq_fast);

		if (seq_len < min || seq_len > max) {
			PyErr_Format(PyExc_TypeError,
			             "%s: sequence incorrect size, expected [%d - %d], given %d",
			             error_prefix, min, max, seq_len);
			return NULL;
		}


		/* from now on, use goto */
		alloc = PyMem_MALLOC(seq_len * sizeof(BPy_BMElem **));

		for (i = 0; i < seq_len; i++) {
			item = (BPy_BMElem *)PySequence_Fast_GET_ITEM(seq_fast, i);

			if (!BPy_BMElem_CheckHType(Py_TYPE(item), htype)) {
				PyErr_Format(PyExc_TypeError,
				             "%s: expected %.200s, not '%.200s'",
				             error_prefix, BPy_BMElem_StringFromHType(htype), Py_TYPE(item)->tp_name);
				goto err_cleanup;
			}
			else if (!BPY_BM_IS_VALID(item)) {
				PyErr_Format(PyExc_TypeError,
				             "%s: %d %s has been removed",
				             error_prefix, i, Py_TYPE(item)->tp_name);
				goto err_cleanup;
			}
			/* trick so we can ensure all items have the same mesh,
			 * and allows us to pass the 'bm' as NULL. */
			else if (do_bm_check && (bm  && bm != item->bm)) {
				PyErr_Format(PyExc_ValueError,
				             "%s: %d %s is from another mesh",
				             error_prefix, i, BPy_BMElem_StringFromHType(htype));
				goto err_cleanup;
			}

			if (bm == NULL) {
				bm = item->bm;
			}

			alloc[i] = item->ele;

			if (do_unique_check) {
				BM_elem_flag_enable(item->ele, BM_ELEM_INTERNAL_TAG);
			}
		}

		if (do_unique_check) {
			/* check for double verts! */
			int ok = TRUE;
			for (i = 0; i < seq_len; i++) {
				if (UNLIKELY(BM_elem_flag_test(alloc[i], BM_ELEM_INTERNAL_TAG) == FALSE)) {
					ok = FALSE;
				}

				/* ensure we don't leave this enabled */
				BM_elem_flag_disable(alloc[i], BM_ELEM_INTERNAL_TAG);
			}

			if (ok == FALSE) {
				PyErr_Format(PyExc_ValueError,
				             "%s: found the same %.200s used multiple times",
				             error_prefix, BPy_BMElem_StringFromHType(htype));
				goto err_cleanup;
			}
		}

		Py_DECREF(seq_fast);
		*r_size = seq_len;
		if (r_bm) *r_bm = bm;
		return alloc;

err_cleanup:
		Py_DECREF(seq_fast);
		PyMem_FREE(alloc);
		return NULL;
	}
}


PyObject *BPy_BMElem_Array_As_Tuple(BMesh *bm, BMHeader **elem, Py_ssize_t elem_len)
{
	Py_ssize_t i;
	PyObject *ret = PyTuple_New(elem_len);
	for (i = 0; i < elem_len; i++) {
		PyTuple_SET_ITEM(ret, i, BPy_BMElem_CreatePyObject(bm, elem[i]));
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

/**
 * Use for error strings only, not thread safe,
 *
 * \return a sting like '(BMVert/BMEdge/BMFace/BMLoop)'
 */
char *BPy_BMElem_StringFromHType_ex(const char htype, char ret[32])
{
	/* zero to ensure string is always NULL terminated */
	char *ret_ptr = ret;
	if (htype & BM_VERT) ret_ptr += sprintf(ret_ptr, "/%s", BPy_BMVert_Type.tp_name);
	if (htype & BM_EDGE) ret_ptr += sprintf(ret_ptr, "/%s", BPy_BMEdge_Type.tp_name);
	if (htype & BM_FACE) ret_ptr += sprintf(ret_ptr, "/%s", BPy_BMFace_Type.tp_name);
	if (htype & BM_LOOP) ret_ptr += sprintf(ret_ptr, "/%s", BPy_BMLoop_Type.tp_name);
	ret[0]   = '(';
	*ret_ptr = ')';
	return ret;
}
char *BPy_BMElem_StringFromHType(const char htype)
{
	/* zero to ensure string is always NULL terminated */
	static char ret[32];
	return BPy_BMElem_StringFromHType_ex(htype, ret);
}
