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

#include "BLT_translation.h"

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

#include "UI_resources.h"

#include "mesh_intern.h"  /* own include */

#define USE_MANIPULATOR

#ifdef USE_MANIPULATOR
#include "ED_manipulator_library.h"
#include "ED_undo.h"
#endif

static int mesh_bisect_exec(bContext *C, wmOperator *op);

/* -------------------------------------------------------------------- */
/* Model Helpers */

typedef struct {
	/* modal only */
	BMBackup mesh_backup;
	bool is_first;
	short twflag;
} BisectData;

static bool mesh_bisect_interactive_calc(
        bContext *C, wmOperator *op,
        BMEditMesh *em,
        float plane_co[3], float plane_no[3])
{
	wmGesture *gesture = op->customdata;
	BisectData *opdata;

	View3D *v3d = CTX_wm_view3d(C);
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
	ED_view3d_win_to_3d(v3d, ar, co_ref, co_a_ss, plane_co);

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
		opdata->twflag = v3d->twflag;
		v3d->twflag = 0;

		/* initialize modal callout */
		ED_area_headerprint(CTX_wm_area(C), IFACE_("LMB: Click and drag to draw cut line"));
	}
	return ret;
}

static void edbm_bisect_exit(bContext *C, BisectData *opdata)
{
	View3D *v3d = CTX_wm_view3d(C);
	EDBM_redo_state_free(&opdata->mesh_backup, NULL, false);
	v3d->twflag = opdata->twflag;
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

#ifdef USE_MANIPULATOR
		/* Setup manipulators */
		{
			View3D *v3d = CTX_wm_view3d(C);
			if (v3d && (v3d->twflag & V3D_MANIPULATOR_DRAW)) {
				WM_manipulator_group_type_ensure("MESH_WGT_bisect");
			}
		}
#endif
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
		copy_v3_v3(plane_co, ED_view3d_cursor3d_get(scene, v3d)->location);
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
	mul_transposed_mat3_m4_v3(obedit->obmat, plane_no);

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
		        bm, &bmop_fill, 0,
		        "triangle_fill edges=%S normal=%v use_dissolve=%b",
		        &bmop, "geom_cut.out", normal_fill, true);
		BMO_op_exec(bm, &bmop_fill);

		/* Copy Attributes */
		BMO_op_initf(bm, &bmop_attr, 0,
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

#ifdef USE_MANIPULATOR
static void MESH_WGT_bisect(struct wmManipulatorGroupType *wgt);
#endif

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


	prop = RNA_def_float_vector(ot->srna, "plane_co", 3, NULL, -1e12f, 1e12f,
	                            "Plane Point", "A point on the plane", -1e4f, 1e4f);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_float_vector(ot->srna, "plane_no", 3, NULL, -1.0f, 1.0f,
	                            "Plane Normal", "The direction the plane points", -1.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);

	RNA_def_boolean(ot->srna, "use_fill", false, "Fill", "Fill in the cut");
	RNA_def_boolean(ot->srna, "clear_inner", false, "Clear Inner", "Remove geometry behind the plane");
	RNA_def_boolean(ot->srna, "clear_outer", false, "Clear Outer", "Remove geometry in front of the plane");

	RNA_def_float(ot->srna, "threshold", 0.0001, 0.0, 10.0, "Axis Threshold", "", 0.00001, 0.1);

	WM_operator_properties_gesture_straightline(ot, CURSOR_EDIT);

#ifdef USE_MANIPULATOR
	WM_manipulatorgrouptype_append(MESH_WGT_bisect);
#endif
}


#ifdef USE_MANIPULATOR

/* -------------------------------------------------------------------- */

/** \name Bisect Manipulator
 * \{ */

typedef struct ManipulatorGroup {
	/* Arrow to change plane depth. */
	struct wmManipulator *translate_z;
	/* Translate XYZ */
	struct wmManipulator *translate_c;
	/* For grabbing the manipulator and moving freely. */
	struct wmManipulator *rotate_c;

	/* We could store more vars here! */
	struct {
		bContext *context;
		wmOperator *op;
		PropertyRNA *prop_plane_co;
		PropertyRNA *prop_plane_no;

		float rotate_axis[3];
		float rotate_up[3];
	} data;
} ManipulatorGroup;

