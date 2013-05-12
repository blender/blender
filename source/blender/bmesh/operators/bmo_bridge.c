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
 * Contributor(s): Joseph Eagar, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_bridge.c
 *  \ingroup bmesh
 *
 * Connect verts across faces (splits faces) and bridge tool.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_MARK	4
#define FACE_OUT	16

/* get the 2 loops matching 2 verts.
 * first attempt to get the face corners that use the edge defined by v1 & v2,
 * if that fails just get any loop thats on the vert (the first one) */
static void bm_vert_loop_pair(BMesh *bm, BMVert *v1, BMVert *v2, BMLoop **l1, BMLoop **l2)
{
	BMIter liter;
	BMLoop *l;

	if ((v1->e && v1->e->l) &&
	    (v2->e && v2->e->l))
	{
		BM_ITER_ELEM (l, &liter, v1, BM_LOOPS_OF_VERT) {
			if (l->prev->v == v2) {
				*l1 = l;
				*l2 = l->prev;
				return;
			}
			else if (l->next->v == v2) {
				*l1 = l;
				*l2 = l->next;
				return;
			}
		}
	}

	/* fallback to _any_ loop */
	*l1 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v1, 0);
	*l2 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v2, 0);
}

/* el_b can have any offset */
static float bm_edgeloop_offset_length(LinkData *el_a, LinkData *el_b,
                                       LinkData *el_b_first, const float len_max)
{
	float len = 0.0f;
	BLI_assert(el_a->prev == NULL);  /* must be first */
	do {
		len += len_v3v3(((BMVert *)el_a->data)->co, ((BMVert *)el_b->data)->co);
	} while ((el_b = el_b->next ? el_b->next : el_b_first),
	         (el_a = el_a->next) && (len < len_max));
	return len;
}

static void bm_bridge_best_rotation(struct BMEdgeLoopStore *el_store_a, struct BMEdgeLoopStore *el_store_b)
{
	ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
	ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);
	LinkData *el_a = lb_a->first;
	LinkData *el_b = lb_b->first;
	LinkData *el_b_first = el_b;
	LinkData *el_b_best = NULL;

	float len_best = FLT_MAX;

	for (; el_b; el_b = el_b->next) {
		const float len = bm_edgeloop_offset_length(el_a, el_b, el_b_first, len_best);
		if (len < len_best) {
			el_b_best = el_b;
			len_best = len;
		}
	}

	if (el_b_best) {
		BLI_rotatelist(lb_b, el_b_best);
	}
}

static bool bm_edge_test_cb(BMEdge *e, void *bm_v)
{
	return BMO_elem_flag_test((BMesh *)bm_v, e, EDGE_MARK);
}

