/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

/**
 * \brief BMesh Iterators
 *
 * The functions and structures in this file
 * provide a unified method for iterating over
 * the elements of a mesh and answering simple
 * adjacency queries. Tool authors should use
 * the iterators provided in this file instead
 * of inspecting the structure directly.
 */

#include "BLI_bit_span.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_mempool.h"

/* these iterator over all elements of a specific
 * type in the mesh.
 *
 * be sure to keep 'bm_iter_itype_htype_map' in sync with any changes
 */
typedef enum BMIterType {
  BM_VERTS_OF_MESH = 1,
  BM_EDGES_OF_MESH = 2,
  BM_FACES_OF_MESH = 3,
  /* these are topological iterators. */
  BM_EDGES_OF_VERT = 4,
  BM_FACES_OF_VERT = 5,
  BM_LOOPS_OF_VERT = 6,
  BM_VERTS_OF_EDGE = 7, /* just v1, v2: added so py can use generalized sequencer wrapper */
  BM_FACES_OF_EDGE = 8,
  BM_VERTS_OF_FACE = 9,
  BM_EDGES_OF_FACE = 10,
  BM_LOOPS_OF_FACE = 11,
  /* returns elements from all boundaries, and returns
   * the first element at the end to flag that we're entering
   * a different face hole boundary. */
  // BM_ALL_LOOPS_OF_FACE = 12,
  /* iterate through loops around this loop, which are fetched
   * from the other faces in the radial cycle surrounding the
   * input loop's edge. */
  BM_LOOPS_OF_LOOP = 12,
  BM_LOOPS_OF_EDGE = 13,
} BMIterType;

#define BM_ITYPE_MAX 14

/* the iterator htype for each iterator */
extern const char bm_iter_itype_htype_map[BM_ITYPE_MAX];

/* -------------------------------------------------------------------- */
/** \name Defines for passing to #BM_iter_new.
 *
 * "OF" can be substituted for "around" so #BM_VERTS_OF_FACE means "vertices* around a face."
 * \{ */

#define BM_ITER_MESH(ele, iter, bm, itype) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, bm, itype, NULL); ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_step(iter))

#define BM_ITER_MESH_INDEX(ele, iter, bm, itype, indexvar) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, bm, itype, NULL), indexvar = 0; ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_step(iter), (indexvar)++)

/* a version of BM_ITER_MESH which keeps the next item in storage
 * so we can delete the current item, see bug #36923. */
#ifndef NDEBUG
#  define BM_ITER_MESH_MUTABLE(ele, ele_next, iter, bm, itype) \
    for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, bm, itype, NULL); \
         ele ? ((void)((iter)->count = BM_iter_mesh_count(itype, bm)), \
                (void)(BM_CHECK_TYPE_ELEM_ASSIGN(ele_next) = BM_iter_step(iter)), \
                1) : \
               0; \
         BM_CHECK_TYPE_ELEM_ASSIGN(ele) = ele_next)
#else
#  define BM_ITER_MESH_MUTABLE(ele, ele_next, iter, bm, itype) \
    for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, bm, itype, NULL); \
         ele ? ((BM_CHECK_TYPE_ELEM_ASSIGN(ele_next) = BM_iter_step(iter)), 1) : 0; \
         ele = ele_next)
#endif

#define BM_ITER_ELEM(ele, iter, data, itype) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, NULL, itype, data); ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_step(iter))

#define BM_ITER_ELEM_INDEX(ele, iter, data, itype, indexvar) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_new(iter, NULL, itype, data), indexvar = 0; ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BM_iter_step(iter), (indexvar)++)

/** \} */

/* iterator type structs */
struct BMIter__elem_of_mesh {
  BLI_mempool_iter pooliter;
};
struct BMIter__edge_of_vert {
  BMVert *vdata;
  BMEdge *e_first, *e_next;
};
struct BMIter__face_of_vert {
  BMVert *vdata;
  BMLoop *l_first, *l_next;
  BMEdge *e_first, *e_next;
};
struct BMIter__loop_of_vert {
  BMVert *vdata;
  BMLoop *l_first, *l_next;
  BMEdge *e_first, *e_next;
};
struct BMIter__loop_of_edge {
  BMEdge *edata;
  BMLoop *l_first, *l_next;
};
struct BMIter__loop_of_loop {
  BMLoop *ldata;
  BMLoop *l_first, *l_next;
};
struct BMIter__face_of_edge {
  BMEdge *edata;
  BMLoop *l_first, *l_next;
};
struct BMIter__vert_of_edge {
  BMEdge *edata;
};
struct BMIter__vert_of_face {
  BMFace *pdata;
  BMLoop *l_first, *l_next;
};
struct BMIter__edge_of_face {
  BMFace *pdata;
  BMLoop *l_first, *l_next;
};
struct BMIter__loop_of_face {
  BMFace *pdata;
  BMLoop *l_first, *l_next;
};

typedef void (*BMIter__begin_cb)(void *);
typedef void *(*BMIter__step_cb)(void *);

/* Iterator Structure */
/* NOTE: some of these vars are not used,
 * so they have been commented to save stack space since this struct is used all over */
