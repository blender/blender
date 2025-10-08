/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pybmesh
 */

#pragma once

#include <Python.h>

#include "bmesh.hh"

struct BMesh;
struct BMEdge;
struct BMElem;
struct BMFace;
struct BMLoop;
struct BMVert;

extern PyTypeObject BPy_BMesh_Type;
extern PyTypeObject BPy_BMVert_Type;
extern PyTypeObject BPy_BMEdge_Type;
extern PyTypeObject BPy_BMFace_Type;
extern PyTypeObject BPy_BMLoop_Type;

extern PyTypeObject BPy_BMElemSeq_Type;
extern PyTypeObject BPy_BMVertSeq_Type;
extern PyTypeObject BPy_BMEdgeSeq_Type;
extern PyTypeObject BPy_BMFaceSeq_Type;
extern PyTypeObject BPy_BMLoopSeq_Type;

extern PyTypeObject BPy_BMIter_Type;

#define BPy_BMesh_Check(v) (Py_TYPE(v) == &BPy_BMesh_Type)
#define BPy_BMVert_Check(v) (Py_TYPE(v) == &BPy_BMVert_Type)
#define BPy_BMEdge_Check(v) (Py_TYPE(v) == &BPy_BMEdge_Type)
#define BPy_BMFace_Check(v) (Py_TYPE(v) == &BPy_BMFace_Type)
#define BPy_BMLoop_Check(v) (Py_TYPE(v) == &BPy_BMLoop_Type)
#define BPy_BMElemSeq_Check(v) (Py_TYPE(v) == &BPy_BMElemSeq_Type)
#define BPy_BMVertSeq_Check(v) (Py_TYPE(v) == &BPy_BMVertSeq_Type)
#define BPy_BMEdgeSeq_Check(v) (Py_TYPE(v) == &BPy_BMEdgeSeq_Type)
#define BPy_BMFaceSeq_Check(v) (Py_TYPE(v) == &BPy_BMFaceSeq_Type)
#define BPy_BMLoopSeq_Check(v) (Py_TYPE(v) == &BPy_BMLoopSeq_Type)
#define BPy_BMIter_Check(v) (Py_TYPE(v) == &BPy_BMIter_Type)
/* trick since we know they share a hash function */
#define BPy_BMElem_Check(v) (Py_TYPE(v)->tp_hash == BPy_BMVert_Type.tp_hash)

/* cast from _any_ bmesh type - they all have BMesh first */
struct BPy_BMGeneric {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
};

/* BPy_BMVert/BPy_BMEdge/BPy_BMFace/BPy_BMLoop can cast to this */
struct BPy_BMElem {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMElem *ele;
};

struct BPy_BMesh {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  int flag;
};

/* element types */
struct BPy_BMVert {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMVert *v;
};

struct BPy_BMEdge {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMEdge *e;
};

struct BPy_BMFace {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMFace *f;
};

struct BPy_BMLoop {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMLoop *l;
};

/* iterators */

/* used for ...
 * - BPy_BMElemSeq_Type
 * - BPy_BMVertSeq_Type
 * - BPy_BMEdgeSeq_Type
 * - BPy_BMFaceSeq_Type
 * - BPy_BMLoopSeq_Type
 */
struct BPy_BMElemSeq {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */

  /* if this is a sequence on an existing element,
   * loops of faces for eg.
   * If this variable is set, it will be used */

  /* we hold a reference to this.
   * check in case the owner becomes invalid on access */
  /* TODO: make this a GC'd object!, will function OK without this though. */
  BPy_BMElem *py_ele;

  /* iterator type */
  short itype;
};

struct BPy_BMIter {
  PyObject_VAR_HEAD
  BMesh *bm; /* keep first */
  BMIter iter;
};

void BPy_BM_init_types();

[[nodiscard]] PyObject *BPyInit_bmesh_types();

enum {
  BPY_BMFLAG_NOP = 0,        /* do nothing */
  BPY_BMFLAG_IS_WRAPPED = 1, /* the mesh is owned by editmode */
};

