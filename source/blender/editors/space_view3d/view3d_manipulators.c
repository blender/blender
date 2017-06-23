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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_manipulators.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Lamp Manipulators
 * \{ */

static bool WIDGETGROUP_lamp_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);
	}
	return false;
}

static void WIDGETGROUP_lamp_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	const char *propname = "spot_size";

	const float color[4] = {0.5f, 0.5f, 1.0f, 1.0f};
	const float color_hi[4] = {0.8f, 0.8f, 0.45f, 1.0f};

	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_arrow_3d", mgroup, propname, NULL);
	RNA_enum_set(wwrapper->manipulator->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_INVERTED);

	mgroup->customdata = wwrapper;

	ED_manipulator_arrow3d_set_range_fac(wwrapper->manipulator, 4.0f);
	WM_manipulator_set_color(wwrapper->manipulator, color);
	WM_manipulator_set_color_highlight(wwrapper->manipulator, color_hi);
}

static void WIDGETGROUP_lamp_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	float dir[3];

	negate_v3_v3(dir, ob->obmat[2]);

	WM_manipulator_set_matrix_rotation_from_z_axis(wwrapper->manipulator, dir);
	WM_manipulator_set_matrix_location(wwrapper->manipulator, ob->obmat[3]);

	/* need to set property here for undo. TODO would prefer to do this in _init */
	PointerRNA lamp_ptr;
	const char *propname = "spot_size";
	RNA_pointer_create(&la->id, &RNA_Lamp, la, &lamp_ptr);
	WM_manipulator_target_property_def_rna(wwrapper->manipulator, "offset", &lamp_ptr, propname, -1);
}

void VIEW3D_WGT_lamp(wmManipulatorGroupType *wgt)
{
	wgt->name = "Lamp Widgets";
	wgt->idname = "VIEW3D_WGT_lamp";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_lamp_poll;
	wgt->setup = WIDGETGROUP_lamp_setup;
	wgt->refresh = WIDGETGROUP_lamp_refresh;
}

/** \} */


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
	Object *ob = CTX_data_active_object(C);

	return (ob && ob->type == OB_CAMERA);
}

static void cameragroup_property_setup(wmManipulator *widget, Object *ob, Camera *ca, const bool is_ortho)
{
	const float scale[3] = {1.0f / len_v3(ob->obmat[0]), 1.0f / len_v3(ob->obmat[1]), 1.0f / len_v3(ob->obmat[2])};
	const float scale_fac = ca->drawsize;
	const float drawsize = is_ortho ?
	        (0.5f * ca->ortho_scale) :
	        (scale_fac / ((scale[0] + scale[1] + scale[2]) / 3.0f));
	const float half_sensor = 0.5f * ((ca->sensor_fit == CAMERA_SENSOR_FIT_VERT) ? ca->sensor_y : ca->sensor_x);
	const char *propname = is_ortho ? "ortho_scale" : "lens";

	PointerRNA camera_ptr;
	float min, max, range;
	float step, precision;

	RNA_pointer_create(&ca->id, &RNA_Camera, ca, &camera_ptr);

	/* get property range */
	PropertyRNA *prop = RNA_struct_find_property(&camera_ptr, propname);
	RNA_property_float_ui_range(&camera_ptr, prop, &min, &max, &step, &precision);
	range = max - min;

	ED_manipulator_arrow3d_set_range_fac(widget, is_ortho ? (scale_fac * range) : (drawsize * range / half_sensor));
}

