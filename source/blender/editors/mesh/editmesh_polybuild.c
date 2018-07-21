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

/** \file blender/editors/mesh/editmesh_polybuild.c
 *  \ingroup edmesh
 *
 * Tools to implement polygon building tool,
 * an experimental tool for quickly constructing/manipulating faces.
 */

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "bmesh.h"

#include "mesh_intern.h"  /* own include */

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void edbm_selectmode_ensure(Scene *scene, BMEditMesh *em, short selectmode)
{
	if ((scene->toolsettings->selectmode & selectmode) == 0) {
		scene->toolsettings->selectmode |= selectmode;
		em->selectmode = scene->toolsettings->selectmode;
		EDBM_selectmode_set(em);
	}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face At Cursor
 * \{ */

static int edbm_polybuild_face_at_cursor_invoke(
        bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ViewContext vc;
	float center[3];
	bool changed = false;

	em_setup_viewcontext(C, &vc);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMElem *ele_act = BM_mesh_active_elem_get(bm);

	invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

	edbm_selectmode_ensure(vc.scene, vc.em, SCE_SELECT_VERTEX);

	if (ele_act == NULL || ele_act->head.htype == BM_FACE) {
		/* Just add vert */
		copy_v3_v3(center, ED_view3d_cursor3d_get(vc.scene, vc.v3d)->location);
		mul_v3_m4v3(center, vc.obedit->obmat, center);
		ED_view3d_win_to_3d_int(vc.v3d, vc.ar, center, event->mval, center);
		mul_m4_v3(vc.obedit->imat, center);

		BMVert *v_new = BM_vert_create(bm, center, NULL, BM_CREATE_NOP);
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BM_vert_select_set(bm, v_new, true);
		changed = true;
	}
	else if (ele_act->head.htype == BM_EDGE) {
		BMEdge *e_act = (BMEdge *)ele_act;
		BMFace *f_reference = e_act->l ? e_act->l->f : NULL;

		mid_v3_v3v3(center, e_act->v1->co, e_act->v2->co);
		mul_m4_v3(vc.obedit->obmat, center);
		ED_view3d_win_to_3d_int(vc.v3d, vc.ar, center, event->mval, center);
		mul_m4_v3(vc.obedit->imat, center);

		BMVert *v_tri[3];
		v_tri[0] = e_act->v1;
		v_tri[1] = e_act->v2;
		v_tri[2] = BM_vert_create(bm, center, NULL, BM_CREATE_NOP);
		if (e_act->l && e_act->l->v == v_tri[0]) {
			SWAP(BMVert *, v_tri[0], v_tri[1]);
		}
		// BMFace *f_new =
		BM_face_create_verts(bm, v_tri, 3, f_reference, BM_CREATE_NOP, true);

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BM_vert_select_set(bm, v_tri[2], true);
		changed = true;
	}
	else if (ele_act->head.htype == BM_VERT) {
		BMVert *v_act = (BMVert *)ele_act;
		BMEdge *e_pair[2] = {NULL};

		if (v_act->e != NULL) {
			for (uint allow_wire = 0; allow_wire < 2 && (e_pair[1] == NULL); allow_wire++) {
				int i = 0;
				BMEdge *e_iter = v_act->e;
				do {
					if ((BM_elem_flag_test(e_iter, BM_ELEM_HIDDEN) == false) &&
					    (allow_wire ? BM_edge_is_wire(e_iter) : BM_edge_is_boundary(e_iter)))
					{
						if (i == 2) {
							e_pair[0] = e_pair[1] = NULL;
							break;
						}
						e_pair[i++] = e_iter;
					}
				} while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v_act)) != v_act->e);
			}
		}

		if (e_pair[1] != NULL) {
			/* Quad from edge pair. */
			if (BM_edge_calc_length_squared(e_pair[0]) <
			    BM_edge_calc_length_squared(e_pair[1]))
			{
				SWAP(BMEdge *, e_pair[0], e_pair[1]);
			}

			BMFace *f_reference = e_pair[0]->l ? e_pair[0]->l->f : NULL;

			mul_v3_m4v3(center, vc.obedit->obmat, v_act->co);
			ED_view3d_win_to_3d_int(vc.v3d, vc.ar, center, event->mval, center);
			mul_m4_v3(vc.obedit->imat, center);

			BMVert *v_quad[4];
			v_quad[0] = v_act;
			v_quad[1] = BM_edge_other_vert(e_pair[0], v_act);
			v_quad[2] = BM_vert_create(bm, center, NULL, BM_CREATE_NOP);
			v_quad[3] = BM_edge_other_vert(e_pair[1], v_act);
			if (e_pair[0]->l && e_pair[0]->l->v == v_quad[0]) {
				SWAP(BMVert *, v_quad[1], v_quad[3]);
			}
			// BMFace *f_new =
			BM_face_create_verts(bm, v_quad, 4, f_reference, BM_CREATE_NOP, true);

			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BM_vert_select_set(bm, v_quad[2], true);
			changed = true;
		}
		else {
			/* Just add edge */
			mul_m4_v3(vc.obedit->obmat, center);
			ED_view3d_win_to_3d_int(vc.v3d, vc.ar, v_act->co, event->mval, center);
			mul_m4_v3(vc.obedit->imat, center);

			BMVert *v_new = BM_vert_create(bm, center, NULL, BM_CREATE_NOP);

			BM_edge_create(bm, v_act, v_new, NULL, BM_CREATE_NOP);

			BM_vert_select_set(bm, v_new, true);
		}
	}

	if (changed) {
		BM_select_history_clear(bm);

		EDBM_mesh_normals_update(em);
		EDBM_update_generic(em, true, true);

		WM_event_add_mousemove(C);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_polybuild_face_at_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Poly Build Face At Cursor";
	ot->idname = "MESH_OT_polybuild_face_at_cursor";
	ot->description = "";

	/* api callbacks */
	ot->invoke = edbm_polybuild_face_at_cursor_invoke;
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split At Cursor
 * \{ */

static int edbm_polybuild_split_at_cursor_invoke(
        bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ViewContext vc;
	float center[3];
	bool changed = false;

	em_setup_viewcontext(C, &vc);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

	edbm_selectmode_ensure(vc.scene, vc.em, SCE_SELECT_VERTEX);

	BMElem *ele_act = BM_mesh_active_elem_get(bm);

	if (ele_act == NULL || ele_act->head.hflag == BM_FACE) {
		return OPERATOR_PASS_THROUGH;
	}
	else if (ele_act->head.htype == BM_EDGE) {
		BMEdge *e_act = (BMEdge *)ele_act;
		mid_v3_v3v3(center, e_act->v1->co, e_act->v2->co);
		mul_m4_v3(vc.obedit->obmat, center);
		ED_view3d_win_to_3d_int(vc.v3d, vc.ar, center, event->mval, center);
		mul_m4_v3(vc.obedit->imat, center);

		const float fac = line_point_factor_v3(center, e_act->v1->co, e_act->v2->co);
		BMVert *v_new = BM_edge_split(bm, e_act, e_act->v1, NULL, CLAMPIS(fac, 0.0f, 1.0f));
		copy_v3_v3(v_new->co, center);

		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BM_vert_select_set(bm, v_new, true);
		changed = true;
	}
	else if (ele_act->head.htype == BM_VERT) {
		/* Just do nothing, allow dragging. */
		return OPERATOR_FINISHED;
	}

	if (changed) {
		BM_select_history_clear(bm);

		EDBM_mesh_normals_update(em);
		EDBM_update_generic(em, true, true);

		WM_event_add_mousemove(C);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_polybuild_split_at_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Poly Build Split At Cursor";
	ot->idname = "MESH_OT_polybuild_split_at_cursor";
	ot->description = "";

	/* api callbacks */
	ot->invoke = edbm_polybuild_split_at_cursor_invoke;
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* to give to transform */
	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR_DUMMY);
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name Dissolve At Cursor
 *
 * \{ */

static int edbm_polybuild_dissolve_at_cursor_invoke(
        bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	ViewContext vc;
	em_setup_viewcontext(C, &vc);
	bool changed = false;

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMVert *v_act = BM_mesh_active_vert_get(bm);
	BMEdge *e_act = BM_mesh_active_edge_get(bm);

	invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

	edbm_selectmode_ensure(vc.scene, vc.em, SCE_SELECT_VERTEX);


	if (e_act) {
		BMLoop *l_a, *l_b;
		if (BM_edge_loop_pair(e_act, &l_a, &l_b)) {
			BMFace *f_new = BM_faces_join_pair(bm, l_a, l_b, true);
			if (f_new) {
				changed = true;
			}
		}
	}
	else if (v_act) {
		if (BM_vert_is_edge_pair(v_act)) {
			BM_edge_collapse(
			        bm, v_act->e, v_act,
			        true, true);
		}
		else {
			/* too involved to do inline */
			if (!EDBM_op_callf(em, op,
			                   "dissolve_verts verts=%hv use_face_split=%b use_boundary_tear=%b",
			                   BM_ELEM_SELECT, false, true))
			{
				return OPERATOR_CANCELLED;
			}
		}
		changed = true;
	}

	if (changed) {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);

		BM_select_history_clear(bm);

		EDBM_mesh_normals_update(em);
		EDBM_update_generic(em, true, true);

		WM_event_add_mousemove(C);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_polybuild_dissolve_at_cursor(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Poly Build Dissolve At Cursor";
	ot->idname = "MESH_OT_polybuild_dissolve_at_cursor";
	ot->description = "";

	/* api callbacks */
	ot->invoke = edbm_polybuild_dissolve_at_cursor_invoke;
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cursor Gizmo
 *
 * \note This may need its own file, for now not.
 * \{ */

static BMElem *edbm_hover_preselect(
        bContext *C,
        const int mval[2],
        bool use_boundary)
{
	ViewContext vc;

	em_setup_viewcontext(C, &vc);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;

	invert_m4_m4(vc.obedit->imat, vc.obedit->obmat);
	ED_view3d_init_mats_rv3d(vc.obedit, vc.rv3d);

	const float mval_fl[2] = {UNPACK2(mval)};
	float ray_origin[3], ray_direction[3];

	BMElem *ele_best = NULL;

	if (ED_view3d_win_to_ray(
	        CTX_data_depsgraph(C),
	        vc.ar, vc.v3d, mval_fl,
	        ray_origin, ray_direction, true))
	{
		BMEdge *e;

		BMIter eiter;
		float dist_sq_best = FLT_MAX;

		BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
			if ((BM_elem_flag_test(e, BM_ELEM_HIDDEN) == false) &&
			    (!use_boundary || BM_edge_is_boundary(e)))
			{
				float dist_sq_test;
				float point[3];
				float depth;
#if 0
				dist_sq_test = dist_squared_ray_to_seg_v3(
				        ray_origin, ray_direction,
				        e->v1->co,  e->v2->co,
				        point, &depth);
#else
				mid_v3_v3v3(point, e->v1->co,  e->v2->co);
				dist_sq_test = dist_squared_to_ray_v3(
				        ray_origin, ray_direction,
				        point, &depth);
#endif

				if (dist_sq_test < dist_sq_best) {
					dist_sq_best = dist_sq_test;
					ele_best = (BMElem *)e;
				}

				dist_sq_test = dist_squared_to_ray_v3(
				        ray_origin, ray_direction,
				        e->v1->co, &depth);
				if (dist_sq_test < dist_sq_best) {
					dist_sq_best = dist_sq_test;
					ele_best = (BMElem *)e->v1;
				}
				dist_sq_test = dist_squared_to_ray_v3(
				        ray_origin, ray_direction,
				        e->v2->co, &depth);
				if (dist_sq_test < dist_sq_best) {
					dist_sq_best = dist_sq_test;
					ele_best = (BMElem *)e->v2;
				}
			}
		}
	}
	return ele_best;
}

/*
 * Developer note: this is not advocating pre-selection highlighting.
 * This is just a quick way to test how a tool for interactively editing polygons may work. */
static int edbm_polybuild_hover_invoke(
        bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool use_boundary = RNA_boolean_get(op->ptr, "use_boundary");
	ViewContext vc;

	em_setup_viewcontext(C, &vc);

	/* Vertex selection is needed */
	if ((vc.scene->toolsettings->selectmode & SCE_SELECT_VERTEX) == 0) {
		return OPERATOR_PASS_THROUGH;
	}

	/* Don't overwrite click-drag events. */
	if (use_boundary == false) {
		/* pass */
	}
	else if (vc.win->tweak ||
	         (vc.win->eventstate->check_click &&
	          vc.win->eventstate->prevval == KM_PRESS &&
	          ISMOUSE(vc.win->eventstate->prevtype)))
	{
		return OPERATOR_PASS_THROUGH;
	}

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	BMElem *ele_active = BM_mesh_active_elem_get(bm);
	BMElem *ele_hover = edbm_hover_preselect(C, event->mval, use_boundary);

	if (ele_hover && (ele_hover != ele_active)) {
		if (event->shift == 0) {
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BM_select_history_clear(bm);
		}
		BM_elem_select_set(bm, ele_hover, true);
		BM_select_history_store(em->bm, ele_hover);
		BKE_mesh_batch_cache_dirty(obedit->data, BKE_MESH_BATCH_DIRTY_SELECT);

		ED_region_tag_redraw(vc.ar);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_polybuild_hover(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Poly Build Hover";
	ot->idname = "MESH_OT_polybuild_hover";
	ot->description = "";

	/* api callbacks */
	ot->invoke = edbm_polybuild_hover_invoke;
	ot->poll = EDBM_view3d_poll;

	/* flags */
	ot->flag = OPTYPE_INTERNAL;

	/* properties */
	RNA_def_boolean(ot->srna, "use_boundary", false, "Boundary", "Select only boundary geometry");
}

/** \} */
