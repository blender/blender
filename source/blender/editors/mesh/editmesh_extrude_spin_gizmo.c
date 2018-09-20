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

/** \file blender/editors/mesh/editmesh_extrude_spin_gizmo.c
 *  \ingroup edmesh
 */

#include "BLI_math.h"

#include "BKE_context.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "ED_gizmo_utils.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "mesh_intern.h"  /* own include */

#include "ED_transform.h"

#include "ED_gizmo_library.h"
#include "ED_undo.h"


/* -------------------------------------------------------------------- */
/** \name Spin Tool Gizmo
 * \{ */

typedef struct GizmoGroupData_SpinInit {
	struct {
		wmGizmo *xyz_view[4];
	} gizmos;

	/* Only for view orientation. */
	struct {
		float viewinv_m3[3][3];
	} prev;

	/* We could store more vars here! */
	struct {
		wmOperatorType *ot_spin;
	} data;
} GizmoGroupData_SpinInit;

static void gizmo_mesh_spin_init_setup(const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	/* alpha values for normal/highlighted states */
	const float alpha = 0.6f;
	const float alpha_hi = 1.0f;

	GizmoGroupData_SpinInit *ggd = MEM_callocN(sizeof(*ggd), __func__);
	gzgroup->customdata = ggd;
	const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);

	for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.xyz_view); i++) {
		wmGizmo *gz = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
		UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, gz->color);
		WM_gizmo_set_flag(gz, WM_GIZMO_DRAW_VALUE, true);
		ggd->gizmos.xyz_view[i] = gz;
	}

	for (int i = 0; i < 3; i++) {
		wmGizmo *gz = ggd->gizmos.xyz_view[i];
		RNA_enum_set(gz->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_CLIP);
		WM_gizmo_set_line_width(gz, 3.0f);
		float color[4];
		UI_GetThemeColor3fv(TH_AXIS_X + i, color);
		color[3] = alpha;
		WM_gizmo_set_color(gz, color);
		color[3] = alpha_hi;
		WM_gizmo_set_color_highlight(gz, color);
	}

	{
		wmGizmo *gz = ggd->gizmos.xyz_view[3];
		WM_gizmo_set_line_width(gz, 2.0f);
		float color[4];
		copy_v3_fl(color, 1.0f);
		color[3] = alpha;
		WM_gizmo_set_color(gz, color);
		color[3] = alpha_hi;
		WM_gizmo_set_color_highlight(gz, color);
	}

	WM_gizmo_set_scale(ggd->gizmos.xyz_view[3], 1.2f);

	ggd->data.ot_spin = WM_operatortype_find("MESH_OT_spin", true);
}

static void gizmo_mesh_spin_init_refresh(const bContext *C, wmGizmoGroup *gzgroup);

static void gizmo_mesh_spin_init_refresh_axis_orientation(
        wmGizmoGroup *gzgroup,
        int axis_index, const float axis_vec[3])
{
	GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
	wmGizmo *gz = ggd->gizmos.xyz_view[axis_index];
	WM_gizmo_set_matrix_rotation_from_z_axis(gz, axis_vec);

	PointerRNA *ptr = WM_gizmo_operator_set(gz, 0, ggd->data.ot_spin, NULL);
	RNA_float_set_array(ptr, "axis", axis_vec);
}


static void gizmo_mesh_spin_init_draw_prepare(
        const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float viewinv_m3[3][3];
	copy_m3_m4(viewinv_m3, rv3d->viewinv);

	{
		Scene *scene = CTX_data_scene(C);
		switch (scene->orientation_type) {
			case V3D_MANIP_VIEW:
			{
				if (!equals_m3m3(viewinv_m3, ggd->prev.viewinv_m3)) {
					/* Take care calling refresh from draw_prepare,
					 * this should be OK because it's only adjusting the cage orientation. */
					gizmo_mesh_spin_init_refresh(C, gzgroup);
				}
				break;
			}
		}
	}

	/* Refresh handled above when using view orientation. */
	if (!equals_m3m3(viewinv_m3, ggd->prev.viewinv_m3)) {
		gizmo_mesh_spin_init_refresh_axis_orientation(gzgroup, 3, rv3d->viewinv[2]);
		copy_m3_m4(ggd->prev.viewinv_m3, rv3d->viewinv);
	}
}

