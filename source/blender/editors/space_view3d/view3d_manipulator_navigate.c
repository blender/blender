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
#define MANIPULATOR_MINI_FAC 0.5
/* How much mini buttons offset from the primary. */
#define MANIPULATOR_MINI_OFFSET_FAC 0.6666f


enum {
	MPR_MOVE = 0,
	MPR_ROTATE = 1,
	MPR_ZOOM = 2,

	/* just buttons */
	/* overlaps MPR_ORTHO (switch between) */
	MPR_PERSP = 3,
	MPR_ORTHO = 4,
	MPR_CAMERA = 5,

	MPR_TOTAL = 6,
};

/* Vector icons compatible with 'GPU_batch_from_poly_2d_encoded' */
static const uchar shape_camera[] = {
	0xa3, 0x19, 0x78, 0x55, 0x4d, 0x19, 0x4f, 0x0a, 0x7f, 0x00, 0xb0, 0x0a, 0xa9, 0x19,
	0xa9, 0x19, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a,
	0x4d, 0x19, 0x47, 0x19, 0x65, 0x55, 0x41, 0x55, 0x41, 0x9e, 0x43, 0xa8, 0x38, 0xb3,
	0x34, 0xc3, 0x38, 0xd2, 0x43, 0xdd, 0x53, 0xe1, 0x62, 0xdd, 0x6d, 0xd2, 0x72, 0xc3,
	0x78, 0xc3, 0x7c, 0xd2, 0x87, 0xdd, 0x96, 0xe1, 0xa6, 0xdd, 0xb1, 0xd2, 0xb5, 0xc3,
	0xb1, 0xb3, 0xa6, 0xa8, 0xa9, 0x9e, 0xa9, 0x8c, 0xbb, 0x8c, 0xbb, 0x86, 0xc7, 0x86,
	0xe0, 0x9e, 0xe0, 0x55, 0xc7, 0x6d, 0xbb, 0x6d, 0xbb, 0x67, 0xa9, 0x67, 0xa9, 0x55,
	0x8a, 0x55, 0xa9, 0x19, 0xb0, 0x0a, 0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0,
	0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff, 0x4f, 0xf5, 0x4f, 0xf5, 0x7c, 0xb3, 0x78, 0xc3,
	0x72, 0xc3, 0x6d, 0xb3, 0x62, 0xa8, 0x53, 0xa4, 0x43, 0xa8, 0x41, 0x9e, 0xa9, 0x9e,
	0xa6, 0xa8, 0x96, 0xa4, 0x87, 0xa8, 0x87, 0xa8,
};
static const uchar shape_ortho[] = {
	0x85, 0x15, 0x85, 0x7c, 0xde, 0xb3, 0xde, 0xb8, 0xd9, 0xba, 0x80, 0x85, 0x27, 0xba,
	0x22, 0xb8, 0x22, 0xb3, 0x7b, 0x7c, 0x7b, 0x15, 0x80, 0x12, 0x80, 0x12, 0x1d, 0xba,
	0x80, 0xf2, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f,
	0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x80, 0x0d, 0x1d, 0x45, 0x1d, 0x45, 0xb0, 0x0a,
	0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff,
	0x80, 0xf2, 0xe3, 0xba, 0xe3, 0x45, 0x80, 0x0d, 0x7f, 0x00, 0x7f, 0x00,
};
static const uchar shape_pan[] = {
	0xbf, 0x4c, 0xbf, 0x66, 0x99, 0x66, 0x99, 0x40, 0xb2, 0x40, 0x7f, 0x0d, 0x7f, 0x00,
	0xb0, 0x0a, 0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5,
	0x80, 0xff, 0x80, 0xf2, 0xb3, 0xbf, 0x99, 0xbf, 0x99, 0x99, 0xbf, 0x99, 0xbf, 0xb2,
	0xf2, 0x7f, 0xf2, 0x7f, 0x40, 0xb3, 0x40, 0x99, 0x66, 0x99, 0x66, 0xbf, 0x4d, 0xbf,
	0x80, 0xf2, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0a, 0x4f,
	0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x7f, 0x0d, 0x4c, 0x40, 0x66, 0x40, 0x66, 0x66,
	0x40, 0x66, 0x40, 0x4d, 0x0d, 0x80, 0x0d, 0x80,
};
static const uchar shape_persp[] = {
	0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff, 0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f,
	0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0x80, 0x07, 0x30, 0x50, 0x18, 0xbd,
	0x80, 0xdb, 0xe8, 0xbd, 0xf5, 0xb0, 0xf5, 0xb0, 0x83, 0x0f, 0x87, 0x7b, 0xe2, 0xb7,
	0xe3, 0xba, 0xe0, 0xbb, 0x80, 0x87, 0x20, 0xbb, 0x1d, 0xba, 0x1d, 0xb7, 0x78, 0x7b,
	0x7d, 0x0f, 0x80, 0x0c, 0x80, 0x0c, 0xd0, 0x50, 0x80, 0x07, 0x7f, 0x00, 0xb0, 0x0a,
	0xda, 0x25, 0xf5, 0x4f, 0xff, 0x80, 0xf5, 0xb0, 0xe8, 0xbd, 0xe8, 0xbd,
};
static const uchar shape_zoom[] = {
	0xad, 0x7f, 0xf1, 0x7f, 0xff, 0x80, 0xf5, 0xb0, 0xda, 0xda, 0xb0, 0xf5, 0x80, 0xff,
	0x4f, 0xf5, 0x25, 0xda, 0x0a, 0xb0, 0x00, 0x7f, 0x0d, 0x7f, 0x52, 0x7f, 0x69, 0xb7,
	0x48, 0xb7, 0x80, 0xd8, 0xb8, 0xb7, 0x96, 0xb7, 0x96, 0xb7, 0x7f, 0x2f, 0x0d, 0x7f,
	0x00, 0x7f, 0x0a, 0x4f, 0x25, 0x25, 0x4f, 0x0a, 0x7f, 0x00, 0xb0, 0x0a, 0xda, 0x25,
	0xf5, 0x4f, 0xff, 0x80, 0xf1, 0x7f, 0xf1, 0x7f,
};