[[nodiscard]] PyObject *BPy_BMesh_CreatePyObject(BMesh *bm, int flag);
[[nodiscard]] PyObject *BPy_BMVert_CreatePyObject(BMesh *bm, BMVert *v);
[[nodiscard]] PyObject *BPy_BMEdge_CreatePyObject(BMesh *bm, BMEdge *e);
[[nodiscard]] PyObject *BPy_BMFace_CreatePyObject(BMesh *bm, BMFace *f);
[[nodiscard]] PyObject *BPy_BMLoop_CreatePyObject(BMesh *bm, BMLoop *l);
[[nodiscard]] PyObject *BPy_BMElemSeq_CreatePyObject(BMesh *bm, BPy_BMElem *py_ele, char itype);
[[nodiscard]] PyObject *BPy_BMVertSeq_CreatePyObject(BMesh *bm);
[[nodiscard]] PyObject *BPy_BMEdgeSeq_CreatePyObject(BMesh *bm);
[[nodiscard]] PyObject *BPy_BMFaceSeq_CreatePyObject(BMesh *bm);
[[nodiscard]] PyObject *BPy_BMLoopSeq_CreatePyObject(BMesh *bm);
[[nodiscard]] PyObject *BPy_BMIter_CreatePyObject(BMesh *bm);

/** Just checks type and creates vert/edge/face/loop. */
[[nodiscard]] PyObject *BPy_BMElem_CreatePyObject(BMesh *bm, BMHeader *ele);

/**
 * Generic python seq as BMVert/Edge/Face array,
 * return value must be freed with PyMem_FREE(...);
 *
 * The 'bm_r' value is assigned when empty, and used when set.
 */
[[nodiscard]] void *BPy_BMElem_PySeq_As_Array_FAST(BMesh **r_bm,
                                                   PyObject *seq_fast,
                                                   Py_ssize_t min,
                                                   Py_ssize_t max,
                                                   Py_ssize_t *r_seq_num,
                                                   char htype,
                                                   bool do_unique_check,
                                                   bool do_bm_check,
                                                   const char *error_prefix);
[[nodiscard]] void *BPy_BMElem_PySeq_As_Array(BMesh **r_bm,
                                              PyObject *seq,
                                              Py_ssize_t min,
                                              Py_ssize_t max,
                                              Py_ssize_t *r_seq_num,
                                              char htype,
                                              bool do_unique_check,
                                              bool do_bm_check,
                                              const char *error_prefix);

[[nodiscard]] BMVert **BPy_BMVert_PySeq_As_Array(BMesh **r_bm,
                                                 PyObject *seq,
                                                 Py_ssize_t min,
                                                 Py_ssize_t max,
                                                 Py_ssize_t *r_seq_num,
                                                 bool do_unique_check,
                                                 bool do_bm_check,
                                                 const char *error_prefix);
[[nodiscard]] BMEdge **BPy_BMEdge_PySeq_As_Array(BMesh **r_bm,
                                                 PyObject *seq,
                                                 Py_ssize_t min,
                                                 Py_ssize_t max,
                                                 Py_ssize_t *r_seq_num,
                                                 bool do_unique_check,
                                                 bool do_bm_check,
                                                 const char *error_prefix);
[[nodiscard]] BMFace **BPy_BMFace_PySeq_As_Array(BMesh **r_bm,
                                                 PyObject *seq,
                                                 Py_ssize_t min,
                                                 Py_ssize_t max,
                                                 Py_ssize_t *r_seq_num,
                                                 bool do_unique_check,
                                                 bool do_bm_check,
                                                 const char *error_prefix);
[[nodiscard]] BMLoop **BPy_BMLoop_PySeq_As_Array(BMesh **r_bm,
                                                 PyObject *seq,
                                                 Py_ssize_t min,
                                                 Py_ssize_t max,
                                                 Py_ssize_t *r_seq_num,
                                                 bool do_unique_check,
                                                 bool do_bm_check,
                                                 const char *error_prefix);