static void gizmo_mesh_spin_init_refresh(const bContext *C, wmGizmoGroup *gzgroup)
{
	GizmoGroupData_SpinInit *ggd = gzgroup->customdata;
	RegionView3D *rv3d = ED_view3d_context_rv3d((bContext *)C);

	{
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = CTX_wm_view3d(C);
		const View3DCursor *cursor = ED_view3d_cursor3d_get(scene, v3d);
		for (int i = 0; i < ARRAY_SIZE(ggd->gizmos.xyz_view); i++) {
			wmGizmo *gz = ggd->gizmos.xyz_view[i];
			WM_gizmo_set_matrix_location(gz, cursor->location);
		}
	}

	float mat[3][3];
	ED_transform_calc_orientation_from_type(C, mat);
	for (int i = 0; i < 3; i++) {
		gizmo_mesh_spin_init_refresh_axis_orientation(gzgroup, i, mat[i]);
	}

	{
		gizmo_mesh_spin_init_refresh_axis_orientation(gzgroup, 3, rv3d->viewinv[2]);
	}

	/* Needed to test view orientation changes. */
	copy_m3_m4(ggd->prev.viewinv_m3, rv3d->viewinv);
}


static void gizmo_mesh_spin_init_message_subscribe(
        const bContext *C, wmGizmoGroup *gzgroup, struct wmMsgBus *mbus)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);

	/* Subscribe to view properties */
	wmMsgSubscribeValue msg_sub_value_gz_tag_refresh = {
		.owner = ar,
		.user_data = gzgroup->parent_gzmap,
		.notify = WM_gizmo_do_msg_notify_tag_refresh,
	};

	PointerRNA scene_ptr;
	RNA_id_pointer_create(&scene->id, &scene_ptr);

	{
		extern PropertyRNA rna_Scene_transform_orientation;
		extern PropertyRNA rna_Scene_cursor_location;
		const PropertyRNA *props[] = {
			&rna_Scene_transform_orientation,
			&rna_Scene_cursor_location,
		};
		for (int i = 0; i < ARRAY_SIZE(props); i++) {
			WM_msg_subscribe_rna(mbus, &scene_ptr, props[i], &msg_sub_value_gz_tag_refresh, __func__);
		}
	}
}

