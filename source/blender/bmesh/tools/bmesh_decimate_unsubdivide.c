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

/** \file blender/bmesh/tools/bmesh_decimate_unsubdivide.c
 *  \ingroup bmesh
 *
 * BMesh decimator that uses a grid un-subdivide method.
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_decimate.h"  /* own include */


static bool bm_vert_dissolve_fan_test(BMVert *v)
{
	/* check if we should walk over these verts */
	BMIter iter;
	BMEdge *e;

	BMVert *varr[4];

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

		/* bail out early */
		if (tot_edge == 4) {
			return false;
		}

		/* used to check overlapping faces */
		varr[tot_edge] = BM_edge_other_vert(e, v);

		tot_edge++;
	}

	if (((tot_edge == 4) && (tot_edge_boundary == 0) && (tot_edge_manifold == 4)) ||
	    ((tot_edge == 3) && (tot_edge_boundary == 0) && (tot_edge_manifold == 3)) ||
	    ((tot_edge == 3) && (tot_edge_boundary == 2) && (tot_edge_manifold == 1)))
	{
		if (!BM_face_exists(varr, tot_edge, NULL)) {
			return true;
		}
	}
	else if ((tot_edge == 2) && (tot_edge_wire == 2)) {
		return true;
	}
	return false;
}

static bool bm_vert_dissolve_fan(BMesh *bm, BMVert *v)
{
	/* collapse under 2 conditions.
	 * - vert connects to 4 manifold edges (and 4 faces).
	 * - vert connects to 1 manifold edge, 2 boundary edges (and 2 faces).
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
			return (BM_vert_collapse_edge(bm, v->e, v, true) != NULL);
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
				BMLoop *l_new;
				BLI_assert(l->prev->v != l->next->v);
				BM_face_split(bm, l->f, l->prev, l->next, &l_new, NULL, true);
				BM_elem_flag_merge_into(l_new->e, l->e, l->prev->e);
			}
		}

		return BM_vert_dissolve(bm, v);
	}

	return false;
}

enum {
	VERT_INDEX_DO_COLLAPSE  = -1,
	VERT_INDEX_INIT         =  0,
	VERT_INDEX_IGNORE       =  1
};

// #define USE_WALKER  /* gives uneven results, disable for now */

/* - BMVert.flag & BM_ELEM_TAG:  shows we touched this vert
 * - BMVert.index == -1:         shows we will remove this vert
 */

/**
 * \param tag_only so we can call this from an operator */
void BM_mesh_decimate_unsubdivide_ex(BMesh *bm, const int iterations, const bool tag_only)
{
#ifdef USE_WALKER
#  define ELE_VERT_TAG 1
#else
	BMVert **vert_seek_a = MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__);
	BMVert **vert_seek_b = MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__);
	unsigned vert_seek_a_tot = 0;
	unsigned vert_seek_b_tot = 0;
#endif

	BMIter iter;

	const unsigned int offset = 0;
	const unsigned int nth = 2;

	int iter_step;

	/* if tag_only is set, we assume the caller knows what verts to tag
	 * needed for the operator */
	if (tag_only == false) {
		BMVert *v;
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
		}
	}

	for (iter_step = 0; iter_step < iterations; iter_step++) {
		BMVert *v, *v_next;
		bool iter_done;

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
		while (true) {
#ifdef USE_WALKER
			BMWalker walker;
#else
			unsigned int depth = 1;
			unsigned int i;
#endif
			BMVert *v_first = NULL;

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

			BM_elem_index_set(v_first, ((offset + depth) % nth) ? VERT_INDEX_IGNORE : VERT_INDEX_DO_COLLAPSE);  /* set_dirty! */

			vert_seek_b_tot = 0;
			vert_seek_b[vert_seek_b_tot++] = v_first;

			while (true) {
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
		iter_done = false;
		BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_index_get(v) == VERT_INDEX_DO_COLLAPSE) {
				if (bm_vert_dissolve_fan(bm, v)) {
					iter_done = true;
				}
			}
		}

		if (iter_done == false) {
			break;
		}
	}

	bm->elem_index_dirty |= BM_VERT;

#ifndef USE_WALKER
	MEM_freeN(vert_seek_a);
	MEM_freeN(vert_seek_b);
#endif
}

void BM_mesh_decimate_unsubdivide(BMesh *bm, const int iterations)
{
	BM_mesh_decimate_unsubdivide_ex(bm, iterations, false);
}
