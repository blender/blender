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

/** \file blender/editors/space_view3d/view3d_gizmo_lamp.c
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
#include "ED_gizmo_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */

/** \name Spot Lamp Gizmos
 * \{ */

static bool WIDGETGROUP_lamp_spot_poll(const bContext *C, wmGizmoGroupType *UNUSED(wgt))
{
	View3D *v3d = CTX_wm_view3d(C);
	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) ||
	    (v3d->mpr_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_CONTEXT)))
	{
		return false;
	}

	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);
	}
	return false;
}

static void WIDGETGROUP_lamp_spot_setup(const bContext *UNUSED(C), wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);

	wwrapper->gizmo = WM_gizmo_new("GIZMO_WT_arrow_3d", mgroup, NULL);
	wmGizmo *mpr = wwrapper->gizmo;
	RNA_enum_set(mpr->ptr, "transform",  ED_GIZMO_ARROW_XFORM_FLAG_INVERTED);

	mgroup->customdata = wwrapper;

	ED_gizmo_arrow3d_set_range_fac(mpr, 4.0f);

	UI_GetThemeColor3fv(TH_GIZMO_SECONDARY, mpr->color);
}

static void WIDGETGROUP_lamp_spot_refresh(const bContext *C, wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = mgroup->customdata;
	wmGizmo *mpr = wwrapper->gizmo;
	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	float dir[3];

	negate_v3_v3(dir, ob->obmat[2]);

	WM_gizmo_set_matrix_rotation_from_z_axis(mpr, dir);
	WM_gizmo_set_matrix_location(mpr, ob->obmat[3]);

	/* need to set property here for undo. TODO would prefer to do this in _init */
	PointerRNA lamp_ptr;
	const char *propname = "spot_size";
	RNA_pointer_create(&la->id, &RNA_Light, la, &lamp_ptr);
	WM_gizmo_target_property_def_rna(mpr, "offset", &lamp_ptr, propname, -1);
}

void VIEW3D_WGT_lamp_spot(wmGizmoGroupType *wgt)
{
	wgt->name = "Spot Light Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_spot";

	wgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT |
	              WM_GIZMOGROUPTYPE_3D |
	              WM_GIZMOGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_lamp_spot_poll;
	wgt->setup = WIDGETGROUP_lamp_spot_setup;
	wgt->refresh = WIDGETGROUP_lamp_spot_refresh;
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Area Lamp Gizmos
 * \{ */

/* scale callbacks */
static void gizmo_area_lamp_prop_matrix_get(
        const wmGizmo *UNUSED(mpr), wmGizmoProperty *mpr_prop,
        void *value_p)
{
	BLI_assert(mpr_prop->type->array_length == 16);
	float (*matrix)[4] = value_p;
	const Lamp *la = mpr_prop->custom_func.user_data;

	matrix[0][0] = la->area_size;
	matrix[1][1] = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ? la->area_sizey : la->area_size;
}

static void gizmo_area_lamp_prop_matrix_set(
        const wmGizmo *UNUSED(mpr), wmGizmoProperty *mpr_prop,
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

static bool WIDGETGROUP_lamp_area_poll(const bContext *C, wmGizmoGroupType *UNUSED(wgt))
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		return false;
	}

	Object *ob = CTX_data_active_object(C);
	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_AREA);
	}
	return false;
}

static void WIDGETGROUP_lamp_area_setup(const bContext *UNUSED(C), wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);
	wwrapper->gizmo = WM_gizmo_new("GIZMO_WT_cage_2d", mgroup, NULL);
	wmGizmo *mpr = wwrapper->gizmo;
	RNA_enum_set(mpr->ptr, "transform",
	             ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE);

	mgroup->customdata = wwrapper;

	WM_gizmo_set_flag(mpr, WM_GIZMO_DRAW_HOVER, true);

	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, mpr->color);
	UI_GetThemeColor3fv(TH_GIZMO_HI, mpr->color_hi);
}

