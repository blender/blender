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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_unsubdivide.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */


static int bm_vert_dissolve_fan_test(BMVert *v)
{
	/* check if we should walk over these verts */
	BMIter iter;
	BMEdge *e;

	unsigned int tot_edge = 0;
	unsigned int tot_edge_boundary = 0;
	unsigned int tot_edge_manifold = 0;
	unsigned int tot_edge_wire     = 0;

	BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_edge_is_boundary(e)) {
			tot_edge_boundary++;
		}
		else if (BM_edge_is_manifold(e)) {
			tot_edge_manifold++;
		}
		else if (BM_edge_is_wire(e)) {
			tot_edge_wire++;
		}
		tot_edge++;
	}

	if ((tot_edge == 4) && (tot_edge_boundary == 0) && (tot_edge_manifold == 4)) {
		return TRUE;
	}
	else if ((tot_edge == 3) && (tot_edge_boundary == 0) && (tot_edge_manifold == 3)) {
		return TRUE;
	}
	else if ((tot_edge == 3) && (tot_edge_boundary == 2) && (tot_edge_manifold == 1)) {
		return TRUE;
	}
	else if ((tot_edge == 2) && (tot_edge_wire == 2)) {
		return TRUE;
	}
	return FALSE;
}

static int bm_vert_dissolve_fan(BMesh *bm, BMVert *v)
{
	/* collapse under 2 conditions.
	 * - vert connects to 4 manifold edges (and 4 faces).
	 * - vert connecrs to 1 manifold edge, 2 boundary edges (and 2 faces).
	 *
	 * This covers boundary verts of a quad grid and center verts.
	 * note that surrounding faces dont have to be quads.
	 */

	BMIter iter;
	BMEdge *e;

	unsigned int tot_loop = 0;
	unsigned int tot_edge = 0;
	unsigned int tot_edge_boundary = 0;
	unsigned int tot_edge_manifold = 0;
	unsigned int tot_edge_wire     = 0;

	BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_edge_is_boundary(e)) {
			tot_edge_boundary++;
		}
		else if (BM_edge_is_manifold(e)) {
			tot_edge_manifold++;
		}
		else if (BM_edge_is_wire(e)) {
			tot_edge_wire++;
		}
		tot_edge++;
	}

	if (tot_edge == 2) {
		/* check for 2 wire verts only */
		if (tot_edge_wire == 2) {
			return (BM_vert_collapse_edge(bm, v->e, v, TRUE) != NULL);
		}
	}
	else if (tot_edge == 4) {
		/* check for 4 faces surrounding */
		if (tot_edge_boundary == 0 && tot_edge_manifold == 4) {
			/* good to go! */
			tot_loop = 4;
		}
	}
	else if (tot_edge == 3) {
		/* check for 2 faces surrounding at a boundary */
		if (tot_edge_boundary == 2 && tot_edge_manifold == 1) {
			/* good to go! */
			tot_loop = 2;
		}
		else if (tot_edge_boundary == 0 && tot_edge_manifold == 3) {
			/* good to go! */
			tot_loop = 3;
		}
	}

	if (tot_loop) {
		BMLoop *f_loop[4];
		unsigned int i;

		/* ensure there are exactly tot_loop loops */
		BLI_assert(BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v, tot_loop) == NULL);
		BM_iter_as_array(bm, BM_LOOPS_OF_VERT, v, (void **)f_loop, tot_loop);

		for (i = 0; i < tot_loop; i++) {
			BMLoop *l = f_loop[i];
			if (l->f->len > 3) {
				BLI_assert(l->prev->v != l->next->v);
				BM_face_split(bm, l->f, l->prev->v, l->next->v, NULL, NULL, TRUE);
			}
		}

		return BM_vert_dissolve(bm, v);
	}

	return FALSE;
}

enum {
	VERT_INDEX_DO_COLLAPSE  = -1,
	VERT_INDEX_INIT         =  0,
	VERT_INDEX_IGNORE       =  1
};

// #define USE_WALKER  /* gives uneven results, disable for now */
// #define USE_ALL_VERTS

/* - BMVert.flag & BM_ELEM_TAG:  shows we touched this vert
 * - BMVert.index == -1:         shows we will remove this vert
 */
void bmo_unsubdivide_exec(BMesh *bm, BMOperator *op)
{
#ifdef USE_WALKER
#  define ELE_VERT_TAG 1
#else
	BMVert **vert_seek_a = MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__);
	BMVert **vert_seek_b = MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__);
	unsigned vert_seek_a_tot = 0;
	unsigned vert_seek_b_tot = 0;
#endif

	BMVert *v;
	BMIter iter;

	const unsigned int offset = 0;
	const unsigned int nth = 2;

	const int iterations = maxi(1, BMO_slot_int_get(op, "iterations"));
	int iter_step;

#ifdef USE_ALL_VERTS
	(void)op;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_enable(v, BM_ELEM_TAG);
	}
