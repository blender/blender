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

/** \file blender/bmesh/tools/bmesh_edgesplit.c
 *  \ingroup bmesh
 *
 * Edge-Split.
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_edgesplit.h"  /* own include */


/**
 * Remove the BM_ELEM_TAG flag for edges we cant split
 *
 * un-tag edges not connected to other tagged edges,
 * unless they are on a boundary
 */
static void bm_edgesplit_validate_seams(BMesh *bm)
{
	BMIter iter;
	BMEdge *e;

	unsigned char *vtouch;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	vtouch = MEM_callocN(sizeof(char) * bm->totvert, __func__);

	/* tag all boundary verts so as not to untag an edge which is inbetween only 2 faces [] */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {

		/* unrelated to flag assignment in this function - since this is the
		 * only place we loop over all edges, disable tag */
		BM_elem_flag_disable(e, BM_ELEM_INTERNAL_TAG);

		if (e->l == NULL) {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
		else if (BM_edge_is_boundary(e)) {
			unsigned char *vt;
			vt = &vtouch[BM_elem_index_get(e->v1)]; if (*vt < 2) (*vt)++;
			vt = &vtouch[BM_elem_index_get(e->v2)]; if (*vt < 2) (*vt)++;

			/* while the boundary verts need to be tagged,
			 * the edge its self can't be split */
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
	}

	/* single marked edges unconnected to any other marked edges
	 * are illegal, go through and unmark them */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			/* lame, but we don't want the count to exceed 255,
			 * so just count to 2, its all we need */
			unsigned char *vt;
			vt = &vtouch[BM_elem_index_get(e->v1)]; if (*vt < 2) (*vt)++;
			vt = &vtouch[BM_elem_index_get(e->v2)]; if (*vt < 2) (*vt)++;
		}
	}
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			if (vtouch[BM_elem_index_get(e->v1)] == 1 &&
			    vtouch[BM_elem_index_get(e->v2)] == 1)
			{
				BM_elem_flag_disable(e, BM_ELEM_TAG);
			}
		}
	}

	MEM_freeN(vtouch);
}

void BM_mesh_edgesplit(BMesh *bm, const bool use_verts, const bool tag_only, const bool copy_select)
{
	BMIter iter;
	BMEdge *e;

	bool use_ese = false;
	GHash *ese_gh = NULL;

	if (copy_select && bm->selected.first) {
		BMEditSelection *ese;

		ese_gh = BLI_ghash_ptr_new(__func__);
		for (ese = bm->selected.first; ese; ese = ese->next) {
			if (ese->htype != BM_FACE) {
				BLI_ghash_insert(ese_gh, ese->ele, ese);
			}
		}

		use_ese = true;
	}

	if (tag_only == false) {
		BM_mesh_elem_hflag_enable_all(bm, BM_EDGE | (use_verts ? BM_VERT : 0), BM_ELEM_TAG, false);
	}

	if (use_verts) {
		/* prevent one edge having both verts unflagged
		 * we could alternately disable these edges, either way its a corner case.
		 *
		 * This is needed so we don't split off the edge but then none of its verts which
		 * would leave a duplicate edge.
		 */
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
				if (UNLIKELY(((BM_elem_flag_test(e->v1, BM_ELEM_TAG) == false) &&
				              (BM_elem_flag_test(e->v2, BM_ELEM_TAG) == false))))
				{
					BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
					BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
				}
			}
		}
	}

	bm_edgesplit_validate_seams(bm);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			/* this flag gets copied so we can be sure duplicate edges get it too (important) */
			BM_elem_flag_enable(e, BM_ELEM_INTERNAL_TAG);

			/* keep splitting until each loop has its own edge */
			while (!BM_edge_is_boundary(e)) {
				BMLoop *l_sep = e->l;
				bmesh_edge_separate(bm, e, l_sep, copy_select);
				BLI_assert(l_sep->e != e);

				if (use_ese) {
					BMEditSelection *ese = BLI_ghash_lookup(ese_gh, e);
					if (UNLIKELY(ese)) {
						BM_select_history_store_after_notest(bm, ese, l_sep->e);
					}
				}
			}

			BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
			BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
		}
	}

	if (use_verts) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e->v1, BM_ELEM_TAG) == false) {
				BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
			}
			if (BM_elem_flag_test(e->v2, BM_ELEM_TAG) == false) {
				BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
			}
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			unsigned int i;
			for (i = 0; i < 2; i++) {
				BMVert *v = ((&e->v1)[i]);
				if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
					BM_elem_flag_disable(v, BM_ELEM_TAG);

					if (use_ese) {
						BMVert **vtar;
						int vtar_len;

						bmesh_vert_separate(bm, v, &vtar, &vtar_len, copy_select);

						/* first value is always in 'v' */
						if (vtar_len > 1) {
							BMEditSelection *ese = BLI_ghash_lookup(ese_gh, v);
							BLI_assert(v == vtar[0]);
							if (UNLIKELY(ese)) {
								int j;
								for (j = 1; j < vtar_len; j++) {
									BLI_assert(v != vtar[j]);
									BM_select_history_store_after_notest(bm, ese, vtar[j]);
								}
							}
						}
						MEM_freeN(vtar);
					}
					else {
						bmesh_vert_separate(bm, v, NULL, NULL, copy_select);
					}
				}
			}
		}
	}

	if (use_ese) {
		BLI_ghash_free(ese_gh, NULL, NULL);
	}
}