[[nodiscard]] PyObject *BPy_BMElem_Array_As_Tuple(BMesh *bm, BMHeader **elem, Py_ssize_t elem_num);
[[nodiscard]] PyObject *BPy_BMVert_Array_As_Tuple(BMesh *bm, BMVert **elem, Py_ssize_t elem_num);
[[nodiscard]] PyObject *BPy_BMEdge_Array_As_Tuple(BMesh *bm, BMEdge **elem, Py_ssize_t elem_num);
[[nodiscard]] PyObject *BPy_BMFace_Array_As_Tuple(BMesh *bm, BMFace **elem, Py_ssize_t elem_num);
[[nodiscard]] PyObject *BPy_BMLoop_Array_As_Tuple(BMesh *bm,
                                                  BMLoop *const *elem,
                                                  Py_ssize_t elem_num);

[[nodiscard]] int BPy_BMElem_CheckHType(PyTypeObject *type, char htype);
/**
 * Use for error strings only, not thread safe,
 *
 * \return a string like '(BMVert/BMEdge/BMFace/BMLoop)'
 */
[[nodiscard]] char *BPy_BMElem_StringFromHType_ex(char htype, char ret[32]);
[[nodiscard]] char *BPy_BMElem_StringFromHType(char htype);

// void bpy_bm_generic_invalidate(BPy_BMGeneric *self);
[[nodiscard]] int bpy_bm_generic_valid_check(BPy_BMGeneric *self);
[[nodiscard]] int bpy_bm_generic_valid_check_source(BMesh *bm_source,
                                                    const char *error_prefix,
                                                    void **args,
                                                    uint args_tot) ATTR_NONNULL(1, 2);
[[nodiscard]] int bpy_bm_check_uv_select_sync_valid(BMesh *bm, const char *error_prefix);
[[nodiscard]] int bpy_bm_uv_layer_offset_or_error(BMesh *bm, const char *error_prefix);
[[nodiscard]] int bpy_bm_check_bm_match_or_error(BMesh *bm_a,
                                                 BMesh *bm_b,
                                                 const char *error_prefix);

#define BPY_BM_CHECK_OBJ(obj) \
  if (UNLIKELY(bpy_bm_generic_valid_check((BPy_BMGeneric *)obj) == -1)) { \
    return NULL; \
  } \
  (void)0
#define BPY_BM_CHECK_INT(obj) \
  if (UNLIKELY(bpy_bm_generic_valid_check((BPy_BMGeneric *)obj) == -1)) { \
    return -1; \
  } \
  (void)0

/**
 * Macros like `BPY_BM_CHECK_OBJ/BPY_BM_CHECK_INT` that ensure we're from the right #BMesh.
 */
#define BPY_BM_CHECK_SOURCE_OBJ(bm, errmsg, ...) \
  { \
    void *_args[] = {__VA_ARGS__}; \
    if (UNLIKELY(bpy_bm_generic_valid_check_source(bm, errmsg, _args, ARRAY_SIZE(_args)) == -1)) \
    { \
      return NULL; \
    } \
  } \
  (void)0
#define BPY_BM_CHECK_SOURCE_INT(bm, errmsg, ...) \
  { \
    void *_args[] = {__VA_ARGS__}; \
    if (UNLIKELY(bpy_bm_generic_valid_check_source(bm, errmsg, _args, ARRAY_SIZE(_args)) == -1)) \
    { \
      return -1; \
    } \
  } \
  (void)0

#define BPY_BM_IS_VALID(obj) (LIKELY((obj)->bm != NULL))

#define BM_ITER_BPY_BM_SEQ(ele, iter, bpy_bmelemseq) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new( \
           iter, \
           (bpy_bmelemseq)->bm, \
           (bpy_bmelemseq)->itype, \
           (bpy_bmelemseq)->py_ele ? ((BPy_BMElem *)(bpy_bmelemseq)->py_ele)->ele : NULL); \
       ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_step(iter))
