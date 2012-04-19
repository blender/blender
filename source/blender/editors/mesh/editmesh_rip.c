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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_rip.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "mesh_intern.h"

/* helper to find edge for edge_rip */
static float edbm_rip_rip_edgedist(ARegion *ar, float mat[][4], float *co1, float *co2, const float mvalf[2])
{
	float vec1[3], vec2[3];

	ED_view3d_project_float_v2(ar, co1, vec1, mat);
	ED_view3d_project_float_v2(ar, co2, vec2, mat);

	return dist_to_line_segment_v2(mvalf, vec1, vec2);
}

static float edbm_rip_edge_side_measure(BMEdge *e,
                                        ARegion *ar,
                                        float projectMat[4][4], const float fmval[2])
{
	float cent[3] = {0, 0, 0}, mid[3];

	float vec[2];
	float fmval_tweak[2];
	float e_v1_co[2], e_v2_co[2];
	float score;

	BMVert *v1_other;
	BMVert *v2_other;

	/* method for calculating distance:
	 *
	 * for each edge: calculate face center, then made a vector
	 * from edge midpoint to face center.  offset edge midpoint
	 * by a small amount along this vector. */

	/* rather then the face center, get the middle of
	 * both edge verts connected to this one */
	v1_other = BM_face_other_vert_loop(e->l->f, e->v2, e->v1)->v;
	v2_other = BM_face_other_vert_loop(e->l->f, e->v1, e->v2)->v;
	mid_v3_v3v3(cent, v1_other->co, v2_other->co);
	mid_v3_v3v3(mid, e->v1->co, e->v2->co);

	ED_view3d_project_float_v2(ar, cent, cent, projectMat);
	ED_view3d_project_float_v2(ar, mid, mid, projectMat);

	ED_view3d_project_float_v2(ar, e->v1->co, e_v1_co, projectMat);
	ED_view3d_project_float_v2(ar, e->v2->co, e_v2_co, projectMat);

	sub_v2_v2v2(vec, cent, mid);
	normalize_v2(vec);
	mul_v2_fl(vec, 0.01f);

	/* rather then adding to both verts, subtract from the mouse */
	sub_v2_v2v2(fmval_tweak, fmval, vec);

	score = len_v2v2(e_v1_co, e_v2_co);

	if (dist_to_line_segment_v2(fmval_tweak, e_v1_co, e_v2_co) >
		dist_to_line_segment_v2(fmval,       e_v1_co, e_v2_co))
	{
		return  score;
	}
	else {
		return -score;
	}
}


/* - Advanced selection handling 'ripsel' functions ----- */

/**
 * How rip selection works
 *
 * Firstly - rip is basically edge split with side-selection & grab.
 * Things would be much more simple if we didn't have to worry about side selection
 *
 * The method used for checking the side of selection is as follows...
 * - First tag all rip-able edges.
 * - Build a contiguous edge list by looping over tagged edges and following each ones tagged siblings in both
 *   directions.
 *   - The loops are not stored in an array, Instead both loops on either side of each edge has its index values set
 *     to count down from the last edge, this way, once we have the 'last' edge its very easy to walk down the
 *     connected edge loops.
 *     The reason for using loops like this is because when the edges are split we don't which face user gets the newly
 *     created edge (its as good as random so we cant assume new edges will be on once side).
 *     After splittingm, its very simple to walk along boundary loops since each only has one edge from a single side.
 * - The end loop pairs are stored in an array however to support multiple edge-selection-islands, so you can rip
 *   multiple selections at once.
 * - * Execute the split *
 * - For each #EdgeLoopPair walk down both sides of the split using the loops and measure which is facing the mouse.
 * - Deselect the edge loop facing away.
 *
 * Limitation!
 * This currently works very poorly with intersecting edge islands (verts with more then 2 tagged edges)
 * This is nice to but for now not essential.
 *
 * - campbell.
 */


#define IS_VISIT_POSSIBLE(e)   (BM_edge_is_manifold(e) && BM_elem_flag_test(e, BM_ELEM_TAG))
#define IS_VISIT_DONE(e) ((e)->l && (BM_elem_index_get((e)->l) != INVALID_UID))
#define INVALID_UID INT_MIN

