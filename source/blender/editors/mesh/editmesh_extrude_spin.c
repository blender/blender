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

/** \file blender/editors/mesh/editmesh_extrude_spin.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h"  /* own include */

#define USE_MANIPULATOR

#ifdef USE_MANIPULATOR
#include "ED_manipulator_library.h"
#include "ED_undo.h"
#endif

/* -------------------------------------------------------------------- */
/** \name Spin Manipulator
 * \{ */

#ifdef USE_MANIPULATOR
typedef struct ManipulatorSpinGroup {
	/* Arrow to change plane depth. */
	struct wmManipulator *translate_z;
	/* Translate XYZ */
	struct wmManipulator *translate_c;
	/* For grabbing the manipulator and moving freely. */
	struct wmManipulator *rotate_c;
	/* Spin angle */
	struct wmManipulator *angle_z;

	/* We could store more vars here! */
	struct {
		bContext *context;
		wmOperator *op;
		PropertyRNA *prop_axis_co;
		PropertyRNA *prop_axis_no;
		PropertyRNA *prop_angle;

		float rotate_axis[3];
		float rotate_up[3];
	} data;
} ManipulatorSpinGroup;

/**
 * XXX. calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void manipulator_spin_exec(ManipulatorSpinGroup *man)
{
	wmOperator *op = man->data.op;
	if (op == WM_operator_last_redo((bContext *)man->data.context)) {
		ED_undo_operator_repeat((bContext *)man->data.context, op);
	}
}

static void manipulator_mesh_spin_update_from_op(ManipulatorSpinGroup *man)
{
	wmOperator *op = man->data.op;

	float plane_co[3], plane_no[3];

	RNA_property_float_get_array(op->ptr, man->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_no, plane_no);

	WM_manipulator_set_matrix_location(man->translate_z, plane_co);
	WM_manipulator_set_matrix_location(man->rotate_c, plane_co);
	WM_manipulator_set_matrix_location(man->angle_z, plane_co);
	/* translate_c location comes from the property. */

	WM_manipulator_set_matrix_rotation_from_z_axis(man->translate_z, plane_no);
	WM_manipulator_set_matrix_rotation_from_z_axis(man->angle_z, plane_no);

	WM_manipulator_set_scale(man->translate_c, 0.2);

	RegionView3D *rv3d = ED_view3d_context_rv3d(man->data.context);
	if (rv3d) {
		normalize_v3_v3(man->data.rotate_axis, rv3d->viewinv[2]);
		normalize_v3_v3(man->data.rotate_up, rv3d->viewinv[1]);

		/* ensure its orthogonal */
		project_plane_normalized_v3_v3v3(man->data.rotate_up, man->data.rotate_up, man->data.rotate_axis);
		normalize_v3(man->data.rotate_up);

		WM_manipulator_set_matrix_rotation_from_z_axis(man->translate_c, plane_no);
		WM_manipulator_set_matrix_rotation_from_yz_axis(man->rotate_c, plane_no, man->data.rotate_axis);

		/* show the axis instead of mouse cursor */
		RNA_enum_set(man->rotate_c->ptr, "draw_options",
		             ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_MIRROR |
		             ED_MANIPULATOR_DIAL_DRAW_FLAG_ANGLE_START_Y);

	}
}

/* depth callbacks */
static void manipulator_spin_prop_depth_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_co[3], plane_no[3];
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_no, plane_no);

	value[0] = dot_v3v3(plane_no, plane_co) - dot_v3v3(plane_no, mpr->matrix_basis[3]);
}

static void manipulator_spin_prop_depth_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	const float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_co[3], plane[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_no, plane);
	normalize_v3(plane);

	plane[3] = -value[0] - dot_v3v3(plane, mpr->matrix_basis[3]);

	/* Keep our location, may be offset simply to be inside the viewport. */
	closest_to_plane_normalized_v3(plane_co, plane, plane_co);

	RNA_property_float_set_array(op->ptr, man->data.prop_axis_co, plane_co);

	manipulator_spin_exec(man);
}

/* translate callbacks */
static void manipulator_spin_prop_translate_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(mpr_prop);

	RNA_property_float_get_array(op->ptr, man->data.prop_axis_co, value);
}

static void manipulator_spin_prop_translate_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;

	BLI_assert(mpr_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(mpr_prop);

	RNA_property_float_set_array(op->ptr, man->data.prop_axis_co, value);

	manipulator_spin_exec(man);
}

/* angle callbacks */
static void manipulator_spin_prop_axis_angle_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_no, plane_no);
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

