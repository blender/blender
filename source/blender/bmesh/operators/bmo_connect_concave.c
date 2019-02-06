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

/** \file \ingroup bmesh
 *
 * Connect vertices so all resulting faces are convex.
 *
 * Implementation:
 *
 * - triangulate all concave face (tagging convex verts),
 * - rotate edges (beautify) so edges will connect nearby verts.
 * - sort long edges (longest first),
 *   put any edges between 2 convex verts last since they often split convex regions.
 * - merge the sorted edges as long as they don't create convex ngons.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_memarena.h"
#include "BLI_heap.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_linklist.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_OUT	(1 << 0)
#define FACE_OUT	(1 << 1)

static int bm_edge_length_cmp(const void *a_, const void *b_)
{
	const BMEdge *e_a = *(const void **)a_;
	const BMEdge *e_b = *(const void **)b_;

	int e_a_concave = ((BM_elem_flag_test(e_a->v1, BM_ELEM_TAG)) && (BM_elem_flag_test(e_a->v2, BM_ELEM_TAG)));
	int e_b_concave = ((BM_elem_flag_test(e_b->v1, BM_ELEM_TAG)) && (BM_elem_flag_test(e_b->v2, BM_ELEM_TAG)));

	/* merge edges between concave edges last since these
	 * are most likely to remain and be the main dividers */
	if      (e_a_concave < e_b_concave) return -1;
	else if (e_a_concave > e_b_concave) return  1;
	else {
		/* otherwise shortest edges last */
		const float e_a_len = BM_edge_calc_length_squared(e_a);
		const float e_b_len = BM_edge_calc_length_squared(e_b);
		if      (e_a_len < e_b_len) return  1;
		else if (e_a_len > e_b_len) return -1;
		else                        return  0;
	}
}

static bool bm_face_split_by_concave(
        BMesh *bm, BMFace *f_base, const float eps,

        MemArena *pf_arena,
        struct Heap *pf_heap)
{
	const int f_base_len = f_base->len;
	int faces_array_tot = f_base_len - 3;
	int edges_array_tot = f_base_len - 3;
	BMFace  **faces_array = BLI_array_alloca(faces_array, faces_array_tot);
	BMEdge  **edges_array = BLI_array_alloca(edges_array, edges_array_tot);
	const int quad_method = 0, ngon_method = 0;  /* beauty */
	LinkNode *faces_double = NULL;

	float normal[3];
	BLI_assert(f_base->len > 3);

	copy_v3_v3(normal, f_base->no);

	BM_face_triangulate(
	        bm, f_base,
	        faces_array, &faces_array_tot,
	        edges_array, &edges_array_tot,
	        &faces_double,
	        quad_method, ngon_method, false,
	        pf_arena,
	        pf_heap);

	BLI_assert(edges_array_tot <= f_base_len - 3);

	if (faces_array_tot) {
		int i;
		for (i = 0; i < faces_array_tot; i++) {
			BMFace *f = faces_array[i];
			BMO_face_flag_enable(bm, f, FACE_OUT);
		}
	}
	BMO_face_flag_enable(bm, f_base, FACE_OUT);

	if (edges_array_tot) {
		int i;

		qsort(edges_array, edges_array_tot, sizeof(*edges_array), bm_edge_length_cmp);

		for (i = 0; i < edges_array_tot; i++) {
			BMLoop *l_pair[2];
			BMEdge *e = edges_array[i];
			BMO_edge_flag_enable(bm, e, EDGE_OUT);

			if (BM_edge_is_contiguous(e) &&
			    BM_edge_loop_pair(e, &l_pair[0], &l_pair[1]))
			{
				bool ok = true;
				int j;
				for (j = 0; j < 2; j++) {
					BMLoop *l = l_pair[j];

					/* check that merging the edge (on this side)
					 * wouldn't result in a convex face-loop.
					 *
					 * This is the (l->next, l->prev) we would have once joined.
					 */
					float cross[3];
					cross_tri_v3(
					        cross,
					        l->v->co,
					        l->radial_next->next->next->v->co,
					        l->prev->v->co
					        );

					if (dot_v3v3(cross, normal) <= eps) {
						ok = false;
						break;
					}
				}

				if (ok) {
					BMFace *f_new, *f_pair[2] = {l_pair[0]->f, l_pair[1]->f};
					f_new = BM_faces_join(bm, f_pair, 2, true);
					if (f_new) {
						BMO_face_flag_enable(bm, f_new, FACE_OUT);
					}
				}
			}
		}
	}

	BLI_heap_clear(pf_heap, NULL);

	while (faces_double) {
		LinkNode *next = faces_double->next;
		BM_face_kill(bm, faces_double->link);
		MEM_freeN(faces_double);
		faces_double = next;
	}

	return true;
}

static bool bm_face_convex_tag_verts(BMFace *f)
{
	bool is_concave = false;
	if (f->len > 3) {
		const BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (BM_loop_is_convex(l_iter) == false) {
				is_concave = true;
				BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
			}
			else {
				BM_elem_flag_disable(l_iter->v, BM_ELEM_TAG);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}
	return is_concave;
}

void bmo_connect_verts_concave_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *f;
	bool changed = false;

	MemArena *pf_arena;
	Heap *pf_heap;

	pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
	pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);

	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (f->len > 3 && bm_face_convex_tag_verts(f)) {
			if (bm_face_split_by_concave(
			        bm, f, FLT_EPSILON,
			        pf_arena, pf_heap))
			{
				changed = true;
			}
		}
	}

	if (changed) {
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
	}

	BLI_memarena_free(pf_arena);
	BLI_heap_free(pf_heap, NULL);
}
