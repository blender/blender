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

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_MARK	4
#define EDGE_OUT	8
#define FACE_OUT	16

/* el_a and el_b _must_ be same size */
static void bm_bridge_splice_loops(BMesh *bm, LinkData *el_a, LinkData *el_b, const float merge_factor)
{
	BMOperator op_weld;
	BMOpSlot *slot_targetmap;

	BMO_op_init(bm, &op_weld, 0, "weld_verts");

	slot_targetmap = BMO_slot_get(op_weld.slots_in, "targetmap");

	do {
		BMVert *v_a = el_a->data, *v_b = el_b->data;
		BM_data_interp_from_verts(bm, v_a, v_b, v_b, merge_factor);
		interp_v3_v3v3(v_b->co, v_a->co, v_b->co, merge_factor);
		BLI_assert(v_a != v_b);
		BMO_slot_map_elem_insert(&op_weld, slot_targetmap, v_a, v_b);
	} while ((el_b = el_b->next),
	         (el_a = el_a->next));

	BMO_op_exec(bm, &op_weld);
	BMO_op_finish(bm, &op_weld);
}

/* get the 2 loops matching 2 verts.
 * first attempt to get the face corners that use the edge defined by v1 & v2,
 * if that fails just get any loop thats on the vert (the first one) */
static void bm_vert_loop_pair(BMesh *bm, BMVert *v1, BMVert *v2, BMLoop **l1, BMLoop **l2)
{
	BMEdge *e = BM_edge_exists(v1, v2);
	BMLoop *l = e->l;

	if (l) {
		if (l->v == v1) {
			*l1 = l;
			*l2 = l->next;
		}
		else {
			*l2 = l;
			*l1 = l->next;
		}
	}
	else {
		/* fallback to _any_ loop */
		*l1 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v1, 0);
		*l2 = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v2, 0);
	}
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
		BLI_rotatelist_first(lb_b, el_b_best);
	}
}

static void bm_face_edges_tag_out(BMesh *bm, BMFace *f)
{
	BMLoop *l_iter, *l_first;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BMO_elem_flag_enable(bm, l_iter->e, EDGE_OUT);
	} while ((l_iter = l_iter->next) != l_first);
}

static bool bm_edge_test_cb(BMEdge *e, void *bm_v)
{
	return BMO_elem_flag_test((BMesh *)bm_v, e, EDGE_MARK);
}

