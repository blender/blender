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

/** \file blender/editors/space_view3d/view3d_manipulator_navigate.c
 *  \ingroup spview3d
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_object_types.h"

#include "ED_screen.h"
#include "ED_manipulator_library.h"

#include "UI_resources.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/** \name View3D Navigation Manipulator Group
 * \{ */

/* Offset from screen edge. */
#define MANIPULATOR_OFFSET_FAC 2.5
/* Size of main icon. */
#define MANIPULATOR_SIZE 64
/* Factor for size of smaller button. */
#define MANIPULATOR_MINI_FAC 0.4
/* How much mini buttons offset from the primary. */
#define MANIPULATOR_MINI_OFFSET_FAC 0.6666f


enum {
	MPR_MOVE = 0,
	MPR_ROTATE = 1,
	MPR_ZOOM = 2,
	/* just buttons */
	MPR_PERSP_ORTHO = 3,
	MPR_CAMERA = 4,

	MPR_TOTAL = 5,
};

struct NavigateManipulatorInfo {
	const char *opname;
	const char *manipulator;
	int icon;
};

struct NavigateManipulatorInfo g_navigate_params[MPR_TOTAL] = {
	{
		.opname = "VIEW3D_OT_move",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.icon = ICON_HAND,
	}, {
		.opname = "VIEW3D_OT_rotate",
		.manipulator = "VIEW3D_WT_navigate_rotate",
		.icon = ICON_NONE,
	}, {
		.opname = "VIEW3D_OT_zoom",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.icon = ICON_VIEWZOOM,
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.icon = ICON_MESH_CUBE,
	}, {
		.opname = "VIEW3D_OT_viewnumpad",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.icon = ICON_OUTLINER_OB_CAMERA,
	},
};

struct NavigateWidgetGroup {
	wmManipulator *mpr_array[MPR_TOTAL];
	int region_size[2];
};

static bool WIDGETGROUP_navigate_poll(const bContext *UNUSED(C), wmManipulatorGroupType *UNUSED(wgt))
{
	if (U.manipulator_flag & USER_MANIPULATOR_DRAW_NAVIGATE) {
		return true;
	}
	return false;

}

static void WIDGETGROUP_navigate_setup(const bContext *UNUSED(C), wmManipulatorGroup *mgroup)
{
	struct NavigateWidgetGroup *navgroup = MEM_callocN(sizeof(struct NavigateWidgetGroup), __func__);

	navgroup->region_size[0] = -1;
	navgroup->region_size[1] = -1;

	wmOperatorType *ot_viewnumpad = WM_operatortype_find("VIEW3D_OT_viewnumpad", true);

	for (int i = 0; i < MPR_TOTAL; i++) {
		const struct NavigateManipulatorInfo *info = &g_navigate_params[i];
		navgroup->mpr_array[i] = WM_manipulator_new(info->manipulator, mgroup, NULL);
		wmManipulator *mpr = navgroup->mpr_array[i];
		mpr->flag |= WM_MANIPULATOR_GRAB_CURSOR;
		RNA_enum_set(mpr->ptr, "draw_options", ED_MANIPULATOR_GRAB_DRAW_FLAG_FILL);
		copy_v3_fl(mpr->color, 1.0f);
		mpr->color[3] = 0.4f;

		/* may be overwritten later */
		mpr->scale_basis = (MANIPULATOR_SIZE * MANIPULATOR_MINI_FAC) / 2;
		if (info->icon != ICON_NONE) {
			RNA_enum_set(mpr->ptr, "icon", info->icon);
		}

		wmOperatorType *ot = WM_operatortype_find(info->opname, true);
		WM_manipulator_operator_set(mpr, 0, ot, NULL);
	}

	{
		wmManipulator *mpr = navgroup->mpr_array[MPR_CAMERA];
		PointerRNA *ptr = WM_manipulator_operator_set(mpr, 0, ot_viewnumpad, NULL);
		RNA_enum_set(ptr, "type", RV3D_VIEW_CAMERA);
	}

	{
		wmManipulator *mpr = navgroup->mpr_array[MPR_ROTATE];
		mpr->scale_basis = MANIPULATOR_SIZE / 2;
		char mapping[6] = {
			RV3D_VIEW_LEFT,
			RV3D_VIEW_RIGHT,
			RV3D_VIEW_FRONT,
			RV3D_VIEW_BACK,
			RV3D_VIEW_BOTTOM,
			RV3D_VIEW_TOP,
		};

		for (int part_index = 0; part_index < 6; part_index+= 1) {
			PointerRNA *ptr = WM_manipulator_operator_set(mpr, mapping[part_index], ot_viewnumpad, NULL);
			RNA_enum_set(ptr, "type", RV3D_VIEW_FRONT + part_index);
		}
	}

	mgroup->customdata = navgroup;
}

static void WIDGETGROUP_navigate_draw_prepare(const bContext *C, wmManipulatorGroup *mgroup)
{
	struct NavigateWidgetGroup *navgroup = mgroup->customdata;
	ARegion *ar = CTX_wm_region(C);
	const RegionView3D *rv3d = ar->regiondata;

	for (int i = 0; i < 3; i++) {
		copy_v3_v3(navgroup->mpr_array[MPR_ROTATE]->matrix_offset[i], rv3d->viewmat[i]);
	}

	if ((navgroup->region_size[0] == ar->winx) &&
	    (navgroup->region_size[1] == ar->winy))
	{
		return;
	}

	navgroup->region_size[0] = ar->winx;
	navgroup->region_size[1] = ar->winy;

	const float icon_size = MANIPULATOR_SIZE;
	const float icon_offset = (icon_size / 2.0) * MANIPULATOR_OFFSET_FAC * U.pixelsize;
	const float icon_offset_mini = icon_size * MANIPULATOR_MINI_OFFSET_FAC * U.pixelsize;
	const float co[2] = {ar->winx - icon_offset, ar->winy - icon_offset};

	wmManipulator *mpr;
	mpr = navgroup->mpr_array[MPR_ROTATE];
	mpr->matrix_basis[3][0] = co[0];
	mpr->matrix_basis[3][1] = co[1];

	mpr = navgroup->mpr_array[MPR_MOVE];
	mpr->matrix_basis[3][0] = co[0] + icon_offset_mini;
	mpr->matrix_basis[3][1] = co[1] - icon_offset_mini;

	mpr = navgroup->mpr_array[MPR_ZOOM];
	mpr->matrix_basis[3][0] = co[0] - icon_offset_mini;
	mpr->matrix_basis[3][1] = co[1] - icon_offset_mini;

	mpr = navgroup->mpr_array[MPR_PERSP_ORTHO];
	mpr->matrix_basis[3][0] = co[0] + icon_offset_mini;
	mpr->matrix_basis[3][1] = co[1] + icon_offset_mini;

	mpr = navgroup->mpr_array[MPR_CAMERA];
	mpr->matrix_basis[3][0] = co[0] - icon_offset_mini;
	mpr->matrix_basis[3][1] = co[1] + icon_offset_mini;
}

void VIEW3D_WGT_navigate(wmManipulatorGroupType *wgt)
{
	wgt->name = "View3D Navigate";
	wgt->idname = "VIEW3D_WGT_navigate";

	wgt->flag |= (WM_MANIPULATORGROUPTYPE_PERSISTENT |
	              WM_MANIPULATORGROUPTYPE_SCALE);

	wgt->poll = WIDGETGROUP_navigate_poll;
	wgt->setup = WIDGETGROUP_navigate_setup;
	wgt->draw_prepare = WIDGETGROUP_navigate_draw_prepare;
}

/** \} */