struct BMIter {
  /* keep union first */
  union {
    BMIter__elem_of_mesh elem_of_mesh;

    BMIter__edge_of_vert edge_of_vert;
    BMIter__face_of_vert face_of_vert;
    BMIter__loop_of_vert loop_of_vert;
    BMIter__loop_of_edge loop_of_edge;
    BMIter__loop_of_loop loop_of_loop;
    BMIter__face_of_edge face_of_edge;
    BMIter__vert_of_edge vert_of_edge;
    BMIter__vert_of_face vert_of_face;
    BMIter__edge_of_face edge_of_face;
    BMIter__loop_of_face loop_of_face;
  } data;

  BMIter__begin_cb begin;
  BMIter__step_cb step;

  int count; /* NOTE: only some iterators set this, don't rely on it. */
  char itype;
};

/**
 * \note Use #BM_vert_at_index / #BM_edge_at_index / #BM_face_at_index for mesh arrays.
 */
void *BM_iter_at_index(BMesh *bm, char itype, void *data, int index) ATTR_WARN_UNUSED_RESULT;
/**
 * \brief Iterator as Array
 *
 * Sometimes its convenient to get the iterator as an array
 * to avoid multiple calls to #BM_iter_at_index.
 */
int BM_iter_as_array(BMesh *bm, char itype, void *data, void **array, int len);
/**
 * \brief Iterator as Array
 *
 * Allocates a new array, has the advantage that you don't need to know the size ahead of time.
 *
 * Takes advantage of less common iterator usage to avoid counting twice,
 * which you might end up doing when #BM_iter_as_array is used.
 *
 * Caller needs to free the array.
 */
void *BM_iter_as_arrayN(BMesh *bm,
                        char itype,
                        void *data,
                        int *r_len,
                        void **stack_array,
                        int stack_array_size) ATTR_WARN_UNUSED_RESULT;
/**
 * \brief Operator Iterator as Array
 *
 * Sometimes its convenient to get the iterator as an array.
 */
int BMO_iter_as_array(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      char restrictmask,
                      void **array,
                      int len);
void *BMO_iter_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                         const char *slot_name,
                         char restrictmask,
                         int *r_len,
                         /* optional args to avoid an alloc (normally stack array) */
                         void **stack_array,
                         int stack_array_size);

int BM_iter_mesh_bitmap_from_filter(char itype,
                                    BMesh *bm,
                                    blender::MutableBitSpan bitmap,
                                    bool (*test_fn)(BMElem *, void *user_data),
                                    void *user_data);
/**
 * Needed when we want to check faces, but return a loop aligned array.
 */
int BM_iter_mesh_bitmap_from_filter_tessface(BMesh *bm,
                                             blender::MutableBitSpan bitmap,
                                             bool (*test_fn)(BMFace *, void *user_data),
                                             void *user_data);

/**
 * \brief Elem Iter Flag Count
 *
 * Counts how many flagged / unflagged items are found in this element.
 */
int BM_iter_elem_count_flag(char itype, void *data, char hflag, bool value);
/**
 * \brief Elem Iter Tool Flag Count
 *
 * Counts how many flagged / unflagged items are found in this element.
 */
int BMO_iter_elem_count_flag(BMesh *bm, char itype, void *data, short oflag, bool value);
/**
 * Utility function.
 */
int BM_iter_mesh_count(char itype, BMesh *bm);
/**
 * \brief Mesh Iter Flag Count
 *
 * Counts how many flagged / unflagged items are found in this mesh.
 */
int BM_iter_mesh_count_flag(char itype, BMesh *bm, char hflag, bool value);

/* private for bmesh_iterators_inline.c */

#define BMITER_CB_DEF(name) \
  struct BMIter__##name; \
  void bmiter__##name##_begin(struct BMIter__##name *iter); \
  void *bmiter__##name##_step(struct BMIter__##name *iter)

BMITER_CB_DEF(elem_of_mesh);
BMITER_CB_DEF(edge_of_vert);
BMITER_CB_DEF(face_of_vert);
BMITER_CB_DEF(loop_of_vert);
BMITER_CB_DEF(loop_of_edge);
BMITER_CB_DEF(loop_of_loop);
BMITER_CB_DEF(face_of_edge);
BMITER_CB_DEF(vert_of_edge);
BMITER_CB_DEF(vert_of_face);
BMITER_CB_DEF(edge_of_face);
BMITER_CB_DEF(loop_of_face);

#undef BMITER_CB_DEF

#include "intern/bmesh_iterators_inline.hh"

#define BM_ITER_CHECK_TYPE_DATA(data) \
  CHECK_TYPE_ANY(data, void *, BMFace *, BMEdge *, BMVert *, BMLoop *, BMElem *)

#define BM_iter_new(iter, bm, itype, data) \
  (BM_ITER_CHECK_TYPE_DATA(data), BM_iter_new(iter, bm, itype, data))
#define BM_iter_init(iter, bm, itype, data) \
  (BM_ITER_CHECK_TYPE_DATA(data), BM_iter_init(iter, bm, itype, data))
