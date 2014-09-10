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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_rip_edge.c
 *  \ingroup edmesh
 *
 * based on mouse cursor position, split of vertices along the closest edge.
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "bmesh.h"

#include "mesh_intern.h"  /* own include */

/* uses total number of selected edges around a vertex to choose how to extend */
#define USE_TRICKY_EXTEND

static int edbm_rip_edge_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMIter viter;
	BMVert *v;
	const float mval_fl[2] = {UNPACK2(event->mval)};
	float cent_sco[2];
	int cent_tot;
	bool changed = false;

	/* mouse direction to view center */
	float mval_dir[2];

	float projectMat[4][4];

	if (bm->totvertsel == 0)
		return OPERATOR_CANCELLED;

	ED_view3d_ob_project_mat_get(rv3d, obedit, projectMat);

	zero_v2(cent_sco);
	cent_tot = 0;

	/* clear tags and calc screen center */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);

		if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
			float v_sco[2];
			ED_view3d_project_float_v2_m4(ar, v->co, v_sco, projectMat);

			add_v2_v2(cent_sco, v_sco);
			cent_tot += 1;
		}
	}
	mul_v2_fl(cent_sco, 1.0f / (float)cent_tot);

	/* not essential, but gives more expected results with edge selection */
	if (bm->totedgesel) {
		/* angle against center can give odd result,
		 * try re-position the center to the closest edge */
		BMIter eiter;
		BMEdge *e;
		float dist_sq_best = len_squared_v2v2(cent_sco, mval_fl);

		BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
				float e_sco[2][2];
				float cent_sco_test[2];
				float dist_sq_test;

				ED_view3d_project_float_v2_m4(ar, e->v1->co, e_sco[0], projectMat);
				ED_view3d_project_float_v2_m4(ar, e->v2->co, e_sco[1], projectMat);

				closest_to_line_segment_v2(cent_sco_test, mval_fl, e_sco[0], e_sco[1]);
				dist_sq_test = len_squared_v2v2(cent_sco_test, mval_fl);
				if (dist_sq_test < dist_sq_best) {
					dist_sq_best = dist_sq_test;

					/* we have a new screen center */
					copy_v2_v2(cent_sco, cent_sco_test);
				}
			}
		}
	}

	sub_v2_v2v2(mval_dir, mval_fl, cent_sco);
	normalize_v2(mval_dir);

	/* operate on selected verts */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		BMEdge *e;
		float v_sco[2];

		if (BM_elem_flag_test(v, BM_ELEM_SELECT) &&
		    BM_elem_flag_test(v, BM_ELEM_TAG) == false)
		{
			/* Rules for */
			float angle_best = FLT_MAX;
			BMEdge *e_best = NULL;

#ifdef USE_TRICKY_EXTEND
			/* first check if we can select the edge to split based on selection-only */
			int tot_sel = 0, tot = 0;

			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
						e_best = e;
						tot_sel += 1;
					}
					tot += 1;
				}
			}
			if (tot_sel != 1) {
				e_best = NULL;
			}

			/* only one edge selected, operate on that */
			if (e_best) {
				goto found_edge;
			}
			/* none selected, fall through and find one */
			else if (tot_sel == 0) {
				/* pass */
			}
			/* selection not 0 or 1, do nothing */
			else {
				goto found_edge;
			}
#endif
			ED_view3d_project_float_v2_m4(ar, v->co, v_sco, projectMat);

			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
					BMVert *v_other = BM_edge_other_vert(e, v);
					float v_other_sco[2];
					float angle_test;

					ED_view3d_project_float_v2_m4(ar, v_other->co, v_other_sco, projectMat);

					/* avoid comparing with view-axis aligned edges (less then a pixel) */
					if (len_squared_v2v2(v_sco, v_other_sco) > 1.0f) {
						float v_dir[2];

						sub_v2_v2v2(v_dir, v_other_sco, v_sco);
						normalize_v2(v_dir);

						angle_test = angle_normalized_v2v2(mval_dir, v_dir);

						if (angle_test < angle_best) {
							angle_best = angle_test;
							e_best = e;
						}
					}
				}
			}

#ifdef USE_TRICKY_EXTEND
found_edge:
#endif
			if (e_best) {
				const bool e_select = BM_elem_flag_test_bool(e_best, BM_ELEM_SELECT);
				BMVert *v_new;
				BMEdge *e_new;

				v_new = BM_edge_split(bm, e_best, v, &e_new, 0.0f);

				BM_vert_select_set(bm, v, false);
				BM_edge_select_set(bm, e_new, false);

				BM_vert_select_set(bm, v_new, true);
				if (e_select) {
					BM_edge_select_set(bm, e_best, true);
				}
				BM_elem_flag_enable(v_new, BM_ELEM_TAG);  /* prevent further splitting */

				changed = true;
			}
		}
	}

	if (changed) {
		BM_select_history_clear(bm);

		BM_mesh_select_mode_flush(bm);

		EDBM_update_generic(em, true, true);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}


void MESH_OT_rip_edge(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extend Vertices";
	ot->idname = "MESH_OT_rip_edge";
	ot->description = "Extend vertices along the edge closest to the cursor";

	/* api callbacks */
	ot->invoke = edbm_rip_edge_invoke;
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);
}