/**
 * XXX. calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void manipulator_bisect_exec(ManipulatorGroup *man)
{
	wmOperator *op = man->data.op;
	if (op == WM_operator_last_redo((bContext *)man->data.context)) {
		ED_undo_operator_repeat((bContext *)man->data.context, op);
	}
}

static void manipulator_mesh_bisect_update_from_op(ManipulatorGroup *man)
{
	wmOperator *op = man->data.op;

	float plane_co[3], plane_no[3];

	RNA_property_float_get_array(op->ptr, man->data.prop_plane_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_no, plane_no);

	WM_manipulator_set_matrix_location(man->translate_z, plane_co);
	WM_manipulator_set_matrix_location(man->rotate_c, plane_co);
	/* translate_c location comes from the property. */

	WM_manipulator_set_matrix_rotation_from_z_axis(man->translate_z, plane_no);

	WM_manipulator_set_scale(man->translate_c, 0.2);

	RegionView3D *rv3d = ED_view3d_context_rv3d(man->data.context);
	if (rv3d) {
		normalize_v3_v3(man->data.rotate_axis, rv3d->viewinv[2]);
		normalize_v3_v3(man->data.rotate_up, rv3d->viewinv[1]);

		/* ensure its orthogonal */
		project_plane_normalized_v3_v3v3(man->data.rotate_up, man->data.rotate_up, man->data.rotate_axis);
		normalize_v3(man->data.rotate_up);

		WM_manipulator_set_matrix_rotation_from_z_axis(man->translate_c, plane_no);

		float plane_no_cross[3];
		cross_v3_v3v3(plane_no_cross, plane_no, man->data.rotate_axis);

		WM_manipulator_set_matrix_offset_rotation_from_yz_axis(man->rotate_c, plane_no_cross, man->data.rotate_axis);
		RNA_enum_set(man->rotate_c->ptr, "draw_options",
		             ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_MIRROR |
		             ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_START_Y);
	}
}

/* depth callbacks */
static void manipulator_bisect_prop_depth_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_co[3], plane_no[3];
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_no, plane_no);

	value[0] = dot_v3v3(plane_no, plane_co) - dot_v3v3(plane_no, mpr->matrix_basis[3]);
}

static void manipulator_bisect_prop_depth_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	const float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_co[3], plane[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_no, plane);
	normalize_v3(plane);

	plane[3] = -value[0] - dot_v3v3(plane, mpr->matrix_basis[3]);

	/* Keep our location, may be offset simply to be inside the viewport. */
	closest_to_plane_normalized_v3(plane_co, plane, plane_co);

	RNA_property_float_set_array(op->ptr, man->data.prop_plane_co, plane_co);

	manipulator_bisect_exec(man);
}

/* translate callbacks */
static void manipulator_bisect_prop_translate_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;

	BLI_assert(mpr_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(mpr_prop);

	RNA_property_float_get_array(op->ptr, man->data.prop_plane_co, value_p);
}

static void manipulator_bisect_prop_translate_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;

	BLI_assert(mpr_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(mpr_prop);

	RNA_property_float_set_array(op->ptr, man->data.prop_plane_co, value_p);

	manipulator_bisect_exec(man);
}

/* angle callbacks */
static void manipulator_bisect_prop_angle_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_no, plane_no);
	normalize_v3(plane_no);

	float plane_no_proj[3];
	project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, man->data.rotate_axis);

	if (!is_zero_v3(plane_no_proj)) {
		const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, man->data.rotate_up, man->data.rotate_axis);
		value[0] = angle;
	}
	else {
		value[0] = 0.0f;
	}
}