#else  /* USE_ALL_VERTS */
	BMOpSlot *vinput = BMO_slot_get(op, "verts");
	BMVert **vinput_arr = (BMVert **)vinput->data.p;
	int v_index;

	/* tag verts */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}
	for (v_index = 0; v_index < vinput->len; v_index++) {
		v = vinput_arr[v_index];
		BM_elem_flag_enable(v, BM_ELEM_TAG);
	}
#endif  /* USE_ALL_VERTS */


	for (iter_step = 0; iter_step < iterations; iter_step++) {
		int iter_done;

		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG) && bm_vert_dissolve_fan_test(v)) {
#ifdef USE_WALKER
				BMO_elem_flag_enable(bm, v, ELE_VERT_TAG);
#endif
				BM_elem_index_set(v, VERT_INDEX_INIT);  /* set_dirty! */
			}
			else {
				BM_elem_index_set(v, VERT_INDEX_IGNORE);  /* set_dirty! */
			}
		}
		/* done with selecting tagged verts */


		/* main loop, keep tagging until we can't tag any more islands */
		while (TRUE) {
#ifdef USE_WALKER
			BMWalker walker;
#else
			unsigned int depth = 1;
			unsigned int i;
#endif
			BMVert *v_first = NULL;
			BMVert *v;

			/* we could avoid iterating from the start each time */
			BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
				if (v->e && (BM_elem_index_get(v) == VERT_INDEX_INIT)) {
#ifdef USE_WALKER
					if (BMO_elem_flag_test(bm, v, ELE_VERT_TAG))
#endif
					{
						/* check again incase the topology changed */
						if (bm_vert_dissolve_fan_test(v)) {
							v_first = v;
						}
						break;
					}
				}
			}
			if (v_first == NULL) {
				break;
			}

#ifdef USE_WALKER
			/* Walk over selected elements starting at active */
			BMW_init(&walker, bm, BMW_CONNECTED_VERTEX,
					 ELE_VERT_TAG, BMW_MASK_NOP, BMW_MASK_NOP,
					 BMW_FLAG_NOP, /* don't use BMW_FLAG_TEST_HIDDEN here since we want to desel all */
					 BMW_NIL_LAY);

			BLI_assert(walker.order == BMW_BREADTH_FIRST);
			for (v = BMW_begin(&walker, v_first); v != NULL; v = BMW_step(&walker)) {
				/* Deselect elements that aren't at "nth" depth from active */
				if (BM_elem_index_get(v) == VERT_INDEX_INIT) {
					if ((offset + BMW_current_depth(&walker)) % nth) {
						/* tag for removal */
						BM_elem_index_set(v, VERT_INDEX_DO_COLLAPSE);  /* set_dirty! */
					}
					else {
						/* works better to allow these verts to be checked again */
						//BM_elem_index_set(v, VERT_INDEX_IGNORE);  /* set_dirty! */
					}
				}
			}
			BMW_end(&walker);
#else

			BM_elem_index_set(v_first, (offset + depth) % nth ? VERT_INDEX_IGNORE : VERT_INDEX_DO_COLLAPSE);  /* set_dirty! */

			vert_seek_b_tot = 0;
			vert_seek_b[vert_seek_b_tot++] = v_first;

			while (TRUE) {
				BMEdge *e;

				if ((offset + depth) % nth) {
					vert_seek_a_tot = 0;
					for (i = 0; i < vert_seek_b_tot; i++) {
						v = vert_seek_b[i];
						BLI_assert(BM_elem_index_get(v) == VERT_INDEX_IGNORE);
						BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
							BMVert *v_other = BM_edge_other_vert(e, v);
							if (BM_elem_index_get(v_other) == VERT_INDEX_INIT) {
								BM_elem_index_set(v_other, VERT_INDEX_DO_COLLAPSE);  /* set_dirty! */
								vert_seek_a[vert_seek_a_tot++] = v_other;
							}
						}
					}
					if (vert_seek_a_tot == 0) {
						break;
					}
				}
				else {
					vert_seek_b_tot = 0;
					for (i = 0; i < vert_seek_a_tot; i++) {
						v = vert_seek_a[i];
						BLI_assert(BM_elem_index_get(v) == VERT_INDEX_DO_COLLAPSE);
						BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
							BMVert *v_other = BM_edge_other_vert(e, v);
							if (BM_elem_index_get(v_other) == VERT_INDEX_INIT) {
								BM_elem_index_set(v_other, VERT_INDEX_IGNORE);  /* set_dirty! */
								vert_seek_b[vert_seek_b_tot++] = v_other;
							}
						}
					}
					if (vert_seek_b_tot == 0) {
						break;
					}
				}

				depth++;
			}
#endif  /* USE_WALKER */

		}

		/* now we tagged all verts -1 for removal, lets loop over and rebuild faces */
		iter_done = FALSE;
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_index_get(v) == VERT_INDEX_DO_COLLAPSE) {
				iter_done |= bm_vert_dissolve_fan(bm, v);
			}
		}

		if (iter_done == FALSE) {
			break;
		}
	}

	bm->elem_index_dirty |= BM_VERT;

#ifndef USE_WALKER
	MEM_freeN(vert_seek_a);
	MEM_freeN(vert_seek_b);
#endif

}

