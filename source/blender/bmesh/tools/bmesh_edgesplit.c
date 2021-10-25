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
 * \param use_verts  Use flagged verts instead of edges.
 * \param tag_only  Only split tagged edges.
 * \param copy_select  Copy selection history.
 */
void BM_mesh_edgesplit(
        BMesh *bm,
        const bool use_verts,
        const bool tag_only, const bool copy_select)
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

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
			BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			uint i;
			for (i = 0; i < 2; i++) {
				BMVert *v = ((&e->v1)[i]);
				if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
					BM_elem_flag_disable(v, BM_ELEM_TAG);

					if (use_ese) {
						BMVert **vtar;
						int vtar_len;

						BM_vert_separate_hflag(bm, v, BM_ELEM_TAG, copy_select, &vtar, &vtar_len);

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
						BM_vert_separate_hflag(bm, v, BM_ELEM_TAG, copy_select, NULL, NULL);
					}
				}
			}
		}
	}

#ifndef NDEBUG
	/* ensure we don't have any double edges! */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BLI_assert(BM_edge_find_double(e) == NULL);
		}
	}
#endif

	if (use_ese) {
		BLI_ghash_free(ese_gh, NULL, NULL);
	}
}
