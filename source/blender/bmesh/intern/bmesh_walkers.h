/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTE: do NOT modify topology while walking a mesh!
 */

typedef enum {
  BMW_DEPTH_FIRST,
  BMW_BREADTH_FIRST,
} BMWOrder;

typedef enum {
  BMW_FLAG_NOP = 0,
  BMW_FLAG_TEST_HIDDEN = (1 << 0),
} BMWFlag;

/*Walkers*/
typedef struct BMWalker {
  char begin_htype; /* only for validating input */
  void (*begin)(struct BMWalker *walker, void *start);
  void *(*step)(struct BMWalker *walker);
  void *(*yield)(struct BMWalker *walker);
  int structsize;
  BMWOrder order;
  int valid_mask;

  /* runtime */
  int layer;

  BMesh *bm;
  BLI_mempool *worklist;
  ListBase states;

  /* these masks are to be tested against elements BMO_elem_flag_test(),
   * should never be accessed directly only through BMW_init() and bmw_mask_check_*() functions */
  short mask_vert;
  short mask_edge;
  short mask_face;

  BMWFlag flag;

  struct GSet *visit_set;
  struct GSet *visit_set_alt;
  int depth;
} BMWalker;

/* define to make BMW_init more clear */
#define BMW_MASK_NOP 0

/**
 * \brief Init Walker
 *
 * Allocates and returns a new mesh walker of a given type.
 * The elements visited are filtered by the bitmask 'searchmask'.
 */
void BMW_init(struct BMWalker *walker,
              BMesh *bm,
              int type,
              short mask_vert,
              short mask_edge,
              short mask_face,
              BMWFlag flag,
              int layer);
void *BMW_begin(BMWalker *walker, void *start);
/**
 * \brief Step Walker
 */
void *BMW_step(struct BMWalker *walker);
/**
 * \brief End Walker
 *
 * Frees a walker's worklist.
 */
void BMW_end(struct BMWalker *walker);
/**
 * \brief Walker Current Depth
 *
 * Returns the current depth of the walker.
 */
int BMW_current_depth(BMWalker *walker);

/* These are used by custom walkers. */
/**
 * \brief Current Walker State
 *
 * Returns the first state from the walker state
 * worklist. This state is the next in the
 * worklist for processing.
 */
void *BMW_current_state(BMWalker *walker);
/**
 * \brief Add a new Walker State
 *
 * Allocate a new empty state and put it on the worklist.
 * A pointer to the new state is returned so that the caller
 * can fill in the state data. The new state will be inserted
 * at the front for depth-first walks, and at the end for
 * breadth-first walks.
 */
void *BMW_state_add(BMWalker *walker);
/**
 * \brief Remove Current Walker State
 *
 * Remove and free an item from the end of the walker state
 * worklist.
 */
void BMW_state_remove(BMWalker *walker);
/**
 * \brief Main Walking Function
 *
 * Steps a mesh walker forward by one element
 */
void *BMW_walk(BMWalker *walker);
/**
 * \brief Reset Walker
 *
 * Frees all states from the worklist, resetting the walker
 * for reuse in a new walk.
 */
void BMW_reset(BMWalker *walker);

#define BMW_ITER(ele, walker, data) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMW_begin(walker, (BM_CHECK_TYPE_ELEM(data), data)); ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMW_step(walker))

/*
 * example of usage, walking over an island of tool flagged faces:
 *
 * BMWalker walker;
 * BMFace *f;
 *
 * BMW_init(&walker, bm, BMW_ISLAND, SOME_OP_FLAG);
 *
 * for (f = BMW_begin(&walker, some_start_face); f; f = BMW_step(&walker)) {
 *     // do something with f
 * }
 * BMW_end(&walker);
 */

enum {
  BMW_VERT_SHELL,
  BMW_LOOP_SHELL,
  BMW_LOOP_SHELL_WIRE,
  BMW_FACE_SHELL,
  BMW_EDGELOOP,
  BMW_FACELOOP,
  BMW_EDGERING,
  BMW_EDGEBOUNDARY,
  BMW_EDGELOOP_NONMANIFOLD,
  /* BMW_RING, */
  BMW_LOOPDATA_ISLAND,
  BMW_ISLANDBOUND,
  BMW_ISLAND,
  BMW_ISLAND_MANIFOLD,
  BMW_CONNECTED_VERTEX,
  /* End of array index enum values. */

  /* Do not initialize function pointers and struct size in #BMW_init. */
  BMW_CUSTOM,
  BMW_MAXWALKERS,
};

/* use with BMW_init, so as not to confuse with restrict flags */
#define BMW_NIL_LAY 0

#ifdef __cplusplus
}
#endif
