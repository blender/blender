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

/** \file blender/python/bmesh/bmesh_py_types.h
 *  \ingroup pybmesh
 */

#ifndef __BMESH_TYPES_H__
#define __BMESH_TYPES_H__

extern PyTypeObject BPy_BMesh_Type;
extern PyTypeObject BPy_BMVert_Type;
extern PyTypeObject BPy_BMEdge_Type;
extern PyTypeObject BPy_BMFace_Type;
extern PyTypeObject BPy_BMLoop_Type;
extern PyTypeObject BPy_BMElemSeq_Type;
extern PyTypeObject BPy_BMIter_Type;

#define BPy_BMesh_Check(v)      (Py_TYPE(v) == &BPy_BMesh_Type)
#define BPy_BMVert_Check(v)     (Py_TYPE(v) == &BPy_BMVert_Type)
#define BPy_BMEdge_Check(v)     (Py_TYPE(v) == &BPy_BMEdge_Type)
#define BPy_BMFace_Check(v)     (Py_TYPE(v) == &BPy_BMFace_Type)
#define BPy_BMLoop_Check(v)     (Py_TYPE(v) == &BPy_BMLoop_Type)
#define BPy_BMElemSeq_Check(v)  (Py_TYPE(v) == &BPy_BMElemSeq_Type)
#define BPy_BMIter_Check(v)     (Py_TYPE(v) == &BPy_BMIter_Type)

/* cast from _any_ bmesh type - they all have BMesh first */
typedef struct BPy_BMGeneric {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
} BPy_BMGeneric;

/* BPy_BMVert/BPy_BMEdge/BPy_BMFace/BPy_BMLoop can cast to this */
typedef struct BPy_BMElem {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	struct BMElem *ele;
} BPy_BMElem;

typedef struct BPy_BMesh {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
} BPy_BMesh;

/* element types */
typedef struct BPy_BMVert {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	struct BMVert *v;
} BPy_BMVert;

typedef struct BPy_BMEdge {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	struct BMEdge *e;
} BPy_BMEdge;

typedef struct BPy_BMFace {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	struct BMFace *f;
} BPy_BMFace;

typedef struct BPy_BMLoop {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	struct BMLoop *l;
} BPy_BMLoop;


/* iterators */

typedef struct BPy_BMElemSeq {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */

	/* if this is a sequence on an existing element,
	 * loops of faces for eg.
	 * If this veriable is set, it will be used */

	/* we hold a reference to this.
	 * check incase the owner becomes invalid on access */
	/* TODO - make this a GC'd object!, will function OK without this though */
	BPy_BMElem *py_ele;

	/* iterator type */
	short itype;
} BPy_BMElemSeq;

typedef struct BPy_BMIter {
	PyObject_VAR_HEAD
	struct BMesh *bm; /* keep first */
	BMIter iter;
} BPy_BMIter;

void BPy_BM_init_types(void);

PyObject *BPyInit_bmesh_types(void);

PyObject *BPy_BMesh_CreatePyObject(BMesh *bm);
PyObject *BPy_BMVert_CreatePyObject(BMesh *bm, BMVert *v);
PyObject *BPy_BMEdge_CreatePyObject(BMesh *bm, BMEdge *e);
PyObject *BPy_BMFace_CreatePyObject(BMesh *bm, BMFace *f);
PyObject *BPy_BMLoop_CreatePyObject(BMesh *bm, BMLoop *l);
PyObject *BPy_BMElemSeq_CreatePyObject(BMesh *bm, BPy_BMElem *py_ele, const char itype);
PyObject *BPy_BMIter_CreatePyObject(BMesh *bm);

PyObject *BPy_BMElem_CreatePyObject(BMesh *bm, BMHeader *ele); /* just checks type and creates v/e/f/l */

int  bpy_bm_generic_valid_check(BPy_BMGeneric *self);
void bpy_bm_generic_invalidate(BPy_BMGeneric *self);

void *BPy_BMElem_PySeq_As_Array(BMesh **r_bm, PyObject *seq, Py_ssize_t min, Py_ssize_t max, Py_ssize_t *r_size,
                                PyTypeObject *type,
                                const char do_unique_check, const char do_bm_check,
                                const char *error_prefix);

#define BPY_BM_CHECK_OBJ(obj) if (UNLIKELY(bpy_bm_generic_valid_check((BPy_BMGeneric *)obj) == -1)) { return NULL; } (void)0
#define BPY_BM_CHECK_INT(obj) if (UNLIKELY(bpy_bm_generic_valid_check((BPy_BMGeneric *)obj) == -1)) { return -1; }   (void)0

#define BPY_BM_IS_VALID(obj) (LIKELY((obj)->bm != NULL))

#define BM_ITER_BPY_BM_SEQ(ele, iter, bpy_bmelemseq)                \
	BM_ITER(ele, iter, (bpy_bmelemseq)->bm, (bpy_bmelemseq)->itype, \
	(bpy_bmelemseq)->py_ele ? ((BPy_BMElem *)(bpy_bmelemseq)->py_ele)->ele : NULL)

#endif /* __BMESH_TYPES_H__ */