struct NavigateManipulatorInfo {
	const char *opname;
	const char *manipulator;
	const unsigned char *shape;
	uint shape_size;
};

#define SHAPE_VARS(shape_id) shape = shape_id, .shape_size = ARRAY_SIZE(shape_id)

struct NavigateManipulatorInfo g_navigate_params[MPR_TOTAL] = {
	{
		.opname = "VIEW3D_OT_move",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.SHAPE_VARS(shape_pan),
	}, {
		.opname = "VIEW3D_OT_rotate",
		.manipulator = "VIEW3D_WT_navigate_rotate",
		.shape = NULL,
		.shape_size = 0,
	}, {
		.opname = "VIEW3D_OT_zoom",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.SHAPE_VARS(shape_zoom),
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.SHAPE_VARS(shape_persp),
	}, {
		.opname = "VIEW3D_OT_view_persportho",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.SHAPE_VARS(shape_ortho),
	}, {
		.opname = "VIEW3D_OT_viewnumpad",
		.manipulator = "MANIPULATOR_WT_button_2d",
		.SHAPE_VARS(shape_camera),
	},
};

#undef SHAPE_VARS

struct NavigateWidgetGroup {
	wmManipulator *mpr_array[MPR_TOTAL];
	int region_size[2];
	bool is_persp;
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
		mpr->flag |= WM_MANIPULATOR_GRAB_CURSOR | WM_MANIPULATOR_DRAW_MODAL;
		mpr->color[3] = 0.2f;
		mpr->color_hi[3] = 0.4f;

		/* may be overwritten later */
		mpr->scale_basis = (MANIPULATOR_SIZE * MANIPULATOR_MINI_FAC) / 2;
		if (info->shape != NULL) {
			PropertyRNA *prop = RNA_struct_find_property(mpr->ptr, "shape");
			RNA_property_string_set_bytes(
			        mpr->ptr, prop,
			        (const char *)info->shape, info->shape_size);
			/* don't fade icons so much */
			mpr->color[3] = 0.5f;
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

		for (int part_index = 0; part_index < 6; part_index += 1) {
			PointerRNA *ptr = WM_manipulator_operator_set(mpr, part_index + 1, ot_viewnumpad, NULL);
			RNA_enum_set(ptr, "type", mapping[part_index]);
		}

		/* When dragging an axis, use this instead. */
		mpr->drag_part = 0;
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
	    (navgroup->region_size[1] == ar->winy) &&
	    (navgroup->is_persp == rv3d->is_persp))
	{
		return;
	}

	navgroup->region_size[0] = ar->winx;
	navgroup->region_size[1] = ar->winy;
	navgroup->is_persp = rv3d->is_persp;


	const float icon_size = MANIPULATOR_SIZE;
	const float icon_offset = (icon_size / 2.0) * MANIPULATOR_OFFSET_FAC * U.ui_scale;
	const float icon_offset_mini = icon_size * MANIPULATOR_MINI_OFFSET_FAC * U.ui_scale;
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

	if (rv3d->is_persp) {
		mpr = navgroup->mpr_array[MPR_PERSP];
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, true);
		mpr = navgroup->mpr_array[MPR_ORTHO];
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, false);
	}
	else {
		mpr = navgroup->mpr_array[MPR_ORTHO];
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, true);
		mpr = navgroup->mpr_array[MPR_PERSP];
		WM_manipulator_set_flag(mpr, WM_MANIPULATOR_HIDDEN, false);
	}
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
	              WM_MANIPULATORGROUPTYPE_SCALE |
	              WM_MANIPULATORGROUPTYPE_DRAW_MODAL_ALL);

	wgt->poll = WIDGETGROUP_navigate_poll;
	wgt->setup = WIDGETGROUP_navigate_setup;
	wgt->draw_prepare = WIDGETGROUP_navigate_draw_prepare;
}

/** \} */