static void manipulator_bisect_prop_angle_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	const float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_plane_no, plane_no);
	normalize_v3(plane_no);

	float plane_no_proj[3];
	project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, man->data.rotate_axis);

	if (!is_zero_v3(plane_no_proj)) {
		const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, man->data.rotate_up, man->data.rotate_axis);
		const float angle_delta = angle - angle_compat_rad(value[0], angle);
		if (angle_delta != 0.0f) {
			float mat[3][3];
			axis_angle_normalized_to_mat3(mat, man->data.rotate_axis, angle_delta);
			mul_m3_v3(mat, plane_no);

			/* re-normalize - seems acceptable */
			RNA_property_float_set_array(op->ptr, man->data.prop_plane_no, plane_no);

			manipulator_bisect_exec(man);
		}
	}
}

static bool manipulator_mesh_bisect_poll(const bContext *C, wmManipulatorGroupType *wgt)
{
	wmOperator *op = WM_operator_last_redo(C);
	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_bisect")) {
		WM_manipulator_group_type_unlink_delayed_ptr(wgt);
		return false;
	}
	return true;
}

static void manipulator_mesh_bisect_setup(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmOperator *op = WM_operator_last_redo(C);

	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_bisect")) {
		return;
	}

	struct ManipulatorGroup *man = MEM_callocN(sizeof(ManipulatorGroup), __func__);
	mgroup->customdata = man;

	const wmManipulatorType *wt_arrow = WM_manipulatortype_find("MANIPULATOR_WT_arrow_3d", true);
	const wmManipulatorType *wt_grab = WM_manipulatortype_find("MANIPULATOR_WT_grab_3d", true);
	const wmManipulatorType *wt_dial = WM_manipulatortype_find("MANIPULATOR_WT_dial_3d", true);

	man->translate_z = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
	man->translate_c = WM_manipulator_new_ptr(wt_grab, mgroup, NULL);
	man->rotate_c = WM_manipulator_new_ptr(wt_dial, mgroup, NULL);

	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, man->translate_z->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, man->translate_c->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_SECONDARY, man->rotate_c->color);

	RNA_enum_set(man->translate_z->ptr, "draw_style", ED_MANIPULATOR_ARROW_STYLE_NORMAL);
	RNA_enum_set(man->translate_c->ptr, "draw_style", ED_MANIPULATOR_GRAB_STYLE_RING_2D);

	WM_manipulator_set_flag(man->translate_c, WM_MANIPULATOR_DRAW_VALUE, true);
	WM_manipulator_set_flag(man->rotate_c, WM_MANIPULATOR_DRAW_VALUE, true);

	{
		man->data.context = (bContext *)C;
		man->data.op = op;
		man->data.prop_plane_co = RNA_struct_find_property(op->ptr, "plane_co");
		man->data.prop_plane_no = RNA_struct_find_property(op->ptr, "plane_no");
	}

	manipulator_mesh_bisect_update_from_op(man);

	/* Setup property callbacks */
	{
		WM_manipulator_target_property_def_func(
		        man->translate_z, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_bisect_prop_depth_get,
		            .value_set_fn = manipulator_bisect_prop_depth_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_manipulator_target_property_def_func(
		        man->translate_c, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_bisect_prop_translate_get,
		            .value_set_fn = manipulator_bisect_prop_translate_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_manipulator_target_property_def_func(
		        man->rotate_c, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_bisect_prop_angle_get,
		            .value_set_fn = manipulator_bisect_prop_angle_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });
	}
}

static void manipulator_mesh_bisect_draw_prepare(
        const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	ManipulatorGroup *man = mgroup->customdata;
	if (man->data.op->next) {
		man->data.op = WM_operator_last_redo((bContext *)man->data.context);
	}
	manipulator_mesh_bisect_update_from_op(man);
}

static void MESH_WGT_bisect(struct wmManipulatorGroupType *wgt)
{
	wgt->name = "Mesh Bisect";
	wgt->idname = "MESH_WGT_bisect";

	wgt->flag = WM_MANIPULATORGROUPTYPE_3D;

	wgt->mmap_params.spaceid = SPACE_VIEW3D;
	wgt->mmap_params.regionid = RGN_TYPE_WINDOW;

	wgt->poll = manipulator_mesh_bisect_poll;
	wgt->setup = manipulator_mesh_bisect_setup;
	wgt->draw_prepare = manipulator_mesh_bisect_draw_prepare;
}

/** \} */

#endif  /* USE_MANIPULATOR */