static void manipulator_spin_prop_axis_angle_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	const float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, man->data.prop_axis_no, plane_no);
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
			RNA_property_float_set_array(op->ptr, man->data.prop_axis_no, plane_no);

			manipulator_spin_exec(man);
		}
	}
}

/* angle callbacks */
static void manipulator_spin_prop_angle_get(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	float *value = value_p;

	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);
	value[0] = RNA_property_float_get(op->ptr, man->data.prop_angle);
}

static void manipulator_spin_prop_angle_set(
        const wmManipulator *mpr, wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	ManipulatorSpinGroup *man = mpr->parent_mgroup->customdata;
	wmOperator *op = man->data.op;
	BLI_assert(mpr_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(mpr_prop);
	const float *value = value_p;
	RNA_property_float_set(op->ptr, man->data.prop_angle, value[0]);

	manipulator_spin_exec(man);
}

static bool manipulator_mesh_spin_poll(const bContext *C, wmManipulatorGroupType *wgt)
{
	wmOperator *op = WM_operator_last_redo(C);
	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_spin")) {
		WM_manipulator_group_type_unlink_delayed_ptr(wgt);
		return false;
	}
	return true;
}

static void manipulator_mesh_spin_setup(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmOperator *op = WM_operator_last_redo(C);

	if (op == NULL || !STREQ(op->type->idname, "MESH_OT_spin")) {
		return;
	}

	struct ManipulatorSpinGroup *man = MEM_callocN(sizeof(ManipulatorSpinGroup), __func__);
	mgroup->customdata = man;

	const wmManipulatorType *wt_arrow = WM_manipulatortype_find("MANIPULATOR_WT_arrow_3d", true);
	const wmManipulatorType *wt_grab = WM_manipulatortype_find("MANIPULATOR_WT_grab_3d", true);
	const wmManipulatorType *wt_dial = WM_manipulatortype_find("MANIPULATOR_WT_dial_3d", true);

	man->translate_z = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
	man->translate_c = WM_manipulator_new_ptr(wt_grab, mgroup, NULL);
	man->rotate_c = WM_manipulator_new_ptr(wt_dial, mgroup, NULL);
	man->angle_z = WM_manipulator_new_ptr(wt_dial, mgroup, NULL);

	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, man->translate_z->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, man->translate_c->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_SECONDARY, man->rotate_c->color);
	UI_GetThemeColor3fv(TH_AXIS_Z, man->angle_z->color);


	RNA_enum_set(man->translate_z->ptr, "draw_style", ED_MANIPULATOR_ARROW_STYLE_NORMAL);
	RNA_enum_set(man->translate_c->ptr, "draw_style", ED_MANIPULATOR_GRAB_STYLE_RING_2D);

	WM_manipulator_set_flag(man->translate_c, WM_MANIPULATOR_DRAW_VALUE, true);
	WM_manipulator_set_flag(man->rotate_c, WM_MANIPULATOR_DRAW_VALUE, true);
	WM_manipulator_set_flag(man->angle_z, WM_MANIPULATOR_DRAW_VALUE, true);

	WM_manipulator_set_scale(man->angle_z, 0.5f);

	{
		man->data.context = (bContext *)C;
		man->data.op = op;
		man->data.prop_axis_co = RNA_struct_find_property(op->ptr, "center");
		man->data.prop_axis_no = RNA_struct_find_property(op->ptr, "axis");
		man->data.prop_angle = RNA_struct_find_property(op->ptr, "angle");
	}

	manipulator_mesh_spin_update_from_op(man);

	/* Setup property callbacks */
	{
		WM_manipulator_target_property_def_func(
		        man->translate_z, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_spin_prop_depth_get,
		            .value_set_fn = manipulator_spin_prop_depth_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_manipulator_target_property_def_func(
		        man->translate_c, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_spin_prop_translate_get,
		            .value_set_fn = manipulator_spin_prop_translate_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_manipulator_target_property_def_func(
		        man->rotate_c, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_spin_prop_axis_angle_get,
		            .value_set_fn = manipulator_spin_prop_axis_angle_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_manipulator_target_property_def_func(
		        man->angle_z, "offset",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_spin_prop_angle_get,
		            .value_set_fn = manipulator_spin_prop_angle_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

	}
}

static void manipulator_mesh_spin_draw_prepare(
        const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	ManipulatorSpinGroup *man = mgroup->customdata;
	if (man->data.op->next) {
		man->data.op = WM_operator_last_redo((bContext *)man->data.context);
	}
	manipulator_mesh_spin_update_from_op(man);
}

static void MESH_WGT_spin(struct wmManipulatorGroupType *wgt)
{
	wgt->name = "Mesh Spin";
	wgt->idname = "MESH_WGT_spin";

	wgt->flag = WM_MANIPULATORGROUPTYPE_3D;

	wgt->mmap_params.spaceid = SPACE_VIEW3D;
	wgt->mmap_params.regionid = RGN_TYPE_WINDOW;

	wgt->poll = manipulator_mesh_spin_poll;
	wgt->setup = manipulator_mesh_spin_setup;
	wgt->draw_prepare = manipulator_mesh_spin_draw_prepare;
}

/** \} */

#endif  /* USE_MANIPULATOR */

/* -------------------------------------------------------------------- */
/** \name Spin Operator
 * \{ */

static int edbm_spin_exec(bContext *C, wmOperator *op)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	float cent[3], axis[3];
	float d[3] = {0.0f, 0.0f, 0.0f};
	int steps, dupli;
	float angle;

	RNA_float_get_array(op->ptr, "center", cent);
	RNA_float_get_array(op->ptr, "axis", axis);
	steps = RNA_int_get(op->ptr, "steps");
	angle = RNA_float_get(op->ptr, "angle");
	//if (ts->editbutflag & B_CLOCKWISE)
	angle = -angle;
	dupli = RNA_boolean_get(op->ptr, "dupli");

	if (is_zero_v3(axis)) {
		BKE_report(op->reports, RPT_ERROR, "Invalid/unset axis");
		return OPERATOR_CANCELLED;
	}

	uint objects_len = 0;
	Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, &objects_len);

	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		BMesh *bm = em->bm;
		BMOperator spinop;

		/* keep the values in worldspace since we're passing the obmat */
		if (!EDBM_op_init(em, &spinop, op,
		                  "spin geom=%hvef cent=%v axis=%v dvec=%v steps=%i angle=%f space=%m4 use_duplicate=%b",
		                  BM_ELEM_SELECT, cent, axis, d, steps, angle, obedit->obmat, dupli))
		{
			continue;
		}
		BMO_op_exec(bm, &spinop);
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(bm, spinop.slots_out, "geom_last.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, true);
		if (!EDBM_op_finish(em, &spinop, op, true)) {
			continue;
		}

		EDBM_update_generic(em, true, true);
	}

	MEM_freeN(objects);

	return OPERATOR_FINISHED;
}

/* get center and axis, in global coords */
static int edbm_spin_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);

	PropertyRNA *prop;
	prop = RNA_struct_find_property(op->ptr, "center");
	if (!RNA_property_is_set(op->ptr, prop)) {
		RNA_property_float_set_array(op->ptr, prop, ED_view3d_cursor3d_get(scene, v3d)->location);
	}
	if (rv3d) {
		prop = RNA_struct_find_property(op->ptr, "axis");
		if (!RNA_property_is_set(op->ptr, prop)) {
			RNA_property_float_set_array(op->ptr, prop, rv3d->viewinv[2]);
		}
	}

	int ret = edbm_spin_exec(C, op);