/* mark, assign uid and step */
static BMEdge *edbm_ripsel_edge_mark_step(BMesh *bm, BMVert *v, const int uid)
{
	BMIter iter;
	BMEdge *e;
	BM_ITER (e, &iter, bm, BM_EDGES_OF_VERT, v) {
		if (IS_VISIT_POSSIBLE(e) && !IS_VISIT_DONE(e)) {
			BMLoop *l_a, *l_b;

			BM_edge_loop_pair(e, &l_a, &l_b); /* no need to check, we know this will be true */

			/* so (IS_VISIT_DONE == TRUE) */
			BM_elem_index_set(l_a, uid);
			BM_elem_index_set(l_b, uid);

			return e;
		}
	}
	return NULL;
}

typedef struct EdgeLoopPair {
	BMLoop *l_a;
	BMLoop *l_b;
} EdgeLoopPair;

static EdgeLoopPair *edbm_ripsel_looptag_helper(BMesh *bm)
{
	BMIter fiter;
	BMIter liter;

	BMFace *f;
	BMLoop *l;

	int uid_start;
	int uid_end;
	int uid = bm->totedge; /* can start anywhere */

	EdgeLoopPair *eloop_pairs = NULL;
	BLI_array_declare(eloop_pairs);
	EdgeLoopPair *lp;

	/* initialize loops with dummy invalid index values */
	BM_ITER (f, &fiter, bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER (l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_elem_index_set(l, INVALID_UID);
		}
	}

	/* set contiguous loops ordered 'uid' values for walking after split */
	while (TRUE) {
		int tot = 0;
		BMIter eiter;
		BMEdge *e_step;
		BMVert *v_step;
		BMEdge *e;
		BMEdge *e_first;
		BMEdge *e_last;

		e_first = NULL;
		BM_ITER (e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {
			if (IS_VISIT_POSSIBLE(e) && !IS_VISIT_DONE(e)) {
				e_first = e;
				break;
			}
		}

		if (e_first == NULL) {
			break;
		}

		e_first = e;

		/* initialize  */
		v_step = e_first->v1;

		uid_start = uid;
		while ((e = edbm_ripsel_edge_mark_step(bm, v_step, uid))) {
			BM_elem_flag_disable(e, BM_ELEM_SMOOTH);
			v_step = BM_edge_other_vert((e_step = e), v_step);
			uid++; /* only different line */
			tot++;
		}

		/* this edges loops have the highest uid's, store this to walk down later */
		e_last = e_step;

		/* always store the highest 'uid' edge for the stride */
		uid_end = uid - 1;
		uid = uid_start - 1;

		/* initialize */
		v_step = e_first->v1;

		while ((e = edbm_ripsel_edge_mark_step(bm, v_step, uid))) {
			BM_elem_flag_disable(e, BM_ELEM_SMOOTH);
			v_step = BM_edge_other_vert((e_step = e), v_step);
			uid--; /* only different line */
			tot++;
		}

		/* stride far enough not to _ever_ overlap range */
		uid_start = uid;
		uid = uid_end + bm->totedge;

		BLI_array_growone(eloop_pairs);
		lp = &eloop_pairs[BLI_array_count(eloop_pairs) - 1];
		BM_edge_loop_pair(e_last, &lp->l_a, &lp->l_b); /* no need to check, we know this will be true */


		BLI_assert(tot == uid_end - uid_start);

#if 0
		printf("%s: found contiguous edge loop of (%d)\n", __func__, uid_end - uid_start);
#endif

	}

	/* null terminate */
	BLI_array_growone(eloop_pairs);
	lp = &eloop_pairs[BLI_array_count(eloop_pairs) - 1];
	lp->l_a = lp->l_b = NULL;

	return eloop_pairs;
}


/* - De-Select the worst rip-edge side -------------------------------- */


static BMEdge *edbm_ripsel_edge_uid_step(BMesh *bm, BMEdge *e_orig, BMVert **v_prev)
{
	BMIter eiter;
	BMEdge *e;
	BMVert *v = BM_edge_other_vert(e_orig, *v_prev);
	const int uid_cmp = BM_elem_index_get(e_orig->l) - 1;

	BM_ITER (e, &eiter, bm, BM_EDGES_OF_VERT, v) {
		if (BM_elem_index_get(e->l) == uid_cmp) {
			*v_prev = v;
			return e;
		}
	}
	return NULL;
}

static BMVert *edbm_ripsel_edloop_pair_start_vert(BMesh *bm, BMEdge *e)
{
	/* try step in a direction, if it fails we know do go the other way */
	BMVert *v_test = e->v1;
	return (edbm_ripsel_edge_uid_step(bm, e, &v_test)) ? e->v1 : e->v2;
}

static void edbm_ripsel_deselect_helper(BMesh *bm, EdgeLoopPair *eloop_pairs,
                                        ARegion *ar, float projectMat[4][4], float fmval[2])
{
	EdgeLoopPair *lp;

	for (lp = eloop_pairs; lp->l_a; lp++) {
		BMEdge *e;
		BMVert *v_prev;

		float score_a = 0.0f;
		float score_b = 0.0f;

		e = lp->l_a->e;
		v_prev = edbm_ripsel_edloop_pair_start_vert(bm, e);
		for (; e; e = edbm_ripsel_edge_uid_step(bm, e, &v_prev)) {
			score_a += edbm_rip_edge_side_measure(e, ar, projectMat, fmval);
		}
		e = lp->l_b->e;
		v_prev = edbm_ripsel_edloop_pair_start_vert(bm, e);
		for (; e; e = edbm_ripsel_edge_uid_step(bm, e, &v_prev)) {
			score_b += edbm_rip_edge_side_measure(e, ar, projectMat, fmval);
		}

		e = (score_a > score_b) ? lp->l_a->e : lp->l_b->e;
		v_prev = edbm_ripsel_edloop_pair_start_vert(bm, e);
		for (; e; e = edbm_ripsel_edge_uid_step(bm, e, &v_prev)) {
			BM_elem_select_set(bm, e, FALSE);
		}
	}
}
/* --- end 'ripsel' selection handling code --- */



/* based on mouse cursor position, it defines how is being ripped */
static int edbm_rip_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	BMEditMesh *em = BMEdit_FromObject(obedit);
	BMesh *bm = em->bm;
	BMOperator bmop;
	BMIter iter, eiter, liter;
	BMLoop *l;
	BMEdge *e, *e2;
	BMVert *v, *ripvert = NULL;
	int i, singlesel = FALSE;
	float projectMat[4][4], fmval[3] = {event->mval[0], event->mval[1]};
	float dist = FLT_MAX;
	float d;
	const int totedge_orig = bm->totedge;

	EdgeLoopPair *eloop_pairs;

	/* note on selection:
	 * When calling edge split we operate on tagged edges rather then selected
	 * this is important because the edges to operate on are extended by one,
	 * but the selection is left alone.
	 *
	 * After calling edge split - the duplicated edges have the same selection state as the
	 * original, so all we do is de-select the far side from the mouse and we have a
	 * useful selection for grabbing.
	 */

	ED_view3d_ob_project_mat_get(rv3d, obedit, projectMat);

	/* BM_ELEM_SELECT --> BM_ELEM_TAG */
	BM_ITER (e, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_flag_set(e, BM_ELEM_TAG, BM_elem_flag_test(e, BM_ELEM_SELECT));
	}

	/* handle case of one vert selected.  identify
	 * closest edge around that vert to mouse cursor,
	 * then rip two adjacent edges in the vert fan. */
	if (bm->totvertsel == 1 && bm->totedgesel == 0 && bm->totfacesel == 0) {
		BMEditSelection ese;
		int totboundary_edge = 0;
		singlesel = TRUE;

		/* find selected vert - same some time and check history first */
		if (EDBM_editselection_active_get(em, &ese) && ese.htype == BM_VERT) {
			v = (BMVert *)ese.ele;
		}
		else {
			ese.ele = NULL;

			BM_ITER (v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
				if (BM_elem_flag_test(v, BM_ELEM_SELECT))
					break;
			}
		}

		/* this should be impossible, but sanity checks are a good thing */
		if (!v)
			return OPERATOR_CANCELLED;

		e2 = NULL;

		if (v->e) {
			/* find closest edge to mouse cursor */
			BM_ITER (e, &iter, bm, BM_EDGES_OF_VERT, v) {
				int is_boundary = BM_edge_is_boundary(e);
				/* consider wire as boundary for this purpose,
				 * otherwise we can't a face away from a wire edge */
				totboundary_edge += (is_boundary != 0 || BM_edge_is_wire(e));
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					if (is_boundary == FALSE && BM_edge_is_manifold(e)) {
						d = edbm_rip_rip_edgedist(ar, projectMat, e->v1->co, e->v2->co, fmval);
						if (d < dist) {
							dist = d;
							e2 = e;
						}
					}
				}
			}

		}

		/* should we go ahead with edge rip or do we need to do special case, split off vertex?:
		 * split off vertex if...
		 * - we cant find an edge - this means we are ripping a faces vert that is connected to other
		 *   geometry only at the vertex.
		 * - the boundary edge total is greater then 2,
		 *   in this case edge split _can_ work but we get far nicer results if we use this special case. */
		if (totboundary_edge > 2) {
			BMVert **vout;
			int vout_len;

			BM_elem_select_set(bm, v, FALSE);
			bmesh_vert_separate(bm, v, &vout, &vout_len);

			if (vout_len < 2) {
				/* set selection back to avoid active-unselected vertex */
				BM_elem_select_set(bm, v, TRUE);
				/* should never happen */
				BKE_report(op->reports, RPT_ERROR, "Error ripping vertex from faces");
				return OPERATOR_CANCELLED;
			}
			else {
				int vi_best = 0;

				if (ese.ele) {
					EDBM_editselection_remove(em, &ese.ele->head);
				}

				dist = FLT_MAX;

				for (i = 0; i < vout_len; i++) {
					BM_ITER (l, &iter, bm, BM_LOOPS_OF_VERT, vout[i]) {
						if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
							float l_mid_co[3];
							BM_loop_face_tangent(l, l_mid_co);

							/* scale to average of surrounding edge size, only needs to be approx */
							mul_v3_fl(l_mid_co, (BM_edge_length_calc(l->e) + BM_edge_length_calc(l->prev->e)) / 2.0f);
							add_v3_v3(l_mid_co, v->co);

							d = edbm_rip_rip_edgedist(ar, projectMat, v->co, l_mid_co, fmval);

							if (d < dist) {
								dist = d;
								vi_best = i;
							}
						}
					}
				}

				/* select the vert from the best region */
				v = vout[vi_best];
				BM_elem_select_set(bm, v, TRUE);

				if (ese.ele) {
					EDBM_editselection_store(em, &v->head);
				}

				/* splice all others back together */
				if (vout_len > 2) {

					/* vout[0]  == best
					 * vout[1]  == glue
					 * vout[2+] == splice with glue
					 */
					if (vi_best != 0) {
						SWAP(BMVert *, vout[0], vout[vi_best]);
						vi_best = 0;
					}

					for (i = 2; i < vout_len; i++) {
						BM_vert_splice(bm, vout[i], vout[1]);
					}
				}

				MEM_freeN(vout);

				return OPERATOR_FINISHED;
			}
		}

		if (!e2) {
			BKE_report(op->reports, RPT_ERROR, "Selected vertex has no edge/face pairs attached");
			return OPERATOR_CANCELLED;
		}

		/* rip two adjacent edges */
		if (BM_edge_is_boundary(e2) || BM_vert_face_count(v) == 2) {
			l = e2->l;
			ripvert = BM_face_vert_separate(bm, l->f, v);

			BLI_assert(ripvert);
			if (!ripvert) {
				return OPERATOR_CANCELLED;
			}
		}
		else if (BM_edge_is_manifold(e2)) {
			l = e2->l;
			e = BM_face_other_edge_loop(l->f, e2, v)->e;
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			
			l = e2->l->radial_next;
			e = BM_face_other_edge_loop(l->f, e2, v)->e;
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		}

		dist = FLT_MAX;
	}
	else {
		/* expand edge selection */
		BM_ITER (v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
			e2 = NULL;
			i = 0;
			BM_ITER (e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				/* important to check selection rather then tag here
				 * else we get feedback loop */
				if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
					e2 = e;
					i++;
				}
			}
			
			if (i == 1 && e2->l) {
				l = BM_face_other_edge_loop(e2->l->f, e2, v);
				l = l->radial_next;
				l = BM_face_other_edge_loop(l->f, l->e, v);

				if (l) {
					BM_elem_flag_enable(l->e, BM_ELEM_TAG);
				}
			}
		}
	}

	eloop_pairs = edbm_ripsel_looptag_helper(bm);

	if (!EDBM_op_init(em, &bmop, op, "edgesplit edges=%he verts=%hv use_verts=%b",
	                  BM_ELEM_TAG, BM_ELEM_SELECT, TRUE)) {
		MEM_freeN(eloop_pairs);
		return OPERATOR_CANCELLED;
	}
	
	BMO_op_exec(bm, &bmop);

	if (totedge_orig == bm->totedge) {
		EDBM_op_finish(em, &bmop, op, TRUE);

		BKE_report(op->reports, RPT_ERROR, "No edges could be ripped");
		MEM_freeN(eloop_pairs);
		return OPERATOR_CANCELLED;
	}

