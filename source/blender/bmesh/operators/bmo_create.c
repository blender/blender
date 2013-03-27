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

/** \file blender/bmesh/operators/bmo_create.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define ELE_NEW		1
#define ELE_OUT		2

/* This is what runs when pressing the F key
 * doing the best thing here isn't always easy create vs dissolve, its nice to support
 * but it it _really_ gives issues we might have to not call dissolve. - campbell
 */
void bmo_contextual_create_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op2;
	BMOIter oiter;
	BMIter iter;
	BMHeader *h;
	BMVert *v, *verts[4];
	BMEdge *e;
	BMFace *f;
	int totv = 0, tote = 0, totf = 0, amount;
	const short mat_nr     = BMO_slot_int_get(op->slots_in,  "mat_nr");
	const bool use_smooth  = BMO_slot_bool_get(op->slots_in, "use_smooth");

	/* count number of each element type we were passe */
	BMO_ITER (h, &oiter, op->slots_in, "geom", BM_VERT | BM_EDGE | BM_FACE) {
		switch (h->htype) {
			case BM_VERT: totv++; break;
			case BM_EDGE: tote++; break;
			case BM_FACE: totf++; break;
		}

		BMO_elem_flag_enable(bm, (BMElemF *)h, ELE_NEW);
	}
	
	/* --- Support for Special Case ---
	 * where there is a contiguous edge ring with one isolated vertex.
	 *
	 * This example shows 2 edges created from 3 verts
	 * with 1 free standing vertex. Dotted lines denote the 2 edges that are created.
	 *
	 * note that this works for any sided shape.
	 *
	 * +--------+
	 * |        .
	 * |        .
	 * |        .
	 * |        .
	 * +........+ <-- starts out free standing.
	 *
	 */

	/* Here we check for consistency and create 2 edges */
	if (totf == 0 && totv >= 4 && totv == tote + 2) {
		/* find a free standing vertex and 2 endpoint verts */
		BMVert *v_free = NULL, *v_a = NULL, *v_b = NULL;
		bool ok = true;


		BMO_ITER (v, &oiter, op->slots_in, "geom", BM_VERT) {
			/* count how many flagged edges this vertex uses */
			int tot_edges = 0;
			BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
				if (BMO_elem_flag_test(bm, e, ELE_NEW)) {
					tot_edges++;
					if (tot_edges > 2) {
						break;
					}
				}
			}

			if (tot_edges == 0) {
				/* only accept 1 free vert */
				if (v_free == NULL)  v_free = v;
				else                 ok = false;  /* only ever want one of these */
			}
			else if (tot_edges == 1) {
				if (v_a == NULL)       v_a = v;
				else if (v_b == NULL)  v_b = v;
				else                   ok = false;  /* only ever want 2 of these */
			}
			else if (tot_edges == 2) {
				/* do nothing, regular case */
			}
			else {
				ok = false; /* if a vertex has 3+ edge users then cancel - this is only simple cases */
			}

			if (ok == false) {
				break;
			}
		}

		if (ok == true && v_free && v_a && v_b) {
			e = BM_edge_create(bm, v_free, v_a, NULL, BM_CREATE_NO_DOUBLE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);

			e = BM_edge_create(bm, v_free, v_b, NULL, BM_CREATE_NO_DOUBLE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);
			tote += 2;
		}
	}
	/* --- end special case support, continue as normal --- */

	/* call edgenet create */
	/* call edgenet prepare op so additional face creation cases wore */
	BMO_op_initf(bm, &op2, op->flag, "edgenet_prepare edges=%fe", ELE_NEW);
	BMO_op_exec(bm, &op2);
	BMO_slot_buffer_flag_enable(bm, op2.slots_out, "edges.out", BM_EDGE, ELE_NEW);
	BMO_op_finish(bm, &op2);

	BMO_op_initf(bm, &op2, op->flag,
	             "edgenet_fill edges=%fe use_fill_check=%b mat_nr=%i use_smooth=%b",
	             ELE_NEW, true, mat_nr, use_smooth);

	BMO_op_exec(bm, &op2);

	/* return if edge net create did something */
	if (BMO_slot_buffer_count(op2.slots_out, "faces.out")) {
		BMO_slot_copy(&op2, slots_out, "faces.out",
		              op,   slots_out, "faces.out");
		BMO_op_finish(bm, &op2);
		return;
	}

	BMO_op_finish(bm, &op2);
	
	/* now call dissolve face */
	BMO_op_initf(bm, &op2, op->flag, "dissolve_faces faces=%ff", ELE_NEW);
	BMO_op_exec(bm, &op2);
	
	/* if we dissolved anything, then return */
	if (BMO_slot_buffer_count(op2.slots_out, "region.out")) {
		BMO_slot_copy(&op2, slots_out, "region.out",
		              op,   slots_out,  "faces.out");
		BMO_op_finish(bm, &op2);
		return;
	}

	BMO_op_finish(bm, &op2);

	/* now, count how many verts we have */
	amount = 0;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, ELE_NEW)) {
			verts[amount] = v;
			if (amount == 3) {
				break;
			}
			amount++;

		}
	}

	if (amount == 2) {
		/* create edge */
		e = BM_edge_create(bm, verts[0], verts[1], NULL, BM_CREATE_NO_DOUBLE);
		BMO_elem_flag_enable(bm, e, ELE_OUT);
		tote += 1;
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, ELE_OUT);
	}
	else if (0) { /* nice feature but perhaps it should be a different tool? */

		/* tricky feature for making a line/edge from selection history...
		 *
		 * Rather then do nothing, when 5+ verts are selected, check if they are in our history,
		 * when this is so, we can make edges from them, but _not_ a face,
		 * if it is the intention to make a face the user can just hit F again since there will be edges next
		 * time around.
		 *
		 * if all history verts have ELE_NEW flagged and the total number of history verts == totv,
		 * then we know the history contains all verts here and we can continue...
		 */

		BMEditSelection *ese;
		int tot_ese_v = 0;

		for (ese = bm->selected.first; ese; ese = ese->next) {
			if (ese->htype == BM_VERT) {
				if (BMO_elem_flag_test(bm, (BMElemF *)ese->ele, ELE_NEW)) {
					tot_ese_v++;
				}
				else {
					/* unflagged vert means we are not in sync */
					tot_ese_v = -1;
					break;
				}
			}
		}

		if (tot_ese_v == totv) {
			BMVert *v_prev = NULL;
			/* yes, all select-history verts are accounted for, now make edges */

			for (ese = bm->selected.first; ese; ese = ese->next) {
				if (ese->htype == BM_VERT) {
					v = (BMVert *)ese->ele;
					if (v_prev) {
						e = BM_edge_create(bm, v, v_prev, NULL, BM_CREATE_NO_DOUBLE);
						BMO_elem_flag_enable(bm, e, ELE_OUT);
					}
					v_prev = v;
				}
			}
		}
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, ELE_OUT);
		/* done creating edges */
	}
	else if (amount > 2) {
		/* TODO, all these verts may be connected by edges.
		 * we should check on this before assuming they are a random set of verts */

		BMVert **vert_arr = MEM_mallocN(sizeof(BMVert **) * totv, __func__);
		int i = 0;

		BMO_ITER (v, &oiter, op->slots_in, "geom", BM_VERT) {
			vert_arr[i] = v;
			i++;
		}

		f = BM_face_create_ngon_vcloud(bm, vert_arr, totv, BM_CREATE_NO_DOUBLE);

		if (f) {
			BMO_elem_flag_enable(bm, f, ELE_OUT);
			f->mat_nr = mat_nr;
			if (use_smooth) {
				BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
			}
			BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, ELE_OUT);
		}

		MEM_freeN(vert_arr);
	}

	(void)tote;
}