void MESH_GGT_spin(struct wmGizmoGroupType *gzgt)
{
	gzgt->name = "Mesh Spin Init";
	gzgt->idname = "MESH_GGT_spin";

	gzgt->flag = WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = ED_gizmo_poll_or_unlink_delayed_from_tool;
	gzgt->setup = gizmo_mesh_spin_init_setup;
	gzgt->refresh = gizmo_mesh_spin_init_refresh;
	gzgt->message_subscribe = gizmo_mesh_spin_init_message_subscribe;
	gzgt->draw_prepare = gizmo_mesh_spin_init_draw_prepare;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Spin Redo Gizmo
 * \{ */

/**
 * Don't show these for now, because they overlay the tools #MESH_GGT_spin_redo gizmos.
 * this means we cant rotate the currently running gizmo.
 */
// #define USE_EXTRA_CONTROLS
/**
 * Orient the dial so the 'arc' starts where the mouse cursor is,
 * this is simply to keep the gizmo displaying where the cursor starts.
 * It's not needed for practical functionality.
 */
#define USE_ANGLE_Z_ORIENT

typedef struct GizmoGroupData_SpinRedo {
	/* Arrow to change plane depth. */
	struct wmGizmo *translate_z;
	/* Translate XYZ */
	struct wmGizmo *translate_c;
	/* For grabbing the gizmo and moving freely. */
	struct wmGizmo *rotate_c;
	/* Spin angle */
	struct wmGizmo *angle_z;

	/* We could store more vars here! */
	struct {
		bContext *context;
		wmOperatorType *ot;
		wmOperator *op;
		PropertyRNA *prop_axis_co;
		PropertyRNA *prop_axis_no;
		PropertyRNA *prop_angle;

		float rotate_axis[3];
		float rotate_up[3];
#ifdef USE_ANGLE_Z_ORIENT
		float orient_axis[3];
#endif
	} data;
} GizmoGroupData_SpinRedo;

/**
 * XXX. calling redo from property updates is not great.
 * This is needed because changing the RNA doesn't cause a redo
 * and we're not using operator UI which does just this.
 */
static void gizmo_spin_exec(GizmoGroupData_SpinRedo *ggd)
{
	wmOperator *op = ggd->data.op;
	if (op == WM_operator_last_redo((bContext *)ggd->data.context)) {
		ED_undo_operator_repeat((bContext *)ggd->data.context, op);
	}
}

static void gizmo_mesh_spin_redo_update_from_op(GizmoGroupData_SpinRedo *ggd)
{
	wmOperator *op = ggd->data.op;

	float plane_co[3], plane_no[3];

	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);

	WM_gizmo_set_matrix_location(ggd->translate_z, plane_co);
	WM_gizmo_set_matrix_location(ggd->rotate_c, plane_co);
	WM_gizmo_set_matrix_location(ggd->angle_z, plane_co);
	/* translate_c location comes from the property. */

	WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_z, plane_no);
#ifdef USE_ANGLE_Z_ORIENT
	{
		float plane_tan[3];
		project_plane_normalized_v3_v3v3(plane_tan, ggd->data.orient_axis, plane_no);
		if (normalize_v3(plane_tan) != 0.0f) {
			WM_gizmo_set_matrix_rotation_from_yz_axis(ggd->angle_z, plane_tan, plane_no);
		}
		else {
			WM_gizmo_set_matrix_rotation_from_z_axis(ggd->angle_z, plane_no);
		}
	}
#else
	WM_gizmo_set_matrix_rotation_from_z_axis(ggd->angle_z, plane_no);
#endif

	WM_gizmo_set_scale(ggd->translate_c, 0.2);

	WM_gizmo_set_scale(ggd->angle_z, 2.0f);
	WM_gizmo_set_line_width(ggd->angle_z, 1.0f);

	RegionView3D *rv3d = ED_view3d_context_rv3d(ggd->data.context);
	if (rv3d) {
		normalize_v3_v3(ggd->data.rotate_axis, rv3d->viewinv[2]);
		normalize_v3_v3(ggd->data.rotate_up, rv3d->viewinv[1]);

		/* ensure its orthogonal */
		project_plane_normalized_v3_v3v3(ggd->data.rotate_up, ggd->data.rotate_up, ggd->data.rotate_axis);
		normalize_v3(ggd->data.rotate_up);

		WM_gizmo_set_matrix_rotation_from_z_axis(ggd->translate_c, plane_no);
		WM_gizmo_set_matrix_rotation_from_yz_axis(ggd->rotate_c, plane_no, ggd->data.rotate_axis);

		/* show the axis instead of mouse cursor */
		RNA_enum_set(ggd->rotate_c->ptr, "draw_options",
		             ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_MIRROR |
		             ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_START_Y);

	}
}

/* depth callbacks */
static void gizmo_spin_prop_depth_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);

	float plane_co[3], plane_no[3];
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);

	value[0] = dot_v3v3(plane_no, plane_co) - dot_v3v3(plane_no, gz->matrix_basis[3]);
}

static void gizmo_spin_prop_depth_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	const float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);

	float plane_co[3], plane[4];
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane);
	normalize_v3(plane);

	plane[3] = -value[0] - dot_v3v3(plane, gz->matrix_basis[3]);

	/* Keep our location, may be offset simply to be inside the viewport. */
	closest_to_plane_normalized_v3(plane_co, plane, plane_co);

	RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_co, plane_co);

	gizmo_spin_exec(ggd);
}

/* translate callbacks */
static void gizmo_spin_prop_translate_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(gz_prop);

	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, value);
}

static void gizmo_spin_prop_translate_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;

	BLI_assert(gz_prop->type->array_length == 3);
	UNUSED_VARS_NDEBUG(gz_prop);

	RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_co, value);

	gizmo_spin_exec(ggd);
}

/* angle callbacks */
static void gizmo_spin_prop_axis_angle_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
	normalize_v3(plane_no);

	float plane_no_proj[3];
	project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, ggd->data.rotate_axis);

	if (!is_zero_v3(plane_no_proj)) {
		const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, ggd->data.rotate_up, ggd->data.rotate_axis);
		value[0] = angle;
	}
	else {
		value[0] = 0.0f;
	}
}

