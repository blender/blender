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

#ifndef __BMESH_WALKERS_H__
#define __BMESH_WALKERS_H__

/** \file
 * \ingroup bmesh
 */

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

void BMW_init(struct BMWalker *walker,
              BMesh *bm,
              int type,
              short mask_vert,
              short mask_edge,
              short mask_face,
              BMWFlag flag,
              int layer);
void *BMW_begin(BMWalker *walker, void *start);
void *BMW_step(struct BMWalker *walker);
void BMW_end(struct BMWalker *walker);
int BMW_current_depth(BMWalker *walker);

/*these are used by custom walkers*/
void *BMW_current_state(BMWalker *walker);
void *BMW_state_add(BMWalker *walker);
void BMW_state_remove(BMWalker *walker);
void *BMW_walk(BMWalker *walker);
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
  /* BMW_RING, */
  BMW_LOOPDATA_ISLAND,
  BMW_ISLANDBOUND,
  BMW_ISLAND,
  BMW_ISLAND_MANIFOLD,
  BMW_CONNECTED_VERTEX,
  /* end of array index enum vals */

  /* do not intitialze function pointers and struct size in BMW_init */
  BMW_CUSTOM,
  BMW_MAXWALKERS,
};

/* use with BMW_init, so as not to confuse with restrict flags */
#define BMW_NIL_LAY 0

#endif /* __BMESH_WALKERS_H__ */
