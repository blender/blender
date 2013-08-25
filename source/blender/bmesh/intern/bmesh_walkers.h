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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_WALKERS_H__
#define __BMESH_WALKERS_H__

/** \file blender/bmesh/intern/bmesh_walkers.h
 *  \ingroup bmesh
 */

#include "BLI_ghash.h"

/*
 * NOTE: do NOT modify topology while walking a mesh!
 */

typedef enum {
	BMW_DEPTH_FIRST,
	BMW_BREADTH_FIRST
} BMWOrder;

typedef enum {
	BMW_FLAG_NOP = 0,
	BMW_FLAG_TEST_HIDDEN = (1 << 0)
} BMWFlag;

/*Walkers*/
typedef struct BMWalker {
	void  (*begin) (struct BMWalker *walker, void *start);
	void *(*step)  (struct BMWalker *walker);
	void *(*yield) (struct BMWalker *walker);
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

	GSet *visit_set;
	GSet *visit_set_alt;
	int depth;
} BMWalker;

/* define to make BMW_init more clear */
#define BMW_MASK_NOP 0

/* initialize a walker.  searchmask restricts some (not all) walkers to
 * elements with a specific tool flag set.  flags is specific to each walker.*/
void BMW_init(struct BMWalker *walker, BMesh *bm, int type,
              short mask_vert, short mask_edge, short mask_face,
              BMWFlag flag,
              int layer);
void *BMW_begin(BMWalker *walker, void *start);
void *BMW_step(struct BMWalker *walker);
void  BMW_end(struct BMWalker *walker);
int   BMW_current_depth(BMWalker *walker);

/*these are used by custom walkers*/
void *BMW_current_state(BMWalker *walker);
void *BMW_state_add(BMWalker *walker);
void  BMW_state_remove(BMWalker *walker);
void *BMW_walk(BMWalker *walker);
void  BMW_reset(BMWalker *walker);

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
	/* walk over connected geometry.  can restrict to a search flag,
	 * or not, it's optional.
	 *
	 * takes a vert as an argument, and spits out edges, restrict flag acts
	 * on the edges as well. */
	BMW_SHELL,
	/*walk over an edge loop.  search flag doesn't do anything.*/
	BMW_LOOP,
	BMW_FACELOOP,
	BMW_EDGERING,
	/* #define BMW_RING	2 */
	/* walk over uv islands; takes a loop as input.  restrict flag
	 * restricts the walking to loops whose vert has restrict flag set as a
	 * tool flag.
	 *
	 * the flag parameter to BMW_init maps to a loop customdata layer index.
	 */
	BMW_LOOPDATA_ISLAND,
	/* walk over an island of flagged faces.  note, that this doesn't work on
	 * non-manifold geometry.  it might be better to rewrite this to extract
	 * boundary info from the island walker, rather then directly walking
	 * over the boundary.  raises an error if it encounters nonmanifold
	 * geometry. */
	BMW_ISLANDBOUND,
	/* walk over all faces in an island of tool flagged faces. */
	BMW_ISLAND,
	/* walk from a vertex to all connected vertices. */
	BMW_CONNECTED_VERTEX,
	/* end of array index enum vals */

	/* do not intitialze function pointers and struct size in BMW_init */
	BMW_CUSTOM,
	BMW_MAXWALKERS
};

/* use with BMW_init, so as not to confuse with restrict flags */
#define BMW_NIL_LAY  0

#endif /* __BMESH_WALKERS_H__ */
