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

/** \file blender/editors/space_view3d/view3d_manipulator_camera.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"

#include "DNA_object_types.h"
#include "DNA_camera_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "view3d_intern.h"  /* own include */


/* -------------------------------------------------------------------- */

/** \name Camera Manipulators
 * \{ */

struct CameraWidgetGroup {
	wmManipulator *dop_dist;
	wmManipulator *focal_len;
	wmManipulator *ortho_scale;
};

static bool WIDGETGROUP_camera_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		return false;
	}

	Object *ob = CTX_data_active_object(C);
	if (ob && ob->type == OB_CAMERA) {
		Camera *camera = ob->data;
		/* TODO: support overrides. */
		if (camera->id.lib == NULL) {
			return true;
		}
	}
	return false;
}

static void WIDGETGROUP_camera_setup(const bContext *C, wmManipulatorGroup *mgroup)
{
	Object *ob = CTX_data_active_object(C);
	float dir[3];

	const wmManipulatorType *wt_arrow = WM_manipulatortype_find("MANIPULATOR_WT_arrow_3d", true);

	struct CameraWidgetGroup *camgroup = MEM_callocN(sizeof(struct CameraWidgetGroup), __func__);
	mgroup->customdata = camgroup;

	negate_v3_v3(dir, ob->obmat[2]);

	/* dof distance */
	{
		wmManipulator *mpr;
		mpr = camgroup->dop_dist = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
		RNA_enum_set(mpr->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CROSS);
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_HOVER, true);

		UI_GetThemeColor3fv(TH_MANIPULATOR_A, mpr->color);
		UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);
	}

	/* focal length
	 * - logic/calculations are similar to BKE_camera_view_frame_ex, better keep in sync */
	{
		wmManipulator *mpr;
		mpr = camgroup->focal_len = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
		mpr->flag |= WM_MANIPULATOR_DRAW_NO_SCALE;
		RNA_enum_set(mpr->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CONE);
		RNA_enum_set(mpr->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);

		UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
		UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);

		mpr = camgroup->ortho_scale = WM_manipulator_new_ptr(wt_arrow, mgroup, NULL);
		mpr->flag |= WM_MANIPULATOR_DRAW_NO_SCALE;
		RNA_enum_set(mpr->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CONE);
		RNA_enum_set(mpr->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);

		UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
		UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);
	}
}

static void WIDGETGROUP_camera_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	if (!mgroup->customdata)
		return;

	struct CameraWidgetGroup *camgroup = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	Camera *ca = ob->data;
	PointerRNA camera_ptr;
	float dir[3];

	const float ob_scale_inv[3] = {
		1.0f / len_v3(ob->obmat[0]),
		1.0f / len_v3(ob->obmat[1]),
		1.0f / len_v3(ob->obmat[2]),
	};
	const float ob_scale_uniform_inv = (ob_scale_inv[0] + ob_scale_inv[1] + ob_scale_inv[2]) / 3.0f;

	RNA_pointer_create(&ca->id, &RNA_Camera, ca, &camera_ptr);

	negate_v3_v3(dir, ob->obmat[2]);

	if (ca->flag & CAM_SHOWLIMITS) {
		WM_manipulator_set_matrix_location(camgroup->dop_dist, ob->obmat[3]);
		WM_manipulator_set_matrix_rotation_from_yz_axis(camgroup->dop_dist, ob->obmat[1], dir);
		WM_manipulator_set_scale(camgroup->dop_dist, ca->drawsize);
		WM_manipulator_set_flag(camgroup->dop_dist, WM_MANIPULATOR_HIDDEN, false);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		WM_manipulator_target_property_def_rna(camgroup->dop_dist, "offset", &camera_ptr, "dof_distance", -1);
	}
	else {
		WM_manipulator_set_flag(camgroup->dop_dist, WM_MANIPULATOR_HIDDEN, true);
	}

	/* TODO - make focal length/ortho ob_scale_inv widget optional */
	const Scene *scene = CTX_data_scene(C);
	const float aspx = (float)scene->r.xsch * scene->r.xasp;
	const float aspy = (float)scene->r.ysch * scene->r.yasp;
	const bool is_ortho = (ca->type == CAM_ORTHO);
	const int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, aspx, aspy);
	wmManipulator *widget = is_ortho ? camgroup->ortho_scale : camgroup->focal_len;
	float scale_matrix;
	if (true) {
		float offset[3];
		float aspect[2];

		WM_manipulator_set_flag(widget, WM_MANIPULATOR_HIDDEN, false);
		WM_manipulator_set_flag(is_ortho ? camgroup->focal_len : camgroup->ortho_scale, WM_MANIPULATOR_HIDDEN, true);


		/* account for lens shifting */
		offset[0] = ((ob->size[0] > 0.0f) ? -2.0f : 2.0f) * ca->shiftx;
		offset[1] = 2.0f * ca->shifty;
		offset[2] = 0.0f;

		/* get aspect */
		aspect[0] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? 1.0f : aspx / aspy;
		aspect[1] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? aspy / aspx : 1.0f;

		unit_m4(widget->matrix_basis);
		WM_manipulator_set_matrix_location(widget, ob->obmat[3]);
		WM_manipulator_set_matrix_rotation_from_yz_axis(widget, ob->obmat[1], dir);

		if (is_ortho) {
			scale_matrix = ca->ortho_scale * 0.5f;
		}
		else {
			scale_matrix = ca->drawsize / ob_scale_uniform_inv;
		}
		mul_v3_fl(widget->matrix_basis[0], scale_matrix);
		mul_v3_fl(widget->matrix_basis[1], scale_matrix);

		RNA_float_set_array(widget->ptr, "aspect", aspect);

		WM_manipulator_set_matrix_offset_location(widget, offset);
	}

	/* define & update properties */
	{
		const char *propname = is_ortho ? "ortho_scale" : "lens";
		PropertyRNA *prop = RNA_struct_find_property(&camera_ptr, propname);
		const wmManipulatorPropertyType *mpr_prop_type = WM_manipulatortype_target_property_find(widget->type, "offset");

		WM_manipulator_target_property_clear_rna_ptr(widget, mpr_prop_type);

		float min, max, range;
		float step, precision;

		/* get property range */
		RNA_property_float_ui_range(&camera_ptr, prop, &min, &max, &step, &precision);
		range = max - min;

		ED_manipulator_arrow3d_set_range_fac(
		        widget, is_ortho ?
		        (ca->drawsize * range) :
		        (scale_matrix * range /
		         /* Half sensor, intentionally use sensor from camera and not calculated above. */
		         (0.5f * ((ca->sensor_fit == CAMERA_SENSOR_FIT_HOR) ? ca->sensor_x : ca->sensor_x))));

		WM_manipulator_target_property_def_rna_ptr(widget, mpr_prop_type, &camera_ptr, prop, -1);
	}

}