static void gizmo_spin_prop_axis_angle_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	const float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);

	float plane_no[4];
	RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
	normalize_v3(plane_no);

	float plane_no_proj[3];
	project_plane_normalized_v3_v3v3(plane_no_proj, plane_no, ggd->data.rotate_axis);

	if (!is_zero_v3(plane_no_proj)) {
		const float angle = -angle_signed_on_axis_v3v3_v3(plane_no_proj, ggd->data.rotate_up, ggd->data.rotate_axis);
		const float angle_delta = angle - angle_compat_rad(value[0], angle);
		if (angle_delta != 0.0f) {
			float mat[3][3];
			axis_angle_normalized_to_mat3(mat, ggd->data.rotate_axis, angle_delta);
			mul_m3_v3(mat, plane_no);

			/* re-normalize - seems acceptable */
			RNA_property_float_set_array(op->ptr, ggd->data.prop_axis_no, plane_no);

			gizmo_spin_exec(ggd);
		}
	}
}

/* angle callbacks */
static void gizmo_spin_prop_angle_get(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	float *value = value_p;

	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);
	value[0] = RNA_property_float_get(op->ptr, ggd->data.prop_angle);
}

static void gizmo_spin_prop_angle_set(
        const wmGizmo *gz, wmGizmoProperty *gz_prop,
        const void *value_p)
{
	GizmoGroupData_SpinRedo *ggd = gz->parent_gzgroup->customdata;
	wmOperator *op = ggd->data.op;
	BLI_assert(gz_prop->type->array_length == 1);
	UNUSED_VARS_NDEBUG(gz_prop);
	const float *value = value_p;
	RNA_property_float_set(op->ptr, ggd->data.prop_angle, value[0]);

	gizmo_spin_exec(ggd);
}

static bool gizmo_mesh_spin_redo_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
	return ED_gizmo_poll_or_unlink_delayed_from_operator(C, gzgt, "MESH_OT_spin");
}


static void gizmo_mesh_spin_redo_modal_from_setup(
        const bContext *C, wmGizmoGroup *gzgroup)
{
	/* Start off dragging. */
	GizmoGroupData_SpinRedo *ggd = gzgroup->customdata;
	wmWindow *win = CTX_wm_window(C);
	wmGizmo *gz = ggd->angle_z;
	wmGizmoMap *gzmap = gzgroup->parent_gzmap;


#ifdef USE_ANGLE_Z_ORIENT
	{
		wmOperator *op = ggd->data.op;
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);
		const wmEvent *event = win->eventstate;
		float plane_co[3], plane_no[3];
		RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_co, plane_co);
		RNA_property_float_get_array(op->ptr, ggd->data.prop_axis_no, plane_no);
		float cursor_co[3];
		const int mval[2] = {event->x - ar->winrct.xmin, event->y - ar->winrct.ymin};
		float plane[4];
		plane_from_point_normal_v3(plane, plane_co, plane_no);
		if (UNLIKELY(!ED_view3d_win_to_3d_on_plane_int(ar, plane, mval, false, cursor_co))) {
			ED_view3d_win_to_3d_int(v3d, ar, plane, mval, cursor_co);
		}
		sub_v3_v3v3(ggd->data.orient_axis, cursor_co, plane_co);
		normalize_v3(ggd->data.orient_axis);
	}
#endif

	WM_gizmo_modal_set_from_setup(
	        gzmap, (bContext *)C, gz, 0, win->eventstate);
}