static void bridge_loop_pair(BMesh *bm,
                             struct BMEdgeLoopStore *el_store_a,
                             struct BMEdgeLoopStore *el_store_b,
                             const bool use_merge, const float merge_factor)
{
	LinkData *el_a_first, *el_b_first;
	const bool is_closed = BM_edgeloop_is_closed(el_store_a) && BM_edgeloop_is_closed(el_store_b);
	int el_store_a_len, el_store_b_len;
	bool el_store_b_free = false;

	el_store_a_len = BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store_a);
	el_store_b_len = BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store_b);

	if (el_store_a_len < el_store_b_len) {
		SWAP(int, el_store_a_len, el_store_b_len);
		SWAP(struct BMEdgeLoopStore *, el_store_a, el_store_b);
	}

	if (use_merge) {
		BLI_assert((el_store_a_len == el_store_a_len));
	}

	if (el_store_a_len != el_store_b_len) {
		BM_mesh_elem_hflag_disable_all(bm, BM_FACE | BM_EDGE, BM_ELEM_TAG, false);
	}

	if (dot_v3v3(BM_edgeloop_normal_get(el_store_a), BM_edgeloop_normal_get(el_store_b)) < 0.0f) {
		BM_edgeloop_flip(bm, el_store_b);
	}

	/* we only care about flipping if we make faces */
	if (use_merge == false) {
		float no[3];
		float dir[3];

		add_v3_v3v3(no, BM_edgeloop_normal_get(el_store_a), BM_edgeloop_normal_get(el_store_b));
		sub_v3_v3v3(dir, BM_edgeloop_center_get(el_store_a), BM_edgeloop_center_get(el_store_b));

		if (dot_v3v3(no, dir) < 0.0f) {
			BM_edgeloop_flip(bm, el_store_a);
			BM_edgeloop_flip(bm, el_store_b);
		}

		/* vote on winding (so new face winding is based on existing connected faces) */
		if (bm->totface) {
			struct BMEdgeLoopStore *estore_pair[2] = {el_store_a, el_store_b};
			int i;
			int winding_votes = 0;
			int winding_dir = 1;
			for (i = 0; i < 2; i++, winding_dir = -winding_dir) {
				LinkData *el;
				for (el = BM_edgeloop_verts_get(estore_pair[i])->first; el; el = el->next) {
					LinkData *el_next = BM_EDGELOOP_NEXT(estore_pair[i], el);
					BMEdge *e = BM_edge_exists(el->data, el_next->data);
					if (e && BM_edge_is_boundary(e)) {
						winding_votes += ((e->l->v == el->data) ? winding_dir : -winding_dir);
					}
				}
			}

			if (winding_votes < 0) {
				BM_edgeloop_flip(bm, el_store_a);
				BM_edgeloop_flip(bm, el_store_b);
			}
		}
	}

	if (el_store_a_len > el_store_b_len) {
		el_store_b = BM_edgeloop_copy(el_store_b);
		BM_edgeloop_expand(bm, el_store_b, el_store_a_len);
		el_store_b_free = true;
	}

	if (is_closed) {
		bm_bridge_best_rotation(el_store_a, el_store_b);
	}

	/* Assign after flipping is finalized */
	el_a_first = BM_edgeloop_verts_get(el_store_a)->first;
	el_b_first = BM_edgeloop_verts_get(el_store_b)->first;


	if (use_merge) {
		LinkData *el_a;
		LinkData *el_b;
		const int vert_len = BM_edgeloop_length_get(el_store_a);
		const int edge_len = is_closed ? vert_len : vert_len - 1;
		BMEdge **earr_a = MEM_mallocN(sizeof(*earr_a) * vert_len, __func__);
		BMEdge **earr_b = MEM_mallocN(sizeof(*earr_b) * vert_len, __func__);
		int i;

		el_a = el_a_first;
		el_b = el_b_first;

		/* first get the edges in order (before splicing verts) */
		for (i = 0; i < vert_len; i++) {
			LinkData *el_a_next = BM_EDGELOOP_NEXT(el_store_a, el_a);
			LinkData *el_b_next = BM_EDGELOOP_NEXT(el_store_b, el_b);

			/* last edge will be NULL for non closed loops */
			earr_a[i] = BM_edge_exists(el_a->data, el_a_next->data);
			earr_b[i] = BM_edge_exists(el_b->data, el_b_next->data);

			el_a = el_a_next;
			el_b = el_b_next;
		}

		el_a = el_a_first;
		el_b = el_b_first;
		for (i = 0; i < vert_len; i++) {
			BMVert *v_a = el_a->data, *v_b = el_b->data;
			BM_data_interp_from_verts(bm, v_a, v_b, v_b, merge_factor);
			interp_v3_v3v3(v_b->co, v_a->co, v_b->co, merge_factor);
			BM_elem_flag_merge(v_a, v_b);
			BM_vert_splice(bm, v_a, v_b);

			el_a = el_a->next;
			el_b = el_b->next;
		}
		for (i = 0; i < edge_len; i++) {
			BMEdge *e1 = earr_a[i];
			BMEdge *e2 = earr_b[i];
			BM_data_interp_from_edges(bm, e1, e2, e2, merge_factor);
			BM_elem_flag_merge(e1, e2);
			BM_edge_splice(bm, e1, e2);
		}

		MEM_freeN(earr_a);
		MEM_freeN(earr_b);
	}
	else {
		LinkData *el_a = el_a_first;
		LinkData *el_b = el_b_first;

		LinkData *el_a_next;
		LinkData *el_b_next;


		while (true) {
			BMFace *f, *f_example;
			BMLoop *l_iter;
			BMVert *v_a, *v_b, *v_a_next, *v_b_next;

			BMLoop *l_1 = NULL;
			BMLoop *l_2 = NULL;
			BMLoop *l_1_next = NULL;
			BMLoop *l_2_next = NULL;

			if (is_closed) {
				el_a_next = BM_EDGELOOP_NEXT(el_store_a, el_a);
				el_b_next = BM_EDGELOOP_NEXT(el_store_b, el_b);
			}
			else {
				el_a_next = el_a->next;
				el_b_next = el_b->next;
				if (ELEM(NULL, el_a_next, el_b_next)) {
					break;
				}
			}

			v_a = el_a->data;
			v_b = el_b->data;
			v_a_next = el_a_next->data;
			v_b_next = el_b_next->data;

			/* get loop data - before making the face */
			if (v_b != v_b_next) {
				bm_vert_loop_pair(bm, v_a, v_b, &l_1, &l_2);
				bm_vert_loop_pair(bm, v_a_next, v_b_next, &l_1_next, &l_2_next);
			}
			else {
				/* lazy, could be more clever here */
				l_1      = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v_a, 0);
				l_1_next = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v_a_next, 0);
				l_2      = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v_b, 0);
				l_2_next = l_2;
			}

			if (l_1 && l_1_next == NULL) l_1_next = l_1;
			if (l_1_next && l_1 == NULL) l_1 = l_1_next;
			if (l_2 && l_2_next == NULL) l_2_next = l_2;
			if (l_2_next && l_2 == NULL) l_2 = l_2_next;
			f_example = l_1 ? l_1->f : (l_2 ? l_2->f : NULL);

			if (v_b != v_b_next) {
				/* copy if loop data if its is missing on one ring */
				f = BM_face_create_quad_tri(bm, v_a, v_b, v_b_next, v_a_next, f_example, true);
				BMO_elem_flag_enable(bm, f, FACE_OUT);
				BM_elem_flag_enable(f, BM_ELEM_TAG);

				l_iter = BM_FACE_FIRST_LOOP(f);

				if (l_1)      BM_elem_attrs_copy(bm, bm, l_1,      l_iter); l_iter = l_iter->next;
				if (l_2)      BM_elem_attrs_copy(bm, bm, l_2,      l_iter); l_iter = l_iter->next;
				if (l_2_next) BM_elem_attrs_copy(bm, bm, l_2_next, l_iter); l_iter = l_iter->next;
				if (l_1_next) BM_elem_attrs_copy(bm, bm, l_1_next, l_iter);
			}
			else {
				/* fan-fill a triangle */
				f = BM_face_create_quad_tri(bm, v_a, v_b, v_a_next, NULL, f_example, true);
				BMO_elem_flag_enable(bm, f, FACE_OUT);
				BM_elem_flag_enable(f, BM_ELEM_TAG);

				l_iter = BM_FACE_FIRST_LOOP(f);

				if (l_1)      BM_elem_attrs_copy(bm, bm, l_1,      l_iter); l_iter = l_iter->next;
				if (l_2)      BM_elem_attrs_copy(bm, bm, l_2,      l_iter); l_iter = l_iter->next;
				if (l_2_next) BM_elem_attrs_copy(bm, bm, l_1_next, l_iter);
			}

			if (el_a_next == el_a_first) {
				break;
			}

			el_a = el_a_next;
			el_b = el_b_next;
		}
	}

	if (el_store_a_len != el_store_b_len) {
		struct BMEdgeLoopStore *estore_pair[2] = {el_store_a, el_store_b};
		int i;

		BMOperator op_sub;
		/* when we have to bridge betweeen different sized edge-loops,
		 * be clever and post-process for best results */
		BM_mesh_triangulate(bm, true, true, NULL, NULL);

		/* tag verts on each side so we can restrict rotation of edges to verts on the same side */
		for (i = 0; i < 2; i++) {
			LinkData *el;
			for (el = BM_edgeloop_verts_get(estore_pair[i])->first; el; el = el->next) {
				BM_elem_flag_set((BMVert *)el->data, BM_ELEM_TAG, i);
			}
		}

		BMO_op_initf(bm, &op_sub, 0,
		             "beautify_fill faces=%hf edges=ae use_restrict_tag=%b",
		             BM_ELEM_TAG, true);
		BMO_op_exec(bm, &op_sub);
		/* there may also be tagged faces that didnt rotate, mark input */
		BMO_slot_buffer_flag_enable(bm, op_sub.slots_in, "faces", BM_FACE, FACE_OUT);
		BMO_slot_buffer_flag_enable(bm, op_sub.slots_out, "geom.out", BM_FACE, FACE_OUT);
		BMO_op_finish(bm, &op_sub);
	}

	if (el_store_b_free) {
		BM_edgeloop_free(el_store_b);
	}
}

