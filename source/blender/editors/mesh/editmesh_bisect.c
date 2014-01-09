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
 * The Original Code is Copyright (C) 2013 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_bisect.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "BLF_translation.h"

#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_report.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"


#include "mesh_intern.h"  /* own include */

static int mesh_bisect_exec(bContext *C, wmOperator *op);

/* -------------------------------------------------------------------- */
/* Model Helpers */

typedef struct {
	/* modal only */
	BMBackup mesh_backup;
	bool is_first;
	short twtype;
} BisectData;

static bool mesh_bisect_interactive_calc(
        bContext *C, wmOperator *op,
        BMEditMesh *em,
        float plane_co[3], float plane_no[3])
{
	wmGesture *gesture = op->customdata;
	BisectData *opdata;

	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	int x_start = RNA_int_get(op->ptr, "xstart");
	int y_start = RNA_int_get(op->ptr, "ystart");
	int x_end = RNA_int_get(op->ptr, "xend");
	int y_end = RNA_int_get(op->ptr, "yend");

	/* reference location (some point in front of the view) for finding a point on a plane */
	const float *co_ref = rv3d->ofs;
	float co_a_ss[2] = {x_start, y_start}, co_b_ss[2] = {x_end, y_end}, co_delta_ss[2];
	float co_a[3], co_b[3];
	const float zfac = ED_view3d_calc_zfac(rv3d, co_ref, NULL);

	opdata = gesture->userdata;

	/* view vector */
	ED_view3d_win_to_vector(ar, co_a_ss, co_a);

	/* view delta */
	sub_v2_v2v2(co_delta_ss, co_a_ss, co_b_ss);
	ED_view3d_win_to_delta(ar, co_delta_ss, co_b, zfac);

	/* cross both to get a normal */
	cross_v3_v3v3(plane_no, co_a, co_b);
	normalize_v3(plane_no);  /* not needed but nicer for user */

	/* point on plane, can use either start or endpoint */
	ED_view3d_win_to_3d(ar, co_ref, co_a_ss, plane_co);

	if (opdata->is_first == false)
		EDBM_redo_state_restore(opdata->mesh_backup, em, false);

	opdata->is_first = false;

	return true;
}

static int mesh_bisect_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	int ret;

	if (em->bm->totedgesel == 0) {
		BKE_report(op->reports, RPT_ERROR, "Selected edges/faces required");
		return OPERATOR_CANCELLED;
	}

	/* if the properties are set or there is no rv3d,
	 * skip model and exec immediately */

	if ((CTX_wm_region_view3d(C) == NULL) ||
	    (RNA_struct_property_is_set(op->ptr, "plane_co") &&
	     RNA_struct_property_is_set(op->ptr, "plane_no")))
	{
		return mesh_bisect_exec(C, op);
	}

	ret = WM_gesture_straightline_invoke(C, op, event);
	if (ret & OPERATOR_RUNNING_MODAL) {
		View3D *v3d = CTX_wm_view3d(C);

		wmGesture *gesture = op->customdata;
		BisectData *opdata;


		opdata = MEM_mallocN(sizeof(BisectData), "inset_operator_data");
		opdata->mesh_backup = EDBM_redo_state_store(em);
		opdata->is_first = true;
		gesture->userdata = opdata;

		/* misc other vars */
		G.moving = G_TRANSFORM_EDIT;
		opdata->twtype = v3d->twtype;
		v3d->twtype = 0;

		/* initialize modal callout */
		ED_area_headerprint(CTX_wm_area(C), IFACE_("LMB: Click and drag to draw cut line"));
	}
	return ret;
}

static void edbm_bisect_exit(bContext *C, BisectData *opdata)
{
	View3D *v3d = CTX_wm_view3d(C);
	EDBM_redo_state_free(&opdata->mesh_backup, NULL, false);
	v3d->twtype = opdata->twtype;
	G.moving = 0;
}

static int mesh_bisect_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	wmGesture *gesture = op->customdata;
	BisectData *opdata = gesture->userdata;
	BisectData opdata_back = *opdata;  /* annoyance, WM_gesture_straightline_modal, frees */
	int ret;

	ret = WM_gesture_straightline_modal(C, op, event);

	/* update or clear modal callout */
	if (event->type == EVT_MODAL_MAP) {
		if (event->val == GESTURE_MODAL_BEGIN) {
			ED_area_headerprint(CTX_wm_area(C), IFACE_("LMB: Release to confirm cut line"));
		}
		else {
			ED_area_headerprint(CTX_wm_area(C), NULL);
		}
	}

	if (ret & (OPERATOR_FINISHED | OPERATOR_CANCELLED)) {
		edbm_bisect_exit(C, &opdata_back);
	}

	return ret;
}

/* End Model Helpers */
/* -------------------------------------------------------------------- */



