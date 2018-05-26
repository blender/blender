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

/** \file blender/editors/space_view3d/view3d_manipulator_lamp.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_object_types.h"
#include "DNA_lamp_types.h"

#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph_query.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Spot Lamp Manipulators
 * \{ */

static bool WIDGETGROUP_lamp_spot_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);
	}
	return false;
}

static void WIDGETGROUP_lamp_spot_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);

	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_arrow_3d", mgroup, NULL);
	wmManipulator *mpr = wwrapper->manipulator;
	RNA_enum_set(mpr->ptr, "draw_options",  ED_MANIPULATOR_ARROW_STYLE_INVERTED);

	mgroup->customdata = wwrapper;

	ED_manipulator_arrow3d_set_range_fac(mpr, 4.0f);

	UI_GetThemeColor3fv(TH_MANIPULATOR_SECONDARY, mpr->color);
}

static void WIDGETGROUP_lamp_spot_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	wmManipulator *mpr = wwrapper->manipulator;
	Object *ob = CTX_data_active_object(C);
	const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	Lamp *la = ob->data;
	float dir[3];

	negate_v3_v3(dir, ob_eval->obmat[2]);

	WM_manipulator_set_matrix_rotation_from_z_axis(mpr, dir);
	WM_manipulator_set_matrix_location(mpr, ob_eval->obmat[3]);

	/* need to set property here for undo. TODO would prefer to do this in _init */
	PointerRNA lamp_ptr;
	const char *propname = "spot_size";
	RNA_pointer_create(&la->id, &RNA_Lamp, la, &lamp_ptr);
	WM_manipulator_target_property_def_rna(mpr, "offset", &lamp_ptr, propname, -1);
}

void VIEW3D_WGT_lamp_spot(wmManipulatorGroupType *wgt)
{
	wgt->name = "Spot Lamp Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_spot";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_lamp_spot_poll;
	wgt->setup = WIDGETGROUP_lamp_spot_setup;
	wgt->refresh = WIDGETGROUP_lamp_spot_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Area Lamp Manipulators
 * \{ */

/* scale callbacks */
static void manipulator_area_lamp_prop_matrix_get(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	BLI_assert(mpr_prop->type->array_length == 16);
	float (*matrix)[4] = value_p;
	const Lamp *la = mpr_prop->custom_func.user_data;

	matrix[0][0] = la->area_size;
	matrix[1][1] = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ? la->area_sizey : la->area_size;
}

static void manipulator_area_lamp_prop_matrix_set(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	const float (*matrix)[4] = value_p;
	BLI_assert(mpr_prop->type->array_length == 16);
	Lamp *la = mpr_prop->custom_func.user_data;

	if (ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
		la->area_size = len_v3(matrix[0]);
		la->area_sizey = len_v3(matrix[1]);
	}
	else {
		la->area_size = len_v3(matrix[0]);
	}
}

static bool WIDGETGROUP_lamp_area_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_AREA);
	}
	return false;
}

static void WIDGETGROUP_lamp_area_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);
	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_cage_2d", mgroup, NULL);
	wmManipulator *mpr = wwrapper->manipulator;
	RNA_enum_set(mpr->ptr, "transform",
	             ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE);

	mgroup->customdata = wwrapper;

	WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_HOVER, true);

	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);
}

static void WIDGETGROUP_lamp_area_refresh(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = CTX_data_active_object(C);
	const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	Lamp *la = ob_eval->data;
	wmManipulator *mpr = wwrapper->manipulator;

	copy_m4_m4(mpr->matrix_basis, ob_eval->obmat);

	int flag = ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE;
	if (ELEM(la->area_shape, LA_AREA_SQUARE, LA_AREA_DISK)) {
		flag |= ED_MANIPULATOR_CAGE2D_XFORM_FLAG_SCALE_UNIFORM;
	}
	RNA_enum_set(mpr->ptr, "transform", flag);

	/* need to set property here for undo. TODO would prefer to do this in _init */
	WM_manipulator_target_property_def_func(
	        mpr, "matrix",
	        &(const struct wmManipulatorPropertyFnParams) {
	            .value_get_fn = manipulator_area_lamp_prop_matrix_get,
	            .value_set_fn = manipulator_area_lamp_prop_matrix_set,
	            .range_get_fn = NULL,
	            .user_data = la,
	        });
}

void VIEW3D_WGT_lamp_area(wmManipulatorGroupType *wgt)
{
	wgt->name = "Area Lamp Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_area";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D |
	              WM_MANIPULATORGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_lamp_area_poll;
	wgt->setup = WIDGETGROUP_lamp_area_setup;
	wgt->refresh = WIDGETGROUP_lamp_area_refresh;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Lamp Target Manipulator
 * \{ */

static bool WIDGETGROUP_lamp_target_poll(const bContext *C, wmManipulatorGroupType *UNUSED(wgt))
{
	Object *ob = CTX_data_active_object(C);

	if (ob != NULL) {
		if (ob->type == OB_LAMP) {
			Lamp *la = ob->data;
			return (ELEM(la->type, LA_SUN, LA_SPOT, LA_HEMI, LA_AREA));
		}
#if 0
		else if (ob->type == OB_CAMERA) {
			return true;
		}
#endif
	}
	return false;
}

static void WIDGETGROUP_lamp_target_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = MEM_mallocN(sizeof(wmManipulatorWrapper), __func__);
	wwrapper->manipulator = WM_manipulator_new("MANIPULATOR_WT_grab_3d", mgroup, NULL);
	wmManipulator *mpr = wwrapper->manipulator;

	mgroup->customdata = wwrapper;

	UI_GetThemeColor3fv(TH_MANIPULATOR_PRIMARY, mpr->color);
	UI_GetThemeColor3fv(TH_MANIPULATOR_HI, mpr->color_hi);

	mpr->scale_basis = 0.06f;

	wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_transform_axis_target", true);

	RNA_enum_set(mpr->ptr, "draw_options",
	             ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL | ED_MANIPULATOR_GRAB_DRAW_FLAG_ALIGN_VIEW);

	WM_manipulator_operator_set(mpr, 0, ot, NULL);
}

static void WIDGETGROUP_lamp_target_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	wmManipulatorWrapper *wwrapper = mgroup->customdata;
	const Depsgraph *depsgraph = CTX_data_depsgraph(C);
	Object *ob = CTX_data_active_object(C);
	const Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
	wmManipulator *mpr = wwrapper->manipulator;

	copy_m4_m4(mpr->matrix_basis, ob_eval->obmat);
	unit_m4(mpr->matrix_offset);
	mpr->matrix_offset[3][2] = -2.4f / mpr->scale_basis;
	WM_manipulator_set_flag(mpr, WM_MANIPULATOR_DRAW_OFFSET_SCALE, true);
}

void VIEW3D_WGT_lamp_target(wmManipulatorGroupType *wgt)
{
	wgt->name = "Target Lamp Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_target";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_3D);

	wgt->poll = WIDGETGROUP_lamp_target_poll;
	wgt->setup = WIDGETGROUP_lamp_target_setup;
	wgt->draw_prepare = WIDGETGROUP_lamp_target_draw_prepare;
}

/** \} */