void bmo_bridge_loops_exec(BMesh *bm, BMOperator *op)
{
	ListBase eloops = {NULL};
	LinkData *el_store;

	/* merge-bridge support */
	const bool  use_merge    = BMO_slot_bool_get(op->slots_in,  "use_merge");
	const float merge_factor = BMO_slot_float_get(op->slots_in, "merge_factor");
	const bool  use_cyclic   = BMO_slot_bool_get(op->slots_in,  "use_cyclic") && (use_merge == false);
	int count;
	bool change = false;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

	count = BM_mesh_edgeloops_find(bm, &eloops, bm_edge_test_cb, bm);

	BM_mesh_edgeloops_calc_normal(bm, &eloops);
	BM_mesh_edgeloops_calc_center(bm, &eloops);

	if (count < 2) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Select at least two edge loops");
		goto cleanup;
	}

	if (use_merge) {
		bool match = true;
		const int eloop_len = BM_edgeloop_length_get(eloops.first);
		for (el_store = eloops.first; el_store; el_store = el_store->next) {
			if (eloop_len != BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store)) {
				match = false;
				break;
			}
		}
		if (!match) {
			BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
			                "Selected loops must have equal edge counts");
			goto cleanup;
		}
	}

	if (count > 2) {
		BM_mesh_edgeloops_calc_order(bm, &eloops);
	}

	for (el_store = eloops.first; el_store; el_store = el_store->next) {
		LinkData *el_store_next = el_store->next;

		if (el_store_next == NULL) {
			if (use_cyclic && (count > 2)) {
				el_store_next = eloops.first;
			}
			else {
				break;
			}
		}

		bridge_loop_pair(bm,
		                 (struct BMEdgeLoopStore *)el_store,
		                 (struct BMEdgeLoopStore *)el_store_next,
		                 use_merge, merge_factor);
		change = true;
	}

	if ((count == 2) && (BM_edgeloop_length_get(eloops.first) == BM_edgeloop_length_get(eloops.last))) {



	}
	else if (count == 2) {

	}
	else {

	}

cleanup:
	BM_mesh_edgeloops_free(&eloops);

	if (change) {
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
	}
}