#if 1
	edbm_ripsel_deselect_helper(bm, eloop_pairs,
	                            ar, projectMat, fmval);
	MEM_freeN(eloop_pairs);
#else
	{
		/* simple per edge selection check, saves a lot of code and is almost good enough */
		BMOIter siter;
		BMO_ITER (e, &siter, bm, &bmop, "edgeout", BM_EDGE) {
			if (edbm_rip_edge_side_measure(e, ar, projectMat, fmval) > 0.0f) {
				BM_elem_select_set(bm, e, FALSE);
			}
		}
	}
#endif

	if (singlesel) {
		BMVert *v_best = NULL;
		float l_prev_co[3], l_next_co[3], l_corner_co[3];
		float scale;

		/* not good enough! - original vert may not be attached to the closest edge */
#if 0
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BM_elem_select_set(bm, ripvert, TRUE);
#else

		dist = FLT_MAX;
		BM_ITER (v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
				/* disable by default, re-enable winner at end */
				BM_elem_select_set(bm, v, FALSE);

				BM_ITER (l, &liter, bm, BM_LOOPS_OF_VERT, v) {
					/* calculate a point in the face, rather then calculate the middle,
					 * make a vector pointing between the 2 edges attached to this loop */
					sub_v3_v3v3(l_prev_co, l->prev->v->co, l->v->co);
					sub_v3_v3v3(l_next_co, l->next->v->co, l->v->co);

					scale = normalize_v3(l_prev_co) + normalize_v3(l_next_co);
					mul_v3_fl(l_prev_co, scale);
					mul_v3_fl(l_next_co, scale);

					add_v3_v3v3(l_corner_co, l_prev_co, l_next_co);
					add_v3_v3(l_corner_co, l->v->co);

					d = edbm_rip_rip_edgedist(ar, projectMat, l->v->co, l_corner_co, fmval);
					if (d < dist) {
						v_best = v;
						dist = d;
					}
				}
			}
		}

		if (v_best) {
			BM_elem_select_set(bm, v_best, TRUE);
		}
#endif
	}

	EDBM_selectmode_flush(em);

	BLI_assert(singlesel ? (bm->totvertsel > 0) : (bm->totedgesel > 0));

	if (!EDBM_op_finish(em, &bmop, op, TRUE)) {
		return OPERATOR_CANCELLED;
	}

	if (bm->totvertsel == 0) {
		return OPERATOR_CANCELLED;
	}

	EDBM_update_generic(C, em, TRUE);

	return OPERATOR_FINISHED;
}

void MESH_OT_rip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Rip";
	ot->idname = "MESH_OT_rip";
	ot->description = "Disconnect vertex or edges from connected geometry";

	/* api callbacks */
	ot->invoke = edbm_rip_invoke;
	ot->poll = EM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_PROPORTIONAL);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}