static void WIDGETGROUP_camera_setup(const bContext *C, wmManipulatorGroup *mgroup)
{
	Object *ob = CTX_data_active_object(C);
	Camera *ca = ob->data;
	float dir[3];

	const wmManipulatorType *wt_arrow = WM_manipulatortype_find("MANIPULATOR_WT_arrow_3d", true);

	struct CameraWidgetGroup *camgroup = MEM_callocN(sizeof(struct CameraWidgetGroup), __func__);
	mgroup->customdata = camgroup;

	negate_v3_v3(dir, ob->obmat[2]);

	/* dof distance */
	{
		const float color[4] = {1.0f, 0.3f, 0.0f, 1.0f};
		const float color_hi[4] = {1.0f, 0.3f, 0.0f, 1.0f};

		camgroup->dop_dist = WM_manipulator_new_ptr(wt_arrow, mgroup, "dof_distance", NULL);
		RNA_enum_set(camgroup->dop_dist->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CROSS);
		WM_manipulator_set_flag(camgroup->dop_dist, WM_MANIPULATOR_DRAW_HOVER, true);
		WM_manipulator_set_color(camgroup->dop_dist, color);
		WM_manipulator_set_color_highlight(camgroup->dop_dist, color_hi);
	}

	/* focal length
	 * - logic/calculations are similar to BKE_camera_view_frame_ex, better keep in sync */
	{
		const float color[4] = {1.0f, 1.0, 0.27f, 0.5f};
		const float color_hi[4] = {1.0f, 1.0, 0.27f, 1.0f};

		camgroup->focal_len = WM_manipulator_new_ptr(wt_arrow, mgroup, "focal_len", NULL);
		RNA_enum_set(camgroup->focal_len->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CONE);
		RNA_enum_set(camgroup->focal_len->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);

		WM_manipulator_set_color(camgroup->focal_len, color);
		WM_manipulator_set_color_highlight(camgroup->focal_len, color_hi);
		cameragroup_property_setup(camgroup->focal_len, ob, ca, false);

		camgroup->ortho_scale = WM_manipulator_new_ptr(wt_arrow, mgroup, "ortho_scale", NULL);
		RNA_enum_set(camgroup->ortho_scale->ptr, "draw_style",  ED_MANIPULATOR_ARROW_STYLE_CONE);
		RNA_enum_set(camgroup->ortho_scale->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);

		WM_manipulator_set_color(camgroup->ortho_scale, color);
		WM_manipulator_set_color_highlight(camgroup->ortho_scale, color_hi);
		cameragroup_property_setup(camgroup->ortho_scale, ob, ca, true);
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

	/* TODO - make focal length/ortho scale widget optional */
	if (true) {
		const bool is_ortho = (ca->type == CAM_ORTHO);
		const float scale[3] = {1.0f / len_v3(ob->obmat[0]), 1.0f / len_v3(ob->obmat[1]), 1.0f / len_v3(ob->obmat[2])};
		const float scale_fac = ca->drawsize;
		const float drawsize = is_ortho ?
		        (0.5f * ca->ortho_scale) :
		        (scale_fac / ((scale[0] + scale[1] + scale[2]) / 3.0f));
		float offset[3];
		float aspect[2];

		wmManipulator *widget = is_ortho ? camgroup->ortho_scale : camgroup->focal_len;
		WM_manipulator_set_flag(widget, WM_MANIPULATOR_HIDDEN, false);
		WM_manipulator_set_flag(is_ortho ? camgroup->focal_len : camgroup->ortho_scale, WM_MANIPULATOR_HIDDEN, true);


		/* account for lens shifting */
		offset[0] = ((ob->size[0] > 0.0f) ? -2.0f : 2.0f) * ca->shiftx;
		offset[1] = 2.0f * ca->shifty;
		offset[2] = 0.0f;

		/* get aspect */
		const Scene *scene = CTX_data_scene(C);
		const float aspx = (float)scene->r.xsch * scene->r.xasp;
		const float aspy = (float)scene->r.ysch * scene->r.yasp;
		const int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, aspx, aspy);
		aspect[0] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? 1.0 : aspx / aspy;
		aspect[1] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? aspy / aspx : 1.0f;

		WM_manipulator_set_matrix_location(widget, ob->obmat[3]);
		WM_manipulator_set_matrix_rotation_from_yz_axis(widget, ob->obmat[1], dir);

		RNA_float_set_array(widget->ptr, "aspect", aspect);

		WM_manipulator_set_matrix_offset_location(widget, offset);
		WM_manipulator_set_scale(widget, drawsize);

		/* need to set property here for undo. TODO would prefer to do this in _init */
		WM_manipulator_target_property_def_rna(camgroup->focal_len, "offset", &camera_ptr, "lens", -1);
		WM_manipulator_target_property_def_rna(camgroup->ortho_scale, "offset", &camera_ptr, "ortho_scale", -1);
	}
}

void VIEW3D_WGT_camera(wmManipulatorGroupType *wgt)
{
	wgt->name = "Camera Widgets";
	wgt->idname = "VIEW3D_WGT_camera";

	wgt->flag = (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	             WM_MANIPULATORGROUPTYPE_3D |
	             WM_MANIPULATORGROUPTYPE_SCALE |
	             WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_camera_poll;
	wgt->setup = WIDGETGROUP_camera_setup;
	wgt->refresh = WIDGETGROUP_camera_refresh;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Force Field Manipulators
 * \{ */

static bool WIDGETGROUP_forcefield_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	return (ob && ob->pd && ob->pd->forcefield);
}

static void WIDGETGROUP_forcefield_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	const float col[4] = {0.8f, 0.8f, 0.45f, 0.5f};
	const float col_hi[4] = {0.8f, 0.8f, 0.45f, 1.0f};

	/* only wind effector for now */
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);
	mgroup->customdata = wwrapper;

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_arrow_3d", mgroup, "field_strength", NULL);
	RNA_enum_set(wwrapper->manipulator->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_CONSTRAINED);
	ED_manipulator_arrow3d_set_ui_range(wwrapper->manipulator, -200.0f, 200.0f);
	ED_manipulator_arrow3d_set_range_fac(wwrapper->manipulator, 6.0f);
	WM_manipulator_set_color(wwrapper->manipulator, col);
	WM_manipulator_set_color_highlight(wwrapper->manipulator, col_hi);
}

static void WIDGETGROUP_forcefield_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	PartDeflect *pd = ob->pd;

	if (pd->forcefield == PFIELD_WIND) {
		const float size = (ob->type == OB_EMPTY) ? ob->empty_drawsize : 1.0f;
		const float ofs[3] = {0.0f, -size, 0.0f};
		PointerRNA field_ptr;

		RNA_pointer_create(&ob->id, &RNA_FieldSettings, pd, &field_ptr);
		WM_manipulator_set_matrix_location(wwrapper->manipulator, ob->obmat[3]);
		WM_manipulator_set_matrix_rotation_from_z_axis(wwrapper->manipulator, ob->obmat[2]);
		WM_manipulator_set_matrix_offset_location(wwrapper->manipulator, ofs);
		WM_manipulator_set_flag(wwrapper->manipulator, WM_MANIPULATOR_HIDDEN, false);
		WM_manipulator_target_property_def_rna(wwrapper->manipulator, "offset", &field_ptr, "strength", -1);
	}
	else {
		WM_manipulator_set_flag(wwrapper->manipulator, WM_MANIPULATOR_HIDDEN, true);
	}
}

void VIEW3D_WGT_force_field(wmManipulatorGroupType *wgt)
{
	wgt->name = "Force Field Widgets";
	wgt->idname = "VIEW3D_WGT_force_field";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_SCALE |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_forcefield_poll;
	wgt->setup = WIDGETGROUP_forcefield_setup;
	wgt->refresh = WIDGETGROUP_forcefield_refresh;
}

/** \} */