static void WIDGETGROUP_camera_message_subscribe(
        const bContext *C, wmManipulatorGroup *mgroup, struct wmMsgBus *mbus)
{
	ARegion *ar = CTX_wm_region(C);
	Object *ob = CTX_data_active_object(C);
	Camera *ca = ob->data;

	wmMsgSubscribeValue msg_sub_value_mpr_tag_refresh = {
		.owner = ar,
		.user_data = mgroup->parent_mmap,
		.notify = WM_manipulator_do_msg_notify_tag_refresh,
	};

	{
		extern PropertyRNA rna_Camera_dof_distance;
		extern PropertyRNA rna_Camera_draw_size;
		extern PropertyRNA rna_Camera_ortho_scale;
		extern PropertyRNA rna_Camera_sensor_fit;
		extern PropertyRNA rna_Camera_sensor_width;
		extern PropertyRNA rna_Camera_shift_x;
		extern PropertyRNA rna_Camera_shift_y;
		extern PropertyRNA rna_Camera_type;
		extern PropertyRNA rna_Camera_lens;
		const PropertyRNA *props[] = {
			&rna_Camera_dof_distance,
			&rna_Camera_draw_size,
			&rna_Camera_ortho_scale,
			&rna_Camera_sensor_fit,
			&rna_Camera_sensor_width,
			&rna_Camera_shift_x,
			&rna_Camera_shift_y,
			&rna_Camera_type,
			&rna_Camera_lens,
		};

		PointerRNA idptr;
		RNA_id_pointer_create(&ca->id, &idptr);

		for (int i = 0; i < ARRAY_SIZE(props); i++) {
			WM_msg_subscribe_rna(mbus, &idptr, props[i], &msg_sub_value_mpr_tag_refresh, __func__);
		}
	}

	/* Subscribe to render settings */
	{
		WM_msg_subscribe_rna_anon_prop(mbus, RenderSettings, resolution_x, &msg_sub_value_mpr_tag_refresh);
		WM_msg_subscribe_rna_anon_prop(mbus, RenderSettings, resolution_y, &msg_sub_value_mpr_tag_refresh);
		WM_msg_subscribe_rna_anon_prop(mbus, RenderSettings, pixel_aspect_x, &msg_sub_value_mpr_tag_refresh);
		WM_msg_subscribe_rna_anon_prop(mbus, RenderSettings, pixel_aspect_y, &msg_sub_value_mpr_tag_refresh);
	}
}