static void gizmo_mesh_spin_redo_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
	wmOperatorType *ot = WM_operatortype_find("MESH_OT_spin", true);
	wmOperator *op = WM_operator_last_redo(C);

	if ((op == NULL) || (op->type != ot)) {
		return;
	}

	GizmoGroupData_SpinRedo *ggd = MEM_callocN(sizeof(*ggd), __func__);
	gzgroup->customdata = ggd;

	const wmGizmoType *gzt_arrow = WM_gizmotype_find("GIZMO_GT_arrow_3d", true);
	const wmGizmoType *gzt_move = WM_gizmotype_find("GIZMO_GT_move_3d", true);
	const wmGizmoType *gzt_dial = WM_gizmotype_find("GIZMO_GT_dial_3d", true);

	ggd->translate_z = WM_gizmo_new_ptr(gzt_arrow, gzgroup, NULL);
	ggd->translate_c = WM_gizmo_new_ptr(gzt_move, gzgroup, NULL);
	ggd->rotate_c = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);
	ggd->angle_z = WM_gizmo_new_ptr(gzt_dial, gzgroup, NULL);

	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->translate_z->color);
	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, ggd->translate_c->color);
	UI_GetThemeColor3fv(TH_GIZMO_SECONDARY, ggd->rotate_c->color);
	copy_v3_v3(ggd->angle_z->color, ggd->angle_z->color_hi);
	ggd->angle_z->color[3] = 0.5f;

	RNA_enum_set(ggd->translate_z->ptr, "draw_style", ED_GIZMO_ARROW_STYLE_NORMAL);
	RNA_enum_set(ggd->translate_c->ptr, "draw_style", ED_GIZMO_MOVE_STYLE_RING_2D);

	RNA_boolean_set(ggd->angle_z->ptr, "wrap_angle", false);
	RNA_enum_set(ggd->angle_z->ptr, "draw_options", ED_GIZMO_DIAL_DRAW_FLAG_ANGLE_VALUE);
	RNA_float_set(ggd->angle_z->ptr, "arc_inner_factor", 0.9f);

	WM_gizmo_set_flag(ggd->translate_c, WM_GIZMO_DRAW_VALUE, true);
	WM_gizmo_set_flag(ggd->rotate_c, WM_GIZMO_DRAW_VALUE, true);
	WM_gizmo_set_flag(ggd->angle_z, WM_GIZMO_DRAW_VALUE, true);

	WM_gizmo_set_scale(ggd->rotate_c, 0.8f);

	{
		ggd->data.context = (bContext *)C;
		ggd->data.ot = ot;
		ggd->data.op = op;
		ggd->data.prop_axis_co = RNA_struct_type_find_property(ot->srna, "center");
		ggd->data.prop_axis_no = RNA_struct_type_find_property(ot->srna, "axis");
		ggd->data.prop_angle = RNA_struct_type_find_property(ot->srna, "angle");
	}

	gizmo_mesh_spin_redo_update_from_op(ggd);

	/* Setup property callbacks */
	{
		WM_gizmo_target_property_def_func(
		        ggd->translate_z, "offset",
		        &(const struct wmGizmoPropertyFnParams) {
		            .value_get_fn = gizmo_spin_prop_depth_get,
		            .value_set_fn = gizmo_spin_prop_depth_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_gizmo_target_property_def_func(
		        ggd->translate_c, "offset",
		        &(const struct wmGizmoPropertyFnParams) {
		            .value_get_fn = gizmo_spin_prop_translate_get,
		            .value_set_fn = gizmo_spin_prop_translate_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_gizmo_target_property_def_func(
		        ggd->rotate_c, "offset",
		        &(const struct wmGizmoPropertyFnParams) {
		            .value_get_fn = gizmo_spin_prop_axis_angle_get,
		            .value_set_fn = gizmo_spin_prop_axis_angle_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

		WM_gizmo_target_property_def_func(
		        ggd->angle_z, "offset",
		        &(const struct wmGizmoPropertyFnParams) {
		            .value_get_fn = gizmo_spin_prop_angle_get,
		            .value_set_fn = gizmo_spin_prop_angle_set,
		            .range_get_fn = NULL,
		            .user_data = NULL,
		        });

	}

#ifndef USE_EXTRA_CONTROLS
	/* Disable for now. */
	WM_gizmo_set_flag(ggd->translate_z, WM_GIZMO_HIDDEN, true);
	WM_gizmo_set_flag(ggd->translate_c, WM_GIZMO_HIDDEN, true);
	WM_gizmo_set_flag(ggd->rotate_c, WM_GIZMO_HIDDEN, true);
#endif

	/* Become modal as soon as it's started. */
	gizmo_mesh_spin_redo_modal_from_setup(C, gzgroup);
}

static void gizmo_mesh_spin_redo_draw_prepare(
        const bContext *UNUSED(C), wmGizmoGroup *gzgroup)
{
	GizmoGroupData_SpinRedo *ggd = gzgroup->customdata;
	if (ggd->data.op->next) {
		ggd->data.op = WM_operator_last_redo((bContext *)ggd->data.context);
	}
	gizmo_mesh_spin_redo_update_from_op(ggd);
}

void MESH_GGT_spin_redo(struct wmGizmoGroupType *gzgt)
{
	gzgt->name = "Mesh Spin Redo";
	gzgt->idname = "MESH_GGT_spin_redo";

	gzgt->flag = WM_GIZMOGROUPTYPE_3D;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = gizmo_mesh_spin_redo_poll;
	gzgt->setup = gizmo_mesh_spin_redo_setup;
	gzgt->draw_prepare = gizmo_mesh_spin_redo_draw_prepare;
}

/** \} */