static void bridge_loop_pair(BMesh *bm,
                             struct BMEdgeLoopStore *el_store_a,
                             struct BMEdgeLoopStore *el_store_b,
                             const bool use_merge, const float merge_factor, const int twist_offset)
{
	const float eps = 0.00001f;
	LinkData *el_a_first, *el_b_first;
	const bool is_closed = BM_edgeloop_is_closed(el_store_a) && BM_edgeloop_is_closed(el_store_b);
	int el_store_a_len, el_store_b_len;
	bool el_store_b_free = false;
	float el_dir[3];
	float dot_a, dot_b;
	const bool use_edgeout = true;

	el_store_a_len = BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store_a);
	el_store_b_len = BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store_b);

	if (el_store_a_len < el_store_b_len) {
		SWAP(int, el_store_a_len, el_store_b_len);
		SWAP(struct BMEdgeLoopStore *, el_store_a, el_store_b);
	}

	if (use_merge) {
		BLI_assert((el_store_a_len == el_store_b_len));
	}

	if (el_store_a_len != el_store_b_len) {
		BM_mesh_elem_hflag_disable_all(bm, BM_FACE | BM_EDGE, BM_ELEM_TAG, false);
	}

	sub_v3_v3v3(el_dir, BM_edgeloop_center_get(el_store_a), BM_edgeloop_center_get(el_store_b));

	if (is_closed) {
		/* if all loops are closed this will calculate twice for all loops */
		BM_edgeloop_calc_normal(bm, el_store_a);
		BM_edgeloop_calc_normal(bm, el_store_b);
	}
	else {
		ListBase *lb_a = BM_edgeloop_verts_get(el_store_a);
		ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);

		/* normalizing isn't strictly needed but without we may get very large values */
		float no[3];
		float dir_a[3], dir_b[3];

		sub_v3_v3v3(dir_a,
		            ((BMVert *)(((LinkData *)lb_a->first)->data))->co,
		            ((BMVert *)(((LinkData *)lb_a->last)->data))->co);
		sub_v3_v3v3(dir_b,
		            ((BMVert *)(((LinkData *)lb_b->first)->data))->co,
		            ((BMVert *)(((LinkData *)lb_b->last)->data))->co);

		/* make the directions point out from the normals, 'no' is used as a temp var */
		cross_v3_v3v3(no, dir_a, el_dir); cross_v3_v3v3(dir_a, no, el_dir);
		cross_v3_v3v3(no, dir_b, el_dir); cross_v3_v3v3(dir_b, no, el_dir);

		if (dot_v3v3(dir_a, dir_b) < 0.0f) {
			BM_edgeloop_flip(bm, el_store_b);
		}

		normalize_v3_v3(no, el_dir);
		BM_edgeloop_calc_normal_aligned(bm, el_store_a, no);
		BM_edgeloop_calc_normal_aligned(bm, el_store_b, no);
	}

	dot_a = dot_v3v3(BM_edgeloop_normal_get(el_store_a), el_dir);
	dot_b = dot_v3v3(BM_edgeloop_normal_get(el_store_b), el_dir);

	if (UNLIKELY((len_squared_v3(el_dir) < eps) ||
	             ((fabsf(dot_a) < eps) && (fabsf(dot_b) < eps))))
	{
		/* in this case there is no depth between the two loops,
		 * eg: 2x 2d circles, one scaled smaller,
		 * in this case 'el_dir' cant be used, just ensure we have matching flipping. */
		if (dot_v3v3(BM_edgeloop_normal_get(el_store_a),
		             BM_edgeloop_normal_get(el_store_b)) < 0.0f)
		{
			BM_edgeloop_flip(bm, el_store_b);
		}
	}
	else if ((dot_a < 0.0f) != (dot_b < 0.0f)) {
		BM_edgeloop_flip(bm, el_store_b);
	}

	/* we only care about flipping if we make faces */
	if (use_merge == false) {
		float no[3];

		add_v3_v3v3(no, BM_edgeloop_normal_get(el_store_a), BM_edgeloop_normal_get(el_store_b));

		if (dot_v3v3(no, el_dir) < 0.0f) {
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
					LinkData *el_next = BM_EDGELINK_NEXT(estore_pair[i], el);
					if (el_next) {
						BMEdge *e = BM_edge_exists(el->data, el_next->data);
						if (e && BM_edge_is_boundary(e)) {
							winding_votes += ((e->l->v == el->data) ? winding_dir : -winding_dir);
						}
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

		/* add twist */
		if (twist_offset != 0) {
			const int len_b = BM_edgeloop_length_get(el_store_b);
			ListBase *lb_b = BM_edgeloop_verts_get(el_store_b);
			LinkData *el_b = BLI_rfindlink(lb_b, mod_i(twist_offset, len_b));
			BLI_rotatelist_first(lb_b, el_b);
		}
	}

	/* Assign after flipping is finalized */
	el_a_first = BM_edgeloop_verts_get(el_store_a)->first;
	el_b_first = BM_edgeloop_verts_get(el_store_b)->first;


	if (use_merge) {
		bm_bridge_splice_loops(bm, el_a_first, el_b_first, merge_factor);
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

			BMLoop *l_a = NULL;
			BMLoop *l_b = NULL;
			BMLoop *l_a_next = NULL;
			BMLoop *l_b_next = NULL;

			if (is_closed) {
				el_a_next = BM_EDGELINK_NEXT(el_store_a, el_a);
				el_b_next = BM_EDGELINK_NEXT(el_store_b, el_b);
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
				bm_vert_loop_pair(bm, v_a, v_a_next, &l_a, &l_a_next);
				bm_vert_loop_pair(bm, v_b, v_b_next, &l_b, &l_b_next);
			}
			else {
				/* lazy, could be more clever here */
				bm_vert_loop_pair(bm, v_a, v_a_next, &l_a, &l_a_next);
				l_b = l_b_next = BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v_b, 0);
			}

			if (l_a && l_a_next == NULL) l_a_next = l_a;
			if (l_a_next && l_a == NULL) l_a = l_a_next;
			if (l_b && l_b_next == NULL) l_b_next = l_b;
			if (l_b_next && l_b == NULL) l_b = l_b_next;
			f_example = l_a ? l_a->f : (l_b ? l_b->f : NULL);

			if (v_b != v_b_next) {
				BMVert *v_arr[4] = {v_a, v_b, v_b_next, v_a_next};
				if (BM_face_exists(v_arr, 4, &f) == false) {
					/* copy if loop data if its is missing on one ring */
					f = BM_face_create_verts(bm, v_arr, 4, NULL, BM_CREATE_NOP, true);

					l_iter = BM_FACE_FIRST_LOOP(f);
					if (l_b)      BM_elem_attrs_copy(bm, bm, l_b,      l_iter); l_iter = l_iter->next;
					if (l_b_next) BM_elem_attrs_copy(bm, bm, l_b_next, l_iter); l_iter = l_iter->next;
					if (l_a_next) BM_elem_attrs_copy(bm, bm, l_a_next, l_iter); l_iter = l_iter->next;
					if (l_a)      BM_elem_attrs_copy(bm, bm, l_a,      l_iter);
				}
			}
			else {
				BMVert *v_arr[3] = {v_a, v_b, v_a_next};
				if (BM_face_exists(v_arr, 3, &f) == false) {
					/* fan-fill a triangle */
					f = BM_face_create_verts(bm, v_arr, 3, NULL, BM_CREATE_NOP, true);

					l_iter = BM_FACE_FIRST_LOOP(f);
					if (l_b)      BM_elem_attrs_copy(bm, bm, l_b,      l_iter); l_iter = l_iter->next;
					if (l_a_next) BM_elem_attrs_copy(bm, bm, l_a_next, l_iter); l_iter = l_iter->next;
					if (l_a)      BM_elem_attrs_copy(bm, bm, l_a,      l_iter);
				}
			}

			if (f_example && (f_example != f)) {
				BM_elem_attrs_copy(bm, bm, f_example, f);
			}
			BMO_elem_flag_enable(bm, f, FACE_OUT);
			BM_elem_flag_enable(f, BM_ELEM_TAG);

			/* tag all edges of the face, untag the loop edges after */
			if (use_edgeout) {
				bm_face_edges_tag_out(bm, f);
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
		/* when we have to bridge between different sized edge-loops,
		 * be clever and post-process for best results */


		/* triangulate inline */
		BMO_op_initf(bm, &op_sub, 0,
		             "triangulate faces=%hf",
		             BM_ELEM_TAG, true);
		/* calc normals for input faces before executing */
		{
			BMOIter siter;
			BMFace *f;
			BMO_ITER (f, &siter, op_sub.slots_in, "faces", BM_FACE) {
				BM_face_normal_update(f);
			}
		}
		BMO_op_exec(bm, &op_sub);
		BMO_slot_buffer_flag_enable(bm, op_sub.slots_out, "faces.out", BM_FACE, FACE_OUT);
		BMO_slot_buffer_hflag_enable(bm, op_sub.slots_out, "faces.out", BM_FACE, BM_ELEM_TAG, false);
		BMO_op_finish(bm, &op_sub);


		/* tag verts on each side so we can restrict rotation of edges to verts on the same side */
		for (i = 0; i < 2; i++) {
			LinkData *el;
			for (el = BM_edgeloop_verts_get(estore_pair[i])->first; el; el = el->next) {
				BM_elem_flag_set((BMVert *)el->data, BM_ELEM_TAG, i);
			}
		}


		BMO_op_initf(bm, &op_sub, 0,
		             "beautify_fill faces=%hf edges=ae use_restrict_tag=%b method=%i",
		             BM_ELEM_TAG, true, 1);

		if (use_edgeout) {
			BMOIter siter;
			BMFace *f;
			BMO_ITER (f, &siter, op_sub.slots_in, "faces", BM_FACE) {
				BMO_elem_flag_enable(bm, f, FACE_OUT);
				bm_face_edges_tag_out(bm, f);
			}
		}

		BMO_op_exec(bm, &op_sub);
		/* there may also be tagged faces that didnt rotate, mark input */

		if (use_edgeout) {
			BMOIter siter;
			BMFace *f;
			BMO_ITER (f, &siter, op_sub.slots_out, "geom.out", BM_FACE) {
				BMO_elem_flag_enable(bm, f, FACE_OUT);
				bm_face_edges_tag_out(bm, f);
			}
		}
		else {
			BMO_slot_buffer_flag_enable(bm, op_sub.slots_out, "geom.out", BM_FACE, FACE_OUT);
		}

		BMO_op_finish(bm, &op_sub);
	}

	if (use_edgeout && use_merge == false) {
		/* we've enabled all face edges above, now disable all loop edges */
		struct BMEdgeLoopStore *estore_pair[2] = {el_store_a, el_store_b};
		int i;
		for (i = 0; i < 2; i++) {
			LinkData *el;
			for (el = BM_edgeloop_verts_get(estore_pair[i])->first; el; el = el->next) {
				LinkData *el_next = BM_EDGELINK_NEXT(estore_pair[i], el);
				if (el_next) {
					if (el->data != el_next->data) {
						BMEdge *e = BM_edge_exists(el->data, el_next->data);
						BMO_elem_flag_disable(bm, e, EDGE_OUT);
					}
				}
			}
		}
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
	const bool  use_pairs    = BMO_slot_bool_get(op->slots_in,  "use_pairs");
	const bool  use_merge    = BMO_slot_bool_get(op->slots_in,  "use_merge");
	const float merge_factor = BMO_slot_float_get(op->slots_in, "merge_factor");
	const bool  use_cyclic   = BMO_slot_bool_get(op->slots_in,  "use_cyclic") && (use_merge == false);
	const int   twist_offset = BMO_slot_int_get(op->slots_in,   "twist_offset");
	int count;
	bool changed = false;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

	count = BM_mesh_edgeloops_find(bm, &eloops, bm_edge_test_cb, bm);

	BM_mesh_edgeloops_calc_center(bm, &eloops);

	if (count < 2) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Select at least two edge loops");
		goto cleanup;
	}

	if (use_pairs && (count % 2)) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Select an even number of loops to bridge pairs");
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
		if (use_pairs) {
			BM_mesh_edgeloops_calc_normal(bm, &eloops);
		}
		BM_mesh_edgeloops_calc_order(bm, &eloops, use_pairs);
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
		                 use_merge, merge_factor, twist_offset);
		if (use_pairs) {
			el_store = el_store->next;
		}
		changed = true;
	}

cleanup:
	BM_mesh_edgeloops_free(&eloops);

	if (changed) {
		if (use_merge == false) {
			BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
			BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);
		}
	}
}