#ifdef USE_MANIPULATOR
	if (ret & OPERATOR_FINISHED) {
		/* Setup manipulators */
		if (v3d && (v3d->twflag & V3D_MANIPULATOR_DRAW)) {
			WM_manipulator_group_type_ensure("MESH_WGT_spin");
		}
	}
#endif

	return ret;
}

void MESH_OT_spin(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Spin";
	ot->description = "Extrude selected vertices in a circle around the cursor in indicated viewport";
	ot->idname = "MESH_OT_spin";

	/* api callbacks */
	ot->invoke = edbm_spin_invoke;
	ot->exec = edbm_spin_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_int(ot->srna, "steps", 9, 0, 1000000, "Steps", "Steps", 0, 1000);
	RNA_def_boolean(ot->srna, "dupli", 0, "Dupli", "Make Duplicates");
	prop = RNA_def_float(ot->srna, "angle", DEG2RADF(90.0f), -1e12f, 1e12f, "Angle", "Rotation for each step",
	                     DEG2RADF(-360.0f), DEG2RADF(360.0f));
	RNA_def_property_subtype(prop, PROP_ANGLE);

	RNA_def_float_vector(ot->srna, "center", 3, NULL, -1e12f, 1e12f,
	                     "Center", "Center in global view space", -1e4f, 1e4f);
	RNA_def_float_vector(ot->srna, "axis", 3, NULL, -1.0f, 1.0f, "Axis", "Axis in global view space", -1.0f, 1.0f);

#ifdef USE_MANIPULATOR
	WM_manipulatorgrouptype_append(MESH_WGT_spin);
#endif
}