static int mesh_bisect_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);

	/* both can be NULL, fallbacks values are used */
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm;
	BMOperator bmop;
	float plane_co[3];
	float plane_no[3];
	float imat[4][4];

	const float thresh = RNA_float_get(op->ptr, "threshold");
	const bool use_fill = RNA_boolean_get(op->ptr, "use_fill");
	const bool clear_inner = RNA_boolean_get(op->ptr, "clear_inner");
	const bool clear_outer = RNA_boolean_get(op->ptr, "clear_outer");

	PropertyRNA *prop_plane_co;
	PropertyRNA *prop_plane_no;

	prop_plane_co = RNA_struct_find_property(op->ptr, "plane_co");
	if (RNA_property_is_set(op->ptr, prop_plane_co)) {
		RNA_property_float_get_array(op->ptr, prop_plane_co, plane_co);
	}
	else {
		copy_v3_v3(plane_co, ED_view3d_cursor3d_get(scene, v3d));
		RNA_property_float_set_array(op->ptr, prop_plane_co, plane_co);
	}

	prop_plane_no = RNA_struct_find_property(op->ptr, "plane_no");
	if (RNA_property_is_set(op->ptr, prop_plane_no)) {
		RNA_property_float_get_array(op->ptr, prop_plane_no, plane_no);
	}
	else {
		if (rv3d) {
			copy_v3_v3(plane_no, rv3d->viewinv[1]);
		}
		else {
			/* fallback... */
			plane_no[0] = plane_no[1] = 0.0f; plane_no[2] = 1.0f;
		}
		RNA_property_float_set_array(op->ptr, prop_plane_no, plane_no);
	}



	/* -------------------------------------------------------------------- */
	/* Modal support */
	/* Note: keep this isolated, exec can work wihout this */
	if ((op->customdata != NULL) &&
	    mesh_bisect_interactive_calc(C, op, em, plane_co, plane_no))
	{
		/* write back to the props */
		RNA_property_float_set_array(op->ptr, prop_plane_no, plane_no);
		RNA_property_float_set_array(op->ptr, prop_plane_co, plane_co);
	}
	/* End Modal */
	/* -------------------------------------------------------------------- */



	bm = em->bm;

	invert_m4_m4(imat, obedit->obmat);
	mul_m4_v3(imat, plane_co);
	mul_mat3_m4_v3(imat, plane_no);

	EDBM_op_init(em, &bmop, op,
	             "bisect_plane geom=%hvef plane_co=%v plane_no=%v dist=%f clear_inner=%b clear_outer=%b",
	             BM_ELEM_SELECT, plane_co, plane_no, thresh, clear_inner, clear_outer);
	BMO_op_exec(bm, &bmop);

	EDBM_flag_disable_all(em, BM_ELEM_SELECT);

	if (use_fill) {
		float normal_fill[3];
		BMOperator bmop_fill;
		BMOperator bmop_attr;

		normalize_v3_v3(normal_fill, plane_no);
		if (clear_outer == true && clear_inner == false) {
			negate_v3(normal_fill);
		}

		/* Fill */
		BMO_op_initf(
		        bm, &bmop_fill, op->flag,
		        "triangle_fill edges=%S normal=%v use_dissolve=%b",
		        &bmop, "geom_cut.out", normal_fill, true);
		BMO_op_exec(bm, &bmop_fill);

		/* Copy Attributes */
		BMO_op_initf(bm, &bmop_attr, op->flag,
		             "face_attribute_fill faces=%S use_normals=%b use_data=%b",
		             &bmop_fill, "geom.out", false, true);
		BMO_op_exec(bm, &bmop_attr);

		BMO_slot_buffer_hflag_enable(bm, bmop_fill.slots_out, "geom.out", BM_FACE, BM_ELEM_SELECT, true);

		BMO_op_finish(bm, &bmop_attr);
		BMO_op_finish(bm, &bmop_fill);
	}

	BMO_slot_buffer_hflag_enable(bm, bmop.slots_out, "geom_cut.out", BM_VERT | BM_EDGE, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}
	else {
		EDBM_update_generic(em, true, true);
		EDBM_selectmode_flush(em);
		return OPERATOR_FINISHED;
	}
}


void MESH_OT_bisect(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Bisect";
	ot->description = "Cut geometry along a plane (click-drag to define plane)";
	ot->idname = "MESH_OT_bisect";

	/* api callbacks */
	ot->exec = mesh_bisect_exec;
	ot->invoke = mesh_bisect_invoke;
	ot->modal = mesh_bisect_modal;
	ot->cancel = WM_gesture_straightline_cancel;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;


	prop = RNA_def_float_vector(ot->srna, "plane_co", 3, NULL, -FLT_MAX, FLT_MAX,
	                            "Plane Point", "A point on the plane", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_float_vector(ot->srna, "plane_no", 3, NULL, -FLT_MAX, FLT_MAX,
	                            "Plane Normal", "The direction the plane points", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_boolean(ot->srna, "use_fill", false, "Fill", "Fill in the cut");
	RNA_def_boolean(ot->srna, "clear_inner", false, "Clear Inner", "Remove geometry behind the plane");
	RNA_def_boolean(ot->srna, "clear_outer", false, "Clear Outer", "Remove geometry in front of the plane");

	RNA_def_float(ot->srna, "threshold", 0.0001, 0.0, 10.0, "Axis Threshold", "", 0.00001, 0.1);

	WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);
}