static void WIDGETGROUP_lamp_area_refresh(const bContext *C, wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	wmGizmo *mpr = wwrapper->gizmo;

	copy_m4_m4(mpr->matrix_basis, ob->obmat);

	int flag = ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE;
	if (ELEM(la->area_shape, LA_AREA_SQUARE, LA_AREA_DISK)) {
		flag |= ED_GIZMO_CAGE2D_XFORM_FLAG_SCALE_UNIFORM;
	}
	RNA_enum_set(mpr->ptr, "transform", flag);

	/* need to set property here for undo. TODO would prefer to do this in _init */
	WM_gizmo_target_property_def_func(
	        mpr, "matrix",
	        &(const struct wmGizmoPropertyFnParams) {
	            .value_get_fn = gizmo_area_lamp_prop_matrix_get,
	            .value_set_fn = gizmo_area_lamp_prop_matrix_set,
	            .range_get_fn = NULL,
	            .user_data = la,
	        });
}

void VIEW3D_WGT_lamp_area(wmGizmoGroupType *wgt)
{
	wgt->name = "Area Light Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_area";

	wgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT |
	              WM_GIZMOGROUPTYPE_3D |
	              WM_GIZMOGROUPTYPE_DEPTH_3D);

	wgt->poll = WIDGETGROUP_lamp_area_poll;
	wgt->setup = WIDGETGROUP_lamp_area_setup;
	wgt->refresh = WIDGETGROUP_lamp_area_refresh;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Lamp Target Gizmo
 * \{ */

static bool WIDGETGROUP_lamp_target_poll(const bContext *C, wmGizmoGroupType *UNUSED(wgt))
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		return false;
	}

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

static void WIDGETGROUP_lamp_target_setup(const bContext *UNUSED(C), wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = MEM_mallocN(sizeof(wmGizmoWrapper), __func__);
	wwrapper->gizmo = WM_gizmo_new("GIZMO_WT_grab_3d", mgroup, NULL);
	wmGizmo *mpr = wwrapper->gizmo;

	mgroup->customdata = wwrapper;

	UI_GetThemeColor3fv(TH_GIZMO_PRIMARY, mpr->color);
	UI_GetThemeColor3fv(TH_GIZMO_HI, mpr->color_hi);

	mpr->scale_basis = 0.06f;

	wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_transform_axis_target", true);

	RNA_enum_set(mpr->ptr, "draw_options",
	             ED_GIZMO_GRAB_DRAW_FLAG_FILL | ED_GIZMO_GRAB_DRAW_FLAG_ALIGN_VIEW);

	WM_gizmo_operator_set(mpr, 0, ot, NULL);
}

static void WIDGETGROUP_lamp_target_draw_prepare(const bContext *C, wmGizmoGroup *mgroup)
{
	wmGizmoWrapper *wwrapper = mgroup->customdata;
	Object *ob = CTX_data_active_object(C);
	wmGizmo *mpr = wwrapper->gizmo;

	copy_m4_m4(mpr->matrix_basis, ob->obmat);
	unit_m4(mpr->matrix_offset);
	mpr->matrix_offset[3][2] = -2.4f / mpr->scale_basis;
	WM_gizmo_set_flag(mpr, WM_GIZMO_DRAW_OFFSET_SCALE, true);
}

void VIEW3D_WGT_lamp_target(wmGizmoGroupType *wgt)
{
	wgt->name = "Target Light Widgets";
	wgt->idname = "VIEW3D_WGT_lamp_target";

	wgt->flag |= (WM_GIZMOGROUPTYPE_PERSISTENT |
	              WM_GIZMOGROUPTYPE_3D);

	wgt->poll = WIDGETGROUP_lamp_target_poll;
	wgt->setup = WIDGETGROUP_lamp_target_setup;
	wgt->draw_prepare = WIDGETGROUP_lamp_target_draw_prepare;
}

/** \} */