void VIEW3D_WGT_camera(wmManipulatorGroupType *wgt)
{
	wgt->name = "Camera Widgets";
	wgt->idname = "VIEW3D_WGT_camera";

	wgt->flag = (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	             WM_MANIPULATORGROUPTYPE_3D |
	             WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_camera_poll;
	wgt->setup = WIDGETGROUP_camera_setup;
	wgt->refresh = WIDGETGROUP_camera_refresh;
	wgt->message_subscribe = WIDGETGROUP_camera_message_subscribe;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name CameraView Manipulators
 * \{ */

struct CameraViewWidgetGroup {
	wmManipulator *border;

	struct {
		rctf *edit_border;
		rctf view_border;
	} state;
};

/* scale callbacks */
static void manipulator_render_border_prop_matrix_get(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	struct CameraViewWidgetGroup *viewgroup = mpr_prop->custom_func.user_data;
	const rctf *border = viewgroup->state.edit_border;

	unit_m4(matrix);
	matrix[0][0] = BLI_rctf_size_x(border);
	matrix[1][1] = BLI_rctf_size_y(border);
	matrix[3][0] = BLI_rctf_cent_x(border);
	matrix[3][1] = BLI_rctf_cent_y(border);
}

static void manipulator_render_border_prop_matrix_set(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	struct CameraViewWidgetGroup *viewgroup = mpr_prop->custom_func.user_data;
	rctf *border = viewgroup->state.edit_border;
	BLI_assert(mpr_prop->type->array_length == 16);

	BLI_rctf_resize(border, len_v3(matrix[0]), len_v3(matrix[1]));
	BLI_rctf_recenter(border, matrix[3][0], matrix[3][1]);
	BLI_rctf_isect(&(rctf){.xmin = 0, .ymin = 0, .xmax = 1, .ymax = 1}, border, border);
}

static bool WIDGETGROUP_camera_view_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Scene *scene = CTX_data_scene(C);

	/* This is just so the border isn't always in the way,
	 * stealing mouse clicks from regular usage.
	 * We could change the rules for when to show. */
	{
		ViewLayer *view_layer = CTX_data_view_layer(C);
		if (scene->camera != OBACT(view_layer)) {
			return false;
		}
	}

	View3D *v3d = CTX_wm_view3d(C);
	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		return false;
	}

	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	if (rv3d->persp == RV3D_CAMOB) {
		if (scene->r.mode & R_BORDER) {
			/* TODO: support overrides. */
			if (scene->id.lib == NULL) {
				return true;
			}
		}
	}
	else if (v3d->flag2 & V3D_RENDER_BORDER) {
		return true;
	}
	return false;
}

static void WIDGETGROUP_camera_view_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct CameraViewWidgetGroup *viewgroup = MEM_mallocN(sizeof(struct CameraViewWidgetGroup), __func__);

	viewgroup->border = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, NULL);

	RNA_enum_set(viewgroup->border->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE);
	/* Box style is more subtle in this case. */
	RNA_enum_set(viewgroup->border->ptr, "draw_style", ED_MANIPULATOR_CAGE2D_STYLE_BOX);


	mgroup->customdata = viewgroup;
}

static void WIDGETGROUP_camera_view_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct CameraViewWidgetGroup *viewgroup = mgroup->customdata;

	ARegion *ar = CTX_wm_region(C);
	struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
	RegionView3D *rv3d = ar->regiondata;
	if (rv3d->persp == RV3D_CAMOB) {
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = CTX_wm_view3d(C);
		ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &viewgroup->state.view_border, false);
	}
	else {
		viewgroup->state.view_border = (rctf){.xmin = 0, .ymin = 0, .xmax = ar->winx, .ymax = ar->winy};
	}

	wmManipulator *mpr = viewgroup->border;
	unit_m4(mpr->matrix_space);
	mul_v3_fl(mpr->matrix_space[0], BLI_rctf_size_x(&viewgroup->state.view_border));
	mul_v3_fl(mpr->matrix_space[1], BLI_rctf_size_y(&viewgroup->state.view_border));
	mpr->matrix_space[3][0] = viewgroup->state.view_border.xmin;
	mpr->matrix_space[3][1] = viewgroup->state.view_border.ymin;
}

static void WIDGETGROUP_camera_view_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct CameraViewWidgetGroup *viewgroup = mgroup->customdata;

	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	Scene *scene = CTX_data_scene(C);

	{
		wmManipulator *mpr = viewgroup->border;
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, false);

		RNA_enum_set(viewgroup->border->ptr, "transform",
		             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_TRANSLATE | ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE);

		if (rv3d->persp == RV3D_CAMOB) {
			viewgroup->state.edit_border = &scene->r.border;
		}
		else {
			viewgroup->state.edit_border = &v3d->render_border;
		}

		WM_manipulator_target_property_def_func(
		        mpr, "matrix",
		        &(const struct wmManipulatorPropertyFnParams) {
		            .value_get_fn = manipulator_render_border_prop_matrix_get,
		            .value_set_fn = manipulator_render_border_prop_matrix_set,
		            .range_get_fn = NULL,
		            .user_data = viewgroup,
		        });
	}

}

void VIEW3D_WGT_camera_view(wmManipulatorGroupType *wgt)
{
	wgt->name = "Camera View Widgets";
	wgt->idname = "VIEW3D_WGT_camera_view";

	wgt->flag = (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	             WM_MANIPULATORGROUPTYPE_SCALE);

	wgt->poll = WIDGETGROUP_camera_view_poll;
	wgt->setup = WIDGETGROUP_camera_view_setup;
	wgt->draw_prepare = WIDGETGROUP_camera_view_draw_prepare;
	wgt->refresh = WIDGETGROUP_camera_view_refresh;
}

/** \} */
